#pragma once
#include "../core/types.h"

// ============================================
// Failure Store
//
// Manages in-memory storage of failure records.
// Provides serial-based export for PC companion
// to capture failure data over USB.
//
// Export format: JSON-like records over Serial.
//
// Future: SD card backend for persistent storage.
// ============================================

// Initialize the store
void failure_store_init(void);

// Store a failure record
void failure_store_add(const FailureRecord* rec);

// Get the number of stored failures
uint8_t failure_store_count(void);

// Get a specific failure by index
const FailureRecord* failure_store_get(uint8_t idx);

// Export all failures over serial
void failure_store_export_all(void);

// Export a single failure over serial
void failure_store_export_one(uint8_t idx);

// Clear all stored failures
void failure_store_clear(void);

// Export format:
// --- AUTOFUZZER_EXPORT_START ---
// { "records": [ ... ] }
// --- AUTOFUZZER_EXPORT_END ---
