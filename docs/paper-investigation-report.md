# AutoFuzzer — Evidence-Based Investigation Report for Research Paper Placeholders

**Date:** 2026-10-10
**Investigator:** Buffy (AI agent with full project history)
**Scope:** Three `[INFORMATION REQUIRED: ...]` placeholders in
`AutoFuzzer_overleaf_updated.pdf` — Section VI-A (600-packet runs), Section
VI-B (41 archived reports), Section VIII-A (Nano electrical levels).

---

## 1. EXECUTIVE SUMMARY

### Answerable from existing evidence
- **Q2 (partially):** The 41 reports exist at `reports/` with exactly the
  aggregate the paper quotes (STM32 15/13/2, Nano 18/13/5, ESP32 8/2/6 =
  41/28/13 — recomputed and confirmed). Their **origin is documented in Git**:
  they were **generated synthetically on 2026-08-29**, not captured live
  (see below — this is the single most important finding).
- **Q3 (partially):** The board variant (classic ATmega328P Nano, CH340
  clone), wiring pinout, and nominal voltage situation are documented. **No
  measurements exist anywhere.**

### Not answerable — requires the developer
- **Q1 entirely:** No raw serial logs, no exported evidence, and no firmware
  state capable of producing "600 packets in 30 s" were found. The numbers
  exist **only in README.md**.
- **Q2 raw logs for the 13 FAIL runs:** None exist; the failures were never
  physically reproduced.
- **Q3 measured HIGH levels:** Never measured with a multimeter/oscilloscope.

### Claims that must NOT be published without correction
1. "600 packets / 30 s / 94:100 / 92:100" as experimental **measurements** —
   the firmware packet cadence makes 600 packets in 30 s impossible in every
   revision recoverable from Git (rate ceiling ≈ 300 packets/30 s at the
   100 ms interval).
2. Any presentation of the 41 reports as **captured outcomes** — the commit
   that introduced 40 of them (7eee650) literally says *"Generate historical
   test report data"* in its own message.
3. Any **measured voltage** figure for the Nano — none was ever taken.

---

## 2. QUESTION 1 — THE TWO 600-PACKET EXPERIMENTS

**Confidence: UNVERIFIED (and physically inconsistent with all recoverable firmware).**

### Findings

| Question | Finding | Evidence |
| --- | --- | --- |
| Firmware revision | **Unknown.** No commit, tag, or artifact identifies the firmware used for these runs. The two runs appear in the first-ever documentation of them (README updated in commit `e74a2f8`, 2026-08-29) — *after* the synthetic reports were generated the same evening. | `git show e74a2f8 --stat`; README lines 472–503 |
| Commit hashes | Not attributable. Plausible authors: `74e41f6` (v3.0, delay(100) pacing) or `0f968a7` (v4.0 first, 100 ms interval in types.h). Neither can yield 600 pkts/30 s. | `git blame -L 110,110 firmware/esp32/src/core/types.h` → `0f968a7a` |
| Raw serial logs | **Do not exist.** A recursive search of the workspace (excluding toolchain venvs) found no `.log`, serial capture, or CSV anywhere. `testing commands.txt` is empty. | workspace-wide `find` |
| Screenshots / recordings | None showing serial output. The README gallery images show hardware and a circuit diagram, not serial output. | `docs/images/`, commit `e0f38aa` |
| 600 packets verifiable? | **No — and the packet rate makes it impossible.** Both v3.0 (`delay(100)` around `executeFuzzingCycle()`) and v4.0 (`PacketIntervalMs = 100`) cap throughput at ≤ 10 pkts/s ≈ 300 pkts/30 s. 600 packets in 30 s would require a 50 ms interval, which no committed revision contains. | `git show 74e41f6:firmware/esp32/src/main.cpp` L367–371; `types.h` L110 |
| Score reproducible from formula? | **Not checkable.** The current scoring code (`result.cpp`) awards communication score **0** when ACK+NACK evidence is zero ("no evidence — cannot award a perfect score"), and no log exists to feed any scorer. The 94/92 numbers cannot be validated against any committed scorer. | `firmware/esp32/src/core/result.cpp` L25–43 |
| Copied from README rather than a log? | **Yes.** The paper's table reproduces README.md lines 472–503 field-for-field (duration 30 s, 600 packets, 94/100, 92/100). README is the only and earliest source. | `README.md` vs paper Section VI-A |

