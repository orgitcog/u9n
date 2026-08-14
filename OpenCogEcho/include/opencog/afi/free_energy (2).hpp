#pragma once
/**
 * @file free_energy.hpp
 * @brief Free Energy Minimizer — variational free energy computation
 *
 * Implements Karl Friston's variational free energy minimization:
 *
 *   F = accuracy_error + complexity_cost
 *     = E_q[-log p(o|s)] + KL[q(s) || p(s)]
 *
 * The Civic Angel minimizes city-wide free energy across all cognitive
 * districts. Each district maintains a generative model whose predictions
 * are updated via gradient descent toward observations.
 *
 * Expected free energy (G) is used for prospective planning:
 *   G = ambiguity + risk
 *
 * Design: static methods, header-only logic, thin .cpp compilation unit.
 */

#include <opencog/afi/types.hpp>
#include <opencog/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace opencog::afi {

// ============================================================================
// Free Energy Minimizer
// ============================================================================

/**
 * @brief Utility class for variational free energy computation and minimization
 *
 * All methods are static. Operates on GenerativeModelState structs
 * that hold predictions, observations, and precision weights per district.
 */
class FreeEnergyMinimizer {
public:
    FreeEnergyMinimizer() = delete;

    // -----------------------------------------------------------------------
    // Compute free energy for a single district
    // -----------------------------------------------------------------------

    /**
     * @brief Compute variational free energy from a district's generative model
     *
     * F = accuracy_error + complexity_cost
     *
     * accuracy_error = mean squared prediction error (from model.prediction_error())
     *
     * complexity_cost = KL divergence approximation
     *   = mean squared deviation from prior mean (0.5) across predictions
     *   = sum((pred_i - 0.5)^2) / N
     *
     * The prior mean of 0.5 represents maximum uncertainty (uniform prior
     * on [0,1]). Predictions that deviate from 0.5 are "complex" — they
     * commit to specific values, increasing model complexity.
     *
     * @param model District's generative model state
     * @return FreeEnergy decomposition (accuracy + complexity)
     */
    [[nodiscard]] static FreeEnergy compute(const GenerativeModelState& model) noexcept {
        // Accuracy term: mean squared prediction error
        const float accuracy_error = model.prediction_error();

        // Complexity term: KL divergence approximation
        // mean deviation from prior mean (0.5) across predictions
        float complexity_cost = 0.0f;
        if (!model.predictions.empty()) {
            float kl_sum = 0.0f;
            for (const float pred : model.predictions) {
                const float dev = pred - 0.5f;
                kl_sum += dev * dev;
            }
            complexity_cost = kl_sum / static_cast<float>(model.predictions.size());
        }

        return FreeEnergy{accuracy_error, complexity_cost};
    }

    // -----------------------------------------------------------------------
    // Compute city-wide free energy across multiple districts
    // -----------------------------------------------------------------------

    /**
     * @brief Aggregate free energy across all cognitive districts
     *
     * F_city = sum(district_energy.total()) + inter_district_divergence
     *
     * The inter-district divergence term captures the cost of
     * inconsistency between districts (e.g., perception and memory
     * disagree about the world state).
     *
     * @param district_energies Free energy from each district
     * @param inter_district_divergence Additional divergence cost [0, inf)
     * @return Total city-wide free energy
     */
    [[nodiscard]] static float city_free_energy(
        const std::vector<FreeEnergy>& district_energies,
        float inter_district_divergence = 0.0f) noexcept
    {
        float total = 0.0f;
        for (const auto& fe : district_energies) {
            total += fe.total();
        }
        return total + inter_district_divergence;
    }

    // -----------------------------------------------------------------------
    // Update generative model predictions (gradient step)
    // -----------------------------------------------------------------------

    /**
     * @brief Gradient descent step: move predictions toward observations
     *
     * For each signal i:
     *   predictions[i] += learning_rate * (observations[i] - predictions[i])
     *
     * This is the core perceptual inference update in active inference.
     * With learning_rate = 1.0, predictions instantly match observations.
     * With smaller rates, the model smoothly tracks observations.
     *
     * @param model District's generative model state (modified in place)
     * @param learning_rate Step size [0, 1]; default 0.01
     */
    static void update_predictions(
        GenerativeModelState& model, float learning_rate = 0.01f) noexcept
    {
        learning_rate = std::clamp(learning_rate, 0.0f, 1.0f);

        const size_t n = std::min(model.predictions.size(),
                                  model.observations.size());
        for (size_t i = 0; i < n; ++i) {
            model.predictions[i] += learning_rate *
                (model.observations[i] - model.predictions[i]);
        }
    }

    // -----------------------------------------------------------------------
    // Expected free energy (for prospective planning)
    // -----------------------------------------------------------------------

    /**
     * @brief Compute expected free energy for a prospective action/policy
     *
     * G = ambiguity + risk
     *
     * ambiguity = mean squared deviation between predicted observations
     *             and current predictions (epistemic uncertainty under policy)
     *
     * risk = mean squared deviation between predicted observations
     *        and current observations (pragmatic divergence from preferred)
     *
     * Low G = the policy is expected to reduce both uncertainty and
     * divergence from desired outcomes. Used for developmental planning.
     *
     * @param model Current generative model state
     * @param predicted_observations Expected observations under the policy
     * @return ExpectedFreeEnergy decomposition (ambiguity + risk)
     */
    [[nodiscard]] static ExpectedFreeEnergy expected(
        const GenerativeModelState& model,
        const std::vector<float>& predicted_observations) noexcept
    {
        float ambiguity = 0.0f;
        float risk = 0.0f;

        const size_t n = std::min({
            model.predictions.size(),
            model.observations.size(),
            predicted_observations.size()
        });

        if (n == 0) {
            return ExpectedFreeEnergy{0.0f, 0.0f};
        }

        for (size_t i = 0; i < n; ++i) {
            // Ambiguity: how much the predicted future deviates from
            // current model predictions (epistemic value)
            const float a_diff = predicted_observations[i] - model.predictions[i];
            ambiguity += a_diff * a_diff;

            // Risk: how much the predicted future deviates from
            // current observations (pragmatic value)
            const float r_diff = predicted_observations[i] - model.observations[i];
            risk += r_diff * r_diff;
        }

        const float inv_n = 1.0f / static_cast<float>(n);
        return ExpectedFreeEnergy{ambiguity * inv_n, risk * inv_n};
    }
};

} // namespace opencog::afi
