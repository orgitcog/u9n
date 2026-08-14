#pragma once
/**
 * @file marduk_adapter.hpp
 * @brief Marduk Endocrine Adapter — Bidirectional coupling (Left Hemisphere)
 *
 * Maps hormone concentrations to Marduk operational parameters (task urgency,
 * memory retention, validation threshold, etc.) and feeds execution metrics
 * back into the hormone bus. Also manages operational mode transitions driven
 * by cognitive mode changes with hysteresis.
 *
 * Marduk is the Left Hemisphere complement to o9c2 (Right Hemisphere):
 *
 *   Memory subsystem     ← consolidation_depth, memory_retention
 *   Task subsystem       ← task_urgency, task_exploration
 *   AI subsystem         ← synthesis_intensity, integration_openness
 *   Autonomy subsystem   ← autonomy_rate, validation_threshold
 *   Mode selection       ← CognitiveMode → MardukOperationalMode mapping
 *
 * HEMISPHERIC COUPLING:
 *   Marduk reads COG_COHERENCE (ch15, written by o9c2) → validation_threshold
 *   Marduk writes ORG_COHERENCE (ch17) ← organization metric
 *   o9c2 can read ORG_COHERENCE (ch17) → membrane_permeability modulation
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/marduk_types.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::endo {

/**
 * @brief Maps between hormone bus and Marduk cognitive architecture
 *
 * READ path (apply_endocrine_modulation):
 *   Cortisol        → task_urgency↑        (stress = prioritize critical tasks)
 *   Cortisol        → consolidation_depth↓  (stress = shallow processing)
 *   DA_tonic        → task_exploration↑     (motivation = branch to novel tasks)
 *   DA_phasic       → consolidation_depth↑  (reward = encode strongly)
 *   DA_phasic       → synthesis_intensity↑  (reward = maximize structure)
 *   Serotonin       → memory_retention↑     (patience = thorough retention)
 *   Serotonin       → validation_threshold↑ (patience = higher quality bar)
 *   Norepinephrine  → task_exploration↓     (vigilance = narrow focus)
 *   Melatonin       → synthesis_intensity↑  (maintenance = consolidate)
 *   Anandamide      → task_urgency↓         (dampening = relax pressure)
 *   Oxytocin        → integration_openness↑ (trust = accept external input)
 *   T3/T4           → autonomy_rate↑        (thyroid = faster cycles)
 *   COG_COHERENCE   → validation_threshold↑ (HEMISPHERIC: o9c2 coherence → quality bar)
 *
 * WRITE path (apply_feedback):
 *   Task queue depth           → MARDUK_LOAD (ch16)
 *   Organization metric        → ORG_COHERENCE (ch17)
 *   Consolidation success (Δ)  → serotonin
 *   Optimization complete      → DA_phasic
 *   High cognitive load        → IL6
 *   LightFace burst (edge)     → DA_tonic
 *   DarkFace synthesis success → serotonin + anandamide
 *   Queue growing stalled      → cortisol + IL6
 *   Scalability drop (Δ)       → NE
 *   Reliability drop (Δ)       → cortisol
 *
 * MODE TRANSITIONS:
 *   CognitiveMode persists N ticks → trigger mode transition
 *   REFLECTIVE/SOCIAL  → Knowledge Architect
 *   EXPLORATORY/REWARD → Blueprint Designer
 *   FOCUSED            → Task Executor
 *   VIGILANT/STRESSED/THREAT → Verification Guardian
 */
class MardukEndocrineAdapter : public EndocrineConnector {
public:
    MardukEndocrineAdapter(HormoneBus& bus, MardukInterface& marduk)
        : EndocrineConnector(bus)
        , marduk_(marduk)
    {
        base_config_ = marduk_.current_config();
        prev_telemetry_ = marduk_.telemetry();
        prev_execution_ = marduk_.execution();
        current_mode_ = bus.current_mode();
        mode_persist_count_ = 0;
    }

    // ========================================================================
    // Read path: Hormones → Marduk parameter modulation
    // ========================================================================