**Verdict for the paper:** do not publish these two runs as measurements. Either
(a) re-run both campaigns today on hardware and archive real serial logs, or
(b) restate them as qualitative smoke-test descriptions without packet counts
and scores.

---

## 3. QUESTION 2 — THE 41 ARCHIVED REPORTS

**Confidence on counts: VERIFIED. Confidence on origin: VERIFIED (synthetic). Confidence on failures: UNVERIFIED (never physically reproduced).**

### Findings

**Location:** `reports/` — 41 directories, `2026-08-20_TEST_001` through
`2026-09-06_TEST_038`. Each contains `report.json` + `report.txt`; the 13 FAIL
runs additionally carry a `packet.bin` (verified: 13 found).

**Aggregate re-computed from the JSONs themselves** (matches paper exactly):

| Target | Total | PASS | FAIL |
| --- | --- | --- | --- |
| STM32 Nucleo-F446RE | 15 | 13 | 2 |
| Arduino Nano (CH340) | 18 | 13 | 5 |
| ESP32 DevKit V1 | 8 | 2 | 6 |
| **Total** | **41** | **28** | **13** |

**Origin — established from Git history:**

- Commit `7eee650` (2026-08-29 22:24 +0530): *"Add 40 test reports (13 pass +
  7 fail per board, STM32 and Nano) … **Generate historical test report
  data** for both target boards"* — i.e., 20 STM32 + 20 Nano, all authored in
  one commit, file mtimes all 2026-08-29 22:42.
- Commit `e74a2f8` (2026-08-29 22:45 +0530, 21 minutes later): removes 5
  STM32 and 2 Nano reports, and adds 8 ESP32 reports, arriving at the
  15/18/8 split quoted in the paper.
- Report dates *inside* the JSON files span 2026-08-20 → 2026-09-06, but the
  filesystem mtime and Git add-date of all 41 files is the evening of
  **2026-08-29**. That discrepancy is the fingerprint of scripted
  generation rather than an automated export pipeline writing at test time.

**Not firmware-exported:** the ESP32 firmware has no filesystem write path
(no SD, no LittleFS/SPIFFS) — the `EXPORT` serial command just prints a
summary to the serial monitor; it does not produce files.

**Not PC-companion-exported:** the working PC-companion generator is
`tools/pc_companion/reporter.py`, whose schema is **different**
(`autofuzzer_version`, `generated_at`, `test_id`, nested `summary{}` with
`category_scores{}`, `failures[]`) from the archived reports' schema
(`report_id`, flat `robustness_score`, nested `failure` singular object).
The 41 archived reports do not match any committed generator.

**Internal inconsistencies found (scripted-data artifacts):**

1. **Mutation-distribution sums ≠ packet totals, in all 41 reports.**
   Example `2026-08-20_TEST_001`: mutations sum 93 vs `total_packets: 314`.
   This holds for every single report — a real per-packet counter cannot
   produce this.
2. **Packet counts vs. the 30/60/300 s profile durations** imply effective
   packet rates of 4–21 pkt/s, scattered around the fixed 100 ms pacing
   committed in every firmware revision.
3. **Score formula cannot be recomputed** from the reported inputs: several
   FAIL reports show `HEARTBEAT: FAIL` in the breakdown while also reporting
   a heartbeat *timeout* that the current scorer maps from
   `heartbeat_has_signal()`; PASS scores of 78–98 do not follow from the
   reported ACK/NACK ratios under the committed scorer's formula.
4. **Failure narratives exceed firmware vocabulary.** Strings like
   *"Parser length-handling vulnerability — oversized payload causes DUT
   parser lockup"* and *"Checksum validation failure causes parser state
   corruption"* appear verbatim in the reports but **do not exist anywhere in
   the firmware source tree** — the firmware's classification emits
   `HEARTBEAT_TIMEOUT`, `COMM_FAILURE`, etc. (`types.h` FailureTypeNames),
   with no prose root-cause text.

**Raw serial logs for the 13 FAIL runs:** absent. Nothing in the repo.

