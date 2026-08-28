#pragma once
#include "../core/types.h"

// ============================================
// Indicators
//
// Controls LEDs and buzzer using PWM.
// All operations are non-blocking:
//   - LEDs: set on/off immediately
//   - Buzzer: beep for specified duration, returns immediately
//
// The buzzer uses a timer-based approach so it
// doesn't block the main loop.
// ============================================

// Initialize all indicators
void indicators_init(void);

// LED control
void indicators_led_pass_on(void);
void indicators_led_pass_off(void);
void indicators_led_fail_on(void);
void indicators_led_fail_off(void);
void indicators_led_active_on(void);
void indicators_led_active_off(void);

// All LEDs off
void indicators_all_leds_off(void);

// Buzzer — non-blocking beep
// Returns immediately; beep plays in background
void indicators_beep_start(uint32_t durationMs);
void indicators_beep_stop(void);

// Update — call every loop iteration
// Handles non-blocking buzzer timing
void indicators_update(void);

// Self-test — brief visual/audio confirmation
void indicators_self_test(void);

// Set fail LED state (convenience)
void indicators_set_fail(bool active);
void indicators_set_pass(bool active);
void indicators_set_active(bool active);
