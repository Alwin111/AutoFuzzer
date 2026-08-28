#pragma once
#include "../core/types.h"
#include <Adafruit_SSD1306.h>

// ============================================
// OLED UI Manager
//
// Manages all display screens:
//
//   Main Menu:      NEW TEST, RESULTS, FAILURES, SETTINGS, ABOUT
//   Protocol:       UART, SPI, I2C, CAN
//   Test Profile:   QUICK 30s, STANDARD 60s, DEEP 5min, CUSTOM
//   Confirm:        YES / NO
//   Protocol Check: Communication, Response, Heartbeat
//   During Test:    Time, Packets, Failures, Mutation, DUT Status
//   Failure:        REPLAY, MINIMIZE, REPORT, TRY FIX, DETAILS, SAVE
//   Result:         Score, Verdict, Categories
//
// Uses non-blocking refresh (500ms minimum between redraws).
// Supports OLED toggle on/off via Button 4.
// ============================================

// Initialize OLED display
void oled_ui_init(void);

// Set the current screen
void oled_ui_set_screen(UiScreen screen);

// Get the current screen
UiScreen oled_ui_get_screen(void);

// Get the currently selected menu item index (for navigation)
uint8_t oled_ui_get_selection(void);

// Set menu selection (for navigation)
void oled_ui_set_selection(uint8_t idx);

// Cycle selection up/down within current screen's options
void oled_ui_select_next(void);
void oled_ui_select_prev(void);

// Confirm current selection — returns the selection index
uint8_t oled_ui_confirm(void);

// Update display — call every loop iteration
// Handles refresh timing and screen-specific rendering
void oled_ui_update(void);

// Toggle OLED on/off
void oled_ui_toggle(void);
bool oled_ui_is_on(void);

// Force immediate redraw (e.g., after state change)
void oled_ui_redraw(void);

// Get the max number of menu items for the current screen
uint8_t oled_ui_get_max_items(void);

// Get menu item name by screen and index
const char* oled_ui_get_item_name(UiScreen screen, uint8_t idx);

// Board selection
void oled_ui_set_board(TargetBoard board);
TargetBoard oled_ui_get_board(void);

// Wiring protocol (for diagram display)
void oled_ui_set_wiring_protocol(ProtocolMode proto);
ProtocolMode oled_ui_get_wiring_protocol(void);