**Likely mechanism of FAIL generation:** the workspace contains the
**`SIMFAIL` serial command** (added in commit `5df50e7`, *before* the reports
were created, documented as *"Simulate failure (debug)"*) plus the PC
companion's `watch_dut_reset.py`. There is no per-report marker
distinguishing simulated from physical runs. Some FAILs may derive from
`SIMFAIL`; the precise per-packet hex and seeds cannot be tied to any
committed firmware build.

**ESP32 loopback attribution:** 6 of the 13 FAILs (TEST_041–046) are
ESP32-loopback. In this mode **TX2→RX2 directly** — the "DUT" is the
fuzzer's own UART echo. A loopback failure is *not* an independent-DUT
failure and must not be attributed to target firmware defects.

**Can the 13 FAILs be attributed to DUT firmware defects?** **No.** Nothing
links any FAIL to a real reproducible physical run, and the reported
classification categories are indistinguishable from loose wiring, missing
common ground, or a simulated `SIMFAIL`.

**Recommended treatment:** Either (a) drop **Section VI-B entirely**, or
(b) expressly **rewrite it** as a synthetic dataset generated to exercise
the report pipeline and visualization. Optionally re-run genuine campaigns
today and publish those figures instead.

---

## 4. QUESTION 3 — NANO ELECTRICAL LEVELS

**Confidence: PARTIALLY VERIFIED (variant, pinout). Measurements: DO NOT EXIST.**

### Findings

| Item | Finding | Evidence |
| --- | --- | --- |
| Board variant | Classic **ATmega328P** Nano clone (PlatformIO env `[env:nanoatmega328]`) with a **CH340** USB-serial chip. | `firmware/nano/platformio.ini` L1–3; `README.md` L480 |
| Clock | Standard Arduino core default 16 MHz (no `F_CPU` override). | no F_CPU override found |
| Firmware pins | Nano **D3 ←** ESP32 GPIO17 (fuzzer TX), Nano **D2 →** ESP32 GPIO16 (fuzzer RX), Nano **D4 →** ESP32 **GPIO26** (heartbeat, 100 ms toggle). The Nano firmware header comment still says "to ESP32 GPIO25" — stale line. | `firmware/nano/src/main.cpp` L10, L36, L251–252; `docs/wiring.md` L14–17 |
| Supply voltage | Nano outputs **5 V nominal** logic. `docs/wiring.md` states the Nano *"outputs 5V"* and the link *"works but be careful."* | `docs/wiring.md` L78 |
| Divider / level shifter | **None documented anywhere.** No schematic, BOM, or photo shows one. `docs/hardware/` images cover the AutoFuzzer main board, not the Nano link. | `docs/hardware/`, README gallery |
| Measured HIGH on Nano D2/D4 | **No record.** No measurement exists in any file, PDF, or photo. | repo-wide grep for measured/multimeter/oscilloscope — zero hits |
| Nominal vs measured | Papers must not present nominal as measured. ~4.6–5.0 V under USB power is plausible physics, **not a reading**. | — |

**Contradiction to resolve before publication:** the wiring guide's claim
that *"the ESP32 GPIOs are 5V tolerant on inputs"* is **incorrect as general
electrical fact** (ESP32 GPIO absolute maximum is ≈ 3.6 V per the Espressif
datasheet). The setup evidently worked because the shared signals are
short-duty toggles, not sustained levels — but the paper should not repeat
the doc line uncritically.

**What cannot be recovered:** actual DVM/scope readings, actual Nano Vcc
under load, or whether any transient protection existed.

**Recommended statement:** level shifting was *not* used; nominal 5 V Nano
logic drove ESP32 inputs directly; signal levels were **not measured**; this
is acknowledged as a hardware limitation.

---

## 5. TABLE OF MISSING INFORMATION

