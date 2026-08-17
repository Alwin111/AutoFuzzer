# Wiring

Start with this UART-only setup. Unplug both USB cables before changing wires.
All connections are 3.3 V logic and require a shared ground.

## UART + heartbeat (milestone 1)

| ESP32 DevKit V1 | STM32 Nucleo-G431RB | Purpose |
|---|---|---|
| GND | GND | Common reference |
| GPIO 17 (TX2) | PA10 (USART1_RX) | Fuzzer to DUT input |
| GPIO 16 (RX2) | PA9 (USART1_TX) | DUT response to fuzzer |
| GPIO 27 | PB5 | DUT heartbeat to monitor |

The `PA9`, `PA10`, and `PB5` labels are STM32 pin names. Locate those labels
on the Nucleo's Morpho headers before wiring; board-header silk labels can
differ from Arduino header names.

## Planned protocol wiring

Do **not** connect these until the UART milestone passes.

| Protocol | ESP32 | STM32 | Notes |
|---|---|---|---|
| SPI | 23 MOSI, 19 MISO, 18 SCLK, 5 CS | PA7 MOSI, PA6 MISO, PA5 SCK, PA4 NSS | ESP32 master, STM32 slave |
| I2C | 21 SDA, 22 SCL | PB7 SDA, PB6 SCL | ESP32 master. Add one pair of 4.7 kOhm pull-ups to 3.3 V only if neither board/module already supplies them. |
| CAN controller | 26 TX, 25 RX | PB9 TX, PB8 RX | These connect to the logic-side pins of their respective CAN transceivers. |
| CAN bus | CANH/CANL via transceiver | CANH/CANL via transceiver | Use two 120 Ohm terminators, one at each physical end; share GND. |

For CAN, the ESP32's built-in TWAI controller needs an external transceiver;
the STM32 FDCAN controller also needs one. Never wire TX/RX MCU pins directly
to CANH/CANL.

