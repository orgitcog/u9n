#pragma once
/**
 * @file cortical_adapter.hpp
 * @brief CorticalPLNAdapter — Bidirectional coupling to PLN inference
 *
 * READ: REASONING_CORTEX → max_iterations, confidence thresholds
 * WRITE: Inference results → PATTERN_CORTEX, error → ERROR_SIGNAL
 *
 * Delta-triggered: responds to changes in reasoning cortex activation
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/pln/inference.hpp>

namespace opencog::nerv {

class CorticalPLNAdapter : public NeuralConnector {
public:
    CorticalPLNAdapter(NerveBus& bus, pln::PLNEngine& engine)
        : NeuralConnector(bus)
        , engine_(engine)
    {
        base_config_ = engine_.config();
        prev_reasoning_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        pln::InferenceConfig cfg = base_config_;

        float reasoning = bus.activation(NeuralChannelId::REASONING_CORTEX);
        float working_mem = bus.activation(NeuralChannelId::WORKING_MEMORY);
        float conflict = bus.activation(NeuralChannelId::ANTERIOR_CINGULATE);

        // Reasoning cortex activation → more iterations (deeper inference)
        float reasoning_delta = rising_edge(reasoning, prev_reasoning_, 0.03f);
        cfg.max_iterations = static_cast<size_t>(
            static_cast<float>(base_config_.max_iterations) *
            (1.0f + std::abs(reasoning) * 0.5f + reasoning_delta * 0.3f));

        // Conflict detection → higher confidence threshold (be more cautious)
        if (std::abs(conflict) > 0.2f) {
            cfg.min_confidence = base_config_.min_confidence +
                                  std::abs(conflict) * 0.3f;
        }

        // Working memory → attention threshold (broader search when WM active)
        if (std::abs(working_mem) > 0.2f) {
            cfg.attention_threshold = base_config_.attention_threshold *
                                       (1.0f - std::abs(working_mem) * 0.3f);
        }

        engine_.set_config(cfg);
        prev_reasoning_ = reasoning;
    }

    void write_feedback() override {
        // PLN inference results feed back to cortical channels
        auto cfg = engine_.config();

        // Confidence level maps to pattern cortex
        float confidence = cfg.min_confidence;
        if (std::abs(confidence - prev_confidence_) > 0.05f) {
            bus_.fire(NeuralChannelId::PATTERN_CORTEX,
                      confidence * 0.4f, 0.3f);
        }
        prev_confidence_ = confidence;
    }

private:
    pln::PLNEngine& engine_;
    pln::InferenceConfig base_config_;
    float prev_reasoning_{0.0f};
    float prev_confidence_{0.0f};
};

} // namespace opencog::nerv
