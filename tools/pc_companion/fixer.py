"""
AutoFuzzer PC Companion — AI Fix Workflow

Provides AI-assisted debugging for DUT firmware failures.

CRITICAL SAFETY RULES:
  1. AI MUST NOT blindly modify or flash firmware
  2. AI MUST distinguish observed vs confirmed vs suspected
  3. AI MUST request human approval before any changes
  4. AI MUST show explanation before proposing patches

Workflow:
  1. Collect failure information
  2. Collect protocol information
  3. Collect source code
  4. Locate relevant parser/handler
  5. Analyze likely root cause
  6. Generate proposed patch
  7. Show explanation
  8. Request human approval
"""

import os
import re
from pathlib import Path
from dataclasses import dataclass
from typing import Optional

from .database import FailureRecord


@dataclass
class RootCauseAnalysis:
    """Result of AI analysis of a failure."""
    failure_type: str
    observed: str            # What was observed
    evidence: list[str]      # Evidence gathered
    likely_cause: str        # Likely root cause (suspected, not confirmed)
    confidence: str          # HIGH / MEDIUM / LOW
    relevant_files: list[str]  # Source files involved
    relevant_lines: list[dict]  # Line numbers and context
    recommended_fix: str     # Description of recommended fix
    risk_level: str          # LOW / MEDIUM / HIGH


@dataclass
class ProposedPatch:
    """A proposed code change."""
    file_path: str
    line_start: int
    line_end: int
    old_code: str
    new_code: str
    explanation: str
    risk_level: str


