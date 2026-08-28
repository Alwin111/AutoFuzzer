#include "failure.h"
#include "campaign.h"

// ============================================
// Storage — ring buffer of failure records
// ============================================
static const uint8_t kMaxFailures = 10;
static FailureRecord s_failures[kMaxFailures];
static uint8_t s_failureCount = 0;
static uint8_t s_currentIdx   = 0;  // Index of the failure being investigated

// ============================================
// Init
// ============================================
void failure_init(void) {
  s_failureCount = 0;
  s_currentIdx = 0;
  memset(s_failures, 0, sizeof(s_failures));
}

// ============================================
// Classify heartbeat timeout
// ============================================
FailureType failure_classify_heartbeat(uint32_t timeoutMs, bool dutResponded) {
  // If DUT responded but heartbeat died, likely a watchdog reset
  // or peripheral lockup
  if (dutResponded && timeoutMs > 1000) {
    return FAIL_WATCHDOG_RESET;
  }

  // Large timeout suggests complete hang
  if (timeoutMs > 2000) {
    return FAIL_HEARTBEAT_TIMEOUT;
  }

  // Standard timeout
  if (timeoutMs > 350) {
    return FAIL_HEARTBEAT_TIMEOUT;
  }

  // Very short — might be power issue
  if (timeoutMs < 50) {
    return FAIL_POWER_FAILURE;
  }

  return FAIL_HEARTBEAT_TIMEOUT;
}

// ============================================
// Freeze — capture failure state
// ============================================
void failure_freeze(FailureType type, const TestcaseMeta* testcase,
                    uint32_t heartbeatLostMs, bool dutResponded,
                    uint8_t dutStatus, uint32_t dutRespTimeMs) {
  if (s_failureCount >= kMaxFailures) {
    // Ring buffer — overwrite oldest
    // Shift records left
    for (uint8_t i = 0; i < kMaxFailures - 1; i++) {
      memcpy(&s_failures[i], &s_failures[i + 1], sizeof(FailureRecord));
    }
    s_failureCount = kMaxFailures - 1;
  }

  FailureRecord* rec = &s_failures[s_failureCount];

  rec->id                 = s_failureCount;
  rec->type               = type;
  rec->status             = STATUS_SUSPECTED;
  rec->protocol           = campaign_get_protocol();
  rec->profile            = campaign_get_profile();
  rec->heartbeatLostMs    = heartbeatLostMs;
  rec->dutRespTimeMs      = dutRespTimeMs;
  rec->dutResponseStatus  = dutStatus;
  rec->dutResponded       = dutResponded;
  rec->packetCountAtFail  = campaign_get_packet_count();
  rec->campaignElapsedMs  = campaign_get_elapsed_ms();
  rec->replayAttempts     = 0;
  rec->replayFails        = 0;
  rec->minimizedLen       = 0;

  // Copy testcase metadata
  if (testcase) {
    memcpy(&rec->testcase, testcase, sizeof(TestcaseMeta));
  }

  s_currentIdx = s_failureCount;
  s_failureCount++;

  // Update campaign stats
  campaign_record_heartbeat_timeout();

  Serial.printf("FAILURE #%u frozen: %s at seq=%u, %u ms into campaign\n",
                rec->id, FailureTypeNames[type],
                rec->testcase.sequence, rec->campaignElapsedMs);
}

// ============================================
// Getters
// ============================================
const FailureRecord* failure_get_current(void) {
  if (s_failureCount == 0) return nullptr;
  return &s_failures[s_currentIdx];
}

const FailureRecord* failure_get_by_index(uint8_t idx) {
  if (idx >= s_failureCount) return nullptr;
  return &s_failures[idx];
}

uint8_t failure_get_count(void) {
  return s_failureCount;
}

// ============================================
// Update after replay
// ============================================
void failure_update_status(FailureStatus status, uint8_t attempts, uint8_t fails) {
  if (s_failureCount == 0) return;
  FailureRecord* rec = &s_failures[s_currentIdx];
  rec->status = status;
  rec->replayAttempts = attempts;
  rec->replayFails = fails;
}

