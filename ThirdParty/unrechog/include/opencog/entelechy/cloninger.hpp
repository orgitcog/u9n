#pragma once
/**
 * @file cloninger.hpp
 * @brief Cloninger Temperament/Character gain system
 *
 * Computes multiplicative GAIN PARAMETERS on hormone production and decay
 * based on a TemperamentProfile. Runs in Phase 0 of the tick pipeline
 * (BEFORE gland production), so temperament shapes the *sensitivity* of
 * the entire endocrine cascade without directly producing hormones.
 *
 * Cloninger's 7-factor model maps to hormonal sensitivities:
 *
 *   TEMPERAMENT (innate, slow-changing):
 *     Novelty Seeking      -> DA_TONIC, DA_PHASIC production gain
 *     Harm Avoidance       -> SEROTONIN, CORTISOL production gain
 *     Reward Dependence    -> OXYTOCIN production gain
 *     Persistence          -> DA_TONIC decay gain (longer half-life)
 *
 *   CHARACTER (developed through experience):
 *     Self-Directedness    -> CORTISOL production gain (stress resilience)
 *                          -> T3_T4 production gain (executive drive)
 *     Cooperativeness      -> OXYTOCIN production gain (multiplicative)
 *     Self-Transcendence   -> ANANDAMIDE, MELATONIN production gain
 *
 *   DEVELOPMENTAL MODIFIERS:
 *     allostatic_load      -> CORTISOL production gain (cumulative wear)
 *     trauma_encoding      -> effective harm_avoidance boost
 *     attachment insecurity -> CORTISOL boost, OXYTOCIN reduction
 *
 * All gains are multiplicative (default 1.0 = no change) and bounded
 * to [0.3, 2.0] to prevent zeroing or runaway amplification.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/endocrine/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace opencog::entelechy {

using endo::HormoneId;
using endo::HORMONE_COUNT;

/// Absolute bounds on any gain parameter
inline constexpr float GAIN_MIN = 0.3f;
inline constexpr float GAIN_MAX = 2.0f;

/**
 * @brief Computes hormone production/decay gains from Cloninger temperament
 *
 * This is a stateless-ish computation class: it stores a TemperamentProfile
 * and produces a GainProfile on demand. The GainProfile is then consumed by
 * the HormoneBus (or EndocrineSystem facade) to scale gland production rates
 * and decay half-lives before the main tick cycle.
 */
class CloningerSystem {
public:
    /// Construct with an initial temperament profile
    explicit CloningerSystem(const TemperamentProfile& profile) noexcept
        : profile_(profile) {}

    // ========================================================================
    // Core Computation
    // ========================================================================

