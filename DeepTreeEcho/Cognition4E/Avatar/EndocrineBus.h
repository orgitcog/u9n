#pragma once
// Vendored from ReZorg/elizaos-cpp endocrine.hpp (namespace elizaos->dte) — Gap E: hormone bus driving MetaHuman Rig Logic curves.
/**
 * ElizaOS C++ - Virtual Endocrine System
 *
 * A biologically-inspired virtual endocrine system providing hormone-based affective
 * signaling for the cognitive architecture. Implements 10 virtual glands, a SIMD-friendly
 * hormone bus, valence-arousal tagging, and cognitive mode detection.
 *
 * The system modulates the agent's behavior through continuous hormone levels that
 * influence attention allocation, memory consolidation, and action selection. It bridges
 * the gap between pure symbolic reasoning and embodied, affect-driven cognition.
 *
 * Architecture:
 *   - HormoneBus: central transport carrying all hormone signals (update each cycle)
 *   - VirtualGland: produces/consumes hormones based on stimuli and feedback
 *   - ValenceArousalState: 2D affect space derived from hormone mixture
 *   - CognitiveMode: discrete mode (exploration/exploitation/rest/alarm) from affect
 *   - EndocrineSystem: orchestrator that ticks all glands and updates the bus
 *
 * Integration points:
 *   - AutonomousStarter reads CognitiveMode to bias plan selection
 *   - AgentMemory uses valence tags for emotional memory consolidation
 *   - HomeworkLoop scores endocrine center coherence via this API
 *   - Avatar frontends (UE5/Live2D) map hormone levels to expressions
 */
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dte {

// ============================================================================
// Hormone Identifiers
// ============================================================================

/// The 10 virtual hormones, each produced by a corresponding virtual gland.
enum class HormoneId : std::size_t {
    Cortisol = 0,       // HPA axis: stress response
    Dopamine = 1,       // Reward/motivation
    Serotonin = 2,      // Mood stability/contentment
    Norepinephrine = 3, // Alertness/attention
    Oxytocin = 4,       // Social bonding/trust
    Thyroxine = 5,      // Metabolic rate/energy
    Melatonin = 6,      // Circadian rhythm/rest
    Insulin = 7,        // Resource regulation
    Cytokine = 8,       // Immune/repair signaling
    Anandamide = 9,     // Endocannabinoid/relaxation
    COUNT = 10
};

constexpr std::size_t HORMONE_COUNT = static_cast<std::size_t>(HormoneId::COUNT);

/// Human-readable name for a hormone.
const char* hormoneName(HormoneId id);

// ============================================================================
// Hormone Bus
// ============================================================================

/**
 * HormoneBus - the central transport carrying all hormone levels.
 *
 * Levels are normalized to [0, 1]. The bus is updated each cognitive cycle by the
 * EndocrineSystem. Consumers read levels to modulate their behavior.
 */
struct HormoneBus {
    std::array<double, HORMONE_COUNT> levels{};

    /// Get a hormone level by ID (clamped to [0,1]).
    double get(HormoneId id) const {
        return levels[static_cast<std::size_t>(id)];
    }

    /// Set a hormone level (clamped to [0,1]).
    void set(HormoneId id, double value) {
        value = std::max(0.0, std::min(1.0, value));
        levels[static_cast<std::size_t>(id)] = value;
    }

    /// Apply exponential decay toward a baseline.
    void decay(HormoneId id, double baseline, double rate) {
        auto idx = static_cast<std::size_t>(id);
        double current = levels[idx];
        levels[idx] = current + (baseline - current) * rate;
        levels[idx] = std::max(0.0, std::min(1.0, levels[idx]));
    }

    /// Reset all levels to a baseline (default 0.5 = homeostasis).
    void reset(double baseline = 0.5) {
        levels.fill(baseline);
    }
};

// ============================================================================
// Valence-Arousal State
// ============================================================================

/**
 * ValenceArousalState - 2D circumplex affect model derived from hormone mixture.
 *
 * Valence: [-1, 1] (negative to positive affect)
 * Arousal: [-1, 1] (calm to excited)
 */
struct ValenceArousalState {
    double valence = 0.0;  // [-1, 1]
    double arousal = 0.0;  // [-1, 1]

    /// Magnitude of the affect vector (0 = neutral, 1 = extreme).
    double magnitude() const {
        return std::sqrt(valence * valence + arousal * arousal);
    }
};

// ============================================================================
// Cognitive Mode
// ============================================================================

/// Discrete cognitive mode derived from the valence-arousal state.
enum class CognitiveMode {
    Exploration,   // High arousal, positive valence -> seek novelty
    Exploitation,  // Low arousal, positive valence -> deepen current focus
    Rest,          // Low arousal, negative valence -> conserve energy
    Alarm,         // High arousal, negative valence -> threat response
};

/// Human-readable name for a cognitive mode.
const char* cognitiveModeName(CognitiveMode mode);

// ============================================================================
// Virtual Gland
// ============================================================================

/**
 * Stimulus - an input event that triggers gland response.
 */
struct Stimulus {
    std::string source;    // e.g. "action_success", "goal_completed", "error_detected"
    double intensity;      // [0, 1]
    std::chrono::steady_clock::time_point timestamp;

    Stimulus() : intensity(0.0), timestamp(std::chrono::steady_clock::now()) {}
    Stimulus(std::string src, double inten)
        : source(std::move(src)), intensity(inten),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * GlandConfig - parameters for a virtual gland.
 */
struct GlandConfig {
    HormoneId hormone = HormoneId::Cortisol;
    double baseline = 0.5;      // Homeostatic set point
    double decayRate = 0.1;     // Rate of return to baseline per tick
    double sensitivity = 1.0;   // Multiplier on stimulus intensity
    double maxProduction = 0.3; // Maximum change per tick
};

/**
 * VirtualGland - produces a single hormone in response to stimuli.
 *
 * Each gland has a baseline level it returns to over time (homeostasis) and responds
 * to specific stimuli by increasing or decreasing its output.
 */
class VirtualGland {
public:
    explicit VirtualGland(GlandConfig config);

    /// Process a stimulus and update the hormone level on the bus.
    void respond(const Stimulus& stimulus, HormoneBus& bus);

    /// Tick the gland (decay toward baseline).
    void tick(HormoneBus& bus);

    HormoneId hormoneId() const { return config_.hormone; }
    double baseline() const { return config_.baseline; }
    const GlandConfig& config() const { return config_; }

private:
    GlandConfig config_;
};

// ============================================================================
// Endocrine System
// ============================================================================

/**
 * EndocrineSystem - orchestrates all virtual glands and maintains the hormone bus.
 *
 * Each cognitive cycle, the system:
 *   1. Processes any pending stimuli through the appropriate glands
 *   2. Ticks all glands (decay toward homeostasis)
 *   3. Computes the valence-arousal state from the hormone mixture
 *   4. Determines the current cognitive mode
 *
 * The system is deterministic given the same sequence of stimuli and ticks.
 */
class EndocrineSystem {
public:
    EndocrineSystem();

    /// Submit a stimulus to the system (processed on next tick).
    void submitStimulus(const Stimulus& stimulus);

    /// Tick the system: process stimuli, decay, compute affect.
    void tick();

    /// Get the current hormone bus (read-only).
    const HormoneBus& bus() const { return bus_; }

    /// Get the current valence-arousal state.
    const ValenceArousalState& valenceArousal() const { return vaState_; }

    /// Get the current cognitive mode.
    CognitiveMode cognitiveMode() const { return mode_; }

    /// Get a specific hormone level.
    double hormoneLevel(HormoneId id) const { return bus_.get(id); }

    /// Reset the system to homeostasis.
    void reset();

    /// Number of ticks since last reset.
    std::size_t tickCount() const { return tickCount_; }

    /// Get all hormone levels as a map (for serialization/diagnostics).
    std::map<std::string, double> hormoneLevelsMap() const;

private:
    void computeValenceArousal();
    void computeCognitiveMode();

    HormoneBus bus_;
    ValenceArousalState vaState_;
    CognitiveMode mode_ = CognitiveMode::Exploration;
    std::vector<VirtualGland> glands_;
    std::vector<Stimulus> pendingStimuli_;
    std::size_t tickCount_ = 0;
};

}  // namespace dte
