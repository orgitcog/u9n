#pragma once
/**
 * @file interoceptive.hpp
 * @brief Interoceptive Model — body-state computation for channels 20-31
 *
 * Implements Porges' polyvagal hierarchy, Craig's insular interoception,
 * and McEwen's allostatic load accumulation. Runs in Phase 7 of the tick
 * pipeline (AFTER adapter feedback in Phase 4), reading from the hormone
 * bus snapshot and computing derived interoceptive channels.
 *
 * The model READS from the bus snapshot but does not directly write to bus;
 * the adapter layer does that via to_bus_values().
 *
 * Design: header-only logic, SIMD-aligned state, all bounds clamped.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/endocrine/types.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace opencog::entelechy {

using endo::HormoneId;
using endo::EndocrineState;
using endo::SIMD_ALIGN;

// ============================================================================
// Interoceptive Channel Configuration
// ============================================================================

/**
 * @brief Configuration for a single interoceptive channel (20-31)
 *
 * Provides half-life, baseline, and ceiling parameters for bus setup.
 */
struct InteroceptiveChannelConfig {
    uint8_t channel;    ///< Channel index (20-31)
    float half_life;    ///< Decay half-life in ticks
    float baseline;     ///< Homeostatic setpoint
    float ceiling;      ///< Maximum allowed value
};

// ============================================================================
// Interoceptive Model
// ============================================================================

/**
 * @brief Computes interoceptive state from cross-channel hormone interactions
 *
 * Phase 7 of the tick pipeline. Reads the current EndocrineState snapshot
 * and computes 12 interoceptive channels (20-31) representing:
 * - Polyvagal hierarchy (Porges): vagal tone, sympathetic drive, dorsal vagal
 * - Cardiac/respiratory feedback: HRV proxy, breathing regularity
 * - Visceral signals: gut-brain axis, immune extension, pain
 * - Integrative: insular cortex (Craig), allostatic load (McEwen)
 * - Body schema: proprioception, thermoregulation
 */
class InteroceptiveModel {
public:
    InteroceptiveModel() = default;

    // -----------------------------------------------------------------------
    // Main update: compute interoceptive state from hormone bus snapshot
    // Called in Phase 7 of tick pipeline
    // -----------------------------------------------------------------------

