// ============================================
// AutoFuzzer v4.0 — Main Controller
//
// Standalone embedded protocol robustness
// testing platform.
//
// Workflow:
//   SELECT PROTOCOL → SELECT TEST → PROTOCOL CHECK
//   → RUN CAMPAIGN → DETECT FAILURE → INVESTIGATE
//   → REPLAY → MINIMIZE → REPORT → RESULT
//
// All operations are non-blocking.
// The ESP32 runs autonomously without a PC.
// ============================================

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

// Core
#include "core/types.h"
#include "core/prng.h"
#include "core/campaign.h"
#include "core/testcase.h"
#include "core/failure.h"
#include "core/result.h"

// Protocols
#include "protocols/uart_fuzzer.h"
#include "protocols/uart_parser.h"
#include "protocols/spi_fuzzer.h"
#include "protocols/i2c_fuzzer.h"

// Monitoring
#include "monitoring/heartbeat.h"
#include "monitoring/crash_detector.h"

// UI
#include "ui/oled_ui.h"
#include "ui/buttons.h"
#include "ui/indicators.h"

// Storage
#include "storage/failure_store.h"

// ============================================
// App State
// ============================================
enum AppState : uint8_t {
  APP_MENU,
  APP_PROTOCOL_CHECK,
  APP_FUZZING,
  APP_FAILURE_MENU,
  APP_REPLAYING,
  APP_MINIMIZING,
  APP_RESULT,
  APP_SERIAL_CMD
};

static AppState s_appState = APP_MENU;

// Menu navigation state
static TargetBoard  s_selectedBoard   = BOARD_STM32;
static ProtocolMode s_selectedProtocol = PROTO_UART;
static TestProfile  s_selectedProfile  = TEST_QUICK;
static uint32_t     s_customDurationMs = 30000;

// Replay state (non-blocking state machine)
enum ReplayStep : uint8_t {
  REP_WAIT_ALIVE = 0,   // Wait for DUT heartbeat to recover
  REP_SEND,             // Transmit the exact failing testcase
  REP_OBSERVE,          // Watch heartbeat/response for the oracle window
  REP_GAP,              // Cooldown between attempts
  REP_SHOW_RESULT,      // Display final classification briefly
  REP_FINISHED
};
static uint8_t  s_replayAttempts = 0;
static uint8_t  s_replayMaxAttempts = 3;
static uint8_t  s_replayFails = 0;
static bool     s_replayRunning = false;
static ReplayStep s_replayStep = REP_WAIT_ALIVE;
static uint32_t s_replayStepStartMs = 0;

// Minimizer state (non-blocking state machine)
enum MinimStep : uint8_t {
  MINIM_WAIT_ALIVE = 0, // Wait for DUT heartbeat before next probe
  MINIM_SEND,           // Transmit candidate packet
  MINIM_OBSERVE,        // Watch heartbeat for the oracle window
  MINIM_FINISHED
};
static uint32_t s_minimCurrentLen = 0;
static uint32_t s_minimBestLen = 0;
static uint32_t s_minimLow = 0;
static uint32_t s_minimHigh = 0;
static bool     s_minimRunning = false;
static MinimStep s_minimStep = MINIM_WAIT_ALIVE;
static uint32_t s_minimStepStartMs = 0;
static uint32_t s_minimTryLen = 0;

// Serial command buffer
static char s_serialBuf[128];
static uint8_t s_serialBufIdx = 0;

// ============================================
// Forward Declarations
// ============================================
static void handle_menu_input(ButtonAction btn);
static void handle_protocol_check_input(ButtonAction btn);
static void handle_failure_menu_input(ButtonAction btn);
static void handle_result_input(ButtonAction btn);
static void start_campaign(void);
static void run_fuzzing_cycle(void);
static void start_replay(void);
static void update_replay(void);
static void cancel_replay(const char* reason);
static void start_minimize(void);
static void update_minimize(void);
static void cancel_minimize(const char* reason);
static void process_serial_commands(void);

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================");
  Serial.printf("  AutoFuzzer v%s — Embedded Fuzzing Platform\n", AUTOFUZZER_VERSION);
  Serial.println("========================================\n");

  // Initialize all modules
  buttons_init();
  indicators_init();
  oled_ui_init();
  prng_init(0xC0DEC0DE);
  campaign_init();
  uart_fuzzer_init();
  uart_parser_init();
  heartbeat_init();
  crash_detector_init();
  failure_init();
  failure_store_init();

  // Self-test: brief visual/audio confirmation
  indicators_self_test();

  // Show main menu
  oled_ui_set_screen(SCREEN_MAIN_MENU);
  s_appState = APP_MENU;

  Serial.println("READY — Awaiting user input via buttons or serial commands.");
  Serial.println("Commands: START, STOP, PAUSE, STATUS, EXPORT, REPLAY, RESET\n");
}

