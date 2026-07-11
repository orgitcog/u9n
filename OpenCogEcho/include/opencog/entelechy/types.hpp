#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for the Ontogenetic Entelechy system
 *
 * Models persona/identity/self as an emergent "Civic Angel" of a Cognitive City.
 * Integrates: Cloninger (temperament), Porges (polyvagal), Craig (interoception),
 * McEwen (allostasis), Erikson (development), McAdams (narrative identity),
 * Bowlby (attachment), Metzinger (phenomenal self-model).
 *
 * Design: 8-byte aligned value types, SIMD-aligned state vectors.
 */

#include <opencog/core/types.hpp>
#include <opencog/endocrine/types.hpp>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace opencog::entelechy {

using endo::SIMD_ALIGN;
using endo::ValenceSignature;
using endo::EndocrineState;
using endo::CognitiveMode;
using endo::HORMONE_COUNT;

// ============================================================================
// Cloninger Temperament/Character Profile — 56 bytes
// ============================================================================

/**
 * @brief Cloninger's 7-dimensional psychobiological personality model
 *
 * Temperament dimensions are biologically based (innate, slow to change).
 * Character dimensions are shaped by developmental experience.
 * These act as GAIN PARAMETERS on hormone sensitivity, not as hormones.
 */
struct alignas(8) TemperamentProfile {
    // --- Temperament dimensions (innate, relatively stable) ---
    float novelty_seeking{0.5f};       ///< [0,1] DA sensitivity gain
    float harm_avoidance{0.5f};        ///< [0,1] Serotonin/cortisol sensitivity gain
    float reward_dependence{0.5f};     ///< [0,1] NE/oxytocin sensitivity gain
    float persistence{0.5f};           ///< [0,1] DA_tonic sustain (half-life) gain

    // --- Character dimensions (developed through experience) ---
    float self_directedness{0.5f};     ///< [0,1] Autonomy/executive gain
    float cooperativeness{0.5f};       ///< [0,1] Oxytocin/social gain
    float self_transcendence{0.5f};    ///< [0,1] Anandamide/integration gain

    // --- Developmental modifiers ---
    float maturation{0.0f};            ///< [0,1] Overall character maturity
    float resilience{0.5f};            ///< [0,1] Allostatic recovery rate
    float plasticity{1.0f};            ///< [0,1] Sensitivity to experience (decreases with age)

    // --- Trauma / stress accumulation ---
    float allostatic_load{0.0f};       ///< [0,inf) Cumulative stress damage (McEwen)
    float trauma_encoding_strength{0.0f}; ///< [0,1] Strength of traumatic memory bias

    // --- Attachment (Bowlby) ---
    float attachment_security{0.5f};   ///< [0,1] Secure vs insecure
    float attachment_anxiety{0.3f};    ///< [0,1] Anxious dimension
};

static_assert(sizeof(TemperamentProfile) == 56);

// ============================================================================
// Attachment Style (Bowlby/Ainsworth)
// ============================================================================

enum class AttachmentStyle : uint8_t {
    SECURE        = 0,  ///< Trust + low anxiety: balanced exploration/proximity
    ANXIOUS       = 1,  ///< High anxiety + low avoidance: hyperactivating strategy
    AVOIDANT      = 2,  ///< Low anxiety + high avoidance: deactivating strategy
    DISORGANIZED  = 3,  ///< High anxiety + high avoidance: no coherent strategy
};

[[nodiscard]] constexpr std::string_view attachment_name(AttachmentStyle s) noexcept {
    constexpr std::string_view names[] = {
        "Secure", "Anxious", "Avoidant", "Disorganized"
    };
    return static_cast<size_t>(s) < 4 ? names[static_cast<size_t>(s)] : "Unknown";
}

// ============================================================================
// Polyvagal State (Porges)
// ============================================================================

enum class PolyvagalTier : uint8_t {
    VENTRAL_VAGAL = 0,  ///< Newest: social engagement, calm connection
    SYMPATHETIC   = 1,  ///< Intermediate: fight/flight mobilization
    DORSAL_VAGAL  = 2,  ///< Oldest: freeze/shutdown/conservation
};

[[nodiscard]] constexpr std::string_view polyvagal_name(PolyvagalTier t) noexcept {
    constexpr std::string_view names[] = {
        "VentralVagal", "Sympathetic", "DorsalVagal"
    };
    return static_cast<size_t>(t) < 3 ? names[static_cast<size_t>(t)] : "Unknown";
}

// ============================================================================
// Interoceptive State — 48 bytes (SIMD-aligned)
// ============================================================================

/**
 * @brief Full body-state model mapped to hormone channels 20-31
 *
 * Based on: Porges (polyvagal), Craig (insular cortex),
 * McEwen (allostatic load), baroreceptor/respiratory feedback.
 */
