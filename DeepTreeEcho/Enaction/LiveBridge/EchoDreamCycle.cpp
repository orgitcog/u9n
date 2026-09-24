// ═══════════════════════════════════════════════════════════════════════════
// EchoDreamCycle.cpp
// Compilation unit for FEchoDreamCycle — sleep/wake consolidation loop.
//
// FEchoDreamCycle is a fully header-only class. All method bodies are
// defined inline in EchoDreamCycle.h to enable:
//   - Phase-transition logic inlined (AWAKE → DROWSY → DREAM → AWAKE)
//   - Fatigue accumulation computed inline without heap allocation
//   - Replay buffer prioritisation inlined for tight inner loop
//   - Consolidation wave computation kept at call sites for unrolling
//
// The Echo Dream Cycle models biological sleep-wake consolidation.
// During the DREAM phase the Online AutoRL Runtime replays high-priority
// episodes against the current policy, strengthening beneficial patterns
// and weakening conflicting ones before the agent wakes.
//
// Feature:  F1.5.5 — Echo Dream Cycle
// Phase:    1.5 — LiveBridge
// Epic:     E4 — Live-Learning Bridge
// ═══════════════════════════════════════════════════════════════════════════

#include "EchoDreamCycle.h"
