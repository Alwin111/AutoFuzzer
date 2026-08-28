#include "result.h"
#include "campaign.h"
#include "failure.h"

// ============================================
// Internal scores
// ============================================
static uint8_t s_score = 0;
static uint8_t s_commScore = 0;
static uint8_t s_boundScore = 0;
static uint8_t s_malfScore = 0;
static uint8_t s_recoveryScore = 0;
static uint8_t s_hbScore = 0;
static uint8_t s_randScore = 0;
static TestResult s_result = RESULT_NONE;

// ============================================
// Score Calculator
// ============================================
static uint8_t calc_score(uint32_t good, uint32_t total) {
  if (total == 0) return 100;  // No tests = no failures observed
  if (good >= total) return 100;
  return (uint8_t)((good * 100) / total);
}

// ============================================
// Calculate all scores
// ============================================
void result_calculate_all(void) {
  const CampaignStats* stats = campaign_get_stats();
  uint8_t failCount = failure_get_count();

  // Communication score: ACK rate vs total responses
  uint32_t totalResp = stats->totalAcks + stats->totalNacks;
  s_commScore = calc_score(stats->totalAcks, totalResp);

  // Boundary handling: valid + empty + max should all ACK
  // If any boundary packets caused failures, reduce score
  s_boundScore = 100;
  if (failCount > 0) {
    uint8_t boundaryFails = 0;
    for (uint8_t i = 0; i < failCount; i++) {
      const FailureRecord* rec = failure_get_by_index(i);
      if (rec && (rec->testcase.mutation == MUT_VALID ||
                  rec->testcase.mutation == MUT_EMPTY ||
                  rec->testcase.mutation == MUT_MAX_LEN)) {
        boundaryFails++;
      }
    }
    if (boundaryFails > 0) {
      s_boundScore = (boundaryFails > 3) ? 0 : 100 - (boundaryFails * 33);
    }
  }

  // Malformed input: overlength, bad CRC should get NACK or handle gracefully
  s_malfScore = 100;
  if (failCount > 0) {
    uint8_t malfFails = 0;
    for (uint8_t i = 0; i < failCount; i++) {
      const FailureRecord* rec = failure_get_by_index(i);
      if (rec && (rec->testcase.mutation == MUT_OVERLENGTH ||
                  rec->testcase.mutation == MUT_BAD_CRC)) {
        malfFails++;
      }
    }
    if (malfFails > 0) {
      s_malfScore = (malfFails > 3) ? 0 : 100 - (malfFails * 33);
    }
  }

  // Recovery: heartbeat timeouts indicate failed recovery
  uint32_t hbTimeouts = stats->totalHeartbeatTimeouts;
  uint32_t totalPkts = stats->totalPackets;
  if (totalPkts == 0) {
    s_recoveryScore = 100;
  } else {
    s_recoveryScore = calc_score(totalPkts - hbTimeouts, totalPkts);
  }

  // Heartbeat stability
  s_hbScore = (hbTimeouts == 0) ? 100 : (hbTimeouts <= 2 ? 75 : (hbTimeouts <= 5 ? 50 : 0));

  // Random input handling: random + adaptive mutations
  s_randScore = 100;
  if (failCount > 0) {
    uint8_t randFails = 0;
    for (uint8_t i = 0; i < failCount; i++) {
      const FailureRecord* rec = failure_get_by_index(i);
      if (rec && (rec->testcase.mutation == MUT_RANDOM)) {
        randFails++;
      }
    }
    if (randFails > 0) {
      s_randScore = (randFails > 3) ? 0 : 100 - (randFails * 33);
    }
  }

  // Overall score: weighted average
  s_score = (uint8_t)(
    (s_commScore * 25 + s_boundScore * 20 + s_malfScore * 20 +
     s_recoveryScore * 15 + s_hbScore * 10 + s_randScore * 10) / 100
  );

  // Determine verdict
  if (failCount == 0 && stats->totalPackets > 0) {
    s_result = RESULT_PASS;
  } else if (failCount > 0) {
    s_result = RESULT_FAIL;
  } else {
    s_result = RESULT_INCONCLUSIVE;
  }
}

// ============================================
// Public API
// ============================================
TestResult result_calculate(void) {
  result_calculate_all();
  return s_result;
}

uint8_t result_get_score(void) { return s_score; }
uint8_t result_get_communication_score(void) { return s_commScore; }
uint8_t result_get_boundary_score(void) { return s_boundScore; }
uint8_t result_get_malformed_score(void) { return s_malfScore; }
uint8_t result_get_recovery_score(void) { return s_recoveryScore; }
uint8_t result_get_heartbeat_score(void) { return s_hbScore; }
uint8_t result_get_random_score(void) { return s_randScore; }

const char* result_get_verdict(void) {
  switch (s_result) {
    case RESULT_PASS:         return "PASS";
    case RESULT_FAIL:         return "FAIL";
    case RESULT_INCONCLUSIVE: return "INCONCLUSIVE";
    default:                  return "NOT RUN";
  }
}

// ============================================
// Print Report
// ============================================
void result_print_report(void) {
  const CampaignStats* stats = campaign_get_stats();

  Serial.println();
  Serial.println("========================================");
  Serial.println("  AUTOFUZZER TEST RESULT");
  Serial.println("========================================");
  Serial.printf("  Protocol: %s\n", ProtocolNames[campaign_get_protocol()]);
  Serial.printf("  Profile:  %s\n", TestProfileNames[campaign_get_profile()]);
  Serial.printf("  Duration: %u ms\n", stats->elapsedMs);
  Serial.printf("  Packets:  %u\n", stats->totalPackets);
  Serial.printf("  ACKs:     %u\n", stats->totalAcks);
  Serial.printf("  NACKs:    %u\n", stats->totalNacks);
  Serial.printf("  No Resp:  %u\n", stats->totalNoResponse);
  Serial.printf("  Failures: %u\n", stats->totalHeartbeatTimeouts);
  Serial.println();
  Serial.printf("  ROBUSTNESS SCORE: %u/100\n", s_score);
  Serial.println();
  Serial.printf("  Communication:  %u/100\n", s_commScore);
  Serial.printf("  Boundary:       %u/100\n", s_boundScore);
  Serial.printf("  Malformed:      %u/100\n", s_malfScore);
  Serial.printf("  Recovery:       %u/100\n", s_recoveryScore);
  Serial.printf("  Heartbeat:      %u/100\n", s_hbScore);
  Serial.printf("  Random Input:   %u/100\n", s_randScore);
  Serial.println();
  Serial.printf("  VERDICT: %s\n", result_get_verdict());
  Serial.printf("  (for %s protocol, %s test profile)\n",
                ProtocolNames[campaign_get_protocol()],
                TestProfileNames[campaign_get_profile()]);
  Serial.println("========================================");
  Serial.println();

  // Print failure details if any
  uint8_t failCount = failure_get_count();
  if (failCount > 0) {
    Serial.printf("  FAILURE DETAILS (%u total):\n", failCount);
    for (uint8_t i = 0; i < failCount; i++) {
      const FailureRecord* rec = failure_get_by_index(i);
      failure_print_summary(rec);
    }
  }
}
