// ═══════════════════════════════════════════════════════════════════════════
// Sys6OperadEngine.cpp  (Sys6/ forwarding shim)
// Compilation unit for the Sys6/ backward-compatibility forwarding header.
//
// The canonical implementation lives in DeepTreeEcho/Core/Sys6OperadEngine.h
// and DeepTreeEcho/Core/Sys6OperadEngine.cpp. This shim exists only to provide
// a compilation unit that pulls in the forwarding header at its legacy location,
// ensuring that any translation unit including the Sys6/ path still produces a
// valid object file.
//
// USys6OperadEngine — implements the composite sys6 operad:
//   Sys6 := σ ∘ (φ ∘ μ ∘ (Δ_2 ⊗ Δ_3 ⊗ id_P))
// Δ_2 moves 2³=8 into cubic concurrency (C8); Δ_3 moves 3²=9 into orthogonal
// triadic convolution (K9); μ aligns D/T/P into a 30-step clock (LCM(2,3,5));
// φ folds 2×3→4 via double-step delay; σ schedules 5 stages × 6 steps across
// the 30-step cycle. The engine exposes the complete state via FSys6FullState.
//
// Feature:  F1.3.5 — Sys6 Operad Engine
// Phase:    1.3 — Core Pipeline
// Epic:     E3 — Embodied Cognitive Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "Sys6OperadEngine.h"
