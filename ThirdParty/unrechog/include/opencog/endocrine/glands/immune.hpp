#pragma once
/**
 * @file immune.hpp
 * @brief Immune monitor — cytokine system health signaling
 *
 * Monitors error rates, inconsistency accumulation, and processing latency.
 * Elevated cytokines trigger "sickness behavior": reduced exploration,
 * resource conservation, prioritized self-repair.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class ImmuneMonitor : public VirtualGland {
public:
    explicit ImmuneMonitor(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "ImmuneMonitor";
        cfg.output_channel = HormoneId::IL6;
        cfg.release_pattern = ReleasePattern::EVENT_DRIVEN;
        cfg.base_production_rate = 0.02f;
        cfg.max_production_rate = 0.6f;
        cfg.homeostatic_setpoint = 0.05f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.2f;
        // Cortisol suppresses immune (immunosuppression under stress)
        cfg.inhibitors = {{HormoneId::CORTISOL, 0.4f}};
        return cfg;
    }

    /// Signal a system error (exception, inconsistency, unexpected state)
    void signal_error(float severity) {
        error_accumulator_ += std::clamp(severity, 0.0f, 1.0f);
    }

    /// Signal detected inconsistency in knowledge base
    void signal_inconsistency(float degree) {
        inconsistency_accumulator_ += std::clamp(degree, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float health_signal = error_accumulator_ * 0.3f + inconsistency_accumulator_ * 0.2f;
        float raw = config_.base_production_rate + stim - inhib + health_signal;
        return apply_feedback(std::max(0.0f, raw));
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);

        // Accumulators decay (system self-heals)
        error_accumulator_ *= 0.95f;
        inconsistency_accumulator_ *= 0.97f;

        update_allostasis(dt);
    }

private:
    float error_accumulator_{0.0f};
    float inconsistency_accumulator_{0.0f};
};

} // namespace opencog::endo