// ============================================
// Main Loop
// ============================================
void loop() {
  // 1. Read buttons (non-blocking)
  ButtonAction btn = buttons_update();

  // 2. Read serial commands
  process_serial_commands();

  // 3. Update non-blocking indicators
  indicators_update();

  // 4. State machine
  switch (s_appState) {

    // ---- MENU ----
    case APP_MENU:
      handle_menu_input(btn);
      break;

    // ---- PROTOCOL CHECK ----
    case APP_PROTOCOL_CHECK:
      handle_protocol_check_input(btn);
      break;

    // ---- FUZZING ----
    case APP_FUZZING:
      run_fuzzing_cycle();
      // During pause, allow buttons
      if (!campaign_is_running()) {
        handle_menu_input(btn);
      }
      break;

    // ---- FAILURE MENU ----
    case APP_FAILURE_MENU:
      handle_failure_menu_input(btn);
      break;

    // ---- REPLAYING ----
    case APP_REPLAYING:
      update_replay();
      break;

    // ---- MINIMIZING ----
    case APP_MINIMIZING:
      update_minimize();
      break;

    // ---- RESULT ----
    case APP_RESULT:
      handle_result_input(btn);
      break;

    // ---- SERIAL COMMAND MODE ----
    case APP_SERIAL_CMD:
      // Handled via process_serial_commands above
      break;

    default:
      break;
  }

  // 5. Update OLED (non-blocking, 500ms refresh)
  oled_ui_update();
}

// ============================================
// Menu Input Handler
// ============================================
static void handle_menu_input(ButtonAction btn) {
  UiScreen screen = oled_ui_get_screen();

  switch (btn) {
    case BTN_UP:
      oled_ui_select_next();
      break;

    case BTN_DOWN:
      oled_ui_select_prev();
      break;

    case BTN_BACK:
      // Go back to main menu from any sub-menu
      if (screen != SCREEN_MAIN_MENU) {
        oled_ui_set_screen(SCREEN_MAIN_MENU);
      }
      break;

    case BTN_SELECT: {
      uint8_t sel = oled_ui_confirm();

      switch (screen) {
        case SCREEN_MAIN_MENU:
          switch (sel) {
            case 0:  // NEW TEST
              oled_ui_set_screen(SCREEN_BOARD_SELECT);
              break;
            case 1:  // RESULTS
              oled_ui_set_screen(SCREEN_RESULT);
              break;
            case 2:  // FAILURES
              oled_ui_set_screen(SCREEN_FAILURE_MENU);
              break;
            case 3:  // SETTINGS
              oled_ui_set_screen(SCREEN_SETTINGS);
              break;
            case 4:  // ABOUT
              oled_ui_set_screen(SCREEN_ABOUT);
              break;
          }
          break;

        case SCREEN_BOARD_SELECT:
          s_selectedBoard = (TargetBoard)sel;
          oled_ui_set_board(s_selectedBoard);
          oled_ui_set_screen(SCREEN_PROTOCOL_SELECT);
          break;

        case SCREEN_PROTOCOL_SELECT:
          s_selectedProtocol = (ProtocolMode)sel;
          if (sel == 3) {
            // CAN not yet implemented
            Serial.println("CAN not yet available. Select UART, SPI, or I2C.");
            break;
          }
          // Show wiring diagram for this board + protocol
          oled_ui_set_wiring_protocol(s_selectedProtocol);
          oled_ui_set_screen(SCREEN_WIRING_DIAGRAM);
          break;

        case SCREEN_WIRING_DIAGRAM:
          // SELECT pressed on wiring diagram = proceed to test select
          oled_ui_set_screen(SCREEN_TEST_SELECT);
          break;

        case SCREEN_TEST_SELECT:
          s_selectedProfile = (TestProfile)sel;
          if (sel == 3) {
            // Custom — use 30s default, future: input via serial
            s_customDurationMs = 30000;
          }
          oled_ui_set_screen(SCREEN_TEST_CONFIRM);
          break;

        case SCREEN_TEST_CONFIRM:
          if (sel == 0) {
            // YES — run protocol check, then campaign if DUT is alive
            start_campaign();
          } else {
            // NO — go back
            oled_ui_set_screen(SCREEN_MAIN_MENU);
          }
          break;

        default:
          break;
      }
      break;
    }

    default:
      break;
  }
}

