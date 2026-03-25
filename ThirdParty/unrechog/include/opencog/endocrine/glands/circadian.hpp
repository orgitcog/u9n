#pragma once
/**
 * @file circadian.hpp
 * @brief Circadian / Pineal gland — melatonin and maintenance cycle scheduling
 *
 * Sinusoidal melatonin production based on internal clock phase.
 * Phase is entrainable by external signals (light).
 * High melatonin triggers maintenance mode (memory consolidation, pruning).
 */

#include <opencog/endocrine/gland.hpp>
#include <cmath>
#include <numbers>

namespace opencog::endo {

class CircadianGland : public VirtualGland {
public:
    explicit CircadianGland(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "Circadian";
        cfg.output_channel = HormoneId::MELATONIN;
        cfg.release_pattern = ReleasePattern::CIRCADIAN;
        cfg.base_production_rate = 0.1f;
        cfg.max_production_rate = 0.5f;
        cfg.homeostatic_setpoint = 0.0f;
        cfg.negative_feedback = false;
        return cfg;
    }

    /// Set the circadian phase directly [0, 1) where 0.5 = peak melatonin
    void set_phase(float phase) { phase_ = std::fmod(phase, 1.0f); }

    /// Get current phase
    [[nodiscard]] float phase() const noexcept { return phase_; }

    /// Light signal suppresses melatonin and entrains the clock
    void signal_light(float intensity) {
        light_input_ = std::clamp(intensity, 0.0f, 1.0f);
    }

    /// Set the period (in ticks) for one full circadian cycle
    void set_period(float ticks) { period_ = std::max(1.0f, ticks); }

    float compute_production_rate() const override {
        // Sinusoidal: peak at phase 0.5, trough at phase 0.0/1.0
        float melatonin_drive = 0.5f * (1.0f + std::sin(2.0f * std::numbers::pi_v<float> * (phase_ - 0.25f)));
        // Light suppresses melatonin
        melatonin_drive *= std::max(0.0f, 1.0f - light_input_);
        return melatonin_drive * config_.max_production_rate;
    }

    void update(float dt) override {
        // Advance phase
        phase_ += dt / period_;
        phase_ = std::fmod(phase_, 1.0f);

        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);

        // Light decays
        light_input_ *= 0.98f;
    }

private:
    float phase_{0.0f};      // [0, 1) circadian phase
    float period_{1000.0f};  // Ticks per full cycle
    float light_input_{0.0f};
};

} // namespace opencog::endo
