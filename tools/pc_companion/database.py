"""
AutoFuzzer PC Companion — Failure Database

Provides persistent storage for historical failure records.
Uses JSON files organized by date and test ID.
Supports querying, filtering, and regression test selection.
"""

import json
import os
from datetime import datetime
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional


@dataclass
class FailureRecord:
    """Represents a single failure occurrence."""
    id: int = 0
    failure_type: str = "UNKNOWN"
    status: str = "SUSPECTED"
    protocol: str = "UART"
    profile: str = "QUICK 30s"
    sequence: int = 0
    mutation: str = "RANDOM"
    packet_len: int = 0
    payload_len: int = 0
    seed: str = "0x00000000"
    prng_before: str = "0x00000000"
    prng_after: str = "0x00000000"
    heartbeat_lost_ms: int = 0
    dut_responded: bool = False
    dut_status: str = "0x00"
    dut_resp_time_ms: int = 0
    packets_at_fail: int = 0
    elapsed_ms: int = 0
    replay_attempts: int = 0
    replay_fails: int = 0
    minimized_len: int = 0
    packet_bytes: list[int] = field(default_factory=list)
    timestamp: str = ""


@dataclass
class CampaignStats:
    """Campaign statistics."""
    total_packets: int = 0
    total_acks: int = 0
    total_nacks: int = 0
    total_no_response: int = 0
    heartbeat_timeouts: int = 0
    total_failures: int = 0
    elapsed_ms: int = 0
    duration_ms: int = 0


@dataclass
class TestResult:
    """Test result with scoring."""
    protocol: str = "UART"
    profile: str = "QUICK 30s"
    score: int = 0
    verdict: str = "NOT RUN"
    comm_score: int = 0
    bound_score: int = 0
    malf_score: int = 0
    recovery_score: int = 0
    hb_score: int = 0
    rand_score: int = 0


@dataclass
class TestSession:
    """Complete test session record."""
    test_id: str = ""
    timestamp: str = ""
    protocol: str = "UART"
    profile: str = "QUICK 30s"
    duration_ms: int = 0
    total_packets: int = 0
    score: int = 0
    verdict: str = "NOT RUN"
    failure_count: int = 0
    failures: list[FailureRecord] = field(default_factory=list)
    stats: CampaignStats = field(default_factory=CampaignStats)
    result: TestResult = field(default_factory=TestResult)


class FailureDatabase:
    """Persistent storage for failure records."""

    def __init__(self, db_dir: str = "reports"):
        self.db_dir = Path(db_dir)
        self.db_dir.mkdir(parents=True, exist_ok=True)

    def _get_session_path(self, test_id: str) -> Path:
        """Get the path for a test session file."""
        date_str = datetime.now().strftime("%Y-%m-%d")
        return self.db_dir / date_str / test_id / "session.json"

    def save_session(self, session: TestSession):
        """Save a test session to disk."""
        path = self._get_session_path(session.test_id)
        path.parent.mkdir(parents=True, exist_ok=True)

        data = asdict(session)
        with open(path, "w") as f:
            json.dump(data, f, indent=2)

        print(f"[+] Session saved: {path}")

    def load_session(self, test_id: str, date: Optional[str] = None) -> Optional[TestSession]:
        """Load a test session by ID."""
        if date is None:
            # Search recent dates
            for d in sorted(self.db_dir.iterdir(), reverse=True):
                if d.is_dir() and not d.name.startswith("."):
                    path = d / test_id / "session.json"
                    if path.exists():
                        return self._load_from_path(path)
        else:
            path = self.db_dir / date / test_id / "session.json"
            if path.exists():
                return self._load_from_path(path)

        return None

    def _load_from_path(self, path: Path) -> Optional[TestSession]:
        """Load session from a specific file path."""
        try:
            with open(path) as f:
                data = json.load(f)

            session = TestSession()
            for key, value in data.items():
                if hasattr(session, key):
                    if key == "failures":
                        session.failures = [
                            FailureRecord(**f) for f in value
                        ]
                    elif key == "stats":
                        session.stats = CampaignStats(**value)
                    elif key == "result":
                        session.result = TestResult(**value)
                    else:
                        setattr(session, key, value)

            return session
        except (json.JSONDecodeError, TypeError) as e:
            print(f"[!] Failed to load session {path}: {e}")
            return None

    def list_sessions(self, limit: int = 50) -> list[dict]:
        """List all stored sessions, most recent first."""
        sessions = []

        for date_dir in sorted(self.db_dir.iterdir(), reverse=True):
            if not date_dir.is_dir() or date_dir.name.startswith("."):
                continue
            for test_dir in sorted(date_dir.iterdir(), reverse=True):
                if not test_dir.is_dir():
                    continue
                session_file = test_dir / "session.json"
                if session_file.exists():
                    try:
                        with open(session_file) as f:
                            data = json.load(f)
                        sessions.append({
                            "test_id": data.get("test_id", test_dir.name),
                            "date": date_dir.name,
                            "protocol": data.get("protocol", "?"),
                            "verdict": data.get("verdict", "?"),
                            "score": data.get("score", 0),
                            "failures": data.get("failure_count", 0),
                        })
                        if len(sessions) >= limit:
                            return sessions
                    except (json.JSONDecodeError, KeyError):
                        continue

        return sessions

    def get_all_failures(self) -> list[FailureRecord]:
        """Get all failure records across all sessions."""
        failures = []
        for session_info in self.list_sessions(limit=100):
            session = self.load_session(
                session_info["test_id"],
                session_info["date"]
            )
            if session:
                failures.extend(session.failures)
        return failures

    def get_failures_by_type(self, failure_type: str) -> list[FailureRecord]:
        """Get failures filtered by type."""
        return [f for f in self.get_all_failures() if f.failure_type == failure_type]

    def get_reproducible_failures(self) -> list[FailureRecord]:
        """Get all failures marked as REPRODUCIBLE."""
        return [f for f in self.get_all_failures() if f.status == "REPRODUCIBLE"]

    def get_regression_testcases(self) -> list[FailureRecord]:
        """Get failures suitable for regression testing.

        Returns reproducible failures with minimized testcases
        when available.
        """
        failures = self.get_reproducible_failures()
        # Deduplicate by sequence + mutation
        seen = set()
        unique = []
        for f in failures:
            key = (f.sequence, f.mutation, f.packet_len)
            if key not in seen:
                seen.add(key)
                unique.append(f)
        return unique

    def get_stats(self) -> dict:
        """Get aggregate database statistics."""
        sessions = self.list_sessions(limit=1000)
        failures = self.get_all_failures()

        return {
            "total_sessions": len(sessions),
            "total_failures": len(failures),
            "reproducible": len([f for f in failures if f.status == "REPRODUCIBLE"]),
            "intermittent": len([f for f in failures if f.status == "INTERMITTENT"]),
            "protocols_tested": list(set(s["protocol"] for s in sessions)),
            "verdicts": {
                "PASS": len([s for s in sessions if s["verdict"] == "PASS"]),
                "FAIL": len([s for s in sessions if s["verdict"] == "FAIL"]),
                "INCONCLUSIVE": len([s for s in sessions if s["verdict"] == "INCONCLUSIVE"]),
            },
        }

    def clear_all(self):
        """Clear all stored data. Use with caution."""
        import shutil
        if self.db_dir.exists():
            shutil.rmtree(self.db_dir)
            self.db_dir.mkdir(parents=True)
            print("[+] Database cleared")
