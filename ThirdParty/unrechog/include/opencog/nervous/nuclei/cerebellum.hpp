#pragma once
/**
 * @file cerebellum.hpp
 * @brief Cerebellar Nucleus — Prediction, timing, and error correction
 *
 * The cerebellum maintains forward models for prediction and generates
 * prediction error signals when actual outcomes differ from expectations.
 * Primary interface to the o9c2 ESN prediction system.
 *
 * Cosmos S5: Throat center — expression and prediction
 *
 * Output: CEREBELLAR_PREDICT → prediction/timing signal
 * Excitatory: TEMPORAL_CORTEX (sequence patterns), ASSOCIATION_CORTEX
 * Inhibitory: ERROR_SIGNAL (error dampens overconfident predictions)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class CerebellumNucleus : public NeuralNucleus {
public:
    explicit CerebellumNucleus(NerveBus& bus)
        : NeuralNucleus(bus, make_config()) {}

    float compute_output() const override {
        float exc = read_excitation();
        float inh = read_inhibition();
        float net = config_.base_output + exc - inh;

        if (std::abs(net) < adapted_threshold_) {
            return 0.0f;
        }

        return apply_self_inhibition(net);
    }

    void update(float dt) override {
        current_output_ = compute_output();
        emit(current_output_ * dt);

        // Compute prediction error from actual vs predicted
        float predicted = bus_.activation(NeuralChannelId::CEREBELLAR_PREDICT);
        float actual_novelty = bus_.activation(NeuralChannelId::NOVELTY_SIGNAL);
        float actual_error = bus_.activation(NeuralChannelId::ERROR_SIGNAL);
        float prediction_error = std::abs(actual_novelty + actual_error) -
                                  std::abs(predicted) * 0.5f;

        if (std::abs(prediction_error) > 0.1f) {
            bus_.fire(NeuralChannelId::CEREBELLAR_ERROR, prediction_error * 0.6f * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Cerebellum";
        cfg.output_channel = NeuralChannelId::CEREBELLAR_PREDICT;
        cfg.base_output = 0.1f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.08f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::TEMPORAL_CORTEX, 0.5f},
            {NeuralChannelId::ASSOCIATION_CORTEX, 0.3f},
            {NeuralChannelId::PATTERN_CORTEX, 0.3f},
            {NeuralChannelId::WORKING_MEMORY, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::ERROR_SIGNAL, 0.3f},
            {NeuralChannelId::CEREBELLAR_ERROR, 0.2f}, // own error feeds back
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.003f;  // moderate — learns from errors
        return cfg;
    }
};

} // namespace opencog::nerv
