#include "heartbeat.h"

// ============================================
// Internal State
// ============================================
static bool     s_lastState      = LOW;
static uint32_t s_lastEdgeMs     = 0;
static uint32_t s_prevEdgeMs     = 0;  // For period measurement
static uint32_t s_periodMs       = 0;
static bool     s_alive          = false;  // FALSE until a real edge is seen
static bool     s_hasSignal      = false;  // Evidence: at least one real edge ever
static uint32_t s_initTimeMs     = 0;
static uint32_t s_lastEdgeDebounceMs = 0;  // Debounce noise edges

// ============================================
// Init
// ============================================
void heartbeat_init(void) {
  // INPUT_PULLDOWN: with no DUT connected the pin reads a
  // quiet LOW instead of floating and generating noise edges.
  pinMode(Pin::Heartbeat, INPUT_PULLDOWN);
  s_lastState  = digitalRead(Pin::Heartbeat);
  s_lastEdgeMs = millis();
  s_prevEdgeMs = s_lastEdgeMs;
  s_alive      = false;   // No evidence yet — cannot claim alive
  s_hasSignal  = false;
  s_initTimeMs = millis();
}

// ============================================
// Update — detect edges and timeouts
// ============================================
bool heartbeat_update(void) {
  bool currentState = digitalRead(Pin::Heartbeat);

  // Edge detected (debounced — ignore edges within 10ms)
  if (currentState != s_lastState) {
    uint32_t now = millis();
    if (now - s_lastEdgeDebounceMs < 10) {
      return s_alive;  // Too soon — likely pin noise
    }
    s_lastEdgeDebounceMs = now;
    s_lastState = currentState;
    s_prevEdgeMs = s_lastEdgeMs;
    s_lastEdgeMs = now;

    // Calculate period (time between last two edges)
    if (s_lastEdgeMs > s_prevEdgeMs) {
      s_periodMs = s_lastEdgeMs - s_prevEdgeMs;
    }

    // A real edge is definitive evidence of a live DUT
    if (!s_alive) {
      s_alive = true;
      Serial.printf("Heartbeat DETECTED (period=%u ms)\n", s_periodMs);
    }
    s_hasSignal = true;
  }

  // Check timeout
  uint32_t age = millis() - s_lastEdgeMs;

  // Never had a real edge: this is NOT a failure event, the DUT
  // is simply not connected / not toggling. Report not-alive and
  // let the caller (protocol check / campaign gate) decide.
  if (!s_hasSignal) {
    return false;
  }

  // Allow a grace period after reset (DUT may need time to boot)
  if (millis() - s_initTimeMs < 2000) {
    return s_alive;
  }

  if (age > Proto::HeartbeatTimeoutMs) {
    if (s_alive) {
      s_alive = false;
      Serial.printf("Heartbeat TIMEOUT after %u ms\n", age);
    }
    return false;
  }

  return true;
}

// ============================================
// Getters
// ============================================
bool heartbeat_is_alive(void) {
  return s_alive;
}

bool heartbeat_has_signal(void) {
  return s_hasSignal;
}

void heartbeat_clear_signal(void) {
  s_hasSignal = false;
  s_alive     = false;
}

uint32_t heartbeat_get_age_ms(void) {
  return millis() - s_lastEdgeMs;
}

uint32_t heartbeat_get_period_ms(void) {
  return s_periodMs;
}

void heartbeat_reset(void) {
  s_lastState  = digitalRead(Pin::Heartbeat);
  s_lastEdgeMs = millis();
  s_prevEdgeMs = s_lastEdgeMs;
  // s_alive stays as-is: only a real edge can revive it
  s_initTimeMs = millis();
}