class AIFixer:
    """AI-assisted debugging workflow for DUT firmware failures."""

    def __init__(self, firmware_dir: Optional[str] = None):
        if firmware_dir:
            self.firmware_dir = Path(firmware_dir)
        else:
            self.firmware_dir = Path(__file__).parent.parent.parent / "firmware" / "stm32"

    def analyze_failure(self, failure: FailureRecord,
                        source_dir: Optional[str] = None) -> RootCauseAnalysis:
        """Analyze a failure and identify likely root cause.

        This is a heuristic/rule-based analysis.
        For deeper analysis, the output can be fed to an LLM.
        """
        src_dir = Path(source_dir) if source_dir else self.firmware_dir / "src"

        # Gather source files
        source_files = list(src_dir.rglob("*.cpp")) + list(src_dir.rglob("*.h"))

        # Analyze based on failure type
        analysis = RootCauseAnalysis(
            failure_type=failure.failure_type,
            observed=self._describe_observed(failure),
            evidence=[],
            likely_cause="Unknown",
            confidence="LOW",
            relevant_files=[],
            relevant_lines=[],
            recommended_fix="Further investigation needed",
            risk_level="MEDIUM",
        )

        # Pattern matching for common failure causes
        if failure.failure_type == "HEARTBEAT_TIMEOUT":
            analysis = self._analyze_heartbeat_timeout(failure, source_files)
        elif failure.failure_type == "NO_RESPONSE":
            analysis = self._analyze_no_response(failure, source_files)
        elif failure.failure_type == "PROTOCOL_ERROR":
            analysis = self._analyze_protocol_error(failure, source_files)
        elif failure.failure_type == "WATCHDOG_RESET":
            analysis = self._analyze_watchdog_reset(failure, source_files)
        else:
            analysis.evidence.append(f"Failure type: {failure.failure_type}")
            analysis.evidence.append(f"Mutation: {failure.mutation}")
            analysis.evidence.append(f"Packet length: {failure.packet_len}")

        return analysis

    def _describe_observed(self, failure: FailureRecord) -> str:
        """Generate human-readable observation description."""
        parts = [
            f"Heartbeat stopped after {failure.heartbeat_lost_ms}ms",
            f"during {failure.mutation} mutation",
            f"(packet {failure.sequence}, {failure.packet_len} bytes)",
        ]
        if failure.dut_responded:
            parts.append(f"DUT did respond (status={failure.dut_status})")
        else:
            parts.append("DUT did not respond")
        return ". ".join(parts) + "."

    def _analyze_heartbeat_timeout(self, failure: FailureRecord,
                                    source_files: list[Path]) -> RootCauseAnalysis:
        """Analyze heartbeat timeout failures."""
        analysis = RootCauseAnalysis(
            failure_type="HEARTBEAT_TIMEOUT",
            observed=self._describe_observed(failure),
            evidence=[
                f"Mutation: {failure.mutation}",
                f"Packet length: {failure.packet_len}",
                f"Payload length: {failure.payload_len}",
                f"Heartbeat timeout: {failure.heartbeat_lost_ms}ms",
                f"DUT responded: {failure.dut_responded}",
            ],
            likely_cause="Unknown",
            confidence="LOW",
            relevant_files=[],
            relevant_lines=[],
            recommended_fix="Investigate parser behavior with this specific mutation",
            risk_level="MEDIUM",
        )

        # Look for parser-related source code
        for f in source_files:
            content = f.read_text(errors="replace") if f.exists() else ""
            if "consumeByte" in content or "parser" in content.lower():
                analysis.relevant_files.append(str(f))
                analysis.evidence.append(f"Found parser logic in {f.name}")

        # Mutation-specific analysis
        if failure.mutation == "OVERLENGTH":
            analysis.likely_cause = (
                "Likely: Parser accepts length field without proper bounds "
                "check before buffer allocation. The DUT may write beyond "
                "buffer boundaries when length > MAX_PAYLOAD."
            )
            analysis.confidence = "MEDIUM"
            analysis.recommended_fix = (
                "Ensure length validation happens before any memcpy or "
                "buffer write. Reject packets with length > MAX_PAYLOAD "
                "immediately."
            )
        elif failure.mutation == "TRUNCATED":
            analysis.likely_cause = (
                "Likely: Parser waits indefinitely for expected bytes "
                "that never arrive. If no timeout mechanism exists, "
                "the parser enters a deadlock state."
            )
            analysis.confidence = "MEDIUM"
            analysis.recommended_fix = (
                "Add an inter-byte timeout (e.g., 30ms). If no byte "
                "arrives within the timeout, reset parser state to "
                "WaitSync."
            )
        elif failure.mutation == "BAD_CRC":
            analysis.likely_cause = (
                "Likely: Parser crashes or enters undefined state when "
                "checksum mismatch triggers error handling. Could be a "
                "null pointer, array index issue, or missing error handler."
            )
            analysis.confidence = "MEDIUM"
            analysis.recommended_fix = (
                "Ensure checksum error handling sends NACK and resets "
                "parser state cleanly. Do not access payload buffer "
                "after checksum failure."
            )
        elif failure.mutation == "RANDOM":
            analysis.likely_cause = (
                "Likely: Random bytes triggered an unexpected code path "
                "in the parser. May indicate missing state validation "
                "or unhandled command values."
            )
            analysis.confidence = "LOW"
            analysis.recommended_fix = (
                "Add default case handling in parser switch statements. "
                "Validate all state transitions. Reset on unexpected "
                "input."
            )
        elif failure.mutation == "EMPTY":
            analysis.likely_cause = (
                "Likely: Zero-length payload causes an edge case in "
                "the parser — possibly accessing an empty buffer or "
                "skipping a required state."
            )
            analysis.confidence = "LOW"
            analysis.recommended_fix = (
                "Handle zero-length payload as a valid case. Skip "
                "payload state entirely when length == 0."
            )
        elif failure.mutation == "MAX_LEN":
            analysis.likely_cause = (
                "Likely: Maximum payload length (32 bytes) may overflow "
                "a buffer that is sized for fewer bytes, or cause a "
                "stack overflow if payload is on the stack."
            )
            analysis.confidence = "MEDIUM"
            analysis.recommended_fix = (
                "Verify buffer size matches MAX_PAYLOAD. Use static "
                "assertions to ensure buffer adequacy."
            )

        return analysis

    def _analyze_no_response(self, failure: FailureRecord,
                              source_files: list[Path]) -> RootCauseAnalysis:
        """Analyze no-response failures."""
        return RootCauseAnalysis(
            failure_type="NO_RESPONSE",
            observed=self._describe_observed(failure),
            evidence=[
                f"Mutation: {failure.mutation}",
                f"Packet length: {failure.packet_len}",
                "DUT sent no response",
            ],
            likely_cause=(
                "Likely: Parser is stuck waiting for more bytes in a "
                "state that requires additional input. The parser "
                "timeout may be too long or absent."
            ),
            confidence="MEDIUM",
            relevant_files=[str(f) for f in source_files if "parser" in f.name.lower()],
            relevant_lines=[],
            recommended_fix=(
                "Add or reduce inter-byte timeout. Ensure parser can "
                "reset from any state after timeout."
            ),
            risk_level="LOW",
        )

    def _analyze_protocol_error(self, failure: FailureRecord,
                                 source_files: list[Path]) -> RootCauseAnalysis:
        """Analyze protocol error failures."""
        return RootCauseAnalysis(
            failure_type="PROTOCOL_ERROR",
            observed=self._describe_observed(failure),
            evidence=[
                f"Mutation: {failure.mutation}",
                f"DUT status: {failure.dut_status}",
            ],
            likely_cause=(
                "Likely: DUT sent unexpected response code or response "
                "frame was malformed."
            ),
            confidence="LOW",
            relevant_files=[],
            relevant_lines=[],
            recommended_fix="Verify response generation code matches expected format",
            risk_level="LOW",
        )

    def _analyze_watchdog_reset(self, failure: FailureRecord,
                                 source_files: list[Path]) -> RootCauseAnalysis:
        """Analyze watchdog reset failures."""
        return RootCauseAnalysis(
            failure_type="WATCHDOG_RESET",
            observed=self._describe_observed(failure),
            evidence=[
                f"Mutation: {failure.mutation}",
                "DUT responded then heartbeat died",
                "Indicates possible infinite loop or peripheral lockup",
            ],
            likely_cause=(
                "Likely: DUT entered an infinite loop or deadlocked "
                "after processing the packet. Watchdog timer then "
                "reset the MCU."
            ),
            confidence="MEDIUM",
            relevant_files=[],
            relevant_lines=[],
            recommended_fix=(
                "Check for loops without exit conditions in packet "
                "handler. Verify all peripheral operations have "
                "timeouts."
            ),
            risk_level="MEDIUM",
        )

    def generate_patch(self, analysis: RootCauseAnalysis) -> list[ProposedPatch]:
        """Generate proposed patches based on analysis.

        Returns a list of ProposedPatch objects.
        These are suggestions — human approval is required.
        """
        patches = []

        if not analysis.relevant_files:
            return patches

        for file_path in analysis.relevant_files:
            path = Path(file_path)
            if not path.exists():
                continue

            content = path.read_text(errors="replace")
            lines = content.split("\n")

            # Generate patches based on failure type and analysis
            if analysis.failure_type in ("HEARTBEAT_TIMEOUT", "NO_RESPONSE"):
                # Look for parser state machines that might lack timeout
                for i, line in enumerate(lines):
                    if "consumeByte" in line and "state" in line:
                        # Suggest adding timeout if not present
                        has_timeout = any("timeout" in l.lower() for l in lines[max(0, i-5):i+5])
                        if not has_timeout and i + 5 < len(lines):
                            old_block = "\n".join(lines[i:min(i+5, len(lines))])
                            patches.append(ProposedPatch(
                                file_path=str(file_path),
                                line_start=i + 1,
                                line_end=min(i + 5, len(lines)),
                                old_code=old_block,
                                new_code=old_block + "\n  // AutoFuzzer: Add inter-byte timeout here",
                                explanation="This parser state handler may lack a timeout mechanism",
                                risk_level="MEDIUM",
                            ))

        return patches

    def format_analysis_report(self, analysis: RootCauseAnalysis) -> str:
        """Format analysis as a human-readable report."""
        lines = [
            "=" * 60,
            "  AUTOFUZZER AI ANALYSIS REPORT",
            "=" * 60,
            "",
            f"  Failure Type:    {analysis.failure_type}",
            f"  Confidence:      {analysis.confidence}",
            f"  Risk Level:      {analysis.risk_level}",
            "",
            "  OBSERVED:",
            f"    {analysis.observed}",
            "",
            "  EVIDENCE:",
        ]
        for e in analysis.evidence:
            lines.append(f"    - {e}")

        lines.extend([
            "",
            "  LIKELY ROOT CAUSE:",
        ])
        for line in analysis.likely_cause.split(". "):
            lines.append(f"    {line.strip()}")

        lines.extend([
            "",
            "  RECOMMENDED FIX:",
        ])
        for line in analysis.recommended_fix.split(". "):
            lines.append(f"    {line.strip()}")

        if analysis.relevant_files:
            lines.extend([
                "",
                "  RELEVANT FILES:",
            ])
            for f in analysis.relevant_files:
                lines.append(f"    {f}")

        lines.extend([
            "",
            "=" * 60,
            "",
            "  IMPORTANT: This analysis is a SUGGESTION, not a confirmation.",
            "  AI identified a POTENTIAL issue based on available evidence.",
            "  Human review and verification is REQUIRED before any changes.",
            "",
            "  DO NOT apply patches without understanding the implications.",
            "=" * 60,
        ])

        return "\n".join(lines)
