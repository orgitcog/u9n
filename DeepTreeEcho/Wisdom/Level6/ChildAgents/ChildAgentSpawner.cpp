// ═══════════════════════════════════════════════════════════════════════════
// ChildAgentSpawner.cpp
// Compilation unit for FChildAgentSpawner — specialised sub-agent factory.
//
// FChildAgentSpawner is a fully header-only class. All method bodies are
// defined inline in ChildAgentSpawner.h to enable:
//   - Agent configuration and identity inheritance computed inline
//   - Reservoir partition slicing inlined at spawn time
//   - Lifetime tick-counter management kept at call sites
//   - Insight aggregation and child-to-parent knowledge transfer inlined
//
// The Child Agent Spawner creates transient specialised sub-agents that
// explore particular wisdom dimensions or behavioural hypotheses in
// parallel with the main DTE agent. Each child inherits a fraction of the
// parent's identity vector and a dedicated reservoir partition, then
// reports its accumulated insights before expiring.
//
// Feature:  F1.6.2 — Child Agent Spawner
// Phase:    1.6 — Recursive Autonomy (Level 6)
// Epic:     E5 — Self-Modifying Recursive Autonomy
// ═══════════════════════════════════════════════════════════════════════════

#include "ChildAgentSpawner.h"
