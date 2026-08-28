#include "i2c_fuzzer.h"
#include "../core/prng.h"
#include "../core/campaign.h"
#include <Wire.h>

void i2c_fuzzer_init(void) {
  // I2C is initialized by Wire.begin() in main setup
}

bool i2c_fuzzer_send(MutationType mutation) {
  (void)mutation;

  // Random 7-bit I2C address (0x08–0x77)
  uint8_t addr = (uint8_t)(prng_range(0x70) + 0x08);

  Wire.beginTransmission(addr);
  Wire.write(prng_byte());  // Register address
  Wire.write(prng_byte());  // Data value
  Wire.endTransmission();

  campaign_increment_packet_count();
  return true;
}
