#include "campaign.h"
#include "prng.h"

// ============================================
// Internal State
// ============================================
enum CampaignState : uint8_t {
  CAMP_IDLE = 0,
  CAMP_PREPARING,
  CAMP_RUNNING,
  CAMP_PAUSED,
  CAMP_COMPLETE
};

static CampaignState  s_state     = CAMP_IDLE;
static ProtocolMode   s_protocol  = PROTO_UART;
static TestProfile    s_profile   = TEST_QUICK;
static CampaignPhase  s_phase     = PHASE_BASELINE;
static uint32_t       s_seed      = 0xC0DEC0DE;
static uint32_t       s_startMs   = 0;
static uint32_t       s_elapsedMs = 0;
static uint32_t       s_durationMs = 30000;
static uint32_t       s_pauseStart = 0;
static uint32_t       s_pauseTotal = 0;
static CampaignStats  s_stats;

// ============================================
// Adaptive Mutation Statistics
//
// Tracks per-mutation outcomes to dynamically
// adjust selection probability. Mutations that
// produce interesting behavior (NACKs, timeouts,
// heartbeat failures) get boosted priority.
// ============================================
static MutationStats s_mutationStats[MUT_COUNT];

// Exploration rate: 10% of selections are pure random
// to prevent starvation of any mutation type
static const uint32_t kExplorationRate = 10;  // 1 in 10

// Adaptive weight boost factors
static const uint32_t kNackBoost = 3;       // NACK = interesting response
static const uint32_t kTimeoutBoost = 5;    // Timeout = very interesting
static const uint32_t kHbFailBoost = 10;    // Heartbeat failure = highest interest

// ============================================
// Phase transition times (relative to campaign start)
// ============================================
static const uint32_t kPhaseBoundaries[] = {
  0,      // PHASE_BASELINE:   0-5s
  5000,   // PHASE_BOUNDARY:   5-10s
  10000,  // PHASE_OVERLENGTH: 10-15s
  15000,  // PHASE_CHECKSUM:   15-20s
  20000,  // PHASE_MALFORMED:  20-25s
  25000,  // PHASE_RANDOM:     25-30s
  30000   // PHASE_FREEFORM:   30s+
};

static const char* kPhaseNames[] = {
  "BASELINE", "BOUNDARY", "OVERLENGTH",
  "CHECKSUM", "MALFORMED", "RANDOM", "FREEFORM"
};

// ============================================
// Phase → Mutation Distribution
//
// Each phase has a weighted set of mutations
// that it prefers. campaign_select_mutation()
// picks one based on these weights.
// ============================================

// Weights per phase: [VALID, EMPTY, MAX, OVER, CRC, TRUNC, RAND, HEADER, LEN, SEQ]
static const uint8_t kPhaseWeights[][MUT_COUNT] = {
  // PHASE_BASELINE: mostly valid, some empty
  { 80, 15, 5, 0, 0, 0, 0, 0, 0, 0 },
  // PHASE_BOUNDARY: empty + max length
  { 10, 30, 40, 10, 0, 10, 0, 0, 0, 0 },
  // PHASE_OVERLENGTH: overlength dominant
  { 5, 5, 10, 60, 5, 10, 5, 0, 0, 0 },
  // PHASE_CHECKSUM: bad CRC dominant
  { 5, 5, 5, 10, 60, 5, 10, 0, 0, 0 },
  // PHASE_MALFORMED: header/len/seq mutations
  { 5, 5, 5, 10, 10, 20, 15, 15, 10, 5 },
  // PHASE_RANDOM: equal random + adaptive
  { 5, 5, 5, 10, 10, 10, 30, 10, 10, 5 },
  // PHASE_FREEFORM: full distribution
  { 10, 8, 8, 12, 12, 12, 18, 10, 8, 4 }
};

// ============================================
// Init
// ============================================
void campaign_init(void) {
  s_state = CAMP_IDLE;
  memset(&s_stats, 0, sizeof(s_stats));
  memset(s_mutationStats, 0, sizeof(s_mutationStats));
}