    void apply_endocrine_modulation(const HormoneBus& bus) {
        MardukEndocrineConfig cfg = base_config_;

        float cortisol     = bus.concentration(HormoneId::CORTISOL);
        float da_tonic     = bus.concentration(HormoneId::DOPAMINE_TONIC);
        float da_phasic    = bus.concentration(HormoneId::DOPAMINE_PHASIC);
        float serotonin    = bus.concentration(HormoneId::SEROTONIN);
        float ne           = bus.concentration(HormoneId::NOREPINEPHRINE);
        float melatonin    = bus.concentration(HormoneId::MELATONIN);
        float anandamide   = bus.concentration(HormoneId::ANANDAMIDE);
        float oxytocin     = bus.concentration(HormoneId::OXYTOCIN);
        float thyroid      = bus.concentration(HormoneId::T3_T4);
        float cog_coherence = bus.concentration(HormoneId::COG_COHERENCE);

        // --- Task Urgency: priority pressure ---
        // Cortisol (stress) increases urgency, anandamide (dampening) decreases
        cfg.task_urgency = base_config_.task_urgency
            + cortisol * 0.3f
            - anandamide * 0.2f;
        cfg.task_urgency = std::clamp(cfg.task_urgency, 0.0f, 1.0f);

        // --- Task Exploration: willingness to branch ---
        // DA_tonic (motivation) increases, NE (vigilance) narrows focus
        cfg.task_exploration = base_config_.task_exploration
            + da_tonic * 0.3f
            - ne * 0.25f;
        cfg.task_exploration = std::clamp(cfg.task_exploration, 0.0f, 1.0f);

        // --- Consolidation Depth: memory encoding depth ---
        // DA_phasic (reward) deepens encoding, cortisol (stress) shallows it
        cfg.consolidation_depth = base_config_.consolidation_depth
            + da_phasic * 0.3f
            - cortisol * 0.2f;
        cfg.consolidation_depth = std::clamp(cfg.consolidation_depth, 0.0f, 1.0f);

        // --- Memory Retention: persistence strength ---
        // Serotonin (patience) increases retention
        cfg.memory_retention = base_config_.memory_retention
            + serotonin * 0.2f;
        cfg.memory_retention = std::clamp(cfg.memory_retention, 0.0f, 1.0f);

        // --- Validation Threshold: quality bar ---
        // Serotonin (patience) raises bar; COG_COHERENCE (hemispheric coupling) raises bar
        cfg.validation_threshold = base_config_.validation_threshold
            + serotonin * 0.15f
            + cog_coherence * 0.2f;
        cfg.validation_threshold = std::clamp(cfg.validation_threshold, 0.0f, 1.0f);

        // --- Synthesis Intensity: DarkFace constraint integration ---
        // DA_phasic (reward) and melatonin (maintenance) increase synthesis
        cfg.synthesis_intensity = base_config_.synthesis_intensity
            + da_phasic * 0.2f
            + melatonin * 0.3f;
        cfg.synthesis_intensity = std::clamp(cfg.synthesis_intensity, 0.0f, 1.0f);

        // --- Autonomy Rate: self-optimization speed ---
        // T3/T4 (thyroid) speeds up cycles
        cfg.autonomy_rate = base_config_.autonomy_rate
            + thyroid * 0.3f;
        cfg.autonomy_rate = std::clamp(cfg.autonomy_rate, 0.0f, 1.0f);

        // --- Integration Openness: receptivity to external input ---
        // Oxytocin (trust) opens to external guidance
        cfg.integration_openness = base_config_.integration_openness
            + oxytocin * 0.3f;
        cfg.integration_openness = std::clamp(cfg.integration_openness, 0.0f, 1.0f);

        // Maintain current mode tag
        cfg.active_mode = marduk_.active_mode();

        marduk_.apply_config(cfg);

        // Check for operational mode transition
        update_mode_transition(bus.current_mode());
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    // ========================================================================
    // Write path: Marduk execution → Hormone feedback
    // ========================================================================

    void apply_feedback() {
        MardukTelemetry tel = marduk_.telemetry();
        ExecutionMetrics ex = marduk_.execution();

        // --- Task queue depth → MARDUK_LOAD channel (16) ---
        bus_.produce(HormoneId::MARDUK_LOAD, tel.task_queue_depth * 0.1f);

        // --- Organization metric → ORG_COHERENCE channel (17) ---
        bus_.produce(HormoneId::ORG_COHERENCE, ex.organization * 0.1f);

        // --- Consolidation success (Δ > threshold) → serotonin ---
        float delta_consolidation = tel.consolidation_progress - prev_telemetry_.consolidation_progress;
        if (delta_consolidation > consolidation_threshold_) {
            bus_.produce(HormoneId::SEROTONIN, 0.05f);
        }

        // --- Optimization cycle complete → DA_phasic ---
        if (tel.optimization_cycles > prev_telemetry_.optimization_cycles) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.1f);
        }

        // --- High cognitive load (queue > 0.8 && autonomy > 0.7) → IL6 ---
        if (tel.task_queue_depth > 0.8f && tel.autonomy_intensity > 0.7f) {
            bus_.produce(HormoneId::IL6, 0.05f);
        }

