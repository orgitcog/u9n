#pragma once
/**
 * @file neuroendocrine_bridge.hpp
 * @brief Bidirectional VNS ↔ VES coupling at the hypothalamic bridge
 *
 * The NeuroEndocrineBridge couples the fast NerveBus (10ms, [-1,+1]) with
 * the slow HormoneBus (100ms, [0,1]). It accumulates neural signals over
 * a VES tick period and syncs at VES tick boundaries.
 *
 * VNS → VES (neural firing → hormone production):
 *   SYMPATHETIC_OUT       → CRH + NE (stress cascade)
 *   PARASYMPATHETIC_OUT   → serotonin + anandamide (calming)
 *   AMYGDALA_VALENCE neg  → cortisol burst
 *   REWARD_PREDICTION pos → DA_phasic
 *   PAIN_SIGNAL           → IL6 + cortisol
 *   SOCIAL_AFFERENT       → oxytocin
 *
 * VES → VNS (hormone concentration → neural baseline):
 *   Cortisol > 0.5    → RETICULAR_ACTIVATION increase
 *   Serotonin > 0.4   → PARASYMPATHETIC_OUT increase
 *   NE > 0.3          → THALAMIC_GATE sensitivity increase
 *   DA_tonic > 0.4    → BASAL_GANGLIA_GO threshold decrease
 *   T3/T4 > 0.5       → global neural decay rate decrease (faster processing)
 *
 * Tick rate bridging: VNS ticks at 10ms, VES at 100ms (10:1 ratio).
 * Bridge accumulates neural signals over a VES tick period.
 */

#include <opencog/nervous/nerve_bus.hpp>
#include <opencog/endocrine/hormone_bus.hpp>

#include <cmath>

namespace opencog::nerv {

class NeuroEndocrineBridge {
public:
    NeuroEndocrineBridge(NerveBus& nerve_bus, endo::HormoneBus& hormone_bus)
        : nerve_bus_(nerve_bus)
        , hormone_bus_(hormone_bus)
    {}