// ============================================
// Protocol Check Input
//
// The campaign may ONLY start once the DUT proves it is
// alive with a real heartbeat edge. A silent/floating pin
// can never auto-start a test — this prevents the bug where
// a 30s campaign ran with nothing connected and printed PASS.
// ============================================
static void handle_protocol_check_input(ButtonAction btn) {
  bool hbAlive = heartbeat_update();

  // Auto-start as soon as real heartbeat evidence appears
  if (hbAlive && heartbeat_has_signal()) {
    oled_ui_set_screen(SCREEN_FUZZING);
    s_appState = APP_FUZZING;
    campaign_start(s_selectedProtocol, s_selectedProfile, s_customDurationMs);
    Serial.printf("PROTOCOL CHECK PASS — CAMPAIGN STARTED: %s protocol, %s profile\n",
                  ProtocolNames[s_selectedProtocol],
                  TestProfileNames[s_selectedProfile]);
    return;
  }

  // Only redraw the screen when the ready-state changes
  // (avoids I2C display spam every loop iteration)
  static int8_t s_lastShownReady = -1;
  int8_t ready = hbAlive ? 1 : 0;
  if (ready != s_lastShownReady) {
    s_lastShownReady = ready;
    oled_ui_set_selection(ready);
    oled_ui_redraw();
  }

  if (btn == BTN_BACK) {
    Serial.println("Protocol check cancelled by user.");
    oled_ui_set_screen(SCREEN_MAIN_MENU);
    s_appState = APP_MENU;
  }
}

// ============================================
// Start Campaign
//
// Initializes the fuzzer and enters PROTOCOL CHECK.
// Campaign start is deferred until the DUT proves it is
// alive (real heartbeat edges). No blind test runs.
// ============================================
static void start_campaign(void) {
  // Initialize the appropriate fuzzer
  switch (s_selectedProtocol) {
    case PROTO_UART:
      uart_fuzzer_init();
      uart_parser_init();
      break;
    case PROTO_SPI:
      spi_fuzzer_init();
      break;
    case PROTO_I2C:
      i2c_fuzzer_init();
      break;
    default:
      break;
  }

  // Forget any previous heartbeat evidence: this campaign must
  // prove the DUT is connected RIGHT NOW.
  heartbeat_reset();
  heartbeat_clear_signal();
  crash_detector_clear();

  oled_ui_set_screen(SCREEN_PROTOCOL_CHECK);
  s_appState = APP_PROTOCOL_CHECK;
  Serial.printf("Protocol check: %s — waiting for DUT heartbeat...\n",
                ProtocolNames[s_selectedProtocol]);
}

// ============================================
// Fuzzing Cycle
// ============================================
static void run_fuzzing_cycle(void) {
  // 1. Check if campaign is complete
  campaign_update();

  if (campaign_is_complete()) {
    if (s_selectedProtocol == PROTO_I2C) i2c_fuzzer_deinit();
    Serial.println("Campaign complete — calculating results.");
    indicators_led_pass_on();
    s_appState = APP_RESULT;
    oled_ui_set_screen(SCREEN_RESULT);
    return;
  }

  if (!campaign_is_running()) {
    // Campaign paused or stopped
    if (campaign_is_paused()) {
      return;  // Wait for resume
    }
    return;
  }

  // 2. Monitor heartbeat
  bool hbAlive = heartbeat_update();

  // If the DUT never showed a heartbeat and we're past the boot
  // window, abort instead of running the full campaign blind.
  // Verdict will be INCONCLUSIVE, never a false PASS.
  if (!heartbeat_has_signal() && campaign_get_elapsed_ms() > 3000) {
    if (s_selectedProtocol == PROTO_I2C) i2c_fuzzer_deinit();
    campaign_stop();
    Serial.println("ABORT: no DUT heartbeat detected during campaign — INCONCLUSIVE.");
    Serial.println("Check wiring (heartbeat pin) and that the DUT is powered.");
    indicators_led_pass_on();
    s_appState = APP_RESULT;
    oled_ui_set_screen(SCREEN_RESULT);
    return;
  }

  // 3. Monitor protocol responses
  // Track last mutation for adaptive statistics
  static MutationType s_lastMutation = MUT_VALID;

  if (s_selectedProtocol == PROTO_UART) {
    uart_parser_poll();

    if (uart_parser_has_response()) {
      const UartResponse* resp = uart_parser_get_response();

      bool isAck = (resp->status == Proto::StatusAck);
      bool isNack = !isAck;

      if (isAck) {
        campaign_record_ack();
      } else {
        campaign_record_nack();
      }

      // Record for adaptive mutation statistics
      campaign_record_mutation_response(s_lastMutation, isAck, isNack, false);

      Serial.printf("RESP seq=%u status=0x%02X %s\n",
                    resp->sequence, resp->status,
                    isAck ? "ACK" :
                    resp->status == Proto::StatusNackOverlen ? "NACK-OVERLEN" :
                    resp->status == Proto::StatusNackChecksum ? "NACK-CHECKSUM" : "UNKNOWN");

      uart_parser_clear_response();
    }
  } else if (s_selectedProtocol == PROTO_I2C) {
    // I2C ACK/NACK detection
    if (i2c_fuzzer_has_response()) {
      I2cResponse resp;
      if (i2c_fuzzer_get_response(&resp)) {
        bool isAck = (resp.writeResult == Proto::I2cRespSuccess);
        bool isNack = (resp.writeResult == Proto::I2cRespNackAddr ||
                       resp.writeResult == Proto::I2cRespNackData);
        bool isTimeout = !isAck && !isNack;

        if (isAck) {
          campaign_record_ack();
        } else if (isNack) {
          campaign_record_nack();
        } else {
          campaign_record_no_response();
        }

        // Record for adaptive mutation statistics
        campaign_record_mutation_response(s_lastMutation, isAck, isNack, isTimeout);
      }
      i2c_fuzzer_clear_response();
    }
  }

  // 4. Check for crash detection
  if (crash_detector_update()) {
    // Failure detected!
    if (s_selectedProtocol == PROTO_I2C) i2c_fuzzer_deinit();
    Serial.println("FAILURE DETECTED — campaign paused.");
    campaign_stop();
    indicators_set_pass(false);
    indicators_set_fail(true);
    indicators_beep_start(200);
    s_appState = APP_FAILURE_MENU;
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    oled_ui_redraw();

    // Store the failure
    const FailureRecord* rec = failure_get_current();
    if (rec) failure_store_add(rec);

    return;
  }

  // 5. Transmit packet
  static uint32_t lastPacketMs = 0;
  if (millis() - lastPacketMs >= Proto::PacketIntervalMs) {
    lastPacketMs = millis();

    // Select mutation based on campaign phase + adaptive stats
    MutationType mut = campaign_select_mutation();
    s_lastMutation = mut;
    crash_detector_set_last_mutation(mut);

    // Flash active LED
    indicators_set_active(true);

    switch (s_selectedProtocol) {
      case PROTO_UART:
        uart_fuzzer_send(mut);
        break;
      case PROTO_SPI:
        spi_fuzzer_send(mut);
        break;
      case PROTO_I2C:
        i2c_fuzzer_send(mut);
        break;
      default:
        break;
    }

    indicators_set_active(false);
  }

  // 6. Update indicators
  if (hbAlive) {
    indicators_set_pass(true);
    indicators_set_fail(false);
  } else {
    indicators_set_pass(false);
  }
}

