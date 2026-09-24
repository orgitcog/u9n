// ═══════════════════════════════════════════════════════════════════════════
// ImitationLearner.cpp
// Compilation unit for FImitationLearner — behaviour cloning from demos.
//
// FImitationLearner is a fully header-only class. All method bodies are
// defined inline in ImitationLearner.h to enable:
//   - Episode recording and replay inlined at call sites
//   - Behaviour-cloning gradient computation inlined
//   - Demonstration session compression inlined
//   - Policy clone weight updates kept near usage for loop fusion
//
// The Imitation Learner ingests demonstration sessions recorded by the
// DemonstrationRecorder and applies supervised behaviour cloning to
// bootstrap DTE's policy prior, reducing the exploration burden during
// online RL fine-tuning.
//
// Feature:  F1.4.6 — Imitation Learning
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "ImitationLearner.h"
