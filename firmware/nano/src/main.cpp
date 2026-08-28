// ============================================
// AutoFuzzer DUT — Arduino Nano (ATmega328P)
//
// Pin mapping:
//   D0/D1 (HardwareSerial) -> USB debug (CH340)
//   D2 (SoftwareSerial RX) <- ESP32 TX (GPIO17)
//   D3 (SoftwareSerial TX) -> ESP32 RX (GPIO16)
//   D4                     -> Heartbeat output (to ESP32 GPIO25)
//
// HardwareSerial = USB debug (115200 baud)
// SoftwareSerial = ESP32 communication (115200 baud)
// ============================================

#include <Arduino.h>
#include <SoftwareSerial.h>

constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kAckSync = 0x5A;
constexpr uint8_t kMaxPayload = 32;
constexpr uint8_t kHeartbeatPin = 4;       // D4
constexpr uint8_t kEspRxPin = 3;           // D3 <- ESP32 GPIO17 (RX)
constexpr uint8_t kEspTxPin = 2;           // D2 -> ESP32 GPIO16 (TX)
constexpr uint32_t kPacketTimeoutMs = 30;

SoftwareSerial espSerial(kEspRxPin, kEspTxPin);

enum class ParserState : uint8_t { WaitSync, Command, Length, SeqLow, SeqHigh, Payload, Checksum };
ParserState state = ParserState::WaitSync;
uint8_t commandByte = 0;
uint8_t lengthByte = 0;
uint8_t payload[32];
uint8_t payloadPosition = 0;
uint16_t sequenceNumber = 0;
uint8_t runningChecksum = 0;
uint32_t lastParserByteAt = 0;
uint32_t lastHeartbeatAt = 0;

uint8_t xorByte(uint8_t current, uint8_t b) { return current ^ b; }

void resetParser() {
  state = ParserState::WaitSync;
  payloadPosition = 0;
  runningChecksum = 0;
}

void sendReply(uint8_t status) {
  uint8_t reply[] = {kAckSync,
                     (uint8_t)(sequenceNumber & 0xFF),
                     (uint8_t)(sequenceNumber >> 8),
                     status};
  espSerial.write(reply, 4);
}

void consumeByte(uint8_t byte) {
  lastParserByteAt = millis();
  switch (state) {
    case ParserState::WaitSync:
      if (byte == kSync) state = ParserState::Command;
      break;
    case ParserState::Command:
      commandByte = byte;
      runningChecksum = byte;
      state = ParserState::Length;
      break;
    case ParserState::Length:
      lengthByte = byte;
      runningChecksum = xorByte(runningChecksum, byte);
      if (lengthByte > kMaxPayload) {
        Serial.print(F("DUT: Overlength len=")); Serial.println(lengthByte);
        sendReply(0x02);
        resetParser();
      } else {
        state = ParserState::SeqLow;
      }
      break;
    case ParserState::SeqLow:
      sequenceNumber = byte;
      runningChecksum = xorByte(runningChecksum, byte);
      state = ParserState::SeqHigh;
      break;
    case ParserState::SeqHigh:
      sequenceNumber |= (uint16_t)byte << 8;
      runningChecksum = xorByte(runningChecksum, byte);
      payloadPosition = 0;
      state = (lengthByte == 0) ? ParserState::Checksum : ParserState::Payload;
      break;
    case ParserState::Payload:
      payload[payloadPosition++] = byte;
      runningChecksum = xorByte(runningChecksum, byte);
      if (payloadPosition == lengthByte) state = ParserState::Checksum;
      break;
    case ParserState::Checksum:
      if (byte == runningChecksum) {
        Serial.print(F("DUT: OK seq=")); Serial.println(sequenceNumber);
        sendReply(0x00);
      } else {
        Serial.print(F("DUT: BAD_CRC seq=")); Serial.println(sequenceNumber);
        sendReply(0x03);
      }
      resetParser();
      break;
  }
}

void setup() {
  pinMode(kHeartbeatPin, OUTPUT);
  digitalWrite(kHeartbeatPin, LOW);

  // HardwareSerial = USB debug
  Serial.begin(115200);
  Serial.println(F("AutoFuzzer DUT Nano v4.0 ready"));
  Serial.println(F("  D3 (SoftRX) <- ESP32 TX GPIO17"));
  Serial.println(F("  D2 (SoftTX) -> ESP32 RX GPIO16"));
  Serial.println(F("  D4          -> Heartbeat to ESP32 GPIO25"));

  // SoftwareSerial = ESP32 communication (9600 baud — reliable for ATmega328P SoftwareSerial)
  espSerial.begin(9600);
}

void loop() {
  // Heartbeat: toggle every 100ms
  if (millis() - lastHeartbeatAt >= 100) {
    digitalWrite(kHeartbeatPin, !digitalRead(kHeartbeatPin));
    lastHeartbeatAt = millis();
  }

  // Process incoming bytes from ESP32
  while (espSerial.available()) {
    consumeByte(espSerial.read());
  }

  // Parser timeout
  if (state != ParserState::WaitSync && millis() - lastParserByteAt > kPacketTimeoutMs) {
    resetParser();
  }
}
