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

namespace opencog::nerv {

class ThalamicECANAdapter : public NeuralConnector {
public:
    ThalamicECANAdapter(NerveBus& bus, AttentionBank& bank)
        : NeuralConnector(bus)
        , bank_(bank)
    {
        base_config_ = bank_.config();
        prev_gate_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        ECANConfig cfg = base_config_;

        float gate = bus.activation(NeuralChannelId::THALAMIC_GATE);
        float arousal = bus.activation(NeuralChannelId::RETICULAR_ACTIVATION);
        float attention = bus.activation(NeuralChannelId::MOTOR_ATTENTION);

        // Gate opening → lower AF boundary (more atoms in attentional focus)
        float gate_delta = rising_edge(gate, prev_gate_, 0.03f);
        cfg.af_boundary = base_config_.af_boundary - gate * 40.0f - gate_delta * 20.0f;

        // Arousal → spreading rate (alert = faster spreading)
        cfg.spreading_rate = base_config_.spreading_rate * (1.0f + arousal * 0.4f);

        // Attention steering signal → stimulus wages
        if (std::abs(attention) > 0.1f) {
            cfg.stimulus_wage = base_config_.stimulus_wage * (1.0f + std::abs(attention) * 0.3f);
        }

        bank_.set_config(cfg);
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
    AttentionBank& bank_;
    ECANConfig base_config_;
    float prev_gate_{0.0f};
    float prev_af_utilization_{0.0f};
};

} // namespace opencog::nerv
