#pragma once
/**
 * @file serotonergic.hpp
 * @brief Serotonergic system — mood baseline and patience/impulsivity tradeoff
 *
 * Slow dynamics, long half-life. Governs patience parameters and
 * modulates the impulsivity-deliberation axis.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class SerotonergicGland : public SimpleGland {
public:
    explicit SerotonergicGland(HormoneBus& bus, GlandConfig config = default_config())
        : SimpleGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Serotonergic";
        cfg.output_channel = HormoneId::SEROTONIN;
        cfg.release_pattern = ReleasePattern::TONIC;
        cfg.base_production_rate = 0.06f;
        cfg.max_production_rate = 0.4f;
        cfg.homeostatic_setpoint = 0.4f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.2f;
        cfg.allostatic_rate = 0.0002f; // Very slow drift
        // Cortisol inhibits serotonin
        cfg.inhibitors = {{HormoneId::CORTISOL, 0.3f}};
        // Oxytocin stimulates serotonin
        cfg.stimulators = {{HormoneId::OXYTOCIN, 0.2f}};
        return cfg;
    }
};

} // namespace opencog::endo
