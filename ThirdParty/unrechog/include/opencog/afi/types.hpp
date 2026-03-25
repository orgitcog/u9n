#pragma once
/**
 * @file types.hpp
 * @brief Core types for Active Free-energy Inference (AFI)
 *
 * Implements Karl Friston's Free Energy Principle as a unifying mathematical
 * framework for the Cognitive City architecture. Each cognitive district is
 * wrapped in a Markov blanket; the Civic Angel minimizes city-wide free energy.
 *
 * Precision weighting maps to ECAN's attention economy (STI).
 */

#include <opencog/core/types.hpp>

#include <cmath>
#include <vector>

namespace opencog::afi {

// ============================================================================
// Markov Blanket — Boundary definition for a cognitive district
// ============================================================================

/**
 * @brief Defines the statistical boundary between a district and its environment
 *
 * The blanket separates internal states from external states.
 * Sensory states carry information inward; active states carry influence outward.
 */
struct MarkovBlanket {
    std::vector<AtomId> sensory_states;   ///< Incoming from environment
    std::vector<AtomId> active_states;    ///< Outgoing to environment
    std::vector<AtomId> internal_states;  ///< Hidden internal states
    std::vector<AtomId> external_states;  ///< States outside the blanket

    [[nodiscard]] size_t blanket_size() const noexcept {
        return sensory_states.size() + active_states.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return sensory_states.empty() && active_states.empty()
            && internal_states.empty();
    }
};

// ============================================================================
// Free Energy — Variational free energy decomposition
// ============================================================================

/**
 * @brief Variational free energy decomposition
 *
 * F = accuracy_error + complexity_cost
 *   = -E_q[log p(o|s)] + KL[q(s) || p(s)]
 *
 * Low free energy = good model fit with minimal complexity.
 */
struct alignas(8) FreeEnergy {
    float accuracy_error{0.0f};  ///< Prediction error at blanket boundary (non-negative)
    float complexity_cost{0.0f}; ///< Model complexity (KL from prior, non-negative)

    [[nodiscard]] float total() const noexcept {
        return accuracy_error + complexity_cost;
    }

    [[nodiscard]] float surprise() const noexcept { return total(); }

    /// Bayesian model evidence (negative free energy)
    [[nodiscard]] float evidence() const noexcept { return -total(); }
};

static_assert(sizeof(FreeEnergy) == 8);

// ============================================================================
// Precision Weight — Inverse variance, maps to attention
// ============================================================================

/**
 * @brief Precision weight for a specific signal/atom
 *
 * High precision = reliable signal = deserves attention (high STI).
 * Low precision = noisy signal = ignore (low STI).
 * Maps directly to ECAN's STI via PrecisionWeighting adapter.
 */
struct PrecisionWeight {
    AtomId target;          ///< Which atom this precision applies to
    float value{1.0f};      ///< [0, inf): inverse variance

    constexpr PrecisionWeight() = default;
    constexpr PrecisionWeight(AtomId t, float v) : target(t), value(v) {}
};

// ============================================================================
// Generative Model State — Per-district prediction state
// ============================================================================

/**
 * @brief State of a district's generative model
 *
 * Each district maintains predictions about its sensory inputs.
 * Prediction errors drive free energy; the system learns to minimize these.
 */
struct GenerativeModelState {
    std::vector<float> predictions;       ///< Predicted sensory values
    std::vector<float> observations;      ///< Actual sensory values
    std::vector<PrecisionWeight> weights; ///< Per-signal precision weights

    /// Mean squared prediction error (accuracy term)
    [[nodiscard]] float prediction_error() const noexcept {
        if (predictions.size() != observations.size() || predictions.empty())
            return 0.0f;
        float sum = 0.0f;
        for (size_t i = 0; i < predictions.size(); ++i) {
            float diff = predictions[i] - observations[i];
            sum += diff * diff;
        }
        return sum / static_cast<float>(predictions.size());
    }

    /// Weighted prediction error (precision-modulated)
    [[nodiscard]] float weighted_prediction_error() const noexcept {
        if (predictions.size() != observations.size() || predictions.empty())
            return 0.0f;
        float sum = 0.0f;
        float total_weight = 0.0f;
        for (size_t i = 0; i < predictions.size(); ++i) {
            float w = (i < weights.size()) ? weights[i].value : 1.0f;
            float diff = predictions[i] - observations[i];
            sum += w * diff * diff;
            total_weight += w;
        }
        return total_weight > 0.0f ? sum / total_weight : 0.0f;
    }
};

// ============================================================================
// Expected Free Energy — For planning and development
// ============================================================================

/**
 * @brief Expected free energy for future actions/policies
 *
 * G = ambiguity + risk
 *   = E_q[H(o|s,pi)] + KL[q(s|pi) || p(s)]
 *
 * Used by the Civic Angel for developmental planning.
 */
struct alignas(8) ExpectedFreeEnergy {
    float ambiguity{0.0f};  ///< Expected uncertainty under policy
    float risk{0.0f};       ///< Divergence from preferred outcomes

    [[nodiscard]] float total() const noexcept {
        return ambiguity + risk;
    }
};

static_assert(sizeof(ExpectedFreeEnergy) == 8);

} // namespace opencog::afi
