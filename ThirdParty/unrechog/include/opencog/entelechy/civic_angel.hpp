#pragma once
/**
 * @file civic_angel.hpp
 * @brief CivicAngel — emergent self-model of the Cognitive City
 *
 * The Civic Angel is NOT a central controller. It is an emergent self-model
 * that periodically observes the cognitive districts and computes integrated
 * metrics about the system's overall state. It runs at the SLOWEST frequency
 * in the tick pipeline (every ~10 VES ticks).
 *
 * The metaphor: the Civic Angel is the city's "spirit" — an emergent
 * property of all the districts working together, not an entity that
 * commands them. It observes, integrates, and reflects, providing the
 * substrate for phenomenal selfhood (Metzinger's PSM).
 *
 * Sub-systems (all value-owned):
 * - CloningerSystem: temperament/character gain computation
 * - InteroceptiveModel: body-state (polyvagal, allostatic load)
 * - DevelopmentalTrajectory: Erikson stages, trauma, critical periods
 * - NarrativeIdentity: McAdams life-story, chapter detection
 * - SocialSelf: attachment, roles, theory of mind
 *
 * Design: header-only logic, thin .cpp compilation unit.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/entelechy/district.hpp>
#include <opencog/entelechy/cloninger.hpp>
#include <opencog/entelechy/interoceptive.hpp>
#include <opencog/entelechy/developmental.hpp>
#include <opencog/entelechy/narrative.hpp>
#include <opencog/entelechy/social.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace opencog::entelechy {

// ============================================================================
// CivicAngel — emergent self-model
// ============================================================================

class CivicAngel {
public:
    /// Config for observation frequency and thresholds
    struct Config {
        size_t observation_interval{10};    ///< How often to run self-model (in VES ticks)
        size_t narrative_interval{100};     ///< How often to update narrative
        float coherence_threshold{0.6f};    ///< Threshold for "high coherence"
        float free_energy_alarm{5.0f};      ///< Threshold for free energy alarm
    };

    explicit CivicAngel(Config config = {})
        : config_(config)
        , cloninger_(TemperamentProfile{})
    {}

    // ========================================================================
    // Main Update (called every VES tick, self-throttles)
    // ========================================================================

    /**
     * @brief Advance the Civic Angel by one VES tick
     *
     * Self-throttles: only performs a full observation every
     * `observation_interval` ticks. Returns true if a self-model
     * observation was performed this tick.
     */
    bool tick(uint64_t current_tick) {
        // Self-throttle: only observe every observation_interval ticks
        if (current_tick - last_observation_tick_ < config_.observation_interval)
            return false;

        last_observation_tick_ = current_tick;
        observe(current_tick);

        // Narrative update at slower rate
        if (current_tick - last_narrative_tick_ >= config_.narrative_interval) {
            last_narrative_tick_ = current_tick;
            // narrative_.update() would be called by the adapter with current valence
        }

        return true;
    }

    // ========================================================================
    // District Management
    // ========================================================================

    /**
     * @brief Register a district for observation
     *
     * Districts are non-owning pointers. The caller is responsible for
     * ensuring the district outlives the CivicAngel (or is deregistered).
     */
    void register_district(CognitiveDistrict* district) {
        if (district) {
            districts_.push_back(district);
        }
    }

    [[nodiscard]] size_t district_count() const noexcept {
        return districts_.size();
    }

    // ========================================================================
    // Sub-system Access
    // ========================================================================

    [[nodiscard]] CloningerSystem& cloninger() noexcept { return cloninger_; }
    [[nodiscard]] const CloningerSystem& cloninger() const noexcept { return cloninger_; }

    [[nodiscard]] InteroceptiveModel& interoceptive() noexcept { return interoceptive_; }
    [[nodiscard]] const InteroceptiveModel& interoceptive() const noexcept { return interoceptive_; }

    [[nodiscard]] DevelopmentalTrajectory& developmental() noexcept { return developmental_; }
    [[nodiscard]] const DevelopmentalTrajectory& developmental() const noexcept { return developmental_; }

    [[nodiscard]] NarrativeIdentity& narrative() noexcept { return narrative_; }
    [[nodiscard]] const NarrativeIdentity& narrative() const noexcept { return narrative_; }

    [[nodiscard]] SocialSelf& social() noexcept { return social_; }
    [[nodiscard]] const SocialSelf& social() const noexcept { return social_; }

    // ========================================================================
    // State Query
    // ========================================================================

    [[nodiscard]] const CivicAngelState& state() const noexcept { return state_; }
    [[nodiscard]] float total_free_energy() const noexcept { return state_.total_free_energy; }
    [[nodiscard]] float self_coherence() const noexcept { return state_.self_coherence; }
    [[nodiscard]] DevelopmentalStage developmental_stage() const noexcept { return state_.developmental_stage; }

