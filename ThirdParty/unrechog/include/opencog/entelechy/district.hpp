#pragma once
/**
 * @file district.hpp
 * @brief CognitiveDistrict — Markov-blanket-wrapped cognitive subsystem
 *
 * Each cognitive subsystem (PLN, ECAN, VES glands, etc.) is modeled as a
 * "district" in the Cognitive City metaphor. Each district is wrapped in a
 * Markov blanket (Friston) that defines its statistical boundary, and
 * maintains a generative model whose free energy is minimized over time.
 *
 * The Civic Angel observes districts but does not control them.
 * Districts are the unit of free-energy accounting.
 *
 * Design: header-only logic, thin .cpp compilation unit.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/afi/types.hpp>
#include <opencog/afi/free_energy.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace opencog::entelechy {

// ============================================================================
// District Identifiers — named cognitive subsystems
// ============================================================================

/// Names for the standard cognitive districts
enum class DistrictId : uint8_t {
    HPA_QUARTER     = 0,   ///< VES glands (utilities)
    ACADEMY         = 1,   ///< PLN engine (university)
    MARKETPLACE     = 2,   ///< ECAN bank (markets)
    OBSERVATORY     = 3,   ///< o9c2 adapter (arts/right hemisphere)
    COURTHOUSE      = 4,   ///< Marduk adapter (city hall/left hemisphere)
    CLOCKTOWER      = 5,   ///< TCS brain (clocks)
    HARBOR          = 6,   ///< NPU adapter (port)
    TEMPLE          = 7,   ///< MoralPerception (ethics board)
    ARCHIVES        = 8,   ///< ValenceMemory/AtomSpace (library)
    AQUEDUCTS       = 9,   ///< HormoneBus (waterworks)
    NERVE_GRID      = 10,  ///< VNS (electrical grid)
};

inline constexpr size_t DISTRICT_COUNT = 11;

[[nodiscard]] constexpr std::string_view district_name(DistrictId id) noexcept {
    constexpr std::string_view names[] = {
        "HPA Quarter", "Academy", "Marketplace", "Observatory", "Courthouse",
        "Clocktower", "Harbor", "Temple", "Archives", "Aqueducts", "Nerve Grid"
    };
    return static_cast<size_t>(id) < DISTRICT_COUNT ? names[static_cast<size_t>(id)] : "Unknown";
}

// ============================================================================
// CognitiveDistrict — Markov-blanket-wrapped subsystem
// ============================================================================

/**
 * @brief A cognitive subsystem wrapped in a Markov blanket
 *
 * Each district maintains:
 * - A Markov blanket defining its statistical boundary
 * - A generative model (predictions vs observations)
 * - Free energy metrics (accuracy, complexity, coherence)
 * - An active/inactive flag for lifecycle management
 *
 * The Civic Angel reads district metrics to compute city-wide free energy.
 * Districts do NOT know about the Civic Angel.
 */
class CognitiveDistrict {
public:
    explicit CognitiveDistrict(DistrictId id)
        : id_(id) {}

    [[nodiscard]] DistrictId id() const noexcept { return id_; }
    [[nodiscard]] std::string_view name() const noexcept { return district_name(id_); }

    // ========================================================================
    // Markov blanket
    // ========================================================================

    void set_blanket(afi::MarkovBlanket blanket) {
        blanket_ = std::move(blanket);
    }

    [[nodiscard]] const afi::MarkovBlanket& blanket() const noexcept {
        return blanket_;
    }

    // ========================================================================
    // Generative model
    // ========================================================================

    /**
     * @brief Update the observation vector of the generative model
     *
     * Observations come from the district's sensory states (bus channels,
     * adapter telemetry, etc.). If the prediction vector is empty or
     * mismatched, it is resized and initialized to 0.5 (maximum-entropy prior).
     */
    void update_observations(const std::vector<float>& obs) {
        model_.observations = obs;

        // Ensure predictions vector matches observation size
        if (model_.predictions.size() != obs.size()) {
            model_.predictions.resize(obs.size(), 0.5f);
        }
    }

    /**
     * @brief Run one gradient-descent step on the generative model
     *
     * Moves predictions toward observations at the given learning rate.
     * This is the core perceptual inference update (active inference).
     */
    void update_predictions(float learning_rate = 0.01f) {
        afi::FreeEnergyMinimizer::update_predictions(model_, learning_rate);
    }

    [[nodiscard]] const afi::GenerativeModelState& model() const noexcept {
        return model_;
    }

    // ========================================================================
    // Free energy
    // ========================================================================

    /**
     * @brief Compute variational free energy for this district
     *
     * F = accuracy_error + complexity_cost
     */
    [[nodiscard]] afi::FreeEnergy compute_free_energy() const {
        return afi::FreeEnergyMinimizer::compute(model_);
    }

    // ========================================================================
    // Metrics
    // ========================================================================

    /**
     * @brief Recompute district metrics from the current generative model
     *
     * Fills DistrictMetrics from the free energy decomposition:
     * - free_energy = total free energy
     * - surprise = total free energy (surprise = -log evidence)
     * - complexity = KL complexity cost
     * - accuracy = 1 - accuracy_error, clamped [0,1]
     * - coherence = 1 - prediction_error, clamped [0,1]
     */
    void update_metrics() {
        const auto fe = compute_free_energy();
        metrics_.free_energy = fe.total();
        metrics_.surprise    = fe.surprise();
        metrics_.complexity  = fe.complexity_cost;
        metrics_.accuracy    = std::clamp(1.0f - fe.accuracy_error, 0.0f, 1.0f);
        metrics_.coherence   = std::clamp(1.0f - model_.prediction_error(), 0.0f, 1.0f);
    }

    [[nodiscard]] const DistrictMetrics& metrics() const noexcept {
        return metrics_;
    }

    // ========================================================================
    // Activity tracking
    // ========================================================================

    void set_active(bool active) noexcept { active_ = active; }
    [[nodiscard]] bool is_active() const noexcept { return active_; }

private:
    DistrictId id_;
    afi::MarkovBlanket blanket_;
    afi::GenerativeModelState model_;
    DistrictMetrics metrics_{};
    bool active_{true};
};

} // namespace opencog::entelechy
