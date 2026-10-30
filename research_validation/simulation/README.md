# AutoFuzzer — Simulation Framework

This directory contains the deterministic simulation framework used to generate the
software-based experimental data in the corrected paper. It is **not** a substitute for the
physical hardware evaluation, which is marked as pending in that revision. Every number in
the simulation section of the paper is explicitly classified as:

> **SIMULATED -- NOT PHYSICAL HARDWARE MEASUREMENTS**

The simulator and every generated result are deterministic: the same source, the same
configuration, and the same seed reproduce identical results.

## Files

| File | Purpose |
|---|---|
| `simulate.py` | Deterministic fault-injection simulator (Python reference model + trials) |
| `config.yaml` | Simulation configuration (scenarios, seeds, campaigns, packet counts) |
| `README.md` | This file |

## Running the simulation

```bash
cd research_validation
python3 simulation/simulate.py --config simulation/config.yaml --report-path simulation/results.json
```

The report is written to `simulation/results.json`. A directory containing the raw
trial-level output is also produced for external audit.

## Scenarios

| Scenario | Fault injection rule | Expected outcome |
|---|---|---|
| F0 | Clean reference firmware; no fault condition defined | No fault, clean traffic |
| F1 | Payload-length validation fault (overflow / oversized payload) | Fault detected |
| F2 | Incorrect checksum handling in the parser | Fault detected |
| F3 | Truncated-frame handling fault | Fault detected |
| F4 | Input-dependent fault (defined input condition) | Fault detected |

## Recorded per trial

- Simulation ID
- Target variant
- Seed
- Firmware/reference-model version
- Number of packets
- ACK count
- NACK count
- No-response count
- Heartbeat timeouts
- Detection result
- Detection latency
- Packets until detection
- Replay result
- Minimized input length
- Verdict (PASS / FAIL / INCONCLUSIVE)
- Score

## Analysis

- Fault-detection rate
- False-positive rate
- False-negative rate
- Median detection latency
- Median packets until detection
- Replay reproducibility
- Minimization results

Confidence intervals are reported at the 95% level over the trials that reached the fault
condition. A 95% confidence interval has the following form:

> observed_value ± t_(0.975, n-1) * s / sqrt(n)

where t is the Student-t quantile and s is the sample standard deviation. The simulator
derives this from the recorded trial series; it does **not** treat the simulator as a
physical measurement of the real platform.

## Limitations

- The results characterize the reference model and the software fault-injection rules.
- They are not measured on the physical AutoFuzzer controller, the Arduino Nano, the STM32
  Nucleo, or any other target.
- No UART/SPI/I2C messages or heartbeat signals from the actual hardware are involved.
