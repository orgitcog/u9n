#pragma once
/**
 * @file thalamus.hpp
 * @brief Thalamic Nucleus — Attentional gate for sensory relay
 *
 * The thalamus is the primary sensory relay station. It gates which sensory
 * signals reach cortical processing based on attentional state (ECAN).
 *
 * Cosmos S5: Heart center — gateway of perception
 *
 * Output: THALAMIC_GATE → controls sensory relay strength
 * Excitatory: RETICULAR_ACTIVATION (arousal), VISUAL_PRIMARY, AUDITORY_PRIMARY
 * Inhibitory: FATIGUE_SIGNAL (resource depletion), PARASYMPATHETIC_OUT (calming)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class ThalamusNucleus : public SimpleNucleus {
public:
    explicit ThalamusNucleus(NerveBus& bus)
        : SimpleNucleus(bus, make_config()) {}

    /// Thalamus also modulates relay channels proportionally to gate strength
    void update(float dt) override {
        SimpleNucleus::update(dt);

        // Modulate relay channels: gate strength scales sensory throughput
        float gate = std::abs(bus_.activation(NeuralChannelId::THALAMIC_GATE));
        if (gate > 0.1f) {
            // Boost relay channels by gate level
            float relay1 = bus_.activation(NeuralChannelId::THALAMIC_RELAY_1);
            float relay2 = bus_.activation(NeuralChannelId::THALAMIC_RELAY_2);
            bus_.set_activation(NeuralChannelId::THALAMIC_RELAY_1,
                                relay1 * (0.5f + 0.5f * gate));
            bus_.set_activation(NeuralChannelId::THALAMIC_RELAY_2,
                                relay2 * (0.5f + 0.5f * gate));
        }
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Thalamus";
        cfg.output_channel = NeuralChannelId::THALAMIC_GATE;
        cfg.base_output = 0.2f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.05f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::RETICULAR_ACTIVATION, 0.6f},
            {NeuralChannelId::VISUAL_PRIMARY, 0.3f},
            {NeuralChannelId::AUDITORY_PRIMARY, 0.3f},
            {NeuralChannelId::SOMATOSENSORY, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::FATIGUE_SIGNAL, 0.4f},
            {NeuralChannelId::PARASYMPATHETIC_OUT, 0.3f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.001f;
        return cfg;
    }
};

} // namespace opencog::nerv
