#pragma once
/**
 * @file touchpad_adapter.hpp
 * @brief VirtualTouchpad Endocrine Adapter — Bidirectional coupling
 *
 * Maps hormone concentrations to VirtualTouchpad manifold parameters
 * (sensitivity, dimensionality, curvature, etc.) and feeds gesture metrics
 * back into the hormone bus. Also manages operational mode transitions
 * driven by cognitive mode changes with hysteresis.
 *
 * The VirtualTouchpad is a cognitive gesture manifold that complements
 * the hemispheric architecture:
 *   o9c2 (Right Hemisphere):  novelty, emergence, multi-scale dynamics
 *   Marduk (Left Hemisphere): structure, task management, meta-optimization
 *   Touchpad (Interface):     gesture intent, manifold topology, field dynamics
 *
 * CROSS-MODULE COUPLING:
 *   Touchpad reads ORG_COHERENCE (ch17, written by Marduk) → coherence_requirement
 *   Touchpad writes TOUCHPAD_LOAD (ch18) ← contact intensity
 *   Touchpad writes TOUCHPAD_COHERENCE (ch19) ← manifold coherence
 *
 * READ path (apply_endocrine_modulation):
 *   Cortisol        → pressure_threshold↑  (stress = raise input bar)
 *   Cortisol        → sensitivity↓          (stress = narrow receptivity)
 *   Serotonin       → sensitivity↑          (patience = open sensing)
 *   Serotonin       → coherence_requirement↑(patience = demand cleaner signals)
 *   DA_tonic        → gesture_complexity↑   (motivation = enable complex gestures)
 *   DA_phasic       → manifold_curvature↑   (reward = amplify nonlinear geometry)
 *   NE              → temporal_resolution↑  (vigilance = sharpen time perception)
 *   NE              → velocity_damping↓     (vigilance = reduce damping)
 *   Melatonin       → velocity_damping↑     (maintenance = slow responses)
 *   Anandamide      → pressure_threshold↓   (dampening = relax input bar)
 *   Oxytocin        → dimensionality_scale↑ (trust = open more dimensions)
 *   T3/T4           → temporal_resolution↑  (thyroid = processing speed)
 *   ORG_COHERENCE   → coherence_requirement↑(CROSS-MODULE: Marduk coherence → quality bar)
 *
 * WRITE path (apply_feedback):
 *   active_contact_count        → TOUCHPAD_LOAD (ch18)
 *   manifold_coherence          → TOUCHPAD_COHERENCE (ch19)
 *   gesture_confidence Δ>0.05   → serotonin
 *   gestures_recognized++       → DA_phasic
 *   error_rate > 0.5            → IL6 + cortisol
 *   pattern_novelty > 0.7       → DA_tonic + NE
 *   field_energy drop < -0.15   → cortisol
 *   coherence drop < -0.1       → NE
 *   precision drop < -0.1       → cortisol
 *
 * MODE TRANSITIONS:
 *   CognitiveMode persists N ticks → trigger mode transition
 *   REFLECTIVE/SOCIAL           → RECEPTIVE (open sensing)
 *   EXPLORATORY/REWARD          → EXPRESSIVE (output/broadcast)
 *   FOCUSED/VIGILANT/STRESSED/THREAT → GUARDED (filter input)
 *   MAINTENANCE                 → CALIBRATING (self-tune)
 *   RESTING                     → (no transition)
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/touchpad_types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::endo {

/**
 * @brief Maps between hormone bus and VirtualTouchpad gesture manifold
 */
class TouchpadEndocrineAdapter : public EndocrineConnector {
public:
    TouchpadEndocrineAdapter(HormoneBus& bus, TouchpadInterface& touchpad)
        : EndocrineConnector(bus)
        , touchpad_(touchpad)
    {
        base_config_ = touchpad_.current_config();
        prev_telemetry_ = touchpad_.telemetry();
        prev_metrics_ = touchpad_.gesture_metrics();
        current_mode_ = bus.current_mode();
        mode_persist_count_ = 0;
    }

    // ========================================================================
    // Read path: Hormones → Touchpad manifold parameter modulation
    // ========================================================================