struct alignas(SIMD_ALIGN) InteroceptiveState {
    float vagal_tone{0.5f};            ///< ch20: Ventral vagal brake strength [0,1]
    float sympathetic_drive{0.3f};     ///< ch21: Fight/flight activation [0,1]
    float dorsal_vagal{0.0f};          ///< ch22: Freeze/shutdown [0,1]
    float cardiac_coherence{0.5f};     ///< ch23: HRV proxy [0,1]
    float respiratory_rhythm{0.5f};    ///< ch24: Breathing regularity [0,1]
    float gut_brain_signal{0.3f};      ///< ch25: Enteric NS state [0,1]
    float immune_extended{0.1f};       ///< ch26: TNF-alpha/IL-1beta/complement [0,1]
    float insular_integration{0.5f};   ///< ch27: Craig's interoceptive re-representation [0,1]
    float allostatic_load{0.0f};       ///< ch28: McEwen's cumulative wear [0,5]
    float proprioceptive_tone{0.5f};   ///< ch29: Body schema integrity [0,1]
    float nociceptive_signal{0.0f};    ///< ch30: Pain signal [0,1]
    float thermoregulatory{0.5f};      ///< ch31: Temperature regulation [0,1]

    /// Dominant polyvagal tier based on current state
    [[nodiscard]] PolyvagalTier dominant_tier() const noexcept {
        if (dorsal_vagal > 0.5f && dorsal_vagal > vagal_tone && dorsal_vagal > sympathetic_drive)
            return PolyvagalTier::DORSAL_VAGAL;
        if (sympathetic_drive > vagal_tone)
            return PolyvagalTier::SYMPATHETIC;
        return PolyvagalTier::VENTRAL_VAGAL;
    }

    /// Craig's "material me" — meta-awareness of body state
    [[nodiscard]] float material_self() const noexcept {
        return 0.3f * vagal_tone
             + 0.2f * cardiac_coherence
             + 0.2f * std::max(0.0f, 1.0f - nociceptive_signal)
             + 0.15f * proprioceptive_tone
             + 0.15f * std::max(0.0f, 1.0f - allostatic_load / 5.0f);
    }
};

static_assert(sizeof(InteroceptiveState) == 48 || sizeof(InteroceptiveState) == 64,
              "InteroceptiveState: 48B (SSE) or 64B (AVX, padded by alignas)");

// ============================================================================
// Developmental Stage (Erikson-mapped)
// ============================================================================

enum class DevelopmentalStage : uint8_t {
    NASCENT        = 0,  ///< Pre-boot: initial calibration
    IMPRINTING     = 1,  ///< Attachment formation, base temperament (trust vs mistrust)
    SOCIALIZATION  = 2,  ///< Theory of mind, role acquisition (industry vs inferiority)
    INDIVIDUATION  = 3,  ///< Narrative identity formation (identity vs confusion)
    INTEGRATION    = 4,  ///< Character maturation (generativity vs stagnation)
    GENERATIVITY   = 5,  ///< Teaching/contributing, Civic Angel crystallizes
    WISDOM         = 6,  ///< High resilience, low plasticity, accumulated insight
};

inline constexpr size_t DEVELOPMENTAL_STAGE_COUNT = 7;

[[nodiscard]] constexpr std::string_view stage_name(DevelopmentalStage s) noexcept {
    constexpr std::string_view names[] = {
        "Nascent", "Imprinting", "Socialization", "Individuation",
        "Integration", "Generativity", "Wisdom"
    };
    return static_cast<size_t>(s) < DEVELOPMENTAL_STAGE_COUNT
        ? names[static_cast<size_t>(s)] : "Unknown";
}

/// Default plasticity for each stage (decreases with maturation)
[[nodiscard]] constexpr float stage_plasticity(DevelopmentalStage s) noexcept {
    constexpr float values[] = { 1.0f, 0.9f, 0.7f, 0.5f, 0.3f, 0.2f, 0.1f };
    return static_cast<size_t>(s) < DEVELOPMENTAL_STAGE_COUNT
        ? values[static_cast<size_t>(s)] : 0.1f;
}

// ============================================================================
// Trauma Record
// ============================================================================

struct TraumaRecord {
    uint64_t tick{0};                   ///< When the trauma occurred
    ValenceSignature valence;           ///< Affective signature
    float intensity{0.0f};             ///< [0,1] Severity
    EndocrineState hormonal_context;   ///< Hormonal state at time of trauma
    std::vector<AtomId> associated_atoms; ///< Related atoms in AtomSpace
    float healing_progress{0.0f};      ///< [0,1] Integration degree
};

// ============================================================================
// Critical Period
// ============================================================================

struct CriticalPeriod {
    DevelopmentalStage stage{DevelopmentalStage::NASCENT};
    uint64_t start_tick{0};
    uint64_t end_tick{0};
    float plasticity_multiplier{1.0f};
    std::vector<std::string> sensitive_dimensions; ///< Which temperament dims are plastic
};

