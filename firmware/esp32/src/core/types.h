#pragma once
#include <Arduino.h>

// ============================================
// VERSION
// ============================================
#define AUTOFUZZER_VERSION "4.0"

// ============================================
// PIN CONFIGURATION
// ============================================
namespace Pin {
  // OLED (I2C)
  constexpr int OledSda   = 21;
  constexpr int OledScl   = 22;
  constexpr int OledAddr  = 0x3C;

  // UART to DUT
  constexpr int UartTx    = 17;  // ESP32 TX2 -> Target RX
  constexpr int UartRx    = 16;  // ESP32 RX2 <- Target TX

  // Heartbeat from DUT
  constexpr int Heartbeat = 25;  // Input from STM32 PB5

  // SPI
  constexpr int SpiSck    = 18;
  constexpr int SpiMiso   = 19;
  constexpr int SpiMosi   = 23;
  constexpr int SpiCs     = 5;

  // I2C (shared with OLED — note bus contention when fuzzing I2C)
  constexpr int I2cSda    = 21;
  constexpr int I2cScl    = 22;

  // LEDs
  constexpr int LedPass   = 2;   // Green — DUT healthy
  constexpr int LedFail   = 4;   // Red — failure detected
  constexpr int LedActive = 15;  // Blue — packet transmitting

  // Buzzer
  constexpr int Buzzer    = 32;

  // Buttons (active LOW, internal pull-up)
  constexpr int BtnUp     = 12;  // Up / Next menu item
  constexpr int BtnDown   = 13;  // Down / Previous menu item
  constexpr int BtnBack   = 14;  // Back / Cancel / Reset
  constexpr int BtnSelect = 27;  // Select / Confirm / Start
}

// ============================================
// PWM CHANNELS
// ============================================
namespace Pwm {
  constexpr int LedFreq    = 5000;   // 5 kHz
  constexpr int LedRes     = 8;      // 8-bit
  constexpr int BuzzerFreq = 2000;   // 2 kHz
  constexpr int BuzzerRes  = 8;
  constexpr uint32_t LedOn   = 127;  // ~50% duty
  constexpr uint32_t LedOff  = 0;
  constexpr uint32_t BuzzOn  = 127;
  constexpr uint32_t BuzzOff = 0;

  // Channel assignments
  constexpr int ChPassLed   = 0;
  constexpr int ChFailLed   = 1;
  constexpr int ChActiveLed = 2;
  constexpr int ChBuzzer    = 3;
}

// ============================================
// DISPLAY
// ============================================
namespace Display {
  constexpr int Width  = 128;
  constexpr int Height = 64;
}

// ============================================
// PROTOCOL CONSTANTS
// ============================================
namespace Proto {
  constexpr uint8_t UartSync    = 0xA5;
  constexpr uint8_t UartAckSync = 0x5A;
  constexpr uint8_t UartCmd     = 0x01;
  constexpr uint8_t UartMaxPayload = 32;
  constexpr uint8_t UartHeaderSize  = 5;  // SYNC + CMD + LEN + SEQ_LO + SEQ_HI
  constexpr uint8_t UartFooterSize  = 1;  // CHECKSUM
  constexpr uint8_t UartFrameOverhead = Proto::UartHeaderSize + Proto::UartFooterSize;

  // DUT response status codes
  constexpr uint8_t StatusAck          = 0x00;
  constexpr uint8_t StatusNackOverlen  = 0x02;
  constexpr uint8_t StatusNackChecksum = 0x03;

  // DUT response frame size
  constexpr uint8_t UartRespSize = 4;  // ACK_SYNC + SEQ_LO + SEQ_HI + STATUS

  // Timing
  constexpr uint32_t HeartbeatTimeoutMs = 350;
  constexpr uint32_t UartRespTimeoutMs  = 100;
  constexpr uint32_t PacketIntervalMs   = 100; // Minimum time between packets (increased for 9600 baud DUT link)
}

