#include "uart_parser.h"

// ============================================
// Parser State Machine
// ============================================
enum ParserState : uint8_t {
  PARSE_WAIT_SYNC = 0,
  PARSE_SEQ_LO,
  PARSE_SEQ_HI,
  PARSE_STATUS
};

static ParserState s_state = PARSE_WAIT_SYNC;
static UartResponse s_response;
static bool s_hasResponse = false;
static uint8_t s_respBuffer[4];
static uint8_t s_respIdx = 0;
static uint32_t s_lastByteMs = 0;
static uint16_t s_expectedSeq = 0;

static const uint32_t kRespByteTimeoutMs = 50;

// ============================================
// Init
// ============================================
void uart_parser_init(void) {
  s_state = PARSE_WAIT_SYNC;
  s_hasResponse = false;
  s_respIdx = 0;
  s_expectedSeq = 0;
  memset(&s_response, 0, sizeof(UartResponse));
  memset(s_respBuffer, 0, sizeof(s_respBuffer));
}

// ============================================
// Poll — read available bytes from Serial2
// ============================================
bool uart_parser_poll(void) {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    s_lastByteMs = millis();

    switch (s_state) {
      case PARSE_WAIT_SYNC:
        if (b == Proto::UartAckSync) {
          s_respBuffer[0] = b;
          s_respIdx = 1;
          s_state = PARSE_SEQ_LO;
        }
        break;

      case PARSE_SEQ_LO:
        s_respBuffer[1] = b;
        s_respIdx = 2;
        s_state = PARSE_SEQ_HI;
        break;

      case PARSE_SEQ_HI:
        s_respBuffer[2] = b;
        s_respIdx = 3;
        s_state = PARSE_STATUS;
        break;

      case PARSE_STATUS:
        s_respBuffer[3] = b;
        s_respIdx = 4;

        // Complete response received
        s_response.received   = true;
        s_response.syncByte   = s_respBuffer[0];
        s_response.sequence   = s_respBuffer[1] | ((uint16_t)s_respBuffer[2] << 8);
        s_response.status     = s_respBuffer[3];
        s_response.recvTimeMs = millis();

        s_hasResponse = true;
        s_state = PARSE_WAIT_SYNC;
        s_respIdx = 0;

        return true;
    }
  }

  // Byte timeout — if we're mid-parse and no byte arrives, reset
  if (s_state != PARSE_WAIT_SYNC && s_respIdx > 0) {
    if (millis() - s_lastByteMs > kRespByteTimeoutMs) {
      // Partial response — discard
      s_state = PARSE_WAIT_SYNC;
      s_respIdx = 0;
    }
  }

  return false;
}

// ============================================
// Getters
// ============================================
const UartResponse* uart_parser_get_response(void) {
  return &s_response;
}

bool uart_parser_has_response(void) {
  return s_hasResponse;
}

void uart_parser_clear_response(void) {
  s_hasResponse = false;
  memset(&s_response, 0, sizeof(UartResponse));
}

uint16_t uart_parser_get_expected_seq(void) {
  return s_expectedSeq;
}
