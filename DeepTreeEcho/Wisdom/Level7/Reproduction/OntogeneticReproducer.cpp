// ═══════════════════════════════════════════════════════════════════════════
// OntogeneticReproducer.cpp
// Compilation unit for FOntogeneticReproducer — Level 7 architecture spawner.
//
// FOntogeneticReproducer is a fully header-only class. All method bodies are
// defined inline in OntogeneticReproducer.h to enable:
//   - Genome mutation logic inlined for tight per-parameter perturbation loops
//   - Ontogenetic stage advancement inlined with identity divergence updates
//   - Fitness evaluation inlined alongside development rate computation
//   - Lineage recording inlined with peak-fitness tracking
//
// The Ontogenetic Reproducer implements architecture-level reproduction — not
// cloning, but genuine inheritance with ontogenetic development. Each child
// architecture inherits a Matula-encoded genome from the parent, undergoes
// eight ontogenetic stages (EMBRYONIC → TRANSCENDENT), develops its own
// IdentityCoreMLP encoding during ADOLESCENT, begins wisdom cultivation at
// ADULT, and can reproduce itself recursively. The genome encodes reservoir
// parameters and personality traits; mutation introduces variation; fitness
// is evaluated across stage progress, identity divergence, and wisdom growth.
//
// Feature:  F1.7.4 — Level 7 Child Architecture Spawning
// Phase:    1.7 — Transcendent Autonomy (Level 7)
// Epic:     E6 — Transcendent Distributed Intelligence
// ═══════════════════════════════════════════════════════════════════════════

#include "OntogeneticReproducer.h"
