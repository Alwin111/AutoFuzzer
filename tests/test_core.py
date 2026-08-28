"""
AutoFuzzer Host-Side Unit Tests

Tests core logic that can run on a PC without hardware:
  - PRNG determinism and state snapshots
  - XOR checksum calculation
  - Packet generation for all mutation types
  - Mutation selection
  - Response parsing
  - Failure classification
  - Robustness scoring

Run: python -m pytest tests/ -v
"""

import pytest


# ============================================
# PRNG Tests
# ============================================

class Xorshift32:
    """Python reference implementation of the ESP32 Xorshift32 PRNG."""

    def __init__(self, seed=0xC0DEC0DE):
        self._state = seed if seed != 0 else 0xC0DEC0DE

    def next(self):
        self._state ^= (self._state << 13) & 0xFFFFFFFF
        self._state ^= (self._state >> 17)
        self._state ^= (self._state << 5) & 0xFFFFFFFF
        self._state &= 0xFFFFFFFF
        return self._state

    def get_state(self):
        return self._state

    def set_state(self, state):
        self._state = state if state != 0 else 0xC0DEC0DE

    def byte(self):
        return self.next() & 0xFF

    def range(self, max_val):
        if max_val == 0:
            return 0
        return self.next() % max_val


class TestPRNG:
    """Test Xorshift32 PRNG determinism."""

    def test_deterministic_sequence(self):
        """Same seed produces same sequence."""
        prng1 = Xorshift32(0xC0DEC0DE)
        prng2 = Xorshift32(0xC0DEC0DE)

        for _ in range(100):
            assert prng1.next() == prng2.next()

    def test_different_seeds(self):
        """Different seeds produce different sequences."""
        prng1 = Xorshift32(0xC0DEC0DE)
        prng2 = Xorshift32(0xDEADBEEF)

        # At least some values should differ
        vals1 = [prng1.next() for _ in range(10)]
        vals2 = [prng2.next() for _ in range(10)]
        assert vals1 != vals2

    def test_state_snapshot_restore(self):
        """State snapshot and restore reproduces exact sequence."""
        prng = Xorshift32(0xC0DEC0DE)

        # Advance 50 steps
        for _ in range(50):
            prng.next()

        # Save state
        saved_state = prng.get_state()

        # Generate 10 more values
        values = [prng.next() for _ in range(10)]

        # Restore state and verify same 10 values
        prng.set_state(saved_state)
        for v in values:
            assert prng.next() == v

    def test_zero_state_handled(self):
        """Zero state is replaced with default seed."""
        prng = Xorshift32(0)
        assert prng.get_state() == 0xC0DEC0DE

    def test_byte_range(self):
        """Byte values are in 0-255 range."""
        prng = Xorshift32(0xC0DEC0DE)
        for _ in range(1000):
            b = prng.byte()
            assert 0 <= b <= 255

    def test_range_boundaries(self):
        """Range function respects max value."""
        prng = Xorshift32(0xC0DEC0DE)
        for _ in range(1000):
            val = prng.range(7)
            assert 0 <= val < 7


# ============================================
# Checksum Tests
# ============================================

class TestChecksum:
    """Test XOR checksum calculation."""

    def test_checksum_calculation(self):
        """XOR checksum matches ESP32 implementation."""
        def xor_checksum(cmd, length, seq_lo, seq_hi, payload):
            chk = cmd ^ length ^ seq_lo ^ seq_hi
            for b in payload:
                chk ^= b
            return chk

        # Simple case
        assert xor_checksum(0x01, 0x04, 0x00, 0x00, [0xF2, 0x8A, 0x91, 0xAA]) == (
            0x01 ^ 0x04 ^ 0x00 ^ 0x00 ^ 0xF2 ^ 0x8A ^ 0x91 ^ 0xAA
        )

    def test_checksum_empty_payload(self):
        """Checksum with empty payload."""
        def xor_checksum(cmd, length, seq_lo, seq_hi, payload):
            chk = cmd ^ length ^ seq_lo ^ seq_hi
            for b in payload:
                chk ^= b
            return chk

        assert xor_checksum(0x01, 0x00, 0x00, 0x00, []) == 0x01

    def test_bad_crc_inversion(self):
        """Bad CRC mutation inverts the checksum."""
        def xor_checksum(cmd, length, seq_lo, seq_hi, payload):
            chk = cmd ^ length ^ seq_lo ^ seq_hi
            for b in payload:
                chk ^= b
            return chk

        normal = xor_checksum(0x01, 0x08, 0x00, 0x00, [1, 2, 3, 4, 5, 6, 7, 8])
        bad_crc = normal ^ 0xFF
        assert bad_crc != normal
        assert bad_crc == normal ^ 0xFF


# ============================================
# Packet Generation Tests
# ============================================

