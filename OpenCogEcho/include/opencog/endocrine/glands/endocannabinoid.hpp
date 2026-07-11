#pragma once
/**
 * @file endocannabinoid.hpp
 * @brief Endocannabinoid system — noise reduction and homeostatic dampening
 *
 * Demand-driven (on-demand synthesis). Activated by excessive noise or
 * high-frequency oscillation. Dampens cortisol and norepinephrine sensitivity.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class EndocannabinoidGland : public VirtualGland {
public:
    explicit EndocannabinoidGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Endocannabinoid";
        cfg.output_channel = HormoneId::ANANDAMIDE;
        cfg.release_pattern = ReleasePattern::EVENT_DRIVEN;
        cfg.base_production_rate = 0.02f;
        cfg.max_production_rate = 0.4f;
        cfg.homeostatic_setpoint = 0.1f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.3f;
        // Activated when cortisol and norepinephrine are high (homeostatic response)
        cfg.stimulators = {
            {HormoneId::CORTISOL, 0.2f},
            {HormoneId::NOREPINEPHRINE, 0.2f}
        };
        return cfg;
    }

    /// Signal excessive noise in processing
    void signal_noise(float level) {
        noise_input_ = std::clamp(level, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float raw = config_.base_production_rate + stim - inhib + noise_input_ * 0.3f;
        return apply_feedback(std::max(0.0f, raw));
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);
        noise_input_ *= 0.9f;
        update_allostasis(dt);
    }

private:
    float noise_input_{0.0f};
};

} // namespace opencog::endo
