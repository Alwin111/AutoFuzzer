#pragma once
#include "../core/types.h"

// ============================================
// Button Handler
//
// Provides proper debouncing using per-button
// state machines with separate press/release
// detection.
//
// Each button has:
//   - Debounce timer (25 ms)
//   - Press detection (edge-triggered)
//   - Held detection (optional future use)
//
// Buttons are active LOW (INPUT_PULLUP).
//
// Mapping:
//   BTN 1 (GPIO 12) → BTN_SELECT
//   BTN 2 (GPIO 13) → BTN_NEXT
//   BTN 3 (GPIO 14) → BTN_BACK
//   BTN 4 (GPIO 27) → BTN_DISPLAY
// ============================================

// Initialize all buttons with debounce
void buttons_init(void);

// Update — call every loop iteration
// Returns the action of any button pressed this iteration
// (only one action per call, in priority order)
ButtonAction buttons_update(void);

// Check if a specific button is currently held
bool buttons_is_held(ButtonAction action);
