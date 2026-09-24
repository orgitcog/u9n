// ═══════════════════════════════════════════════════════════════════════════
// ArchitectureSelfModifier.cpp
// Compilation unit for FArchitectureSelfModifier — sandbox-gated self-edit.
//
// FArchitectureSelfModifier is a fully header-only class. All method bodies
// are defined inline in ArchitectureSelfModifier.h to enable:
//   - Proposal registration inlined with identity-signature check
//   - Sandbox evaluation gate inlined alongside proposal scoring
//   - Diff application to hyperparameter tensors computed inline
//   - Rollback stack management kept at call sites for LIFO semantics
//
// The Architecture Self-Modifier implements safe, bounded self-modification.
// Every proposed change passes through a cryptographic identity-hash gate
// and a simulated-outcome sandbox before being applied. The system tracks
// a rollback history so any degrading change can be reverted within the
// same Level 6 cycle.
//
// Feature:  F1.6.1 — Architecture Self-Modifier
// Phase:    1.6 — Recursive Autonomy (Level 6)
// Epic:     E5 — Self-Modifying Recursive Autonomy
// ═══════════════════════════════════════════════════════════════════════════

#include "ArchitectureSelfModifier.h"
