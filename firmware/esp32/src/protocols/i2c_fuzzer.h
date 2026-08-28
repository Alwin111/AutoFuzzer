#pragma once
#include "../core/types.h"

// ============================================
// I2C Fuzzer
//
// Protocol-aware I2C bus fuzzing using Wire on GPIO 21/22.
//
// SHARED with the OLED display — the OLED is disabled
// during I2C fuzzing to prevent bus corruption.
// OLED is restored when fuzzing ends.
//
// The DUT is expected to be an I2C slave at
// address 0x10 (configurable).
//
// ACK/NACK detection via Wire.endTransmission()
// return code.
// ============================================

// I2C response record (one per transaction)
struct I2cResponse {
  bool     received;      // Whether a response was captured
  uint8_t  addr;          // Address that was targeted
  uint8_t  writeResult;   // Wire.endTransmission() return code
  uint8_t  bytesWritten;  // Number of data bytes sent
  uint32_t timestampMs;   // When the response was captured
};

// Initialize the I2C fuzzing bus (Wire on GPIO 21/22, shared with OLED)
// Disables OLED display during fuzzing
void i2c_fuzzer_init(void);

// Deinit I2C bus and restore OLED display
void i2c_fuzzer_deinit(void);

// Send a fuzzed I2C transaction.
// Returns true if the bus operation completed (not necessarily ACK).
bool i2c_fuzzer_send(MutationType mutation);

// Check for I2C response (ACK/NACK)
// Must be called after each send to capture timing
void i2c_fuzzer_poll_response(void);

// Check if there's a new response to process
bool i2c_fuzzer_has_response(void);

// Get the latest response (copies to dest)
bool i2c_fuzzer_get_response(I2cResponse* dest);

// Clear the response buffer
void i2c_fuzzer_clear_response(void);

// Scan I2C bus for devices (uses Wire, OLED disabled)
// Returns number of devices found, prints to Serial
uint8_t i2c_fuzzer_scan(void);

// Get I2C statistics
uint32_t i2c_fuzzer_get_acks(void);
uint32_t i2c_fuzzer_get_nacks(void);
uint32_t i2c_fuzzer_get_errors(void);