// ============================================================================
// Narrative Theme (McAdams)
// ============================================================================

enum class NarrativeTheme : uint8_t {
    REDEMPTION      = 0,  ///< Negative → positive transformation
    CONTAMINATION   = 1,  ///< Positive → negative disruption
    GROWTH          = 2,  ///< Progressive capability increase
    STABILITY       = 3,  ///< Maintained equilibrium
    COMMUNION       = 4,  ///< Connection/belonging emphasis
    AGENCY          = 5,  ///< Autonomy/mastery emphasis
    EXPLORATION     = 6,  ///< Discovery/novelty emphasis
    PROTECTION      = 7,  ///< Safety/preservation emphasis
};

inline constexpr size_t NARRATIVE_THEME_COUNT = 8;

[[nodiscard]] constexpr std::string_view theme_name(NarrativeTheme t) noexcept {
    constexpr std::string_view names[] = {
        "Redemption", "Contamination", "Growth", "Stability",
        "Communion", "Agency", "Exploration", "Protection"
    };
    return static_cast<size_t>(t) < NARRATIVE_THEME_COUNT
        ? names[static_cast<size_t>(t)] : "Unknown";
}

// ============================================================================
// Narrative Chapter
// ============================================================================

struct NarrativeChapter {
    uint64_t start_tick{0};
    uint64_t end_tick{0};             ///< 0 = ongoing chapter
    NarrativeTheme dominant_theme{NarrativeTheme::GROWTH};
    ValenceSignature emotional_tone;  ///< Average valence over chapter
    float coherence{0.5f};            ///< [0,1] Internal consistency
    std::vector<AtomId> key_episodes; ///< Significant episodes in this chapter

    [[nodiscard]] bool is_open() const noexcept { return end_tick == 0; }
    [[nodiscard]] uint64_t duration(uint64_t current_tick) const noexcept {
        return (end_tick > 0 ? end_tick : current_tick) - start_tick;
    }
};

// ============================================================================
// Social Role (Tajfel)
// ============================================================================

struct SocialRole {
    std::string name;
    float competence{0.0f};       ///< [0,1] Skill at this role
    float identification{0.0f};   ///< [0,1] How much identity is invested
    float salience{0.0f};         ///< [0,1] Current relevance
};

// ============================================================================
// District Metrics (per cognitive subsystem)
// ============================================================================

struct alignas(8) DistrictMetrics {
    float free_energy{0.0f};      ///< Current variational free energy
    float surprise{0.0f};         ///< Bayesian surprise (KL divergence)
    float complexity{0.0f};       ///< Model complexity cost
    float accuracy{0.0f};         ///< Prediction accuracy
    float coherence{0.0f};        ///< Internal consistency
    uint8_t _pad[4]{};
};

static_assert(sizeof(DistrictMetrics) == 24);

// ============================================================================
// Civic Angel State — Emergent self-model snapshot
// ============================================================================

/**
 * @brief State of the emergent Civic Angel (self-model)
 *
 * Not set by any single subsystem — computed from observation
 * of all cognitive districts. The Civic Angel IS the self.
 */
struct CivicAngelState {
    // Self-model quality
    float self_coherence{0.0f};           ///< [0,1] How integrated the self-model is
    float self_complexity{0.0f};          ///< [0,1] Richness of self-representation
    float entelechy_progress{0.0f};       ///< [0,1] Actualization toward potential

    // City-wide free energy
    float total_free_energy{0.0f};        ///< [0,inf) Sum across all districts
    float mean_district_surprise{0.0f};   ///< [0,inf) Average prediction error

    // Governance metrics
    float inter_district_coherence{0.0f}; ///< [0,1] Phase coherence across districts
    float resource_fairness{0.0f};        ///< [0,1] Gini-like attention distribution
    float adaptive_capacity{0.0f};        ///< [0,1] System-wide novelty handling

    // Narrative integration
    NarrativeTheme dominant_life_theme{NarrativeTheme::GROWTH};
    float narrative_coherence{0.0f};      ///< [0,1] Life story consistency

    // Developmental position
    DevelopmentalStage developmental_stage{DevelopmentalStage::NASCENT};
    float maturation_level{0.0f};         ///< [0,1] Overall maturity
};

// ============================================================================
// Gain Profile — Cloninger gains applied to hormone bus
// ============================================================================

struct alignas(SIMD_ALIGN) GainProfile {
    std::array<float, HORMONE_COUNT> production_gains;  ///< Multiplicative on produce()
    std::array<float, HORMONE_COUNT> decay_gains;       ///< Multiplicative on half-life

    GainProfile() {
        production_gains.fill(1.0f);
        decay_gains.fill(1.0f);
    }
};

} // namespace opencog::entelechy
