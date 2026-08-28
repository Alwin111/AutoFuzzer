#include "indicators.h"

// ============================================
// Buzzer State (non-blocking)
// ============================================
static bool     s_buzzActive  = false;
static uint32_t s_buzzStartMs = 0;
static uint32_t s_buzzDurationMs = 0;

// ============================================
// Init
// ============================================
void indicators_init(void) {
  // Setup PWM channels
  ledcSetup(Pwm::ChPassLed,   Pwm::LedFreq, Pwm::LedRes);
  ledcSetup(Pwm::ChFailLed,   Pwm::LedFreq, Pwm::LedRes);
  ledcSetup(Pwm::ChActiveLed, Pwm::LedFreq, Pwm::LedRes);
  ledcSetup(Pwm::ChBuzzer,    Pwm::BuzzerFreq, Pwm::BuzzerRes);

  // Attach pins
  ledcAttachPin(Pin::LedPass,   Pwm::ChPassLed);
  ledcAttachPin(Pin::LedFail,   Pwm::ChFailLed);
  ledcAttachPin(Pin::LedActive, Pwm::ChActiveLed);
  ledcAttachPin(Pin::Buzzer,    Pwm::ChBuzzer);

  // Start with everything off
  indicators_all_leds_off();
  s_buzzActive = false;
}

// ============================================
// LED Control
// ============================================
void indicators_led_pass_on(void) {
  ledcWrite(Pwm::ChPassLed, Pwm::LedOn);
}

void indicators_led_pass_off(void) {
  ledcWrite(Pwm::ChPassLed, Pwm::LedOff);
}

void indicators_led_fail_on(void) {
  ledcWrite(Pwm::ChFailLed, Pwm::LedOn);
}

void indicators_led_fail_off(void) {
  ledcWrite(Pwm::ChFailLed, Pwm::LedOff);
}

void indicators_led_active_on(void) {
  ledcWrite(Pwm::ChActiveLed, Pwm::LedOn);
}

void indicators_led_active_off(void) {
  ledcWrite(Pwm::ChActiveLed, Pwm::LedOff);
}

void indicators_all_leds_off(void) {
  indicators_led_pass_off();
  indicators_led_fail_off();
  indicators_led_active_off();
  ledcWrite(Pwm::ChBuzzer, Pwm::BuzzOff);
}

void indicators_set_fail(bool active) {
  if (active) indicators_led_fail_on();
  else indicators_led_fail_off();
}

void indicators_set_pass(bool active) {
  if (active) indicators_led_pass_on();
  else indicators_led_pass_off();
}

void indicators_set_active(bool active) {
  if (active) indicators_led_active_on();
  else indicators_led_active_off();
}

// ============================================
// Buzzer — Non-blocking
// ============================================
void indicators_beep_start(uint32_t durationMs) {
  s_buzzActive = true;
  s_buzzStartMs = millis();
  s_buzzDurationMs = durationMs;
  ledcWrite(Pwm::ChBuzzer, Pwm::BuzzOn);
}

void indicators_beep_stop(void) {
  s_buzzActive = false;
  ledcWrite(Pwm::ChBuzzer, Pwm::BuzzOff);
}

// ============================================
// Update — handle buzzer timing
// ============================================
void indicators_update(void) {
  if (s_buzzActive) {
    if (millis() - s_buzzStartMs >= s_buzzDurationMs) {
      indicators_beep_stop();
    }
  }
}

// ============================================
// Self-test
// ============================================
void indicators_self_test(void) {
  indicators_led_pass_on();
  indicators_led_active_on();
  indicators_beep_start(100);
  // Note: caller should call indicators_update() before moving on
  // to let the beep play. For simplicity, we add a small non-blocking
  // delay alternative: just let the main loop handle it.
  delay(150);
  indicators_update();
  indicators_led_pass_off();
  indicators_led_active_off();
}
