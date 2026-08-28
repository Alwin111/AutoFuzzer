#include "prng.h"

static uint32_t s_prng_state = 0xC0DEC0DE;

void prng_init(uint32_t seed) {
  s_prng_state = seed;
  // Ensure state is never zero (xorshift requirement)
  if (s_prng_state == 0) s_prng_state = 0xC0DEC0DE;
}

uint32_t prng_get_state(void) {
  return s_prng_state;
}

void prng_set_state(uint32_t state) {
  s_prng_state = state;
  if (s_prng_state == 0) s_prng_state = 0xC0DEC0DE;
}

uint32_t prng_next(void) {
  s_prng_state ^= (s_prng_state << 13);
  s_prng_state ^= (s_prng_state >> 17);
  s_prng_state ^= (s_prng_state << 5);
  return s_prng_state;
}

uint32_t prng_range(uint32_t max) {
  if (max == 0) return 0;
  return prng_next() % max;
}

uint8_t prng_byte(void) {
  return (uint8_t)(prng_next() & 0xFF);
}
