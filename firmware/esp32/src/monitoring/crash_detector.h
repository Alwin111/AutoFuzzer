#pragma once
#include "../core/types.h"

// ============================================
// Crash Detector
//
// Combines heartbeat monitoring and UART response
// data to detect and classify failures.
//
// Detection pipeline:
//   1. Heartbeat times out
//   2. Check if DUT responded recently
//   3. Classify failure type
//   4. Freeze campaign state
//   5. Record failure
//
// Does NOT immediately assume CPU crash.
// Uses careful terminology:
//   SUSPECTED FAILURE → REPLAY → REPRODUCIBLE/INTERMITTENT
// ============================================

// Initialize crash detector
void crash_detector_init(void);

// Update — call every loop iteration after heartbeat and parser updates
// Returns true if a new failure was detected this iteration
bool crash_detector_update(void);

// Check if we're currently in a failure state
bool crash_detector_is_failed(void);

// Get the classification of the current failure
FailureType crash_detector_get_type(void);

// Get heartbeat age at time of failure
uint32_t crash_detector_get_heartbeat_age(void);

// Clear failure state (e.g., after DUT recovery or replay)
void crash_detector_clear(void);

// Set the last mutation type sent (for adaptive statistics)
void crash_detector_set_last_mutation(MutationType mut);
