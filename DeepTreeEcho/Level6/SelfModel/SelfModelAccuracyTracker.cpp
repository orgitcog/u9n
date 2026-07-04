// ═══════════════════════════════════════════════════════════════════════════
// SelfModelAccuracyTracker.cpp
// Compilation unit for FSelfModelAccuracyTracker — metacognitive calibration.
//
// FSelfModelAccuracyTracker is a fully header-only class. All method bodies
// are defined inline in SelfModelAccuracyTracker.h to enable:
//   - Prediction error accumulation inlined for low-overhead tracking
//   - Blind-spot detection heuristic computed at call sites
//   - Calibration score update inlined with exponential moving average
//   - Snapshot serialisation kept inline for minimal allocation
//
// The Self Model Accuracy Tracker maintains a running calibration of how
// well DTE's self-model matches observed behaviour. It identifies
// dimensions where predictions consistently diverge from outcomes
// (blind spots) and flags them to the Level 6 orchestrator so targeted
// architecture modifications can be proposed.
//
// Feature:  F1.6.3 — Self Model Accuracy Tracker
// Phase:    1.6 — Recursive Autonomy (Level 6)
// Epic:     E5 — Self-Modifying Recursive Autonomy
// ═══════════════════════════════════════════════════════════════════════════

#include "SelfModelAccuracyTracker.h"
