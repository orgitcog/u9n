// ═══════════════════════════════════════════════════════════════════════════
// CognitiveReadoutNode.cpp
// Compilation unit for FCognitiveReadoutNode — supervised readout layer.
//
// FCognitiveReadoutNode is a fully header-only class. All method bodies are
// defined inline in CognitiveReadoutNode.h to enable:
//   - Eigen-optimised ridge-regression weight updates
//   - Zero-copy projection of reservoir activations to output dimensions
//   - Inline softmax and argmax for classification readout paths
//
// Feature:  F1.1.2 — Cognitive Readout Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "CognitiveReadoutNode.h"
