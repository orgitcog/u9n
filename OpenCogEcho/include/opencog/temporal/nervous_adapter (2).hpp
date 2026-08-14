#pragma once
/**
 * @file nervous_adapter.hpp
 * @brief Bidirectional coupling between Temporal Crystal System and VNS
 *
 * Maps temporal crystal oscillation states to neural channel activations
 * and vice versa. The TCS operates at intermediate timescales (8ms-1s)
 * that bridge the VNS (ms) and VES (seconds-minutes) domains.
 *
 * TCS → VNS (READ phase: oscillations modulate neural channels):
 *   ULTRA_FAST amplitude → TEMPORAL_CORTEX (fast temporal processing)
 *   MEDIUM amplitude     → REASONING_CORTEX (decision/reasoning speed)
 *   VERY_SLOW amplitude  → WORKING_MEMORY (sustained working memory)
 *   Global coherence     → COHERENCE_SIGNAL (cross-system coherence)
 *   Phase lock events    → REWARD_PREDICTION (successful synchronization)
 *
 * VNS → TCS (WRITE phase: neural channels modulate oscillations):
 *   THALAMIC_GATE        → Global coupling ↑ (attention gates synchronization)
 *   PREFRONTAL_EXEC      → MEDIUM amplitude ↑ (executive boosts decisions)
 *   HIPPOCAMPAL_ENCODE   → VERY_SLOW phase reset (memory encoding window)
 *   CEREBELLAR_PREDICT   → Coupling refinement (prediction tunes phases)
 *   AROUSAL_SIGNAL       → All amplitudes scale (arousal gates processing)
 */

#include <opencog/temporal/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>
#include <opencog/nervous/types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::temporal {

/**
 * @brief Configuration for crystal↔neural coupling
 */
struct CrystalNeuralConfig {
    // TCS → VNS gains
    float ultra_fast_to_temporal{0.3f};       ///< ULTRA_FAST → TEMPORAL_CORTEX
    float medium_to_reasoning{0.25f};         ///< MEDIUM → REASONING_CORTEX
    float very_slow_to_working_mem{0.3f};     ///< VERY_SLOW → WORKING_MEMORY
    float coherence_to_signal{0.5f};          ///< Coherence → COHERENCE_SIGNAL
    float phase_lock_to_reward{0.2f};         ///< Phase lock → REWARD_PREDICTION

    // VNS → TCS gains
    float thalamic_coupling_boost{0.15f};     ///< THALAMIC_GATE → coupling ↑
    float prefrontal_medium_boost{0.2f};      ///< PREFRONTAL_EXEC → MEDIUM amp ↑
    float hippocampal_phase_reset{0.5f};      ///< HIPPOCAMPAL_ENCODE → phase reset strength
    float cerebellar_coupling_refine{0.1f};   ///< CEREBELLAR_PREDICT → coupling refinement
    float arousal_amplitude_scale{0.3f};      ///< AROUSAL_SIGNAL → amplitude scaling
};

/**
 * @brief Bidirectional adapter coupling CrystalBus to NerveBus
 *
 * Follows the same pattern as CrystalEndocrineAdapter:
 * - apply_neural_modulation(): VNS → TCS (neural signals modulate oscillations)
 * - apply_feedback(): TCS → VNS (oscillations modulate neural channels)
 */
class CrystalNeuralAdapter {
public:
    explicit CrystalNeuralAdapter(CrystalBus& crystal_bus,
                                    CrystalNeuralConfig config = {}) noexcept
        : bus_(crystal_bus), config_(config) {}

