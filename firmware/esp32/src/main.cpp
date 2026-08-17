#include <Arduino.h>

namespace {
constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kCommand = 0x01;
constexpr uint8_t kAckSync = 0x5A;
constexpr int kDutRx = 16;
constexpr int kDutTx = 17;
constexpr int kHeartbeatPin = 27;
constexpr uint32_t kBaud = 115200;
constexpr uint32_t kHeartbeatTimeoutMs = 350;
constexpr uint32_t kNormalIntervalMs = 150;
constexpr uint32_t kStressIntervalMs = 15;

HardwareSerial dutUart(2);
uint32_t rngState = 0xC0DEC0DE;
uint16_t sequenceNumber = 0;
uint32_t lastPacketAt = 0;
uint32_t lastHeartbeatAt = 0;
int lastHeartbeatLevel = HIGH;

enum class Mutation : uint8_t { Valid, Empty, Maximum, Overlength, BadChecksum, Truncated, Random };

uint32_t nextRandom() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

const char *mutationName(Mutation mutation) {
  switch (mutation) {
    case Mutation::Valid: return "valid";
    case Mutation::Empty: return "empty";
    case Mutation::Maximum: return "maximum";
    case Mutation::Overlength: return "overlength";
    case Mutation::BadChecksum: return "bad-checksum";
    case Mutation::Truncated: return "truncated";
    case Mutation::Random: return "random";
  }
  return "unknown";
}

uint8_t checksum(const uint8_t *data, size_t size) {
  uint8_t value = 0;
  for (size_t i = 0; i < size; ++i) value ^= data[i];
  return value;
}

void printHex(const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (data[i] < 16) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 != size) Serial.print(' ');
  }
}

void transmitCase(Mutation mutation) {
  uint8_t frame[40];
  size_t payloadLength = 4;
  uint8_t declaredLength = payloadLength;
  uint8_t command = kCommand;

  if (mutation == Mutation::Empty) payloadLength = declaredLength = 0;
  if (mutation == Mutation::Maximum) payloadLength = declaredLength = 32;
  if (mutation == Mutation::Overlength) payloadLength = 4, declaredLength = 36;
  if (mutation == Mutation::Random) {
    payloadLength = nextRandom() % 12;
    declaredLength = payloadLength;
    command = static_cast<uint8_t>(nextRandom());
  }

  frame[0] = kSync;
  frame[1] = command;
  frame[2] = declaredLength;
  frame[3] = static_cast<uint8_t>(sequenceNumber);
  frame[4] = static_cast<uint8_t>(sequenceNumber >> 8);
  for (size_t i = 0; i < payloadLength; ++i) frame[5 + i] = static_cast<uint8_t>(nextRandom());
  size_t fullSize = 6 + payloadLength;
  frame[5 + payloadLength] = checksum(&frame[1], 4 + payloadLength);
  if (mutation == Mutation::BadChecksum) frame[fullSize - 1] ^= 0xFF;
  size_t transmittedSize = (mutation == Mutation::Truncated) ? 5 + payloadLength : fullSize;

  dutUart.write(frame, transmittedSize);
  Serial.printf("TX seq=%u type=%s seed=%08lX bytes=", sequenceNumber, mutationName(mutation),
                static_cast<unsigned long>(rngState));
  printHex(frame, transmittedSize);
  Serial.println();
  ++sequenceNumber;
}

void readDutReplies() {
  static uint8_t reply[4];
  static size_t position = 0;
  while (dutUart.available()) {
    uint8_t byte = static_cast<uint8_t>(dutUart.read());
    if (position == 0 && byte != kAckSync) continue;
    reply[position++] = byte;
    if (position == sizeof(reply)) {
      uint16_t sequence = reply[1] | (static_cast<uint16_t>(reply[2]) << 8);
      Serial.printf("DUT seq=%u status=0x%02X\n", sequence, reply[3]);
      position = 0;
    }
  }
}

void monitorHeartbeat() {
  int level = digitalRead(kHeartbeatPin);
  if (level != lastHeartbeatLevel) {
    lastHeartbeatLevel = level;
    lastHeartbeatAt = millis();
  }
  if (millis() - lastHeartbeatAt > kHeartbeatTimeoutMs) {
    Serial.printf("FAIL heartbeat-timeout after %lu ms; pause and record last sequence=%u\n",
                  static_cast<unsigned long>(millis() - lastHeartbeatAt), sequenceNumber - 1);
    lastHeartbeatAt = millis();  // Rate-limit repeated failure messages.
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kHeartbeatPin, INPUT_PULLUP);
  lastHeartbeatLevel = digitalRead(kHeartbeatPin);
  lastHeartbeatAt = millis();
  dutUart.begin(kBaud, SERIAL_8N1, kDutRx, kDutTx);
  Serial.println("AutoFuzzer ESP32: UART baseline ready");
}

void loop() {
  monitorHeartbeat();
  readDutReplies();
  const Mutation testPlan[] = {Mutation::Valid, Mutation::Empty, Mutation::Maximum,
                               Mutation::Overlength, Mutation::BadChecksum,
                               Mutation::Truncated, Mutation::Random};
  uint32_t interval = kNormalIntervalMs;
  if (millis() - lastPacketAt >= interval) {
    transmitCase(testPlan[sequenceNumber % (sizeof(testPlan) / sizeof(testPlan[0]))]);
    lastPacketAt = millis();
  }
}

