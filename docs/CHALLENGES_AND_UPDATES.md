# AutoFuzzer Technical Challenges & Solutions

## Overview
During the development and testing of **AutoFuzzer** (Embedded Communication Fuzzer & Target Firmware Robustness Testing Framework), several hardware, software, and protocol-level challenges were encountered and successfully overcome.

---

## 1. Hardware & Soldering Challenges (Dot Board vs. Direct Connections)

### Problem: OLED Display Dark on Dot Board Setup
* **Symptom:** When connecting the 0.96" SSD1306 OLED display to the ESP32 using female-to-female jumper wires, the display worked perfectly. However, after soldering to a dot board, the display remained dark.
* **Root Cause Analysis:**
  1. **I2C Bus ACK Check:** Using a custom I2C scanner, the ESP32 confirmed address `0x3C` was acknowledging requests over `GPIO 21 (SDA)` and `GPIO 22 (SCL)`.
  2. **Internal Charge Pump Voltage Drop:** The SSD1306 controller contains an internal DC-DC charge pump (boosting 3.3V to 7–9V to illuminate OLED LEDs). Thin dot board tracks and dry solder joints caused a small voltage drop across `VCC` and `GND`, leaving the logic alive (I2C responding) while keeping the screen matrix dark.
* **Solution:**
  * Reflowed all 4 header joints with flux.
  * Verified solid power rails (`VCC` at 3.3V/5V and common ground).

---

## 2. Firmware & Serial Upload Failures

### Problem: Upload Interruption at High Baud Rates
* **Symptom:** During high-speed firmware uploads via PlatformIO / `arduino-cli`, the upload failed with: `A fatal error occurred: Packet content transfer stopped (received 6 bytes)`.
* **Root Cause:** Signal degradation and USB-to-UART converter timing jitter at `921600` baud.
* **Solution:** Explicitly configured `--upload-property upload.speed=115200` to ensure stable firmware flashing.

---

## 3. Communication & Parser Robustness Challenges

### Problem: UART Parser Wedging & Truncated Packets
* **Symptom:** When sending malformed or truncated packet mutations (e.g. fewer bytes than specified in the `LENGTH` byte), the target parser would hang indefinitely waiting for bytes.
* **Solution:**
  * Implemented a **30 ms inter-byte packet timeout** in the STM32 target parser.
  * If no byte arrives within 30 ms, the frame buffer automatically flushes and resets to the `SYNC (0xA5)` state.

---

## 4. Hardware Heartbeat Monitoring

### Problem: False Crash Detection
* **Symptom:** Small CPU delays in the DUT parser caused false heartbeat timeouts on the fuzzer.
* **Solution:** Configured a **350 ms safety threshold** on the ESP32 interrupt/timer monitoring the **100 ms PB5 square wave** from the STM32.
