#pragma once
/**
 * @file endocrine_adapter.hpp
 * @brief Bidirectional coupling between Temporal Crystal System and VES
 *
 * Maps temporal crystal oscillation states to endocrine hormone levels
 * and vice versa. Follows the established EndocrineConnector adapter
 * pattern from the VES subsystem.
 *
 * TCS → VES (READ phase: oscillations modulate hormones):
 *   ULTRA_SLOW amplitude → Serotonin (patience/sustained processing)
 *   SLOWEST amplitude   → Melatonin (maintenance/consolidation)
 *   Global coherence    → COG_COHERENCE (cognitive integration quality)
 *   FAST amplitude      → NE (vigilance/fast processing)
 *   Phase lock events   → DA_phasic (reward: successful synchronization)
 *
 * VES → TCS (WRITE phase: hormones modulate oscillations):
 *   Cortisol            → Global coupling ↓ (stress desynchronizes)
 *   Serotonin           → ULTRA_SLOW amplitude ↑ (patience sustains slow rhythms)
 *   DA_tonic            → FAST amplitude ↑ (motivation boosts fast processing)
 *   NE                  → Coupling strength ↑ (vigilance tightens synchronization)
 *   Melatonin           → SLOWEST amplitude ↑ (maintenance cycle)
 *   Oxytocin            → Global coupling ↑ (social bonding synchronizes)
 */

#include <opencog/temporal/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>
#include <opencog/endocrine/types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::temporal {

/**
 * @brief Configuration for crystal↔endocrine coupling
 */
struct CrystalEndocrineConfig {
    // TCS → VES gains
    float coherence_to_cog_coherence{0.8f};    ///< Global coherence → COG_COHERENCE
    float ultra_slow_to_serotonin{0.05f};      ///< ULTRA_SLOW amp → serotonin nudge
    float slowest_to_melatonin{0.03f};         ///< SLOWEST amp → melatonin nudge
    float fast_to_norepinephrine{0.04f};       ///< FAST amp → NE nudge
    float phase_lock_to_dopamine{0.1f};        ///< Phase lock event → DA_phasic

    // VES → TCS gains
    float cortisol_coupling_suppression{0.3f}; ///< Cortisol → coupling reduction
    float serotonin_slow_boost{0.2f};          ///< Serotonin → ULTRA_SLOW amp ↑
    float dopamine_fast_boost{0.15f};          ///< DA_tonic → FAST amp ↑
    float ne_coupling_boost{0.1f};             ///< NE → coupling strength ↑
    float melatonin_slowest_boost{0.15f};      ///< Melatonin → SLOWEST amp ↑
    float oxytocin_coupling_boost{0.2f};       ///< Oxytocin → global coupling ↑
};

/**
 * @brief Bidirectional adapter coupling CrystalBus to HormoneBus
 *
 * Follows the EndocrineConnector pattern:
 * - apply_endocrine_modulation(): VES → TCS (hormones modulate oscillations)
 * - apply_feedback(): TCS → VES (oscillations modulate hormones)
 *
 * Thread safety: reads are lock-free, writes accumulate deltas.
 */
class CrystalEndocrineAdapter {
public:
    explicit CrystalEndocrineAdapter(CrystalBus& crystal_bus,
                                      CrystalEndocrineConfig config = {}) noexcept
        : bus_(crystal_bus), config_(config) {}

