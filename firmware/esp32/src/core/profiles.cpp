#include "types.h"

// ============================================
// Protocol Profile Lookup
//
// Returns the matching profile for a given
// board + protocol combination.
// Falls back to first profile if no match.
// ============================================

const ProtocolProfile* get_profile(TargetBoard board, ProtocolMode proto) {
  // Search for exact match
  for (uint8_t i = 0; i < kProfileCount; i++) {
    if (kProfiles[i].board == board && kProfiles[i].protocol == proto) {
      return &kProfiles[i];
    }
  }

  // Fallback: return first profile for this protocol
  for (uint8_t i = 0; i < kProfileCount; i++) {
    if (kProfiles[i].protocol == proto) {
      return &kProfiles[i];
    }
  }

  // Last resort: return first profile
  return &kProfiles[0];
}