    void update(const EndocrineState& hormones, float dt = 1.0f) {
        (void)dt; // Reserved for future time-scaling

        // Extract relevant hormone concentrations
        const float cortisol   = hormones[HormoneId::CORTISOL];
        const float ne         = hormones[HormoneId::NOREPINEPHRINE];
        const float serotonin  = hormones[HormoneId::SEROTONIN];
        const float oxytocin   = hormones[HormoneId::OXYTOCIN];
        const float il6        = hormones[HormoneId::IL6];

        // ===================================================================
        // Polyvagal hierarchy (Porges' three tiers, phylogenetically ordered)
        // ===================================================================

        // Tier 3 (oldest): Dorsal vagal = freeze/collapse
        // Active when overwhelmed: high cortisol AND high NE AND low serotonin
        if (cortisol > 0.7f && ne > 0.7f && serotonin < 0.2f) {
            state_.dorsal_vagal = std::clamp(
                state_.dorsal_vagal + 0.1f, 0.0f, 1.0f);
            state_.sympathetic_drive = std::clamp(
                state_.sympathetic_drive - 0.05f, 0.0f, 1.0f);
            state_.vagal_tone = std::clamp(
                state_.vagal_tone - 0.1f, 0.0f, 1.0f);
        }
        // Tier 2 (older): Sympathetic = fight/flight
        // Active when mobilized: elevated cortisol OR high NE
        else if (cortisol > 0.4f || ne > 0.5f) {
            state_.sympathetic_drive = std::clamp(
                state_.sympathetic_drive + 0.08f, 0.0f, 1.0f);
            state_.vagal_tone = std::clamp(
                state_.vagal_tone - 0.05f, 0.0f, 1.0f);
            state_.dorsal_vagal = std::clamp(
                state_.dorsal_vagal - 0.05f, 0.0f, 1.0f);
        }
        // Tier 1 (newest): Ventral vagal = social engagement
        // Active when safe: oxytocin present, low stress
        else if (oxytocin > 0.4f && cortisol < 0.3f && ne < 0.4f) {
            state_.vagal_tone = std::clamp(
                state_.vagal_tone + 0.08f, 0.0f, 1.0f);
            state_.sympathetic_drive = std::clamp(
                state_.sympathetic_drive - 0.05f, 0.0f, 1.0f);
            state_.dorsal_vagal = std::clamp(
                state_.dorsal_vagal - 0.05f, 0.0f, 1.0f);
        }
        // No tier strongly activated — gentle drift toward baseline
        else {
            state_.vagal_tone = drift_toward(state_.vagal_tone, 0.5f, 0.02f);
            state_.sympathetic_drive = drift_toward(state_.sympathetic_drive, 0.3f, 0.02f);
            state_.dorsal_vagal = drift_toward(state_.dorsal_vagal, 0.0f, 0.02f);
        }

        // ===================================================================
        // Cardiac coherence: rises with vagal tone, drops with sympathetic
        // ===================================================================
        state_.cardiac_coherence = std::clamp(
            state_.vagal_tone * 0.6f + (1.0f - state_.sympathetic_drive) * 0.4f,
            0.0f, 1.0f);

        // ===================================================================
        // Respiratory rhythm: influenced by vagal tone and anxiety
        // ===================================================================
        state_.respiratory_rhythm = std::clamp(
            state_.vagal_tone * 0.5f + (1.0f - state_.sympathetic_drive * 0.5f) * 0.5f,
            0.0f, 1.0f);

        // ===================================================================
        // Gut-brain signal: 90% of body serotonin is in gut
        // Partial coupling to serotonergic channel
        // ===================================================================
        state_.gut_brain_signal = std::clamp(
            0.4f * serotonin + 0.6f * state_.gut_brain_signal,
            0.0f, 1.0f);

        // ===================================================================
        // Immune extended: rises with IL-6, slow decay (half-life 80)
        // ===================================================================
        constexpr float immune_decay = 1.0f - 1.0f / 80.0f;
        state_.immune_extended = std::clamp(
            0.3f * il6 + 0.7f * state_.immune_extended * immune_decay,
            0.0f, 1.0f);

        // ===================================================================
        // Allostatic load accumulation (McEwen)
        // Cumulative stress damage with very slow recovery
        // ===================================================================
        if (cortisol > 0.5f) {
            high_cortisol_ticks_++;
            low_cortisol_ticks_ = 0;
            if (high_cortisol_ticks_ > 100) {
                state_.allostatic_load += (cortisol - 0.5f) * 0.001f * (1.0f - resilience_);
            }
        } else {
            high_cortisol_ticks_ = 0;
        }

        if (cortisol < 0.3f) {
            low_cortisol_ticks_++;
            if (low_cortisol_ticks_ > 50) {
                state_.allostatic_load -= 0.0005f * resilience_;
                state_.allostatic_load = std::max(0.0f, state_.allostatic_load);
            }
        } else {
            low_cortisol_ticks_ = 0;
        }

        state_.allostatic_load = std::clamp(state_.allostatic_load, 0.0f, 5.0f);

        // ===================================================================
        // Proprioceptive tone: degrades with pain and dorsal vagal (freeze)
        // ===================================================================
        state_.proprioceptive_tone = std::clamp(
            0.5f * (1.0f - state_.nociceptive_signal)
            * (1.0f - state_.dorsal_vagal * 0.7f)
            + 0.5f * state_.proprioceptive_tone,
            0.0f, 1.0f);

        // ===================================================================
        // Nociceptive signal: fast decay (half-life 5), driven externally
        // Decays naturally each tick but can be set externally
        // ===================================================================
        constexpr float noci_decay = 1.0f - 1.0f / 5.0f;
        state_.nociceptive_signal = std::clamp(
            state_.nociceptive_signal * noci_decay,
            0.0f, 1.0f);

        // ===================================================================
        // Thermoregulatory: disrupted by high allostatic load
        // ===================================================================
        state_.thermoregulatory = std::clamp(
            0.5f * (1.0f - state_.allostatic_load / 5.0f)
            + 0.5f * state_.thermoregulatory,
            0.0f, 1.0f);

        // ===================================================================
        // Insular integration: Craig's "material me" — meta-awareness of
        // body state, computed from all other interoceptive channels
        // ===================================================================
        state_.insular_integration = state_.material_self();
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Current interoceptive state
    [[nodiscard]] const InteroceptiveState& state() const noexcept {
        return state_;
    }

    /// Dominant polyvagal tier based on current state
    [[nodiscard]] PolyvagalTier dominant_tier() const noexcept {
        return state_.dominant_tier();
    }

    /// Craig's material self — meta-awareness of body state
    [[nodiscard]] float material_self() const noexcept {
        return state_.material_self();
    }

    /// McEwen's allostatic load — cumulative stress damage [0,5]
    [[nodiscard]] float allostatic_load() const noexcept {
        return state_.allostatic_load;
    }

    // -----------------------------------------------------------------------
    // Bus interface
    // -----------------------------------------------------------------------

    /**
     * @brief Get recommended channel configurations for bus setup
     *
     * Returns the 12 interoceptive channels (20-31) with their
     * half-lives, baselines, and ceilings from the physiological model.
     */
    [[nodiscard]] static std::array<InteroceptiveChannelConfig, 12> default_channel_configs() {
        return {{
            // ch  half_life  baseline  ceiling
            { 20,   50.0f,    0.5f,     1.0f },  // VAGAL_TONE
            { 21,   10.0f,    0.3f,     1.0f },  // SYMPATHETIC_DRIVE
            { 22,   30.0f,    0.0f,     1.0f },  // DORSAL_VAGAL
            { 23,   40.0f,    0.5f,     1.0f },  // CARDIAC_COHERENCE
            { 24,   20.0f,    0.5f,     1.0f },  // RESPIRATORY_RHYTHM
            { 25,   60.0f,    0.3f,     1.0f },  // GUT_BRAIN_SIGNAL
            { 26,   80.0f,    0.1f,     1.0f },  // IMMUNE_EXTENDED
            { 27,   30.0f,    0.5f,     1.0f },  // INSULAR_INTEGRATION
            { 28,  500.0f,    0.0f,     5.0f },  // ALLOSTATIC_LOAD
            { 29,   25.0f,    0.5f,     1.0f },  // PROPRIOCEPTIVE_TONE
            { 30,    5.0f,    0.0f,     1.0f },  // NOCICEPTIVE_SIGNAL
            { 31,  100.0f,    0.5f,     1.0f },  // THERMOREGULATORY
        }};
    }

    /**
     * @brief Write interoceptive state to bus channels 20-31
     *
     * Returns array of 12 floats corresponding to channels 20-31.
     * The adapter layer uses this to write back to the HormoneBus.
     */
    [[nodiscard]] std::array<float, 12> to_bus_values() const noexcept {
        return {{
            state_.vagal_tone,            // ch20
            state_.sympathetic_drive,     // ch21
            state_.dorsal_vagal,          // ch22
            state_.cardiac_coherence,     // ch23
            state_.respiratory_rhythm,    // ch24
            state_.gut_brain_signal,      // ch25
            state_.immune_extended,       // ch26
            state_.insular_integration,   // ch27
            state_.allostatic_load,       // ch28
            state_.proprioceptive_tone,   // ch29
            state_.nociceptive_signal,    // ch30
            state_.thermoregulatory,      // ch31
        }};
    }

    // -----------------------------------------------------------------------
    // Direct state mutation (for external signals, e.g. pain injection)
    // -----------------------------------------------------------------------

    /// Inject a nociceptive (pain) signal [0,1]
    void inject_pain(float intensity) noexcept {
        state_.nociceptive_signal = std::clamp(
            state_.nociceptive_signal + intensity, 0.0f, 1.0f);
    }

    /// Set resilience parameter (from TemperamentProfile)
    void set_resilience(float r) noexcept {
        resilience_ = std::clamp(r, 0.0f, 1.0f);
    }

    /// Get current resilience
    [[nodiscard]] float resilience() const noexcept {
        return resilience_;
    }

private:
    InteroceptiveState state_{};

    // Allostatic load tracking
    uint64_t high_cortisol_ticks_{0};
    uint64_t low_cortisol_ticks_{0};

    // From TemperamentProfile — modulates allostatic recovery
    float resilience_{0.5f};

    // -----------------------------------------------------------------------
    // Helper: drift a value toward target at a given rate
    // -----------------------------------------------------------------------
    [[nodiscard]] static float drift_toward(float current, float target, float rate) noexcept {
        if (current < target) {
            return std::min(current + rate, target);
        } else {
            return std::max(current - rate, target);
        }
    }
};

} // namespace opencog::entelechy
