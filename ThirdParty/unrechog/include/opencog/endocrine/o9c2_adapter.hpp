#pragma once
/**
 * @file o9c2_adapter.hpp
 * @brief o9c2 Deep Tree Echo Endocrine Adapter — Bidirectional coupling
 *
 * Maps hormone concentrations to o9c2 persona hyperparameters (spectral
 * radius, input scaling, leak rate, membrane permeability) and feeds
 * emergence metrics back into the hormone bus. Also manages persona
 * transitions driven by cognitive mode changes with hysteresis.
 *
 * This is the most complex adapter because it bridges the endocrine
 * system's distributed chemical signaling to the o9c2's six-paradigm
 * computational architecture:
 *
 *   ESN dynamics      ← spectral_radius, input_scaling, leak_rate
 *   P-System membranes ← membrane_permeability
 *   Emotion theory     ← directly maps to valence/arousal (VES native)
 *   Persona selection  ← CognitiveMode → O9C2Persona mapping
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/o9c2_types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::endo {

/**
 * @brief Maps between hormone bus and o9c2 cognitive architecture
 *
 * READ path (apply_endocrine_modulation):
 *   Serotonin       → spectral_radius↑  (patience = deeper memory)
 *   Cortisol        → spectral_radius↓  (stress = shorter memory)
 *   Norepinephrine  → input_scaling↑    (vigilance = more sensitive)
 *   Dopamine_tonic  → input_scaling↑    (motivation = more receptive)
 *   Anandamide      → input_scaling↓    (dampening = less reactive)
 *   T3/T4           → leak_rate↑        (thyroid = faster dynamics)
 *   Melatonin       → leak_rate↓        (maintenance = slower dynamics)
 *   Oxytocin        → membrane_permeability↑ (trust = more open)
 *   Cortisol        → membrane_permeability↓ (stress = tighter boundaries)
 *
 * WRITE path (apply_feedback):
 *   Wisdom gain     → serotonin
 *   Coherence       → COG_COHERENCE channel (15)
 *   Complexity      → dopamine_tonic
 *   Instability     → cortisol + norepinephrine
 *   Adaptability    → dopamine_phasic
 *
 * PERSONA TRANSITIONS:
 *   CognitiveMode persists N ticks → trigger persona transition
 *   REFLECTIVE    → Contemplative Scholar
 *   EXPLORATORY   → Dynamic Explorer
 *   FOCUSED/VIGILANT → Cautious Analyst
 *   REWARD        → Creative Visionary
 */
class O9C2EndocrineAdapter : public EndocrineConnector {
public:
    O9C2EndocrineAdapter(HormoneBus& bus, O9C2Interface& o9c2)
        : EndocrineConnector(bus)
        , o9c2_(o9c2)
    {
        base_config_ = o9c2_.current_config();
        prev_emergence_ = o9c2_.emergence();
        current_mode_ = bus.current_mode();
        mode_persist_count_ = 0;
    }

    // === Read path: Hormones → o9c2 hyperparameter modulation ===

    void apply_endocrine_modulation(const HormoneBus& bus) {
        O9C2PersonaConfig cfg = base_config_;

        float serotonin  = bus.concentration(HormoneId::SEROTONIN);
        float cortisol   = bus.concentration(HormoneId::CORTISOL);
        float ne         = bus.concentration(HormoneId::NOREPINEPHRINE);
        float da_tonic   = bus.concentration(HormoneId::DOPAMINE_TONIC);
        float anandamide = bus.concentration(HormoneId::ANANDAMIDE);
        float thyroid    = bus.concentration(HormoneId::T3_T4);
        float melatonin  = bus.concentration(HormoneId::MELATONIN);
        float oxytocin   = bus.concentration(HormoneId::OXYTOCIN);

        // --- Spectral Radius: memory depth ---
        // Serotonin (patience) increases, cortisol (stress) decreases
        cfg.spectral_radius = base_config_.spectral_radius
            + serotonin * 0.1f
            - cortisol * 0.15f;
        cfg.spectral_radius = std::clamp(cfg.spectral_radius, 0.5f, 0.99f);

        // --- Input Scaling: sensitivity ---
        // NE (vigilance) and DA_tonic (motivation) increase
        // Anandamide (dampening) decreases
        cfg.input_scaling = base_config_.input_scaling
            + ne * 0.3f
            + da_tonic * 0.2f
            - anandamide * 0.3f;
        cfg.input_scaling = std::clamp(cfg.input_scaling, 0.1f, 0.9f);

        // --- Leak Rate: dynamics speed ---
        // T3/T4 (thyroid) increases processing speed
        // Melatonin (maintenance) slows dynamics
        cfg.leak_rate = base_config_.leak_rate
            + thyroid * 0.4f
            - melatonin * 0.5f;
        cfg.leak_rate = std::clamp(cfg.leak_rate, 0.1f, 0.9f);

        // --- Membrane Permeability: boundary openness ---
        // Oxytocin (trust) opens boundaries
        // Anandamide (noise reduction) allows openness
        // Cortisol (stress) tightens boundaries
        cfg.membrane_permeability = base_config_.membrane_permeability
            + oxytocin * 0.4f
            + anandamide * 0.2f
            - cortisol * 0.3f;
        cfg.membrane_permeability = std::clamp(cfg.membrane_permeability, 0.1f, 0.9f);

        // Maintain current persona tag
        cfg.active_persona = o9c2_.active_persona();

        o9c2_.apply_config(cfg);

        // Check for persona transition
        update_persona_transition(bus.current_mode());
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    // === Write path: o9c2 emergence → Hormone feedback ===

    void apply_feedback() {
        EmergenceMetrics em = o9c2_.emergence();

        // --- Coherence → COG_COHERENCE channel (15) ---
        bus_.produce(HormoneId::COG_COHERENCE, em.coherence * 0.1f);

        // --- Wisdom gain (delta) → serotonin ---
        float delta_wisdom = em.wisdom - prev_emergence_.wisdom;
        if (delta_wisdom > wisdom_gain_threshold_) {
            bus_.produce(HormoneId::SEROTONIN, 0.1f);
        }

        // --- Complexity increase → dopamine_tonic ---
        float delta_complexity = em.complexity - prev_emergence_.complexity;
        if (delta_complexity > 0.05f) {
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.05f);
        }

        // --- Instability → cortisol + NE ---
        if (em.stability < stability_floor_) {
            float instability = stability_floor_ - em.stability;
            bus_.produce(HormoneId::CORTISOL, instability * 0.3f);
            bus_.produce(HormoneId::NOREPINEPHRINE, instability * 0.3f);
        }

        // --- Adaptability spike → dopamine_phasic ---
        float delta_adapt = em.adaptability - prev_emergence_.adaptability;
        if (delta_adapt > 0.1f) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.05f);
        }

        // --- Coherence drop detection ---
        float delta_coherence = em.coherence - prev_emergence_.coherence;
        if (delta_coherence < -0.1f) {
            // Significant coherence drop — raise alarm
            bus_.produce(HormoneId::CORTISOL, 0.1f);
        }

        prev_emergence_ = em;
    }

    // === Configuration ===

    /// Set number of ticks a mode must persist before triggering persona transition
    void set_hysteresis_ticks(size_t n) noexcept { hysteresis_ticks_ = n; }

    /// Set wisdom gain threshold for serotonin feedback
    void set_wisdom_threshold(float t) noexcept { wisdom_gain_threshold_ = t; }

    /// Set stability floor below which instability feedback activates
    void set_stability_floor(float f) noexcept { stability_floor_ = f; }

    /// Manually suggest a persona transition (e.g., from guidance)
    void suggest_transition(O9C2Persona persona) {
        o9c2_.transition_persona(persona);
        base_config_ = PERSONA_PRESETS[static_cast<size_t>(persona)];
    }

    /// Access the o9c2 interface
    [[nodiscard]] O9C2Interface& o9c2() noexcept { return o9c2_; }
    [[nodiscard]] const O9C2Interface& o9c2() const noexcept { return o9c2_; }

    /// Get the base (unmodulated) configuration
    [[nodiscard]] const O9C2PersonaConfig& base_config() const noexcept {
        return base_config_;
    }

