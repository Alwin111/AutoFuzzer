#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
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

// Fuzzer UART (Hardware Serial 2)
constexpr int kFuzzerTxPin = 17; // ESP32 TX2 -> Target RX
constexpr int kFuzzerRxPin = 16; // ESP32 RX2 <- Target TX
constexpr int kHeartbeatPin = 27; // Heartbeat input from Target

// SPI Fuzzer Pins
constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kSpiCs = 5;

// Indicators & UI Controls
constexpr int kPassLedPin = 2;    // Green LED (Target Healthy)
constexpr int kFailLedPin = 4;    // Red LED (Target Crash Alert)
constexpr int kActiveLedPin = 15; // Yellow LED (Transmission Active)
constexpr int kBuzzerPin = 32;    // Piezo Alarm Buzzer

constexpr int kStartBtnPin = 12;  // Button 1: Start/Pause Fuzzer
constexpr int kModeBtnPin = 13;   // Button 2: Cycle Protocol/Mutation Mode
constexpr int kResetBtnPin = 14;  // Button 3: Reset Stats & AI Weights

// ==========================================
// PROTOCOL & MUTATION MODES
// ==========================================
enum ModeType {
  MODE_UART_VALID = 0,
  MODE_UART_EMPTY,
  MODE_UART_MAX,
  MODE_UART_OVERLENGTH,
  MODE_UART_BAD_CRC,
  MODE_UART_TRUNCATED,
  MODE_UART_RANDOM,
  MODE_SPI_FUZZ,
  MODE_I2C_FUZZ,
  MODE_AI_ADAPTIVE,
  MODE_COUNT
};

const char* modeNames[] = {
  "UART: VALID",
  "UART: EMPTY",
  "UART: MAX_LEN",
  "UART: OVERLEN",
  "UART: BAD_CRC",
  "UART: TRUNC",
  "UART: RANDOM",
  "SPI: FUZZ",
  "I2C: SCAN_FUZZ",
  "AI: ADAPTIVE"
};

// ==========================================
// GLOBAL OBJECTS & STATE
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

ModeType currentMode = MODE_UART_VALID;

// AI Reinforcement Learning Crash Weights (for adaptive mutation)
uint32_t aiMutationCrashWeights[7] = {1, 1, 2, 5, 5, 4, 3};
uint32_t aiTotalWeight = 21;
uint8_t lastAiMutation = 0;

// Deterministic PRNG
uint32_t xorshift32() {
  currentSeed ^= (currentSeed << 13);
  currentSeed ^= (currentSeed >> 17);
  currentSeed ^= (currentSeed << 5);
  return currentSeed;
}

// ==========================================
// OLED UI DISPLAY
// ==========================================
void updateOledUI() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title Header
  display.setTextSize(1);
  display.setCursor(12, 0);
  display.println("AUTOFUZZER v3.0");

  // Status Rows
  display.setCursor(0, 13);
  display.printf("State : %s\n", isFuzzingRunning ? "RUNNING" : "PAUSED");

  display.setCursor(0, 24);
  display.printf("Mode  : %s\n", modeNames[currentMode]);

  display.setCursor(0, 35);
  display.printf("Pkts  : %u\n", totalPacketsSent);

  display.setCursor(0, 44);
  display.printf("DUT   : %s (C:%u)\n", dutAlive ? "OK" : "FAIL!", totalCrashes);

  display.setCursor(0, 54);
  display.printf("Seed  : 0x%08X", currentSeed);

  display.display();
}

// ==========================================
// AI REINFORCEMENT LEARNING ENGINE
// ==========================================
uint8_t selectAiMutation() {
  uint32_t roll = xorshift32() % aiTotalWeight;
  uint32_t cumulative = 0;
  for (uint8_t i = 0; i < 7; i++) {
    cumulative += aiMutationCrashWeights[i];
    if (roll < cumulative) {
      lastAiMutation = i;
      return i;
    }
  }
  return 6; // Default to random
}

void recordAiCrashReward() {
  // Reward the mutation strategy that successfully caused a crash
  if (currentMode == MODE_AI_ADAPTIVE) {
    aiMutationCrashWeights[lastAiMutation] += 3;
    aiTotalWeight += 3;
    Serial.printf("AI RL: Rewarded mutation index %u (New Weight: %u)\n", 
                  lastAiMutation, aiMutationCrashWeights[lastAiMutation]);
  }
}

