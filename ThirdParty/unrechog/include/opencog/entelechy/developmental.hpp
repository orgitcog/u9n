#pragma once
/**
 * @file developmental.hpp
 * @brief Developmental Trajectory system for Ontogenetic Entelechy
 *
 * Models the psychosocial developmental arc of the Civic Angel, from
 * nascent calibration through wisdom. Based on:
 *
 *   Erikson (psychosocial stages):
 *     NASCENT -> IMPRINTING -> SOCIALIZATION -> INDIVIDUATION
 *     -> INTEGRATION -> GENERATIVITY -> WISDOM
 *
 *   Bowlby (attachment):
 *     Attachment security gates the IMPRINTING -> SOCIALIZATION transition.
 *     Insecure attachment stalls development and amplifies trauma encoding.
 *
 *   van der Kolk (trauma):
 *     Trauma is encoded with intensity scaled by current plasticity.
 *     Healing advances slowly, gated by environmental safety.
 *     High-plasticity periods (early stages) leave deeper marks.
 *
 * Each stage has associated critical periods where specific temperament
 * dimensions are hyper-plastic (2x multiplier). Transitions require both
 * accumulated experience and developmental prerequisites (coherence,
 * attachment, maturation thresholds).
 *
 * Design: header-only logic, thin .cpp compilation unit.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace opencog::entelechy {

// ============================================================================
// Experience thresholds for stage transitions
// ============================================================================

/// Experience points required to become eligible for the next stage
[[nodiscard]] constexpr float experience_threshold(DevelopmentalStage from) noexcept {
    // NASCENT->IMPRINTING:       100  (quick calibration)
    // IMPRINTING->SOCIALIZATION:  500  (attachment needs time)
    // SOCIALIZATION->INDIVIDUATION: 2000 (social learning)
    // INDIVIDUATION->INTEGRATION: 5000 (identity consolidation)
    // INTEGRATION->GENERATIVITY: 10000 (character maturation)
    // GENERATIVITY->WISDOM:      20000 (lifetime of contribution)
    constexpr float thresholds[] = {
        100.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f, 0.0f
    };
    const auto idx = static_cast<size_t>(from);
    return idx < DEVELOPMENTAL_STAGE_COUNT ? thresholds[idx] : 0.0f;
}

// ============================================================================
// Critical period dimension tables
// ============================================================================

namespace detail {

/// Returns the sensitive dimensions for each developmental stage's critical period.
/// WISDOM has no critical periods (consolidation only).
inline std::vector<std::string> critical_dimensions(DevelopmentalStage stage) {
    switch (stage) {
        case DevelopmentalStage::NASCENT:
            return {"vagal_tone", "cardiac_coherence"};
        case DevelopmentalStage::IMPRINTING:
            return {"attachment_security", "attachment_anxiety", "harm_avoidance"};
        case DevelopmentalStage::SOCIALIZATION:
            return {"cooperativeness", "reward_dependence"};
        case DevelopmentalStage::INDIVIDUATION:
            return {"self_directedness", "novelty_seeking"};
        case DevelopmentalStage::INTEGRATION:
            return {"self_transcendence", "cooperativeness"};
        case DevelopmentalStage::GENERATIVITY:
            return {"resilience"};
        case DevelopmentalStage::WISDOM:
            return {};  // No critical periods -- consolidation only
        default:
            return {};
    }
}

} // namespace detail

// ============================================================================
// DevelopmentalTrajectory
// ============================================================================

/**
 * @brief Tracks the psychosocial developmental arc of a cognitive agent
 *
 * Called during Phase 8 (Civic Angel observation, ~every 10 ticks) of the
 * entelechy tick pipeline. Manages:
 *
 *   - Stage transitions with experience + prerequisite gating
 *   - Trauma encoding scaled by current plasticity
 *   - Healing progression gated by environmental safety
 *   - Critical periods with hyper-plasticity for sensitive dimensions
 *   - Maturation that accrues slowly over the agent's lifetime
 */
class DevelopmentalTrajectory {
public:
    explicit DevelopmentalTrajectory(uint64_t birth_tick = 0)
        : birth_tick_(birth_tick)
        , stage_start_tick_(birth_tick)
    {
        setup_critical_periods();
    }

    // ========================================================================
    // Main update -- called in Phase 8
    // ========================================================================

