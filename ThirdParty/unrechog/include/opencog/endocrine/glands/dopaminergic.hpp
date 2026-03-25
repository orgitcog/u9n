#pragma once
/**
 * @file dopaminergic.hpp
 * @brief Dopaminergic system — tonic motivation + phasic reward signals
 *
 * Tonic dopamine: sustained motivation baseline, cognitive flexibility.
 * Phasic dopamine: event-driven bursts on reward prediction error.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class DopaminergicGland : public VirtualGland {
public:
    explicit DopaminergicGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Dopaminergic";
        cfg.output_channel = HormoneId::DOPAMINE_TONIC;
        cfg.release_pattern = ReleasePattern::TONIC;
        cfg.base_production_rate = 0.08f;
        cfg.max_production_rate = 0.6f;
        cfg.homeostatic_setpoint = 0.3f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.3f;
        // Cortisol suppresses dopamine
        cfg.inhibitors = {{HormoneId::CORTISOL, 0.4f}};
        // Serotonin modulates
        cfg.stimulators = {{HormoneId::SEROTONIN, 0.1f}};
        return cfg;
    }

    /// Signal a reward prediction error (positive = better than expected)
    void signal_reward(float prediction_error) {
        phasic_burst_ = prediction_error;
    }

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float raw = config_.base_production_rate + stim - inhib;
        return apply_feedback(std::max(0.0f, raw));
    }

    void update(float dt) override {
        // Tonic component
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);

        // Phasic component (separate channel)
        if (std::abs(phasic_burst_) > 0.01f) {
            float phasic_amount = std::clamp(phasic_burst_, -0.5f, 0.5f);
            if (phasic_amount > 0.0f) {
                bus_.produce(HormoneId::DOPAMINE_PHASIC, phasic_amount);
            }
            // Negative prediction error: dip (reduce tonic briefly)
            if (phasic_amount < 0.0f) {
                float current = bus_.concentration(HormoneId::DOPAMINE_TONIC);
                bus_.set_concentration(HormoneId::DOPAMINE_TONIC,
                    std::max(0.0f, current + phasic_amount * 0.3f));
            }
            phasic_burst_ *= 0.5f; // Rapid decay of phasic signal
        }

        update_allostasis(dt);
    }

private:
    float phasic_burst_{0.0f};
};

} // namespace opencog::endo
