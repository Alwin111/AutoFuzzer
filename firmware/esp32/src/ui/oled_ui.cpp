#include "oled_ui.h"
#include "../core/campaign.h"
#include "../core/failure.h"
#include "../core/result.h"
#include "../monitoring/heartbeat.h"

// ============================================
// Display Object
// ============================================
static Adafruit_SSD1306 s_display(Display::Width, Display::Height, &Wire, -1);

// ============================================
// UI State
// ============================================
static UiScreen s_currentScreen = SCREEN_MAIN_MENU;
static uint8_t  s_selection     = 0;  // Current menu selection
static bool     s_oledOn        = true;
static uint32_t s_lastRefreshMs = 0;
static const uint32_t kRefreshIntervalMs = 500;

// ============================================
// Menu Definitions
// ============================================
struct MenuItem {
  const char* name;
};

// Main menu items
static const MenuItem kMainMenu[] = {
  { "NEW TEST" },
  { "RESULTS" },
  { "FAILURES" },
  { "SETTINGS" },
  { "ABOUT" }
};
static const uint8_t kMainMenuCount = 5;

// Board select items
static const MenuItem kBoardMenu[] = {
  { "STM32 NUCLEO" },
  { "ESP32" },
  { "ARDUINO NANO" }
};
static const uint8_t kBoardMenuCount = 3;

// Protocol select items
static const MenuItem kProtocolMenu[] = {
  { "UART" },
  { "SPI" },
  { "I2C" },
  { "CAN (future)" }
};
static const uint8_t kProtocolMenuCount = 4;

// Test profile items
static const MenuItem kTestMenu[] = {
  { "QUICK 30 SEC" },
  { "STANDARD 1 MIN" },
  { "DEEP 5 MIN" },
  { "CUSTOM" }
};
static const uint8_t kTestMenuCount = 4;

// Confirm items
static const MenuItem kConfirmMenu[] = {
  { "YES" },
  { "NO" }
};
static const uint8_t kConfirmMenuCount = 2;

// Failure menu items
static const MenuItem kFailureMenu[] = {
  { "REPLAY FAILURE" },
  { "MINIMIZE" },
  { "REPORT" },
  { "TRY FIXING" },
  { "VIEW DETAILS" },
  { "SAVE & EXIT" }
};
static const uint8_t kFailureMenuCount = 6;

// Result menu items
static const MenuItem kResultMenu[] = {
  { "VIEW RESULT" },
  { "VIEW FAILURES" },
  { "REPLAY FAILURE" },
  { "NEW TEST" }
};
static const uint8_t kResultMenuCount = 4;

// ============================================
// Get max items for current screen
// ============================================
uint8_t oled_ui_get_max_items(void) {
  switch (s_currentScreen) {
    case SCREEN_MAIN_MENU:       return kMainMenuCount;
    case SCREEN_BOARD_SELECT:    return kBoardMenuCount;
    case SCREEN_PROTOCOL_SELECT: return kProtocolMenuCount;
    case SCREEN_TEST_SELECT:     return kTestMenuCount;
    case SCREEN_TEST_CONFIRM:    return kConfirmMenuCount;
    case SCREEN_FAILURE_MENU:    return kFailureMenuCount;
    case SCREEN_RESULT:          return kResultMenuCount;
    default: return 0;
  }
}

// ============================================
// Get item name
// ============================================
const char* oled_ui_get_item_name(UiScreen screen, uint8_t idx) {
  switch (screen) {
    case SCREEN_MAIN_MENU: {
      if (idx < kMainMenuCount) return kMainMenu[idx].name;
      break;
    }
    case SCREEN_BOARD_SELECT: {
      if (idx < kBoardMenuCount) return kBoardMenu[idx].name;
      break;
    }
    case SCREEN_PROTOCOL_SELECT: {
      if (idx < kProtocolMenuCount) return kProtocolMenu[idx].name;
      break;
    }
    case SCREEN_TEST_SELECT: {
      if (idx < kTestMenuCount) return kTestMenu[idx].name;
      break;
    }
    case SCREEN_TEST_CONFIRM: {
      if (idx < kConfirmMenuCount) return kConfirmMenu[idx].name;
      break;
    }
    case SCREEN_FAILURE_MENU: {
      if (idx < kFailureMenuCount) return kFailureMenu[idx].name;
      break;
    }
    case SCREEN_RESULT: {
      if (idx < kResultMenuCount) return kResultMenu[idx].name;
      break;
    }
    default: break;
  }
  return "";
}

