# AutoFuzzer — Logic Analyzer Integration Plan

## Overview

Integrate the Saleae-compatible 8-channel 24MHz logic analyzer into AutoFuzzer
to add hardware-level bus verification, protocol decode, failure forensics,
and signal integrity monitoring.

This transforms AutoFuzzer from a **black-box fuzzer** (trusts its own sends)
into a **white-box fuzzer** (independently verifies what's on the wire).

---

## Current State

### What We Have
- AutoFuzzer v4.0 ESP32 fuzzer (UART, SPI, I2C)
- 3 target boards tested (STM32, Nano, ESP32)
- 41 test reports in `reports/`
- Saleae-compatible 8CH 24MHz logic analyzer on the PCB
- Male header pins on the PCB for HB, TX, RX, SDA, SCL, CS, CLK, MISO, MOSI

### What's Missing
- No independent verification of what the ESP32 sends
- No hardware timestamps on packets
- SoftwareSerial misses ACKs/NACKs (0 received vs actual)
- No bus state capture during failures
- No signal integrity data
- No correlation between ESP32 packet log and actual wire data

---

## Hardware Setup

### Logic Analyzer Channel Mapping

```
LA Channel    Signal        ESP32 Pin    Direction
──────────    ──────        ────────     ─────────
CH0           UART TX       GPIO 17      ESP32 → DUT
CH1           UART RX       GPIO 16      DUT → ESP32
CH2           SPI CLK       GPIO 18      ESP32 → DUT
CH3           SPI MOSI      GPIO 23      ESP32 → DUT
CH4           SPI MISO      GPIO 19      DUT → ESP32
CH5           SPI CS        GPIO 5       ESP32 → DUT
CH6           I2C SDA       GPIO 21      Bidirectional
CH7           Heartbeat     GPIO 25      DUT → ESP32
```

### Probe Connection

The male headers on the PCB already break out all signals.
LA probe clips connect directly to these headers.

```
PCB Male Headers          Logic Analyzer Probes
────────────────          ────────────────────
  HB  ────────────────→   CH7 (Heartbeat)
  TX  ────────────────→   CH0 (UART TX)
  RX  ────────────────→   CH1 (UART RX)
  SDA ────────────────→   CH6 (I2C SDA)
  SCL ────────────────→   (spare / future)
  CS  ────────────────→   CH5 (SPI CS)
  CLK ────────────────→   CH2 (SPI CLK)
  MISO ───────────────→   CH4 (SPI MISO)
  MOSI ───────────────→   CH3 (SPI MOSI)
```

### Important Notes

- LA probes are **high impedance** (>100kΩ) — they do not affect the signals
- Common ground between LA and AutoFuzzer PCB is required
- LA sample rate per channel: 24MHz ÷ active channels
  - 2 channels (UART): 12MHz effective → great for 115200 baud
  - 8 channels (all): 3MHz effective → still fine for UART, SPI ≤1MHz, I2C ≤400kHz

---

## Phased Implementation

### PHASE A — Passive Monitoring + PC Export

**Goal:** Capture all bus traffic, export to PC, correlate with ESP32 packet log.

**Difficulty:** Medium  
**Time estimate:** 3-4 days  
**Depends on:** Nothing — can start immediately

#### A.1 — PC Companion: LA Capture Module

Create `tools/pc_companion/logic_analyzer.py`

```
Responsibilities:
- Connect to Saleae Logic software via API (or PulseView CLI)
- Configure sample rate and channel mapping
- Start/stop capture synchronized with ESP32 campaign start/stop
- Export capture to VCD (Value Change Dump) format
- Export capture to CSV with timestamps
- Parse VCD into packet-level events using protocol decoders
```

**Software Options:**
| Software | API | Protocol Decoders | Export |
|---|---|---|---|
| Saleae Logic 2 | WebSocket API (Python) | Built-in UART/SPI/I2C | VCD, CSV, binary |
| PulseView (sigrok) | CLI / libsigrok | 100+ protocol decoders | VCD, CSV |
| sigrok-cli | Command line | Same as PulseView | VCD, CSV |

**Recommended:** PulseView/sigrok-cli (open source, no Saleae license needed)

```bash
# Example sigrok-cli capture
sigrok-cli -d fx2lafw --config samplerate=1M \
  --channels 0=TX,1=RX,7=HB \
  --triggers 7=r \
  --samples 1000000 \
  --output /tmp/capture.vcd \
  --output-format vcd

# With protocol decoder
sigrok-cli -d fx2lafw --config samplerate=1M \
  --channels 0=TX,1=RX \
  -P uart:tx=0:rx=1 \
  --output /tmp/capture.vcd
```

#### A.2 — Synchronized Capture Start/Stop

```
Workflow:
1. ESP32 receives START command
2. PC companion sends START to ESP32 via serial
3. PC companion simultaneously starts LA capture
4. Campaign runs for N seconds
5. ESP32 sends STOP or campaign ends
6. PC companion stops LA capture
7. Both logs are saved with matching timestamps
8. PC companion correlates ESP32 packet # with LA timestamp
```

#### A.3 — VCD-to-Packet Parser

Create `tools/pc_companion/vcd_parser.py`

```
Responsibilities:
- Parse VCD file into timestamped signal transitions
- Apply UART decoder: extract bytes from CH0 (TX) and CH1 (RX)
- Apply SPI decoder: extract transactions from CH2-CH5
- Apply I2C decoder: extract messages from CH6
- Create unified packet list:
  [
    { timestamp_us: 12345, protocol: "UART", direction: "TX",
      bytes: [0xA5, 0x01, 0x08, ...], source: "LA" },
    { timestamp_us: 12400, protocol: "UART", direction: "RX",
      bytes: [0x5A, 0x01, 0x00], source: "LA" },
    ...
  ]
```

#### A.4 — Correlation Engine

Create `tools/pc_companion/correlator.py`

```
Responsibilities:
- Match ESP32 packet log (from SERIAL OUTPUT) with LA capture
- Compare: ESP32 says it sent 0xA5 0x01 0x08 vs LA confirms same bytes
- Flag discrepancies:
  - ESP32 sent but LA shows different bytes (signal integrity issue)
  - LA shows bytes ESP32 didn't log (ESP32 missed something)
  - LA shows DUT response ESP32 didn't receive (SoftwareSerial issue)
- Generate correlation report
```

#### A.5 — Deliverables

```
tools/pc_companion/
  logic_analyzer.py      # LA connection and control
  vcd_parser.py          # VCD to packet events
  correlator.py          # ESP32 log ↔ LA capture matching
  
docs/
  la-integration.md      # How to connect and use LA
  
reports/ (example)
  2026-08-20_TEST_001/
    report.json          # ESP32 report
    report.txt           # Human readable
    packet.bin           # Failing packet
    la_capture.vcd       # ← NEW: Logic analyzer raw capture
    la_packets.json      # ← NEW: Decoded packets from LA
    correlation.json     # ← NEW: ESP32 vs LA comparison
```

---

### PHASE B — Trigger-Based Failure Capture

**Goal:** LA automatically captures the window around a failure event.

**Difficulty:** Medium  
**Time estimate:** 2-3 days  
**Depends on:** Phase A

#### B.1 — Failure Trigger Signal

ESP32 already has a FAILURE state. Use an unused GPIO to signal the LA:

```
ESP32 GPIO (spare) ──→ LA CH (spare)
                       │
                       └── Goes HIGH when failure detected
                           LA triggers on rising edge
                           Captures pre-trigger buffer (context before failure)
```

This gives you the **exact bus state** leading up to the crash.

#### B.2 — Pre/Post Trigger Buffer

```
LA Capture Buffer:
  ◄─── Pre-trigger (500ms) ───► ◄─── Post-trigger (200ms) ───►
  
  [valid] [valid] [OVERLENGTH] [no response] [HEARTBEAT DIED] [silence]
                                    ↑                            ↑
                              trigger point               capture ends
```

#### B.3 — ESP32→LA Timestamp Marker

ESP32 sends a serial command to PC companion at the moment of failure:

```
ESP32 serial output:
  FAILURE seq=183 mutation=OVERLENGTH time=17832ms

PC companion:
  1. Receives failure notification
  2. Stops LA capture
  3. Exports the last 700ms window
  4. Correlates ESP32's packet #183 with LA timestamp
```

#### B.4 — Failure Forensics Report

```
FAILURE FORENSICS — TEST_014
═══════════════════════════════════════

ESP32 Log:
  Packet #183: OVERLENGTH mutation, 45 bytes
  Heartbeat: last seen at T-372ms
  
LA Capture (decoded):
  T-500ms:  UART TX → A5 01 08 ... (VALID, 8 bytes)
  T-490ms:  UART RX → 5A 01 00     (ACK)
  T-400ms:  UART TX → A5 03 00 ... (EMPTY)
  T-390ms:  UART RX → 5A 03 00     (ACK)
  T-300ms:  UART TX → A5 05 2D ... (OVERLENGTH, 45 bytes declared)
  T-298ms:  UART RX → (nothing)    ← no response
  T-100ms:  Heartbeat → HIGH→LOW
  T-372ms:  Heartbeat → stuck LOW  ← DUT died
  
  Bus state at T+0: TX=idle, RX=idle, HB=LOW
  
Signal Integrity:
  UART TX rise time: 120ns (OK)
  UART RX at DUT: marginal at 2.8V (threshold: 2.5V)
  No bus contention detected
  
Correlation:
  ESP32 packet #183 bytes match LA capture ✓
  ESP32 reported 0 ACKs — LA shows 2 ACKs before failure ✓
  ESP32 missed ACKs due to SoftwareSerial timing ✓
```

---

### PHASE C — Real-Time Response Verification

**Goal:** LA feeds verified ACK/NACK data back to ESP32 in real-time.

**Difficulty:** Hard  
**Time estimate:** 5-7 days  
**Depends on:** Phase A, hardware modification

#### C.1 — Problem Being Solved

Currently the ESP32 SoftwareSerial receives 0 ACKs from the Nano.
The LA confirms ACKs ARE on the wire — SoftwareSerial just misses them.

#### C.2 — Architecture Options

**Option 1: LA → PC → ESP32 (via serial)**
```
LA captures DUT response
  → PC decodes UART response
  → PC sends ACK/NACK status to ESP32 via serial
  → ESP32 records hardware-verified response
  
Pros: No ESP32 hardware changes
Cons: PC must be connected, adds latency (~10ms)
```

**Option 2: LA → ESP32 (via SPI/I2C)**
```
LA captures DUT response
  → LA sends decoded byte to ESP32 via SPI
  → ESP32 records hardware-verified response
  
Pros: No PC needed, fast
Cons: Requires LA with SPI slave firmware (custom)
```

**Option 3: Duplicate UART RX (recommended for v4.1)**
```
UART RX wire splits:
  → ESP32 GPIO16 (SoftwareSerial, may miss bytes)
  → LA CH1 (hardware capture, never misses)
  
ESP32 gets best-effort from SoftwareSerial
LA gets perfect capture for post-analysis
PC correlates both after test completes
```

#### C.3 — Recommendation

Start with **Option 3** (passive duplicate) — it requires zero firmware changes
and gives you perfect RX data for post-test analysis. Option 1 can be added
later for real-time feedback.

---

### PHASE D — Signal Integrity Monitoring

**Goal:** Continuous bus health checks during campaigns.

**Difficulty:** Easy (data collection) / Medium (analysis)  
**Time estimate:** 2-3 days  
**Depends on:** Phase A

#### D.1 — Metrics to Monitor

| Metric | Threshold | Failure Mode |
|---|---|---|
| UART TX rise time | < 200ns | Slow edges → bit errors at high baud |
| UART RX voltage (at DUT) | > 2.0V (3.3V logic) | Marginal voltage → intermittent NACKs |
| SPI clock frequency | Within ±5% of expected | Clock drift → sampling errors |
| I2C SDA low time | > setup_time spec | Bus timing violations |
| Heartbeat frequency | 50-200 Hz | Too fast = watchdog, too slow = dying |
| Bus contention | None detected | Two drivers fighting → undefined state |
| Ground noise | < 200mV | High current switching → data corruption |

#### D.2 — Periodic Health Snapshot

During a campaign, capture a 10ms window every 5 seconds:

```
T=0s:   Health snapshot — UART edges: 180ns rise, clean
T=5s:   Health snapshot — UART edges: 175ns rise, clean
T=10s:  Health snapshot — UART edges: 210ns rise, WARNING
T=15s:  Health snapshot — FAILURE — last snapshot saved
```

#### D.3 — Correlation with Failures

If a failure occurs, the health snapshot history shows whether
signal degradation preceded the crash:

```
Timeline:
  T=0:   Edge timing 180ns (normal)
  T=5:   Edge timing 175ns (normal)
  T=10:  Edge timing 210ns (marginal)
  T=15:  Edge timing 250ns (degraded)
  T=18:  FAILURE — Heartbeat timeout
  
Conclusion: Signal degradation preceded failure.
Suspected cause: Bus capacitance increasing (loose connection?)
```

---

## File Structure After Integration

```
AutoFuzzer/
├── firmware/
│   └── esp32/src/
│       └── (no changes for Phase A/B/D)
│           (Phase C: minor serial command additions)
│
├── tools/
│   └── pc_companion/
│       ├── __init__.py
│       ├── cli.py
│       ├── connection.py
│       ├── reporter.py
│       ├── database.py
│       ├── builder.py
│       ├── fixer.py
│       ├── regression.py
│       ├── logic_analyzer.py      ← NEW: LA control
│       ├── vcd_parser.py          ← NEW: VCD decode
│       ├── correlator.py          ← NEW: ESP32↔LA matching
│       ├── signal_integrity.py    ← NEW: Bus health checks
│       └── requirements.txt
│
├── docs/
│   ├── la-integration.md          ← NEW: LA setup guide
│   ├── logic-analyzer-plan.md     ← NEW: This document
│   └── ...
│
├── reports/
│   └── YYYY-MM-DD_TEST_NNN/
│       ├── report.json
│       ├── report.txt
│       ├── packet.bin
│       ├── la_capture.vcd         ← NEW: Raw LA data
│       ├── la_packets.json        ← NEW: Decoded packets
│       ├── correlation.json       ← NEW: Cross-reference
│       └── signal_health.json     ← NEW: Bus integrity
│
└── tests/
    └── test_logic_analyzer.py     ← NEW: LA unit tests
```

---

## New Serial Commands

```
LA START           — Tell PC companion to begin LA capture
LA STOP            — Tell PC companion to stop LA capture
LA TRIGGER         — Set failure trigger (rising edge on spare GPIO)
LA STATUS          — Show LA connection status
```

---

## Dependencies

### Software

| Tool | Purpose | Install |
|---|---|---|
| sigrok-cli | LA control and capture | `sudo apt install sigrok-cli` |
| PulseView (optional) | GUI for LA visualization | `sudo apt install pulseview` |
| Saleae Logic 2 (optional) | If using Saleae hardware | Download from saleae.com |

### Python Packages

```
# Add to tools/pc_companion/requirements.txt
numpy>=1.24          # Signal analysis
scipy>=1.10          # Edge timing, rise time measurement
```

---

## Testing Plan

### Phase A Tests
- [ ] LA connects and captures UART traffic
- [ ] VCD export works with correct timestamps
- [ ] UART decoder extracts correct bytes
- [ ] Correlator matches ESP32 packet #183 with LA timestamp
- [ ] Capture starts/stops synchronized with campaign

### Phase B Tests
- [ ] Failure trigger fires on rising edge
- [ ] Pre-trigger buffer captures 500ms before failure
- [ ] Post-trigger buffer captures 200ms after failure
- [ ] Forensics report includes decoded bus traffic

### Phase C Tests
- [ ] LA-verified ACK/NACK count matches wire reality
- [ ] ESP32 receives ACK data (via PC or SPI)
- [ ] Latency < 20ms for real-time feedback

### Phase D Tests
- [ ] Rise time measurement accurate to ±10ns
- [ ] Health snapshot captures correct window
- [ ] Signal degradation detected before failure
- [ ] No performance impact on fuzzing speed

---

## Risk Assessment

| Risk | Impact | Mitigation |
|---|---|---|
| LA not detected by sigrok | Can't capture | Verify USB device ID, try different driver |
| Sample rate too low for SPI | Missed clock edges | Use 24MHz, reduce active channels |
| VCD file too large | Disk/memory issues | Limit capture duration, compress |
| LA probes affect signal | False failures | Use high-Z probes, verify with known-good capture |
| PC companion latency | Miss real-time events | Phase A/B use post-analysis, not real-time |
| PulseView/sigrok not installed | Can't run | Document install steps, add to requirements |

---

## Priority Order

1. **Phase A** (Passive Monitoring) — Start here, biggest value
2. **Phase D** (Signal Integrity) — Easy to add alongside Phase A
3. **Phase B** (Trigger Capture) — Critical for failure forensics
4. **Phase C** (Real-Time Feedback) — Nice to have, complex

Estimated total: **2-3 weeks** for Phases A + B + D
Phase C is optional for v4.1.

---

## Success Criteria

After full integration:

| Before (v4.0) | After (v4.1) |
|---|---|
| "Heartbeat stopped" | "Heartbeat stopped 372ms after OVERLENGTH, LA confirms no DUT response" |
| "0 ACKs received" | "LA confirms 14 ACKs on wire, ESP32 SoftwareSerial missed them" |
| "Random failure" | "LA shows SDA stuck LOW — I2C bus contention detected" |
| "DUT crashed" | "LA captured last 500ms — DUT sent 0xFF 0xFF (watchdog reset)" |
| "Signal looks fine" | "Rise time 250ns — exceeding 200ns threshold" |
| No wire verification | "ESP32 TX bytes match LA capture ✓" |