// ============================================
// MUTATION TYPES
// ============================================
enum MutationType : uint8_t {
  MUT_VALID = 0,
  MUT_EMPTY,
  MUT_MAX_LEN,
  MUT_OVERLENGTH,
  MUT_BAD_CRC,
  MUT_TRUNCATED,
  MUT_RANDOM,
  MUT_COUNT
};

static const char* MutationNames[] = {
  "VALID", "EMPTY", "MAX_LEN", "OVERLENGTH",
  "BAD_CRC", "TRUNCATED", "RANDOM"
};

// ============================================
// PROTOCOL MODES
// ============================================
enum ProtocolMode : uint8_t {
  PROTO_UART = 0,
  PROTO_SPI,
  PROTO_I2C,
  PROTO_CAN,    // Future — requires transceiver
  PROTO_COUNT
};

static const char* ProtocolNames[] = {
  "UART", "SPI", "I2C", "CAN"
};

// ============================================
// TARGET BOARDS
// ============================================
enum TargetBoard : uint8_t {
  BOARD_STM32 = 0,
  BOARD_ESP32,
  BOARD_ARDUINO_NANO,
  BOARD_COUNT
};

static const char* BoardNames[] = {
  "STM32 Nucleo",
  "ESP32",
  "Arduino Nano"
};

// ============================================
// TEST PROFILES
// ============================================
enum TestProfile : uint8_t {
  TEST_QUICK    = 0,  // 30 seconds
  TEST_STANDARD,       // 60 seconds
  TEST_DEEP,           // 5 minutes (300s)
  TEST_CUSTOM,         // User-configured
  TEST_COUNT
};

static const char* TestProfileNames[] = {
  "QUICK 30s", "STANDARD 60s", "DEEP 5min", "CUSTOM"
};

static const uint32_t TestProfileDurations[] = {
  30000,   // Quick: 30 seconds
  60000,   // Standard: 60 seconds
  300000,  // Deep: 5 minutes
  0        // Custom: set by user
};

// ============================================
// CAMPAIGN PHASES
// ============================================
enum CampaignPhase : uint8_t {
  PHASE_BASELINE = 0,   // 0-5s: valid packets, establish baseline
  PHASE_BOUNDARY,       // 5-10s: empty/max length
  PHASE_OVERLENGTH,     // 10-15s: oversized payloads
  PHASE_CHECKSUM,       // 15-20s: bad CRC mutations
  PHASE_MALFORMED,      // 20-25s: truncated, partial
  PHASE_RANDOM,         // 25-30s: random/adaptive
  PHASE_FREEFORM,       // Beyond 30s: any mutation
  PHASE_DONE
};

// ============================================
// UI SCREENS
// ============================================
enum UiScreen : uint8_t {
  SCREEN_MAIN_MENU = 0,
  SCREEN_BOARD_SELECT,
  SCREEN_PROTOCOL_SELECT,
  SCREEN_WIRING_DIAGRAM,
  SCREEN_TEST_SELECT,
  SCREEN_TEST_CONFIRM,
  SCREEN_PROTOCOL_CHECK,
  SCREEN_FUZZING,
  SCREEN_FAILURE_MENU,
  SCREEN_REPLAY,
  SCREEN_MINIMIZING,
  SCREEN_RESULT,
  SCREEN_FAILURE_DETAIL,
  SCREEN_SETTINGS,
  SCREEN_ABOUT
};

// ============================================
// FAILURE CLASSIFICATION
// ============================================
enum FailureType : uint8_t {
  FAIL_NONE = 0,
  FAIL_HEARTBEAT_TIMEOUT,
  FAIL_NO_RESPONSE,
  FAIL_COMMUNICATION,
  FAIL_PARSER_TIMEOUT,
  FAIL_PROTOCOL_ERROR,
  FAIL_WATCHDOG_RESET,
  FAIL_BUS_ERROR,
  FAIL_POWER_FAILURE,
  FAIL_UNKNOWN
};

