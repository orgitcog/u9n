/**
// Vendored from ReZorg/elizaos-cpp endocrine.cpp (namespace elizaos->dte).
 * ElizaOS C++ - Virtual Endocrine System implementation.
 *
 * Implements the 10-gland virtual endocrine system with hormone bus, valence-arousal
 * computation, and cognitive mode detection. See include/elizaos/endocrine.hpp for
 * the full design rationale.
 */
#include "EndocrineBus.h"
#include <algorithm>
#include <cmath>

namespace dte {

// ============================================================================
// Hormone Names
// ============================================================================

const char* hormoneName(HormoneId id) {
    switch (id) {
        case HormoneId::Cortisol:       return "cortisol";
        case HormoneId::Dopamine:       return "dopamine";
        case HormoneId::Serotonin:      return "serotonin";
        case HormoneId::Norepinephrine: return "norepinephrine";
        case HormoneId::Oxytocin:       return "oxytocin";
        case HormoneId::Thyroxine:      return "thyroxine";
        case HormoneId::Melatonin:      return "melatonin";
        case HormoneId::Insulin:        return "insulin";
        case HormoneId::Cytokine:       return "cytokine";
        case HormoneId::Anandamide:     return "anandamide";
        default:                        return "unknown";
    }
}

const char* cognitiveModeName(CognitiveMode mode) {
    switch (mode) {
        case CognitiveMode::Exploration:  return "exploration";
        case CognitiveMode::Exploitation: return "exploitation";
        case CognitiveMode::Rest:         return "rest";
        case CognitiveMode::Alarm:        return "alarm";
    }
    return "unknown";
}

// ============================================================================
// VirtualGland
// ============================================================================

VirtualGland::VirtualGland(GlandConfig config) : config_(std::move(config)) {}

void VirtualGland::respond(const Stimulus& stimulus, HormoneBus& bus) {
    // Each gland responds to stimuli based on its sensitivity and max production.
    // The response is additive: positive stimuli increase the hormone, negative decrease.
    double response = stimulus.intensity * config_.sensitivity;
    response = std::max(-config_.maxProduction, std::min(config_.maxProduction, response));

    double current = bus.get(config_.hormone);
    bus.set(config_.hormone, current + response);
}

void VirtualGland::tick(HormoneBus& bus) {
    // Exponential decay toward baseline (homeostasis).
    bus.decay(config_.hormone, config_.baseline, config_.decayRate);
}

// ============================================================================
// EndocrineSystem
// ============================================================================

EndocrineSystem::EndocrineSystem() {
    // Initialize the 10 virtual glands with biologically-inspired parameters.
    // Each gland has a distinct baseline, decay rate, and sensitivity profile.
    glands_.emplace_back(GlandConfig{HormoneId::Cortisol,       0.3, 0.05, 1.2, 0.25});
    glands_.emplace_back(GlandConfig{HormoneId::Dopamine,       0.4, 0.08, 1.0, 0.30});
    glands_.emplace_back(GlandConfig{HormoneId::Serotonin,      0.5, 0.03, 0.8, 0.20});
    glands_.emplace_back(GlandConfig{HormoneId::Norepinephrine, 0.4, 0.10, 1.5, 0.35});
    glands_.emplace_back(GlandConfig{HormoneId::Oxytocin,       0.4, 0.04, 0.7, 0.15});
    glands_.emplace_back(GlandConfig{HormoneId::Thyroxine,      0.5, 0.02, 0.6, 0.10});
    glands_.emplace_back(GlandConfig{HormoneId::Melatonin,      0.3, 0.06, 0.9, 0.20});
    glands_.emplace_back(GlandConfig{HormoneId::Insulin,        0.5, 0.04, 0.5, 0.15});
    glands_.emplace_back(GlandConfig{HormoneId::Cytokine,       0.2, 0.07, 1.1, 0.20});
    glands_.emplace_back(GlandConfig{HormoneId::Anandamide,     0.4, 0.05, 0.8, 0.20});

    reset();
}

void EndocrineSystem::submitStimulus(const Stimulus& stimulus) {
    pendingStimuli_.push_back(stimulus);
}

void EndocrineSystem::tick() {
    // 1. Process pending stimuli through glands.
    // Route stimuli to appropriate glands based on source type.
    for (const auto& stimulus : pendingStimuli_) {
        // Stimulus routing: different sources activate different glands.
        if (stimulus.source == "action_success" || stimulus.source == "goal_completed") {
            // Reward pathway: dopamine + serotonin up, cortisol down
            glands_[static_cast<std::size_t>(HormoneId::Dopamine)].respond(stimulus, bus_);
            glands_[static_cast<std::size_t>(HormoneId::Serotonin)].respond(
                Stimulus(stimulus.source, stimulus.intensity * 0.5), bus_);
            glands_[static_cast<std::size_t>(HormoneId::Cortisol)].respond(
                Stimulus(stimulus.source, -stimulus.intensity * 0.3), bus_);
        } else if (stimulus.source == "error_detected" || stimulus.source == "threat") {
            // Stress pathway: cortisol + norepinephrine up, dopamine + serotonin down
            glands_[static_cast<std::size_t>(HormoneId::Cortisol)].respond(stimulus, bus_);
            glands_[static_cast<std::size_t>(HormoneId::Norepinephrine)].respond(stimulus, bus_);
            glands_[static_cast<std::size_t>(HormoneId::Dopamine)].respond(
                Stimulus(stimulus.source, -stimulus.intensity * 0.5), bus_);
            glands_[static_cast<std::size_t>(HormoneId::Serotonin)].respond(
                Stimulus(stimulus.source, -stimulus.intensity * 0.4), bus_);
        } else if (stimulus.source == "social_interaction" || stimulus.source == "cooperation") {
            // Social pathway: oxytocin up
            glands_[static_cast<std::size_t>(HormoneId::Oxytocin)].respond(stimulus, bus_);
        } else if (stimulus.source == "novelty" || stimulus.source == "exploration" ||
                   stimulus.source == "novelty_detected") {
            // Curiosity pathway: dopamine + norepinephrine up
            glands_[static_cast<std::size_t>(HormoneId::Dopamine)].respond(
                Stimulus(stimulus.source, stimulus.intensity * 0.7), bus_);
            glands_[static_cast<std::size_t>(HormoneId::Norepinephrine)].respond(
                Stimulus(stimulus.source, stimulus.intensity * 0.5), bus_);
        } else if (stimulus.source == "fatigue" || stimulus.source == "rest_needed") {
            // Rest pathway: melatonin up, thyroxine down
            glands_[static_cast<std::size_t>(HormoneId::Melatonin)].respond(stimulus, bus_);
            glands_[static_cast<std::size_t>(HormoneId::Thyroxine)].respond(
                Stimulus(stimulus.source, -stimulus.intensity * 0.4), bus_);
        } else if (stimulus.source == "repair" || stimulus.source == "healing") {
            // Immune pathway: cytokine + anandamide up
            glands_[static_cast<std::size_t>(HormoneId::Cytokine)].respond(stimulus, bus_);
            glands_[static_cast<std::size_t>(HormoneId::Anandamide)].respond(
                Stimulus(stimulus.source, stimulus.intensity * 0.6), bus_);
        } else {
            // Generic stimulus: mild dopamine response
            glands_[static_cast<std::size_t>(HormoneId::Dopamine)].respond(
                Stimulus(stimulus.source, stimulus.intensity * 0.3), bus_);
        }
    }
    pendingStimuli_.clear();

    // 2. Tick all glands (decay toward homeostasis).
    for (auto& gland : glands_) {
        gland.tick(bus_);
    }

    // 3. Compute valence-arousal state.
    computeValenceArousal();

    // 4. Determine cognitive mode.
    computeCognitiveMode();

    ++tickCount_;
}

void EndocrineSystem::computeValenceArousal() {
    // Valence = (positive hormones - negative hormones) / normalizer
    // Positive: dopamine, serotonin, oxytocin, anandamide
    // Negative: cortisol, cytokine
    double positive = bus_.get(HormoneId::Dopamine) +
                      bus_.get(HormoneId::Serotonin) +
                      bus_.get(HormoneId::Oxytocin) +
                      bus_.get(HormoneId::Anandamide);
    double negative = bus_.get(HormoneId::Cortisol) +
                      bus_.get(HormoneId::Cytokine);

    vaState_.valence = std::max(-1.0, std::min(1.0, (positive - negative) / 2.0));

    // Arousal = (activating hormones - calming hormones) / normalizer
    // Activating: norepinephrine, thyroxine, cortisol
    // Calming: melatonin, anandamide, serotonin
    double activating = bus_.get(HormoneId::Norepinephrine) +
                        bus_.get(HormoneId::Thyroxine) +
                        bus_.get(HormoneId::Cortisol);
    double calming = bus_.get(HormoneId::Melatonin) +
                     bus_.get(HormoneId::Anandamide) +
                     bus_.get(HormoneId::Serotonin);

    vaState_.arousal = std::max(-1.0, std::min(1.0, (activating - calming) / 2.0));
}

void EndocrineSystem::computeCognitiveMode() {
    // Quadrant mapping in the valence-arousal circumplex:
    //   High arousal + positive valence -> Exploration
    //   Low arousal + positive valence  -> Exploitation
    //   Low arousal + negative valence  -> Rest
    //   High arousal + negative valence -> Alarm
    if (vaState_.arousal >= 0.0) {
        mode_ = (vaState_.valence >= 0.0) ? CognitiveMode::Exploration
                                          : CognitiveMode::Alarm;
    } else {
        mode_ = (vaState_.valence >= 0.0) ? CognitiveMode::Exploitation
                                          : CognitiveMode::Rest;
    }
}

void EndocrineSystem::reset() {
    // Reset all hormones to their gland baselines.
    for (const auto& gland : glands_) {
        bus_.set(gland.hormoneId(), gland.baseline());
    }
    vaState_ = ValenceArousalState{};
    pendingStimuli_.clear();
    tickCount_ = 0;
    // Derive the initial cognitive mode from the actual baseline hormone levels
    // rather than hardcoding, so the mode is consistent with the gland configuration.
    // With balanced baselines the agent starts in Exploration (positive valence,
    // non-negative arousal) -- the natural starting disposition for a curious agent.
    computeValenceArousal();
    computeCognitiveMode();
}

std::map<std::string, double> EndocrineSystem::hormoneLevelsMap() const {
    std::map<std::string, double> result;
    for (std::size_t i = 0; i < HORMONE_COUNT; ++i) {
        auto id = static_cast<HormoneId>(i);
        result[hormoneName(id)] = bus_.get(id);
    }
    return result;
}

}  // namespace dte