    /**
     * @brief VNS → TCS: Neural signals modulate crystal oscillations
     *
     * Called during tick phase 3 (adapters READ signals).
     *
     * @param neural Current neural state (64 channel activations, bipolar [-1,+1])
     */
    void apply_neural_modulation(const nerv::NeuralState& neural) noexcept {
        using Ch = nerv::NeuralChannelId;

        float thalamic = neural[Ch::THALAMIC_GATE];
        float prefrontal = neural[Ch::PREFRONTAL_EXEC];
        float hippocampal = neural[Ch::HIPPOCAMPAL_ENCODE];
        float cerebellar = neural[Ch::CEREBELLAR_PREDICT];
        float arousal = neural[Ch::AROUSAL_SIGNAL];

        // Thalamic gating → coupling boost (attention synchronizes)
        if (thalamic > 0.1f) {
            float base = bus_.config().global_coupling;
            bus_.set_global_coupling(std::clamp(
                base + thalamic * config_.thalamic_coupling_boost, 0.0f, 1.0f));
        }

        // Prefrontal executive → MEDIUM amplitude (decision facilitation)
        if (prefrontal > 0.1f) {
            float current = bus_.phase(TemporalScaleId::MEDIUM).amplitude;
            bus_.set_amplitude(TemporalScaleId::MEDIUM,
                std::clamp(current + prefrontal * config_.prefrontal_medium_boost,
                           -1.0f, 1.0f));
        }

        // Hippocampal encoding → VERY_SLOW phase nudge (memory window)
        if (hippocampal > 0.3f) {
            bus_.inject(TemporalScaleId::VERY_SLOW,
                hippocampal * config_.hippocampal_phase_reset);
        }

        // Cerebellar prediction → coupling refinement
        if (std::abs(cerebellar) > 0.1f) {
            // Prediction error sharpens coupling
            float base = bus_.config().global_coupling;
            bus_.set_global_coupling(std::clamp(
                base + cerebellar * config_.cerebellar_coupling_refine,
                0.0f, 1.0f));
        }

        // Arousal scales all amplitudes
        if (std::abs(arousal) > 0.1f) {
            float scale_factor = 1.0f + arousal * config_.arousal_amplitude_scale;
            for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
                auto sid = static_cast<TemporalScaleId>(i);
                float current = bus_.phase(sid).amplitude;
                bus_.set_amplitude(sid, std::clamp(current * scale_factor, -1.0f, 1.0f));
            }
        }

        prev_coherence_ = bus_.global_coherence();
    }

    /**
     * @brief TCS → VNS: Crystal oscillations modulate neural channels
     *
     * Called during tick phase 4 (adapters WRITE feedback).
     *
     * @param neural Mutable neural state for writing feedback
     */
    void apply_feedback(nerv::NeuralState& neural) noexcept {
        using Ch = nerv::NeuralChannelId;

        // ULTRA_FAST amplitude → TEMPORAL_CORTEX
        float ultra_fast = std::abs(bus_.phase(TemporalScaleId::ULTRA_FAST).amplitude);
        neural[Ch::TEMPORAL_CORTEX] = std::clamp(
            neural[Ch::TEMPORAL_CORTEX] + ultra_fast * config_.ultra_fast_to_temporal,
            -1.0f, 1.0f);

        // MEDIUM amplitude → REASONING_CORTEX
        float medium = std::abs(bus_.phase(TemporalScaleId::MEDIUM).amplitude);
        neural[Ch::REASONING_CORTEX] = std::clamp(
            neural[Ch::REASONING_CORTEX] + medium * config_.medium_to_reasoning,
            -1.0f, 1.0f);

        // VERY_SLOW amplitude → WORKING_MEMORY
        float very_slow = std::abs(bus_.phase(TemporalScaleId::VERY_SLOW).amplitude);
        neural[Ch::WORKING_MEMORY] = std::clamp(
            neural[Ch::WORKING_MEMORY] + very_slow * config_.very_slow_to_working_mem,
            -1.0f, 1.0f);

        // Global coherence → COHERENCE_SIGNAL
        float coherence = bus_.global_coherence();
        neural[Ch::COHERENCE_SIGNAL] = std::clamp(
            neural[Ch::COHERENCE_SIGNAL] + coherence * config_.coherence_to_signal,
            -1.0f, 1.0f);

        // Phase lock detection → REWARD_PREDICTION
        if (coherence > prev_coherence_ + 0.05f) {
            neural[Ch::REWARD_PREDICTION] = std::clamp(
                neural[Ch::REWARD_PREDICTION] + config_.phase_lock_to_reward,
                -1.0f, 1.0f);
        }

        prev_coherence_ = coherence;
    }

    [[nodiscard]] const CrystalNeuralConfig& config() const noexcept { return config_; }
    void set_config(const CrystalNeuralConfig& cfg) noexcept { config_ = cfg; }

private:
    CrystalBus& bus_;
    CrystalNeuralConfig config_;
    float prev_coherence_{0.0f};
};

} // namespace opencog::temporal
