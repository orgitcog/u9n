#pragma once
/**
 * @file prefrontal_cortex.hpp
 * @brief Prefrontal Cortex Nucleus — Executive function and deliberate reasoning
 *
 * The prefrontal cortex is the highest-order processing nucleus. It integrates
 * working memory, reasoning, and executive intent into deliberate motor
 * commands. Primary interface to Marduk executive and PLN reasoning.
 *
 * Cosmos S5: Head/Third Eye center — executive consciousness
 *
 * Output: PREFRONTAL_EXEC → executive control signal
 * Excitatory: WORKING_MEMORY, REASONING_CORTEX, ASSOCIATION_CORTEX
 * Inhibitory: FATIGUE_SIGNAL, SYMPATHETIC_OUT (stress degrades executive function)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class PrefrontalCortexNucleus : public NeuralNucleus {
public:
    explicit PrefrontalCortexNucleus(NerveBus& bus)
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

        // Top-down regulation: strong PFC suppresses amygdala reactivity
        float pfc = std::abs(current_output_);
        if (pfc > 0.4f) {
            bus_.inhibit(NeuralChannelId::AMYGDALA_VALENCE, pfc * 0.15f * dt);
            bus_.inhibit(NeuralChannelId::AMYGDALA_AROUSAL, pfc * 0.1f * dt);
        }

        // Executive → working memory maintenance
        if (pfc > 0.3f) {
            bus_.fire(NeuralChannelId::WORKING_MEMORY, pfc * 0.2f * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "PrefrontalCortex";
        cfg.output_channel = NeuralChannelId::PREFRONTAL_EXEC;
        cfg.base_output = 0.15f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.12f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::WORKING_MEMORY, 0.5f},
            {NeuralChannelId::REASONING_CORTEX, 0.4f},
            {NeuralChannelId::ASSOCIATION_CORTEX, 0.3f},
            {NeuralChannelId::THALAMIC_GATE, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::FATIGUE_SIGNAL, 0.5f},
            {NeuralChannelId::SYMPATHETIC_OUT, 0.3f},  // stress degrades exec
            {NeuralChannelId::PAIN_SIGNAL, 0.2f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.002f;
        return cfg;
    }
};

} // namespace opencog::nerv