static const char* FailureTypeNames[] = {
  "NONE", "HEARTBEAT_TIMEOUT", "NO_RESPONSE",
  "COMM_FAILURE", "PARSER_TIMEOUT", "PROTOCOL_ERROR",
  "WATCHDOG_RESET", "BUS_ERROR", "POWER_FAILURE", "UNKNOWN"
};

// ============================================
// FAILURE STATUS
// ============================================
enum FailureStatus : uint8_t {
  STATUS_SUSPECTED = 0,
  STATUS_REPRODUCIBLE,
  STATUS_INTERMITTENT,
  STATUS_NOT_REPRODUCIBLE
};

static const char* FailureStatusNames[] = {
  "SUSPECTED", "REPRODUCIBLE", "INTERMITTENT", "NOT_REPRODUCIBLE"
};

// ============================================
// TEST RESULT
// ============================================
enum TestResult : uint8_t {
  RESULT_NONE = 0,
  RESULT_PASS,
  RESULT_FAIL,
  RESULT_INCONCLUSIVE
};

// ============================================
// BUTTON ACTIONS
// ============================================
enum ButtonAction : uint8_t {
  BTN_NONE = 0,
  BTN_UP,        // Navigate up / next item
  BTN_DOWN,      // Navigate down / previous item
  BTN_SELECT,    // Confirm / Start / Select
  BTN_BACK       // Back / Cancel / Reset
};

// ============================================
// REPLAY STATE
// ============================================
enum ReplayState : uint8_t {
  REPLAY_IDLE = 0,
  REPLAY_RUNNING,
  REPLAY_COMPLETE
};

// ============================================
// MINIMIZER STATE
// ============================================
enum MinimizerState : uint8_t {
  MINIM_IDLE = 0,
  MINIM_RUNNING,
  MINIM_COMPLETE
};

// ============================================
// TESTCASE METADATA
// ============================================
struct TestcaseMeta {
  uint32_t campaignSeed;       // Seed at campaign start
  uint32_t packetSeed;         // Seed before this packet was generated
  uint32_t prngStateBefore;    // PRNG internal state before packet
  uint32_t prngStateAfter;     // PRNG internal state after packet
  uint16_t sequence;           // Packet sequence number
  MutationType mutation;       // Which mutation was applied
  uint8_t packetLen;           // Total bytes transmitted
  uint8_t payloadLen;          // Payload length field value
  uint8_t packetBytes[64];     // Full packet bytes
  uint32_t timestampMs;        // When packet was sent
};

// ============================================
// FAILURE RECORD
// ============================================
struct FailureRecord {
  uint8_t id;                  // Failure index
  FailureType type;            // Classification
  FailureStatus status;        // Reproducibility status
  ProtocolMode protocol;       // Which protocol
  TestProfile profile;         // Test profile in use
  TestcaseMeta testcase;       // The failing testcase
  uint32_t heartbeatLostMs;    // How long since last heartbeat edge
  uint32_t dutRespTimeMs;      // Response time if received
  uint8_t dutResponseStatus;   // ACK/NACK code if received
  bool dutResponded;           // Whether DUT sent any response
  uint32_t packetCountAtFail;  // Total packets when failure occurred
  uint32_t campaignElapsedMs;  // Time into campaign when failure occurred
  uint8_t replayAttempts;      // Number of replay attempts
  uint8_t replayFails;         // How many replays failed
  uint32_t minimizedLen;       // Minimized packet length (0 = not minimized)
};

// ============================================
// CAMPAIGN STATISTICS
// ============================================
struct CampaignStats {
  uint32_t totalPackets;
  uint32_t totalAcks;
  uint32_t totalNacks;
  uint32_t totalNoResponse;
  uint32_t totalHeartbeatTimeouts;
  uint32_t totalFailures;
  uint32_t uniqueFailures;
  uint32_t elapsedMs;
  uint32_t durationMs;
  uint8_t mutationCounts[MUT_COUNT];  // Packets per mutation type
};

// ============================================
// UART RESPONSE
// ============================================
struct UartResponse {
  bool received;
  uint8_t syncByte;
  uint16_t sequence;
  uint8_t status;
  uint32_t recvTimeMs;
};
