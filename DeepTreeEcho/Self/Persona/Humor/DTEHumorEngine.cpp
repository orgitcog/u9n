// ═══════════════════════════════════════════════════════════════════════════
// DTEHumorEngine.cpp
// Compilation unit for FDTEHumorEngine — humour and wit generation engine.
//
// FDTEHumorEngine is a fully header-only class. All method bodies are defined
// inline in DTEHumorEngine.h to enable:
//   - Inline incongruity-resolution detection over concept-pair tensors
//   - Zero-overhead benign violation scoring via Eigen dot products
//   - Compile-time joke archetype switch for pattern classification
//
// The engine holds no joke text. It selects a humor style numerically and
// requests every utterance from FDTELanguageModelClient; when the model does
// not answer, nothing is said and LogDTEModel carries the reason.
//
// Feature:  F1.2.2 — DTE Humour Engine
// Phase:    1.2 — Persona Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "DTEHumorEngine.h"
