// ═══════════════════════════════════════════════════════════════════════════
// EchoReservoirNode.cpp
// Compilation unit for FEchoReservoirNode — the Echo State Network reservoir.
//
// FEchoReservoirNode is a fully header-only class. All method bodies are
// defined inline in EchoReservoirNode.h to enable:
//   - Eigen SIMD vectorisation of reservoir state updates
//   - Power-iteration spectral-radius normalisation inlined at call sites
//   - Template-friendly leaky integration loop with constant folding
//
// Feature:  F1.1.1 — Echo Reservoir Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "EchoReservoirNode.h"
