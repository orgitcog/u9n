// ═══════════════════════════════════════════════════════════════════════════
// EchobeatNode.cpp
// Compilation unit for FEchobeatNode — 12-step cognitive rhythm generator.
//
// FEchobeatNode is a fully header-only class. All method bodies are defined
// inline in EchobeatNode.h to enable:
//   - Inlined beat-phase transitions for zero-overhead 12-step cycle dispatch
//   - Eigen-backed temporal coupling computation for rhythm synchronisation
//   - Compile-time phase enumeration so the switch is table-optimised
//
// Feature:  F1.1.4 — Echobeat Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "EchobeatNode.h"
