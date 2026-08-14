#pragma once
/**
 * @file thyroid.hpp
 * @brief Thyroid gland — global processing rate governor
 *
 * Very slow dynamics (longest half-life). Acts as a global metabolic
 * rate multiplier that scales processing speed across all subsystems.
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class ThyroidGland : public SimpleGland {
public:
    explicit ThyroidGland(HormoneBus& bus, GlandConfig config = default_config())
        : SimpleGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Thyroid";
        cfg.output_channel = HormoneId::T3_T4;
        cfg.release_pattern = ReleasePattern::TONIC;
        cfg.base_production_rate = 0.05f;
        cfg.max_production_rate = 0.3f;
        cfg.homeostatic_setpoint = 0.5f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.3f;
        cfg.allostatic_rate = 0.0001f; // Extremely slow drift
        return cfg;
    }
};

} // namespace opencog::endo
