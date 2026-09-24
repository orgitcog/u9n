// ═══════════════════════════════════════════════════════════════════════════
// NanEchoSelfTrainer.cpp
// Compilation unit for FNanEchoSelfTrainer — offline self-distillation.
//
// FNanEchoSelfTrainer is a fully header-only class. All method bodies are
// defined inline in NanEchoSelfTrainer.h to enable:
//   - Introspection sample buffering inlined for zero-copy appends
//   - Training batch assembly with importance weighting inlined
//   - JSONL serialisation for NanEcho export kept at call sites
//   - Token-length estimation inlined for batch sizing
//
// The NanEcho Self-Trainer is the offline counterpart to the Online AutoRL
// Runtime. During Level 6 cycles it exports curated introspection records
// as JSONL training batches for fine-tuning the NanEcho language model
// (the system's inner monologue generator), closing the loop between
// runtime self-reflection and model weight improvement.
//
// Feature:  F1.6.4 — NanEcho Self-Trainer
// Phase:    1.6 — Recursive Autonomy (Level 6)
// Epic:     E5 — Self-Modifying Recursive Autonomy
// ═══════════════════════════════════════════════════════════════════════════

#include "NanEchoSelfTrainer.h"
