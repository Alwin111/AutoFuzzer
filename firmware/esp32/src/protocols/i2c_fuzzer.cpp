#include "i2c_fuzzer.h"
#include "../core/prng.h"
#include "../core/campaign.h"
#include <Wire.h>

// ============================================
// I2C bus: uses Wire (GPIO 21/22, shared with OLED)
// OLED is disabled during I2C fuzzing to prevent
// display corruption, re-enabled when done.
// ============================================

// Response tracking
static I2cResponse s_lastResponse;
static bool        s_responseReady = false;

// Statistics
static uint32_t s_totalAcks  = 0;
static uint32_t s_totalNacks = 0;
static uint32_t s_totalErrors = 0;

// Track OLED state
static bool s_oledWasActive = false;

// ============================================
// OLED helper — called from main via function pointer
// We extern these so we can talk to the OLED module
// ============================================
extern void oled_ui_sleep(void);
extern void oled_ui_wake(void);

// ============================================
// Init — configure the fuzzing I2C bus
// ============================================
void i2c_fuzzer_init(void) {
  // Disable OLED to free the I2C bus for fuzzing
  oled_ui_sleep();
  s_oledWasActive = true;

  // Initialize Wire on GPIO 21/22 as master at 100kHz
  Wire.begin(Pin::I2cSda, Pin::I2cScl, 100000);
  
  memset(&s_lastResponse, 0, sizeof(s_lastResponse));
  s_responseReady = false;
  s_totalAcks = 0;
  s_totalNacks = 0;
  s_totalErrors = 0;
  Serial.printf("I2C fuzzer: bus on SDA=%d SCL=%d (shared w/ OLED, display OFF)\n", 
                Pin::I2cSda, Pin::I2cScl);
}

// ============================================
// Deinit — restore OLED after I2C fuzzing
// ============================================
void i2c_fuzzer_deinit(void) {
  Wire.end();
  if (s_oledWasActive) {
    oled_ui_wake();
    s_oledWasActive = false;
  }
  Serial.println("I2C fuzzer: bus released, OLED restored");
}

// ============================================
// Build a valid register+payload buffer
// Returns the number of bytes written to buf
// ============================================
static uint8_t build_payload(uint8_t* buf, uint8_t maxLen, MutationType mutation) {
  uint8_t len = 0;

  switch (mutation) {
    case MUT_VALID:
      // Normal register write: 2-4 data bytes
      len = (uint8_t)prng_range(3) + 2;  // 2-4 bytes
      if (len > maxLen) len = maxLen;
      buf[0] = (uint8_t)(prng_range(0x10));  // Register 0x00-0x0F
      for (uint8_t i = 1; i < len; i++) {
        buf[i] = prng_byte();
      }
      break;

    case MUT_EMPTY:
      // Zero-length write (address only)
      len = 0;
      break;

    case MUT_MAX_LEN:
      // Maximum payload
      len = maxLen;
      buf[0] = (uint8_t)(prng_range(0x10));  // Register 0x00-0x0F
      for (uint8_t i = 1; i < len; i++) {
        buf[i] = prng_byte();
      }
      break;

    case MUT_OVERLENGTH:
      // Send more data than the slave can handle
      len = maxLen + (uint8_t)prng_range(20) + 5;  // 37-56 bytes
      for (uint8_t i = 0; i < len && i < 64; i++) {
        buf[i] = prng_byte();
      }
      break;

    case MUT_BAD_CRC:
      // For I2C: write to register 0xFF (likely invalid)
      len = (uint8_t)prng_range(8) + 1;
      buf[0] = 0xFF;  // Invalid register
      for (uint8_t i = 1; i < len; i++) {
        buf[i] = 0x00;  // All zeros (suspicious pattern)
      }
      break;

    case MUT_TRUNCATED:
      // Very short transaction
      len = 1;
      buf[0] = (uint8_t)(prng_range(0x20));
      break;

    case MUT_RANDOM:
      // Fully random address + data
      len = (uint8_t)prng_range(maxLen) + 1;
      for (uint8_t i = 0; i < len; i++) {
        buf[i] = prng_byte();
      }
      break;

    default:
      len = 2;
      buf[0] = 0x00;
      buf[1] = prng_byte();
      break;
  }

  return len;
}

