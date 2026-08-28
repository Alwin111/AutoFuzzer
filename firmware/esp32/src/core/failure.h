#pragma once
#include "types.h"

// ============================================
// Failure Manager
//
// Handles:
//   - Failure classification
//   - Failure record creation
//   - Failure status tracking
//   - Failure freeze (stop campaign, capture state)
// ============================================

// Initialize failure manager
void failure_init(void);

// Freeze the campaign on failure — captures all state
void failure_freeze(FailureType type, const TestcaseMeta* testcase,
                    uint32_t heartbeatLostMs, bool dutResponded,
                    uint8_t dutStatus, uint32_t dutRespTimeMs);

// Get the currently frozen failure (the one being investigated)
const FailureRecord* failure_get_current(void);

// Get a specific failure by index
const FailureRecord* failure_get_by_index(uint8_t idx);

// Get total number of recorded failures
uint8_t failure_get_count(void);

// Update failure status after replay attempts
void failure_update_status(FailureStatus status, uint8_t attempts, uint8_t fails);

// Update minimized length
void failure_update_minimized(uint32_t minimizedLen);

// Clear all failures
void failure_clear_all(void);

// Export failure over serial (JSON-like format)
void failure_export_serial(uint8_t idx);

// Print failure summary to serial
void failure_print_summary(const FailureRecord* rec);

// Classify a heartbeat timeout based on available evidence
FailureType failure_classify_heartbeat(uint32_t timeoutMs, bool dutResponded);