// ==========================================
// UART FUZZER DRIVER
// ==========================================
void sendUartFuzzPacket(uint8_t mutationIdx) {
  uint8_t buffer[64];
  uint8_t payloadLen = 8;
  uint8_t cmd = 0x01;
  uint8_t sync = 0xA5;

  switch (mutationIdx) {
    case 0: payloadLen = 8; break;                   // Valid
    case 1: payloadLen = 0; break;                   // Empty
    case 2: payloadLen = 32; break;                  // Max
    case 3: payloadLen = 45; break;                  // Overlength
    case 4: payloadLen = 8; break;                   // Bad CRC
    case 5: payloadLen = 12; break;                  // Truncated
    case 6: payloadLen = (xorshift32() % 32) + 1; break; // Random
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

  if (mutationIdx == 4) checksum ^= 0xFF; // Invert CRC

  uint8_t totalBytes = 5 + payloadLen;
  buffer[totalBytes] = checksum;
  totalBytes += 1;

  if (mutationIdx == 5) totalBytes /= 2; // Cut truncated packet

  Serial2.write(buffer, totalBytes);
  totalPacketsSent++;
  currentSequence++;
}

// ==========================================
// SPI FUZZER DRIVER
// ==========================================
void sendSpiFuzzPacket() {
  digitalWrite(kSpiCs, LOW);
  delayMicroseconds(10);
  
  uint8_t spiLen = (xorshift32() % 16) + 4;
  for (uint8_t i = 0; i < spiLen; i++) {
    SPI.transfer((uint8_t)(xorshift32() & 0xFF));
  }
  
  delayMicroseconds(10);
  digitalWrite(kSpiCs, HIGH);
  totalPacketsSent++;
}

// ==========================================
// I2C BUS FUZZER DRIVER
// ==========================================
void sendI2cFuzzPacket() {
  uint8_t targetAddr = (xorshift32() % 112) + 8; // Valid 7-bit I2C range
  Wire.beginTransmission(targetAddr);
  
  uint8_t regAddr = (uint8_t)(xorshift32() & 0xFF);
  Wire.write(regAddr);
  
  uint8_t dataVal = (uint8_t)(xorshift32() & 0xFF);
  Wire.write(dataVal);
  
  Wire.endTransmission();
  totalPacketsSent++;
}

// ==========================================
// DISPATCH FUZZING TASK
// ==========================================
void executeFuzzingCycle() {
  if (!isFuzzingRunning) return;

  digitalWrite(kActiveLedPin, HIGH);

  switch (currentMode) {
    case MODE_UART_VALID: sendUartFuzzPacket(0); break;
    case MODE_UART_EMPTY: sendUartFuzzPacket(1); break;
    case MODE_UART_MAX: sendUartFuzzPacket(2); break;
    case MODE_UART_OVERLENGTH: sendUartFuzzPacket(3); break;
    case MODE_UART_BAD_CRC: sendUartFuzzPacket(4); break;
    case MODE_UART_TRUNCATED: sendUartFuzzPacket(5); break;
    case MODE_UART_RANDOM: sendUartFuzzPacket(6); break;
    case MODE_SPI_FUZZ: sendSpiFuzzPacket(); break;
    case MODE_I2C_FUZZ: sendI2cFuzzPacket(); break;
    case MODE_AI_ADAPTIVE: sendUartFuzzPacket(selectAiMutation()); break;
    default: break;
  }

  delay(5);
  digitalWrite(kActiveLedPin, LOW);
}

// ==========================================
// SETUP & HARDWARE INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize Peripheral Interfaces
  Serial2.begin(115200, SERIAL_8N1, kFuzzerRxPin, kFuzzerTxPin);
  
  pinMode(kSpiCs, OUTPUT);
  digitalWrite(kSpiCs, HIGH);
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi, kSpiCs);

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

  // Audio/Visual Self-Test
  digitalWrite(kPassLedPin, HIGH);
  digitalWrite(kActiveLedPin, HIGH);
  digitalWrite(kBuzzerPin, HIGH);
  delay(150);
  digitalWrite(kBuzzerPin, LOW);
  digitalWrite(kPassLedPin, LOW);
  digitalWrite(kActiveLedPin, LOW);

  lastHeartbeatEdge = millis();
  updateOledUI();
  Serial.println("--- AUTOFUZZER V3.0 COMPLETE FIRMWARE READY ---");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  static uint32_t lastPacketTime = 0;
  static uint32_t lastUiTime = 0;
  static uint32_t lastBtnCheck = 0;

  // 1. Monitor Target Heartbeat
  bool hbCurrent = digitalRead(kHeartbeatPin);
  if (hbCurrent != lastHeartbeatState) {
    lastHeartbeatState = hbCurrent;
    lastHeartbeatEdge = millis();
  }

  if (millis() - lastHeartbeatEdge > 350) {
    if (dutAlive) {
      dutAlive = false;
      totalCrashes++;
      recordAiCrashReward(); // AI Learning Loop Reward
      digitalWrite(kPassLedPin, LOW);
      digitalWrite(kFailLedPin, HIGH);
      digitalWrite(kBuzzerPin, HIGH);
      delay(200);
      digitalWrite(kBuzzerPin, LOW);
      Serial.printf("ALERT: Target Heartbeat Timeout! Crash #%u\n", totalCrashes);
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

  // 2. Button Controls
  if (millis() - lastBtnCheck > 150) {
    if (digitalRead(kStartBtnPin) == LOW) {
      lastBtnCheck = millis();
      isFuzzingRunning = !isFuzzingRunning;
      Serial.printf("Btn 1: Fuzzing %s\n", isFuzzingRunning ? "RUNNING" : "PAUSED");
      updateOledUI();
    }

    if (digitalRead(kModeBtnPin) == LOW) {
      lastBtnCheck = millis();
      currentMode = (ModeType)((currentMode + 1) % MODE_COUNT);
      Serial.printf("Btn 2: Mode -> %s\n", modeNames[currentMode]);
      updateOledUI();
    }

    if (digitalRead(kResetBtnPin) == LOW) {
      lastBtnCheck = millis();
      totalPacketsSent = 0;
      totalCrashes = 0;
      currentSequence = 0;
      currentSeed = 0xC0DEC0DE;
      for (int i = 0; i < 7; i++) aiMutationCrashWeights[i] = 1;
      aiTotalWeight = 7;
      Serial.println("Btn 3: Reset Stats & AI Weights!");
      updateOledUI();
    }
  }

  // 3. Packet Transmission Interval
  if (isFuzzingRunning && (millis() - lastPacketTime > 100)) {
    lastPacketTime = millis();
    executeFuzzingCycle();
  }

  // 4. OLED Refresh
  if (millis() - lastUiTime > 500) {
    lastUiTime = millis();
    updateOledUI();
  }
}