// ============================================
// I2C Address Selection
// ============================================
static uint8_t select_address(MutationType mutation) {
  switch (mutation) {
    case MUT_VALID:
    case MUT_EMPTY:
    case MUT_MAX_LEN:
    case MUT_TRUNCATED:
      return Proto::I2cDefaultAddr;

    case MUT_OVERLENGTH:
    case MUT_BAD_CRC:
    case MUT_RANDOM:
      // 60% DUT address, 40% random
      if (prng_range(100) < 60) return Proto::I2cDefaultAddr;
      return (uint8_t)(prng_range(0x70) + 0x08);

    default:
      return Proto::I2cDefaultAddr;
  }
}

// ============================================
// Send a fuzzed I2C transaction
// ============================================
bool i2c_fuzzer_send(MutationType mutation) {
  uint8_t data[64];
  uint8_t dataLen = build_payload(data, Proto::I2cMaxPayload, mutation);
  uint8_t addr = select_address(mutation);

  // Begin transaction
  Wire.beginTransmission(addr);

  // Write data bytes
  if (dataLen > 0) {
    Wire.write(data, dataLen);
  }

  // End transmission — this sends on the bus
  //   0 = success (ACK)
  //   2 = NACK on address
  //   3 = NACK on data byte
  //   4 = other error
  uint8_t result = Wire.endTransmission(true);  // true = send stop

  // Record response
  s_lastResponse.addr = addr;
  s_lastResponse.writeResult = result;
  s_lastResponse.bytesWritten = dataLen;
  s_lastResponse.timestampMs = millis();
  s_lastResponse.received = true;
  s_responseReady = true;

  // Update statistics
  if (result == Proto::I2cRespSuccess) {
    s_totalAcks++;
  } else if (result == Proto::I2cRespNackAddr || result == Proto::I2cRespNackData) {
    s_totalNacks++;
  } else {
    s_totalErrors++;
  }

  // Log non-ACK responses
  if (result != Proto::I2cRespSuccess) {
    const char* resultStr = (result == Proto::I2cRespNackAddr) ? "NACK_ADDR" :
                            (result == Proto::I2cRespNackData) ? "NACK_DATA" :
                            "ERROR";
    Serial.printf("I2C addr=0x%02X %s (result=%u, sent=%u bytes)\n",
                  addr, resultStr, result, dataLen);
  }

  campaign_increment_packet_count();
  return true;
}

// ============================================
// Response monitoring
// ============================================
void i2c_fuzzer_poll_response(void) {
  // Response is captured synchronously in i2c_fuzzer_send()
}

bool i2c_fuzzer_has_response(void) {
  return s_responseReady;
}

bool i2c_fuzzer_get_response(I2cResponse* dest) {
  if (!s_responseReady || !dest) return false;
  memcpy(dest, &s_lastResponse, sizeof(I2cResponse));
  return true;
}

void i2c_fuzzer_clear_response(void) {
  s_responseReady = false;
}

// ============================================
// Statistics
// ============================================
uint32_t i2c_fuzzer_get_acks(void)   { return s_totalAcks; }
uint32_t i2c_fuzzer_get_nacks(void)  { return s_totalNacks; }
uint32_t i2c_fuzzer_get_errors(void) { return s_totalErrors; }

// ============================================
// I2C Bus Scan — uses Wire (shared with OLED)
// ============================================
uint8_t i2c_fuzzer_scan(void) {
  Serial.printf("Scanning I2C bus SDA=%d SCL=%d ...\n", Pin::I2cSda, Pin::I2cScl);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == Proto::I2cRespSuccess) {
      // Filter out OLED (0x3C) since it's on the shared bus
      if (addr == Pin::OledAddr) {
        Serial.printf("  0x%02X = OLED (filtered, on shared bus)\n", addr);
      } else {
        Serial.printf("  0x%02X = DUT device\n", addr);
        found++;
      }
    }
  }
  if (found == 0) Serial.println("  No DUT found (only OLED on shared bus)");
  else Serial.printf("  Total: %u DUT device(s)\n", found);
  return found;
}