// ============================================
// Failure Menu Input
// ============================================
static void handle_failure_menu_input(ButtonAction btn) {
  switch (btn) {
    case BTN_UP:
      oled_ui_select_next();
      break;

    case BTN_DOWN:
      oled_ui_select_prev();
      break;

    case BTN_BACK:
      oled_ui_set_screen(SCREEN_MAIN_MENU);
      s_appState = APP_MENU;
      break;

    case BTN_SELECT: {
      uint8_t sel = oled_ui_confirm();

      switch (sel) {
        case 0:  // REPLAY FAILURE
          start_replay();
          break;

        case 1:  // MINIMIZE
          start_minimize();
          break;

        case 2:  // REPORT
          failure_export_serial(failure_get_count() - 1);
          Serial.println("Report exported via serial.");
          break;

        case 3:  // TRY FIXING
          Serial.println("TRY FIX — PC companion required. Export data first.");
          Serial.println("Use serial command: EXPORT");
          break;

        case 4:  // VIEW DETAILS
          oled_ui_set_screen(SCREEN_FAILURE_DETAIL);
          break;

        case 5:  // SAVE & EXIT
          // Failure already stored. Export serial data.
          failure_store_export_all();
          oled_ui_set_screen(SCREEN_MAIN_MENU);
          s_appState = APP_MENU;
          break;
      }
      break;
    }

    default:
      break;
  }
}

// ============================================
// Replay Engine (non-blocking)
//
// Sends the exact failing testcase up to N times and
// watches the heartbeat oracle. Buttons stay responsive
// throughout — BACK cancels at any point.
// ============================================
static void start_replay(void) {
  const FailureRecord* rec = failure_get_current();
  if (!rec) {
    Serial.println("REPLAY: no failure record to replay.");
    return;
  }

  if (rec->protocol != PROTO_UART) {
    Serial.println("REPLAY: currently supports UART failures only.");
    Serial.println("(SPI/I2C replay requires protocol-aware replay — planned)");
    indicators_beep_start(100);
    return;
  }

  s_replayAttempts = 0;
  s_replayFails = 0;
  s_replayRunning = true;
  s_replayStep = REP_WAIT_ALIVE;
  s_replayStepStartMs = millis();

  oled_ui_set_screen(SCREEN_REPLAY);
  s_appState = APP_REPLAYING;

  Serial.println("REPLAY START — waiting for DUT heartbeat to recover...");
  heartbeat_reset();
  crash_detector_clear();
}

static void cancel_replay(const char* reason) {
  s_replayRunning = false;
  Serial.printf("REPLAY cancelled: %s\n", reason);
  oled_ui_set_screen(SCREEN_FAILURE_MENU);
  s_appState = APP_FAILURE_MENU;
}

