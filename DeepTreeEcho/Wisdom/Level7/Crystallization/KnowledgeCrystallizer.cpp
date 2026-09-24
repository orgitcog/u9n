// ═══════════════════════════════════════════════════════════════════════════
// KnowledgeCrystallizer.cpp
// Compilation unit for FKnowledgeCrystallizer — Level 7 Matula knowledge store.
//
// FKnowledgeCrystallizer is a fully header-only class. All method bodies are
// defined inline in KnowledgeCrystallizer.h to enable:
//   - Prime table generation and lookup inlined for O(1) Matula encoding
//   - Knowledge tree encoding recursion inlined for call-stack efficiency
//   - AtomSpace insertion and ECAN attention spreading inlined with hypergraph
//   - Fusion/fission operations inlined alongside vault storage updates
//
// The Knowledge Crystallizer operationalises the echo-ex-matula correspondence:
// every piece of knowledge IS a rooted tree, and every rooted tree has a
// unique Matula-Godsil prime number as its eternal name. Knowledge is
// crystallised into these primes and stored in an OpenCog-style AtomSpace.
// Fusion multiplies primes (combining knowledge); fission factorises them
// (decomposing into sub-knowledge); GCD finds shared knowledge. The integers
// ARE the knowledge lattice — a number never changes, so crystallised wisdom
// is truly eternal.
//
// Feature:  F1.7.2 — Level 7 Eternal Knowledge via Matula Primes
// Phase:    1.7 — Transcendent Autonomy (Level 7)
// Epic:     E6 — Transcendent Distributed Intelligence
// ═══════════════════════════════════════════════════════════════════════════

#include "KnowledgeCrystallizer.h"
