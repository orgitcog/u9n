// ═══════════════════════════════════════════════════════════════════════════
// NeuroEndocrineAutoRL.cpp
// Compilation unit for FNeuroEndocrineAutoRL — hormonal RL modulation.
//
// FNeuroEndocrineAutoRL is a fully header-only class (≈33 KB). All method
// bodies are defined inline in NeuroEndocrineAutoRL.h to enable:
//   - Neuromodulator level updates inlined for tight numeric loops
//   - Reward gating through dopamine/serotonin axes inlined
//   - Cortisol-driven stress responses kept at call sites
//   - Oxytocin/norepinephrine social and arousal modulation inlined
//   - Homeostatic setpoint correction computed without virtual dispatch
//
// The Neuro-Endocrine AutoRL system models biological hormonal regulation
// as a meta-learning layer over the standard RL policy gradient. Each of
// the eight modelled neuromodulators (dopamine, serotonin, norepinephrine,
// acetylcholine, cortisol, oxytocin, endorphin, GABA) adjusts exploration,
// learning rate, risk tolerance, and social reward weighting in real time.
//
// Feature:  F1.4.7 — Neuro-Endocrine Auto-RL
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "NeuroEndocrineAutoRL.h"
