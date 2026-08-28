#include "crash_detector.h"
#include "heartbeat.h"
#include "../protocols/uart_parser.h"
#include "../core/failure.h"
#include "../core/testcase.h"
#include "../core/campaign.h"

// ============================================
// Internal State
// ============================================
static bool         s_failed       = false;
static FailureType  s_failType     = FAIL_NONE;
static uint32_t     s_failHeartbeatAge = 0;

// ============================================
// Init
// ============================================
void crash_detector_init(void) {
  s_failed = false;
  s_failType = FAIL_NONE;
  s_failHeartbeatAge = 0;
}

// ============================================
// Update
// ============================================
bool crash_detector_update(void) {
  // If already in failure state, don't re-trigger
  if (s_failed) return false;

  // Only check during an active campaign
  if (!campaign_is_running()) return false;

  // Check heartbeat
  bool hbAlive = heartbeat_update();

  if (hbAlive) return false;

  // Heartbeat has timed out — this is a potential failure
  s_failHeartbeatAge = heartbeat_get_age_ms();

  // Check if DUT has responded recently
  bool dutResponded = uart_parser_has_response();
  uint8_t dutStatus = 0;
  uint32_t dutRespTime = 0;

  if (dutResponded) {
    const UartResponse* resp = uart_parser_get_response();
    dutStatus = resp->status;
    dutRespTime = resp->recvTimeMs;
  }

  // Classify the failure based on evidence
  s_failType = failure_classify_heartbeat(s_failHeartbeatAge, dutResponded);

  // Additional classification based on DUT response
  if (dutResponded) {
    // DUT responded but heartbeat died — could be peripheral lockup
    // or watchdog reset
    if (dutStatus == Proto::StatusAck) {
      s_failType = FAIL_WATCHDOG_RESET;
    } else {
      s_failType = FAIL_PROTOCOL_ERROR;
    }
  }

  // Mark as failed
  s_failed = true;

  // Get the current testcase
  const TestcaseMeta* tc = testcase_get_last();

  // Freeze the failure
  failure_freeze(s_failType, tc, s_failHeartbeatAge,
                 dutResponded, dutStatus, dutRespTime);

  Serial.printf("CRASH DETECTED: %s (hb_age=%u ms, dut_resp=%s)\n",
                FailureTypeNames[s_failType],
                s_failHeartbeatAge,
                dutResponded ? "yes" : "no");

  return true;
}

// ============================================
// Getters
// ============================================
bool crash_detector_is_failed(void) {
  return s_failed;
}

FailureType crash_detector_get_type(void) {
  return s_failType;
}

uint32_t crash_detector_get_heartbeat_age(void) {
  return s_failHeartbeatAge;
}

void crash_detector_clear(void) {
  s_failed = false;
  s_failType = FAIL_NONE;
  s_failHeartbeatAge = 0;
  heartbeat_reset();
}