    /// Called every VNS tick (10ms) — accumulates neural signals
    void accumulate() {
        acc_sympathetic_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::SYMPATHETIC_OUT));
        acc_parasympathetic_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::PARASYMPATHETIC_OUT));
        acc_amygdala_neg_ += std::max(0.0f,
            -nerve_bus_.activation(NeuralChannelId::AMYGDALA_VALENCE));
        acc_reward_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::REWARD_PREDICTION));
        acc_pain_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::PAIN_SIGNAL));
        acc_social_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::SOCIAL_AFFERENT));
        acc_endocrine_nudge_ += std::max(0.0f,
            nerve_bus_.activation(NeuralChannelId::ENDOCRINE_NUDGE));

        ++accumulation_count_;
    }

    /// Called every VES tick (100ms) — flush accumulated signals to hormone bus
    /// and read hormones back into neural bus
    void sync() {
        if (accumulation_count_ == 0) return;

        float scale = 1.0f / static_cast<float>(accumulation_count_);

        // ============================================================
        // VNS → VES: Neural firing → Hormone production
        // ============================================================

        // Sympathetic → CRH + NE (stress cascade)
        float symp = acc_sympathetic_ * scale;
        if (symp > sympathetic_threshold_) {
            hormone_bus_.produce(endo::HormoneId::CRH, symp * 0.15f);
            hormone_bus_.produce(endo::HormoneId::NOREPINEPHRINE, symp * 0.1f);
        }

        // Parasympathetic → serotonin + anandamide (calming)
        float para = acc_parasympathetic_ * scale;
        if (para > parasympathetic_threshold_) {
            hormone_bus_.produce(endo::HormoneId::SEROTONIN, para * 0.1f);
            hormone_bus_.produce(endo::HormoneId::ANANDAMIDE, para * 0.08f);
        }

        // Amygdala negative valence → cortisol burst
        float amyg_neg = acc_amygdala_neg_ * scale;
        if (amyg_neg > amygdala_threshold_) {
            hormone_bus_.produce(endo::HormoneId::CORTISOL, amyg_neg * 0.12f);
        }

        // Reward prediction → DA_phasic
        float reward = acc_reward_ * scale;
        if (reward > reward_threshold_) {
            hormone_bus_.produce(endo::HormoneId::DOPAMINE_PHASIC, reward * 0.1f);
        }

        // Pain → IL6 + cortisol
        float pain = acc_pain_ * scale;
        if (pain > pain_threshold_) {
            hormone_bus_.produce(endo::HormoneId::IL6, pain * 0.08f);
            hormone_bus_.produce(endo::HormoneId::CORTISOL, pain * 0.06f);
        }

        // Social → oxytocin
        float social = acc_social_ * scale;
        if (social > social_threshold_) {
            hormone_bus_.produce(endo::HormoneId::OXYTOCIN, social * 0.1f);
        }

        // Endocrine nudge → direct hormone modulation (from hypothalamus/insula)
        float nudge = acc_endocrine_nudge_ * scale;
        if (nudge > 0.1f) {
            // Nudge broadly modulates calming hormones
            hormone_bus_.produce(endo::HormoneId::SEROTONIN, nudge * 0.05f);
        }

        // ============================================================
        // VES → VNS: Hormone concentration → Neural baseline
        // ============================================================

        float cortisol = hormone_bus_.concentration(endo::HormoneId::CORTISOL);
        float serotonin = hormone_bus_.concentration(endo::HormoneId::SEROTONIN);
        float ne = hormone_bus_.concentration(endo::HormoneId::NOREPINEPHRINE);
        float da_tonic = hormone_bus_.concentration(endo::HormoneId::DOPAMINE_TONIC);
        float thyroid = hormone_bus_.concentration(endo::HormoneId::T3_T4);
        float melatonin = hormone_bus_.concentration(endo::HormoneId::MELATONIN);

        // Cortisol > 0.5 → RETICULAR_ACTIVATION increase (hyperarousal)
        if (cortisol > 0.5f) {
            float excess = cortisol - 0.5f;
            nerve_bus_.fire(NeuralChannelId::RETICULAR_ACTIVATION, excess * 0.4f, 0.3f);
        }

        // Serotonin > 0.4 → PARASYMPATHETIC_OUT increase (calming)
        if (serotonin > 0.4f) {
            float excess = serotonin - 0.4f;
            nerve_bus_.fire(NeuralChannelId::PARASYMPATHETIC_OUT, excess * 0.3f, 0.2f);
        }

        // NE > 0.3 → THALAMIC_GATE sensitivity increase (alert)
        if (ne > 0.3f) {
            float excess = ne - 0.3f;
            nerve_bus_.fire(NeuralChannelId::THALAMIC_GATE, excess * 0.3f, 0.3f);
        }

        // DA_tonic > 0.4 → BASAL_GANGLIA_GO increase (facilitation)
        if (da_tonic > 0.4f) {
            float excess = da_tonic - 0.4f;
            nerve_bus_.fire(NeuralChannelId::BASAL_GANGLIA_GO, excess * 0.3f, 0.2f);
        }

        // Melatonin > 0.4 → FATIGUE_SIGNAL increase (sleepiness)
        if (melatonin > 0.4f) {
            float excess = melatonin - 0.4f;
            nerve_bus_.fire(NeuralChannelId::FATIGUE_SIGNAL, excess * 0.3f, 0.2f);
        }

        // T3/T4 > 0.5 → Homeostatic signal (metabolic activation)
        if (thyroid > 0.5f) {
            float excess = thyroid - 0.5f;
            nerve_bus_.fire(NeuralChannelId::ENERGY_STATE, excess * 0.4f, 0.2f);
        }

        // Also feed homeostatic summary to hypothalamic bridge
        float homeostatic_summary = (cortisol + ne + serotonin) / 3.0f;
        nerve_bus_.set_activation(NeuralChannelId::HOMEOSTATIC_SIGNAL,
                                  std::clamp(homeostatic_summary, -1.0f, 1.0f));

        // Reset accumulators
        reset_accumulators();
    }

    // === Configuration ===

    void set_sympathetic_threshold(float t) noexcept { sympathetic_threshold_ = t; }
    void set_parasympathetic_threshold(float t) noexcept { parasympathetic_threshold_ = t; }
    void set_amygdala_threshold(float t) noexcept { amygdala_threshold_ = t; }
    void set_reward_threshold(float t) noexcept { reward_threshold_ = t; }
    void set_pain_threshold(float t) noexcept { pain_threshold_ = t; }
    void set_social_threshold(float t) noexcept { social_threshold_ = t; }

    [[nodiscard]] size_t accumulation_count() const noexcept { return accumulation_count_; }

private:
    NerveBus& nerve_bus_;
    endo::HormoneBus& hormone_bus_;

    // Accumulators for VNS → VES signals
    float acc_sympathetic_{0.0f};
    float acc_parasympathetic_{0.0f};
    float acc_amygdala_neg_{0.0f};
    float acc_reward_{0.0f};
    float acc_pain_{0.0f};
    float acc_social_{0.0f};
    float acc_endocrine_nudge_{0.0f};
    size_t accumulation_count_{0};

    // Thresholds for VNS → VES signal transfer
    float sympathetic_threshold_{0.2f};
    float parasympathetic_threshold_{0.2f};
    float amygdala_threshold_{0.15f};
    float reward_threshold_{0.1f};
    float pain_threshold_{0.1f};
    float social_threshold_{0.1f};

    void reset_accumulators() noexcept {
        acc_sympathetic_ = 0.0f;
        acc_parasympathetic_ = 0.0f;
        acc_amygdala_neg_ = 0.0f;
        acc_reward_ = 0.0f;
        acc_pain_ = 0.0f;
        acc_social_ = 0.0f;
        acc_endocrine_nudge_ = 0.0f;
        accumulation_count_ = 0;
    }
};

} // namespace opencog::nerv
