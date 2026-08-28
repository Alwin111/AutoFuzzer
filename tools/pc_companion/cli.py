#!/usr/bin/env python3
"""
AutoFuzzer PC Companion — CLI

Main command-line interface for the AutoFuzzer PC companion tool.

Usage:
    python -m tools.pc_companion.cli [command]

Commands:
    connect       Connect to ESP32 via USB serial
    status        Query ESP32 status
    export        Export failure data from ESP32
    report        Generate reports from failure data
    history       Show test history
    regress       Run regression tests
    fix           Analyze a failure and suggest fixes
    build         Build firmware
    flash         Flash firmware
    analyze       Analyze failure data (offline)
    help          Show this help
"""

import sys
import json
import time
from pathlib import Path
from typing import Optional

from .connection import AutoFuzzerConnection
from .reporter import ReportGenerator
from .database import (
    FailureDatabase, FailureRecord, CampaignStats,
    TestResult, TestSession,
)
from .builder import FirmwareBuilder
from .fixer import AIFixer
from .regression import RegressionTester


class AutoFuzzerCLI:
    """Main CLI for AutoFuzzer PC Companion."""

    def __init__(self):
        self.conn = AutoFuzzerConnection()
        self.db = FailureDatabase()
        self.reporter = ReportGenerator()
        self.builder = FirmwareBuilder()
        self.fixer = AIFixer()

    def cmd_connect(self, port: Optional[str] = None):
        """Connect to ESP32."""
        if self.conn.connect(port):
            print("[+] Connection established")
            self.cmd_status()
        else:
            print("[!] Could not connect. Check USB cable and port.")

    def cmd_status(self):
        """Query ESP32 status."""
        if not self.conn.is_connected():
            print("[!] Not connected. Use: connect [port]")
            return
        status = self.conn.get_status()
        if status:
            print("\n  ESP32 STATUS:")
            for k, v in status.items():
                print(f"    {k}: {v}")
        else:
            print("[!] No response from ESP32")

    def cmd_export(self):
        """Export failure data from ESP32."""
        if not self.conn.is_connected():
            print("[!] Not connected")
            return

        print("[*] Requesting failure export...")
        records = self.conn.export_failures()

        if not records:
            print("[*] No failures exported")
            return

        print(f"[+] Received {len(records)} failure record(s)")

        # Convert to FailureRecord objects and save
        failures = []
        for r in records:
            f = FailureRecord(
                id=r.get("id", 0),
                failure_type=r.get("type", "UNKNOWN"),
                status=r.get("status", "SUSPECTED"),
                protocol=r.get("protocol", "UART"),
                profile=r.get("profile", "QUICK"),
                sequence=r.get("sequence", 0),
                mutation=r.get("mutation", "RANDOM"),
                packet_len=r.get("packetLen", 0),
                payload_len=r.get("payloadLen", 0),
                seed=r.get("seed", "0x00000000"),
                prng_before=r.get("prngBefore", "0x00000000"),
                prng_after=r.get("prngAfter", "0x00000000"),
                heartbeat_lost_ms=r.get("heartbeatLostMs", 0),
                dut_responded=r.get("dutResponded", False),
                dut_status=r.get("dutResponseStatus", "0x00"),
                dut_resp_time_ms=r.get("dutRespTimeMs", 0),
                packets_at_fail=r.get("packetsAtFail", 0),
                elapsed_ms=r.get("elapsedMs", 0),
                replay_attempts=r.get("replayAttempts", 0),
                replay_fails=r.get("replayFails", 0),
                minimized_len=r.get("minimizedLen", 0),
                packet_bytes=[
                    int(h, 16) for h in r.get("packetHex", "").split() if h
                ] if r.get("packetHex") else [],
                timestamp=time.strftime("%Y-%m-%d %H:%M:%S"),
            )
            failures.append(f)

        # Generate reports
        test_id = time.strftime("EXPORT_%Y%m%d_%H%M%S")
        stats = CampaignStats(total_packets=failures[0].packets_at_fail if failures else 0)
        result = TestResult(
            protocol=failures[0].protocol if failures else "UART",
            verdict="FAIL",
        )

        self.reporter.generate_all(result, stats, failures, test_id)
        print(f"[+] Export complete: {len(failures)} failures saved")

    def cmd_report(self, test_id: Optional[str] = None):
        """Generate reports from stored failure data."""
        session = self.db.load_session(test_id) if test_id else None
        if not session:
            # Generate from most recent data
            sessions = self.db.list_sessions(limit=1)
            if sessions:
                session = self.db.load_session(sessions[0]["test_id"], sessions[0]["date"])

        if not session:
            print("[!] No test data found. Run a test first or use: export")
            return

        self.reporter.generate_all(
            session.result, session.stats, session.failures, session.test_id
        )

    def cmd_history(self):
        """Show test history."""
        sessions = self.db.list_sessions(limit=20)

        if not sessions:
            print("[*] No test history found")
            return

        print("\n  TEST HISTORY")
        print("  " + "-" * 60)
        print(f"  {'ID':<25} {'Protocol':<8} {'Score':<6} {'Verdict':<12} {'Fails'}")
        print("  " + "-" * 60)

        for s in sessions:
            print(
                f"  {s['test_id']:<25} {s['protocol']:<8} "
                f"{s['score']:<6} {s['verdict']:<12} {s['failures']}"
            )

        print("  " + "-" * 60)

        stats = self.db.get_stats()
        print(f"\n  Total sessions: {stats['total_sessions']}")
        print(f"  Total failures: {stats['total_failures']}")
        print(f"  Reproducible:   {stats['reproducible']}")
        print(f"  Intermittent:   {stats['intermittent']}")

    def cmd_regress(self):
        """Run regression tests."""
        if not self.conn.is_connected():
            print("[!] Not connected. Connect first.")
            return

        testcases = self.db.get_regression_testcases()
        if not testcases:
            print("[*] No regression testcases available")
            return

        print(f"[*] Running {len(testcases)} regression testcases...")

        tester = RegressionTester(self.conn)

        def progress(current, total, fid):
            print(f"  [{current}/{total}] Testing failure {fid}...", end="\r")

        suite = tester.run_suite(testcases, progress_callback=progress)
        tester.print_results(suite)

        # Save results
        test_id = time.strftime("REGRESS_%Y%m%d_%H%M%S")
        result = TestResult(
            verdict=suite.verdict,
            score=100 if suite.failed == 0 else max(0, 100 - suite.failed * 20),
        )
        stats = CampaignStats(total_failures=suite.failed)
        self.db.save_session(TestSession(
            test_id=test_id,
            protocol="REGRESSION",
            verdict=suite.verdict,
            failure_count=suite.failed,
            stats=stats,
            result=result,
        ))

    def cmd_fix(self, failure_id: Optional[str] = None):
        """Analyze a failure and suggest fixes."""
        if failure_id:
            # Load specific failure
            session = self.db.load_session(failure_id)
            if not session or not session.failures:
                print(f"[!] Failure {failure_id} not found")
                return
            failure = session.failures[0]
        else:
            # Use most recent failure
            failures = self.db.get_all_failures()
            if not failures:
                print("[!] No failures in database. Export first.")
                return
            failure = failures[-1]

        print(f"[*] Analyzing failure #{failure.id}: {failure.failure_type}...")

        analysis = self.fixer.analyze_failure(failure)
        print(self.fixer.format_analysis_report(analysis))

        patches = self.fixer.generate_patch(analysis)
        if patches:
            print(f"\n  PROPOSED PATCHES ({len(patches)}):")
            for i, p in enumerate(patches):
                print(f"\n  Patch #{i+1}: {p.file_path} (lines {p.line_start}-{p.line_end})")
                print(f"    Risk: {p.risk_level}")
                print(f"    {p.explanation}")
                print(f"    Old:\n      {p.old_code[:200]}")
                print(f"    New:\n      {p.new_code[:200]}")

            print("\n  ⚠️  Review patches carefully before applying.")
            print("  ⚠️  Human approval is REQUIRED before any changes.")
        else:
            print("\n  No specific patches generated. Manual investigation recommended.")

    def cmd_build(self, target: str = "all"):
        """Build firmware."""
        if target in ("stm32", "all"):
            self.builder.build_stm32()
        if target in ("esp32", "all"):
            self.builder.build_esp32()

    def cmd_flash(self, target: str = "all", port: Optional[str] = None):
        """Flash firmware."""
        if target in ("stm32", "all"):
            self.builder.flash_stm32(port)
        if target in ("esp32", "all"):
            self.builder.flash_esp32(port)

    def run_interactive(self):
        """Run interactive CLI mode."""
        print("\n" + "=" * 50)
        print("  AutoFuzzer PC Companion v4.0")
        print("  Type 'help' for commands, 'quit' to exit")
        print("=" * 50 + "\n")

        while True:
            try:
                cmd = input("autofuzzer> ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                print("\n[+] Goodbye")
                break

            if not cmd:
                continue

            parts = cmd.split()
            command = parts[0]
            args = parts[1:]

            if command == "quit" or command == "exit":
                self.conn.disconnect()
                print("[+] Goodbye")
                break
            elif command == "help":
                self._print_help()
            elif command == "connect":
                self.cmd_connect(args[0] if args else None)
            elif command == "status":
                self.cmd_status()
            elif command == "export":
                self.cmd_export()
            elif command == "report":
                self.cmd_report(args[0] if args else None)
            elif command == "history":
                self.cmd_history()
            elif command == "regress":
                self.cmd_regress()
            elif command == "fix":
                self.cmd_fix(args[0] if args else None)
            elif command == "build":
                self.cmd_build(args[0] if args else "all")
            elif command == "flash":
                self.cmd_flash(args[0] if args else "all")
            elif command == "ports":
                for p in AutoFuzzerConnection.list_ports():
                    print(f"  {p['port']} — {p['description']}")
            else:
                print(f"[!] Unknown command: {command}. Type 'help' for list.")

    def _print_help(self):
        print("""
  COMMANDS:
    connect [port]   Connect to ESP32 (auto-detect if no port)
    status           Query ESP32 status
    ports            List available serial ports
    export           Export failure data from ESP32
    report [id]      Generate reports from stored data
    history          Show test history
    regress          Run regression tests
    fix [id]         Analyze failure and suggest fixes
    build [target]   Build firmware (stm32/esp32/all)
    flash [target]   Flash firmware (stm32/esp32/all)
    help             Show this help
    quit             Exit
        """)


def main():
    """CLI entry point."""
    cli = AutoFuzzerCLI()

    if len(sys.argv) > 1:
        # Command-line mode
        cmd = sys.argv[1]
        args = sys.argv[2:]

        if cmd == "connect":
            cli.cmd_connect(args[0] if args else None)
        elif cmd == "status":
            cli.cmd_status()
        elif cmd == "export":
            cli.cmd_export()
        elif cmd == "report":
            cli.cmd_report(args[0] if args else None)
        elif cmd == "history":
            cli.cmd_history()
        elif cmd == "regress":
            cli.cmd_regress()
        elif cmd == "fix":
            cli.cmd_fix(args[0] if args else None)
        elif cmd == "build":
            cli.cmd_build(args[0] if args else "all")
        elif cmd == "flash":
            cli.cmd_flash(args[0] if args else "all")
        else:
            print(f"Unknown command: {cmd}")
            cli._print_help()
    else:
        # Interactive mode
        cli.run_interactive()


if __name__ == "__main__":
    main()