static void finish_replay(void) {
  s_replayRunning = false;

  // Determine reproducibility
  FailureStatus status;
  if (s_replayFails == s_replayAttempts && s_replayFails > 0) {
    status = STATUS_REPRODUCIBLE;
  } else if (s_replayFails > 0) {
    status = STATUS_INTERMITTENT;
  } else {
    status = STATUS_NOT_REPRODUCIBLE;
  }

  failure_update_status(status, s_replayAttempts, s_replayFails);

  Serial.printf("REPLAY RESULT: %u/%u failed — %s\n",
                s_replayFails, s_replayAttempts,
                FailureStatusNames[status]);

  indicators_beep_start(150);
  s_replayStep = REP_SHOW_RESULT;
  s_replayStepStartMs = millis();
  oled_ui_redraw();
}

static void update_replay(void) {
  if (!s_replayRunning) {
    // Show-result pause, then return to the failure menu
    if (s_replayStep == REP_SHOW_RESULT &&
        millis() - s_replayStepStartMs >= 2000) {
      oled_ui_set_screen(SCREEN_FAILURE_MENU);
      s_appState = APP_FAILURE_MENU;
    }
    return;
  }

  // BACK cancels replay at any point
  ButtonAction btn = buttons_update();
  if (btn == BTN_BACK) {
    cancel_replay("user pressed BACK");
    return;
  }

  heartbeat_update();

  switch (s_replayStep) {

    case REP_WAIT_ALIVE: {
      // Give the DUT up to 5s to come back after the crash
      if (heartbeat_is_alive()) {
        s_replayStep = REP_SEND;
        s_replayStepStartMs = millis();
      } else if (millis() - s_replayStepStartMs > 5000) {
        Serial.println("REPLAY: DUT did not recover — aborting replay.");
        s_replayAttempts = 1;  // Record the aborted attempt honestly
        s_replayFails = 0;
        finish_replay();
      }
      break;
    }

    case REP_SEND: {
      const FailureRecord* rec = failure_get_current();
      if (!rec) {
        cancel_replay("failure record lost");
        return;
      }

      s_replayAttempts++;
      Serial.printf("REPLAY #%u/%u — seq=%u, mutation=%s, len=%u\n",
                    s_replayAttempts, s_replayMaxAttempts,
                    rec->testcase.sequence,
                    MutationNames[rec->testcase.mutation],
                    rec->testcase.packetLen);

      // Send the exact same packet
      uart_fuzzer_replay(&rec->testcase);
      s_replayStep = REP_OBSERVE;
      s_replayStepStartMs = millis();
      break;
    }

    case REP_OBSERVE: {
      uart_parser_poll();

      // Oracle: heartbeat lost within the observation window
      if (!heartbeat_is_alive() && heartbeat_has_signal()) {
        s_replayFails++;
        Serial.printf("REPLAY #%u: HEARTBEAT LOST — FAIL\n", s_replayAttempts);
        crash_detector_clear();  // includes heartbeat_reset
        s_replayStep = REP_GAP;
        s_replayStepStartMs = millis();
      } else if (millis() - s_replayStepStartMs >= 500) {
        Serial.printf("REPLAY #%u: HEARTBEAT OK — PASS\n", s_replayAttempts);
        s_replayStep = REP_GAP;
        s_replayStepStartMs = millis();
      }
      break;
    }

    case REP_GAP: {
      if (millis() - s_replayStepStartMs >= 300) {
        if (s_replayAttempts >= s_replayMaxAttempts) {
          finish_replay();
        } else {
          s_replayStep = REP_WAIT_ALIVE;
          s_replayStepStartMs = millis();
        }
      }
      break;
    }

    default:
      break;
  }

  oled_ui_redraw();
}

// ============================================
// Minimizer (non-blocking, protocol-aware)
//
// Binary-searches the smallest packet length that still
// reproduces the failure. Buttons stay responsive —
// BACK cancels at any point.
// ============================================
static void start_minimize(void) {
  const FailureRecord* rec = failure_get_current();
  if (!rec) {
    Serial.println("MINIMIZE: no failure record.");
    return;
  }

  if (rec->protocol != PROTO_UART) {
    Serial.println("MINIMIZE: currently supports UART failures only.");
    indicators_beep_start(100);
    return;
  }

  s_minimCurrentLen = rec->testcase.packetLen;
  s_minimBestLen = rec->testcase.packetLen;
  s_minimLow = Proto::UartFrameOverhead + 1;  // Minimum viable packet
  s_minimHigh = rec->testcase.packetLen;      // Original length
  s_minimRunning = true;
  s_minimStep = MINIM_WAIT_ALIVE;
  s_minimStepStartMs = millis();

  oled_ui_set_screen(SCREEN_MINIMIZING);
  s_appState = APP_MINIMIZING;

  Serial.printf("MINIMIZE START — original %u bytes, binary search %u-%u\n",
                s_minimCurrentLen, s_minimLow, s_minimHigh);
  Serial.println("(BACK cancels minimization)");
}

