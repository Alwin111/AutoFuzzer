#pragma once
#include <Arduino.h>

// ============================================
// Deterministic PRNG — Xorshift32
// 
// Provides exact state snapshots so that:
//   SAME seed + SAME sequence + SAME mutation + SAME PRNG state
//   = SAME packet
//
// The global PRNG is used for packet generation.
// Snapshot/restore allows replay of exact failing testcases.
// ============================================

// Initialize PRNG with a known seed
void prng_init(uint32_t seed);

// Get current internal state (for snapshots)
uint32_t prng_get_state(void);

// Set internal state (for replay)
void prng_set_state(uint32_t state);

// Advance PRNG and return next random value
uint32_t prng_next(void);

// Generate random value in range [0, max)
uint32_t prng_range(uint32_t max);

// Generate random byte
uint8_t prng_byte(void);