void failure_update_minimized(uint32_t minimizedLen) {
  if (s_failureCount == 0) return;
  s_failures[s_currentIdx].minimizedLen = minimizedLen;
}

// ============================================
// Clear
// ============================================
void failure_clear_all(void) {
  s_failureCount = 0;
  s_currentIdx = 0;
  memset(s_failures, 0, sizeof(s_failures));
}

// ============================================
// Serial Export (JSON-like record)
// ============================================
void failure_export_serial(uint8_t idx) {
  const FailureRecord* rec = failure_get_by_index(idx);
  if (!rec) return;

  Serial.println("--- FAILURE RECORD START ---");
  Serial.printf("{\"id\":%u,", rec->id);
  Serial.printf("\"type\":\"%s\",", FailureTypeNames[rec->type]);
  Serial.printf("\"status\":\"%s\",", FailureStatusNames[rec->status]);
  Serial.printf("\"protocol\":\"%s\",", ProtocolNames[rec->protocol]);
  Serial.printf("\"profile\":\"%s\",", TestProfileNames[rec->profile]);
  Serial.printf("\"seq\":%u,", rec->testcase.sequence);
  Serial.printf("\"mutation\":\"%s\",", MutationNames[rec->testcase.mutation]);
  Serial.printf("\"pktLen\":%u,", rec->testcase.packetLen);
  Serial.printf("\"payLen\":%u,", rec->testcase.payloadLen);
  Serial.printf("\"seed\":%u,", rec->testcase.packetSeed);
  Serial.printf("\"prngBefore\":%u,", rec->testcase.prngStateBefore);
  Serial.printf("\"prngAfter\":%u,", rec->testcase.prngStateAfter);
  Serial.printf("\"heartbeatLostMs\":%u,", rec->heartbeatLostMs);
  Serial.printf("\"dutResponded\":%s,", rec->dutResponded ? "true" : "false");
  Serial.printf("\"dutStatus\":%u,", rec->dutResponseStatus);
  Serial.printf("\"dutRespTimeMs\":%u,", rec->dutRespTimeMs);
  Serial.printf("\"packetsAtFail\":%u,", rec->packetCountAtFail);
  Serial.printf("\"elapsedMs\":%u,", rec->campaignElapsedMs);
  Serial.printf("\"replayAttempts\":%u,", rec->replayAttempts);
  Serial.printf("\"replayFails\":%u,", rec->replayFails);
  Serial.printf("\"minimizedLen\":%u,", rec->minimizedLen);

  // Packet bytes hex
  Serial.print("\"bytes\":\"");
  for (uint8_t i = 0; i < rec->testcase.packetLen && i < 64; i++) {
    Serial.printf("%02X", rec->testcase.packetBytes[i]);
  }
  Serial.print("\"");

  Serial.println("}");
  Serial.println("--- FAILURE RECORD END ---");
}

// ============================================
// Print Summary
// ============================================
void failure_print_summary(const FailureRecord* rec) {
  if (!rec) return;
  Serial.printf("FAILURE #%u [%s] — %s\n", rec->id,
                FailureTypeNames[rec->type], FailureStatusNames[rec->status]);
  Serial.printf("  Protocol: %s, Profile: %s\n",
                ProtocolNames[rec->protocol], TestProfileNames[rec->profile]);
  Serial.printf("  Seq: %u, Mutation: %s, PktLen: %u\n",
                rec->testcase.sequence, MutationNames[rec->testcase.mutation],
                rec->testcase.packetLen);
  Serial.printf("  Heartbeat lost: %u ms, DUT responded: %s\n",
                rec->heartbeatLostMs, rec->dutResponded ? "yes" : "no");
  if (rec->replayAttempts > 0) {
    Serial.printf("  Replay: %u/%u failed\n", rec->replayFails, rec->replayAttempts);
  }
  if (rec->minimizedLen > 0) {
    Serial.printf("  Minimized to: %u bytes\n", rec->minimizedLen);
  }
}
