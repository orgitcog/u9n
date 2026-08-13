// ═══════════════════════════════════════════════════════════════════════════
// AutonomyPipeline.cpp
// Compilation unit for FAutonomyPipeline — master cognitive orchestrator.
//
// FAutonomyPipeline is a fully header-only class. All method bodies are
// defined inline in AutonomyPipeline.h to enable:
//   - Inlined 12-step echobeat tick dispatch (Step() hot path)
//   - Zero-overhead subsystem wiring at pipeline construction
//   - Eigen matrix operations (reservoir → readout → motor) inlined in tick
//
// AutonomyPipeline wires together the complete Deep Tree Echo cognitive loop:
//   EchoReservoir → CognitiveReadout → AAR → Echobeats → Introspection
//   → Membrane → Somatic → Humor → IonShell
//
// Feature:  F1.3.2 — Autonomy Pipeline
// Phase:    1.3 — Core Pipeline Infrastructure
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "AutonomyPipeline.h"
