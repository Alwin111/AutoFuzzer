# Hardware Validation Checklist — AutoFuzzer

Use this checklist when physical hardware becomes available. It is deliberately
operator-led so no file in this repository can accidentally be marked complete before a
real test. **Do not mark any item done until the physical test is performed and
recorded in the campaign log.**

## Pre-flight

- [ ] ESP32 AutoFuzzer controller built from `firmware/esp32` and flashed (version v1.7.0)
- [ ] Reference target (Arduino Nano / STM32 Nucleo / ESP32 loopback) flashed with the matching
      frame-protocol firmware
- [ ] Shared ground and matching logic levels confirmed
- [ ] UART/SPI/I2C wiring matches the `pin_mapping` in the selected YAML config
- [ ] `hardware_configuration.example.yaml` copied and edited for the target
- [ ] Required UART/SPI/I2C lines or pins connected; non-used lines floating or tied as
      documented
- [ ] Power supply verified before connecting the target
- [ ] `python3 -m venv .venv-hw` and `pip install -r requirements.txt` completed
- [ ] `experiment_runner.py --config <campaign.yaml>` runs with no config errors

## Live test

- [ ] Target boots and publishes heartbeat within `grace_period_seconds`
- [ ] Baseline communication is verified: valid frames receive expected responses
- [ ] Campaign runs for the configured duration with the configured mutation profile
- [ ] Response lines are logged
- [ ] `heartbeat_timeouts` is recorded separately from `no_response` and
      `malformed_packet_nacks`
- [ ] NACK for an intentionally malformed packet is recorded separately, not as firmware
      failure
- [ ] Result JSON saved to `result.json` and validated against
      `expected_output_schema.json`
- [ ] Replay is run on at least the first detected failure
- [ ] Minimization is run on the first failing test case

## Post-test

- [ ] Verdict is `PASS` / `FAIL` / `INCONCLUSIVE` according to the README rules
- [ ] `execution_mode` is recorded as `hardware`
- [ ] Scheduler seed, campaign profile, and packet counts recorded for reproducibility
- [ ] No item is marked done without the physical test result

## Hardware used

- [ ] Target board: _____________
- [ ] Target firmware version: _____________
- [ ] Controller board: _____________
- [ ] Protocol: _____________
- [ ] Pin mapping: _____________
- [ ] Baud rate / bus speed: _____________
- [ ] Campaign profile: _____________
- [ ] Campaign duration: _____________
- [ ] Mutation configuration: _____________
- [ ] Random seed: _____________
- [ ] Response timeout: _____________
- [ ] Heartbeat timeout: _____________
- [ ] Firmware version: _____________
