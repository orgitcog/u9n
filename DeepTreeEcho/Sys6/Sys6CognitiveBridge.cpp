// ═══════════════════════════════════════════════════════════════════════════
// Sys6CognitiveBridge.cpp  (Sys6/ forwarding shim)
// Compilation unit for the Sys6/ backward-compatibility forwarding header.
//
// The canonical implementation lives in DeepTreeEcho/Core/Sys6CognitiveBridge.h
// and DeepTreeEcho/Core/Sys6CognitiveBridge.cpp. This shim exists only to
// provide a compilation unit that pulls in the forwarding header at its legacy
// location, ensuring that any translation unit including the Sys6/ path still
// produces a valid object file.
//
// USys6CognitiveBridge — bridges the sys6 operad 30-step clock to the 12-step
// cognitive cycle using OEIS A000081 nested-shell mappings. Shell transitions
// advance thread-pair permutations P(i,j) and triadic complements MP1/MP2.
// Three mapping modes are supported: Direct (modular), Interleaved (stage-based),
// and Hierarchical (nested-shell A000081).
//
// Feature:  F1.3.6 — Sys6 Cognitive Bridge
// Phase:    1.3 — Core Pipeline
// Epic:     E3 — Embodied Cognitive Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "Sys6CognitiveBridge.h"
