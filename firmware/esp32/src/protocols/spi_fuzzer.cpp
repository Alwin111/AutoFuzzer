#include "spi_fuzzer.h"
#include "../core/prng.h"
#include "../core/campaign.h"
#include <SPI.h>

void spi_fuzzer_init(void) {
  pinMode(Pin::SpiCs, OUTPUT);
  digitalWrite(Pin::SpiCs, HIGH);
  SPI.begin(Pin::SpiSck, Pin::SpiMiso, Pin::SpiMosi, Pin::SpiCs);
}

bool spi_fuzzer_send(MutationType mutation) {
  (void)mutation;  // SPI fuzzer doesn't differentiate mutations yet

  digitalWrite(Pin::SpiCs, LOW);
  delayMicroseconds(10);

  uint8_t spiLen = (uint8_t)(prng_range(16) + 4);
  for (uint8_t i = 0; i < spiLen; i++) {
    SPI.transfer((uint8_t)prng_byte());
  }

  delayMicroseconds(10);
  digitalWrite(Pin::SpiCs, HIGH);

  campaign_increment_packet_count();
  return true;
}
