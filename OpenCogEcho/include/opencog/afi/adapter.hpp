#pragma once
/**
 * @file adapter.hpp
 * @brief AFI (Active Free-energy Inference) Endocrine Adapter
 *
 * Bridges the Free Energy Principle subsystem with the Virtual Endocrine System.
 * Monitors cognitive districts (wrapped in Markov blankets), computes city-wide
 * free energy and inter-district divergence, and emits hormonal signals on
 * significant changes.
 *
 * PRECISION WEIGHTING -> ECAN STI:
 *   Each district's generative model produces precision weights (inverse variance).
 *   High precision = reliable signal = deserves attention (high STI).
 *   compute_sti_adjustments() returns recommended STI changes for ECAN.
 *
 * FEEDBACK SIGNALS (edge-triggered):
 *   Mean district free energy spike (current > prev * 1.5 AND current > 3.0)
 *     -> CORTISOL +0.05, NE +0.05  (prediction crisis)
 *   Mean district free energy drop (current < prev * 0.7 AND prev > 2.0)
 *     -> DA_PHASIC +0.05  (model improvement reward)
 *
 * The AFI adapter does NOT own districts. Districts are registered externally
 * (non-owning pointers), consistent with the Cognitive City pattern where the
 * Civic Angel observes but does not control.
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/afi/precision.hpp>
#include <opencog/entelechy/district.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace opencog::endo {

class AFIEndocrineAdapter : public EndocrineConnector {
public:
    explicit AFIEndocrineAdapter(HormoneBus& bus)
        : EndocrineConnector(bus)
    {}

    // ========================================================================
    // District Management
    // ========================================================================

    /**
     * @brief Register a district for free energy monitoring
     *
     * Non-owning pointer. The caller is responsible for ensuring the
     * district outlives this adapter.
     */
    void register_district(entelechy::CognitiveDistrict* district) {
        if (district) {
            districts_.push_back(district);
        }
    }

    // ========================================================================
    // District Metrics Update
    // ========================================================================

    /**
     * @brief Update all district metrics and compute city-wide aggregates
     *
     * For each registered active district:
     * 1. Call district->update_metrics() to refresh from generative model
     * 2. Sum free energies for city_free_energy_
     * 3. Compute mean free energy for scale-invariant edge detection
     * 4. Compute variance of free energies for inter_district_divergence_
     */
    void update_districts() {
        float sum_fe = 0.0f;
        std::vector<float> energies;
        energies.reserve(districts_.size());

        for (auto* district : districts_) {
            if (district && district->is_active()) {
                district->update_metrics();
                float fe = district->metrics().free_energy;
                sum_fe += fe;
                energies.push_back(fe);
            }
        }

        active_district_count_ = energies.size();
        city_free_energy_ = sum_fe;
        mean_district_free_energy_ = sum_fe / static_cast<float>(std::max<size_t>(1, active_district_count_));

        // Compute variance of district free energies
        if (energies.size() >= 2) {
            const float n = static_cast<float>(energies.size());
            float mean = sum_fe / n;
            float var_sum = 0.0f;
            for (float e : energies) {
                float diff = e - mean;
                var_sum += diff * diff;
            }
            inter_district_divergence_ = var_sum / n;
        } else {
            inter_district_divergence_ = 0.0f;
        }
    }

    // ========================================================================
    // Precision Weighting -> ECAN STI
    // ========================================================================

    /**
     * @brief Compute recommended STI adjustments from precision weighting
     *
     * For each registered district, computes precision weights from its
     * generative model and maps them to ECAN STI values. Returns a vector
     * of (AtomId, recommended_sti) pairs.
     *
     * Only includes weights with non-null AtomIds.
     */
    [[nodiscard]] std::vector<std::pair<AtomId, float>> compute_sti_adjustments() const {
        std::vector<std::pair<AtomId, float>> adjustments;

        for (const auto* district : districts_) {
            if (!district || !district->is_active()) continue;

            auto weights = afi::PrecisionWeighting::compute(district->model());
            for (const auto& pw : weights) {
                if (pw.target != ATOM_NULL) {
                    float sti = afi::PrecisionWeighting::precision_to_sti(pw.value);
                    adjustments.emplace_back(pw.target, sti);
                }
            }
        }

        return adjustments;
    }

    // ========================================================================
    // Adapter interface (Phase 3: READ from bus)
    // ========================================================================

    /**
     * @brief Apply endocrine modulation — read bus and update districts
     *
     * Called in Phase 3 of the tick pipeline. Updates district metrics
     * and computes city-wide aggregates.
     */
    void apply_endocrine_modulation(const HormoneBus& /*bus*/) {
        update_districts();
    }

    // ========================================================================
    // Feedback: edge-triggered signals (Phase 4)
    // ========================================================================

    /**
     * @brief Write edge-triggered feedback signals to the bus
     *
     * Follows the Marduk adapter pattern: compare mean per-district free
     * energy to previous state and emit hormonal signals on significant changes.
     * The public city_free_energy() remains the city-wide sum for consumers
     * that need aggregate load; edge triggers use the mean so registering more
     * active districts does not create false spikes by count alone.
     *
     * Edge triggers:
     *   1. Mean district FE spike (current > prev * 1.5 AND current > 3.0)
     *      -> CORTISOL +0.05, NE +0.05 (prediction crisis)
     *   2. Mean district FE drop (current < prev * 0.7 AND prev > 2.0)
     *      -> DA_PHASIC +0.05 (model improvement reward)
     */
    void apply_feedback() {
        // Skip edge-detection on the very first call: previous mean district
        // free energy starts at 0.0, which would otherwise make the spike condition
        // (current > prev * 1.5 && current > 3.0) trivially satisfied by
        // any mean_district_free_energy_ > 3.0 on tick one — a false "spike" against
        // a baseline that was never actually observed.
        if (has_prev_mean_district_free_energy_) {
            // --- 1. Mean district free energy spike → prediction crisis ---
            if (mean_district_free_energy_ > prev_mean_district_free_energy_ * 1.5f &&
                mean_district_free_energy_ > 3.0f) {
                bus_.produce(HormoneId::CORTISOL, 0.05f);
                bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
            }

            // --- 2. Mean district free energy drop → model improvement reward ---
            if (mean_district_free_energy_ < prev_mean_district_free_energy_ * 0.7f &&
                prev_mean_district_free_energy_ > 2.0f) {
                bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.05f);
            }
        }

        // Update previous state for next tick's edge detection
        prev_mean_district_free_energy_ = mean_district_free_energy_;
        has_prev_mean_district_free_energy_ = true;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    [[nodiscard]] float city_free_energy() const noexcept { return city_free_energy_; }
    [[nodiscard]] float mean_district_free_energy() const noexcept { return mean_district_free_energy_; }
    [[nodiscard]] float inter_district_divergence() const noexcept { return inter_district_divergence_; }

private:
    std::vector<entelechy::CognitiveDistrict*> districts_;
    size_t active_district_count_{0};
    float city_free_energy_{0.0f};
    float mean_district_free_energy_{0.0f};
    float inter_district_divergence_{0.0f};
    float prev_mean_district_free_energy_{0.0f};
    bool has_prev_mean_district_free_energy_{false};
};

} // namespace opencog::endo
