// ═══════════════════════════════════════════════════════════════════════════
// SomaticDecisionEngine.cpp
// Compilation unit for FSomaticDecisionEngine — body-schema decision node.
//
// FSomaticDecisionEngine is a fully header-only class. All method bodies are
// defined inline in SomaticDecisionEngine.h to enable:
//   - Inline interoceptive signal integration via Eigen weighted sum
//   - Zero-overhead action affordance scoring using sigmoid gating
//   - Compile-time physiological marker lookup for somatic markers
//
// Feature:  F1.2.1 — Somatic Decision Engine
// Phase:    1.2 — Persona Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "SomaticDecisionEngine.h"