private:
    O9C2Interface& o9c2_;
    O9C2PersonaConfig base_config_;
    EmergenceMetrics prev_emergence_;

    // Persona transition hysteresis
    CognitiveMode current_mode_{CognitiveMode::RESTING};
    size_t mode_persist_count_{0};
    size_t hysteresis_ticks_{5};  ///< Ticks a mode must persist before transition

    // Feedback thresholds
    float wisdom_gain_threshold_{0.05f};
    float stability_floor_{0.3f};

    /**
     * @brief Check if cognitive mode has persisted long enough to trigger
     *        a persona transition
     */
    void update_persona_transition(CognitiveMode mode) {
        if (mode == current_mode_) {
            ++mode_persist_count_;
        } else {
            current_mode_ = mode;
            mode_persist_count_ = 1;
        }

        if (mode_persist_count_ < hysteresis_ticks_) {
            return;  // Not yet stable enough
        }

        O9C2Persona current = o9c2_.active_persona();
        O9C2Persona suggested = mode_to_persona(mode);

        if (suggested != current && suggested != O9C2Persona::CONTEMPLATIVE_SCHOLAR
            || (suggested == O9C2Persona::CONTEMPLATIVE_SCHOLAR
                && mode == CognitiveMode::REFLECTIVE)) {
            // Transition only if there's a definite mapping
            // (RESTING/MAINTENANCE keep current persona)
            if (has_definite_mapping(mode)) {
                o9c2_.transition_persona(suggested);
                base_config_ = PERSONA_PRESETS[static_cast<size_t>(suggested)];
                mode_persist_count_ = 0;  // Reset after transition
            }
        }
    }

    /**
     * @brief Map cognitive mode to suggested persona
     */
    [[nodiscard]] static O9C2Persona mode_to_persona(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::REFLECTIVE:  return O9C2Persona::CONTEMPLATIVE_SCHOLAR;
        case CognitiveMode::EXPLORATORY: return O9C2Persona::DYNAMIC_EXPLORER;
        case CognitiveMode::FOCUSED:     return O9C2Persona::CAUTIOUS_ANALYST;
        case CognitiveMode::VIGILANT:    return O9C2Persona::CAUTIOUS_ANALYST;
        case CognitiveMode::STRESSED:    return O9C2Persona::CAUTIOUS_ANALYST;
        case CognitiveMode::THREAT:      return O9C2Persona::CAUTIOUS_ANALYST;
        case CognitiveMode::REWARD:      return O9C2Persona::CREATIVE_VISIONARY;
        case CognitiveMode::SOCIAL:      return O9C2Persona::CONTEMPLATIVE_SCHOLAR;
        default:                         return O9C2Persona::CONTEMPLATIVE_SCHOLAR;
        }
    }

    /**
     * @brief Whether a cognitive mode has a definite persona mapping
     *        (RESTING and MAINTENANCE don't drive transitions)
     */
    [[nodiscard]] static bool has_definite_mapping(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::RESTING:
        case CognitiveMode::MAINTENANCE:
            return false;
        default:
            return true;
        }
    }
};

} // namespace opencog::endo
