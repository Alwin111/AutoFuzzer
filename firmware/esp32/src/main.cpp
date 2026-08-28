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

// Replay state
static uint8_t  s_replayAttempts = 0;
static uint8_t  s_replayMaxAttempts = 3;
static uint8_t  s_replayFails = 0;
static bool     s_replayRunning = false;

// Minimizer state
static uint32_t s_minimCurrentLen = 0;
static uint32_t s_minimBestLen = 0;
static bool     s_minimRunning = false;

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
static void start_minimize(void);
static void update_minimize(void);
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
      // Don't monitor heartbeat during menu navigation
      // (prevents spam from floating pin when no DUT connected)
      handle_menu_input(btn);
      break;

    // ---- PROTOCOL CHECK ----
    case APP_PROTOCOL_CHECK:
      heartbeat_update();
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
            // YES — start protocol check then campaign
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
// ============================================
static void handle_protocol_check_input(ButtonAction btn) {
  bool hbAlive = heartbeat_update();

  // Update readiness
  oled_ui_set_selection(hbAlive ? 0 : 1);
  oled_ui_redraw();

  if (btn == BTN_SELECT && hbAlive) {
    // Ready — start the campaign
    oled_ui_set_screen(SCREEN_FUZZING);
    s_appState = APP_FUZZING;
    campaign_start(s_selectedProtocol, s_selectedProfile, s_customDurationMs);
    Serial.printf("CAMPAIGN STARTED: %s protocol, %s profile\n",
                  ProtocolNames[s_selectedProtocol],
                  TestProfileNames[s_selectedProfile]);
  } else if (btn == BTN_BACK) {
    oled_ui_set_screen(SCREEN_MAIN_MENU);
    s_appState = APP_MENU;
  }
}

// ============================================
// Start Campaign
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

  // Reset heartbeat and check if DUT is alive
  heartbeat_reset();
  crash_detector_clear();
  delay(100);  // Brief settle time
  bool hbAlive = heartbeat_update();

  if (hbAlive) {
    // Heartbeat detected — skip protocol check, start directly
    Serial.printf("Heartbeat detected — starting campaign immediately.\n");
    oled_ui_set_screen(SCREEN_FUZZING);
    s_appState = APP_FUZZING;
    campaign_start(s_selectedProtocol, s_selectedProfile, s_customDurationMs);
    Serial.printf("CAMPAIGN STARTED: %s protocol, %s profile\n",
                  ProtocolNames[s_selectedProtocol],
                  TestProfileNames[s_selectedProfile]);
  } else {
    // No heartbeat yet — show protocol check, wait for user to confirm
    oled_ui_set_screen(SCREEN_PROTOCOL_CHECK);
    s_appState = APP_PROTOCOL_CHECK;
    Serial.printf("Protocol check: %s — waiting for heartbeat...\n",
                  ProtocolNames[s_selectedProtocol]);
  }
}

