#pragma once
/**
 * @file oxytocinergic.hpp
 * @brief Oxytocinergic system — trust, bonding, prosocial orientation
 *
 * Stimulated by social interaction signals. Features rare positive feedback
 * (self-amplifying during bonding events). Inhibited by cortisol.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class OxytocinergicGland : public VirtualGland {
public:
    explicit OxytocinergicGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Oxytocinergic";
        cfg.output_channel = HormoneId::OXYTOCIN;
        cfg.release_pattern = ReleasePattern::EVENT_DRIVEN;
        cfg.base_production_rate = 0.03f;
        cfg.max_production_rate = 0.6f;
        cfg.homeostatic_setpoint = 0.1f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.2f;
        // Positive feedback (self-amplifying during bonding)
        cfg.positive_feedback = true;
        cfg.positive_feedback_gain = 0.15f;
        // Cortisol inhibits oxytocin
        cfg.inhibitors = {{HormoneId::CORTISOL, 0.3f}};
        return cfg;
    }

    /// Signal social interaction (bonding, trust, cooperation)
    void signal_social(float intensity) {
        social_input_ = std::clamp(intensity, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float raw = config_.base_production_rate + stim - inhib + social_input_ * 0.4f;
        return apply_feedback(std::max(0.0f, raw));
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);
        social_input_ *= 0.85f; // Social signals decay moderately
        update_allostasis(dt);
    }

private:
    float social_input_{0.0f};
};

} // namespace opencog::endo
