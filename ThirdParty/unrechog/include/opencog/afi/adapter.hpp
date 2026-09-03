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
 *   City free energy spike (current > prev * 1.5 AND current > 3.0)
 *     -> CORTISOL +0.05, NE +0.05  (prediction crisis)
 *   City free energy drop (current < prev * 0.7 AND prev > 2.0)
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
     * 3. Compute variance of free energies for inter_district_divergence_
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

        city_free_energy_ = sum_fe;

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
     * Follows the Marduk adapter pattern: compare city free energy to
     * previous state and emit hormonal signals on significant changes.
     *
     * Edge triggers:
     *   1. City FE spike (current > prev * 1.5 AND current > 3.0)
     *      -> CORTISOL +0.05, NE +0.05 (prediction crisis)
     *   2. City FE drop (current < prev * 0.7 AND prev > 2.0)
     *      -> DA_PHASIC +0.05 (model improvement reward)
     */
    void apply_feedback() {
        // --- 1. City free energy spike → prediction crisis ---
        if (city_free_energy_ > prev_city_free_energy_ * 1.5f &&
            city_free_energy_ > 3.0f) {
            bus_.produce(HormoneId::CORTISOL, 0.05f);
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
        }

        // --- 2. City free energy drop → model improvement reward ---
        if (city_free_energy_ < prev_city_free_energy_ * 0.7f &&
            prev_city_free_energy_ > 2.0f) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.05f);
        }

        // Update previous state for next tick's edge detection
        prev_city_free_energy_ = city_free_energy_;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    [[nodiscard]] float city_free_energy() const noexcept { return city_free_energy_; }
    [[nodiscard]] float inter_district_divergence() const noexcept { return inter_district_divergence_; }

private:
    std::vector<entelechy::CognitiveDistrict*> districts_;
    float city_free_energy_{0.0f};
    float inter_district_divergence_{0.0f};
    float prev_city_free_energy_{0.0f};
};

} // namespace opencog::endo
