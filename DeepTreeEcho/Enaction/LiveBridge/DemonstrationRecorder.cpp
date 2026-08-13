// ═══════════════════════════════════════════════════════════════════════════
// DemonstrationRecorder.cpp
// Compilation unit for FDemonstrationRecorder — human demo capture.
//
// FDemonstrationRecorder is a fully header-only class. All method bodies
// are defined inline in DemonstrationRecorder.h to enable:
//   - Frame timestamping inlined for nanosecond-accurate capture
//   - Keyframe detection heuristic kept at call sites
//   - Session metadata serialisation inlined
//   - NanEcho-format export helpers inlined alongside capture logic
//
// The Demonstration Recorder captures human expert play sessions at every
// Echobeat frame, storing (observation, action, reward) tuples for later
// use by the ImitationLearner supervised pre-training stage.
//
// Feature:  F1.5.4 — Demonstration Recorder
// Phase:    1.5 — LiveBridge
// Epic:     E4 — Live-Learning Bridge
// ═══════════════════════════════════════════════════════════════════════════

#include "DemonstrationRecorder.h"
