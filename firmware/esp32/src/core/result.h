#pragma once
#include "types.h"

// ============================================
// Result System
//
// Calculates test results and robustness scores
// based on campaign statistics and failure data.
//
// Score is computed from:
//   - Communication success rate
//   - Boundary handling
//   - Malformed input handling
//   - Recovery behavior
//   - Heartbeat stability
//   - Random input handling
// ============================================

// Calculate and return the test result
TestResult result_calculate(void);

// Get robustness score (0-100)
uint8_t result_get_score(void);

// Get individual category scores
uint8_t result_get_communication_score(void);
uint8_t result_get_boundary_score(void);
uint8_t result_get_malformed_score(void);
uint8_t result_get_recovery_score(void);
uint8_t result_get_heartbeat_score(void);
uint8_t result_get_random_score(void);

// Print full result report to serial
void result_print_report(void);

// Get result as a string description
const char* result_get_verdict(void);

// DEMO mode: when true, verdict is always PASS (no failures)
// regardless of response/heartbeat evidence. Set before or
// during a campaign via result_set_demo_mode().
void result_set_demo_mode(bool enable);
bool result_get_demo_mode(void);
