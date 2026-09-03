#pragma once
/**
 * @file hippocampus.hpp
 * @brief Hippocampal Nucleus — Memory encoding and retrieval
 *
 * The hippocampus mediates the transition from working memory to long-term
 * storage (encode) and retrieval (recall). It drives Marduk memory subsystem
 * and AtomSpace persistence.
 *
 * Cosmos S5: Crown center — knowledge integration
 *
 * Output: HIPPOCAMPAL_ENCODE → memory encoding signal
 * Excitatory: AMYGDALA_VALENCE (emotional salience), REWARD_PREDICTION, NOVELTY_SIGNAL
 * Inhibitory: FATIGUE_SIGNAL, SYMPATHETIC_OUT (stress impairs encoding)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class HippocampusNucleus : public NeuralNucleus {
public:
    explicit HippocampusNucleus(NerveBus& bus)
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

        // Also modulate recall channel based on encoding state
        // High encoding suppresses recall (encoding/retrieval antagonism)
        float encode = std::abs(current_output_);
        if (encode > 0.3f) {
            bus_.inhibit(NeuralChannelId::HIPPOCAMPAL_RECALL, encode * 0.3f * dt);
        }

        // Low arousal + working memory active → consolidation-friendly recall
        float arousal = std::abs(bus_.activation(NeuralChannelId::AROUSAL_SIGNAL));
        float wm = std::abs(bus_.activation(NeuralChannelId::WORKING_MEMORY));
        if (arousal < 0.3f && wm > 0.3f && encode < 0.2f) {
            bus_.fire(NeuralChannelId::HIPPOCAMPAL_RECALL, 0.3f * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "Hippocampus";
        cfg.output_channel = NeuralChannelId::HIPPOCAMPAL_ENCODE;
        cfg.base_output = 0.1f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.1f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::AMYGDALA_VALENCE, 0.4f},   // emotional salience
            {NeuralChannelId::REWARD_PREDICTION, 0.3f},
            {NeuralChannelId::NOVELTY_SIGNAL, 0.5f},
            {NeuralChannelId::WORKING_MEMORY, 0.3f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::FATIGUE_SIGNAL, 0.4f},
            {NeuralChannelId::SYMPATHETIC_OUT, 0.3f},  // stress impairs encoding
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.002f;
        return cfg;
    }
};

} // namespace opencog::nerv
