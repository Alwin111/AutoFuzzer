#include "buttons.h"

// ============================================
// Button State
// ============================================
struct ButtonState {
  int pin;
  ButtonAction action;
  bool lastRaw;
  bool stableState;
  uint32_t lastChangeMs;
  bool pressed;        // Edge-triggered: true for one cycle after press
};

static const uint32_t kDebounceMs = 25;

static ButtonState s_buttons[] = {
  { Pin::BtnUp,     BTN_UP,      HIGH, HIGH, 0, false },  // GPIO 13 → UP
  { Pin::BtnDown,   BTN_DOWN,    HIGH, HIGH, 0, false },  // GPIO 12 → DOWN
  { Pin::BtnBack,   BTN_BACK,    HIGH, HIGH, 0, false },  // GPIO 14 → BACK
  { Pin::BtnSelect, BTN_SELECT,  HIGH, HIGH, 0, false },  // GPIO 27 → SELECT
};

static const uint8_t kButtonCount = sizeof(s_buttons) / sizeof(s_buttons[0]);

// ============================================
// Init
// ============================================
void buttons_init(void) {
  for (uint8_t i = 0; i < kButtonCount; i++) {
    pinMode(s_buttons[i].pin, INPUT_PULLUP);
    s_buttons[i].lastRaw     = HIGH;
    s_buttons[i].stableState = HIGH;
    s_buttons[i].lastChangeMs = millis();
    s_buttons[i].pressed     = false;
  }
}

// ============================================
// Update — debounce each button
// ============================================
ButtonAction buttons_update(void) {
  uint32_t now = millis();
  ButtonAction result = BTN_NONE;

  for (uint8_t i = 0; i < kButtonCount; i++) {
    ButtonState& btn = s_buttons[i];
    bool raw = digitalRead(btn.pin);

    // Reset edge flag each iteration
    btn.pressed = false;

    // Debounce: if raw state differs from stable state, start timer
    if (raw != btn.stableState) {
      if (raw != btn.lastRaw) {
        btn.lastChangeMs = now;
        btn.lastRaw = raw;
      } else if (now - btn.lastChangeMs >= kDebounceMs) {
        // State has been stable for debounce period
        btn.stableState = raw;

        // Detect press (transition from HIGH to LOW)
        if (raw == LOW) {
          btn.pressed = true;
          if (result == BTN_NONE) {
            result = btn.action;
          }
        }
      }
    } else {
      btn.lastRaw = raw;
    }
  }

  return result;
}

// ============================================
// Check if button is held
// ============================================
bool buttons_is_held(ButtonAction action) {
  for (uint8_t i = 0; i < kButtonCount; i++) {
    if (s_buttons[i].action == action) {
      return s_buttons[i].stableState == LOW;
    }
  }
  return false;
}
