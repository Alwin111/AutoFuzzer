#pragma once
#include "../core/types.h"

// ============================================
// Heartbeat Monitor
//
// Monitors the DUT's heartbeat signal:
//   - Edge detection on GPIO pin (pull-down enabled:
//     a floating pin with no DUT reads LOW and quiet)
//   - Timeout detection (no edge within threshold)
//   - Timing measurement for diagnostics
//
// The DUT generates a square wave (toggle every 100ms).
// The monitor detects transitions and triggers a failure
// if no transition occurs within Proto::HeartbeatTimeoutMs.
//
// Evidence rules:
//   - heartbeat_is_alive() is FALSE until a REAL edge is
//     observed. A silent line can never report "alive".
//   - heartbeat_has_signal() reports whether at least one
//     real edge was observed since init/clear. It is kept
//     across heartbeat_reset() so a finished campaign still
//     has its evidence for result scoring.
// ============================================

// Initialize heartbeat monitor
void heartbeat_init(void);

// Update — call every loop iteration
// Returns true if heartbeat is healthy, false if timed out or silent
bool heartbeat_update(void);

// Check if heartbeat is currently active
bool heartbeat_is_alive(void);

// True if at least one real heartbeat edge was observed
// since heartbeat_init()/heartbeat_clear_signal()
bool heartbeat_has_signal(void);

// Forget observed signal — used when starting a NEW campaign
// so results always reflect the current DUT connection
void heartbeat_clear_signal(void);

// Get ms since last heartbeat edge
uint32_t heartbeat_get_age_ms(void);

// Get heartbeat period (time between last two edges) for diagnostics
uint32_t heartbeat_get_period_ms(void);

// Reset the monitor timing (e.g., after DUT reset or before replay).
// Does NOT forget the has-signal evidence.
void heartbeat_reset(void);
