#include "testcase.h"
#include "prng.h"

static TestcaseMeta s_current;
static TestcaseMeta s_last;

void testcase_prepare(uint16_t sequence, MutationType mutation) {
  s_current.campaignSeed    = 0xC0DEC0DE;  // Will be updated by caller if needed
  s_current.packetSeed      = prng_get_state();
  s_current.prngStateBefore = prng_get_state();
  s_current.sequence        = sequence;
  s_current.mutation        = mutation;
  s_current.packetLen       = 0;
  s_current.payloadLen      = 0;
  s_current.timestampMs     = millis();
  memset(s_current.packetBytes, 0, sizeof(s_current.packetBytes));
}

void testcase_finalize(uint8_t packetLen, uint8_t payloadLen, const uint8_t* bytes) {
  s_current.packetLen  = packetLen;
  s_current.payloadLen = payloadLen;
  s_current.prngStateAfter = prng_get_state();

  if (packetLen <= sizeof(s_current.packetBytes)) {
    memcpy(s_current.packetBytes, bytes, packetLen);
  }

  // Move current to last
  memcpy(&s_last, &s_current, sizeof(TestcaseMeta));
}

const TestcaseMeta* testcase_get_current(void) {
  return &s_current;
}

const TestcaseMeta* testcase_get_last(void) {
  return &s_last;
}

void testcase_copy_last(TestcaseMeta* dest) {
  if (dest) {
    memcpy(dest, &s_last, sizeof(TestcaseMeta));
  }
}

void testcase_print(const TestcaseMeta* tc) {
  if (!tc) return;
  Serial.printf("  TC seq=%u mut=%s pktLen=%u payLen=%u seed=0x%08X prng=0x%08X->0x%08X\n",
                tc->sequence, MutationNames[tc->mutation],
                tc->packetLen, tc->payloadLen,
                tc->packetSeed, tc->prngStateBefore, tc->prngStateAfter);

  // Print first 20 bytes
  uint8_t printLen = tc->packetLen > 20 ? 20 : tc->packetLen;
  Serial.print("  BYTES: ");
  for (uint8_t i = 0; i < printLen; i++) {
    Serial.printf("%02X ", tc->packetBytes[i]);
  }
  if (tc->packetLen > 20) Serial.print("...");
  Serial.println();
}