    /**
     * @brief Compute multiplicative gains from the current temperament
     *
     * Returns a GainProfile where each element is a multiplier:
     * - production_gains[ch]: multiplied into gland produce() amount
     * - decay_gains[ch]: multiplied into half-life (higher = slower decay)
     *
     * Default is 1.0 for all channels (no modification).
     */
    [[nodiscard]] GainProfile compute_gains() const noexcept {
        GainProfile gains;  // All 1.0 by default

        // --- Apply developmental modifiers to effective temperament ---
        float effective_ha = effective_harm_avoidance();

        // =================================================================
        // Temperament -> Production Gains
        // =================================================================

        // Novelty Seeking -> DA_TONIC, DA_PHASIC production
        // NS=0 -> 0.5x, NS=1 -> 1.5x
        const float ns = profile_.novelty_seeking;
        gains.production_gains[idx(HormoneId::DOPAMINE_TONIC)]  = 0.5f + ns * 1.0f;
        gains.production_gains[idx(HormoneId::DOPAMINE_PHASIC)] = 0.5f + ns * 1.0f;

        // Harm Avoidance -> SEROTONIN, CORTISOL production
        // Uses effective HA (boosted by trauma encoding)
        gains.production_gains[idx(HormoneId::SEROTONIN)] = 0.5f + effective_ha * 1.0f;
        gains.production_gains[idx(HormoneId::CORTISOL)]  = 0.6f + effective_ha * 0.8f;

        // Reward Dependence -> OXYTOCIN production
        const float rd = profile_.reward_dependence;
        gains.production_gains[idx(HormoneId::OXYTOCIN)] = 0.5f + rd * 1.0f;

        // =================================================================
        // Temperament -> Decay Gains
        // =================================================================

        // Persistence -> DA_TONIC decay gain (longer half-life = slower decay)
        // P=0 -> 0.5x half-life, P=1 -> 1.5x half-life
        const float p = profile_.persistence;
        gains.decay_gains[idx(HormoneId::DOPAMINE_TONIC)] = 0.5f + p * 1.0f;

        // =================================================================
        // Character -> Production Gains
        // =================================================================

        // Self-Directedness -> CORTISOL stress resilience (multiplicative reduction)
        // SD=0 -> *= 1.0 (no resilience), SD=1 -> *= 0.5 (halved cortisol)
        // Floor at 0.3 to prevent complete cortisol suppression
        const float sd = profile_.self_directedness;
        gains.production_gains[idx(HormoneId::CORTISOL)] *=
            std::max(0.3f, 1.0f - sd * 0.5f);

        // Self-Directedness -> T3_T4 production (executive metabolic drive)
        // SD=0 -> 0.7x, SD=1 -> 1.3x
        gains.production_gains[idx(HormoneId::T3_T4)] = 0.7f + sd * 0.6f;

        // Cooperativeness -> OXYTOCIN production (multiplicative on existing gain)
        // C=0 -> *= 0.8, C=1 -> *= 1.2
        const float c = profile_.cooperativeness;
        gains.production_gains[idx(HormoneId::OXYTOCIN)] *= 0.8f + c * 0.4f;

        // Self-Transcendence -> ANANDAMIDE, MELATONIN production
        // ST=0 -> 0.5x, ST=1 -> 1.5x for anandamide
        // ST=0 -> 0.7x, ST=1 -> 1.3x for melatonin
        const float st = profile_.self_transcendence;
        gains.production_gains[idx(HormoneId::ANANDAMIDE)] = 0.5f + st * 1.0f;
        gains.production_gains[idx(HormoneId::MELATONIN)]  = 0.7f + st * 0.6f;

        // =================================================================
        // Developmental Modifiers
        // =================================================================

        // Allostatic load -> CORTISOL production boost (cumulative wear)
        // Each unit of allostatic load adds 0.1 to cortisol gain
        if (profile_.allostatic_load > 0.0f) {
            gains.production_gains[idx(HormoneId::CORTISOL)] +=
                profile_.allostatic_load * 0.1f;
        }

        // Attachment insecurity -> CORTISOL boost, OXYTOCIN reduction
        // Low security + high anxiety = anxious attachment pattern
        if (profile_.attachment_security < 0.5f && profile_.attachment_anxiety > 0.5f) {
            // Insecure-anxious: heightened HPA axis, reduced social bonding
            float insecurity = (1.0f - profile_.attachment_security);
            float anxiety = profile_.attachment_anxiety;
            float attachment_stress = insecurity * anxiety;  // [0, 1]

            gains.production_gains[idx(HormoneId::CORTISOL)] +=
                attachment_stress * 0.3f;
            gains.production_gains[idx(HormoneId::OXYTOCIN)] *=
                std::max(0.3f, 1.0f - attachment_stress * 0.4f);
        }

        // =================================================================
        // Clamp all gains to [GAIN_MIN, GAIN_MAX]
        // =================================================================

        clamp_gains(gains);

        return gains;
    }

    // ========================================================================
    // Profile Management
    // ========================================================================

    /// Update the stored temperament profile
    void update_temperament(const TemperamentProfile& profile) noexcept {
        profile_ = profile;
    }

    /// Access the current temperament profile
    [[nodiscard]] const TemperamentProfile& temperament() const noexcept {
        return profile_;
    }

private:
    TemperamentProfile profile_;

    // ========================================================================
    // Helpers
    // ========================================================================

    /// Convert HormoneId to array index
    [[nodiscard]] static constexpr size_t idx(HormoneId id) noexcept {
        return static_cast<size_t>(id);
    }

    /// Effective harm avoidance incorporating trauma encoding
    [[nodiscard]] float effective_harm_avoidance() const noexcept {
        float ha = profile_.harm_avoidance;
        // Trauma encoding strengthens the harm-avoidant disposition
        // trauma_encoding_strength [0,1] adds up to 0.3 to effective HA
        ha += profile_.trauma_encoding_strength * 0.3f;
        return std::clamp(ha, 0.0f, 1.0f);
    }

    /// Clamp all gains to the safe range [GAIN_MIN, GAIN_MAX]
    static void clamp_gains(GainProfile& gains) noexcept {
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            gains.production_gains[i] =
                std::clamp(gains.production_gains[i], GAIN_MIN, GAIN_MAX);
            gains.decay_gains[i] =
                std::clamp(gains.decay_gains[i], GAIN_MIN, GAIN_MAX);
        }
    }
};

} // namespace opencog::entelechy
