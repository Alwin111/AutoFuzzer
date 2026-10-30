# Paper Audit — AutoFuzzer (submitted to ICITIIT'27)

**Reference file:** [research_validation/paper/submitted_paper.pdf](paper/submitted_paper.pdf)
**Extraction notes:** `pdftotext -layout` (Unicode-safe, TeX-generated PDF, 6 pages).
**Audit date:** 2026-10-30

---

## A. Paper summary (from submitted PDF, verbatim-paraphrased)

The paper presents **AutoFuzzer**, a low-cost standalone ESP32-based fuzzing platform for serial-communication parsers in embedded firmware.

- **Controller/DUT split:** an ESP32 DevKit V1 acts as fuzzing controller and liveness oracle. It generates framed packets over UART (GPIO17→RX, GPIO16←TX), monitors a DUT heartbeat pin (GPIO25), and exposes OLED/button/SIMFAIL control.
- **Protocol:** SYNC(0xA5), CMD(0x01), LEN, SEQ_LO, SEQ_HI, payload (≤32 B), XOR checksum `c = CMD ⊕ LEN ⊕ SEQ_L ⊕ SEQ_H ⊕ payload[0..n-1]`. DUT response: 0x5A, SEQ_LO, SEQ_HI, status (0x00 ACK / 0x02 NACK-overlen / 0x03 NACK-checksum; no response = parser timeout).
- **Mutation classes (Table I):** VALID, EMPTY, MAX_LEN, OVERLENGTH(45 B), BAD CHECKSUM, TRUNCATED(12→6 B), RANDOM — mapped to CWE-20/755, CWE-120, CWE-354, CWE-835.
- **Campaign scheduling (Table I):** 7 primary classes; Quick/Standard/Deep/Custom. Firmware (4.0) implements 10 mutation classes (adds BAD HEADER, BAD LEN, SEQ JUMP).
- **Scoring (Eq. 3):** S = Σ wk·sk over six categories (communication 25, boundary 20, malformed 20, recovery 15, heartbeat 10, random 10). Verdict rules defined.
- **Results (Table IV):** 41 archived reports (28 PASS / 13 FAIL — unverifiable test data); one captured UART run: 258 pkts, 0 ACK, 14 NACK, 0 failures, 75/100.
- **Verification claims (Section IV-C):** 45 host-side Python tests pass (consistency, not firmware correctness); README-reported campaigns "preliminary and excluded."
- **Limitations (Section VIII):** UDP-only, beat-pulse heartbeat oracle only, no seeded-fault sensitivity, no baselines/repetitions, no quantitative performance, review guideline violations.
- **Future work (Section IX):** seeded-fault validation with four fault variants (length-check missing, checksum mishandling, truncated-frame parser stall, input-dependent); SPI/I2C fuzzing; adaptive mutation; more targets.

---

## B. Inconsistency map (submitted PDF ↔ repository source)

| # | Paper claim | Repository reality | Verdict |
|---|-------------|--------------------|---------|
| B1 | "7 primary mutation classes" (Table I) | Firmware implements 10 classes: VALID, EMPTY, MAX_LEN, OVERLENGTH, BAD_CRC, TRUNCATED, RANDOM, MALFORMED_HEADER, INVALID_LENGTH, SEQ_ANOMALY (types.h) | **Clarify** — Table I is the assessment subset; state it explicitly |
| B2 | Verdict definitions | Code implements PASS/FAIL/INCONCLUSIVE; the paper only sketches rules; DEMO mode (default) auto-passes score to 100 | **Fix** — make verdict rules explicit; remove auto-pass |
| B3 | "7 primary classes" scheduling | Firmware supports 10 classes (MUT_BAD_CRC etc.) | **Clarify** |
| B4 | No quantitative performance | Paper admits "no quantitative performance measurements" — consistent | ✅ consistent |
| B5 | "41 archived reports (28 PASS, 13 FAIL)" | Verified: `reports/` count = 41 reports | ✅ consistent |
| B6 | "target and raw log of that run are unavailable" | Source, sdk, and logs are in the repo / /tmp | **Clarify** — state availability |
| B7 | Eq. (1) checksum formula | Code checksum = buffer[1]^buffer[2]^buffer[3]^buffer[4] ^ payload bytes (same) | ✅ consistent |
| B8 | "DUT wiring in Table III" | Wiring table present in README, GPIO mapping matches | ✅ consistent |
| B9 | "Heartbeat pin (GPIO25)" | Code uses GPIO26 for heartbeat, OLED uses 21/22; README advertises 25 | **Fix** — align paper with code: heartbeat GPIO26 |
| B10 | "No baseline or repeated trials exist" | ✓ Consistent |
| B11 | "SPI and I2C stubs" | SPI/I2C fuzzers implemented but output-passive; exact stub vs full is unverified | **Clarify** |
| B12 | " scorer formula Eq. (3) vs code" | Code uses 6 categories; paper text matches | ✅ consistent |
| B13 | "demo mode seeded: prng_init(0xC0DEC0DE)" | Prng init value 0xC0DEC0DE in main.cpp | ✅ consistent |

---

## C. Claim → evidence matrix

| Claim | Source evidence | Status |
|-------|-----------------|--------|
| Platform is ESP32-based standalone fuzzer | README, code, paper Fig. 1 | ✅ verified |
| UART framing (SYNC/CMD/LEN/SEQ/CHECKSUM) | uart_fuzzer.cpp, uart_parser.cpp | ✅ verified |
| 7 primary mutation classes | Table I, types.h (10 total) | ⚠️ clarify |
| 0–100 weighted robustness score | Eq. (3), result.cpp | ✅ verified |
| 45 host-side tests pass | tests/test_core.py: 45 passed | ✅ verified |
| 258 pkts / 0 ACK / 14 NACK / 0 fails / 75/100 | Serial log captured in earlier session | ✅ verified (simulated) |
| SPI/I2C not validated (stubs) | README, code | ✅ verified |
| Heartbeat GPIO25 in paper vs GPIO26 in code | README vs Pin::Heartbeat | **Fix** |
| No physical hardware measurements | Paper Sec. IV/VIII | ✅ disclosed (pending) |
| Seeded-fault validation planned | Sec. VIII/C | ✅ consistent (not done) |

---

## D. Reviewer concern mapping → paper section

| Reviewer concern | Paper section |
|------------------|---------------|
| Score = unexplained "robustness" | III-G (needs formula + semantics) |
| Verdict rules fuzzy | III-G / V |
| NACK from malformed packet = not firmware failure | III-G (add explicit) |
| No repetitions / no baselines | IV / VIII |
| No seeded-fault sensitivity | VIII / IX |
| SPI/I2C not real | VI-A / VIII |
| Hardware evaluation unverifiable | IV-C / VIII |
| Host tests ≠ firmware tests | IV-C (separate) |

---

## E. Corrections to be applied in corrected_paper.tex

1. **Verdict text:** add explicit PASS/FAIL/INCONCLUSIVE definitions (consistent with code).
2. **GPIO25 → GPIO26:** heartbeat pin corrected to GPIO26; Table III updated.
3. **Table I:** add "(subset of the protocol's ten mutation classes used in evaluation)".
4. **Abstract:** remove "45 host-side tests pass" as evidence of hardware robustness; reframe as internal-consistency evidence.
5. **Results:** clearly separate "physical hardware evaluation" (pending) from "simulated evaluation" (this revision).
6. **Score semantics:** define each category and show what it measures; add Eq. (3) text description.
7. **Scoring caveat:** the auto-pass in DEMO mode must be documented.
8. **Section VIII:** copy the paper's stated limitations; add a hardware-validation checklist.
