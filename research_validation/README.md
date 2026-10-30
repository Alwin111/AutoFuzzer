# AutoFuzzer — Research Validation Package

This package is the post-submission research-compliance deliverable for the AutoFuzzer
paper (ICITIIT 27). Its purpose is to make the submission reproducible and to separate
exactly what was verified from what is still pending.

All files are included in the repository so that a reviewer or a future hardware owner can
run the full pipeline.

## Directory layout

```
research_validation/
├── hardware_runs/          Hardware validation scripts and configuration template
├── paper/
│   ├── submitted_paper.pdf              Original submission (byte-for-byte preserved)
│   ├── paper_audit.md                   Page- and claim-level audit
│   ├── claim_evidence_matrix.csv        Claim -> evidence mapping
│   ├── corrected_paper.tex              Corrected LaTeX paper
│   ├── corrected_paper.pdf              Compiled corrected paper
│   └── revision_notes.md                Explanation of every change
├── simulation/            Software-based simulation and preliminary evaluation
└── hardware_validation/   Host-side Python reference suite (+ README)
```

## What is verified here

- [x] **Simulated experimental data** produced by a deterministic fault-injection
      simulator (~200 trials across F0-F4), including detection rates, reproducibility,
      minimization, and confidence intervals.
- [x] **Host-side Python reference suite** (45 tests) that exercises the parser,
      scorer, replay, minimization, and campaign-model logic in C++ and Python.
- [x] **Firmware compilation** of `firmware/esp32` (PlatformIO + `pio run`) and of the
      Python simulator in an isolated virtual environment.
- [x] **Hardware validation architecture**: `hardware_runs/` contains an experiment runner,
      configuration template, expected output schema, and a checklist.
- [x] **Corrected paper** (`research_validation/paper/corrected_paper.tex`) with the
      revised structure, the hardware-status disclosure, and the simulation section.

## What is still pending and why

- **Physical hardware evaluation**: no AutoFuzzer controller or DUT hardware was available.
      Any report asserting a PASS/FAIL from an actual target is marked `pending_hardware`.
- **Replay and minimization in firmware**: the commands exist in the source, but the seed
      archive and failure store have not been exercised on a real DUT.
- **SPI/I2C transport**: the protocol module declares the interfaces; only UART is wired
      end-to-end in the firmware.
- **Canonical wiring check**: the controller was flashed once (MPU6050 diagnostic) and the
      serial output was captured for the MPU6050 component only. The firmware payload logic
      (checksum, heartbeat parsing, score) has not been exercised on physical hardware.

## Key deliverables

| Deliverable | Location |
|---|---|
| Corrected paper source | `research_validation/paper/corrected_paper.tex` |
| Corrected paper PDF | `research_validation/paper/corrected_paper.pdf` |
| Claim-evidence matrix | `research_validation/paper/claim_evidence_matrix.csv` |
| Paper audit | `research_validation/paper/paper_audit.md` |
| Revision notes | `research_validation/paper/revision_notes.md` |
| Simulation runner + data | `research_validation/simulation/` |
| Host Python reference suite + README | `hardware_validation/` |
| Hardware config template + runner | `research_validation/hardware_runs/` |

For the component-level verification done before this paper revision (MPU6050 IMU and
EMG module diagnostics, wiring fixes, and serial interpretation), see
`ssi_diagnostics/verification_report_mpu6050.md` and the matching sketches.
