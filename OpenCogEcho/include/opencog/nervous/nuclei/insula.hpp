#pragma once
/**
 * @file insula.hpp
 * @brief Insular Cortex Nucleus — Interoception and felt sense
 *
 * The insula integrates interoceptive signals into a holistic "felt sense"
 * of the system's internal state. It bridges the gap between raw homeostatic
 * signals and conscious body awareness. Primary interface to FeltSense and
 * ValenceSignature in the VES.
 *
 * Cosmos S5: Heart center — embodied awareness
 *
 * Output: INSULA_INTEROCEPTION → bodily awareness / felt sense signal
 * Excitatory: HOMEOSTATIC_SIGNAL, AMYGDALA_VALENCE, PAIN_SIGNAL
 * Inhibitory: PREFRONTAL_EXEC (top-down suppression), SYMPATHETIC_OUT
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class InsulaNucleus : public NeuralNucleus {
public:
    explicit InsulaNucleus(NerveBus& bus)
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

        // Insula drives endocrine nudge when interoceptive awareness is high
        float awareness = std::abs(current_output_);
        if (awareness > 0.3f) {
            bus_.fire(NeuralChannelId::ENDOCRINE_NUDGE, awareness * 0.15f * dt);
        }

        // Strong interoception → hypothalamic bridge activation (homeostatic correction)
        if (awareness > 0.5f) {
            bus_.fire(NeuralChannelId::HYPOTHALAMIC_BRIDGE, awareness * 0.2f * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Insula";
        cfg.output_channel = NeuralChannelId::INSULA_INTEROCEPTION;
        cfg.base_output = 0.1f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.08f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::HOMEOSTATIC_SIGNAL, 0.5f},
            {NeuralChannelId::AMYGDALA_VALENCE, 0.3f},
            {NeuralChannelId::PAIN_SIGNAL, 0.4f},
            {NeuralChannelId::ENERGY_STATE, 0.2f},
            {NeuralChannelId::COHERENCE_SIGNAL, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::PREFRONTAL_EXEC, 0.3f},
            {NeuralChannelId::SYMPATHETIC_OUT, 0.2f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.002f;
        return cfg;
    }
};

} // namespace opencog::nerv