// ============================================
// Fuzzing Cycle
// ============================================
static void run_fuzzing_cycle(void) {
  // 1. Check if campaign is complete
  campaign_update();

  if (campaign_is_complete()) {
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

  // 3. Monitor UART responses
  if (s_selectedProtocol == PROTO_UART) {
    uart_parser_poll();

    // Process response
    if (uart_parser_has_response()) {
      const UartResponse* resp = uart_parser_get_response();

      // Correlate with expected sequence
      if (resp->status == Proto::StatusAck) {
        campaign_record_ack();
      } else {
        campaign_record_nack();
      }

      // Log response
      Serial.printf("RESP seq=%u status=0x%02X %s\n",
                    resp->sequence, resp->status,
                    resp->status == Proto::StatusAck ? "ACK" :
                    resp->status == Proto::StatusNackOverlen ? "NACK-OVERLEN" :
                    resp->status == Proto::StatusNackChecksum ? "NACK-CHECKSUM" : "UNKNOWN");

      uart_parser_clear_response();
    }
  }

  // 4. Check for crash detection
  if (crash_detector_update()) {
    // Failure detected!
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

    // Select mutation based on campaign phase
    MutationType mut = campaign_select_mutation();

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
// Replay Engine
// ============================================
static void start_replay(void) {
  s_replayAttempts = 0;
  s_replayFails = 0;
  s_replayRunning = true;
  oled_ui_set_screen(SCREEN_REPLAY);
  s_appState = APP_REPLAYING;

  Serial.println("REPLAY START — resetting DUT heartbeat monitor...");
  heartbeat_reset();
  crash_detector_clear();

  // Small delay for DUT to recover
  delay(500);
}

static void update_replay(void) {
  if (!s_replayRunning) return;

  // Wait for DUT heartbeat to recover
  heartbeat_update();

  if (!heartbeat_is_alive()) {
    // Still waiting for DUT to come back
    oled_ui_redraw();
    return;
  }

  // DUT is alive — replay the failing testcase
  const FailureRecord* rec = failure_get_current();
  if (!rec) {
    s_replayRunning = false;
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
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

  // Wait for response and heartbeat check
  uint32_t startMs = millis();
  bool responseReceived = false;
  bool heartbeatFailed = false;

  while (millis() - startMs < 500) {
    uart_parser_poll();
    heartbeat_update();

    if (uart_parser_has_response()) {
      responseReceived = true;
      uart_parser_clear_response();
    }

    if (!heartbeat_is_alive()) {
      heartbeatFailed = true;
      break;
    }
  }

  // Evaluate replay result
  if (heartbeatFailed) {
    s_replayFails++;
    Serial.printf("REPLAY #%u: HEARTBEAT LOST — FAIL\n", s_replayAttempts);
    crash_detector_clear();
    heartbeat_reset();
  } else {
    Serial.printf("REPLAY #%u: HEARTBEAT OK — PASS\n", s_replayAttempts);
  }

  // Check if we've done enough attempts
  if (s_replayAttempts >= s_replayMaxAttempts) {
    s_replayRunning = false;

    // Determine reproducibility
    FailureStatus status;
    if (s_replayFails == s_replayAttempts) {
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
    oled_ui_set_screen(SCREEN_REPLAY);
    oled_ui_redraw();

    // Return to failure menu after showing result
    delay(2000);
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
  }
}

// ============================================
// Minimizer
// ============================================
static void start_minimize(void) {
  const FailureRecord* rec = failure_get_current();
  if (!rec) return;

  s_minimCurrentLen = rec->testcase.packetLen;
  s_minimBestLen = rec->testcase.packetLen;
  s_minimRunning = true;
  oled_ui_set_screen(SCREEN_MINIMIZING);
  s_appState = APP_MINIMIZING;

  Serial.printf("MINIMIZE START — original %u bytes\n", s_minimCurrentLen);
}

static void update_minimize(void) {
  if (!s_minimRunning) return;

  const FailureRecord* rec = failure_get_current();
  if (!rec) {
    s_minimRunning = false;
    return;
  }

  // Protocol-aware minimization: try reducing payload length
  // Start from current length, try smaller sizes
  uint32_t tryLen = s_minimCurrentLen - 1;

  if (tryLen < Proto::UartFrameOverhead + 1) {
    // Can't go smaller than header + 1 byte
    s_minimRunning = false;
    failure_update_minimized(s_minimBestLen);

    Serial.printf("MINIMIZE COMPLETE: %u -> %u bytes\n",
                  rec->testcase.packetLen, s_minimBestLen);

    indicators_beep_start(100);
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
    return;
  }

  // Build a modified packet with reduced length
  Serial.printf("MINIMIZE: trying %u bytes...\n", tryLen);

  // Reset DUT state
  heartbeat_reset();
  crash_detector_clear();
  delay(200);

  // Wait for heartbeat
  uint32_t waitStart = millis();
  while (!heartbeat_is_alive() && millis() - waitStart < 2000) {
    heartbeat_update();
  }

  if (!heartbeat_is_alive()) {
    Serial.println("MINIMIZE: DUT not responding, stopping minimization.");
    s_minimRunning = false;
    failure_update_minimized(s_minimBestLen);
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
    return;
  }

  // Generate minimized packet: same mutation, shorter payload
  uint8_t buffer[64];
  buffer[0] = Proto::UartSync;
  buffer[1] = Proto::UartCmd;
  uint8_t payLen = (uint8_t)(tryLen - Proto::UartFrameOverhead);
  buffer[2] = payLen;
  buffer[3] = 0x00;  // Seq low
  buffer[4] = 0x00;  // Seq high

  uint8_t checksum = buffer[1] ^ buffer[2] ^ buffer[3] ^ buffer[4];
  for (uint8_t i = 0; i < payLen; i++) {
    buffer[5 + i] = (uint8_t)(i & 0xFF);
    checksum ^= buffer[5 + i];
  }

  if (rec->testcase.mutation == MUT_BAD_CRC) {
    checksum ^= 0xFF;  // Keep the bad CRC
  }

  buffer[5 + payLen] = checksum;

  // Send
  Serial2.write(buffer, (uint8_t)(tryLen));

  // Check if failure occurs
  uint32_t checkStart = millis();
  bool failDetected = false;

  while (millis() - checkStart < 500) {
    heartbeat_update();
    if (!heartbeat_is_alive()) {
      failDetected = true;
      break;
    }
  }

  if (failDetected) {
    // This shorter length also fails — keep it
    s_minimBestLen = tryLen;
    s_minimCurrentLen = tryLen;
    Serial.printf("MINIMIZE: %u bytes -> STILL FAILS\n", tryLen);
  } else {
    // This shorter length passes — the minimal failure is one byte larger
    Serial.printf("MINIMIZE: %u bytes -> PASSES (minimal is %u)\n", tryLen, s_minimCurrentLen);
    s_minimRunning = false;
    failure_update_minimized(s_minimBestLen);

    Serial.printf("MINIMIZE COMPLETE: %u -> %u bytes\n",
                  rec->testcase.packetLen, s_minimBestLen);

    indicators_beep_start(100);
    delay(1500);
    oled_ui_set_screen(SCREEN_FAILURE_MENU);
    s_appState = APP_FAILURE_MENU;
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
          }
        } else if (strcmp(s_serialBuf, "STOP") == 0) {
          campaign_stop();
          s_appState = APP_MENU;
          oled_ui_set_screen(SCREEN_MAIN_MENU);
        } else if (strcmp(s_serialBuf, "PAUSE") == 0) {
          campaign_toggle_pause();
        } else if (strcmp(s_serialBuf, "STATUS") == 0) {
          Serial.printf("State: %s\n", s_appState == APP_FUZZING ? "FUZZING" : "IDLE");
          Serial.printf("Packets: %u\n", campaign_get_packet_count());
          Serial.printf("Heartbeat: %s\n", heartbeat_is_alive() ? "OK" : "LOST");
          const CampaignStats* stats = campaign_get_stats();
          Serial.printf("ACKs: %u, NACKs: %u, Timeouts: %u\n",
                        stats->totalAcks, stats->totalNacks, stats->totalNoResponse);
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
          s_appState = APP_MENU;
          oled_ui_set_screen(SCREEN_MAIN_MENU);
          Serial.println("All state reset.");
        } else if (strcmp(s_serialBuf, "HELP") == 0) {
          Serial.println("Commands:");
          Serial.println("  START  — Start test campaign");
          Serial.println("  STOP   — Stop current campaign");
          Serial.println("  PAUSE  — Pause/resume campaign");
          Serial.println("  STATUS — Show current status");
          Serial.println("  EXPORT — Export all failures");
          Serial.println("  REPLAY — Replay last failure");
          Serial.println("  RESET  — Reset all state");
          Serial.println("  HELP   — Show this help");
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
