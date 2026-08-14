#pragma once
/**
 * @file noradrenergic.hpp
 * @brief Noradrenergic system — arousal, vigilance, and attention modulation
 *
 * Stimulated by CRH and novelty signals. Tonic baseline with event-driven spikes.
 * Directly modulates ECAN attentional focus boundary.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class NoradrenergicGland : public VirtualGland {
public:
    explicit NoradrenergicGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Noradrenergic";
        cfg.output_channel = HormoneId::NOREPINEPHRINE;
        cfg.release_pattern = ReleasePattern::EVENT_DRIVEN;
        cfg.base_production_rate = 0.04f;
        cfg.max_production_rate = 0.7f;
        cfg.homeostatic_setpoint = 0.1f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.4f;
        // CRH stimulates norepinephrine (stress -> arousal)
        cfg.stimulators = {{HormoneId::CRH, 0.5f}};
        return cfg;
    }

    /// Signal novelty detection
    void signal_novelty(float intensity) {
        novelty_input_ = std::clamp(intensity, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float raw = config_.base_production_rate + stim - inhib + novelty_input_ * 0.3f;
        return apply_feedback(std::max(0.0f, raw));
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);
        novelty_input_ *= 0.9f; // Novelty habituates
        update_allostasis(dt);
    }

private:
    float novelty_input_{0.0f};
};

} // namespace opencog::endo