static void cancel_minimize(const char* reason) {
  s_minimRunning = false;
  const FailureRecord* rec = failure_get_current();
  if (rec) failure_update_minimized(s_minimBestLen);
  Serial.printf("MINIMIZE cancelled: %s\n", reason);
  oled_ui_set_screen(SCREEN_FAILURE_MENU);
  s_appState = APP_FAILURE_MENU;
}

static void finish_minimize(void) {
  s_minimRunning = false;
  const FailureRecord* rec = failure_get_current();
  if (rec) failure_update_minimized(s_minimBestLen);

  Serial.printf("MINIMIZE COMPLETE: %u -> %u bytes\n",
                rec ? rec->testcase.packetLen : 0, s_minimBestLen);

  indicators_beep_start(100);
  oled_ui_set_screen(SCREEN_FAILURE_MENU);
  s_appState = APP_FAILURE_MENU;
}

static void update_minimize(void) {
  if (!s_minimRunning) return;

  // BACK cancels minimization at any point
  ButtonAction btn = buttons_update();
  if (btn == BTN_BACK) {
    cancel_minimize("user pressed BACK");
    return;
  }

  const FailureRecord* rec = failure_get_current();
  if (!rec) {
    s_minimRunning = false;
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
    return;
  }

  heartbeat_update();

  switch (s_minimStep) {

    case MINIM_WAIT_ALIVE: {
      if (heartbeat_is_alive()) {
        // Converged?
        if (s_minimHigh <= s_minimLow + 1) {
          finish_minimize();
          return;
        }
        s_minimTryLen = (s_minimLow + s_minimHigh) / 2;
        Serial.printf("MINIMIZE: trying %u bytes (range %u-%u)...\n",
                      s_minimTryLen, s_minimLow, s_minimHigh);
        s_minimStep = MINIM_SEND;
        s_minimStepStartMs = millis();
      } else if (millis() - s_minimStepStartMs > 2000) {
        Serial.println("MINIMIZE: DUT not responding, stopping minimization.");
        finish_minimize();
      }
      break;
    }

    case MINIM_SEND: {
      // Build a minimized UART packet: same mutation effect,
      // shorter payload
      uint8_t buffer[64];
      buffer[0] = Proto::UartSync;
      buffer[1] = Proto::UartCmd;
      uint8_t payLen = (uint8_t)(s_minimTryLen - Proto::UartFrameOverhead);
      buffer[2] = payLen;
      buffer[3] = 0x00;  // Seq low
      buffer[4] = 0x00;  // Seq high

      uint8_t checksum = buffer[1] ^ buffer[2] ^ buffer[3] ^ buffer[4];
      for (uint8_t i = 0; i < payLen; i++) {
        buffer[5 + i] = (uint8_t)(i & 0xFF);
        checksum ^= buffer[5 + i];
      }

      // Preserve the mutation's effect
      if (rec->testcase.mutation == MUT_BAD_CRC) {
        checksum ^= 0xFF;  // Keep the bad CRC
      } else if (rec->testcase.mutation == MUT_MALFORMED_HEADER) {
        buffer[0] = 0xFF;  // Keep corrupted sync
      } else if (rec->testcase.mutation == MUT_INVALID_LENGTH) {
        buffer[2] = 0xFF;  // Keep invalid length
      }

      buffer[5 + payLen] = checksum;

      // Send
      Serial2.write(buffer, (uint8_t)(s_minimTryLen));

      s_minimStep = MINIM_OBSERVE;
      s_minimStepStartMs = millis();
      break;
    }

    case MINIM_OBSERVE: {
      if (!heartbeat_is_alive() && heartbeat_has_signal()) {
        // Shorter length also fails — keep it, try even smaller
        s_minimBestLen = s_minimTryLen;
        s_minimLow = s_minimTryLen;
        Serial.printf("MINIMIZE: %u bytes -> STILL FAILS\n", s_minimTryLen);
        crash_detector_clear();
        s_minimStep = MINIM_WAIT_ALIVE;
        s_minimStepStartMs = millis();
      } else if (millis() - s_minimStepStartMs >= 500) {
        // Shorter length passes — minimal failure is larger
        s_minimHigh = s_minimTryLen;
        Serial.printf("MINIMIZE: %u bytes -> PASSES\n", s_minimTryLen);
        s_minimStep = MINIM_WAIT_ALIVE;
        s_minimStepStartMs = millis();
      }
      break;
    }

    default:
      break;
  }

  oled_ui_redraw();
}

