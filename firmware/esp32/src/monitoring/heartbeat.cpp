#include "heartbeat.h"

// ============================================
// Internal State
// ============================================
static bool     s_lastState      = LOW;
static uint32_t s_lastEdgeMs     = 0;
static uint32_t s_prevEdgeMs     = 0;  // For period measurement
static uint32_t s_periodMs       = 0;
static bool     s_alive          = true;
static uint32_t s_initTimeMs     = 0;
static uint32_t s_lastEdgeDebounceMs = 0;  // Debounce floating pin edges

// ============================================
// Init
// ============================================
void heartbeat_init(void) {
  pinMode(Pin::Heartbeat, INPUT);
  s_lastState  = digitalRead(Pin::Heartbeat);
  s_lastEdgeMs = millis();
  s_prevEdgeMs = s_lastEdgeMs;
  s_alive      = true;
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
      return s_alive;  // Too soon — likely floating pin noise
    }
    s_lastEdgeDebounceMs = now;
    s_lastState = currentState;
    s_prevEdgeMs = s_lastEdgeMs;
    s_lastEdgeMs = now;

    // Calculate period (time between last two edges)
    if (s_lastEdgeMs > s_prevEdgeMs) {
      s_periodMs = s_lastEdgeMs - s_prevEdgeMs;
    }

    if (!s_alive) {
      s_alive = true;
      Serial.printf("Heartbeat RECOVERED (period=%u ms)\n", s_periodMs);
    }
  }

  // Check timeout
  uint32_t age = millis() - s_lastEdgeMs;

  // Allow a grace period after init (DUT may need time to boot)
  if (millis() - s_initTimeMs < 2000) {
    return true;
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
  s_alive      = true;
  s_initTimeMs = millis();
}