        // --- LightFace activation (rising edge) → DA_tonic ---
        if (tel.lightface_active && !prev_telemetry_.lightface_active) {
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.08f);
        }

        // --- DarkFace synthesis success (coherence > 0.7 and rising) → serotonin + anandamide ---
        float delta_synthesis = tel.synthesis_coherence - prev_telemetry_.synthesis_coherence;
        if (tel.darkface_active && tel.synthesis_coherence > 0.7f && delta_synthesis > 0.05f) {
            bus_.produce(HormoneId::SEROTONIN, 0.05f);
            bus_.produce(HormoneId::ANANDAMIDE, 0.03f);
        }

        // --- Queue growing without completions → cortisol + IL6 ---
        bool queue_growing = tel.task_queue_depth > prev_telemetry_.task_queue_depth + 0.1f;
        bool no_completions = tel.tasks_completed == prev_telemetry_.tasks_completed;
        if (queue_growing && no_completions) {
            bus_.produce(HormoneId::CORTISOL, 0.05f);
            bus_.produce(HormoneId::IL6, 0.03f);
        }

        // --- Scalability drop (Δ < -0.1) → NE ---
        float delta_scalability = ex.scalability - prev_execution_.scalability;
        if (delta_scalability < -0.1f) {
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
        }

        // --- Reliability drop (Δ < -0.1) → cortisol ---
        float delta_reliability = ex.reliability - prev_execution_.reliability;
        if (delta_reliability < -0.1f) {
            bus_.produce(HormoneId::CORTISOL, 0.03f);
        }

        prev_telemetry_ = tel;
        prev_execution_ = ex;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    /// Set number of ticks a mode must persist before triggering mode transition
    void set_hysteresis_ticks(size_t n) noexcept { hysteresis_ticks_ = n; }

    /// Set consolidation delta threshold for serotonin feedback
    void set_consolidation_threshold(float t) noexcept { consolidation_threshold_ = t; }

    /// Manually suggest a mode transition (e.g., from guidance)
    void suggest_transition(MardukOperationalMode mode) {
        marduk_.transition_mode(mode);
        base_config_ = MARDUK_MODE_PRESETS[static_cast<size_t>(mode)];
    }

    /// Access the Marduk interface
    [[nodiscard]] MardukInterface& marduk() noexcept { return marduk_; }
    [[nodiscard]] const MardukInterface& marduk() const noexcept { return marduk_; }

    /// Get the base (unmodulated) configuration
    [[nodiscard]] const MardukEndocrineConfig& base_config() const noexcept {
        return base_config_;
    }

private:
    MardukInterface& marduk_;
    MardukEndocrineConfig base_config_;
    MardukTelemetry prev_telemetry_;
    ExecutionMetrics prev_execution_;

    // Operational mode transition hysteresis
    CognitiveMode current_mode_{CognitiveMode::RESTING};
    size_t mode_persist_count_{0};
    size_t hysteresis_ticks_{5};  ///< Ticks a mode must persist before transition

    // Feedback thresholds
    float consolidation_threshold_{0.05f};

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

        MardukOperationalMode current = marduk_.active_mode();
        MardukOperationalMode suggested = mode_to_operational(mode);

        if (suggested != current && has_definite_mapping(mode)) {
            marduk_.transition_mode(suggested);
            base_config_ = MARDUK_MODE_PRESETS[static_cast<size_t>(suggested)];
            mode_persist_count_ = 0;  // Reset after transition
        }
    }

    /**
     * @brief Map cognitive mode to suggested Marduk operational mode
     *
     * Complementary to the o9c2 persona mapping — the same endocrine state
     * triggers different behavioral responses in each hemisphere:
     *   REFLECTIVE → Scholar (o9c2) / Knowledge Architect (Marduk)
     *   EXPLORATORY → Explorer (o9c2) / Blueprint Designer (Marduk)
     *   FOCUSED → Analyst (o9c2) / Task Executor (Marduk)
     *   THREAT → Analyst (o9c2) / Verification Guardian (Marduk)
     */
    [[nodiscard]] static MardukOperationalMode mode_to_operational(CognitiveMode mode) noexcept {
        switch (mode) {
        case CognitiveMode::REFLECTIVE:  return MardukOperationalMode::KNOWLEDGE_ARCHITECT;
        case CognitiveMode::SOCIAL:      return MardukOperationalMode::KNOWLEDGE_ARCHITECT;
        case CognitiveMode::EXPLORATORY: return MardukOperationalMode::BLUEPRINT_DESIGNER;
        case CognitiveMode::REWARD:      return MardukOperationalMode::BLUEPRINT_DESIGNER;
        case CognitiveMode::FOCUSED:     return MardukOperationalMode::TASK_EXECUTOR;
        case CognitiveMode::VIGILANT:    return MardukOperationalMode::VERIFICATION_GUARDIAN;
        case CognitiveMode::STRESSED:    return MardukOperationalMode::VERIFICATION_GUARDIAN;
        case CognitiveMode::THREAT:      return MardukOperationalMode::VERIFICATION_GUARDIAN;
        default:                         return MardukOperationalMode::KNOWLEDGE_ARCHITECT;
        }
    }

    /**
     * @brief Whether a cognitive mode has a definite operational mode mapping
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
