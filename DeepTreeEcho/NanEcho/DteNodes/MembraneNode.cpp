// ═══════════════════════════════════════════════════════════════════════════
// MembraneNode.cpp
// Compilation unit for FMembraneNode — P-system membrane boundary node.
//
// FMembraneNode is a fully header-only class. All method bodies are defined
// inline in MembraneNode.h to enable:
//   - Inlined membrane-permeability gating using Eigen coefficient-wise ops
//   - Zero-overhead object transport rule evaluation at boundary crossings
//   - Compile-time dissolved/active membrane state transitions
//
// Feature:  F1.1.6 — Membrane Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "MembraneNode.h"