// ============================================
// Start
// ============================================
void campaign_start(ProtocolMode protocol, TestProfile profile, uint32_t customDurationMs) {
  s_protocol = protocol;
  s_profile = profile;
  s_phase = PHASE_BASELINE;
  s_seed = 0xC0DEC0DE;  // Default campaign seed
  s_pauseTotal = 0;

  if (profile == TEST_CUSTOM) {
    s_durationMs = customDurationMs > 0 ? customDurationMs : 30000;
  } else {
    s_durationMs = TestProfileDurations[profile];
  }

  // Initialize PRNG with campaign seed
  prng_init(s_seed);

  // Clear stats
  memset(&s_stats, 0, sizeof(CampaignStats));
  memset(s_mutationStats, 0, sizeof(s_mutationStats));
  s_stats.durationMs = s_durationMs;

  s_startMs = millis();
  s_elapsedMs = 0;
  s_state = CAMP_RUNNING;

  Serial.printf("Campaign START: %s profile, duration=%u ms, seed=0x%08X\n",
                TestProfileNames[profile], s_durationMs, s_seed);
}

// ============================================
// Stop
// ============================================
void campaign_stop(void) {
  s_state = CAMP_IDLE;
  Serial.printf("Campaign STOPPED at %u ms, %u packets sent\n",
                s_elapsedMs, s_stats.totalPackets);
}

// ============================================
// Pause / Resume
// ============================================
void campaign_toggle_pause(void) {
  if (s_state == CAMP_RUNNING) {
    s_state = CAMP_PAUSED;
    s_pauseStart = millis();
    Serial.println("Campaign PAUSED");
  } else if (s_state == CAMP_PAUSED) {
    s_pauseTotal += millis() - s_pauseStart;
    s_state = CAMP_RUNNING;
    Serial.println("Campaign RESUMED");
  }
}

// ============================================
// Update — called every loop iteration
// ============================================
void campaign_update(void) {
  if (s_state != CAMP_RUNNING) return;

  s_elapsedMs = millis() - s_startMs - s_pauseTotal;
  s_stats.elapsedMs = s_elapsedMs;

  // Check if campaign duration elapsed
  if (s_elapsedMs >= s_durationMs) {
    s_state = CAMP_COMPLETE;
    s_phase = PHASE_DONE;
    Serial.printf("Campaign COMPLETE: %u packets, %u acks, %u nacks, %u failures\n",
                  s_stats.totalPackets, s_stats.totalAcks,
                  s_stats.totalNacks, s_stats.totalFailures);
    return;
  }

  // Update phase based on elapsed time
  CampaignPhase newPhase = PHASE_FREEFORM;

  // Map elapsed time to phase
  if (s_elapsedMs < 5000) {
    newPhase = PHASE_BASELINE;
  } else if (s_elapsedMs < 10000) {
    newPhase = PHASE_BOUNDARY;
  } else if (s_elapsedMs < 15000) {
    newPhase = PHASE_OVERLENGTH;
  } else if (s_elapsedMs < 20000) {
    newPhase = PHASE_CHECKSUM;
  } else if (s_elapsedMs < 25000) {
    newPhase = PHASE_MALFORMED;
  } else if (s_elapsedMs < 30000) {
    newPhase = PHASE_RANDOM;
  } else {
    newPhase = PHASE_FREEFORM;
  }

  if (newPhase != s_phase) {
    s_phase = newPhase;
    Serial.printf("Phase -> %s (%u/%u ms)\n", kPhaseNames[s_phase], s_elapsedMs, s_durationMs);
  }
}

// ============================================
// State queries
// ============================================
bool campaign_is_running(void)  { return s_state == CAMP_RUNNING; }
bool campaign_is_paused(void)   { return s_state == CAMP_PAUSED; }
bool campaign_is_complete(void) { return s_state == CAMP_COMPLETE; }