class TestPacketGeneration:
    """Test packet structure for all mutation types."""

    SYNC = 0xA5
    CMD = 0x01

    def _build_packet(self, prng, mutation, sequence=0):
        """Build packet matching ESP32 uart_fuzzer_send logic."""
        buffer = [0] * 64
        payload_len = 8

        if mutation == 0:   # VALID
            payload_len = 8
        elif mutation == 1: # EMPTY
            payload_len = 0
        elif mutation == 2: # MAX_LEN
            payload_len = 32
        elif mutation == 3: # OVERLENGTH
            payload_len = 45
        elif mutation == 4: # BAD_CRC
            payload_len = 8
        elif mutation == 5: # TRUNCATED
            payload_len = 12
        elif mutation == 6: # RANDOM
            payload_len = (prng.range(32) + 1)

        buffer[0] = self.SYNC
        buffer[1] = self.CMD
        buffer[2] = payload_len
        buffer[3] = sequence & 0xFF
        buffer[4] = (sequence >> 8) & 0xFF

        checksum = self.CMD ^ payload_len ^ buffer[3] ^ buffer[4]
        for i in range(payload_len):
            b = prng.byte()
            buffer[5 + i] = b
            checksum ^= b

        if mutation == 4:
            checksum ^= 0xFF

        total_bytes = 5 + payload_len
        buffer[total_bytes] = checksum
        total_bytes += 1

        if mutation == 5:
            total_bytes //= 2

        return buffer[:total_bytes], payload_len

    def test_valid_packet_structure(self):
        """Valid packet has correct header and checksum."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, _ = self._build_packet(prng, 0)

        assert pkt[0] == self.SYNC
        assert pkt[1] == self.CMD
        assert pkt[2] == 8  # payload len
        assert len(pkt) == 14  # 5 header + 8 payload + 1 checksum

    def test_empty_packet(self):
        """Empty payload packet has minimal size."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, pay_len = self._build_packet(prng, 1)

        assert pay_len == 0
        assert len(pkt) == 6  # 5 header + 0 payload + 1 checksum

    def test_max_len_packet(self):
        """Max length packet uses 32-byte payload."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, pay_len = self._build_packet(prng, 2)

        assert pay_len == 32
        assert len(pkt) == 38  # 5 + 32 + 1

    def test_overlength_packet(self):
        """Overlength packet declares >32 bytes."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, pay_len = self._build_packet(prng, 3)

        assert pay_len == 45
        assert pkt[2] == 45  # Length field says 45

    def test_bad_crc_packet(self):
        """Bad CRC packet has inverted checksum."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, _ = self._build_packet(prng, 4)

        # Verify checksum is XOR of cmd through payload
        expected_chk = 0
        for b in pkt[1:-1]:
            expected_chk ^= b
        expected_chk ^= 0xFF  # Inverted

        assert pkt[-1] == expected_chk

    def test_truncated_packet(self):
        """Truncated packet sends only half the bytes."""
        prng = Xorshift32(0xC0DEC0DE)
        pkt, _ = self._build_packet(prng, 5)

        # Full frame would be 5 + 12 + 1 = 18, truncated = 9
        assert len(pkt) == 9

    def test_deterministic_replay(self):
        """Same seed + same mutation = same packet."""
        prng1 = Xorshift32(0xC0DEC0DE)
        prng2 = Xorshift32(0xC0DEC0DE)

        pkt1, _ = self._build_packet(prng1, 6, sequence=42)
        pkt2, _ = self._build_packet(prng2, 6, sequence=42)

        assert pkt1 == pkt2

    def test_packet_sync_byte(self):
        """Every packet starts with SYNC byte."""
        prng = Xorshift32(0xC0DEC0DE)
        for mut in range(7):
            pkt, _ = self._build_packet(prng, mut)
            assert pkt[0] == self.SYNC, f"Mutation {mut} missing SYNC"


# ============================================
# Mutation Selection Tests
# ============================================

class TestMutationSelection:
    """Test phase-based mutation weight selection."""

    # Phase weights from ESP32 firmware
    PHASE_WEIGHTS = [
        [80, 15, 5, 0, 0, 0, 0],    # BASELINE
        [10, 30, 40, 10, 0, 10, 0],  # BOUNDARY
        [5, 5, 10, 60, 5, 10, 5],    # OVERLENGTH
        [5, 5, 5, 10, 60, 5, 10],    # CHECKSUM
        [5, 5, 5, 10, 10, 35, 30],   # MALFORMED
        [5, 5, 5, 10, 10, 15, 50],   # RANDOM
        [10, 10, 10, 15, 15, 15, 25], # FREEFORM
    ]

    def test_baseline_favors_valid(self):
        """Baseline phase heavily favors VALID mutation."""
        weights = self.PHASE_WEIGHTS[0]
        total = sum(weights)
        valid_pct = weights[0] / total
        assert valid_pct > 0.7, f"Baseline valid weight {valid_pct:.2f} < 70%"

    def test_boundary_favors_max_empty(self):
        """Boundary phase favors MAX_LEN and EMPTY."""
        weights = self.PHASE_WEIGHTS[1]
        boundary_total = weights[1] + weights[2]  # EMPTY + MAX_LEN
        total = sum(weights)
        assert boundary_total / total > 0.5

    def test_overlength_favors_overlength(self):
        """Overlength phase favors overlength mutation."""
        weights = self.PHASE_WEIGHTS[2]
        assert weights[3] > weights[0]  # Overweight > Valid weight

    def test_all_weights_non_negative(self):
        """All phase weights are non-negative."""
        for phase_weights in self.PHASE_WEIGHTS:
            for w in phase_weights:
                assert w >= 0

    def test_all_phases_have_total_gt_zero(self):
        """All phases have non-zero total weight."""
        for i, pw in enumerate(self.PHASE_WEIGHTS):
            assert sum(pw) > 0, f"Phase {i} has zero total weight"


# ============================================
# Response Parser Tests
# ============================================

class TestResponseParser:
    """Test UART response parsing logic."""

    ACK_SYNC = 0x5A

    def _parse_response(self, data: bytes):
        """Simulate ESP32 UART parser state machine."""
        if len(data) < 4:
            return None
        if data[0] != self.ACK_SYNC:
            return None

        return {
            "sync": data[0],
            "seq": data[1] | (data[2] << 8),
            "status": data[3],
        }

    def test_ack_response(self):
        """Parse valid ACK response."""
        resp = self._parse_response(bytes([0x5A, 0x00, 0x00, 0x00]))
        assert resp is not None
        assert resp["status"] == 0x00
        assert resp["seq"] == 0

    def test_nack_overlength(self):
        """Parse NACK overlength response."""
        resp = self._parse_response(bytes([0x5A, 0x01, 0x00, 0x02]))
        assert resp is not None
        assert resp["status"] == 0x02
        assert resp["seq"] == 1

    def test_nack_checksum(self):
        """Parse NACK checksum response."""
        resp = self._parse_response(bytes([0x5A, 0x2A, 0x01, 0x03]))
        assert resp is not None
        assert resp["status"] == 0x03
        assert resp["seq"] == 0x012A

    def test_invalid_sync(self):
        """Reject response with wrong sync byte."""
        resp = self._parse_response(bytes([0x00, 0x00, 0x00, 0x00]))
        assert resp is None

    def test_short_data(self):
        """Reject incomplete response."""
        resp = self._parse_response(bytes([0x5A, 0x00]))
        assert resp is None

    def test_sequence_number_16bit(self):
        """Sequence number spans two bytes correctly."""
        resp = self._parse_response(bytes([0x5A, 0xFF, 0xFF, 0x00]))
        assert resp is not None
        assert resp["seq"] == 0xFFFF


# ============================================
# Failure Classification Tests
# ============================================

class TestFailureClassification:
    """Test failure type classification logic."""

    def classify_heartbeat(self, timeout_ms, dut_responded):
        """Classify heartbeat timeout based on evidence."""
        if dut_responded and timeout_ms > 1000:
            return "WATCHDOG_RESET"
        if timeout_ms > 2000:
            return "HEARTBEAT_TIMEOUT"
        if timeout_ms > 350:
            return "HEARTBEAT_TIMEOUT"
        if timeout_ms < 50:
            return "POWER_FAILURE"
        return "HEARTBEAT_TIMEOUT"

    def test_standard_timeout(self):
        """Standard heartbeat timeout classified correctly."""
        assert self.classify_heartbeat(400, False) == "HEARTBEAT_TIMEOUT"

    def test_long_timeout(self):
        """Very long timeout classified correctly."""
        assert self.classify_heartbeat(3000, False) == "HEARTBEAT_TIMEOUT"

    def test_watchdog_reset(self):
        """DUT responded but heartbeat died — watchdog."""
        assert self.classify_heartbeat(1500, True) == "WATCHDOG_RESET"

    def test_power_failure(self):
        """Very short timeout suggests power issue."""
        assert self.classify_heartbeat(30, False) == "POWER_FAILURE"

    def test_boundary_timeout(self):
        """Timeout exactly at threshold."""
        assert self.classify_heartbeat(350, False) == "HEARTBEAT_TIMEOUT"

    def test_short_but_not_power(self):
        """Timeout between 50-350ms is heartbeat timeout."""
        assert self.classify_heartbeat(200, False) == "HEARTBEAT_TIMEOUT"


# ============================================
# Robustness Scoring Tests
# ============================================

class TestScoring:
    """Test robustness score calculation."""

    def _calc_score(self, good, total):
        if total == 0:
            return 100
        if good >= total:
            return 100
        return (good * 100) // total

    def test_all_pass(self):
        """All good = 100 score."""
        assert self._calc_score(100, 100) == 100

    def test_all_fail(self):
        """All bad = 0 score."""
        assert self._calc_score(0, 100) == 0

    def test_half(self):
        """50% good = 50 score."""
        assert self._calc_score(50, 100) == 50

    def test_no_tests(self):
        """No tests = 100 (no failures observed)."""
        assert self._calc_score(0, 0) == 100

    def test_weighted_overall_score(self):
        """Weighted average produces expected result."""
        # All categories at 100
        scores = [100, 100, 100, 100, 100, 100]
        weights = [25, 20, 20, 15, 10, 10]
        weighted = sum(s * w for s, w in zip(scores, weights)) / 100
        assert weighted == 100

        # Half scores
        scores = [50, 50, 50, 50, 50, 50]
        weighted = sum(s * w for s, w in zip(scores, weights)) / 100
        assert weighted == 50

    def test_verdict_determination(self):
        """Verdict based on failure count."""
        def verdict(failures, packets):
            if failures == 0 and packets > 0:
                return "PASS"
            elif failures > 0:
                return "FAIL"
            return "INCONCLUSIVE"

        assert verdict(0, 100) == "PASS"
        assert verdict(1, 100) == "FAIL"
        assert verdict(0, 0) == "INCONCLUSIVE"


# ============================================
# Database Tests
# ============================================

class TestDatabase:
    """Test failure database operations."""

    def test_session_save_load(self, tmp_path):
        """Save and load a test session."""
        from tools.pc_companion.database import (
            FailureDatabase, TestSession, CampaignStats, TestResult
        )

        db = FailureDatabase(str(tmp_path))

        session = TestSession(
            test_id="TEST_001",
            protocol="UART",
            profile="QUICK 30s",
            verdict="PASS",
            score=95,
            stats=CampaignStats(total_packets=100),
            result=TestResult(verdict="PASS", score=95),
        )

        db.save_session(session)
        loaded = db.load_session("TEST_001")

        assert loaded is not None
        assert loaded.test_id == "TEST_001"
        assert loaded.verdict == "PASS"
        assert loaded.score == 95

    def test_list_sessions(self, tmp_path):
        """List stored sessions."""
        from tools.pc_companion.database import (
            FailureDatabase, TestSession
        )

        db = FailureDatabase(str(tmp_path))

        for i in range(3):
            db.save_session(TestSession(test_id=f"TEST_{i:03d}"))

        sessions = db.list_sessions()
        assert len(sessions) == 3

    def test_empty_database(self, tmp_path):
        """Empty database returns empty results."""
        from tools.pc_companion.database import FailureDatabase

        db = FailureDatabase(str(tmp_path))
        assert db.list_sessions() == []
        assert db.get_all_failures() == []
        assert db.get_stats()["total_sessions"] == 0


# ============================================
# Integration Test
# ============================================

class TestEndToEnd:
    """End-to-end packet generation and parsing."""

    def test_valid_packet_parses_correctly(self):
        """Build a valid packet, verify it would be accepted."""
        prng = Xorshift32(0xC0DEC0DE)

        # Build packet
        cmd = 0x01
        length = 8
        seq = 42
        payload = [prng.byte() for _ in range(length)]

        checksum = cmd ^ length ^ (seq & 0xFF) ^ ((seq >> 8) & 0xFF)
        for b in payload:
            checksum ^= b

        packet = [0xA5, cmd, length, seq & 0xFF, (seq >> 8) & 0xFF] + payload + [checksum]

        # Verify structure
        assert packet[0] == 0xA5
        assert packet[1] == cmd
        assert packet[2] == length
        assert len(packet) == 5 + length + 1  # header + payload + checksum

        # Verify checksum on "receiver" side
        recv_checksum = 0
        for b in packet[1:-1]:
            recv_checksum ^= b
        assert recv_checksum == packet[-1], "Checksum mismatch"

    def test_full_reproduction_cycle(self):
        """Simulate full reproduce cycle: save state, generate, restore, generate same."""
        prng1 = Xorshift32(0xC0DEC0DE)
        prng2 = Xorshift32(0xC0DEC0DE)

        # Generate packet 1
        state_before = prng1.get_state()
        payload1 = [prng1.byte() for _ in range(8)]
        state_after = prng1.get_state()

        # Advance prng2 to same point
        prng2.set_state(state_before)
        payload2 = [prng2.byte() for _ in range(8)]

        assert payload1 == payload2
        assert prng2.get_state() == state_after