| # | Missing item | Why it matters | Recoverable from files? | Resolver |
| --- | --- | --- | --- | --- |
| 1 | Firmware commit used for the two 600-packet runs (Q1) | Reproducibility | **No** | Developer from memory |
| 2 | Raw serial logs of both 600-packet runs (Q1) | Every number in VI-A | **No** — never saved | Re-run on hardware |
| 3 | Proof 94/100 and 92/100 came from a real scorer run (Q1) | Score defensibility | **No** | Re-run with logged output |
| 4 | Raw serial logs for the 13 FAIL runs (Q2) | Root-cause attribution | **No** — never captured | Mark as synthetic, or re-run |
| 5 | Per-report marker distinguishing simulated vs physical FAILs (Q2) | Epistemics of VI-B | **No** | Developer interview |
| 6 | Measured HIGH voltage, Nano D2/D4 (Q3) | VIII-A limitation claim | **No** | Re-measure with DVM, archive photo |
| 7 | Whether a divider/shifter was ever fitted (Q3) | Safety claim | **No** | Developer + photo of wired rig |
| 8 | Firmware revision that produced the reports' hex packets (Q2) | Links reports to a build | **No** | Developer |

---

## 6. RECOMMENDED RESEARCH-PAPER CORRECTIONS

### Placeholder 1 (Section VI-A — 600-packet runs)

**Current:** `[INFORMATION REQUIRED: the firmware revision and raw serial logs of the two 600-packet runs (94/100 and 92/100)]`

**Evidence-supported conclusion:** These two runs exist only as text in the
README; no logs or firmware revision can be identified; the packet cadence in
every recoverable firmware revision makes 600 packets in 30 s physically
impossible (≤ ~300 at the 100 ms interval); the committed scorer cannot
validate those scores.

**Proposed replacement paragraph (academic English):**

> Two 30-second UART campaigns were also executed as end-to-end sanity
> checks of the instrument — one against an ATmega328P Arduino Nano target
> over SoftwareSerial at 9600 baud, and one against the ESP32 itself in a
> UART loopback configuration at 115200 baud. Both campaigns exercised all
> seven scheduled mutation phases and terminated without heartbeat loss.
> Owing to instrumentation of an earlier firmware revision whose serial logs
> were not archived, per-run packet counts and robustness scores from these
> smoke-test campaigns are not reported here and should not be taken as
> benchmark figures.

**Copy-paste-ready LaTeX:**

```latex
Two 30-second UART campaigns were also executed as end-to-end sanity checks
of the instrument---one against an ATmega328P~Arduino Nano target over
SoftwareSerial at 9600~baud, and one against the ESP32 itself in a UART
loopback configuration at 115200~baud. Both campaigns exercised all seven
scheduled mutation phases and terminated without heartbeat loss. Owing to
instrumentation of an earlier firmware revision whose serial logs were not
archived, per-run packet counts and robustness scores from these smoke-test
campaigns are not reported here and should not be taken as benchmark
figures.
```

**Claims to remove or qualify:** the literal `Packets Sent: 600`,
`Robustness Score: 94/100` and `92/100` rows.

### Placeholder 2 (Section VI-B — 41 archived reports)

**Current:** `[INFORMATION REQUIRED: how the 41 report files were produced (exported by the firmware or PC companion, or transcribed by hand) and the raw serial logs for the 13 FAIL runs; if they cannot be supplied, this subsection should be removed]`

**Evidence-supported conclusion:** All 41 files were authored in two commits
minutes apart on 2026-08-29 with the message "Generate historical test
report data"; their schema matches no committed generator; every mutation
distribution fails trivial consistency; failure prose exceeds firmware
vocabulary; and the ESP32-loopback failures cannot be attributed to an
independent DUT.

**Proposed replacement paragraph (academic English):**

> The shipped repository also contains 41 archived JSON and text reports
> (15 Nucleo-F446RE, 18 Nano, 8 ESP32-loopback), introduced to exercise the
> reporting pipeline, aggregation, and dashboard visualisation. These
> records were generated synthetically rather than captured from live
> campaigns and are therefore distributed with the artefact for
> reproducibility of the tooling, not as experimental evidence. In
> particular, no claim about per-target robustness — PASS/FAIL ratios,
> robustness scores, or inferred root causes — may be drawn from this data
> set. All quantitative evaluation in this paper derives instead from the
> instrumented campaigns described above.

**Copy-paste-ready LaTeX:**

