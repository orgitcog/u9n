#pragma once
/**
 * @file anterior_cingulate.hpp
 * @brief Anterior Cingulate Cortex Nucleus — Conflict monitoring and error detection
 *
 * The ACC detects response conflicts (competing action candidates), monitors
 * confidence in PLN inferences, and signals errors that require adaptive
 * behavioral adjustments.
 *
 * Cosmos S5: Brow/Third Eye center — discernment
 *
 * Output: ANTERIOR_CINGULATE → conflict monitoring signal
 * Excitatory: ERROR_SIGNAL, CEREBELLAR_ERROR, AMYGDALA_VALENCE
 * Inhibitory: REWARD_PREDICTION (success suppresses conflict), COHERENCE_SIGNAL
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class AnteriorCingulateNucleus : public NeuralNucleus {
public:
    explicit AnteriorCingulateNucleus(NerveBus& bus)
        : NeuralNucleus(bus, make_config()) {}

    float compute_output() const override {
        float exc = read_excitation();
        float inh = read_inhibition();

        // Also detect GO/NOGO competition (high both = conflict)
        float go = std::abs(bus_.activation(NeuralChannelId::BASAL_GANGLIA_GO));
        float nogo = std::abs(bus_.activation(NeuralChannelId::BASAL_GANGLIA_NOGO));
        float competition = std::min(go, nogo) * 2.0f;  // high when both active

        float net = config_.base_output + exc + competition - inh;

        if (std::abs(net) < adapted_threshold_) {
            return 0.0f;
        }

        return apply_self_inhibition(net);
    }

    void update(float dt) override {
        current_output_ = compute_output();
        emit(current_output_ * dt);

        // High conflict → NOGO bias (be cautious)
        float conflict = std::abs(current_output_);
        if (conflict > 0.4f) {
            bus_.fire(NeuralChannelId::BASAL_GANGLIA_NOGO, conflict * 0.3f * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "AnteriorCingulate";
        cfg.output_channel = NeuralChannelId::ANTERIOR_CINGULATE;
        cfg.base_output = 0.05f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.1f;
        cfg.excitatory_inputs = {
            {NeuralChannelId::ERROR_SIGNAL, 0.6f},
            {NeuralChannelId::CEREBELLAR_ERROR, 0.4f},
            {NeuralChannelId::AMYGDALA_VALENCE, 0.2f},
            {NeuralChannelId::PAIN_SIGNAL, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::REWARD_PREDICTION, 0.4f},
            {NeuralChannelId::COHERENCE_SIGNAL, 0.3f},
            {NeuralChannelId::PARASYMPATHETIC_OUT, 0.2f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.25f;
        cfg.adaptation_rate = 0.002f;
        return cfg;
    }
};

} // namespace opencog::nerv
