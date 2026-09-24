// ═══════════════════════════════════════════════════════════════════════════
// DTEAvatarAgent.cpp
// Compilation unit for FDTEAvatarAgent — embodied avatar orchestrator.
//
// FDTEAvatarAgent is a fully header-only class. All method bodies are
// defined inline in DTEAvatarAgent.h to enable:
//   - Avatar perception pipeline inlined (vision + controller + proprio)
//   - Action selection logic kept at call sites for compiler optimization
//   - Avatar expression mapping inlined for minimal dispatch cost
//   - Emotion state updates inlined with affective decay constants
//
// The DTE Avatar Agent is the unified embodiment host. It binds the
// vision system, virtual controller driver, and sensorimotor integration
// into a single cognitive entity that perceives and acts within the
// game environment through a MetaHuman-backed avatar.
//
// Feature:  F1.4.4 — DTE Avatar Agent
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "DTEAvatarAgent.h"