// ============================================
// Init
// ============================================
void oled_ui_init(void) {
  Wire.begin(Pin::OledSda, Pin::OledScl);
  s_display.begin(SSD1306_SWITCHCAPVCC, Pin::OledAddr);
  s_display.clearDisplay();
  s_display.setTextColor(SSD1306_WHITE);
  s_display.setTextSize(1);
  s_display.setCursor(10, 28);
  s_display.println("AUTOFUZZER v" AUTOFUZZER_VERSION);
  s_display.display();
  s_oledOn = true;
}

// ============================================
// Screen Setters
// ============================================
void oled_ui_set_screen(UiScreen screen) {
  s_currentScreen = screen;
  s_selection = 0;
  s_lastRefreshMs = 0;  // Force immediate redraw
}

UiScreen oled_ui_get_screen(void) {
  return s_currentScreen;
}

uint8_t oled_ui_get_selection(void) {
  return s_selection;
}

void oled_ui_set_selection(uint8_t idx) {
  s_selection = idx;
}

void oled_ui_select_next(void) {
  uint8_t maxItems = oled_ui_get_max_items();
  if (maxItems > 0) {
    s_selection = (s_selection + 1) % maxItems;
  }
}

void oled_ui_select_prev(void) {
  uint8_t maxItems = oled_ui_get_max_items();
  if (maxItems > 0) {
    s_selection = (s_selection == 0) ? maxItems - 1 : s_selection - 1;
  }
}

uint8_t oled_ui_confirm(void) {
  return s_selection;
}

