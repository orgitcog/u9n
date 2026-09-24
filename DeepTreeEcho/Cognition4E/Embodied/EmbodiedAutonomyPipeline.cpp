// ═══════════════════════════════════════════════════════════════════════════
// EmbodiedAutonomyPipeline.cpp
// Compilation unit for FEmbodiedAutonomyPipeline — the full embodied loop.
//
// FEmbodiedAutonomyPipeline is a fully header-only class. All method bodies
// are defined inline in EmbodiedAutonomyPipeline.h to enable:
//   - Sense → Think → Act loop inlined for minimal frame overhead
//   - Reservoir state integration computed at call sites
//   - Imitation learning fallback logic inlined
//   - Neuro-endocrine RL modulation kept at call sites
//
// The Embodied Autonomy Pipeline is the top-level driver for DTE's
// real-time embodied cognition. It orchestrates the Echobeat rhythm,
// integrating visual perception, sensorimotor feedback, and action
// selection into a continuous 60Hz cognitive loop.
//
// Feature:  F1.4.5 — Embodied Autonomy Pipeline
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "EmbodiedAutonomyPipeline.h"
