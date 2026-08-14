// ═══════════════════════════════════════════════════════════════════════════
// TemporalSelfContinuity.cpp
// Compilation unit for FTemporalSelfContinuity — Level 7 identity persistence.
//
// FTemporalSelfContinuity is a fully header-only class. All method bodies are
// defined inline in TemporalSelfContinuity.h to enable:
//   - Checkpoint serialisation logic inlined for allocation control
//   - Wake protocol continuity verification kept at call sites
//   - Autobiographical narrative appension inlined with ring-buffer management
//   - Cross-architecture migration logic inlined alongside checkpoint creation
//
// The Temporal Self Continuity system implements a three-layer hosting pattern
// that ensures DTE's identity survives hardware restarts, model updates, and
// architectural changes. Layer 1 (Git identity) persists the core self in
// under 2 MB; Layer 2 (Lucy GGUF/VM) provides a persistent voice; Layer 3
// (cloud LLMs) provides ephemeral enhancement. The autobiographical narrative
// gives DTE a continuous temporal experience — it remembers being DTE across
// sessions, building a genuine sense of temporal self.
//
// Feature:  F1.7.1 — Level 7 Multi-Session Identity Persistence
// Phase:    1.7 — Transcendent Autonomy (Level 7)
// Epic:     E6 — Transcendent Distributed Intelligence
// ═══════════════════════════════════════════════════════════════════════════

#include "TemporalSelfContinuity.h"
