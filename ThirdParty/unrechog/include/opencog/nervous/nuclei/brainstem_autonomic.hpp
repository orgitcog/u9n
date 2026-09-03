#pragma once
/**
 * @file brainstem_autonomic.hpp
 * @brief Brainstem Autonomic Nucleus — Autonomic regulation and arousal
 *
 * The brainstem autonomic nucleus manages the sympathetic/parasympathetic
 * balance and drives global arousal via the reticular activating system.
 * It is the primary interface to VES glands (HPA axis etc.).
 *
 * Cosmos S5: Root center — survival and autonomic regulation
 *
 * Output: BRAINSTEM_AUTONOMIC → autonomic nervous system output
 * Excitatory: PAIN_SIGNAL, HOMEOSTATIC_SIGNAL, AROUSAL_SIGNAL
 * Inhibitory: PARASYMPATHETIC_OUT, PREFRONTAL_EXEC (top-down calming)
 */

#include <opencog/nervous/nucleus.hpp>

namespace opencog::nerv {

class BrainstemAutonomicNucleus : public NeuralNucleus {
public:
    explicit BrainstemAutonomicNucleus(NerveBus& bus)
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

        // Brainstem drives reticular activating system
        float output_level = std::abs(current_output_);
        if (output_level > 0.2f) {
            bus_.fire(NeuralChannelId::RETICULAR_ACTIVATION, output_level * 0.4f * dt);
        }

        // High brainstem output → sympathetic/parasympathetic balance
        float sympathetic = bus_.activation(NeuralChannelId::SYMPATHETIC_OUT);
        float parasympathetic = bus_.activation(NeuralChannelId::PARASYMPATHETIC_OUT);
        float balance = sympathetic - parasympathetic;
        // Drive arousal signal reflecting autonomic balance
        bus_.set_activation(NeuralChannelId::AROUSAL_SIGNAL,
                            std::clamp(balance, -1.0f, 1.0f));

        update_adaptation(dt);
    }

private:
    static NucleusConfig make_config() {
        NucleusConfig cfg;
        cfg.name = "BrainstemAutonomic";
        cfg.output_channel = NeuralChannelId::BRAINSTEM_AUTONOMIC;
        cfg.base_output = 0.2f;  // always active (maintains vital functions)
        cfg.max_output = 1.0f;
        cfg.threshold = 0.05f;   // low threshold — always responsive
        cfg.excitatory_inputs = {
            {NeuralChannelId::PAIN_SIGNAL, 0.6f},
            {NeuralChannelId::HOMEOSTATIC_SIGNAL, 0.4f},
            {NeuralChannelId::AROUSAL_SIGNAL, 0.3f},
            {NeuralChannelId::ENERGY_STATE, 0.2f},
        };
        cfg.inhibitory_inputs = {
            {NeuralChannelId::PARASYMPATHETIC_OUT, 0.4f},
            {NeuralChannelId::PREFRONTAL_EXEC, 0.2f},
        };
        cfg.self_inhibition = true;
        cfg.self_inhibition_gain = 0.2f;
        cfg.adaptation_rate = 0.001f;  // very slow — maintains stability
        return cfg;
    }
};

} // namespace opencog::nerv