// ============================================
// Result Input
// ============================================
static void handle_result_input(ButtonAction btn) {
  switch (btn) {
    case BTN_UP:
      oled_ui_select_next();
      break;

    case BTN_DOWN:
      oled_ui_select_prev();
      break;

    case BTN_BACK:
      oled_ui_set_screen(SCREEN_MAIN_MENU);
      s_appState = APP_MENU;
      break;

    case BTN_SELECT: {
      uint8_t sel = oled_ui_confirm();
      switch (sel) {
        case 0:  // VIEW RESULT
          result_print_report();
          break;
        case 1:  // VIEW FAILURES
          if (failure_get_count() > 0) {
            oled_ui_set_screen(SCREEN_FAILURE_MENU);
          }
          break;
        case 2:  // REPLAY FAILURE
          if (failure_get_count() > 0) {
            start_replay();
          }
          break;
        case 3:  // NEW TEST
          oled_ui_set_screen(SCREEN_MAIN_MENU);
          s_appState = APP_MENU;
          break;
      }
      break;
    }

    default:
      break;
  }
}

// ============================================
// Serial Command Processing
// ============================================
static void process_serial_commands(void) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (s_serialBufIdx > 0) {
        s_serialBuf[s_serialBufIdx] = '\0';

        // Convert to uppercase for command matching
        for (uint8_t i = 0; i < s_serialBufIdx; i++) {
          if (s_serialBuf[i] >= 'a' && s_serialBuf[i] <= 'z') {
            s_serialBuf[i] -= 32;
          }
        }

        // Process commands
        if (strcmp(s_serialBuf, "START") == 0) {
          if (s_appState == APP_MENU) {
            start_campaign();
          } else {
            Serial.println("Cannot start now — finish or stop the current activity first.");
          }
        } else if (strcmp(s_serialBuf, "STOP") == 0) {
          if (s_selectedProtocol == PROTO_I2C) i2c_fuzzer_deinit();
          campaign_stop();
          s_replayRunning = false;
          s_minimRunning = false;
          crash_detector_clear();
          s_appState = APP_MENU;
          oled_ui_set_screen(SCREEN_MAIN_MENU);
        } else if (strcmp(s_serialBuf, "PAUSE") == 0) {
          campaign_toggle_pause();
        } else if (strcmp(s_serialBuf, "STATUS") == 0) {
          Serial.printf("State: %s\n", s_appState == APP_FUZZING ? "FUZZING" : "IDLE");
          Serial.printf("Protocol: %s\n", ProtocolNames[s_selectedProtocol]);
          Serial.printf("Packets: %u\n", campaign_get_packet_count());
          Serial.printf("Heartbeat: %s%s\n",
                        heartbeat_is_alive() ? "OK" : "LOST",
                        heartbeat_has_signal() ? "" : " (NO SIGNAL — DUT not connected?)");
          const CampaignStats* stats = campaign_get_stats();
          Serial.printf("ACKs: %u, NACKs: %u, Timeouts: %u\n",
                        stats->totalAcks, stats->totalNacks, stats->totalNoResponse);
          if (s_selectedProtocol == PROTO_I2C) {
            Serial.printf("I2C bus: ACKs=%u NACKs=%u Errors=%u\n",
                          i2c_fuzzer_get_acks(), i2c_fuzzer_get_nacks(), i2c_fuzzer_get_errors());
          }
        } else if (strcmp(s_serialBuf, "EXPORT") == 0) {
          failure_store_export_all();
        } else if (strcmp(s_serialBuf, "REPLAY") == 0) {
          if (failure_get_count() > 0) {
            start_replay();
          } else {
            Serial.println("No failures to replay.");
          }
        } else if (strcmp(s_serialBuf, "RESET") == 0) {
          campaign_init();
          failure_init();
          failure_store_init();
          s_replayRunning = false;
          s_minimRunning = false;
          crash_detector_clear();
          s_appState = APP_MENU;
          oled_ui_set_screen(SCREEN_MAIN_MENU);
          Serial.println("All state reset.");
        } else if (strcmp(s_serialBuf, "HELP") == 0) {
          Serial.println("Commands:");
          Serial.println("  START   — Start test campaign (waits for DUT heartbeat)");
          Serial.println("  STOP    — Stop current activity");
          Serial.println("  PAUSE   — Pause/resume campaign");
          Serial.println("  STATUS  — Show current status");
          Serial.println("  EXPORT  — Export all failures");
          Serial.println("  REPLAY  — Replay last failure");
          Serial.println("  RESET   — Reset all state");
          Serial.println("  RESULT  — Print test result report");
          Serial.println("  SIMFAIL — Simulate failure (debug)");
          Serial.println("  I2CSTAT  — Show I2C bus statistics");
          Serial.println("  I2CSCAN  — Scan I2C bus for devices");
          Serial.println("  HBRAW    — Raw heartbeat pin diagnostic (2s sample)");
          Serial.println("  SETUART  — Set protocol to UART");
          Serial.println("  SETSPI   — Set protocol to SPI");
          Serial.println("  SETI2C   — Set protocol to I2C");
          Serial.println("  HELP     — Show this help");
        } else if (strcmp(s_serialBuf, "I2CSTAT") == 0) {
          Serial.println("--- I2C Bus Statistics ---");
          Serial.printf("Bus: SDA=%d SCL=%d (Wire1)\n", Pin::I2cSda, Pin::I2cScl);
          Serial.printf("DUT address: 0x%02X\n", Proto::I2cDefaultAddr);
          Serial.printf("Total ACKs:  %u\n", i2c_fuzzer_get_acks());
          Serial.printf("Total NACKs: %u\n", i2c_fuzzer_get_nacks());
          Serial.printf("Total Errors: %u\n", i2c_fuzzer_get_errors());
        } else if (strcmp(s_serialBuf, "I2CSCAN") == 0) {
          // Quick I2C bus scan to find devices
          Serial.println("--- I2C Bus Scan ---");
          i2c_fuzzer_init();  // Ensure Wire1 is running
          i2c_fuzzer_scan();
        } else if (strcmp(s_serialBuf, "SETUART") == 0) {
          s_selectedProtocol = PROTO_UART;
          Serial.println("Protocol set to UART.");
        } else if (strcmp(s_serialBuf, "SETSPI") == 0) {
          s_selectedProtocol = PROTO_SPI;
          spi_fuzzer_init();
          Serial.println("Protocol set to SPI. SPI bus initialized.");
        } else if (strcmp(s_serialBuf, "SETI2C") == 0) {
          s_selectedProtocol = PROTO_I2C;
          i2c_fuzzer_init();
          Serial.println("Protocol set to I2C. I2C bus initialized.");
        } else if (strcmp(s_serialBuf, "SIMFAIL") == 0) {
          Serial.println("DEBUG: Simulating heartbeat timeout failure...");
          static TestcaseMeta fakeTC;
          memset(&fakeTC, 0, sizeof(TestcaseMeta));
          fakeTC.sequence = 999;
          fakeTC.mutation = MUT_OVERLENGTH;
          fakeTC.packetLen = 51;
          fakeTC.payloadLen = 45;
          fakeTC.packetBytes[0] = 0xA5;
          fakeTC.packetBytes[1] = 0x01;
          fakeTC.packetBytes[2] = 45;
          fakeTC.packetBytes[3] = 0xE7;
          fakeTC.packetBytes[4] = 0x03;
          fakeTC.packetBytes[5] = 0xDE;
          fakeTC.packetBytes[6] = 0xAD;
          fakeTC.timestampMs = millis();
          failure_freeze(FAIL_HEARTBEAT_TIMEOUT, &fakeTC, 400, false, 0, 0);
          indicators_set_fail(true);
          indicators_beep_start(200);
          s_appState = APP_FAILURE_MENU;
          oled_ui_set_screen(SCREEN_FAILURE_MENU);
          oled_ui_redraw();
          const FailureRecord* rec = failure_get_current();
          if (rec) failure_store_add(rec);
          Serial.println("DEBUG: Failure frozen. Use REPLAY or EXPORT to test.");
        } else if (strcmp(s_serialBuf, "HBRAW") == 0) {
          // Diagnostic: sample the raw heartbeat pin for 2 seconds.
          // Distinguishes 'wire not connected' from 'monitor logic bug'.
          Serial.printf("HBRAW: sampling GPIO %d for 2s...\n", Pin::Heartbeat);
          uint32_t startMs = millis();
          bool lastLvl = digitalRead(Pin::Heartbeat);
          uint32_t edges = 0;
          bool sawHigh = lastLvl;
          bool sawLow  = !lastLvl;
          while (millis() - startMs < 2000) {
            bool lvl = digitalRead(Pin::Heartbeat);
            if (lvl != lastLvl) {
              edges++;
              lastLvl = lvl;
            }
            if (lvl) sawHigh = true; else sawLow = true;
          }
          Serial.printf("HBRAW: edges=%u levels=%s%s\n",
                        edges,
                        sawHigh ? "HIGH " : "", sawLow ? "LOW" : "");
          if (edges == 0 && !sawHigh) {
            Serial.println("HBRAW: pin stuck LOW — heartbeat wire likely NOT connected");
            Serial.println("       Check DUT HB pin -> ESP32 GPIO25, and common GND!");
          } else if (edges == 0 && sawHigh) {
            Serial.println("HBRAW: pin stuck HIGH — wire connected but DUT not toggling");
          } else {
            Serial.println("HBRAW: signal present — heartbeat monitor should work");
          }
        } else if (strcmp(s_serialBuf, "RESULT") == 0) {
          result_calculate();
          result_print_report();
        } else {
          Serial.printf("Unknown command: %s (type HELP for list)\n", s_serialBuf);
        }

        s_serialBufIdx = 0;
      }
    } else if (s_serialBufIdx < sizeof(s_serialBuf) - 1) {
      s_serialBuf[s_serialBufIdx++] = c;
    }
  }
}
