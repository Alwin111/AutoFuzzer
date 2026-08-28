#pragma once
#include "../core/types.h"

// ============================================
// I2C Fuzzer
//
// Basic I2C bus stimulation.
// Currently: random address + register + data.
// Future: protocol-aware I2C fuzzing.
//
// Note: This shares the I2C bus with the OLED.
// Bus contention is expected when fuzzing I2C.
// ============================================

void i2c_fuzzer_init(void);
bool i2c_fuzzer_send(MutationType mutation);
