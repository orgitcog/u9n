#pragma once
/**
 * @file association_adapter.hpp
 * @brief AssociationNPUAdapter — Bidirectional coupling to NPU inference
 *
 * READ: ASSOCIATION_CORTEX → n_predict, creativity
 * WRITE: NPU output → SEMANTIC_AFFERENT, errors → ERROR_SIGNAL
 *
 * Uses the abstract NPUInterface from the endocrine system for decoupling.
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/endocrine/npu_types.hpp>

namespace opencog::nerv {

class AssociationNPUAdapter : public NeuralConnector {
public:
    AssociationNPUAdapter(NerveBus& bus, endo::NPUInterface& npu)
        : NeuralConnector(bus)
        , npu_(npu)
    {
        base_config_ = npu_.current_config();
        prev_association_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        endo::NPUEndocrineConfig cfg = base_config_;

        float association = bus.activation(NeuralChannelId::ASSOCIATION_CORTEX);
        float language = bus.activation(NeuralChannelId::LANGUAGE_CORTEX);
        float npu_cmd = bus.activation(NeuralChannelId::MOTOR_NPU_COMMAND);

        // Association cortex activation → higher n_predict (more generation)
        float assoc_delta = rising_edge(association, prev_association_, 0.03f);
        cfg.n_predict = base_config_.n_predict +
            static_cast<int32_t>(std::abs(association) * 64.0f + assoc_delta * 32.0f);

        // Language cortex → creativity (richer language = higher temperature)
        cfg.creativity = std::clamp(
            base_config_.creativity + std::abs(language) * 0.3f, 0.0f, 1.0f);

        // Motor NPU command → context pressure (urgency)
        if (std::abs(npu_cmd) > 0.1f) {
            cfg.context_pressure = std::clamp(
                base_config_.context_pressure + std::abs(npu_cmd) * 0.3f,
                0.0f, 1.0f);
        }

        npu_.apply_config(cfg);
        prev_association_ = association;
    }

    void write_feedback() override {
        auto tel = npu_.telemetry();

        // NPU inference output → semantic afferent
        if (tel.is_busy) {
            bus_.fire(NeuralChannelId::SEMANTIC_AFFERENT,
                      tel.context_utilization * 0.5f, 0.3f);
        }

        // NPU errors → error signal
        if (tel.has_error && !prev_error_) {
            bus_.fire(NeuralChannelId::ERROR_SIGNAL, 0.5f, 0.8f);
        }
        prev_error_ = tel.has_error;

        // High load → fatigue signal
        if (tel.context_utilization > 0.8f) {
            bus_.fire(NeuralChannelId::FATIGUE_SIGNAL,
                      (tel.context_utilization - 0.8f) * 2.0f, 0.3f);
        }
    }

private:
    endo::NPUInterface& npu_;
    endo::NPUEndocrineConfig base_config_;
    float prev_association_{0.0f};
    bool prev_error_{false};
};

} // namespace opencog::nerv