    void apply_endocrine_modulation(const HormoneBus& bus) {
        TouchpadEndocrineConfig cfg = base_config_;

        float cortisol      = bus.concentration(HormoneId::CORTISOL);
        float da_tonic      = bus.concentration(HormoneId::DOPAMINE_TONIC);
        float da_phasic     = bus.concentration(HormoneId::DOPAMINE_PHASIC);
        float serotonin     = bus.concentration(HormoneId::SEROTONIN);
        float ne            = bus.concentration(HormoneId::NOREPINEPHRINE);
        float melatonin     = bus.concentration(HormoneId::MELATONIN);
        float anandamide    = bus.concentration(HormoneId::ANANDAMIDE);
        float oxytocin      = bus.concentration(HormoneId::OXYTOCIN);
        float thyroid       = bus.concentration(HormoneId::T3_T4);
        float org_coherence = bus.concentration(HormoneId::ORG_COHERENCE);

        // --- Sensitivity: overall input gain ---
        // Serotonin (patience) opens sensing, cortisol (stress) narrows
        cfg.sensitivity = base_config_.sensitivity
            + serotonin * 0.25f
            - cortisol * 0.2f;
        cfg.sensitivity = std::clamp(cfg.sensitivity, 0.0f, 1.0f);

        // --- Dimensionality Scale: effective manifold dimensions ---
        // Oxytocin (trust) opens more dimensions
        cfg.dimensionality_scale = base_config_.dimensionality_scale
            + oxytocin * 0.25f;
        cfg.dimensionality_scale = std::clamp(cfg.dimensionality_scale, 0.0f, 1.0f);

        // --- Gesture Complexity: max recognizable complexity ---
        // DA_tonic (motivation) enables complex gestures
        cfg.gesture_complexity = base_config_.gesture_complexity
            + da_tonic * 0.3f;
        cfg.gesture_complexity = std::clamp(cfg.gesture_complexity, 0.0f, 1.0f);

        // --- Temporal Resolution: time-granularity ---
        // NE (vigilance) sharpens, T3/T4 (thyroid) speeds up
        cfg.temporal_resolution = base_config_.temporal_resolution
            + ne * 0.3f
            + thyroid * 0.2f;
        cfg.temporal_resolution = std::clamp(cfg.temporal_resolution, 0.0f, 1.0f);

        // --- Pressure Threshold: minimum conviction ---
        // Cortisol (stress) raises bar, anandamide (dampening) relaxes
        cfg.pressure_threshold = base_config_.pressure_threshold
            + cortisol * 0.3f
            - anandamide * 0.2f;
        cfg.pressure_threshold = std::clamp(cfg.pressure_threshold, 0.0f, 1.0f);

        // --- Velocity Damping: rate-of-change damping ---
        // Melatonin (maintenance) slows, NE (vigilance) reduces damping
        cfg.velocity_damping = base_config_.velocity_damping
            + melatonin * 0.3f
            - ne * 0.2f;
        cfg.velocity_damping = std::clamp(cfg.velocity_damping, 0.0f, 1.0f);

        // --- Manifold Curvature: nonlinear geometry ---
        // DA_phasic (reward) amplifies curvature
        cfg.manifold_curvature = base_config_.manifold_curvature
            + da_phasic * 0.25f;
        cfg.manifold_curvature = std::clamp(cfg.manifold_curvature, 0.0f, 1.0f);

        // --- Coherence Requirement: minimum signal coherence ---
        // Serotonin (patience) demands cleaner signals;
        // ORG_COHERENCE (cross-module: Marduk) raises quality bar
        cfg.coherence_requirement = base_config_.coherence_requirement
            + serotonin * 0.15f
            + org_coherence * 0.15f;
        cfg.coherence_requirement = std::clamp(cfg.coherence_requirement, 0.0f, 1.0f);

        // Maintain current mode tag
        cfg.active_mode = touchpad_.active_mode();

        touchpad_.apply_config(cfg);

        // Check for operational mode transition
        update_mode_transition(bus.current_mode());
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    // ========================================================================
    // Write path: Touchpad metrics → Hormone feedback
    // ========================================================================

    void apply_feedback() {
        TouchpadTelemetry tel = touchpad_.telemetry();
        GestureMetrics met = touchpad_.gesture_metrics();

        // --- Active contact count → TOUCHPAD_LOAD channel (18) ---
        bus_.produce(HormoneId::TOUCHPAD_LOAD, tel.active_contact_count * 0.1f);

        // --- Manifold coherence → TOUCHPAD_COHERENCE channel (19) ---
        bus_.produce(HormoneId::TOUCHPAD_COHERENCE, tel.manifold_coherence * 0.1f);

        // --- Gesture confidence improvement (Δ > 0.05) → serotonin ---
        float delta_confidence = tel.gesture_confidence - prev_telemetry_.gesture_confidence;
        if (delta_confidence > confidence_threshold_) {
            bus_.produce(HormoneId::SEROTONIN, 0.04f);
        }

        // --- Gesture recognized (count++) → DA_phasic ---
        if (tel.gestures_recognized > prev_telemetry_.gestures_recognized) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.08f);
        }

        // --- High error rate (> 0.5) → IL6 + cortisol ---
        if (tel.error_rate > 0.5f) {
            bus_.produce(HormoneId::IL6, 0.05f);
            bus_.produce(HormoneId::CORTISOL, 0.03f);
        }

