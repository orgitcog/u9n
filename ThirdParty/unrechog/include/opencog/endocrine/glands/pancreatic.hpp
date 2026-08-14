#pragma once
/**
 * @file pancreatic.hpp
 * @brief Pancreatic gland — insulin/glucagon energy allocation
 *
 * Tracks computational resource usage. High demand → glucagon (mobilize).
 * Satiation/low demand → insulin (store/conserve).
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class PancreaticGland : public VirtualGland {
public:
    explicit PancreaticGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Pancreatic";
        cfg.output_channel = HormoneId::INSULIN;
        cfg.release_pattern = ReleasePattern::TONIC;
        cfg.base_production_rate = 0.05f;
        cfg.max_production_rate = 0.5f;
        cfg.homeostatic_setpoint = 0.2f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.3f;
        return cfg;
    }

    /// Signal current resource demand [0, 1]
    void signal_resource_demand(float demand) {
        resource_demand_ = std::clamp(demand, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        // High demand → glucagon dominates, low demand → insulin dominates
        // This gland manages both channels antagonistically
        return config_.base_production_rate;
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();

        // Insulin: produced when demand is LOW (conservation)
        float insulin_rate = config_.base_production_rate * (1.0f - resource_demand_);
        bus_.produce(HormoneId::INSULIN, insulin_rate * dt);

        // Glucagon: produced when demand is HIGH (mobilization)
        float glucagon_rate = config_.base_production_rate * resource_demand_ * 1.5f;
        bus_.produce(HormoneId::GLUCAGON, glucagon_rate * dt);

        resource_demand_ *= 0.95f;
        update_allostasis(dt);
    }

private:
    float resource_demand_{0.3f}; // Moderate baseline demand
};

} // namespace opencog::endo
