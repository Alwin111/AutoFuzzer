# Hardware Validation Runs — AutoFuzzer

This directory holds the configuration template, the experiment runner, the expected output
schema, and the hardware verification checklist. It is designed so that a hardware owner can
start a campaign without changing the source tree.

> **Hardware status (2026 revision):** No physical hardware is available in this workspace.
> Every file below is written for future use. Do not mark any hardware test as complete,
> and do not claim hardware-verified results.

## What hardware it supports

- **Controller:** ESP32 DevKit V1 (AutoFuzzer)
- **Reference targets:** Arduino Nano, STM32 Nucleo, ESP32 loopback
- **Protocols:** UART (primary), SPI (stub), I2C (stub)

## Files

| File | Purpose |
|---|---|
| `README.md` | This file |
| `hardware_configuration.example.yaml` | Full configuration template for a campaign |
| `experiment_runner.py` | Runs a campaign from a YAML config; prints a result JSON |
| `validation_checklist.md` | Checklist for operator-led hardware validation |
| `expected_output_schema.json` | JSON schema for machine-readable campaign results |

## How to run a campaign

1. Install the dependencies:
   ```bash
   python3 -m venv .venv-hw
   source .venv-hw/bin/activate
   pip install -r research_validation/hardware_runs/requirements.txt
   ```
2. Copy the example configuration:
   ```bash
   cp research_validation/hardware_runs/hardware_configuration.example.yaml \
      research_validation/hardware_runs/runs/esp32_nano_uart.yaml
   ```
3. Edit the file for the target board, protocol, pin mapping, baud rate or bus speed,
   campaign profile, campaign duration, mutation configuration, random seed, response
   timeout, heartbeat timeout, and firmware version.
4. Run the runner:
   ```bash
   python3 research_validation/hardware_runs/experiment_runner.py \
     --config research_validation/hardware_runs/runs/esp32_nano_uart.yaml
   ```
5. Inspect the printed result JSON, which is validated against
   `expected_output_schema.json`.

An Internet connection is not required. The runner's only third-party dependency is
`pyyaml` (line 18 of `requirements.txt`).

## What the runner does

- Loads the YAML configuration and raises on missing required keys.
- Waits for the target to publish a heartbeat for the configured `grace_period` seconds;
  connection failure is recorded as `hardware_connected: false`.
- Runs the campaign profile with the configured `mutation` distribution.
- Parses response lines, records `response_timestamps`, and classifies the deadline
  behaviour as follows:
  - `PASS` — baseline communication is verified, valid frames receive expected responses,
    and no configured failure condition (heartbeat loss or timeout) is detected.
  - `FAIL` — a configured and observable failure condition (e.g., heartbeat loss,
    timeout, or protocol violation) is detected.
  - `INCONCLUSIVE` — the target could not be validated, baseline communication failed,
    or the test lacks sufficient evidence.
- The result maps every `heartbeat_timeout` to `FAIL` (the heartbeat loss is an
  observable failure condition). A `NACK` for an intentionally malformed packet is not
  treated as firmware failure: `malformed_packet_nacks` are recorded separately.
- Prints the result JSON on stdout and writes `result.json` in the campaign directory.
- Does not stop the experiment when a NACK is received; only heartbeat loss, response
  timeouts, and protocol failures mark the campaign as failed.

## Interpreting the result JSON

```json
{
  "session_id": "20261030_001",
  "hardware_connected": true,
  "target": "arduino_nano",
  "protocol": "uart",
  "firmware_version": "v1.7.0",
  "communication_health": "verified",
  "campaign_profile": "quick",
  "packets_sent": 18,
  "acks": 12,
  "nacks": 4,
  "no_response": 1,
  "heartbeat_timeouts": 0,
  "verdict": "PASS",
  "score": 88,
  "fault_category_counts": {"boundary": 0, "malformed": 0, "random": 0},
  "malformed_packet_nacks": 2,
  "execution_mode": "hardware"
}
```

## Why these verdict rules exist

- A NACK for an intentionally malformed packet is not automatically a firmware failure:
  the DUT may be following the protocol correctly.
- A missing response must be recorded separately from a detected firmware crash (hence
  `no_response`, `malformed_packet_nacks`, and `heartbeat_timeouts` are distinct fields).
- `INCONCLUSIVE` is returned when baseline communication fails or the test lacks
  sufficient evidence, not by default.
