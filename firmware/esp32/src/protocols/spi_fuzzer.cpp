#include "spi_fuzzer.h"
#include "../core/prng.h"
#include "../core/campaign.h"
#include "../core/testcase.h"
#include <SPI.h>

// ============================================
// SPI Fuzzer — Protocol-Aware Mutation Engine
//
// Each mutation type produces distinct SPI bus behavior:
//   VALID:      Normal 4-8 byte transaction
//   EMPTY:      CS toggle only, no data
//   MAX_LEN:    64-byte transaction
//   OVERLENGTH: 80-byte burst (exceeds expected max)
//   BAD_CRC:    Invalid command byte (0xFF)
//   TRUNCATED:  Only 1-2 bytes sent
//   RANDOM:     PRNG-selected length and content
// ============================================

void spi_fuzzer_init(void) {
  pinMode(Pin::SpiCs, OUTPUT);
  digitalWrite(Pin::SpiCs, HIGH);
  SPI.begin(Pin::SpiSck, Pin::SpiMiso, Pin::SpiMosi, Pin::SpiCs);
  SPI.setClockDivider(SPI_CLOCK_DIV8);  // 2 MHz
  SPI.setDataMode(SPI_MODE0);
  Serial.printf("SPI fuzzer: SCK=%d MISO=%d MOSI=%d CS=%d\n",
                Pin::SpiSck, Pin::SpiMiso, Pin::SpiMosi, Pin::SpiCs);
}

bool spi_fuzzer_send(MutationType mutation) {
  uint8_t txBuf[80];
  uint8_t txLen = 0;
  uint8_t rxBuf[80];

  // Build transaction based on mutation type
  switch (mutation) {
    case MUT_VALID: {
      // Normal 4-8 byte transaction with valid-looking command
      txLen = (uint8_t)(prng_range(5) + 4);  // 4-8 bytes
      txBuf[0] = 0x01;  // Register/command
      for (uint8_t i = 1; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }

    case MUT_EMPTY: {
      // CS toggle only — no data bytes
      txLen = 0;
      break;
    }

    case MUT_MAX_LEN: {
      // Maximum expected transaction size (64 bytes)
      txLen = 64;
      txBuf[0] = 0x01;  // Command
      txBuf[1] = 0x00;  // Register
      for (uint8_t i = 2; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }

    case MUT_OVERLENGTH: {
      // Burst — send more data than expected (80 bytes)
      txLen = 80;
      for (uint8_t i = 0; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }

    case MUT_BAD_CRC: {
      // Invalid command byte — likely unrecognized opcode
      txLen = (uint8_t)(prng_range(8) + 2);  // 2-9 bytes
      txBuf[0] = 0xFF;  // Invalid command
      txBuf[1] = 0xFF;  // Invalid register
      for (uint8_t i = 2; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }

    case MUT_TRUNCATED: {
      // Very short transaction — just 1-2 bytes
      txLen = (uint8_t)(prng_range(2) + 1);  // 1-2 bytes
      txBuf[0] = (uint8_t)(prng_range(0x20));  // Random command
      break;
    }

    case MUT_RANDOM: {
      // Fully random length and content
      txLen = (uint8_t)(prng_range(64) + 1);  // 1-64 bytes
      for (uint8_t i = 0; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }

    default: {
      txLen = 4;
      for (uint8_t i = 0; i < txLen; i++) {
        txBuf[i] = prng_byte();
      }
      break;
    }
  }

  // CS timing variation for some mutations
  uint16_t csHoldUs = 10;  // Default 10us
  if (mutation == MUT_RANDOM) {
    csHoldUs = (uint16_t)(prng_range(100) + 5);  // 5-104us
  } else if (mutation == MUT_OVERLENGTH) {
    csHoldUs = 5;  // Short CS hold for burst
  }

  // Transmit
  digitalWrite(Pin::SpiCs, LOW);
  delayMicroseconds(csHoldUs);

  for (uint8_t i = 0; i < txLen; i++) {
    rxBuf[i] = SPI.transfer(txBuf[i]);
  }

  delayMicroseconds(csHoldUs);
  digitalWrite(Pin::SpiCs, HIGH);

  // Log interesting mutations
  if (mutation != MUT_VALID && mutation != MUT_RANDOM) {
    const char* mutName = MutationNames[mutation];
    Serial.printf("SPI %s: %u bytes, CS=%uus", mutName, txLen, csHoldUs);
    // Show first few bytes for debugging
    uint8_t show = txLen > 6 ? 6 : txLen;
    for (uint8_t i = 0; i < show; i++) {
      Serial.printf(" %02X", txBuf[i]);
    }
    if (txLen > 6) Serial.print("...");
    Serial.println();
  }

  campaign_increment_packet_count();
  return true;
}
