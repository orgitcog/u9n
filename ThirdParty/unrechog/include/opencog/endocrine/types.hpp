#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for the Virtual Endocrine System (VES)
 *
 * Compact, cache-friendly types for hormonal signaling, valence tagging,
 * and moral perception. All value types follow the 8-byte alignment pattern
 * established by TruthValue and AttentionValue.
 *
 * Design principles:
 * - ValenceSignature and HormoneLevel are 8 bytes (like TruthValue)
 * - EndocrineState is SIMD-aligned for batch operations
 * - HormoneId count padded to 32 (power-of-2) for AVX2 alignment
 */

#include <opencog/core/types.hpp>

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace opencog::endo {

// ============================================================================
// SIMD alignment constant (matches core/memory.hpp)
// ============================================================================

#if defined(__AVX2__) || defined(__AVX__)
inline constexpr size_t SIMD_ALIGN = 32;
#else
inline constexpr size_t SIMD_ALIGN = 16;
#endif

// ============================================================================
// Hormone Channel Identifiers
// ============================================================================

/// Virtual hormone channels, padded to 32 for SIMD alignment (AVX2: 4×8)
enum class HormoneId : uint8_t {
    // Core biological channels (0-13)
    CRH             = 0,   ///< Corticotropin-releasing hormone (hypothalamus)
    ACTH            = 1,   ///< Adrenocorticotropic hormone (pituitary)
    CORTISOL        = 2,   ///< Stress / resource mobilization
    DOPAMINE_TONIC  = 3,   ///< Baseline motivation / cognitive flexibility
    DOPAMINE_PHASIC = 4,   ///< Reward prediction error signal
    SEROTONIN       = 5,   ///< Mood baseline / patience-impulsivity tradeoff
    NOREPINEPHRINE  = 6,   ///< Arousal / vigilance / attention modulation
    OXYTOCIN        = 7,   ///< Trust / bonding / prosocial orientation
    T3_T4           = 8,   ///< Thyroid: global processing rate governor
    MELATONIN       = 9,   ///< Circadian: maintenance cycle scheduling
    INSULIN         = 10,  ///< Energy storage / conservation mode
    GLUCAGON        = 11,  ///< Energy mobilization / active processing
    IL6             = 12,  ///< Cytokine analog: system health signal
    ANANDAMIDE      = 13,  ///< Endocannabinoid: noise reduction / dampening

    // Integration channels (14-17)
    NPU_LOAD        = 14,  ///< NPU inference load / processing intensity
    COG_COHERENCE   = 15,  ///< o9c2 cognitive coherence / emergence metric
    MARDUK_LOAD     = 16,  ///< Marduk task queue depth / cognitive load
    ORG_COHERENCE   = 17,  ///< Marduk organizational coherence metric

    // Touchpad channels (18-19)
    TOUCHPAD_LOAD      = 18,  ///< VirtualTouchpad active contact load / gesture intensity
    TOUCHPAD_COHERENCE = 19,  ///< VirtualTouchpad manifold coherence metric
    // Interoceptive channels (20-31) — Porges, Craig, McEwen
    VAGAL_TONE          = 20,  ///< Polyvagal: ventral vagal brake strength [0,1]
    SYMPATHETIC_DRIVE   = 21,  ///< Polyvagal: fight/flight activation [0,1]
    DORSAL_VAGAL        = 22,  ///< Polyvagal: freeze/shutdown [0,1]
    CARDIAC_COHERENCE   = 23,  ///< HRV / baroreceptor feedback [0,1]
    RESPIRATORY_RHYTHM  = 24,  ///< Breathing regularity → brain oscillation [0,1]
    GUT_BRAIN_SIGNAL    = 25,  ///< Enteric NS (vagal afferents, gut serotonin) [0,1]
    IMMUNE_EXTENDED     = 26,  ///< TNF-alpha, IL-1beta, complement [0,1]
    INSULAR_INTEGRATION = 27,  ///< Craig's interoceptive re-representation [0,1]
    ALLOSTATIC_LOAD     = 28,  ///< McEwen's cumulative wear [0,5]
    PROPRIOCEPTIVE_TONE = 29,  ///< Body schema integrity [0,1]
    NOCICEPTIVE_SIGNAL  = 30,  ///< Pain signal [0,1]
    THERMOREGULATORY    = 31,  ///< Temperature regulation [0,1]

    HORMONE_COUNT   = 32   ///< Total channels (power-of-2 for SIMD)
};

inline constexpr size_t HORMONE_COUNT = static_cast<size_t>(HormoneId::HORMONE_COUNT);

/// Human-readable hormone names
[[nodiscard]] constexpr std::string_view hormone_name(HormoneId id) noexcept {
    constexpr std::string_view names[] = {
        "CRH", "ACTH", "Cortisol", "Dopamine(tonic)", "Dopamine(phasic)",
        "Serotonin", "Norepinephrine", "Oxytocin", "T3/T4", "Melatonin",
        "Insulin", "Glucagon", "IL-6", "Anandamide", "NPU-Load", "CogCoherence",
        "Marduk-Load", "OrgCoherence",
        "Touchpad-Load", "TouchpadCoherence",
        "VagalTone", "SympatheticDrive", "DorsalVagal", "CardiacCoherence",
        "RespiratoryRhythm", "GutBrainSignal", "ImmuneExtended", "InsularIntegration",
        "AllostaticLoad", "ProprioceptiveTone", "NociceptiveSignal", "Thermoregulatory"
    };
    auto idx = static_cast<size_t>(id);
    return idx < HORMONE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Valence Signature — 8 bytes (matches TruthValue pattern)
// ============================================================================

/**
 * @brief Affective valence tag for atoms and episodes
 *
 * Two-dimensional affect model (Russell's circumplex):
 * - valence: negative (-1) to positive (+1)
 * - arousal: calm (0) to activated (1)
 *
 * Stored alongside atoms in parallel SoA to tag experiential memories
 * with their felt-quality for later fuzzy retrieval.
 */
struct alignas(8) ValenceSignature {
    float valence{0.0f};  ///< [-1, +1]: negative to positive affect
    float arousal{0.0f};  ///< [0, 1]: calm to activated

    constexpr ValenceSignature() = default;
    constexpr ValenceSignature(float v, float a) : valence(v), arousal(a) {}

    // Named constructors
    [[nodiscard]] static constexpr ValenceSignature neutral() noexcept {
        return {0.0f, 0.0f};
    }
    [[nodiscard]] static constexpr ValenceSignature positive(float intensity = 0.7f) noexcept {
        return {0.7f, intensity};
    }
    [[nodiscard]] static constexpr ValenceSignature negative(float intensity = 0.7f) noexcept {
        return {-0.7f, intensity};
    }
    [[nodiscard]] static constexpr ValenceSignature threat() noexcept {
        return {-0.9f, 0.95f};
    }
    [[nodiscard]] static constexpr ValenceSignature joy() noexcept {
        return {0.8f, 0.7f};
    }
    [[nodiscard]] static constexpr ValenceSignature calm() noexcept {
        return {0.3f, 0.1f};
    }

    /// Magnitude in valence-arousal space
    [[nodiscard]] float magnitude() const noexcept {
        return std::sqrt(valence * valence + arousal * arousal);
    }

    /// Whether this signal is salient enough to matter
    [[nodiscard]] constexpr bool is_salient(float threshold = 0.3f) const noexcept {
        return (valence * valence + arousal * arousal) >= threshold * threshold;
    }

    /// Blend two valence signatures with a weight [0,1] favoring other
    [[nodiscard]] constexpr ValenceSignature blend(const ValenceSignature& other,
                                                    float weight) const noexcept {
        float w = std::clamp(weight, 0.0f, 1.0f);
        return {valence * (1.0f - w) + other.valence * w,
                arousal * (1.0f - w) + other.arousal * w};
    }

    constexpr auto operator<=>(const ValenceSignature&) const = default;
};

static_assert(sizeof(ValenceSignature) == 8);

// ============================================================================
// Hormone Level — 8 bytes
// ============================================================================

/// Current state of a single hormone channel
struct alignas(8) HormoneLevel {
    float concentration{0.0f};  ///< Current level [0, 1] normalized
    float rate{0.0f};           ///< Current production rate (units/tick)

    constexpr HormoneLevel() = default;
    constexpr HormoneLevel(float c, float r) : concentration(c), rate(r) {}

    constexpr auto operator<=>(const HormoneLevel&) const = default;
};

static_assert(sizeof(HormoneLevel) == 8);

// ============================================================================
// Endocrine State — Full snapshot (128 bytes, SIMD-aligned)
// ============================================================================

/// Snapshot of all hormone concentrations at a point in time
struct alignas(SIMD_ALIGN) EndocrineState {
    std::array<float, HORMONE_COUNT> concentrations{};

    [[nodiscard]] float operator[](HormoneId id) const noexcept {
        return concentrations[static_cast<size_t>(id)];
    }
    float& operator[](HormoneId id) noexcept {
        return concentrations[static_cast<size_t>(id)];
    }

    /// Euclidean distance to another state (for mode classification)
    [[nodiscard]] float distance_to(const EndocrineState& other) const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            float d = concentrations[i] - other.concentrations[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }
};

static_assert(sizeof(EndocrineState) == 128);

// ============================================================================
// Cognitive Mode — Emergent attractor state
// ============================================================================

/**
 * @brief Cognitive mode that emerges from the hormone constellation
 *
 * Not set explicitly — classified from the current EndocrineState by
 * nearest-centroid in 32D hormone space. The modes are attractors
 * in the coupled dynamical system of virtual glands.
 */
enum class CognitiveMode : uint8_t {
    RESTING     = 0,  ///< Low arousal, neutral valence
    EXPLORATORY = 1,  ///< Moderate dopamine, low cortisol
    FOCUSED     = 2,  ///< High norepinephrine, moderate cortisol
    STRESSED    = 3,  ///< High cortisol, high norepinephrine
    SOCIAL      = 4,  ///< High oxytocin, moderate serotonin
    REFLECTIVE  = 5,  ///< High serotonin, low norepinephrine
    VIGILANT    = 6,  ///< High norepinephrine, moderate cortisol
    MAINTENANCE = 7,  ///< High melatonin (consolidation/repair)
    REWARD      = 8,  ///< Phasic dopamine burst
    THREAT      = 9,  ///< Full HPA axis activation
};

[[nodiscard]] constexpr std::string_view mode_name(CognitiveMode mode) noexcept {
    constexpr std::string_view names[] = {
        "Resting", "Exploratory", "Focused", "Stressed", "Social",
        "Reflective", "Vigilant", "Maintenance", "Reward", "Threat"
    };
    auto idx = static_cast<size_t>(mode);
    return idx < 10 ? names[idx] : "Unknown";
}

inline constexpr size_t COGNITIVE_MODE_COUNT = 10;

// ============================================================================
// Moral Perception — Aggregated moral signal
// ============================================================================

/**
 * @brief Result of the moral perception pipeline
 *
 * Combines raw affective signal, morally-tagged episode associations,
 * empathic inference, and novelty detection into an actionable moral
 * perception that precedes deliberative moral reasoning.
 */
struct MoralPerception {
    ValenceSignature raw_signal;      ///< 8 bytes: unprocessed valence
    float moral_salience{0.0f};       ///< [0, 1]: how morally significant
    float empathic_weight{0.0f};      ///< [0, 1]: weight from other-modeling
    float novel_moral_signal{0.0f};   ///< [0, 1]: novelty of this moral situation
    CognitiveMode action_bias{CognitiveMode::RESTING}; ///< Suggested response mode
    uint8_t _pad[3]{};

    /// Whether this perception warrants attention
    [[nodiscard]] constexpr bool is_significant(float threshold = 0.3f) const noexcept {
        return moral_salience >= threshold;
    }

    /// Whether this is a novel moral situation (no strong precedent)
    [[nodiscard]] constexpr bool is_novel(float threshold = 0.6f) const noexcept {
        return novel_moral_signal >= threshold;
    }
};

static_assert(sizeof(MoralPerception) == 24);

// ============================================================================
// Felt Sense — Integrated affective state
// ============================================================================

/// Aggregated felt-sense from valence memory integration
struct FeltSense {
    ValenceSignature aggregate_valence;  ///< 8 bytes: weighted centroid
    float certainty{0.0f};              ///< Evidence mass from matches
    float novelty{0.0f};                ///< Inverse of best match confidence
    CognitiveMode suggested_mode{CognitiveMode::RESTING};
    uint8_t _pad[3]{};

    [[nodiscard]] constexpr float discomfort() const noexcept {
        return std::max(0.0f, -aggregate_valence.valence) * aggregate_valence.arousal;
    }

    [[nodiscard]] constexpr float comfort() const noexcept {
        return std::max(0.0f, aggregate_valence.valence) * (1.0f - aggregate_valence.arousal * 0.5f);
    }
};

static_assert(sizeof(FeltSense) == 24);

// ============================================================================
// Release Pattern
// ============================================================================

enum class ReleasePattern : uint8_t {
    TONIC        = 0,  ///< Continuous baseline release
    PULSATILE    = 1,  ///< Rhythmic bursts
    CIRCADIAN    = 2,  ///< 24-hour cycle
    EVENT_DRIVEN = 3,  ///< Triggered by external signals
};

// ============================================================================
// Hormone Channel Configuration
// ============================================================================

struct HormoneChannelConfig {
    HormoneId id{HormoneId::CRH};
    float half_life{1.0f};           ///< Decay half-life in ticks
    float saturation_ceiling{1.0f};  ///< Max concentration
    float baseline{0.0f};            ///< Homeostatic setpoint
    float min_level{0.0f};           ///< Floor concentration
};

// ============================================================================
// Gland Configuration
// ============================================================================

struct GlandConfig {
    std::string name;
    HormoneId output_channel{HormoneId::CRH};
    ReleasePattern release_pattern{ReleasePattern::TONIC};

    float base_production_rate{0.1f};
    float max_production_rate{1.0f};
    float homeostatic_setpoint{0.3f};

    /// Hormone channels that stimulate production: (channel, sensitivity)
    std::vector<std::pair<HormoneId, float>> stimulators;
    /// Hormone channels that inhibit production: (channel, sensitivity)
    std::vector<std::pair<HormoneId, float>> inhibitors;

    /// Negative feedback: own output inhibits own production
    bool negative_feedback{true};
    float feedback_gain{0.5f};

    /// Positive feedback: own output amplifies production (rare, e.g. oxytocin)
    bool positive_feedback{false};
    float positive_feedback_gain{0.0f};

    /// Allostatic shifting: setpoint drift rate under chronic load
    float allostatic_rate{0.001f};
};

// ============================================================================
// Hormone Bus Configuration
// ============================================================================

struct HormoneBusConfig {
    float tick_interval_ms{100.0f};      ///< Nominal update interval
    float global_decay_multiplier{1.0f}; ///< Speed up/slow down all decay
    bool enable_simd{true};              ///< Use SIMD for batch decay
    size_t history_length{1000};         ///< Ring buffer size for snapshots
};

// ============================================================================
// Endocrine Event — Signals that trigger hormonal cascades
// ============================================================================

enum class EndocrineEvent : uint8_t {
    // Core events (0-19)
    THREAT_DETECTED     = 0,
    REWARD_RECEIVED     = 1,
    SOCIAL_BOND_SIGNAL  = 2,
    RESOURCE_DEPLETED   = 3,
    NOVELTY_ENCOUNTERED = 4,
    GOAL_ACHIEVED       = 5,
    CONFLICT_DETECTED   = 6,
    UNCERTAINTY_HIGH    = 7,
    ERROR_DETECTED      = 8,
    NOISE_EXCESSIVE     = 9,
    LIGHT_SIGNAL        = 10,

    // NPU events (20-29)
    NPU_INFERENCE_STARTED   = 20,  ///< NPU began an inference
    NPU_INFERENCE_COMPLETE  = 21,  ///< NPU completed inference successfully
    NPU_INFERENCE_ERROR     = 22,  ///< NPU inference failed
    NPU_HIGH_LOAD           = 23,  ///< NPU context utilization above threshold
    NPU_MODEL_LOADED        = 24,  ///< NPU loaded a new model

    // o9c2 events (30-39)
    O9C2_PERSONA_TRANSITION = 30,  ///< o9c2 switched cognitive persona
    O9C2_EMERGENCE_SPIKE    = 31,  ///< Emergence metric crossed threshold
    O9C2_COHERENCE_DROP     = 32,  ///< Cognitive coherence fell below threshold
    O9C2_WISDOM_GAIN        = 33,  ///< Wisdom metric increased significantly
    O9C2_INSTABILITY        = 34,  ///< Reservoir dynamics became unstable

    // Guidance events (40-49)
    GUIDANCE_REQUESTED      = 40,  ///< System requesting external guidance
    GUIDANCE_RECEIVED       = 41,  ///< Guidance response received and applied
    GUIDANCE_TIMEOUT        = 42,  ///< Guidance request timed out
    GUIDANCE_CONFLICT       = 43,  ///< Guidance contradicts current state

    // Marduk events (50-59) — Left-Hemisphere cognitive agent
    MARDUK_TASK_STARTED             = 50,  ///< Marduk began a task execution
    MARDUK_TASK_COMPLETED           = 51,  ///< Marduk completed a task successfully
    MARDUK_TASK_FAILED              = 52,  ///< Marduk task execution failed
    MARDUK_MEMORY_CONSOLIDATED      = 53,  ///< Memory atom successfully encoded
    MARDUK_AUTONOMY_CYCLE           = 54,  ///< Autonomy optimizer completed a cycle
    MARDUK_OPTIMIZATION_COMPLETE    = 55,  ///< Meta-cognitive optimization pass finished
    MARDUK_COGNITIVE_OVERLOAD       = 56,  ///< Task queue + autonomy exceeds capacity
    MARDUK_LIGHTFACE_SPIKE          = 57,  ///< LightFace exploration burst (generative branching)
    MARDUK_DARKFACE_SYNTHESIS       = 58,  ///< DarkFace synthesis success (constraint integration)
    MARDUK_HEMISPHERIC_SYNC         = 59,  ///< Left-Right hemisphere synchronization event

    // VirtualTouchpad events (60-69) — N-dimensional cognitive gesture manifold
    TOUCHPAD_CONTACT_STARTED        = 60,  ///< New contact point registered in manifold
    TOUCHPAD_CONTACT_ENDED          = 61,  ///< Contact point released from manifold
    TOUCHPAD_GESTURE_RECOGNIZED     = 62,  ///< Gesture pattern classified from contacts
    TOUCHPAD_GESTURE_EMITTED        = 63,  ///< Gesture emitted (outgoing intent broadcast)
    TOUCHPAD_CALIBRATION_STARTED    = 64,  ///< Manifold calibration/tuning initiated
    TOUCHPAD_CALIBRATION_COMPLETE   = 65,  ///< Manifold calibration completed
    TOUCHPAD_COHERENCE_DROP         = 66,  ///< Manifold coherence fell below threshold
    TOUCHPAD_FIELD_RESONANCE        = 67,  ///< Amplitude field standing wave detected
    TOUCHPAD_TOPOLOGY_CHANGE        = 68,  ///< Manifold topology altered (fold/twist/collapse)
    TOUCHPAD_OVERLOAD               = 69,  ///< Contact/gesture capacity exceeded

    // Entelechy System (70-78)
    INTEROCEPTIVE_ALARM             = 70,  ///< Interoceptive signal exceeded threshold
    POLYVAGAL_STATE_CHANGE          = 71,  ///< Polyvagal tier transition (ventral/sympathetic/dorsal)
    DEVELOPMENTAL_TRANSITION        = 72,  ///< Developmental stage advanced
    CHAPTER_BOUNDARY                = 73,  ///< Narrative chapter boundary detected
    TRAUMA_ENCODED                  = 74,  ///< Traumatic experience encoded
    TRAUMA_HEALING                  = 75,  ///< Trauma healing progress milestone
    ATTACHMENT_SHIFT                = 76,  ///< Attachment style changed
    IDENTITY_CRYSTALLIZED           = 77,  ///< Self-model coherence threshold crossed
    SELF_TRANSCENDENCE_SPIKE        = 78,  ///< Self-transcendence dimension spike

    // Active Free-energy Inference (80-84)
    FREE_ENERGY_SPIKE               = 80,  ///< District free energy exceeded threshold
    PREDICTION_FAILURE              = 81,  ///< Generative model prediction error spike
    PRECISION_REWEIGHT              = 82,  ///< Precision weights recalculated
    BLANKET_RECONFIGURED            = 83,  ///< Markov blanket boundary changed
    MODEL_UPDATE                    = 84,  ///< Generative model parameters updated

    // Civic Angel (85-89)
    CITY_COHERENCE_HIGH             = 85,  ///< Inter-district coherence above threshold
    CITY_COHERENCE_LOW              = 86,  ///< Inter-district coherence below threshold
    CIVIC_ANGEL_OBSERVATION         = 87,  ///< Civic Angel self-model update cycle
    RESOURCE_REALLOCATION           = 88,  ///< City-wide attention reallocation
    ENTELECHY_MILESTONE             = 89,  ///< Developmental entelechy milestone reached
};

} // namespace opencog::endo
