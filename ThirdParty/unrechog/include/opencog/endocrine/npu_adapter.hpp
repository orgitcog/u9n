#pragma once
/**
 * @file npu_adapter.hpp
 * @brief NPU Endocrine Adapter — Bidirectional hormonal coupling to the NPU
 *
 * Maps hormone concentrations to NPU inference parameters (creativity,
 * n_predict, batch_size, context_pressure) and feeds NPU telemetry
 * back into the hormone bus (errors → IL6, load → NPU_LOAD channel,
 * success → dopamine).
 *
 * Follows the EndocrineConnector pattern established by ECAN and PLN
 * adapters, extended with apply_feedback() for the write-back path.
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/npu_types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::endo {

/**
 * @brief Maps between hormone bus and NPU inference parameters
 *
 * READ path (apply_endocrine_modulation):
 *   Serotonin       → n_predict      (patience = longer generation)
 *   Dopamine_tonic   → creativity     (exploration = higher temperature)
 *   Dopamine_phasic  → creativity     (reward amplifies creativity)
 *   Cortisol         → creativity↓    (stress demands conservative output)
 *   Norepinephrine   → batch_size↓    (vigilance = smaller, controlled batches)
 *   T3/T4            → context_pressure (thyroid governs processing throughput)
 *   CognitiveMode    → system_prompt_index (mode → persona-appropriate prompt)
 *
 * WRITE path (apply_feedback):
 *   NPU busy + high context → NPU_LOAD channel
 *   NPU error              → IL6 + cortisol
 *   NPU success            → dopamine_phasic
 *   NPU tokens/sec         → T3/T4 (small nudge reflecting processing speed)
 */
class NPUEndocrineAdapter : public EndocrineConnector {
public:
    NPUEndocrineAdapter(HormoneBus& bus, NPUInterface& npu)
        : EndocrineConnector(bus)
        , npu_(npu)
    {
        base_config_ = npu_.current_config();
        prev_telemetry_ = npu_.telemetry();
    }

    // === Read path: Hormones → NPU modulation ===

    void apply_endocrine_modulation(const HormoneBus& bus) {
        NPUEndocrineConfig cfg = base_config_;

        float serotonin   = bus.concentration(HormoneId::SEROTONIN);
        float da_tonic    = bus.concentration(HormoneId::DOPAMINE_TONIC);
        float da_phasic   = bus.concentration(HormoneId::DOPAMINE_PHASIC);
        float cortisol    = bus.concentration(HormoneId::CORTISOL);
        float ne          = bus.concentration(HormoneId::NOREPINEPHRINE);
        float thyroid     = bus.concentration(HormoneId::T3_T4);

        // Serotonin → patience → longer generation
        cfg.n_predict = static_cast<int32_t>(
            static_cast<float>(base_config_.n_predict) * (0.5f + serotonin));
        cfg.n_predict = std::max(cfg.n_predict, static_cast<int32_t>(16));

        // Dopamine → creativity / temperature
        cfg.creativity = base_config_.creativity
            * (1.0f + da_tonic * 0.3f + da_phasic * 0.5f);

        // Cortisol → reduce creativity (conservative under stress)
        cfg.creativity *= std::max(0.3f, 1.0f - cortisol * 0.5f);
        cfg.creativity = std::clamp(cfg.creativity, 0.1f, 1.0f);

        // Norepinephrine → smaller batches (vigilance = careful processing)
        float batch_reduction = ne * static_cast<float>(base_config_.batch_size) * 0.5f;
        cfg.batch_size = std::max(1,
            static_cast<int32_t>(static_cast<float>(base_config_.batch_size) - batch_reduction));

        // Thyroid → context utilization pressure
        cfg.context_pressure = std::clamp(thyroid, 0.1f, 1.0f);

        // CognitiveMode → system prompt selection
        cfg.system_prompt_index = mode_to_prompt_index(bus.current_mode());

        npu_.apply_config(cfg);
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    // === Write path: NPU telemetry → Hormone feedback ===

    /**
     * @brief Feed NPU operational state back into the hormone bus
     *
     * Called after apply_endocrine_modulation in the tick pipeline.
     * Reads telemetry and produces hormonal feedback.
     */
    void apply_feedback() {
        NPUTelemetry tel = npu_.telemetry();

        // --- NPU Load channel (channel 14) ---
        if (tel.is_busy) {
            bus_.produce(HormoneId::NPU_LOAD, tel.context_utilization * 0.15f);
        }

        // --- High context utilization → resource pressure ---
        if (tel.context_utilization > 0.8f) {
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
            bus_.produce(HormoneId::CORTISOL, 0.03f);
        }

        // --- Error → immune signal + stress ---
        if (tel.has_error && !prev_telemetry_.has_error) {
            // New error (edge-triggered)
            bus_.produce(HormoneId::IL6, 0.3f);
            bus_.produce(HormoneId::CORTISOL, 0.1f);
        }

        // --- Successful completion → reward signal ---
        if (!tel.is_busy && prev_telemetry_.is_busy && !tel.has_error) {
            // Just finished inference successfully (edge-triggered)
            float reward = 0.15f / (1.0f + static_cast<float>(tel.error_count) * 0.1f);
            bus_.produce(HormoneId::DOPAMINE_PHASIC, reward);
        }

        // --- Tokens/sec → small thyroid nudge (processing speed feedback) ---
        if (tel.tokens_per_second > 0.0f) {
            float tps_normalized = std::clamp(tel.tokens_per_second / expected_tps_, 0.0f, 1.0f);
            bus_.produce(HormoneId::T3_T4, tps_normalized * 0.02f);
        }

        prev_telemetry_ = tel;
    }

    // === Configuration ===

    /// Set expected tokens per second for normalization
    void set_expected_tps(float tps) noexcept { expected_tps_ = tps; }

    /// Get the base (unmodulated) configuration
    [[nodiscard]] const NPUEndocrineConfig& base_config() const noexcept {
        return base_config_;
    }

    /// Access the NPU interface (for external state queries)
    [[nodiscard]] NPUInterface& npu() noexcept { return npu_; }
    [[nodiscard]] const NPUInterface& npu() const noexcept { return npu_; }

private:
    NPUInterface& npu_;
    NPUEndocrineConfig base_config_;
    NPUTelemetry prev_telemetry_;  ///< For edge detection
    float expected_tps_{30.0f};    ///< Expected tokens/sec for normalization

    /**
     * @brief Map cognitive mode to a system prompt index
     *
     * Different modes call for different system prompt personas:
     * 0 = neutral, 1 = reflective, 2 = exploratory, 3 = focused,
     * 4 = cautious, 5 = social, 6 = creative
     */
    [[nodiscard]] static int mode_to_prompt_index(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::RESTING:     return 0;  // neutral
        case CognitiveMode::REFLECTIVE:  return 1;  // reflective/contemplative
        case CognitiveMode::EXPLORATORY: return 2;  // exploratory/curious
        case CognitiveMode::FOCUSED:     return 3;  // focused/precise
        case CognitiveMode::VIGILANT:    return 4;  // cautious/alert
        case CognitiveMode::SOCIAL:      return 5;  // social/empathic
        case CognitiveMode::REWARD:      return 6;  // creative/expansive
        case CognitiveMode::STRESSED:    return 4;  // cautious under stress
        case CognitiveMode::THREAT:      return 4;  // cautious under threat
        case CognitiveMode::MAINTENANCE: return 0;  // neutral during maintenance
        default:                         return 0;
        }
    }
};

} // namespace opencog::endo
