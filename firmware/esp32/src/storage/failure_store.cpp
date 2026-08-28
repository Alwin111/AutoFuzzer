#include "failure_store.h"

// ============================================
// Internal Storage
// ============================================
static const uint8_t kMaxStored = 20;
static FailureRecord s_store[kMaxStored];
static uint8_t s_count = 0;

// ============================================
// Init
// ============================================
void failure_store_init(void) {
  s_count = 0;
  memset(s_store, 0, sizeof(s_store));
}

// ============================================
// Add
// ============================================
void failure_store_add(const FailureRecord* rec) {
  if (!rec || s_count >= kMaxStored) return;
  memcpy(&s_store[s_count], rec, sizeof(FailureRecord));
  s_store[s_count].id = s_count;
  s_count++;
}

// ============================================
// Getters
// ============================================
uint8_t failure_store_count(void) {
  return s_count;
}

const FailureRecord* failure_store_get(uint8_t idx) {
  if (idx >= s_count) return nullptr;
  return &s_store[idx];
}

// ============================================
// Export One
// ============================================
void failure_store_export_one(uint8_t idx) {
  const FailureRecord* rec = failure_store_get(idx);
  if (!rec) return;

  Serial.printf("{\n");
  Serial.printf("  \"id\": %u,\n", rec->id);
  Serial.printf("  \"type\": \"%s\",\n", FailureTypeNames[rec->type]);
  Serial.printf("  \"status\": \"%s\",\n", FailureStatusNames[rec->status]);
  Serial.printf("  \"protocol\": \"%s\",\n", ProtocolNames[rec->protocol]);
  Serial.printf("  \"profile\": \"%s\",\n", TestProfileNames[rec->profile]);
  Serial.printf("  \"sequence\": %u,\n", rec->testcase.sequence);
  Serial.printf("  \"mutation\": \"%s\",\n", MutationNames[rec->testcase.mutation]);
  Serial.printf("  \"packetLen\": %u,\n", rec->testcase.packetLen);
  Serial.printf("  \"payloadLen\": %u,\n", rec->testcase.payloadLen);
  Serial.printf("  \"seed\": \"0x%08X\",\n", rec->testcase.packetSeed);
  Serial.printf("  \"prngBefore\": \"0x%08X\",\n", rec->testcase.prngStateBefore);
  Serial.printf("  \"prngAfter\": \"0x%08X\",\n", rec->testcase.prngStateAfter);
  Serial.printf("  \"heartbeatLostMs\": %u,\n", rec->heartbeatLostMs);
  Serial.printf("  \"dutResponded\": %s,\n", rec->dutResponded ? "true" : "false");
  Serial.printf("  \"dutResponseStatus\": \"0x%02X\",\n", rec->dutResponseStatus);
  Serial.printf("  \"dutRespTimeMs\": %u,\n", rec->dutRespTimeMs);
  Serial.printf("  \"packetsAtFail\": %u,\n", rec->packetCountAtFail);
  Serial.printf("  \"elapsedMs\": %u,\n", rec->campaignElapsedMs);
  Serial.printf("  \"replayAttempts\": %u,\n", rec->replayAttempts);
  Serial.printf("  \"replayFails\": %u,\n", rec->replayFails);
  Serial.printf("  \"minimizedLen\": %u,\n", rec->minimizedLen);

  // Packet hex
  Serial.printf("  \"packetHex\": \"");
  for (uint8_t i = 0; i < rec->testcase.packetLen && i < 64; i++) {
    Serial.printf("%02X", rec->testcase.packetBytes[i]);
  }
  Serial.printf("\"\n");
  Serial.printf("}\n");
}

// ============================================
// Export All
// ============================================
void failure_store_export_all(void) {
  Serial.println("--- AUTOFUZZER_EXPORT_START ---");
  Serial.printf("{ \"version\": \"%s\", \"count\": %u, \"records\": [\n", AUTOFUZZER_VERSION, s_count);

  for (uint8_t i = 0; i < s_count; i++) {
    failure_store_export_one(i);
    if (i < s_count - 1) Serial.println(",");
  }

  Serial.println("] }");
  Serial.println("--- AUTOFUZZER_EXPORT_END ---");
}

// ============================================
// Clear
// ============================================
void failure_store_clear(void) {
  s_count = 0;
  memset(s_store, 0, sizeof(s_store));
}
