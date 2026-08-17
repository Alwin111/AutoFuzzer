# AutoFuzzer

Low-cost, board-to-board robustness testing for firmware communication handlers.

The ESP32 is the fuzzer and health monitor.  The STM32 Nucleo-G431RB is the
device under test (DUT).  The initial, usable milestone is a UART fuzzer with
a GPIO heartbeat.  SPI, I2C, and CAN use the same frame format and are added
after UART logging and failure reproduction are verified.

## Safety and scope

Only connect AutoFuzzer to hardware you own or are expressly authorised to
test. Both boards use 3.3 V logic. Never connect a 5 V UART, CAN-TTL module,
or external powered bus directly to an MCU pin.

## Repository layout

- `firmware/esp32`: fuzzer controller firmware
- `firmware/stm32`: deliberately robust DUT reference firmware
- `docs/wiring.md`: first-stage wiring and later protocol pin plan
- `docs/protocol.md`: frame definition and expected results

## First milestone

1. Wire only UART, GND, and heartbeat as documented.
2. Flash the STM32 DUT firmware, then flash the ESP32 fuzzer firmware.
3. Open the ESP32 serial monitor at 115200 baud.
4. Confirm that `ACK`, `NACK`, and `TIMEOUT` results appear while the STM32
   heartbeat remains alive.

The ESP32 emits its test seed, sequence number, mutation type, and raw frame.
Those four fields are enough to replay an interesting input later.

