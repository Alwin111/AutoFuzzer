#include <Arduino.h>

namespace {
constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kAckSync = 0x5A;
constexpr uint8_t kMaxPayload = 32;
constexpr uint8_t kHeartbeatPin = PB5;
constexpr uint32_t kPacketTimeoutMs = 30;

HardwareSerial dutUart(PA10, PA9);  // RX, TX

enum class ParserState : uint8_t { WaitSync, Command, Length, SeqLow, SeqHigh, Payload, Checksum };
ParserState state = ParserState::WaitSync;
uint8_t commandByte = 0;
uint8_t lengthByte = 0;
uint8_t payload[kMaxPayload];
uint8_t payloadPosition = 0;
uint16_t sequenceNumber = 0;
uint8_t runningChecksum = 0;
uint32_t lastParserByteAt = 0;
uint32_t lastHeartbeatAt = 0;

uint8_t xorByte(uint8_t current, uint8_t byte) { return current ^ byte; }

void resetParser() {
  state = ParserState::WaitSync;
  payloadPosition = 0;
  runningChecksum = 0;
}

void sendReply(uint8_t status) {
  uint8_t reply[] = {kAckSync, static_cast<uint8_t>(sequenceNumber),
                     static_cast<uint8_t>(sequenceNumber >> 8), status};
  dutUart.write(reply, sizeof(reply));
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
      sequenceNumber |= static_cast<uint16_t>(byte) << 8;
      runningChecksum = xorByte(runningChecksum, byte);
      payloadPosition = 0;
      state = lengthByte == 0 ? ParserState::Checksum : ParserState::Payload;
      break;
    case ParserState::Payload:
      payload[payloadPosition++] = byte;
      runningChecksum = xorByte(runningChecksum, byte);
      if (payloadPosition == lengthByte) state = ParserState::Checksum;
      break;
    case ParserState::Checksum:
      sendReply(byte == runningChecksum ? 0x00 : 0x03);
      resetParser();
      break;
  }
}
}  // namespace

void setup() {
  pinMode(kHeartbeatPin, OUTPUT);
  digitalWrite(kHeartbeatPin, LOW);
  Serial.begin(115200);                 // ST-Link virtual COM port / debug.
  dutUart.begin(115200);                // USART1: PA9 TX, PA10 RX.
  Serial.println("AutoFuzzer DUT: robust UART parser ready");
}

void loop() {
  if (millis() - lastHeartbeatAt >= 100) {
    digitalWrite(kHeartbeatPin, !digitalRead(kHeartbeatPin));
    lastHeartbeatAt = millis();
  }
  while (dutUart.available()) consumeByte(static_cast<uint8_t>(dutUart.read()));
  if (state != ParserState::WaitSync && millis() - lastParserByteAt > kPacketTimeoutMs) {
    resetParser();  // A truncated frame must never leave the parser wedged.
  }
}
