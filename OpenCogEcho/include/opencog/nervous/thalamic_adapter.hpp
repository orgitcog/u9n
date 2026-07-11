#pragma once
/**
 * @file thalamic_adapter.hpp
 * @brief ThalamicECANAdapter — Bidirectional coupling to ECAN attention
 *
 * READ: THALAMIC_GATE → af_boundary modulation, spreading_rate
 * WRITE: STI changes → THALAMIC_RELAY channels, attention shifts → MOTOR_ATTENTION
 *
 * Delta-triggered: responds to changes in thalamic gate, not sustained levels
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/attention/attention_bank.hpp>

#include <cmath>

namespace opencog::nerv {

/**
 * @brief Composes transient thalamic ECAN deltas over the current config
 *
 * Endocrine ECAN modulation owns the absolute hormonal baseline. This adapter
 * reads the bank's current config each tick, removes only its previous
 * thalamic contribution when still present, then applies fresh neural deltas.
 */
class ThalamicECANAdapter : public NeuralConnector {
public:
    ThalamicECANAdapter(NerveBus& bus, AttentionBank& bank)
        : NeuralConnector(bus)
        , bank_(bank)
    {
        prev_gate_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        ECANConfig cfg = bank_.config();
        remove_previous_contribution(cfg);

        float gate = bus.activation(NeuralChannelId::THALAMIC_GATE);
        float arousal = bus.activation(NeuralChannelId::RETICULAR_ACTIVATION);
        float attention = bus.activation(NeuralChannelId::MOTOR_ATTENTION);

        // Gate opening → lower AF boundary (more atoms in attentional focus)
        float gate_delta = rising_edge(gate, prev_gate_, 0.03f);
        prev_af_delta_ = -gate * 40.0f - gate_delta * 20.0f;
        cfg.af_boundary += prev_af_delta_;

        // Arousal → spreading rate (alert = faster spreading)
        prev_spreading_multiplier_ = 1.0f + arousal * 0.4f;
        cfg.spreading_rate *= prev_spreading_multiplier_;

        // Attention steering signal → stimulus wages
        prev_stimulus_multiplier_ = 1.0f;
        if (std::abs(attention) > 0.1f) {
            prev_stimulus_multiplier_ = 1.0f + std::abs(attention) * 0.3f;
            cfg.stimulus_wage *= prev_stimulus_multiplier_;
        }

        bank_.set_config(cfg);
        last_written_config_ = cfg;
        has_written_config_ = true;
        prev_gate_ = gate;
    }

    void write_feedback() override {
        // Feed attention bank activity back as neural signals
        auto cfg = bank_.config();

        // Large STI changes → thalamic relay signals
        float af_utilization = static_cast<float>(cfg.af_boundary) / 200.0f;
        if (std::abs(af_utilization - prev_af_utilization_) > 0.05f) {
            bus_.fire(NeuralChannelId::THALAMIC_RELAY_1, af_utilization * 0.3f);
        }
        prev_af_utilization_ = af_utilization;
    }

private:
    static bool approx_equal(float a, float b) noexcept {
        return std::abs(a - b) <= 0.0001f;
    }

    void remove_previous_contribution(ECANConfig& cfg) const {
        if (!has_written_config_) {
            return;
        }

        if (approx_equal(cfg.af_boundary, last_written_config_.af_boundary)) {
            cfg.af_boundary -= prev_af_delta_;
        }
        if (approx_equal(cfg.spreading_rate, last_written_config_.spreading_rate)) {
            cfg.spreading_rate /= prev_spreading_multiplier_;
        }
        if (approx_equal(cfg.stimulus_wage, last_written_config_.stimulus_wage)) {
            cfg.stimulus_wage /= prev_stimulus_multiplier_;
        }
    }

    AttentionBank& bank_;
    float prev_gate_{0.0f};
    float prev_af_utilization_{0.0f};
    float prev_af_delta_{0.0f};
    float prev_spreading_multiplier_{1.0f};
    float prev_stimulus_multiplier_{1.0f};
    ECANConfig last_written_config_{};
    bool has_written_config_{false};
};

} // namespace opencog::nerv
