# Wiring Guide

Start with this UART-only setup. Unplug both USB cables before changing wires.
All connections are 3.3V logic and require a shared ground.

---

## ESP32 ↔ Arduino Nano (UART)

The Arduino Nano uses **SoftwareSerial on D2/D3** — not D0/D1.
D0/D1 are shared with the CH340 USB chip and cannot be used while USB is connected.

```
ESP32 GPIO 17 (TX)  ──→  Nano D3 (RX - SoftwareSerial)
ESP32 GPIO 16 (RX)  ←──  Nano D2 (TX - SoftwareSerial)
ESP32 GPIO 25       ←──  Nano D4 (Heartbeat)
ESP32 GND           ──→  Nano GND
```

### Pin Details

| ESP32 Pin | Nano Pin | Nano Label | Direction | Function |
| :--- | :--- | :--- | :--- | :--- |
| GPIO 17 | D3 | Digital 3 (SoftwareSerial RX) | TX → RX | Fuzzer → DUT |
| GPIO 16 | D2 | Digital 2 (SoftwareSerial TX) | RX ← TX | DUT → Fuzzer |
| GPIO 25 | D4 | Digital 4 | ← | Heartbeat from DUT |
| GND | GND | Any GND pin | ↔ | Common ground |

### Important Notes
- **SoftwareSerial baud rate**: 9600 (not 115200 — SoftwareSerial is unreliable above 9600 on ATmega328P)
- **Do NOT use D0/D1** — these are connected to the CH340 USB chip internally
- D2, D3, D4 are regular digital pins — safe to use while USB is connected
- Heartbeat: Nano firmware toggles D4 every 50ms (100ms period)

---

## ESP32 ↔ STM32 Nucleo-F446RE (UART)

```
ESP32 GPIO 17 (TX)  ──→  STM32 PA10 (USART1_RX)
ESP32 GPIO 16 (RX)  ←──  STM32 PA9  (USART1_TX)
ESP32 GPIO 25       ←──  STM32 PB5  (Heartbeat)
ESP32 GND           ──→  STM32 GND
```

### Pin Details

| ESP32 Pin | STM32 Pin | STM32 Label | Direction | Function |
| :--- | :--- | :--- | :--- | :--- |
| GPIO 17 | PA10 | CN10 pin 33 | TX → RX | Fuzzer → DUT |
| GPIO 16 | PA9 | CN10 pin 21 | RX ← TX | DUT → Fuzzer |
| GPIO 25 | PB5 | CN7 pin 17 | ← | Heartbeat from DUT |
| GND | GND | Any GND pin | ↔ | Common ground |

### Important Notes
- Use the **Morpho headers** (CN7, CN10) — not the Arduino-style headers
- Locate PA9, PA10, PB5 on the silk labels before wiring
- STM32 baud rate: 115200 (hardware UART, no SoftwareSerial limitation)
- Heartbeat: STM32 toggles PB5 every 50ms (100ms period)

---

## Planned Protocol Wiring

Do **not** connect these until UART milestone passes.

| Protocol | ESP32 | STM32 | Notes |
| :--- | :--- | :--- | :--- |
| SPI | 23 MOSI, 19 MISO, 18 SCK, 5 CS | PA7 MOSI, PA6 MISO, PA5 SCK, PA4 NSS | ESP32 master, STM32 slave |
| I2C | 21 SDA, 22 SCL | PB7 SDA, PB6 SCL | Add 4.7kΩ pull-ups if needed |
| CAN | 26 TX, 25 RX | PB9 TX, PB8 RX | Requires CAN transceiver |

---

## Hardware Safety

- **Common ground**: Always connect GND between AutoFuzzer and the DUT.
- **3.3V logic**: Both ESP32 and STM32 GPIO pins are 3.3V. Arduino Nano is 5V-tolerant on digital inputs but outputs 5V — the ESP32 GPIOs are 5V tolerant on inputs, so this works but be careful.
- **Target power**: AutoFuzzer provides 3.3V reference and 5V on headers. Do NOT assume every target can be powered from AutoFuzzer.
- **CAN transceivers**: Never connect MCU TX/RX pins directly to CANH/CANL. Always use a transceiver (e.g., SN65HVD230).
