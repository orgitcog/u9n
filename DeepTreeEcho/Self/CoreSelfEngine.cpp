// ═══════════════════════════════════════════════════════════════════════════
// CoreSelfEngine.cpp
// Compilation unit for FCoreSelfEngine — recursive self-model engine.
//
// FCoreSelfEngine is a fully header-only class. All method bodies are defined
// inline in CoreSelfEngine.h to enable:
//   - Inline Bayesian self-model update using Eigen rank-1 outer product
//   - Zero-overhead introspective prediction-error computation
//   - Compile-time self-report string construction for autognosis output
//
// Feature:  F1.3.3 — Core Self Engine
// Phase:    1.3 — Core Pipeline Infrastructure
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "CoreSelfEngine.h"
