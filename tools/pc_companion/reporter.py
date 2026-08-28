"""
AutoFuzzer PC Companion — Report Generator

Generates test reports in multiple formats:
  - JSON (machine-readable)
  - HTML (human-readable with styling)
  - Text (plain text for terminal/serial)
  - Packet dump (hex + ASCII)

Reports are saved in structured directories:
  reports/YYYY-MM-DD/TEST_NNN/
"""

import os
import json
import time
from datetime import datetime
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, asdict

from .database import FailureRecord, TestResult, CampaignStats


class ReportGenerator:
    """Generates structured test reports."""

    def __init__(self, output_dir: str = "reports"):
        self.output_dir = Path(output_dir)

    def _get_report_dir(self, test_id: Optional[str] = None) -> Path:
        """Create and return the report directory."""
        date_str = datetime.now().strftime("%Y-%m-%d")
        if test_id is None:
            test_id = datetime.now().strftime("TEST_%H%M%S")
        report_dir = self.output_dir / date_str / test_id
        report_dir.mkdir(parents=True, exist_ok=True)
        return report_dir

    def generate_json(self, result: TestResult, stats: CampaignStats,
                      failures: list[FailureRecord],
                      test_id: Optional[str] = None) -> str:
        """Generate JSON report. Returns path to generated file."""
        report_dir = self._get_report_dir(test_id)

        report = {
            "autofuzzer_version": "4.0",
            "generated_at": datetime.now().isoformat(),
            "test_id": test_id or datetime.now().strftime("TEST_%H%M%S"),
            "summary": {
                "protocol": result.protocol,
                "profile": result.profile,
                "duration_ms": stats.elapsed_ms,
                "total_packets": stats.total_packets,
                "acks": stats.total_acks,
                "nacks": stats.total_nacks,
                "no_response": stats.total_no_response,
                "heartbeat_timeouts": stats.heartbeat_timeouts,
                "total_failures": stats.total_failures,
                "robustness_score": result.score,
                "verdict": result.verdict,
                "category_scores": {
                    "communication": result.comm_score,
                    "boundary": result.bound_score,
                    "malformed": result.malf_score,
                    "recovery": result.recovery_score,
                    "heartbeat": result.hb_score,
                    "random_input": result.rand_score,
                },
            },
            "failures": [asdict(f) for f in failures],
        }

        path = report_dir / "report.json"
        with open(path, "w") as f:
            json.dump(report, f, indent=2)

        print(f"[+] JSON report: {path}")
        return str(path)

    def generate_html(self, result: TestResult, stats: CampaignStats,
                      failures: list[FailureRecord],
                      test_id: Optional[str] = None) -> str:
        """Generate styled HTML report. Returns path to generated file."""
        report_dir = self._get_report_dir(test_id)

        verdict_color = {"PASS": "#10b981", "FAIL": "#ef4444", "INCONCLUSIVE": "#f59e0b"}.get(
            result.verdict, "#6b7280"
        )

        categories = [
            ("Communication", result.comm_score),
            ("Boundary Handling", result.bound_score),
            ("Malformed Input", result.malf_score),
            ("Recovery", result.recovery_score),
            ("Heartbeat Stability", result.hb_score),
            ("Random Input", result.rand_score),
        ]

        failures_html = ""
        for f in failures:
            hex_bytes = " ".join(f"{b:02X}" for b in f.packet_bytes[:f.packet_len])
            failures_html += f"""
            <div class="failure-card">
                <h3>Failure #{f.id} — {f.failure_type}</h3>
                <table>
                    <tr><td>Status</td><td>{f.status}</td></tr>
                    <tr><td>Protocol</td><td>{f.protocol}</td></tr>
                    <tr><td>Sequence</td><td>{f.sequence}</td></tr>
                    <tr><td>Mutation</td><td>{f.mutation}</td></tr>
                    <tr><td>Packet Length</td><td>{f.packet_len} bytes</td></tr>
                    <tr><td>Heartbeat Lost</td><td>{f.heartbeat_lost_ms} ms</td></tr>
                    <tr><td>DUT Responded</td><td>{'Yes' if f.dut_responded else 'No'}</td></tr>
                    <tr><td>Replay Attempts</td><td>{f.replay_attempts}</td></tr>
                    <tr><td>Replay Fails</td><td>{f.replay_fails}</td></tr>
                    <tr><td>Minimized Len</td><td>{f.minimized_len if f.minimized_len else 'N/A'}</td></tr>
                </table>
                <div class="packet-hex">
                    <strong>Packet:</strong> <code>{hex_bytes}</code>
                </div>
            </div>
            """

        score_bars = ""
        for name, score in categories:
            color = "#10b981" if score >= 80 else "#f59e0b" if score >= 50 else "#ef4444"
            score_bars += f"""
            <div class="score-row">
                <span class="score-label">{name}</span>
                <div class="score-bar-bg">
                    <div class="score-bar" style="width:{score}%; background:{color}"></div>
                </div>
                <span class="score-val">{score}/100</span>
            </div>
            """

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AutoFuzzer Report — {result.verdict}</title>
<style>
    * {{ margin: 0; padding: 0; box-sizing: border-box; }}
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
           background: #0f172a; color: #e2e8f0; padding: 2rem; }}
    .container {{ max-width: 900px; margin: 0 auto; }}
    h1 {{ font-size: 1.8rem; margin-bottom: 0.5rem; }}
    .verdict {{ font-size: 2.5rem; font-weight: bold; color: {verdict_color};
                margin: 1rem 0; }}
    .summary {{ background: #1e293b; border-radius: 12px; padding: 1.5rem;
                margin: 1rem 0; }}
    .summary table {{ width: 100%; }}
    .summary td {{ padding: 0.4rem 0; }}
    .summary td:first-child {{ color: #94a3b8; width: 200px; }}
    .score-row {{ display: flex; align-items: center; margin: 0.5rem 0; }}
    .score-label {{ width: 180px; color: #94a3b8; font-size: 0.9rem; }}
    .score-bar-bg {{ flex: 1; height: 20px; background: #334155; border-radius: 4px;
                     margin: 0 1rem; overflow: hidden; }}
    .score-bar {{ height: 100%; border-radius: 4px; transition: width 0.3s; }}
    .score-val {{ width: 70px; text-align: right; font-weight: bold; }}
    .failure-card {{ background: #1e293b; border-left: 4px solid #ef4444;
                     border-radius: 8px; padding: 1rem; margin: 1rem 0; }}
    .failure-card h3 {{ color: #f87171; margin-bottom: 0.5rem; }}
    .failure-card table {{ width: 100%; font-size: 0.9rem; }}
    .failure-card td {{ padding: 0.3rem 0; }}
    .failure-card td:first-child {{ color: #94a3b8; width: 180px; }}
    .packet-hex {{ margin-top: 0.5rem; background: #0f172a; padding: 0.5rem;
                   border-radius: 4px; font-size: 0.8rem; word-break: break-all; }}
    code {{ color: #22d3ee; }}
    .meta {{ color: #64748b; font-size: 0.85rem; margin-top: 1rem; }}
</style>
</head>
<body>
<div class="container">
    <h1>AutoFuzzer v4.0 — Test Report</h1>
    <div class="meta">Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")} | Test ID: {test_id or 'N/A'}</div>

    <div class="verdict">{result.verdict}</div>

    <div class="summary">
        <h2 style="margin-bottom: 1rem;">Robustness Score: {result.score}/100</h2>
        <table>
            <tr><td>Protocol</td><td>{result.protocol}</td></tr>
            <tr><td>Profile</td><td>{result.profile}</td></tr>
            <tr><td>Duration</td><td>{stats.elapsed_ms / 1000:.1f} seconds</td></tr>
            <tr><td>Packets Sent</td><td>{stats.total_packets}</td></tr>
            <tr><td>ACKs</td><td>{stats.total_acks}</td></tr>
            <tr><td>NACKs</td><td>{stats.total_nacks}</td></tr>
            <tr><td>No Response</td><td>{stats.total_no_response}</td></tr>
            <tr><td>Failures</td><td>{stats.total_failures}</td></tr>
        </table>
    </div>

    <div class="summary">
        <h2 style="margin-bottom: 1rem;">Category Scores</h2>
        {score_bars}
    </div>

    {"<h2 style='margin: 1.5rem 0 0.5rem;'>Failures</h2>" if failures else ""}
    {failures_html}

    <div class="meta" style="margin-top: 2rem; text-align: center;">
        AutoFuzzer v4.0 — github.com/Alwin111/AutoFuzzer
    </div>
</div>
</body>
</html>"""

        path = report_dir / "report.html"
        with open(path, "w") as f:
            f.write(html)

        print(f"[+] HTML report: {path}")
        return str(path)

    def generate_text(self, result: TestResult, stats: CampaignStats,
                      failures: list[FailureRecord],
                      test_id: Optional[str] = None) -> str:
        """Generate plain text report. Returns path to generated file."""
        report_dir = self._get_report_dir(test_id)

        lines = [
            "=" * 60,
            "  AUTOFUZZER v4.0 — TEST REPORT",
            "=" * 60,
            f"  Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"  Test ID:   {test_id or 'N/A'}",
            "",
            f"  Protocol:  {result.protocol}",
            f"  Profile:   {result.profile}",
            f"  Duration:  {stats.elapsed_ms / 1000:.1f} seconds",
            "",
            "  PACKETS",
            f"    Sent:       {stats.total_packets}",
            f"    ACKs:       {stats.total_acks}",
            f"    NACKs:      {stats.total_nacks}",
            f"    No Response:{stats.total_no_response}",
            f"    Failures:   {stats.total_failures}",
            "",
            f"  ROBUSTNESS SCORE: {result.score}/100",
            f"  VERDICT:          {result.verdict}",
            "",
            "  CATEGORY SCORES",
            f"    Communication:   {result.comm_score}/100",
            f"    Boundary:        {result.bound_score}/100",
            f"    Malformed:       {result.malf_score}/100",
            f"    Recovery:        {result.recovery_score}/100",
            f"    Heartbeat:       {result.hb_score}/100",
            f"    Random Input:    {result.rand_score}/100",
            "=" * 60,
        ]

        if failures:
            lines.append("")
            lines.append(f"  FAILURES ({len(failures)} total)")
            lines.append("-" * 60)
            for f in failures:
                hex_bytes = " ".join(f"{b:02X}" for b in f.packet_bytes[:f.packet_len])
                lines.extend([
                    f"  #{f.id}: {f.failure_type} — {f.status}",
                    f"    Sequence:   {f.sequence}",
                    f"    Mutation:   {f.mutation}",
                    f"    Pkt Len:    {f.packet_len} bytes",
                    f"    HB Lost:    {f.heartbeat_lost_ms} ms",
                    f"    DUT Resp:   {'Yes' if f.dut_responded else 'No'}",
                    f"    Replays:    {f.replay_fails}/{f.replay_attempts}",
                    f"    Minimized:  {f.minimized_len if f.minimized_len else 'N/A'}",
                    f"    Packet:     {hex_bytes}",
                    "",
                ])

        lines.append("=" * 60)
        lines.append("  AutoFuzzer v4.0 — github.com/Alwin111/AutoFuzzer")
        lines.append("=" * 60)

        text = "\n".join(lines)

        path = report_dir / "report.txt"
        with open(path, "w") as f:
            f.write(text)

        print(f"[+] Text report: {path}")
        return str(path)

    def generate_packet_dump(self, failures: list[FailureRecord],
                             test_id: Optional[str] = None) -> str:
        """Generate hex dump of failing packets. Returns path."""
        report_dir = self._get_report_dir(test_id)

        lines = []
        for f in failures:
            lines.append(f"=== Failure #{f.id} — {f.failure_type} ===")
            lines.append(f"Sequence: {f.sequence}  Mutation: {f.mutation}")
            lines.append(f"Length: {f.packet_len} bytes")
            lines.append("")

            # Hex + ASCII dump
            for offset in range(0, f.packet_len, 16):
                chunk = f.packet_bytes[offset:offset + 16]
                hex_part = " ".join(f"{b:02X}" for b in chunk)
                ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
                lines.append(f"  {offset:04X}  {hex_part:<48s}  {ascii_part}")

            lines.append("")

        path = report_dir / "packets.hex"
        with open(path, "w") as f:
            f.write("\n".join(lines))

        print(f"[+] Packet dump: {path}")
        return str(path)

    def generate_all(self, result: TestResult, stats: CampaignStats,
                     failures: list[FailureRecord],
                     test_id: Optional[str] = None) -> dict[str, str]:
        """Generate all report formats. Returns dict of format -> path."""
        return {
            "json": self.generate_json(result, stats, failures, test_id),
            "html": self.generate_html(result, stats, failures, test_id),
            "text": self.generate_text(result, stats, failures, test_id),
            "hex": self.generate_packet_dump(failures, test_id),
        }
