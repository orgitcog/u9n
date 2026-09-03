#pragma once
/**
 * @file hpa_axis.hpp
 * @brief Hypothalamic-Pituitary-Adrenal axis — three-stage stress cascade
 *
 * CRH (hypothalamus) -> ACTH (pituitary) -> Cortisol (adrenal)
 * with negative feedback: Cortisol inhibits CRH (long loop) and ACTH (short loop).
 */

#include <opencog/endocrine/gland.hpp>

namespace opencog::endo {

class HPAAxis : public VirtualGland {
public:
    explicit HPAAxis(HormoneBus& bus, GlandConfig config = default_config())
        : VirtualGland(bus, std::move(config))
    {}

    static GlandConfig default_config() {
        GlandConfig cfg;
        cfg.name = "HPA_Axis";
        cfg.output_channel = HormoneId::CORTISOL;
        cfg.release_pattern = ReleasePattern::PULSATILE;
        cfg.base_production_rate = 0.05f;
        cfg.max_production_rate = 0.8f;
        cfg.homeostatic_setpoint = 0.15f;
        cfg.negative_feedback = true;
        cfg.feedback_gain = 0.6f;
        cfg.allostatic_rate = 0.0005f;
        return cfg;
    }

    /// Signal perceived threat/uncertainty to the HPA axis
    void signal_stress(float intensity) {
        stress_input_ = std::clamp(intensity, 0.0f, 1.0f);
    }

    float compute_production_rate() const override {
        // Stage 1: CRH production based on stress input
        float crh_drive = stress_input_ * 0.5f;
        // Long-loop negative feedback: cortisol suppresses CRH
        float cortisol = bus_.concentration(HormoneId::CORTISOL);
        crh_drive *= std::max(0.0f, 1.0f - cortisol * config_.feedback_gain);

        // Stage 2: ACTH amplifies CRH
        float current_crh = bus_.concentration(HormoneId::CRH);
        float acth_drive = current_crh * 2.0f;
        // Short-loop negative feedback: cortisol suppresses ACTH
        acth_drive *= std::max(0.0f, 1.0f - cortisol * 0.4f);

        // Stage 3: Cortisol production from ACTH
        float current_acth = bus_.concentration(HormoneId::ACTH);
        float cortisol_rate = config_.base_production_rate + current_acth * 0.3f;

        // Store intermediate rates for cascade emission
        crh_rate_ = crh_drive;
        acth_rate_ = acth_drive;

        return std::clamp(cortisol_rate, 0.0f, config_.max_production_rate);
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();

        // Emit all three stages
        bus_.produce(HormoneId::CRH, crh_rate_ * dt);
        bus_.produce(HormoneId::ACTH, acth_rate_ * dt);
        emit(current_rate_ * dt);  // Cortisol

        // Decay stress input (transient signal)
        stress_input_ *= 0.95f;

        update_allostasis(dt);
    }

private:
    float stress_input_{0.0f};
    mutable float crh_rate_{0.0f};
    mutable float acth_rate_{0.0f};
};

} // namespace opencog::endo
