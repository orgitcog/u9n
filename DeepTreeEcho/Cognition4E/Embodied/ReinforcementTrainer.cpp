// ═══════════════════════════════════════════════════════════════════════════
// ReinforcementTrainer.cpp
// Compilation unit for FReinforcementTrainer — PPO/A3C online learner.
//
// FReinforcementTrainer is a fully header-only class. All method bodies
// are defined inline in ReinforcementTrainer.h to enable:
//   - Rollout buffer appends inlined for zero-overhead recording
//   - GAE (Generalized Advantage Estimation) computed inline
//   - PPO clip objective inlined for tight Eigen matrix ops
//   - Value network and policy network forward passes inlined
//   - Entropy regularization coefficient updated inline
//
// The Reinforcement Trainer manages the online PPO update cycle for DTE's
// embodied policy. It works in concert with the NeuroEndocrineAutoRL
// modulator, which adjusts exploration bonuses and reward scaling in
// response to simulated hormonal state.
//
// Feature:  F1.4.8 — Reinforcement Trainer
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "ReinforcementTrainer.h"
