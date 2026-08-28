#pragma once
#include "../core/types.h"

// ============================================
// Heartbeat Monitor
//
// Monitors the DUT's heartbeat signal:
//   - Edge detection on GPIO pin
//   - Timeout detection (no edge within threshold)
//   - Timing measurement for diagnostics
//
// The DUT generates a square wave (toggle every 100ms).
// The monitor detects transitions and triggers a failure
// if no transition occurs within Proto::HeartbeatTimeoutMs.
// ============================================

// Initialize heartbeat monitor
void heartbeat_init(void);

// Update — call every loop iteration
// Returns true if heartbeat is healthy, false if timed out
bool heartbeat_update(void);

// Check if heartbeat is currently active
bool heartbeat_is_alive(void);

// Get ms since last heartbeat edge
uint32_t heartbeat_get_age_ms(void);

// Get heartbeat period (time between last two edges) for diagnostics
uint32_t heartbeat_get_period_ms(void);

// Reset the monitor (e.g., after DUT reset)
void heartbeat_reset(void);
