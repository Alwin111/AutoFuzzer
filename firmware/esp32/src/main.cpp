#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// PIN CONFIGURATION
// ==========================================
// OLED (I2C)
constexpr int kOledSda = 21;
constexpr int kOledScl = 22;
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;

// Fuzzer Interface (Hardware Serial 2)
constexpr int kFuzzerTxPin = 17; // ESP32 TX2 -> STM32 PA10 (RX)
constexpr int kFuzzerRxPin = 16; // ESP32 RX2 <- STM32 PA9 (TX)
constexpr int kHeartbeatPin = 27; // Heartbeat input from STM32 PB5

// Indicators & UI Controls
constexpr int kPassLedPin = 2;    // Green LED (DUT Heartbeat OK)
constexpr int kFailLedPin = 4;    // Red LED (DUT Crash / Timeout Alert)
constexpr int kActiveLedPin = 15; // Yellow LED (Fuzzing Active Indicator)
constexpr int kBuzzerPin = 32;    // Piezo Alarm Buzzer

constexpr int kStartBtnPin = 12;  // Button 1: Start/Pause Fuzzer
constexpr int kModeBtnPin = 13;   // Button 2: Cycle Mutation Type
constexpr int kResetBtnPin = 14;  // Button 3: Reset Counters

// ==========================================
// GLOBAL OBJECTS & STATES
// ==========================================
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);

bool isFuzzingRunning = true;
uint32_t totalPacketsSent = 0;
uint32_t totalCrashes = 0;
uint32_t currentSeed = 0xC0DEC0DE;
uint16_t currentSequence = 0;
uint32_t lastHeartbeatEdge = 0;
bool lastHeartbeatState = LOW;
bool dutAlive = true;

// Mutation Strategies
enum MutationType {
  MUTATION_VALID = 0,
  MUTATION_EMPTY,
  MUTATION_MAX_PAYLOAD,
  MUTATION_OVERLENGTH,
  MUTATION_BAD_CHECKSUM,
  MUTATION_TRUNCATED,
  MUTATION_RANDOM,
  MUTATION_COUNT
};

const char* mutationNames[] = {
  "VALID", "EMPTY", "MAX_PAYLOAD", "OVERLENGTH", "BAD_CRC", "TRUNCATED", "RANDOM"
};

MutationType currentMutation = MUTATION_VALID;

// Deterministic PRNG
uint32_t xorshift32() {
  currentSeed ^= (currentSeed << 13);
  currentSeed ^= (currentSeed >> 17);
  currentSeed ^= (currentSeed << 5);
  return currentSeed;
}

// ==========================================
// OLED UI DISPLAY UPDATE
// ==========================================
void updateOledUI() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title Header
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("AUTOFUZZER v2.0");
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Status Rows
  display.setCursor(0, 13);
  display.printf("State : %s\n", isFuzzingRunning ? "RUNNING" : "PAUSED");

  display.setCursor(0, 24);
  display.printf("Mut   : %s\n", mutationNames[currentMutation]);

  display.setCursor(0, 35);
  display.printf("Pkts  : %u\n", totalPacketsSent);

  display.setCursor(0, 46);
  display.printf("DUT   : %s (Crashes:%u)\n", dutAlive ? "ALIVE [OK]" : "CRASHED!", totalCrashes);

  display.drawLine(0, 56, 128, 56, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.printf("Seed  : 0x%08X", currentSeed);

  display.display();
}

// ==========================================
// PACKET BUILD & TRANSMISSION
// ==========================================
void sendFuzzPacket() {
  if (!isFuzzingRunning) return;

  uint8_t buffer[64];
  uint8_t payloadLen = 8;
  uint8_t cmd = 0x01;
  uint8_t sync = 0xA5;

  switch (currentMutation) {
    case MUTATION_VALID:
      payloadLen = 8;
      break;
    case MUTATION_EMPTY:
      payloadLen = 0;
      break;
    case MUTATION_MAX_PAYLOAD:
      payloadLen = 32;
      break;
    case MUTATION_OVERLENGTH:
      payloadLen = 45; // Exceeds 32 max
      break;
    case MUTATION_BAD_CHECKSUM:
      payloadLen = 8;
      break;
    case MUTATION_TRUNCATED:
      payloadLen = 12;
      break;
    case MUTATION_RANDOM:
      payloadLen = (xorshift32() % 32) + 1;
      break;
    default:
      break;
  }

  buffer[0] = sync;
  buffer[1] = cmd;
  buffer[2] = payloadLen;
  buffer[3] = currentSequence & 0xFF;
  buffer[4] = (currentSequence >> 8) & 0xFF;

  uint8_t checksum = cmd ^ payloadLen ^ buffer[3] ^ buffer[4];

  for (uint8_t i = 0; i < payloadLen; i++) {
    uint8_t b = xorshift32() & 0xFF;
    buffer[5 + i] = b;
    checksum ^= b;
  }

  if (currentMutation == MUTATION_BAD_CHECKSUM) {
    checksum ^= 0xFF; // Invert checksum
  }

  uint8_t totalBytes = 5 + payloadLen;
  buffer[totalBytes] = checksum;
  totalBytes += 1;

  if (currentMutation == MUTATION_TRUNCATED) {
    totalBytes /= 2; // Cut packet in half
  }

  Serial2.write(buffer, totalBytes);
  totalPacketsSent++;
  currentSequence++;

  // Update Yellow Active LED
  digitalWrite(kActiveLedPin, HIGH);
  delay(10);
  digitalWrite(kActiveLedPin, LOW);
}