// ============================================
// Toggle OLED
// ============================================
void oled_ui_toggle(void) {
  s_oledOn = !s_oledOn;
  if (s_oledOn) {
    s_display.ssd1306_command(SSD1306_DISPLAYON);
    s_lastRefreshMs = 0;
  } else {
    s_display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

bool oled_ui_is_on(void) {
  return s_oledOn;
}

// ============================================
// Drawing Helpers
// ============================================
static void draw_header(const char* title) {
  s_display.setTextSize(1);
  s_display.setCursor(0, 0);
  s_display.println(title);
  // Draw separator line
  s_display.drawLine(0, 10, Display::Width - 1, 10, SSD1306_WHITE);
}

static void draw_menu(const char* title, const MenuItem* items, uint8_t count, uint8_t selection) {
  s_display.clearDisplay();
  draw_header(title);

  uint8_t startIdx = 0;
  uint8_t visibleItems = 5;  // Max visible items on 64px height

  // Scroll if selection is beyond visible area
  if (selection >= visibleItems) {
    startIdx = selection - visibleItems + 1;
  }
  if (startIdx + visibleItems > count) {
    startIdx = count > visibleItems ? count - visibleItems : 0;
  }

  for (uint8_t i = 0; i < visibleItems && (startIdx + i) < count; i++) {
    uint8_t itemIdx = startIdx + i;
    uint8_t y = 14 + (i * 10);

    if (itemIdx == selection) {
      s_display.setCursor(0, y);
      s_display.print(">");
    } else {
      s_display.setCursor(6, y);
    }
    s_display.print(items[itemIdx].name);
  }

  // Scroll indicator
  if (count > visibleItems) {
    s_display.setCursor(120, 14);
    s_display.printf("%u/%u", selection + 1, count);
  }

  s_display.display();
}

// ============================================
// Screen Renderers
// ============================================
static void render_main_menu(void) {
  draw_menu("AUTOFUZZER v" AUTOFUZZER_VERSION, kMainMenu, kMainMenuCount, s_selection);
}

static void render_board_select(void) {
  draw_menu("SELECT TARGET BOARD", kBoardMenu, kBoardMenuCount, s_selection);
}

static void render_protocol_select(void) {
  draw_menu("SELECT PROTOCOL", kProtocolMenu, kProtocolMenuCount, s_selection);
}

static void render_test_select(void) {
  draw_menu("SELECT TEST", kTestMenu, kTestMenuCount, s_selection);
}

static void render_confirm(void) {
  draw_menu("START TEST?", kConfirmMenu, kConfirmMenuCount, s_selection);
}

// ============================================
// Wiring Diagram Data
//
// Each board+protocol combination has a specific
// wiring diagram shown on the OLED before testing.
// ============================================
static TargetBoard  s_selectedBoard  = BOARD_STM32;
static ProtocolMode s_wiringProtocol = PROTO_UART;

void oled_ui_set_board(TargetBoard board) { s_selectedBoard = board; }
TargetBoard oled_ui_get_board(void) { return s_selectedBoard; }
void oled_ui_set_wiring_protocol(ProtocolMode proto) { s_wiringProtocol = proto; }
ProtocolMode oled_ui_get_wiring_protocol(void) { return s_wiringProtocol; }

// Wiring info: 3 lines of text per diagram
struct WiringInfo {
  const char* line1;  // Protocol + board name
  const char* line2;  // Signal 1
  const char* line3;  // Signal 2
  const char* line4;  // Signal 3
  const char* line5;  // Signal 4
  const char* line6;  // Extra info
};

// UART wiring diagrams per board
static const WiringInfo kUartWiring[] = {
  // STM32 Nucleo
  { "UART -> STM32 Nucleo",
    "17 -> PA10 RX",
    "16 <- PA9  TX",
    "25 <- PB5  HB",
    "",
    "Press SELECT" },
  // ESP32
  { "UART -> ESP32",
    "17 -> 16",
    "16 <- 17",
    "25 <- 22",
    "",
    "Press SELECT" },
  // Arduino Nano
  { "UART -> Arduino Nano",
    "17 -> D3 RX",
    "16 <- D2 TX",
    "25 <- D4 HB",
    "",
    "Press SELECT" },
};

// SPI wiring diagrams per board
static const WiringInfo kSpiWiring[] = {
  // STM32 Nucleo
  { "SPI -> STM32 Nucleo",
    "23 MOSI -> PA7",
    "19 MISO <- PA6",
    "18 SCK  -> PA5",
    "5  CS   -> PA4",
    "Press SELECT" },
  // ESP32
  { "SPI -> ESP32",
    "23 MOSI -> 23",
    "19 MISO <- 19",
    "18 SCK  -> 18",
    "5  CS   -> 5",
    "Press SELECT" },
  // Arduino Nano
  { "SPI -> Arduino Nano",
    "23 MOSI -> D11",
    "19 MISO <- D12",
    "18 SCK  -> D13",
    "5  CS   -> D10",
    "Press SELECT" },
};

// I2C wiring diagrams per board
static const WiringInfo kI2cWiring[] = {
  // STM32 Nucleo
  { "I2C -> STM32 Nucleo",
    "21 SDA <-> PB7",
    "22 SCL <-> PB6",
    "",
    "",
    "Press SELECT" },
  // ESP32
  { "I2C -> ESP32",
    "21 SDA <-> 21",
    "22 SCL <-> 22",
    "",
    "",
    "Press SELECT" },
  // Arduino Nano
  { "I2C -> Arduino Nano",
    "21 SDA <-> A4",
    "22 SCL <-> A5",
    "",
    "",
    "Press SELECT" },
};

static const WiringInfo* get_wiring_info(ProtocolMode proto, TargetBoard board) {
  uint8_t boardIdx = (uint8_t)board;
  if (boardIdx >= 3) boardIdx = 0;
  switch (proto) {
    case PROTO_UART: return &kUartWiring[boardIdx];
    case PROTO_SPI:  return &kSpiWiring[boardIdx];
    case PROTO_I2C:  return &kI2cWiring[boardIdx];
    default: return &kUartWiring[boardIdx];
  }
}

static void render_wiring_diagram(void) {
  const WiringInfo* w = get_wiring_info(s_wiringProtocol, s_selectedBoard);

  s_display.clearDisplay();
  s_display.setTextSize(1);

  // Title
  s_display.setCursor(0, 0);
  s_display.println("WIRING GUIDE");
  s_display.drawLine(0, 9, Display::Width - 1, 9, SSD1306_WHITE);

  // Connection lines — all small font, tight spacing
  uint8_t y = 12;
  s_display.setCursor(0, y);  s_display.println(w->line1); y += 9;
  s_display.setCursor(0, y);  s_display.println(w->line2); y += 9;
  s_display.setCursor(0, y);  s_display.println(w->line3); y += 9;
  if (w->line4[0]) { s_display.setCursor(0, y); s_display.println(w->line4); y += 9; }
  if (w->line5[0]) { s_display.setCursor(0, y); s_display.println(w->line5); y += 9; }

  // Footer
  s_display.setCursor(0, 56);
  s_display.println(w->line6);

  s_display.display();
}

static void render_protocol_check(void) {
  s_display.clearDisplay();
  draw_header("PROTOCOL CHECK");

  s_display.setCursor(0, 14);
  s_display.printf("Protocol: %s\n", ProtocolNames[campaign_get_protocol()]);
  s_display.setCursor(0, 24);
  s_display.print("Communication: ");
  s_display.println(heartbeat_is_alive() ? "PASS" : "FAIL");
  s_display.setCursor(0, 34);
  s_display.print("Heartbeat:     ");
  s_display.println(heartbeat_is_alive() ? "PASS" : "FAIL");
  s_display.setCursor(0, 48);
  s_display.println(s_selection == 0 ? "> READY" : "  NOT READY");

  s_display.display();
}

static void render_fuzzing(void) {
  s_display.clearDisplay();
  s_display.setTextSize(1);

  // Title with protocol
  const char* protoName = ProtocolNames[campaign_get_protocol()];
  s_display.setCursor(0, 0);
  s_display.printf("%s FUZZING", protoName);

  // Separator
  s_display.drawLine(0, 10, Display::Width - 1, 10, SSD1306_WHITE);

  // Time: elapsed/total
  uint32_t elapsed = campaign_get_elapsed_ms() / 1000;
  uint32_t total = campaign_get_duration_ms() / 1000;
  s_display.setCursor(0, 14);
  s_display.printf("Time:   %u/%us", elapsed, total);

  // Packets
  s_display.setCursor(0, 24);
  s_display.printf("Pkts:   %u", campaign_get_packet_count());

  // Failures
  s_display.setCursor(0, 34);
  s_display.printf("Fails:  %u", failure_get_count());

  // Current mutation
  s_display.setCursor(0, 44);
  MutationType mut = campaign_select_mutation();
  s_display.printf("Mut:    %s", MutationNames[mut]);

  // DUT status
  s_display.setCursor(0, 54);
  s_display.printf("DUT:    %s", heartbeat_is_alive() ? "ALIVE" : "TIMEOUT!");

  s_display.display();
}

static void render_failure_menu(void) {
  draw_menu("FAILURE DETECTED", kFailureMenu, kFailureMenuCount, s_selection);
}

static void render_replay(void) {
  const FailureRecord* rec = failure_get_current();
  s_display.clearDisplay();
  draw_header("REPLAY FAILURE");

  s_display.setCursor(0, 14);
  if (rec) {
    s_display.printf("Failure #%u\n", rec->id);
    s_display.printf("Type: %s\n", FailureTypeNames[rec->type]);
    s_display.setCursor(0, 34);
    s_display.printf("Replays: %u/%u\n", rec->replayFails, rec->replayAttempts);

    if (rec->replayAttempts > 0) {
      if (rec->replayFails == rec->replayAttempts) {
        s_display.setCursor(0, 50);
        s_display.println("REPRODUCIBLE");
      } else if (rec->replayFails > 0) {
        s_display.setCursor(0, 50);
        s_display.println("INTERMITTENT");
      } else {
        s_display.setCursor(0, 50);
        s_display.println("NOT REPRODUCIBLE");
      }
    }
  }

  s_display.display();
}

static void render_minimizing(void) {
  const FailureRecord* rec = failure_get_current();
  s_display.clearDisplay();
  draw_header("MINIMIZING");

  s_display.setCursor(0, 14);
  if (rec) {
    s_display.printf("Original: %u bytes\n", rec->testcase.packetLen);
    s_display.setCursor(0, 30);
    s_display.printf("Current:  %u bytes\n", rec->minimizedLen > 0 ? rec->minimizedLen : rec->testcase.packetLen);
    s_display.setCursor(0, 46);
    s_display.println("Testing...");
  }

  s_display.display();
}

static void render_result(void) {
  result_calculate();
  s_display.clearDisplay();
  draw_header("TEST COMPLETE");

  const CampaignStats* stats = campaign_get_stats();
  s_display.setCursor(0, 14);
  s_display.printf("Pkts:    %u\n", stats->totalPackets);
  s_display.printf("Fails:   %u\n", stats->totalHeartbeatTimeouts);
  s_display.printf("Time:    %u s\n", stats->elapsedMs / 1000);

  s_display.setCursor(0, 40);
  s_display.printf("SCORE:   %u/100\n", result_get_score());

  s_display.setCursor(0, 54);
  s_display.printf("RESULT:  %s", result_get_verdict());

  s_display.display();
}

static void render_failure_detail(void) {
  const FailureRecord* rec = failure_get_current();
  s_display.clearDisplay();
  draw_header("FAILURE DETAILS");

  if (rec) {
    s_display.setCursor(0, 14);
    s_display.printf("Type: %s\n", FailureTypeNames[rec->type]);
    s_display.printf("Seq: %u\n", rec->testcase.sequence);
    s_display.printf("Mut: %s\n", MutationNames[rec->testcase.mutation]);
    s_display.printf("Len: %u bytes\n", rec->testcase.packetLen);
    s_display.printf("HB age: %u ms\n", rec->heartbeatLostMs);
  }

  s_display.display();
}

static void render_settings(void) {
  s_display.clearDisplay();
  draw_header("SETTINGS");
  s_display.setCursor(0, 14);
  s_display.println("Coming in v4.1");
  s_display.display();
}

static void render_about(void) {
  s_display.clearDisplay();
  draw_header("ABOUT");
  s_display.setCursor(0, 14);
  s_display.println("AutoFuzzer v" AUTOFUZZER_VERSION);
  s_display.println("Embedded Protocol");
  s_display.println("Robustness Tester");
  s_display.println();
  s_display.println("github.com/");
  s_display.println("Alwin111/AutoFuzzer");
  s_display.display();
}

// ============================================
// Update — render at controlled interval
// ============================================
void oled_ui_update(void) {
  if (!s_oledOn) return;
  if (millis() - s_lastRefreshMs < kRefreshIntervalMs) return;

  s_lastRefreshMs = millis();

  switch (s_currentScreen) {
    case SCREEN_MAIN_MENU:       render_main_menu(); break;
    case SCREEN_BOARD_SELECT:    render_board_select(); break;
    case SCREEN_PROTOCOL_SELECT: render_protocol_select(); break;
    case SCREEN_WIRING_DIAGRAM:  render_wiring_diagram(); break;
    case SCREEN_TEST_SELECT:     render_test_select(); break;
    case SCREEN_TEST_CONFIRM:    render_confirm(); break;
    case SCREEN_PROTOCOL_CHECK:  render_protocol_check(); break;
    case SCREEN_FUZZING:         render_fuzzing(); break;
    case SCREEN_FAILURE_MENU:    render_failure_menu(); break;
    case SCREEN_REPLAY:          render_replay(); break;
    case SCREEN_MINIMIZING:      render_minimizing(); break;
    case SCREEN_RESULT:          render_result(); break;
    case SCREEN_FAILURE_DETAIL:  render_failure_detail(); break;
    case SCREEN_SETTINGS:        render_settings(); break;
    case SCREEN_ABOUT:           render_about(); break;
    default: break;
  }
}

// ============================================
// Force Redraw
// ============================================
void oled_ui_redraw(void) {
  s_lastRefreshMs = 0;
  oled_ui_update();
}
