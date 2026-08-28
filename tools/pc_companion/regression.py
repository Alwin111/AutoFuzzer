"""
AutoFuzzer PC Companion — Regression Testing

Replays known failure testcases against the DUT to verify
that previously discovered issues are fixed.

Every discovered failure becomes a regression testcase.
Future tests should optionally include:
  - REPLAY known failures
  - Verify fixes hold
  - Detect regressions (previously fixed issues returning)
"""

import time
from dataclasses import dataclass
from typing import Optional

from .connection import AutoFuzzerConnection
from .database import FailureRecord


@dataclass
class RegressionResult:
    """Result of a single regression testcase."""
    failure_id: str
    failure_type: str
    mutation: str
    sequence: int
    packet_len: int
    passed: bool       # True if failure no longer occurs
    response: str      # DUT response description
    heartbeat_ok: bool


@dataclass
class RegressionSuite:
    """Complete regression test results."""
    total: int = 0
    passed: int = 0
    failed: int = 0
    results: list[RegressionResult] = None

    def __post_init__(self):
        if self.results is None:
            self.results = []

    @property
    def verdict(self) -> str:
        if self.total == 0:
            return "NO TESTS"
        if self.failed == 0:
            return "ALL PASS"
        return f"{self.failed} REGRESSION(S)"


class RegressionTester:
    """Replay known failures to verify fixes."""

    def __init__(self, connection: AutoFuzzerConnection):
        self.conn = connection
        self.max_wait_ms = 2000  # Max wait for DUT response

    def run_single(self, failure: FailureRecord, timeout: float = 5.0) -> RegressionResult:
        """Replay a single failure testcase.

        Sends the exact packet that caused the failure
        and checks if the DUT still fails.
        """
        result = RegressionResult(
            failure_id=str(failure.id),
            failure_type=failure.failure_type,
            mutation=failure.mutation,
            sequence=failure.sequence,
            packet_len=failure.packet_len,
            passed=False,
            response="NO RESPONSE",
            heartbeat_ok=False,
        )

        if not self.conn.is_connected():
            result.response = "NOT CONNECTED"
            return result

        # Send the packet bytes directly
        if failure.packet_bytes and failure.packet_len > 0:
            packet = bytes(failure.packet_bytes[:failure.packet_len])
            self.conn._serial.write(packet)
            time.sleep(0.5)
        else:
            result.response = "NO PACKET DATA"
            return result

        # Check heartbeat
        lines = self.conn.send_and_wait("STATUS", wait=2.0)
        heartbeat_ok = any("heartbeat: OK" in l.lower() or "heartbeat: ok" in l for l in lines)
        result.heartbeat_ok = heartbeat_ok

        # Check for response
        for line in lines:
            if "RESP" in line and "seq=" in line:
                result.response = line.strip()
                break

        # If heartbeat is OK after sending the failing packet, it's a pass
        result.passed = heartbeat_ok

        return result

    def run_suite(self, failures: list[FailureRecord],
                  progress_callback=None) -> RegressionSuite:
        """Run regression test suite against all known failures."""
        suite = RegressionSuite(total=len(failures))

        for i, failure in enumerate(failures):
            if progress_callback:
                progress_callback(i + 1, len(failures), failure.id)

            result = self.run_single(failure)
            suite.results.append(result)

            if result.passed:
                suite.passed += 1
            else:
                suite.failed += 1

            # Brief pause between testcases
            time.sleep(0.3)

        return suite

    def format_results(self, suite: RegressionSuite) -> str:
        """Format regression results as readable text."""
        lines = [
            "=" * 50,
            "  REGRESSION TEST RESULTS",
            "=" * 50,
            f"  Total:   {suite.total}",
            f"  Passed:  {suite.passed}",
            f"  Failed:  {suite.failed}",
            f"  Verdict: {suite.verdict}",
            "-" * 50,
        ]

        for r in suite.results:
            status = "PASS" if r.passed else "FAIL"
            lines.append(
                f"  [{status}] {r.failure_id}: {r.failure_type} "
                f"(mut={r.mutation}, seq={r.sequence}, len={r.packet_len})"
            )
            if not r.passed:
                lines.append(f"         Response: {r.response}")

        lines.extend(["", "=" * 50])
        return "\n".join(lines)

    def print_results(self, suite: RegressionSuite):
        """Print regression results to console."""
        print(self.format_results(suite))
