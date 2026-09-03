#pragma once
/**
 * @file amygdala.hpp
 * @brief Amygdala Nucleus — Fast emotional valence and arousal processing
 *
 * The amygdala provides fast emotional valuation of stimuli, with dual outputs
 * for valence (positive/negative) and arousal (intensity). It is the key
 * nucleus in the sympathetic fast-path (1-3 tick threat response).
 *
 * Cosmos S5: Sacral center — emotional core
 *
 * Output: AMYGDALA_VALENCE → emotional evaluation signal
 * Excitatory: THREAT_AFFERENT (fast path), SOCIAL_AFFERENT, NOVELTY_SIGNAL
 * Inhibitory: PREFRONTAL_EXEC (top-down regulation), COHERENCE_SIGNAL
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class AmygdalaNucleus : public NeuralNucleus {
public:
    explicit AmygdalaNucleus(NerveBus& bus)
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

        // Amygdala also drives arousal channel (second output)
        float arousal = std::abs(current_output_) * 0.8f;
        if (arousal > 0.05f) {
            bus_.fire(NeuralChannelId::AMYGDALA_AROUSAL, arousal * dt);
        }

        // Threat shortcut: bypass thalamic gating for survival-critical stimuli
        float threat = bus_.activation(NeuralChannelId::THREAT_AFFERENT);
        if (threat > 0.6f) {
            bus_.fire(NeuralChannelId::SYMPATHETIC_OUT, threat * 0.5f * dt, 1.0f);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Amygdala";
        cfg.output_channel = NeuralChannelId::AMYGDALA_VALENCE;
        cfg.base_output = 0.0f;  // neutral at rest
        cfg.max_output = 1.0f;
        cfg.threshold = 0.05f;   // low threshold — fast responder
        cfg.excitatory_inputs = {
            {NeuralChannelId::THREAT_AFFERENT, 0.8f},
            {NeuralChannelId::SOCIAL_AFFERENT, 0.3f},
            {NeuralChannelId::NOVELTY_SIGNAL, 0.4f},
            {NeuralChannelId::PAIN_SIGNAL, 0.5f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::PREFRONTAL_EXEC, 0.5f},   // cortical regulation
            {NeuralChannelId::COHERENCE_SIGNAL, 0.3f},
            {NeuralChannelId::PARASYMPATHETIC_OUT, 0.3f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.15f;
        cfg.adaptation_rate = 0.001f;  // slow adaptation — stays sensitive
        return cfg;
    }
};

} // namespace opencog::nerv
