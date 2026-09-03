#pragma once
/**
 * @file hypothalamus.hpp
 * @brief Hypothalamic Nucleus — VES ↔ VNS coupling bridge
 *
 * The hypothalamus is the master neuroendocrine interface. It reads homeostatic
 * signals and drives the NeuroEndocrine bridge channel that couples to the VES.
 *
 * Cosmos S5: Bridge — connecting electrical and chemical signaling
 *
 * Output: HYPOTHALAMIC_BRIDGE → NeuroEndocrine bridge coupling
 * Excitatory: HOMEOSTATIC_SIGNAL (interoception), INSULA_INTEROCEPTION (felt sense)
 * Inhibitory: COHERENCE_SIGNAL (stability suppresses bridge activity)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class HypothalamusNucleus : public SimpleNucleus {
public:
    explicit HypothalamusNucleus(NerveBus& bus)
        : SimpleNucleus(bus, make_config()) {}

    /// Hypothalamus also drives autonomic outputs based on bridge state
    void update(float dt) override {
        SimpleNucleus::update(dt);

        float bridge = std::abs(bus_.activation(NeuralChannelId::HYPOTHALAMIC_BRIDGE));
        if (bridge > 0.3f) {
            // High bridge activation → parasympathetic correction
            bus_.fire(NeuralChannelId::PARASYMPATHETIC_OUT, bridge * 0.2f * dt);
        }
        if (bridge > 0.6f) {
            // Very high bridge → endocrine nudge
            bus_.fire(NeuralChannelId::ENDOCRINE_NUDGE, bridge * 0.15f * dt);
        }
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Hypothalamus";
        cfg.output_channel = NeuralChannelId::HYPOTHALAMIC_BRIDGE;
        cfg.base_output = 0.1f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.08f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::HOMEOSTATIC_SIGNAL, 0.6f},
            {NeuralChannelId::INSULA_INTEROCEPTION, 0.4f},
            {NeuralChannelId::PAIN_SIGNAL, 0.3f},
            {NeuralChannelId::ENERGY_STATE, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::COHERENCE_SIGNAL, 0.4f},
            {NeuralChannelId::PREFRONTAL_EXEC, 0.2f},  // top-down suppression
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.25f;
        cfg.adaptation_rate = 0.003f;
        return cfg;
    }
};

} // namespace opencog::nerv
