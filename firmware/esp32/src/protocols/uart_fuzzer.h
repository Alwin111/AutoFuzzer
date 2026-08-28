#pragma once
#include "../core/types.h"

// ============================================
// UART Fuzzer
//
// Builds and transmits fuzzed UART packets
// using the AutoFuzzer protocol:
//
//   SYNC(0xA5) | CMD | LEN | SEQ_LO | SEQ_HI | PAYLOAD | CHECKSUM
//
// Supports 7 mutation types that deliberately
// stress the DUT's parser in different ways.
//
// Every packet generation captures full testcase
// metadata for reproduction.
// ============================================

// Initialize UART fuzzer
void uart_fuzzer_init(void);

// Send a fuzzed UART packet with the specified mutation
// Returns true if packet was sent successfully
bool uart_fuzzer_send(MutationType mutation);

// Send a specific packet for replay (uses exact same bytes)
bool uart_fuzzer_replay(const TestcaseMeta* testcase);

// Get the last sent packet bytes and length
const uint8_t* uart_fuzzer_get_last_packet(uint8_t* outLen);
