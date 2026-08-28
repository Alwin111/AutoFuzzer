// ============================================
// AutoFuzzer DUT — Arduino Nano (ATmega328P)
//
// Supports UART and I2C protocols simultaneously.
//
// Pin mapping:
//   D0/D1 (HardwareSerial) -> USB debug (CH340)
//   D2 (SoftwareSerial RX) <- ESP32 TX (GPIO17)  [UART mode]
//   D3 (SoftwareSerial TX) -> ESP32 RX (GPIO16)  [UART mode]
//   D4                     -> Heartbeat output (to ESP32 GPIO25)
//   A4 (SDA)               <-> I2C bus            [I2C mode]
//   A5 (SCL)               <-> I2C bus            [I2C mode]
//
// I2C slave address: 0x10
// I2C register map: 16 registers (0x00-0x0F)
//
// The DUT auto-detects which protocol is active
// based on incoming traffic on each bus.
// ============================================

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>

// ============================================
// UART protocol constants
// ============================================
constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kAckSync = 0x5A;
constexpr uint8_t kMaxPayload = 32;
constexpr uint32_t kPacketTimeoutMs = 30;

// ============================================
// Pin definitions
// ============================================
constexpr uint8_t kHeartbeatPin = 4;       // D4
constexpr uint8_t kEspRxPin = 3;           // D3 <- ESP32 GPIO17 (RX)
constexpr uint8_t kEspTxPin = 2;           // D2 -> ESP32 GPIO16 (TX)

// ============================================
// I2C slave address and register map
// ============================================
constexpr uint8_t kI2cSlaveAddr = 0x10;
constexpr uint8_t kRegisterCount = 16;

// ============================================
// SoftwareSerial for UART communication
// ============================================
SoftwareSerial espSerial(kEspRxPin, kEspTxPin);

// ============================================
// UART parser state machine
// ============================================
enum class UartParserState : uint8_t {
  WaitSync, Command, Length, SeqLow, SeqHigh, Payload, Checksum
};
static UartParserState uartState = UartParserState::WaitSync;
static uint8_t uartCommandByte = 0;
static uint8_t uartLengthByte = 0;
static uint8_t uartPayload[32];
static uint8_t uartPayloadPos = 0;
static uint16_t uartSequenceNum = 0;
static uint8_t uartRunningChecksum = 0;
static uint32_t uartLastByteAt = 0;

// ============================================
// I2C register map (16 bytes)
// ============================================
static uint8_t i2cRegisters[kRegisterCount];
static uint8_t i2cActiveRegister = 0;
static uint32_t i2cReceiveCount = 0;  // Total I2C writes received
static uint32_t i2cRequestCount = 0;  // Total I2C reads served
static volatile bool i2cNewData = false;

// ============================================
// Heartbeat
// ============================================
static uint32_t lastHeartbeatAt = 0;

// ============================================
// Protocol activity flags
// ============================================
static bool uartActive = false;
static bool i2cActive = false;

// ============================================
// Helper functions
// ============================================
static uint8_t xorByte(uint8_t current, uint8_t b) { return current ^ b; }

static void resetUartParser() {
  uartState = UartParserState::WaitSync;
  uartPayloadPos = 0;
  uartRunningChecksum = 0;
}

// ============================================
// UART reply (ACK/NACK)
// ============================================
static void uartSendReply(uint8_t status) {
  uint8_t reply[] = {kAckSync,
                     (uint8_t)(uartSequenceNum & 0xFF),
                     (uint8_t)(uartSequenceNum >> 8),
                     status};
  espSerial.write(reply, 4);
}

// ============================================
// UART byte parser
// ============================================
static void uartConsumeByte(uint8_t byte) {
  uartLastByteAt = millis();
  switch (uartState) {
    case UartParserState::WaitSync:
      if (byte == kSync) uartState = UartParserState::Command;
      break;
    case UartParserState::Command:
      uartCommandByte = byte;
      uartRunningChecksum = byte;
      uartState = UartParserState::Length;
      break;
    case UartParserState::Length:
      uartLengthByte = byte;
      uartRunningChecksum = xorByte(uartRunningChecksum, byte);
      if (uartLengthByte > kMaxPayload) {
        Serial.print(F("DUT UART: Overlength len="));
        Serial.println(uartLengthByte);
        uartSendReply(0x02);  // NACK overlength
        resetUartParser();
      } else {
        uartState = UartParserState::SeqLow;
      }
      break;
    case UartParserState::SeqLow:
      uartSequenceNum = byte;
      uartRunningChecksum = xorByte(uartRunningChecksum, byte);
      uartState = UartParserState::SeqHigh;
      break;
    case UartParserState::SeqHigh:
      uartSequenceNum |= (uint16_t)byte << 8;
      uartRunningChecksum = xorByte(uartRunningChecksum, byte);
      uartPayloadPos = 0;
      uartState = (uartLengthByte == 0) ? UartParserState::Checksum : UartParserState::Payload;
      break;
    case UartParserState::Payload:
      uartPayload[uartPayloadPos++] = byte;
      uartRunningChecksum = xorByte(uartRunningChecksum, byte);
      if (uartPayloadPos == uartLengthByte) uartState = UartParserState::Checksum;
      break;
    case UartParserState::Checksum:
      if (byte == uartRunningChecksum) {
        Serial.print(F("DUT UART: OK seq="));
        Serial.println(uartSequenceNum);
        uartSendReply(0x00);  // ACK
      } else {
        Serial.print(F("DUT UART: BAD_CRC seq="));
        Serial.println(uartSequenceNum);
        uartSendReply(0x03);  // NACK checksum
      }
      resetUartParser();
      break;
  }
}