ProtocolMode campaign_get_protocol(void) { return s_protocol; }
TestProfile  campaign_get_profile(void)  { return s_profile; }
CampaignPhase campaign_get_phase(void)   { return s_phase; }
const char*  campaign_get_phase_name(void) { return kPhaseNames[s_phase]; }

uint32_t campaign_get_elapsed_ms(void)  { return s_elapsedMs; }
uint32_t campaign_get_duration_ms(void) { return s_durationMs; }

uint8_t campaign_get_progress_pct(void) {
  if (s_durationMs == 0) return 0;
  uint32_t pct = (s_elapsedMs * 100) / s_durationMs;
  return pct > 100 ? 100 : (uint8_t)pct;
}

uint32_t campaign_get_seed(void) { return s_seed; }

uint32_t campaign_get_packet_count(void) { return s_stats.totalPackets; }

void campaign_increment_packet_count(void) {
  s_stats.totalPackets++;
}

void campaign_record_ack(void) {
  s_stats.totalAcks++;
}

void campaign_record_nack(void) {
  s_stats.totalNacks++;
}

void campaign_record_no_response(void) {
  s_stats.totalNoResponse++;
}

void campaign_record_heartbeat_timeout(void) {
  s_stats.totalHeartbeatTimeouts++;
}

const CampaignStats* campaign_get_stats(void) {
  return &s_stats;
}

// ============================================
// Adaptive Mutation Statistics
// ============================================
const MutationStats* campaign_get_mutation_stats(MutationType mut) {
  if (mut >= MUT_COUNT) return nullptr;
  return &s_mutationStats[mut];
}

void campaign_record_mutation_response(MutationType mut, bool ack, bool nack, bool timeout) {
  if (mut >= MUT_COUNT) return;
  MutationStats& ms = s_mutationStats[mut];
  ms.executions++;
  if (ack) ms.acks++;
  if (nack) ms.nacks++;
  if (timeout) ms.timeouts++;
}

void campaign_record_mutation_heartbeat_fail(MutationType mut) {
  if (mut >= MUT_COUNT) return;
  s_mutationStats[mut].heartbeatFails++;
}

uint32_t campaign_get_mutation_weight(MutationType mut) {
  if (mut >= MUT_COUNT) return 0;

  // Start with base phase weight
  uint32_t weight = kPhaseWeights[s_phase][mut];

  // Apply adaptive boosts based on observed behavior
  const MutationStats& ms = s_mutationStats[mut];

  // Boost mutations that produced interesting responses
  weight += ms.nacks * kNackBoost;
  weight += ms.timeouts * kTimeoutBoost;
  weight += ms.heartbeatFails * kHbFailBoost;

  // Small penalty for mutations that only produce ACKs (less interesting)
  if (ms.executions > 10 && ms.nacks == 0 && ms.timeouts == 0 && ms.heartbeatFails == 0) {
    weight = weight * 3 / 4;  // 25% reduction
  }

  return weight;
}

// ============================================
// Mutation Selection — Adaptive with Exploration
//
// 1. 10% of the time, select purely randomly
//    (exploration — prevents starvation)
// 2. Otherwise, use phase-weighted selection
//    boosted by adaptive statistics
// ============================================
MutationType campaign_select_mutation(void) {
  // Exploration: pure random selection
  if (prng_range(100) < kExplorationRate) {
    return (MutationType)prng_range(MUT_COUNT);
  }

  // Adaptive weighted selection
  uint32_t weights[MUT_COUNT];
  uint32_t total = 0;

  for (uint8_t i = 0; i < MUT_COUNT; i++) {
    weights[i] = campaign_get_mutation_weight((MutationType)i);
    total += weights[i];
  }

  if (total == 0) return MUT_VALID;

  uint32_t roll = prng_range(total);
  uint32_t cumulative = 0;

  for (uint8_t i = 0; i < MUT_COUNT; i++) {
    cumulative += weights[i];
    if (roll < cumulative) {
      return (MutationType)i;
    }
  }

  return MUT_RANDOM;  // Fallback
}
