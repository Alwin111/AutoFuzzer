#pragma once
#include "types.h"

// ============================================
// Testcase Manager
//
// Tracks metadata for the current packet being
// generated/sent, so that if a failure occurs,
// the exact failing testcase can be captured
// and replayed deterministically.
// ============================================

// Prepare a new testcase — call BEFORE generating packet
// Records PRNG state, seed, timestamp
void testcase_prepare(uint16_t sequence, MutationType mutation);

// Finalize after packet bytes are written
// Records packet length and byte content
void testcase_finalize(uint8_t packetLen, uint8_t payloadLen, const uint8_t* bytes);

// Get the current testcase metadata
const TestcaseMeta* testcase_get_current(void);

// Get the last finalized testcase
const TestcaseMeta* testcase_get_last(void);

// Copy the last testcase into a provided buffer
void testcase_copy_last(TestcaseMeta* dest);

// Print testcase to serial for debugging
void testcase_print(const TestcaseMeta* tc);