    /**
     * @brief VES → TCS: Hormones modulate crystal oscillations
     *
     * Called during tick phase 3 (adapters READ hormones).
     * Reads current hormone concentrations and adjusts crystal bus
     * coupling strengths and amplitudes accordingly.
     *
     * @param hormones Current endocrine state (32 hormone concentrations)
     */
    void apply_endocrine_modulation(const endo::EndocrineState& hormones) noexcept {
        using H = endo::HormoneId;

        float cortisol = hormones[H::CORTISOL];
        float serotonin = hormones[H::SEROTONIN];
        float da_tonic = hormones[H::DOPAMINE_TONIC];
        float ne = hormones[H::NOREPINEPHRINE];
        float melatonin = hormones[H::MELATONIN];
        float oxytocin = hormones[H::OXYTOCIN];

        // Cortisol suppresses coupling (stress → desynchronization)
        float coupling_delta = -cortisol * config_.cortisol_coupling_suppression;

        // NE and Oxytocin boost coupling (vigilance/bonding → synchronization)
        coupling_delta += ne * config_.ne_coupling_boost;
        coupling_delta += oxytocin * config_.oxytocin_coupling_boost;

        // Apply coupling modulation
        float base_coupling = bus_.config().global_coupling;
        float new_coupling = std::clamp(base_coupling + coupling_delta, 0.0f, 1.0f);
        bus_.set_global_coupling(new_coupling);

        // Serotonin boosts slow oscillators (patience → sustained rhythm)
        bus_.set_amplitude(TemporalScaleId::ULTRA_SLOW,
            std::clamp(bus_.phase(TemporalScaleId::ULTRA_SLOW).amplitude +
                       serotonin * config_.serotonin_slow_boost, -1.0f, 1.0f));

        // DA_tonic boosts fast oscillators (motivation → processing speed)
        bus_.set_amplitude(TemporalScaleId::FAST,
            std::clamp(bus_.phase(TemporalScaleId::FAST).amplitude +
                       da_tonic * config_.dopamine_fast_boost, -1.0f, 1.0f));

        // Melatonin boosts slowest (maintenance → consolidation rhythm)
        bus_.set_amplitude(TemporalScaleId::SLOWEST,
            std::clamp(bus_.phase(TemporalScaleId::SLOWEST).amplitude +
                       melatonin * config_.melatonin_slowest_boost, -1.0f, 1.0f));

        prev_coherence_ = bus_.global_coherence();
    }

    /**
     * @brief TCS → VES: Crystal oscillations modulate hormones
     *
     * Called during tick phase 4 (adapters WRITE feedback).
     * Reads current crystal bus state and returns hormone deltas
     * to be applied to the endocrine bus.
     *
     * @param hormones Mutable endocrine state for writing feedback
     */
    void apply_feedback(endo::EndocrineState& hormones) noexcept {
        using H = endo::HormoneId;

        float coherence = bus_.global_coherence();

        // Global coherence → COG_COHERENCE channel
        hormones[H::COG_COHERENCE] = std::clamp(
            hormones[H::COG_COHERENCE] +
            coherence * config_.coherence_to_cog_coherence * 0.1f,
            0.0f, 1.0f);

        // ULTRA_SLOW amplitude → serotonin nudge
        float ultra_slow_amp = std::abs(bus_.phase(TemporalScaleId::ULTRA_SLOW).amplitude);
        hormones[H::SEROTONIN] = std::clamp(
            hormones[H::SEROTONIN] +
            ultra_slow_amp * config_.ultra_slow_to_serotonin,
            0.0f, 1.0f);

        // SLOWEST amplitude → melatonin nudge
        float slowest_amp = std::abs(bus_.phase(TemporalScaleId::SLOWEST).amplitude);
        hormones[H::MELATONIN] = std::clamp(
            hormones[H::MELATONIN] +
            slowest_amp * config_.slowest_to_melatonin,
            0.0f, 1.0f);

        // FAST amplitude → NE nudge
        float fast_amp = std::abs(bus_.phase(TemporalScaleId::FAST).amplitude);
        hormones[H::NOREPINEPHRINE] = std::clamp(
            hormones[H::NOREPINEPHRINE] +
            fast_amp * config_.fast_to_norepinephrine,
            0.0f, 1.0f);

        // Phase lock detection: coherence increase → DA_phasic reward
        if (coherence > prev_coherence_ + 0.05f) {
            hormones[H::DOPAMINE_PHASIC] = std::clamp(
                hormones[H::DOPAMINE_PHASIC] + config_.phase_lock_to_dopamine,
                0.0f, 1.0f);
        }

        // Coherence drop → cortisol (stress from desynchronization)
        if (coherence < prev_coherence_ - 0.1f) {
            hormones[H::CORTISOL] = std::clamp(
                hormones[H::CORTISOL] + 0.03f,
                0.0f, 1.0f);
        }

        prev_coherence_ = coherence;
    }

    [[nodiscard]] const CrystalEndocrineConfig& config() const noexcept { return config_; }
    void set_config(const CrystalEndocrineConfig& cfg) noexcept { config_ = cfg; }

private:
    CrystalBus& bus_;
    CrystalEndocrineConfig config_;
    float prev_coherence_{0.0f};
};

} // namespace opencog::temporal
