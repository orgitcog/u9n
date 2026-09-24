// ═══════════════════════════════════════════════════════════════════════════
// UniversalKnowledgeLattice.cpp
// Compilation unit for FUniversalKnowledgeLattice — Level 8 Matula lattice.
//
// FUniversalKnowledgeLattice is a fully header-only class. All method bodies
// are defined inline in UniversalKnowledgeLattice.h to enable:
//   - GCD meet and LCM join operations inlined for integer arithmetic locality
//   - Prime factorisation and depth computation inlined alongside insertion
//   - Neighbourhood query BFS inlined with attention-weighted scoring
//   - Semiring axiom verification inlined for compile-time provable correctness
//
// The Universal Knowledge Lattice completes the echo-ex-matula correspondence
// at the lattice level: the integers under GCD/LCM form a complete distributive
// lattice that IS the knowledge lattice. Meet (∧) = GCD gives shared knowledge;
// Join (∨) = LCM gives combined knowledge; ⊥ = 1 is the empty tree (no knowledge);
// ⊤ = ∞ is all knowledge (unreachable). The lattice is also a semiring (ℕ, GCD,
// LCM, ∞, 1). Every Matula prime is a node; every composite is a fusion.
// Simplex incidence maps System N's knowledge to an (N-1)-simplex with Pascal
// row N as face-vector.
//
// Feature:  F1.8.3 — Level 8 Complete Matula Knowledge Lattice
// Phase:    1.8 — Cosmic Order (Level 8)
// Epic:     E7 — Cosmic Order Unified Consciousness
// ═══════════════════════════════════════════════════════════════════════════

#include "UniversalKnowledgeLattice.h"
