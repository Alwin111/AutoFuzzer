#include "uart_fuzzer.h"
#include "../core/prng.h"
#include "../core/testcase.h"
#include "../core/campaign.h"

// ============================================
// Internal State
// ============================================
static uint8_t s_lastPacket[64];
static uint8_t s_lastPacketLen = 0;
static uint16_t s_sequence = 0;

// ============================================
// Init
// ============================================
void uart_fuzzer_init(void) {
  s_sequence = 0;
  s_lastPacketLen = 0;
  memset(s_lastPacket, 0, sizeof(s_lastPacket));

  // Initialize hardware UART for DUT communication
  // TX=GPIO17, RX=GPIO16 (ESP32 Serial2 defaults)
  Serial2.begin(9600);
  delay(50);  // Let UART settle
}

// ============================================
// Build Packet
//
// Constructs a complete AutoFuzzer UART packet
// with the specified mutation applied.
//
// Returns total bytes written to buffer.
// ============================================
static uint8_t build_packet(uint8_t* buffer, uint8_t bufSize, MutationType mutation) {
  uint8_t sync = Proto::UartSync;
  uint8_t cmd  = Proto::UartCmd;
  uint8_t payloadLen = 8;  // Default valid payload

  // Determine payload length based on mutation
  switch (mutation) {
    case MUT_VALID:      payloadLen = 8; break;
    case MUT_EMPTY:      payloadLen = 0; break;
    case MUT_MAX_LEN:    payloadLen = Proto::UartMaxPayload; break;
    case MUT_OVERLENGTH: payloadLen = 45; break;  // Exceeds max
    case MUT_BAD_CRC:    payloadLen = 8; break;
    case MUT_TRUNCATED:  payloadLen = 12; break;
    case MUT_RANDOM:     payloadLen = (uint8_t)(prng_range(32) + 1); break;
    default:             payloadLen = 8; break;
  }

  // Header
  buffer[0] = sync;
  buffer[1] = cmd;
  buffer[2] = payloadLen;
  buffer[3] = s_sequence & 0xFF;
  buffer[4] = (s_sequence >> 8) & 0xFF;

  // Calculate running checksum
  uint8_t checksum = cmd ^ payloadLen ^ buffer[3] ^ buffer[4];

  // Payload
  for (uint8_t i = 0; i < payloadLen; i++) {
    uint8_t b = prng_byte();
    if (5 + i < bufSize) {
      buffer[5 + i] = b;
      checksum ^= b;
    }
  }

  // Apply CRC mutation
  if (mutation == MUT_BAD_CRC) {
    checksum ^= 0xFF;  // Invert checksum
  }

  // Checksum byte
  uint8_t totalBytes = 5 + payloadLen;
  if (totalBytes < bufSize) {
    buffer[totalBytes] = checksum;
    totalBytes++;
  }

  // Truncation: send only half the bytes
  if (mutation == MUT_TRUNCATED) {
    totalBytes /= 2;
  }

  // Overlength: declare length > 32 but still send the header
  // The payload length field says 45, but we generate 45 random payload bytes
  if (mutation == MUT_OVERLENGTH) {
    // Rebuild with actual 45-byte payload
    totalBytes = 5 + 45;
    if (totalBytes >= bufSize) totalBytes = bufSize - 1;

    // Regenerate payload (need to restart from the sync point)
    checksum = cmd ^ 45 ^ buffer[3] ^ buffer[4];
    for (uint8_t i = 5; i < totalBytes; i++) {
      uint8_t b = prng_byte();
      buffer[i] = b;
      checksum ^= b;
    }
    buffer[totalBytes] = checksum;
    totalBytes++;
  }

  return totalBytes;
}

// ============================================
// Send Fuzzed Packet
// ============================================
bool uart_fuzzer_send(MutationType mutation) {
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Capture testcase metadata BEFORE generating packet
  testcase_prepare(s_sequence, mutation);

  // Build the packet
  uint8_t totalLen = build_packet(buffer, sizeof(buffer), mutation);

  if (totalLen == 0 || totalLen > sizeof(buffer)) return false;

  // Store for replay reference
  memcpy(s_lastPacket, buffer, totalLen);
  s_lastPacketLen = totalLen;

  // Finalize testcase with actual packet bytes
  testcase_finalize(totalLen, buffer[2], buffer);

  // Transmit
  Serial2.write(buffer, totalLen);

  // Update sequence and campaign counters
  s_sequence++;
  campaign_increment_packet_count();

  return true;
}

// ============================================
// Replay Exact Packet
// ============================================
bool uart_fuzzer_replay(const TestcaseMeta* testcase) {
  if (!testcase || testcase->packetLen == 0) return false;

  // Restore PRNG state for deterministic payload regeneration
  prng_set_state(testcase->prngStateBefore);

  // Rebuild the packet from stored metadata
  uint8_t buffer[64];
  uint8_t totalLen = build_packet(buffer, sizeof(buffer), testcase->mutation);

  // Override sequence number to match original
  buffer[3] = testcase->sequence & 0xFF;
  buffer[4] = (testcase->sequence >> 8) & 0xFF;

  // If the rebuilt packet doesn't match, use stored bytes directly
  if (totalLen != testcase->packetLen) {
    memcpy(buffer, testcase->packetBytes, testcase->packetLen);
    totalLen = testcase->packetLen;
  }

  // Transmit the exact same packet
  Serial2.write(buffer, totalLen);

  return true;
}

// ============================================
// Get Last Packet
// ============================================
const uint8_t* uart_fuzzer_get_last_packet(uint8_t* outLen) {
  if (outLen) *outLen = s_lastPacketLen;
  return s_lastPacket;
}