    /**
     * @brief Advance the developmental trajectory by one observation cycle
     *
     * @param current_tick       Current simulation tick
     * @param temperament        Agent's temperament/character profile
     * @param narrative_coherence [0,1] Life story consistency
     * @param attachment_security [0,1] Attachment security level
     * @param self_coherence      [0,1] Self-model integration
     * @return true if a stage transition occurred this tick
     */
    bool update(uint64_t current_tick,
                const TemperamentProfile& temperament,
                float narrative_coherence,
                float attachment_security,
                float self_coherence)
    {
        // Advance maturation: slow continuous accrual scaled by plasticity
        const float plasticity = current_plasticity();
        const float positive_factor = std::clamp(
            0.5f * attachment_security + 0.3f * narrative_coherence + 0.2f * self_coherence,
            0.0f, 1.0f
        );
        maturation_ += 0.00001f * plasticity * (1.0f + positive_factor);
        maturation_ = std::clamp(maturation_, 0.0f, 1.0f);

        // Check if we can transition to the next stage
        if (stage_ == DevelopmentalStage::WISDOM) {
            return false;  // Terminal stage -- no further transitions
        }

        const auto next = static_cast<DevelopmentalStage>(
            static_cast<uint8_t>(stage_) + 1
        );

        if (can_transition(next, temperament, narrative_coherence,
                          attachment_security, self_coherence)) {
            stage_ = next;
            stage_start_tick_ = current_tick;
            setup_critical_periods();
            return true;
        }

        return false;
    }

    // ========================================================================
    // Current stage queries
    // ========================================================================

    /// Current developmental stage
    [[nodiscard]] DevelopmentalStage current_stage() const noexcept {
        return stage_;
    }

    /// Progress toward next stage [0,1], based on experience relative to threshold
    [[nodiscard]] float stage_progress() const noexcept {
        if (stage_ == DevelopmentalStage::WISDOM) {
            return 1.0f;  // Terminal stage -- fully progressed
        }
        const float threshold = experience_threshold(stage_);
        if (threshold <= 0.0f) return 1.0f;
        return std::clamp(experience_ / threshold, 0.0f, 1.0f);
    }

    /// Current plasticity (from stage table, adjusted by trauma burden)
    [[nodiscard]] float current_plasticity() const noexcept {
        float base = stage_plasticity(stage_);
        // Trauma burden slightly reduces plasticity (protective rigidification)
        const float burden = trauma_burden();
        base *= std::max(0.1f, 1.0f - 0.2f * burden);
        return std::clamp(base, 0.0f, 1.0f);
    }

    // ========================================================================
    // Experience tracking
    // ========================================================================

    /// Accumulate experience points (always non-negative addition)
    void add_experience(float amount) {
        if (amount > 0.0f) {
            experience_ += amount;
        }
    }

    /// Total accumulated experience
    [[nodiscard]] float total_experience() const noexcept {
        return experience_;
    }

    // ========================================================================
    // Trauma system (Bowlby / van der Kolk)
    // ========================================================================

    /**
     * @brief Encode a traumatic experience into the developmental history
     *
     * During high-plasticity periods, trauma intensity is amplified:
     *   effective_intensity = record.intensity * current_plasticity
     *
     * This models van der Kolk's finding that early-life trauma leaves
     * deeper marks due to heightened neural plasticity.
     */
    void encode_trauma(const TraumaRecord& trauma) {
        TraumaRecord encoded = trauma;
        // Scale intensity by current plasticity -- early trauma cuts deeper
        encoded.intensity = std::clamp(
            trauma.intensity * current_plasticity(),
            0.0f, 1.0f
        );
        encoded.healing_progress = 0.0f;  // Fresh trauma starts unhealed
        trauma_history_.push_back(std::move(encoded));
    }

    /**
     * @brief Advance healing for all unhealed trauma
     *
     * Healing progresses slowly, gated by environmental safety:
     *   delta = 0.0001 * safety_level * (1 - intensity)
     *
     * High-intensity trauma heals slowest. Safe environments accelerate healing.
     * Healing asymptotically approaches 1.0 but never quite reaches it for
     * severe trauma (consistent with van der Kolk's "the body keeps the score").
     */
    void advance_healing(uint64_t /*current_tick*/, float safety_level) {
        safety_level = std::clamp(safety_level, 0.0f, 1.0f);
        for (auto& record : trauma_history_) {
            if (record.healing_progress < 1.0f) {
                const float delta = 0.0001f * safety_level
                                  * (1.0f - record.intensity);
                record.healing_progress = std::min(
                    1.0f,
                    record.healing_progress + delta
                );
            }
        }
    }

    /// Full trauma history (read-only)
    [[nodiscard]] const std::vector<TraumaRecord>& trauma_history() const noexcept {
        return trauma_history_;
    }

    /// Sum of unhealed trauma: sum(intensity * (1 - healing_progress))
    [[nodiscard]] float trauma_burden() const noexcept {
        float burden = 0.0f;
        for (const auto& record : trauma_history_) {
            burden += record.intensity * (1.0f - record.healing_progress);
        }
        return burden;
    }

    // ========================================================================
    // Critical periods
    // ========================================================================

