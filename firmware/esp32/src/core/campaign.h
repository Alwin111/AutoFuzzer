#pragma once
#include "types.h"

// ============================================
// Campaign Manager
//
// Controls the lifecycle of a test campaign:
//   IDLE → PREPARING → RUNNING → PAUSED/COMPLETE
//
// Manages test phases within a campaign:
//   BASELINE → BOUNDARY → OVERLENGTH → CHECKSUM → MALFORMED → RANDOM → FREEFORM
//
// Each phase maps to specific mutation distributions
// that prioritize high-value test cases early.
// ============================================

void campaign_init(void);

// Start a new campaign with given protocol and profile
void campaign_start(ProtocolMode protocol, TestProfile profile, uint32_t customDurationMs);

// Stop the current campaign
void campaign_stop(void);

// Pause/resume toggle
void campaign_toggle_pause(void);

// Call every loop iteration — updates elapsed time, phases
void campaign_update(void);

// Get current campaign state
bool campaign_is_running(void);
bool campaign_is_paused(void);
bool campaign_is_complete(void);

// Get current info
ProtocolMode campaign_get_protocol(void);
TestProfile campaign_get_profile(void);
CampaignPhase campaign_get_phase(void);
const char* campaign_get_phase_name(void);

// Get elapsed time in ms
uint32_t campaign_get_elapsed_ms(void);

// Get total duration in ms
uint32_t campaign_get_duration_ms(void);

// Get progress as percentage (0-100)
uint8_t campaign_get_progress_pct(void);

// Select a mutation for the current phase
// Returns a MutationType appropriate for the current campaign phase
MutationType campaign_select_mutation(void);

// Get campaign seed
uint32_t campaign_get_seed(void);

// Get current packet count
uint32_t campaign_get_packet_count(void);

// Increment packet count
void campaign_increment_packet_count(void);

// Record a response
void campaign_record_ack(void);
void campaign_record_nack(void);
void campaign_record_no_response(void);
void campaign_record_heartbeat_timeout(void);

// Get accumulated stats
const CampaignStats* campaign_get_stats(void);