// ==========================================
// SETUP & INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, kFuzzerRxPin, kFuzzerTxPin);

  pinMode(kPassLedPin, OUTPUT);
  pinMode(kFailLedPin, OUTPUT);
  pinMode(kActiveLedPin, OUTPUT);
  pinMode(kBuzzerPin, OUTPUT);

  pinMode(kStartBtnPin, INPUT_PULLUP);
  pinMode(kModeBtnPin, INPUT_PULLUP);
  pinMode(kResetBtnPin, INPUT_PULLUP);
  pinMode(kHeartbeatPin, INPUT);

  Wire.begin(kOledSda, kOledScl);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Power On Audio/Visual Test
  digitalWrite(kPassLedPin, HIGH);
  digitalWrite(kActiveLedPin, HIGH);
  digitalWrite(kBuzzerPin, HIGH);
  delay(150);
  digitalWrite(kBuzzerPin, LOW);
  digitalWrite(kPassLedPin, LOW);
  digitalWrite(kActiveLedPin, LOW);

  lastHeartbeatEdge = millis();
  updateOledUI();
  Serial.println("--- AUTOFUZZER FIRMWARE V2.0 READY ---");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  static uint32_t lastPacketTime = 0;
  static uint32_t lastUiTime = 0;
  static uint32_t lastBtnCheck = 0;

  // 1. Monitor Heartbeat from Target (DUT)
  bool hbCurrent = digitalRead(kHeartbeatPin);
  if (hbCurrent != lastHeartbeatState) {
    lastHeartbeatState = hbCurrent;
    lastHeartbeatEdge = millis();
  }

  if (millis() - lastHeartbeatEdge > 350) {
    if (dutAlive) {
      dutAlive = false;
      totalCrashes++;
      digitalWrite(kPassLedPin, LOW);
      digitalWrite(kFailLedPin, HIGH);
      digitalWrite(kBuzzerPin, HIGH); // Alarm on crash
      delay(200);
      digitalWrite(kBuzzerPin, LOW);
      Serial.printf("ALERT: DUT Heartbeat Timeout! Crash #%u\n", totalCrashes);
      updateOledUI();
    }
  } else {
    if (!dutAlive) {
      dutAlive = true;
      digitalWrite(kFailLedPin, LOW);
      digitalWrite(kPassLedPin, HIGH);
      updateOledUI();
    } else {
      digitalWrite(kPassLedPin, HIGH);
    }
  }

  // 2. Handle User Button Controls
  if (millis() - lastBtnCheck > 150) {
    if (digitalRead(kStartBtnPin) == LOW) {
      lastBtnCheck = millis();
      isFuzzingRunning = !isFuzzingRunning;
      Serial.printf("Button 1: Fuzzing %s\n", isFuzzingRunning ? "STARTED" : "PAUSED");
      updateOledUI();
    }

    if (digitalRead(kModeBtnPin) == LOW) {
      lastBtnCheck = millis();
      currentMutation = (MutationType)((currentMutation + 1) % MUTATION_COUNT);
      Serial.printf("Button 2: Switched Mutation -> %s\n", mutationNames[currentMutation]);
      updateOledUI();
    }

    if (digitalRead(kResetBtnPin) == LOW) {
      lastBtnCheck = millis();
      totalPacketsSent = 0;
      totalCrashes = 0;
      currentSequence = 0;
      currentSeed = 0xC0DEC0DE;
      Serial.println("Button 3: Counters & PRNG Reset!");
      updateOledUI();
    }
  }

  // 3. Packet Fuzzing Interval
  if (isFuzzingRunning && (millis() - lastPacketTime > 100)) {
    lastPacketTime = millis();
    sendFuzzPacket();
  }

  // 4. Regular OLED Refresh
  if (millis() - lastUiTime > 500) {
    lastUiTime = millis();
    updateOledUI();
  }
}
