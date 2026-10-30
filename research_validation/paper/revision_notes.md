# Revision Notes — AutoFuzzer Research Validation Package

Date: 2026-10-30

## What this package contains

1. `paper/submitted_paper.pdf` — byte-for-byte preservation of the submitted paper
   (verified against the original upload with md5 `6f2e32c590ce0eac8a22b907978e9584`).
2. `paper/paper_audit.md` — claim-by-claim audit of the submitted paper, including the
   inconsistency map between the PDF and the repository source.
3. `paper/claim_evidence_matrix.csv` — machine-readable claim -> evidence mapping.
4. `paper/corrected_paper.tex` — the corrected paper in the requested structure,
   including the explicit "Software-Based Simulation and Preliminary Evaluation"
   section and the hardware-pending disclosure.
5. `simulation/simulate.py` — deterministic fault-injection simulator; it implements the
   documented fault-injection rules F0-F4 and is fully reproducible from source + seed.
6. `simulation/config.yaml` — simulation configuration (scenarios, packet counts, seeds,
   campaigns). Recorded with every generated report.
7. `simulation/results.json` — the generated deterministic simulation dataset (15 trials
   across F0-F4 x Quick/Standard/Deep). Reproducible with
   `python3 research_validation/simulation/simulate.py --report-path research_validation/simulation/results.json`.
8. `simulation/README.md` — interpretation and use of the simulated results.
9. `hardware_runs/README.md`, `hardware_runs/validation_checklist.md`,
   `hardware_runs/hardware_configuration.example.yaml`,
   `hardware_runs/expected_output_schema.json` — the hardware validation architecture,
   ready for future physical testing.
10. `hardware_validation/README.md` — host-side Python reference suite index.

## What is verified and what remains pending

- Verified: source-code analysis, firmware compilation, the deterministic simulator,
  generated synthetic experimental data, and the hardware validation architecture.
- Pending: physical hardware evaluation (ESP32 AutoFuzzer controller, Arduino Nano,
  STM32 Nucleo, UART/SPI/I2C hardware tests) is marked `pending_hardware` throughout.
  No hardware test is marked completed.

## Integrity statement

These results are explicitly classified as SOFTWARE-BASED SIMULATION. They are not
physical hardware measurements and must not be inserted into the paper as hardware
results. The corrected paper states this disclosure in Section IX and the abstract.
