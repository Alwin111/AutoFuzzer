// ============================================================
// MPU6050 Diagnostic Test — Silent Speech Interface project
// Target: ESP32-S3 DevKit  |  No external libraries needed
//
// WHAT IT CHECKS
//   1. I2C bus responds (device ACKs its address)
//   2. WHO_AM_I register returns a valid MPU6050-family ID
//   3. Accelerometer reads physically sane values (~1 g on one
//      axis when the board lies flat / hangs still)
//   4. Gyro is near zero (deg/s) at rest
//   5. Live motion detection — shake the board and watch it react
//
// WIRING (ESP32-S3 Mini: native I2C pins)
//   MPU6050 VCC -> 3V3
//   MPU6050 GND -> GND
//   MPU6050 SDA -> GPIO 8
//   MPU6050 SCL -> GPIO 9
//   (AD0 low = address 0x68, AD0 high = 0x69)
//
// SERIAL: 115200 baud
// ============================================================

#include <Wire.h>

// ---- Pins (ESP32-S3 Mini default Wire pins: SDA=8, SCL=9) ----
#define SDA_PIN 8
#define SCL_PIN 9
#define MPU_ADDR_1 0x68
#define MPU_ADDR_2 0x69

// ---- MPU6050 registers ----
#define REG_WHO_AM_I   0x75
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT 0x3B
#define REG_GYRO_XOUT  0x43

uint8_t mpuAddr = 0;
bool busOK = false, idOK = false, accelOK = false, gyroOK = false;

uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

bool readBurst(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void printVerdict() {
  Serial.println("\n================ VERDICT ================");
  Serial.printf("  I2C bus ACK        : %s\n", busOK  ? "PASS" : "FAIL (check SDA/SCL/VCC/GND wiring)");
  Serial.printf("  WHO_AM_I           : %s\n", idOK   ? "PASS" : "FAIL (wrong chip or bad solder joint)");
  Serial.printf("  Accel sane         : %s\n", accelOK ? "PASS" : "FAIL (sensor flat? readout stuck?)");
  Serial.printf("  Gyro quiet at rest : %s\n", gyroOK ? "PASS" : "FAIL (moving? or sensor fault)");
  if (busOK && idOK && accelOK && gyroOK)
    Serial.println("  >>> MPU6050 IS WORKING <<<");
  else
    Serial.println("  >>> MPU6050 NOT WORKING — fix items marked FAIL <<<");
  Serial.println("=========================================");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== MPU6050 Diagnostic (ESP32-S3) ===");

  // --- Test 1: bus scan ---
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Serial.printf("[1] Scanning I2C bus (SDA=%d, SCL=%d)...\n", SDA_PIN, SCL_PIN);

  for (uint8_t a : { MPU_ADDR_1, MPU_ADDR_2 }) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { mpuAddr = a; busOK = true; break; }
  }
  if (busOK) {
    Serial.printf("    Found device at 0x%02X  -> PASS\n", mpuAddr);
  } else {
    Serial.println("    No device found at 0x68/0x69 -> FAIL");
    Serial.println("    Checklist: VCC to 3V3, GND to GND, SDA=GPIO8, SCL=GPIO9,");
    Serial.println("               pull-ups (most breakouts have them), solder on header pins.");
    printVerdict();
    return; // nothing else to test
  }

  // --- Test 2: WHO_AM_I + wake up ---
  uint8_t who = readReg(mpuAddr, REG_WHO_AM_I);
  Serial.printf("[2] WHO_AM_I = 0x%02X (expected 0x68 on MPU-6000/6050; 0x70/0x72 on some clones)\n", who);
  idOK = (who == 0x68 || who == 0x70 || who == 0x72 || who == 0x98);
  Serial.println(idOK ? "    Recognised device ID -> PASS" :
                        "    Unknown ID -> FAIL (chip may be a different IC, e.g. QMI8658 clone)");

  // Wake up + clear sleep bit
  Wire.beginTransmission(mpuAddr);
  Wire.write(REG_PWR_MGMT_1); Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  // --- Test 3/4: rest-state accel & gyro ---
  Serial.println("[3] Holding still for rest-state sample...");
  delay(1000);

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  const int N = 200;
  for (int i = 0; i < N; i++) {
    uint8_t b[14];
    if (!readBurst(mpuAddr, REG_ACCEL_XOUT, b, 14)) {
      Serial.println("    Burst read failed -> FAIL");
      printVerdict(); return;
    }
    int16_t rawAX = (b[0] << 8) | b[1], rawAY = (b[2] << 8) | b[3], rawAZ = (b[4] << 8) | b[5];
    int16_t rawGX = (b[8] << 8) | b[9], rawGY = (b[10] << 8) | b[11], rawGZ = (b[12] << 8) | b[13];
    ax += (float)rawAX / 16384.0; ay += (float)rawAY / 16384.0; az += (float)rawAZ / 16384.0;
    gx += (float)rawGX / 131.0;   gy += (float)rawGY / 131.0;   gz += (float)rawGZ / 131.0;
    delay(5);
  }
  ax /= N; ay /= N; az /= N; gx /= N; gy /= N; gz /= N;

  Serial.printf("    Accel (g): X=%0.2f  Y=%0.2f  Z=%0.2f\n", ax, ay, az);
  Serial.printf("    Gyro (deg/s): X=%0.2f  Y=%0.2f  Z=%0.2f\n", gx, gy, gz);

  float gMag = sqrtf(ax * ax + ay * ay + az * az);
  accelOK = (gMag > 0.85 && gMag < 1.15);
  Serial.printf("    |a| = %0.2f g ", gMag);
  Serial.println(accelOK ? "(expected ~1g) -> PASS" : "not ~1g -> FAIL");

  gyroOK = (fabsf(gx) < 20 && fabsf(gy) < 20 && fabsf(gz) < 20);
  Serial.println(gyroOK ? "    Gyro quiet -> PASS" : "    Gyro noisy/drifting -> FAIL");

  printVerdict();
  Serial.println("\n--- Live motion test: shake the board — watch values change (Ctrl+C not needed) ---");
}

void loop() {
  if (!busOK) { delay(1000); return; }

  uint8_t b[14];
  if (!readBurst(mpuAddr, REG_ACCEL_XOUT, b, 14)) { Serial.println("read ERROR"); delay(500); return; }

  int16_t rawAX = (b[0] << 8) | b[1], rawAY = (b[2] << 8) | b[3], rawAZ = (b[4] << 8) | b[5];
  int16_t rawGX = (b[8] << 8) | b[9], rawGY = (b[10] << 8) | b[11], rawGZ = (b[12] << 8) | b[13];

  float ax = (float)rawAX / 16384.0, ay = (float)rawAY / 16384.0, az = (float)rawAZ / 16384.0;
  float gx = (float)rawGX / 131.0, gy = (float)rawGY / 131.0, gz = (float)rawGZ / 131.0;
  float gMag = sqrtf(ax * ax + ay * ay + az * az);

  // Motion detector: any deviation from 1g or fast gyro = MOTION
  bool motion = fabsf(gMag - 1.0) > 0.15 || fabsf(gx) > 30 || fabsf(gy) > 30 || fabsf(gz) > 30;

  Serial.printf("A[X%+0.2f Y%+0.2f Z%+0.2f] G[X%+6.1f Y%+6.1f Z%+6.1f] %s\n",
                ax, ay, az, gx, gy, gz, motion ? ">>> MOTION! <<<" : "still");
  delay(200);
}