// ============================================
// I2C receive handler (master writes to us)
// ============================================
static void i2cOnReceive(int numBytes) {
  i2cReceiveCount++;
  i2cActive = true;

  if (numBytes < 1) return;

  // First byte = register address
  i2cActiveRegister = Wire.read();
  i2cActiveRegister %= kRegisterCount;  // Bounds check

  // Store remaining bytes into registers
  uint8_t regIdx = i2cActiveRegister;
  while (Wire.available() && regIdx < kRegisterCount) {
    i2cRegisters[regIdx] = Wire.read();
    regIdx++;
    i2cReceiveCount++;
  }

  i2cNewData = true;

  // Debug output (only every 50th to avoid serial flooding)
  if (i2cReceiveCount % 50 == 0) {
    Serial.print(F("DUT I2C: rx="));
    Serial.print(i2cReceiveCount);
    Serial.print(F(" addr=0x"));
    Serial.print(kI2cSlaveAddr, HEX);
    Serial.print(F(" reg=0x"));
    Serial.println(i2cActiveRegister, HEX);
  }
}

// ============================================
// I2C request handler (master reads from us)
// ============================================
static void i2cOnRequest() {
  i2cRequestCount++;
  i2cActive = true;

  // Send current register and next few bytes
  uint8_t regIdx = i2cActiveRegister;
  uint8_t bytesToSend = 4;  // Send 4 bytes at a time
  if (regIdx + bytesToSend > kRegisterCount) {
    bytesToSend = kRegisterCount - regIdx;
  }

  for (uint8_t i = 0; i < bytesToSend; i++) {
    Wire.write(i2cRegisters[regIdx + i]);
  }
}

// ============================================
// Setup
// ============================================
void setup() {
  // Heartbeat pin
  pinMode(kHeartbeatPin, OUTPUT);
  digitalWrite(kHeartbeatPin, LOW);

  // HardwareSerial = USB debug
  Serial.begin(115200);
  Serial.println(F("AutoFuzzer DUT Nano v4.0 ready"));
  Serial.println(F("  UART: D3(SoftRX) <- ESP32 TX17, D2(SoftTX) -> ESP32 RX16"));
  Serial.println(F("  I2C:  A4(SDA) <-> ESP32 GPIO26"));
  Serial.println(F("  I2C:  A5(SCL) <-> ESP32 GPIO33"));
  Serial.println(F("  HB:   D4 -> ESP32 GPIO25"));

  // SoftwareSerial = ESP32 UART communication
  espSerial.begin(9600);

  // I2C slave initialization
  memset(i2cRegisters, 0, kRegisterCount);
  Wire.begin(kI2cSlaveAddr);  // Join I2C bus as slave
  Wire.onReceive(i2cOnReceive);
  Wire.onRequest(i2cOnRequest);

  Serial.print(F("I2C slave ready at address 0x"));
  Serial.println(kI2cSlaveAddr, HEX);
}

// ============================================
// Main loop
// ============================================
void loop() {
  // Heartbeat: toggle every 100ms
  if (millis() - lastHeartbeatAt >= 100) {
    digitalWrite(kHeartbeatPin, !digitalRead(kHeartbeatPin));
    lastHeartbeatAt = millis();
  }

  // Process UART bytes from ESP32
  while (espSerial.available()) {
    uartConsumeByte(espSerial.read());
    uartActive = true;
  }

  // UART parser timeout
  if (uartState != UartParserState::WaitSync &&
      millis() - uartLastByteAt > kPacketTimeoutMs) {
    resetUartParser();
  }

  // I2C register dump on serial (every 100 requests, non-blocking)
  if (i2cRequestCount > 0 && i2cRequestCount % 100 == 0) {
    Serial.print(F("DUT I2C: req="));
    Serial.print(i2cRequestCount);
    Serial.print(F(" regs=["));
    for (uint8_t i = 0; i < 4; i++) {
      Serial.print(i2cRegisters[i], HEX);
      if (i < 3) Serial.print(',');
    }
    Serial.println(F("]"));
  }
}