        // --- High pattern novelty (> 0.7) → DA_tonic + NE ---
        if (tel.pattern_novelty > 0.7f) {
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.06f);
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.04f);
        }

        // --- Field energy collapse (Δ < -0.15) → cortisol ---
        float delta_energy = tel.field_energy - prev_telemetry_.field_energy;
        if (delta_energy < -0.15f) {
            bus_.produce(HormoneId::CORTISOL, 0.03f);
        }

        // --- Coherence drop (Δ < -0.1) → NE ---
        float delta_coherence = met.coherence - prev_metrics_.coherence;
        if (delta_coherence < -0.1f) {
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
        }

        // --- Precision drop (Δ < -0.1) → cortisol ---
        float delta_precision = met.precision - prev_metrics_.precision;
        if (delta_precision < -0.1f) {
            bus_.produce(HormoneId::CORTISOL, 0.03f);
        }

        prev_telemetry_ = tel;
        prev_metrics_ = met;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    /// Set number of ticks a mode must persist before triggering mode transition
    void set_hysteresis_ticks(size_t n) noexcept { hysteresis_ticks_ = n; }

    /// Set confidence delta threshold for serotonin feedback
    void set_confidence_threshold(float t) noexcept { confidence_threshold_ = t; }

    /// Manually suggest a mode transition (e.g., from guidance)
    void suggest_transition(TouchpadMode mode) {
        touchpad_.transition_mode(mode);
        base_config_ = TOUCHPAD_MODE_PRESETS[static_cast<size_t>(mode)];
    }

    /// Access the Touchpad interface
    [[nodiscard]] TouchpadInterface& touchpad() noexcept { return touchpad_; }
    [[nodiscard]] const TouchpadInterface& touchpad() const noexcept { return touchpad_; }

    /// Get the base (unmodulated) configuration
    [[nodiscard]] const TouchpadEndocrineConfig& base_config() const noexcept {
        return base_config_;
    }

private:
    TouchpadInterface& touchpad_;
    TouchpadEndocrineConfig base_config_;
    TouchpadTelemetry prev_telemetry_;
    GestureMetrics prev_metrics_;

    // Operational mode transition hysteresis
    CognitiveMode current_mode_{CognitiveMode::RESTING};
    size_t mode_persist_count_{0};
    size_t hysteresis_ticks_{5};  ///< Ticks a mode must persist before transition

    // Feedback thresholds
    float confidence_threshold_{0.05f};

    /**
     * @brief Check if cognitive mode has persisted long enough to trigger
     *        an operational mode transition
     */
    void update_mode_transition(CognitiveMode mode) {
        if (mode == current_mode_) {
            ++mode_persist_count_;
        } else {
            current_mode_ = mode;
            mode_persist_count_ = 1;
        }

        if (mode_persist_count_ < hysteresis_ticks_) {
            return;  // Not yet stable enough
        }

        TouchpadMode current = touchpad_.active_mode();
        TouchpadMode suggested = mode_to_operational(mode);

        if (suggested != current && has_definite_mapping(mode)) {
            touchpad_.transition_mode(suggested);
            base_config_ = TOUCHPAD_MODE_PRESETS[static_cast<size_t>(suggested)];
            mode_persist_count_ = 0;  // Reset after transition
        }
    }

    /**
     * @brief Map cognitive mode to suggested Touchpad operational mode
     *
     * Complementary to o9c2 and Marduk persona mappings:
     *   REFLECTIVE/SOCIAL     → Scholar (o9c2)  / Knowledge Architect (Marduk) / RECEPTIVE (Touchpad)
     *   EXPLORATORY/REWARD    → Explorer (o9c2) / Blueprint Designer (Marduk)  / EXPRESSIVE (Touchpad)
     *   FOCUSED/VIGILANT/etc  → Analyst (o9c2)  / Verification Guardian (Marduk) / GUARDED (Touchpad)
     *   MAINTENANCE           → (maintenance)   / (maintenance)                / CALIBRATING (Touchpad)
     */
    [[nodiscard]] static TouchpadMode mode_to_operational(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::REFLECTIVE:  return TouchpadMode::RECEPTIVE;
        case CognitiveMode::SOCIAL:      return TouchpadMode::RECEPTIVE;
        case CognitiveMode::EXPLORATORY: return TouchpadMode::EXPRESSIVE;
        case CognitiveMode::REWARD:      return TouchpadMode::EXPRESSIVE;
        case CognitiveMode::FOCUSED:     return TouchpadMode::GUARDED;
        case CognitiveMode::VIGILANT:    return TouchpadMode::GUARDED;
        case CognitiveMode::STRESSED:    return TouchpadMode::GUARDED;
        case CognitiveMode::THREAT:      return TouchpadMode::GUARDED;
        case CognitiveMode::MAINTENANCE: return TouchpadMode::CALIBRATING;
        default:                         return TouchpadMode::RECEPTIVE;
        }
    }

    /**
     * @brief Whether a cognitive mode has a definite operational mode mapping
     *        (RESTING doesn't drive transitions — persist current mode)
     */
    [[nodiscard]] static bool has_definite_mapping(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::RESTING:
            return false;
        default:
            return true;
        }
    }
};

} // namespace opencog::endo
