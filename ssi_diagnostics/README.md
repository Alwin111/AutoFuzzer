# SSI Component Diagnostics

Two standalone Arduino sketches to verify the two sensors in your
Silent Speech Interface project work before you build everything else
around them.

- [test_mpu6050/](test_mpu6050) — Verifies the MPU6050 IMU over I2C
- [test_emg/](test_emg) — Verifies the Muscle Sensor v3 EMG board on ADC

Both target **ESP32-S3** at **115200 baud** serial.

## Wiring

### MPU6050 (I2C)

| MPU6050 | ESP32-S3 |
| :--- | :--- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

### Muscle Sensor v3 (analog)

| Muscle Sensor v3 (J1) | ESP32-S3 |
| :--- | :--- |
| +VS | 5V (VIN) — 3.3V also works, smaller range |
| GND | GND |
| -VS | leave open for single-supply use |
| SIG (J2) | GPIO 4 (through 10k series resistor) |
| GND (J2) | (already common) |

⚠ The v3 output can exceed 3.3V when powered from 5V on strong
contraction. A 10k/20k divider between SIG and the ADC pin is the safe
option — see the sketch header comment for the circuit.

## How to flash

```bash
# from inside each sketch folder, or use Arduino IDE
arduino-cli compile --fqbn esp32:s3:esp32 ssi_diagnostics/test_mpu6050
arduino-cli upload  --fqbn esp32:s3:esp32 --port /dev/ttyUSB0 ssi_diagnostics/test_mpu6050

arduino-cli compile --fqbn esp32:s3:esp32 ssi_diagnostics/test_emg
arduino-cli upload  --fqbn esp32:s3:esp32 --port /dev/ttyUSB0 ssi_diagnostics/test_emg
```

Or simply open each folder in the Arduino IDE, pick an ESP32-S3 board,
and click Upload.

## What to expect

Each sketch prints a numbered test section and finishes with a clear
`>>> MODULE IS WORKING <<<` or `>>> FAIL: ... <<<` verdict, followed by
a live data stream you can watch while moving/flexing.

**Pass criteria:**

- MPU6050: I2C ACK, sane WHO_AM_I, |a| ≈ 1 g, gyro quiet at rest, values change when shaken
- EMG: quiet baseline, clear spike above baseline when muscle flexes, no rail-pin or flatline