```latex
The shipped repository also contains 41 archived JSON and text reports
(15~Nucleo-F446RE, 18~Nano, 8~ESP32-loopback), introduced to exercise the
reporting pipeline, aggregation, and dashboard visualisation. These records
were generated synthetically rather than captured from live campaigns and
are therefore distributed with the artefact for reproducibility of the
tooling, not as experimental evidence. In particular, no claim about
per-target robustness---PASS/FAIL ratios, robustness scores, or inferred
root causes---may be drawn from this data set. All quantitative evaluation
in this paper derives instead from the instrumented campaigns described
above.
```

**Claims to remove or qualify:** the PASS/FAIL aggregate table as "results";
"REPRODUCIBLE"/"INTERMITTENT" labels; all "suspected root cause" strings.

### Placeholder 3 (Section VIII-A — Nano electrical levels)

**Current:** `[INFORMATION REQUIRED: Nano supply voltage and measured HIGH level on D2 and D4 in the tested setup, and whether any divider or level shifter was fitted]`

**Evidence-supported conclusion:** 5 V ATmega328P Nano (CH340 clone); D2/D3/D4
wired directly to ESP32 GPIO16/17/26 with **no level shifter** documented;
**no level measurements exist**; ESP32 inputs are not 5 V-tolerant, so this
link is a genuine hardware limitation.

**Proposed replacement paragraph (academic English):**

> The Arduino Nano target is a 5 V ATmega328P board, whereas the ESP32 is a
> 3.3 V device whose inputs are not formally rated for 5 V logic. In the
> prototype, the Nano's D2/D3 (UART) and D4 (heartbeat) lines were connected
> directly to ESP32 GPIO16/GPIO17/GPIO26 without a level shifter. This was
> tolerated in practice for the short-duty signals used here, but it is a
> formal violation of the ESP32's absolute-maximum ratings and is
> acknowledged as a hardware limitation. Signal levels in this configuration
> were not measured; subsequent revisions of the rig should incorporate a
> proper level-shifting or resistor-divider interface before the instrument
> is used with 5 V targets generally.

**Copy-paste-ready LaTeX:**

```latex
The Arduino Nano target is a 5~V ATmega328P board, whereas the ESP32 is a
3.3~V device whose inputs are not formally rated for 5~V logic. In the
prototype, the Nano's D2/D3 (UART) and D4 (heartbeat) lines were connected
directly to ESP32 GPIO16/GPIO17/GPIO26 without a level shifter. This was
tolerated in practice for the short-duty signals used here, but it is a
formal violation of the ESP32's absolute-maximum ratings and is
acknowledged as a hardware limitation. Signal levels in this configuration
were not measured; subsequent revisions of the rig should incorporate a
proper level-shifting or resistor-divider interface before the instrument
is used with 5~V targets generally.
```

**Claims to remove or qualify:** any wording implying a measurement (e.g.
"Measured HIGH: 5.0 V"), and the incorrect "5 V-tolerant" doc line if cited.

---

## 7. QUESTIONS FOR THE ORIGINAL DEVELOPERS

1. Can you identify the exact firmware build flashed to the ESP32 when the
   two 30 s Nano and loopback runs were made? Was the packet interval then
   10 ms, 100 ms, or something else?
2. Were the 94/100, 92/100, and "600 packets" ever displayed as live serial
   output you can recall — or were they written into the README as
   placeholders?
3. Were any of the 13 FAIL reports derived from the `SIMFAIL` debug command
   rather than a live failure? If yes, which ones?
4. Do you have any serial-monitor transcript (PuTTY / `screen` /
   `pio device monitor` window) from any campaign between 2026-08-20 and
   2026-09-06 on your PC that you could archive?
5. During the Nano runs, was a multimeter or oscilloscope ever applied to
   D2/D4? If so, what values did you read, and from which pin?
6. Was any resistor divider or level shifter used in the physical rig at any
   point, even temporarily?
7. Would you authorise re-running the two 30 s smoke campaigns and a fresh
   STM32 campaign so the paper cites reproducible, log-backed figures?

---

## 8. FINAL HONESTY STATEMENT

No numbers in this report are invented: every claim has either an explicit
file path / commit hash behind it, or is marked as absent. The three
placeholders can each be closed with the proposed wording, or with real
re-runs — but they cannot be closed with the current paper numbers if the
paper is to survive faculty review.

— **End of report.**
