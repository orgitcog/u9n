// ═══════════════════════════════════════════════════════════════════════════
// OnlineAutoRLRuntime.cpp
// Compilation unit for FOnlineAutoRLRuntime — continuous in-game RL.
//
// FOnlineAutoRLRuntime is a fully header-only class. All method bodies are
// defined inline in OnlineAutoRLRuntime.h to enable:
//   - Experience tuple ingestion inlined for minimal per-frame overhead
//   - Mini-batch sampling inlined with priority weighting computation
//   - Policy gradient computation kept at call sites for loop fusion
//   - Entropy bonus scheduling inlined with exploration decay
//
// The Online AutoRL Runtime is the in-game counterpart to the offline
// NanEcho self-training pipeline. It maintains a prioritised replay
// buffer and performs continuous PPO micro-updates at the end of each
// Dream phase, keeping the policy aligned with the current environment.
//
// Feature:  F1.5.7 — Online Auto-RL Runtime
// Phase:    1.5 — LiveBridge
// Epic:     E4 — Live-Learning Bridge
// ═══════════════════════════════════════════════════════════════════════════

#include "OnlineAutoRLRuntime.h"
