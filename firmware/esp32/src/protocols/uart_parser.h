#pragma once
#include "../core/types.h"

// ============================================
// UART Response Parser
//
// Reads and parses the DUT's response frames:
//
//   0x5A | SEQ_LO | SEQ_HI | STATUS
//
// The parser:
//   1. Reads bytes from Serial2 as they arrive
//   2. Detects the sync byte (0x5A)
//   3. Collects the full 4-byte response
//   4. Validates the sequence number
//   5. Returns the parsed response
//   6. Handles timeouts (no response)
//
// Call uart_parser_poll() every loop iteration.
// ============================================

// Initialize the parser
void uart_parser_init(void);

// Poll for incoming bytes — call every loop iteration
// Returns true if a complete response was received
bool uart_parser_poll(void);

// Get the last complete response
const UartResponse* uart_parser_get_response(void);

// Check if there's a pending (unread) response
bool uart_parser_has_response(void);

// Clear the response flag (after processing)
void uart_parser_clear_response(void);

// Get the expected sequence for the next packet
uint16_t uart_parser_get_expected_seq(void);
