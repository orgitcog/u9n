#pragma once
/**
 * @file precision.hpp
 * @brief Precision Weighting — maps prediction reliability to ECAN attention
 *
 * Implements the precision weighting mechanism from active inference:
 *
 *   precision = 1 / (error^2 + epsilon)
 *
 * High precision signals are reliable and deserve attention (high STI).
 * Low precision signals are noisy and should be ignored (low STI).
 *
 * This bridges Friston's Free Energy Principle with OpenCog's ECAN
 * (Economic Attention Network):
 *   - precision -> STI: reliable signals get attentional resources
 *   - STI -> precision: attended signals are treated as more reliable
 *
 * Design: static methods, header-only logic, thin .cpp compilation unit.
 */

#include <opencog/afi/types.hpp>
#include <opencog/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace opencog::afi {

/// Regularization constant to prevent division by zero
inline constexpr float PRECISION_EPSILON = 0.01f;

/// Minimum precision value (prevents degenerate zero-attention)
inline constexpr float PRECISION_MIN = 0.01f;

/// Maximum precision value (prevents attention monopoly)
inline constexpr float PRECISION_MAX = 100.0f;

// ============================================================================
// Precision Weighting
// ============================================================================

/**
 * @brief Precision weighting: maps prediction errors to attention (STI)
 *
 * All methods are static. Provides bidirectional mapping between
 * precision (inverse variance) and ECAN STI values.
 */
class PrecisionWeighting {
public:
    PrecisionWeighting() = delete;

    // -----------------------------------------------------------------------
    // Compute precision weights from generative model
    // -----------------------------------------------------------------------

    /**
     * @brief Compute precision weights from prediction errors
     *
     * For each signal i:
     *   error = |predictions[i] - observations[i]|
     *   precision = 1.0 / (error^2 + epsilon)
     *   Clamp precision to [PRECISION_MIN, PRECISION_MAX]
     *
     * High precision = low error variance = reliable signal.
     * The target AtomId is taken from the model's existing weights if
     * available; otherwise a null AtomId is used.
     *
     * @param model District's generative model state
     * @return Vector of precision weights, one per signal
     */
    [[nodiscard]] static std::vector<PrecisionWeight> compute(
        const GenerativeModelState& model) noexcept
    {
        const size_t n = std::min(model.predictions.size(),
                                  model.observations.size());
        std::vector<PrecisionWeight> weights;
        weights.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            const float error = std::abs(model.predictions[i] - model.observations[i]);
            float precision = 1.0f / (error * error + PRECISION_EPSILON);
            precision = std::clamp(precision, PRECISION_MIN, PRECISION_MAX);

            // Use existing target AtomId if available
            AtomId target = (i < model.weights.size())
                ? model.weights[i].target
                : ATOM_NULL;

            weights.emplace_back(target, precision);
        }

        return weights;
    }

    // -----------------------------------------------------------------------
    // Precision <-> STI bidirectional mapping
    // -----------------------------------------------------------------------

    /**
     * @brief Map precision to ECAN STI value
     *
     * sti = 2.0 * (precision / (precision + 1.0)) - 1.0
     *
     * Maps [0, inf) -> [-1, 1):
     *   precision = 0   -> sti = -1.0  (totally unreliable, negative attention)
     *   precision = 1   -> sti = 0.0   (neutral)
     *   precision = 10  -> sti ~= 0.82 (highly reliable, strong attention)
     *   precision -> inf -> sti -> 1.0  (perfect reliability, max attention)
     *
     * @param precision_value Non-negative precision
     * @return STI value in [-1, 1)
     */
    [[nodiscard]] static float precision_to_sti(float precision_value) noexcept {
        precision_value = std::max(0.0f, precision_value);
        return 2.0f * (precision_value / (precision_value + 1.0f)) - 1.0f;
    }

    /**
     * @brief Map ECAN STI to precision (inverse mapping)
     *
     * precision = (1.0 + sti) / (1.0 - sti + epsilon)
     *
     * Maps [-1, 1) -> [0, inf):
     *   sti = -1.0  -> precision ~= 0.0
     *   sti = 0.0   -> precision = 1.0
     *   sti = 0.82  -> precision ~= 10.1
     *   sti -> 1.0  -> precision -> inf
     *
     * @param sti STI value in [-1, 1)
     * @return Non-negative precision value
     */
    [[nodiscard]] static float sti_to_precision(float sti) noexcept {
        sti = std::clamp(sti, -1.0f, 1.0f - PRECISION_EPSILON);
        const float precision = (1.0f + sti) / (1.0f - sti + PRECISION_EPSILON);
        return std::max(0.0f, precision);
    }

    // -----------------------------------------------------------------------
    // Attention budget allocation
    // -----------------------------------------------------------------------

    /**
     * @brief Allocate attention budget proportional to precision weights
     *
     * Normalizes weights so they sum to total_budget.
     * Each signal gets a share of the budget proportional to its precision.
     *
     * If all weights are zero, budget is distributed uniformly.
     *
     * @param weights Precision weights (from compute())
     * @param total_budget Total attention budget to distribute (default 1.0)
     * @return Attention allocation per signal, summing to total_budget
     */
    [[nodiscard]] static std::vector<float> attention_allocation(
        const std::vector<PrecisionWeight>& weights,
        float total_budget = 1.0f) noexcept
    {
        std::vector<float> allocation(weights.size(), 0.0f);

        if (weights.empty()) return allocation;

        // Sum all precision values
        float total_precision = 0.0f;
        for (const auto& w : weights) {
            total_precision += w.value;
        }

        if (total_precision > 0.0f) {
            // Proportional allocation
            const float scale = total_budget / total_precision;
            for (size_t i = 0; i < weights.size(); ++i) {
                allocation[i] = weights[i].value * scale;
            }
        } else {
            // Uniform allocation (all weights zero)
            const float uniform = total_budget / static_cast<float>(weights.size());
            for (size_t i = 0; i < weights.size(); ++i) {
                allocation[i] = uniform;
            }
        }

        return allocation;
    }
};

} // namespace opencog::afi
