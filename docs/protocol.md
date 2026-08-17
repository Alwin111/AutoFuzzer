# AutoFuzzer test frame

Every supported transport carries this byte stream. Transport-specific framing
(for example CAN's 8-byte classic-frame limit) is added later; do not change
the logical format while adding a protocol.

```
SYNC | COMMAND | LENGTH | SEQ_LO | SEQ_HI | PAYLOAD[0..LENGTH-1] | XOR_CHECK
 A5       01       n        ..       ..              ..                ..
```

`XOR_CHECK` is the XOR of `COMMAND` through the final payload byte. `LENGTH`
is a payload length, not the total frame length. The reference DUT accepts a
maximum payload of 32 bytes.

## Initial mutation cases

| Case | Intended outcome on robust DUT |
|---|---|
| Valid | ACK (`0x00`) |
| Empty payload | ACK (`0x00`) |
| Maximum payload (32) | ACK (`0x00`) |
| Oversized length (33+) | NACK (`0x02`) |
| Bad checksum | NACK (`0x03`) |
| Truncated frame | Parser timeout; heartbeat continues |
| Random command/payload | ACK or defined NACK; never lost heartbeat |

The STM32 sends an acknowledgement on UART as `5A | SEQ_LO | SEQ_HI | STATUS`.
Status `00` is accepted, `02` is overlength, and `03` is checksum failure.
No acknowledgement for a truncated packet is expected.

