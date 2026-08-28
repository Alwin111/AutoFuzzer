# AutoFuzzer Communication Protocol v4.0

## Packet Format

Every AutoFuzzer test frame conforms to this byte layout:

```
SYNC | COMMAND | LENGTH | SEQ_LO | SEQ_HI | PAYLOAD[0..LENGTH-1] | XOR_CHECK
 0xA5    0x01      n       lo       hi          bytes                  chk
```

| Field | Size | Description |
|---|---|---|
| SYNC | 1 byte | `0xA5` — Frame start marker |
| COMMAND | 1 byte | Command byte (default `0x01`) |
| LENGTH | 1 byte | Payload length (not total frame length) |
| SEQ_LO | 1 byte | Sequence number low byte |
| SEQ_HI | 1 byte | Sequence number high byte |
| PAYLOAD | 0–45 bytes | Variable-length payload data |
| XOR_CHECK | 1 byte | XOR of COMMAND through final payload byte |

### XOR_CHECK Calculation

```
XOR_CHECK = COMMAND ^ LENGTH ^ SEQ_LO ^ SEQ_HI ^ PAYLOAD[0] ^ ... ^ PAYLOAD[n-1]
```

### Frame Sizes

| Mutation | LENGTH field | Actual payload bytes | Total frame bytes |
|---|---|---|---|
| Valid | 8 | 8 | 14 |
| Empty | 0 | 0 | 6 |
| Max Length | 32 | 32 | 38 |
| Overlength | 45 | 45 | 51 |
| Bad CRC | 8 | 8 | 14 |
| Truncated | 12 | 12 (sent: 6) | 7 |
| Random | 1–32 | 1–32 | 7–38 |

The reference DUT (STM32) limits valid payload to **32 bytes**.

---

## DUT Response Format

The DUT replies with a 4-byte acknowledgment frame:

```
ACK_SYNC | SEQ_LO | SEQ_HI | STATUS
  0x5A     lo       hi       st
```

| Status Code | Meaning | Description |
|---|---|---|
| `0x00` | ACK | Packet processed successfully |
| `0x02` | NACK — Overlength | Declared length exceeded 32 bytes |
| `0x03` | NACK — Checksum | Received checksum did not match |
| *No response* | Timeout | Parser timed out (truncated frame) |

---

## Mutation Cases

### 1. VALID (mutation index 0)
- Payload length: 8 bytes
- Correct checksum
- Expected DUT response: ACK (`0x00`)
- Tests: Basic communication path

### 2. EMPTY (mutation index 1)
- Payload length: 0 bytes
- Correct checksum
- Expected DUT response: ACK (`0x00`)
- Tests: Zero-length payload handling

### 3. MAX LENGTH (mutation index 2)
- Payload length: 32 bytes (maximum valid)
- Correct checksum
- Expected DUT response: ACK (`0x00`)
- Tests: Maximum buffer allocation

### 4. OVERLENGTH (mutation index 3)
- Payload length: 45 bytes (exceeds 32-byte max)
- Correct checksum for 45 bytes
- Expected DUT response: NACK (`0x02`)
- Tests: Length validation boundary

### 5. BAD CRC (mutation index 4)
- Payload length: 8 bytes
- Checksum intentionally inverted (`checksum ^= 0xFF`)
- Expected DUT response: NACK (`0x03`)
- Tests: Checksum verification

### 6. TRUNCATED (mutation index 5)
- Payload length field: 12 bytes
- Actual transmitted: half the frame bytes
- Expected DUT response: No response (parser timeout)
- Tests: Parser robustness against incomplete frames

### 7. RANDOM (mutation index 6)
- Payload length: 1–32 bytes (PRNG-selected)
- Correct checksum
- Expected DUT response: ACK or defined NACK; heartbeat should never be lost
- Tests: General robustness under random input

---

## ESP32 Response Parser

The ESP32 reads the DUT response via Serial2 (GPIO 16 RX2) and correlates it with the sent packet:

1. Detects `0x5A` sync byte
2. Collects 4 bytes: SYNC + SEQ_LO + SEQ_HI + STATUS
3. Validates sequence number matches expectation
4. Records response status and timing
5. Reports: `RESP seq=N status=0xXX ACK/NACK`

---

## Timing

| Parameter | Value | Description |
|---|---|---|
| Inter-byte timeout | 30ms | DUT parser timeout for truncated frames |
| Response timeout | 100ms | ESP32 waits for DUT response |
| Heartbeat timeout | 350ms | DUT failure threshold |
| Packet interval | 50ms | Minimum time between packets |
| Heartbeat period | 100ms | DUT toggles heartbeat pin |

---

## Deterministic Reproduction

For each packet, the ESP32 captures:

| Field | Purpose |
|---|---|
| Campaign seed | Seed at campaign start |
| Packet seed | PRNG state before this packet |
| PRNG state before | Exact PRNG internal state before generation |
| PRNG state after | Exact PRNG internal state after generation |
| Sequence | Packet sequence number |
| Mutation | Which mutation was applied |
| Packet bytes | Full transmitted bytes |

**Reproduction guarantee**:
```
SAME seed + SAME sequence + SAME mutation + SAME PRNG state
= SAME PACKET
```

---

## Future: Protocol Profiles

v4.0 will support configurable protocol profiles:

```
UART_PROFILE:
  baud: 115200
  data_bits: 8
  stop_bits: 1
  parity: NONE
  sync: 0xA5
  command: 0x01
  length_field: true
  sequence_field: true
  checksum: XOR
  max_payload: 32
  responses: [ACK=0x00, NACK=0x02, NACK=0x03]
```

This allows testing targets that use different protocols without firmware changes.