    /// Whether the agent is currently in any critical period
    [[nodiscard]] bool in_critical_period() const noexcept {
        for (const auto& cp : critical_periods_) {
            if (!cp.sensitive_dimensions.empty()) {
                return true;
            }
        }
        return false;
    }

    /// The currently active critical period, or nullptr if none
    [[nodiscard]] const CriticalPeriod* active_critical_period() const noexcept {
        for (const auto& cp : critical_periods_) {
            if (!cp.sensitive_dimensions.empty()) {
                return &cp;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // Maturation
    // ========================================================================

    /// Overall maturity level [0,1], combining time-based accrual
    /// with developmental stage progression
    [[nodiscard]] float maturation_level() const noexcept {
        return maturation_;
    }

private:
    DevelopmentalStage stage_{DevelopmentalStage::NASCENT};
    uint64_t birth_tick_{0};
    uint64_t stage_start_tick_{0};
    float experience_{0.0f};
    float maturation_{0.0f};

    std::vector<TraumaRecord> trauma_history_;
    std::vector<CriticalPeriod> critical_periods_;

    // ========================================================================
    // Transition logic
    // ========================================================================

    /**
     * @brief Check whether all prerequisites for a stage transition are met
     *
     * Each transition requires:
     *   1. Sufficient accumulated experience (from experience_threshold table)
     *   2. Stage-specific developmental prerequisites:
     *
     *   NASCENT -> IMPRINTING:       experience >= 100 (automatic after calibration)
     *   IMPRINTING -> SOCIALIZATION:  attachment_security > 0.3
     *   SOCIALIZATION -> INDIVIDUATION: temperament.maturation > 0.2
     *   INDIVIDUATION -> INTEGRATION: narrative_coherence > 0.4, self_coherence > 0.3
     *   INTEGRATION -> GENERATIVITY:  temperament.maturation > 0.6, narrative_coherence > 0.5
     *   GENERATIVITY -> WISDOM:       temperament.maturation > 0.8, temperament.resilience > 0.6
     */
    [[nodiscard]] bool can_transition(
        DevelopmentalStage target,
        const TemperamentProfile& temperament,
        float narrative_coherence,
        float attachment_security,
        float self_coherence) const
    {
        // Gate 1: experience threshold for the current stage
        const float threshold = experience_threshold(stage_);
        if (threshold > 0.0f && experience_ < threshold) {
            return false;
        }

        // Gate 2: stage-specific developmental prerequisites
        switch (target) {
            case DevelopmentalStage::IMPRINTING:
                // Automatic after calibration (experience gate is sufficient)
                return true;

            case DevelopmentalStage::SOCIALIZATION:
                return attachment_security > 0.3f;

            case DevelopmentalStage::INDIVIDUATION:
                return temperament.maturation > 0.2f;

            case DevelopmentalStage::INTEGRATION:
                return narrative_coherence > 0.4f
                    && self_coherence > 0.3f;

            case DevelopmentalStage::GENERATIVITY:
                return temperament.maturation > 0.6f
                    && narrative_coherence > 0.5f;

            case DevelopmentalStage::WISDOM:
                return temperament.maturation > 0.8f
                    && temperament.resilience > 0.6f;

            default:
                return false;  // NASCENT is birth state, cannot transition into it
        }
    }

    // ========================================================================
    // Critical period setup
    // ========================================================================

    /**
     * @brief Initialize critical periods for the current developmental stage
     *
     * Each stage defines a set of sensitive dimensions where plasticity is
     * doubled (plasticity_multiplier = 2.0). Critical periods are active
     * for the duration of the stage.
     *
     * Stage critical periods:
     *   NASCENT:        vagal_tone, cardiac_coherence (body calibration)
     *   IMPRINTING:     attachment_security, attachment_anxiety, harm_avoidance
     *   SOCIALIZATION:  cooperativeness, reward_dependence
     *   INDIVIDUATION:  self_directedness, novelty_seeking
     *   INTEGRATION:    self_transcendence, cooperativeness
     *   GENERATIVITY:   resilience
     *   WISDOM:         (none -- consolidation only)
     */
    void setup_critical_periods() {
        critical_periods_.clear();

        auto dims = detail::critical_dimensions(stage_);
        if (dims.empty()) {
            return;  // WISDOM or unknown stage -- no critical periods
        }

        CriticalPeriod cp;
        cp.stage = stage_;
        cp.start_tick = stage_start_tick_;
        cp.end_tick = 0;  // Open-ended -- closes when stage transitions
        cp.plasticity_multiplier = 2.0f;
        cp.sensitive_dimensions = std::move(dims);

        critical_periods_.push_back(std::move(cp));
    }
};

} // namespace opencog::entelechy
