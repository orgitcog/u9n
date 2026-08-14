#pragma once
/**
 * @file subcortical_adapter.hpp
 * @brief SubcorticalO9C2Adapter — Bidirectional coupling to o9c2 Deep Tree Echo
 *
 * READ: AMYGDALA_VALENCE → spectral_radius, emotional modulation
 * WRITE: Emergence metrics → NOVELTY_SIGNAL, coherence → COHERENCE_SIGNAL
 *
 * Uses the abstract O9C2Interface from the endocrine system for decoupling.
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/endocrine/o9c2_types.hpp>

namespace opencog::nerv {

class SubcorticalO9C2Adapter : public NeuralConnector {
public:
    SubcorticalO9C2Adapter(NerveBus& bus, endo::O9C2Interface& o9c2)
        : NeuralConnector(bus)
        , o9c2_(o9c2)
    {
        base_config_ = o9c2_.current_config();
        prev_valence_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        endo::O9C2PersonaConfig cfg = base_config_;

        float valence = bus.activation(NeuralChannelId::AMYGDALA_VALENCE);
        float arousal = bus.activation(NeuralChannelId::AMYGDALA_AROUSAL);
        float novelty = bus.activation(NeuralChannelId::NOVELTY_SIGNAL);
        float persona_shift = bus.activation(NeuralChannelId::MOTOR_PERSONA_SHIFT);

        // Amygdala valence → spectral radius (emotional modulation of dynamics)
        float valence_delta = delta(valence, prev_valence_);
        cfg.spectral_radius = std::clamp(
            base_config_.spectral_radius + valence * 0.2f + valence_delta * 0.1f,
            0.1f, 1.2f);

        // High arousal → higher input scaling (more responsive to stimuli)
        cfg.input_scaling = std::clamp(
            base_config_.input_scaling + std::abs(arousal) * 0.3f,
            0.01f, 2.0f);

        // Novelty → leak rate modulation (novel input = slower leak = more processing)
        if (std::abs(novelty) > 0.2f) {
            cfg.leak_rate = std::clamp(
                base_config_.leak_rate - std::abs(novelty) * 0.2f,
                0.01f, 1.0f);
        }

        // Persona shift command → trigger persona transition
        if (std::abs(persona_shift) > 0.5f) {
            // Determine target persona from overall neural context
            auto target = determine_persona(bus);
            if (target != o9c2_.active_persona()) {
                o9c2_.transition_persona(target);
            }
        }

        o9c2_.apply_config(cfg);
        prev_valence_ = valence;
    }

    void write_feedback() override {
        auto emergence = o9c2_.emergence();

        // Emergence coherence → COHERENCE_SIGNAL
        float coherence = emergence.aggregate();
        if (std::abs(coherence - prev_coherence_) > 0.03f) {
            bus_.fire(NeuralChannelId::COHERENCE_SIGNAL, coherence * 0.5f, 0.3f);
        }

        // Emergence spike → NOVELTY_SIGNAL
        float novelty_delta = coherence - prev_coherence_;
        if (novelty_delta > 0.1f) {
            bus_.fire(NeuralChannelId::NOVELTY_SIGNAL, novelty_delta * 0.8f, 0.7f);
        }

        prev_coherence_ = coherence;
    }

private:
    endo::O9C2Interface& o9c2_;
    endo::O9C2PersonaConfig base_config_;
    float prev_valence_{0.0f};
    float prev_coherence_{0.0f};

    /// Determine target persona from overall neural activation pattern
    [[nodiscard]] endo::O9C2Persona determine_persona(const NerveBus& bus) const {
        float reasoning = std::abs(bus.activation(NeuralChannelId::REASONING_CORTEX));
        float novelty = std::abs(bus.activation(NeuralChannelId::NOVELTY_SIGNAL));
        float threat = std::abs(bus.activation(NeuralChannelId::THREAT_AFFERENT));
        float reward = std::abs(bus.activation(NeuralChannelId::REWARD_PREDICTION));

        // Highest signal wins
        float max_val = reasoning;
        auto best = endo::O9C2Persona::CONTEMPLATIVE_SCHOLAR;

        if (novelty > max_val) { max_val = novelty; best = endo::O9C2Persona::DYNAMIC_EXPLORER; }
        if (threat > max_val)  { max_val = threat;  best = endo::O9C2Persona::CAUTIOUS_ANALYST; }
        if (reward > max_val)  { best = endo::O9C2Persona::CREATIVE_VISIONARY; }

        return best;
    }
};

} // namespace opencog::nerv
