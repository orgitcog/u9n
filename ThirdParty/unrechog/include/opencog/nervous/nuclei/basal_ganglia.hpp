#pragma once
/**
 * @file basal_ganglia.hpp
 * @brief Basal Ganglia Nucleus — Action selection (GO/NOGO)
 *
 * The basal ganglia implement a competitive GO/NOGO circuit for action
 * selection. GO signals enable motor outputs; NOGO inhibits them.
 * This is the primary interface to Marduk task dispatch.
 *
 * Cosmos S5: Solar plexus — will and action
 *
 * Output: BASAL_GANGLIA_GO → action initiation signal
 * Excitatory: PREFRONTAL_EXEC (executive intent), REWARD_PREDICTION (motivation)
 * Inhibitory: ANTERIOR_CINGULATE (conflict), AMYGDALA_AROUSAL (freeze response)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class BasalGangliaNucleus : public NeuralNucleus {
public:
    explicit BasalGangliaNucleus(NerveBus& bus)
        : NeuralNucleus(bus, make_config()) {}

    float compute_output() const override {
        float exc = read_excitation();
        float inh = read_inhibition();

        // Also read NOGO channel as direct antagonist
        float nogo = std::abs(bus_.activation(NeuralChannelId::BASAL_GANGLIA_NOGO));
        float net = config_.base_output + exc - inh - nogo * 0.6f;

        if (net < adapted_threshold_) {
            return 0.0f;  // GO is always positive (action vs no action)
        }

        return apply_self_inhibition(net);
    }

    void update(float dt) override {
        current_output_ = compute_output();
        emit(current_output_ * dt);

        // Simultaneously modulate NOGO based on conflict signals
        float conflict = std::abs(bus_.activation(NeuralChannelId::ANTERIOR_CINGULATE));
        float error = std::abs(bus_.activation(NeuralChannelId::ERROR_SIGNAL));
        float nogo_drive = (conflict + error) * 0.4f;
        if (nogo_drive > 0.05f) {
            bus_.fire(NeuralChannelId::BASAL_GANGLIA_NOGO, nogo_drive * dt);
        }

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "BasalGanglia";
        cfg.output_channel = NeuralChannelId::BASAL_GANGLIA_GO;
        cfg.base_output = 0.05f;
        cfg.max_output = 1.0f;
        cfg.threshold = 0.15f;  // meaningful threshold — avoids spurious actions
        cfg.excitatory_inputs = {
            {NeuralChannelId::PREFRONTAL_EXEC, 0.6f},
            {NeuralChannelId::REWARD_PREDICTION, 0.4f},
            {NeuralChannelId::MOTOR_TASK_DISPATCH, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::ANTERIOR_CINGULATE, 0.4f},
            {NeuralChannelId::AMYGDALA_AROUSAL, 0.3f},  // freeze response
            {NeuralChannelId::FATIGUE_SIGNAL, 0.2f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.3f;
        cfg.adaptation_rate = 0.002f;
        return cfg;
    }
};

} // namespace opencog::nerv
