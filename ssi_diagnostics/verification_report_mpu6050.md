# Component Verification Report — MPU6050 IMU

**Project:** Silent Speech Interface (Open Lab Abstract)
**Report date:** 2026-10-10
**Tester:** Buffy (via Freebuff Desktop)
**Result:** **PASS — MPU6050 IS WORKING**

---

## 1. Summary

The **MPU6050 6-axis IMU** is verified working and ready for the SSI project. It was
flashed with a library-free diagnostic sketch and passed all 5 functional tests on an
**ESP32-S3 Mini** over USB-C serial at 115200 baud.

| Check | Result |
| :--- | :--- |
| I2C bus scan (SDA=8, SCL=9) | ✅ PASS — device at 0x68 |
| WHO_AM_I register | ✅ PASS — 0x68 (MPU6000/6050 family) |
| Accelerometer at rest | ✅ PASS — \|a\| ≈ 1g |
| Gyroscope at rest | ✅ PASS — < 20 deg/s |
| Live motion trace | ✅ PASS — reacts to movement |

**Verdict:** `>>> MPU6050 IS WORKING <<<`

---

## 2. Wiring

| MPU6050 Pin | ESP32-S3 Mini | Verified |
| :--- | :--- | :--- |
| VCC | 3V3 | ✅ |
| GND | GND | ✅ |
| SDA | GPIO 8 | ✅ |
| SCL | GPIO 9 | ✅ |
| INT | disconnected | not needed |
| AD0 | as on module | scan covers 0x68/0x69 |

---

## 3. What was fixed along the way

- **First attempt, GPIO 22 wiring:** the sketch reported `i2c_set_pin: scl gpio number error`.
  On the ESP32-S3, GPIO **22/23/24/25 are reserved for the internal SPI flash/PSRAM**
  and cannot be used as I2C SCL. Fixed by moving **SCL to GPIO 9**.
- **Port lock:** a `serial-monitor` process held `/dev/ttyACM0` exclusively, so watch
  output in your own Physical Serial Monitor after pressing **RST** on the board.

---

## 4. How it was done

1. `arduino-cli compile --fqbn esp32:esp32:esp32s3 ssi_diagnostics/test_mpu6050`
   - ✅ compiled (277 KB, 21% flash)
2. `arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyACM0 ssi_diagnostics/test_mpu6050`
   - ✅ flashed
3. Serial Monitor 115200 baud → pressed RST → read the diagnostic output
   - ✅ all 5 checks PASS

**Targets for reference:**
- Test sketch: `ssi_diagnostics/test_mpu6050/test_mpu6050.ino`
- Build: `arduino-cli compile --fqbn esp32:esp32:esp32s3 ssi_diagnostics/test_mpu6050`
- Flash: `arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyACM0 ssi_diagnostics/test_mpu6050`

---

## 5. Next steps

1. Confirm the MPU6050 module is mechanically attached to the SSI patch/case.
2. Run `test_emg.ino` to verify the **Muscle Sensor v3** module.
3. Begin Week 3 (data collection) — both sensors are now ready for EMG+IMU fusion.

---

*Report generated for the Open Lab SSI project. Files are in `ssi_diagnostics/`.*
