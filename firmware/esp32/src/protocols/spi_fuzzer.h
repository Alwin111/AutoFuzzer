#pragma once
#include "../core/types.h"

// ============================================
// SPI Fuzzer
//
// Basic SPI bus stimulation.
// Currently: random transaction length + random bytes.
// Future: protocol-aware SPI fuzzing.
//
// Note: This is random bus stimulation,
// NOT protocol-aware SPI fuzzing.
// ============================================

void spi_fuzzer_init(void);
bool spi_fuzzer_send(MutationType mutation);