private:
    Config config_;
    CivicAngelState state_;

    // Sub-systems (owned)
    CloningerSystem cloninger_;
    InteroceptiveModel interoceptive_;
    DevelopmentalTrajectory developmental_;
    NarrativeIdentity narrative_;
    SocialSelf social_;

    // Districts (non-owning pointers)
    std::vector<CognitiveDistrict*> districts_;

    // Tick tracking
    uint64_t last_observation_tick_{0};
    uint64_t last_narrative_tick_{0};

    // ========================================================================
    // Self-Model Observation (every ~10 ticks)
    // ========================================================================

    /**
     * @brief Perform a full self-model observation across all districts
     *
     * This is the core of the Civic Angel: it reads district metrics and
     * computes integrated city-wide state. The Angel OBSERVES but does
     * not CONTROL.
     *
     * Steps:
     * 1. Sum per-district free energy -> total_free_energy
     * 2. Average district surprise -> mean_district_surprise
     * 3. Inter-district coherence (1 - normalized variance of FE)
     * 4. Resource fairness (1 - Gini of district accuracies)
     * 5. Self-coherence (weighted blend of sub-metrics)
     * 6. Self-complexity (log-ratio of active districts)
     * 7. Adaptive capacity (blend of sub-system capacities)
     * 8. Entelechy progress (maturation x coherence x narrative depth)
     * 9. Update developmental stage and narrative from sub-systems
     */
    void observe(uint64_t /*current_tick*/) {
        // --- 1. Total free energy ---
        state_.total_free_energy = compute_total_free_energy();

        // --- 2. Mean district surprise ---
        state_.mean_district_surprise = compute_mean_surprise();

        // --- 3. Inter-district coherence ---
        state_.inter_district_coherence = compute_inter_district_coherence();

        // --- 4. Resource fairness ---
        state_.resource_fairness = compute_resource_fairness();

        // --- 5. Self-coherence ---
        state_.self_coherence = compute_self_coherence();

        // --- 6. Self-complexity ---
        state_.self_complexity = compute_self_complexity();

        // --- 7. Adaptive capacity ---
        state_.adaptive_capacity = compute_adaptive_capacity();

        // --- 8. Entelechy progress ---
        state_.entelechy_progress = compute_entelechy_progress();

        // --- 9. Developmental stage from trajectory ---
        state_.developmental_stage = developmental_.current_stage();
        state_.maturation_level = developmental_.maturation_level();

        // --- 10. Narrative integration ---
        state_.dominant_life_theme = narrative_.dominant_life_theme();
        state_.narrative_coherence = narrative_.narrative_coherence();
    }

    // ========================================================================
    // City-Wide Metric Computations
    // ========================================================================

    /**
     * @brief Sum of free energy across all active districts
     */
    [[nodiscard]] float compute_total_free_energy() const {
        float total = 0.0f;
        for (const auto* d : districts_) {
            if (d && d->is_active()) {
                total += d->metrics().free_energy;
            }
        }
        return total;
    }

    /**
     * @brief Average surprise across all active districts
     */
    [[nodiscard]] float compute_mean_surprise() const {
        float sum = 0.0f;
        size_t count = 0;
        for (const auto* d : districts_) {
            if (d && d->is_active()) {
                sum += d->metrics().surprise;
                ++count;
            }
        }
        return count > 0 ? sum / static_cast<float>(count) : 0.0f;
    }

    /**
     * @brief Inter-district coherence: 1 - normalized variance of district free energies
     *
     * High coherence means districts have similar free energy levels
     * (no single district is dramatically worse than others).
     * Returns 1.0 when all districts have identical free energy.
     */
    [[nodiscard]] float compute_inter_district_coherence() const {
        std::vector<float> energies;
        for (const auto* d : districts_) {
            if (d && d->is_active()) {
                energies.push_back(d->metrics().free_energy);
            }
        }

        if (energies.size() < 2) return 1.0f;

        // Compute mean
        const float n = static_cast<float>(energies.size());
        float sum = 0.0f;
        for (float e : energies) sum += e;
        const float mean = sum / n;

        // Compute variance
        float var_sum = 0.0f;
        for (float e : energies) {
            const float diff = e - mean;
            var_sum += diff * diff;
        }
        const float variance = var_sum / n;

        // Normalize: variance / (mean^2 + epsilon) to get coefficient of variation squared
        // Then 1 - clamp(cv^2, 0, 1) for coherence
        const float cv_sq = (mean > 1e-6f) ? variance / (mean * mean) : 0.0f;
        return std::clamp(1.0f - cv_sq, 0.0f, 1.0f);
    }

    /**
     * @brief Resource fairness: 1 - Gini coefficient of district accuracies
     *
     * Uses accuracy as a proxy for resource allocation quality.
     * Gini = 0 means perfect equality; Gini = 1 means maximum inequality.
     * We return 1 - Gini so that 1.0 = perfect fairness.
     */
    [[nodiscard]] float compute_resource_fairness() const {
        std::vector<float> accuracies;
        for (const auto* d : districts_) {
            if (d && d->is_active()) {
                accuracies.push_back(d->metrics().accuracy);
            }
        }

        if (accuracies.empty()) return 1.0f;

        const size_t n = accuracies.size();
        if (n == 1) return 1.0f;

        // Sort for Gini computation
        std::sort(accuracies.begin(), accuracies.end());

        // Gini = (sum(i * x_i) - (n+1)/2 * sum(x_i)) / (n * sum(x_i))
        // where i is 1-indexed rank
        float sum_ix = 0.0f;
        float sum_x = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            sum_ix += static_cast<float>(i + 1) * accuracies[i];
            sum_x += accuracies[i];
        }

        if (sum_x < 1e-8f) return 1.0f;  // All zero = trivially fair

        const float fn = static_cast<float>(n);
        const float gini = (sum_ix - (fn + 1.0f) / 2.0f * sum_x) / (fn * sum_x);
        return std::clamp(1.0f - gini, 0.0f, 1.0f);
    }

    /**
     * @brief Self-coherence: weighted blend of integration metrics
     *
     * Weights:
     * - 0.3 inter-district coherence (districts working together)
     * - 0.3 narrative coherence (life story consistency)
     * - 0.2 interoceptive material_self (body awareness)
     * - 0.2 developmental maturation (character maturity)
     */
    [[nodiscard]] float compute_self_coherence() const {
        const float idc = state_.inter_district_coherence;
        const float nc  = narrative_.narrative_coherence();
        const float ms  = interoceptive_.material_self();
        const float mat = developmental_.maturation_level();

        return std::clamp(
            0.3f * idc + 0.3f * nc + 0.2f * ms + 0.2f * mat,
            0.0f, 1.0f
        );
    }

    /**
     * @brief Self-complexity: log-ratio of active districts to total
     *
     * complexity = log2(1 + active_count) / log2(1 + DISTRICT_COUNT)
     *
     * A system with more active districts has higher self-complexity
     * (richer self-representation).
     */
    [[nodiscard]] float compute_self_complexity() const {
        size_t active_count = 0;
        for (const auto* d : districts_) {
            if (d && d->is_active()) {
                ++active_count;
            }
        }

        const float numerator   = std::log2(1.0f + static_cast<float>(active_count));
        const float denominator = std::log2(1.0f + static_cast<float>(DISTRICT_COUNT));
        return (denominator > 0.0f) ? std::clamp(numerator / denominator, 0.0f, 1.0f) : 0.0f;
    }

    /**
     * @brief Adaptive capacity: blend of sub-system flexibility metrics
     *
     * Weights:
     * - 0.3 cloninger plasticity (temperament flexibility)
     * - 0.2 social ToM depth (social modeling ability)
     * - 0.2 narrative identity_strength (narrative integration)
     * - 0.3 interoceptive cardiac_coherence (body regulation)
     */
    [[nodiscard]] float compute_adaptive_capacity() const {
        const float plasticity       = cloninger_.temperament().plasticity;
        const float tom_depth        = social_.theory_of_mind_depth();
        const float id_strength      = narrative_.identity_strength();
        const float cardiac_coherence = interoceptive_.state().cardiac_coherence;

        return std::clamp(
            0.3f * plasticity + 0.2f * tom_depth + 0.2f * id_strength + 0.3f * cardiac_coherence,
            0.0f, 1.0f
        );
    }

    /**
     * @brief Entelechy progress: actualization toward potential
     *
     * progress = maturation * self_coherence * min(1, log2(1 + chapters) / 5)
     *
     * Requires maturation (developmental growth), self-coherence (integration),
     * AND accumulated narrative experience (lived story depth).
     */
    [[nodiscard]] float compute_entelechy_progress() const {
        const float maturation = developmental_.maturation_level();
        const float coherence  = state_.self_coherence;
        const float chapter_depth = std::min(
            1.0f,
            std::log2(1.0f + static_cast<float>(narrative_.chapter_count())) / 5.0f
        );

        return std::clamp(maturation * coherence * chapter_depth, 0.0f, 1.0f);
    }
};

} // namespace opencog::entelechy
