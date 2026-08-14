#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for Modern OpenCog
 *
 * Design principles:
 * - Cache-line aware memory layout (64 bytes)
 * - Data-oriented design (Structure of Arrays)
 * - Lock-free where possible
 * - SIMD-friendly data alignment
 */

#include <cstdint>
#include <atomic>
#include <bit>
#include <compare>
#include <concepts>
#include <limits>
#include <span>
#include <string_view>

namespace opencog {

// ============================================================================
// Fundamental Types
// ============================================================================

/// Unique atom identifier - 48 bits usable, 16 bits for type
struct AtomId {
    uint64_t value{0};

    constexpr AtomId() = default;
    constexpr explicit AtomId(uint64_t v) : value(v) {}

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    constexpr auto operator<=>(const AtomId&) const = default;

    // Extract components
    [[nodiscard]] constexpr uint64_t index() const noexcept { return value & 0x0000FFFFFFFFFFFF; }
    [[nodiscard]] constexpr uint16_t generation() const noexcept { return static_cast<uint16_t>(value >> 48); }

    // Create from components (for slot reuse with generation tracking)
    [[nodiscard]] static constexpr AtomId make(uint64_t idx, uint16_t gen) noexcept {
        return AtomId{(static_cast<uint64_t>(gen) << 48) | (idx & 0x0000FFFFFFFFFFFF)};
    }
};

static_assert(sizeof(AtomId) == 8);

/// Null atom constant
inline constexpr AtomId ATOM_NULL{0};

// ============================================================================
// Atom Types - Extensible via enum
// ============================================================================

enum class AtomType : uint16_t {
    // Special
    INVALID = 0,

    // Nodes (1-999)
    NODE = 1,
    CONCEPT_NODE = 2,
    PREDICATE_NODE = 3,
    VARIABLE_NODE = 4,
    NUMBER_NODE = 5,
    TYPE_NODE = 6,
    GROUNDED_OBJECT_NODE = 7,
    ANCHOR_NODE = 8,
    SCHEMA_NODE = 9,
    GROUNDED_SCHEMA_NODE = 10,
    DEFINED_SCHEMA_NODE = 11,

    // Links (1000+)
    LINK = 1000,
    ORDERED_LINK = 1001,
    UNORDERED_LINK = 1002,
    LIST_LINK = 1003,       ///< Ordered list of atoms (standard Atomese)
    SET_LINK = 1004,        ///< Unordered set of atoms

    // Logical Links
    AND_LINK = 1010,
    OR_LINK = 1011,
    NOT_LINK = 1012,

    // Inheritance/Similarity
    INHERITANCE_LINK = 1020,
    SIMILARITY_LINK = 1021,
    SUBSET_LINK = 1022,
    INTENSIONAL_INHERITANCE_LINK = 1023,
    MEMBER_LINK = 1024,     ///< Set membership (MemberLink A B: A is member of B)

    // Evaluation
    EVALUATION_LINK = 1030,
    EXECUTION_LINK = 1031,
    EXECUTION_OUTPUT_LINK = 1032,

    // Variables and Scoping
    SCOPE_LINK = 1040,
    BIND_LINK = 1041,
    GET_LINK = 1042,
    PUT_LINK = 1043,
    LAMBDA_LINK = 1044,

    // Context/State
    CONTEXT_LINK = 1050,
    STATE_LINK = 1051,
    DEFINE_LINK = 1052,

    // Temporal
    AT_TIME_LINK = 1060,
    TIME_INTERVAL_LINK = 1061,
    BEFORE_LINK = 1062,
    OVERLAPS_LINK = 1063,

    // PLN-specific
    IMPLICATION_LINK = 1100,
    EQUIVALENCE_LINK = 1101,
    FORALL_LINK = 1102,
    EXISTS_LINK = 1103,
    AVERAGE_LINK = 1104,

    // User-defined types start here
    USER_DEFINED = 10000,

    // Endocrine System Nodes (10000-10099)
    HORMONE_NODE = 10000,            ///< Named hormone channel
    GLAND_NODE = 10001,              ///< Named virtual gland
    EPISODE_NODE = 10002,            ///< Unique episode identifier
    VALENCE_ANCHOR_NODE = 10003,     ///< Anchor for valence-tagged atoms
    FELT_SENSE_NODE = 10004,         ///< Introspective felt-state label
    MORAL_SIGNAL_NODE = 10005,       ///< Named moral salience channel

    // Endocrine System Links (10500-10599)
    VALENCE_LINK = 10500,            ///< (ValenceLink <atom> <valence-anchor>)
    EPISODE_LINK = 10501,            ///< (EpisodeLink <episode-node> <atom>)
    TEMPORAL_VALENCE_LINK = 10502,   ///< (TemporalValenceLink <episode> <atom> <time>)
    CAUSAL_LINK = 10503,             ///< (CausalLink <cause-episode> <effect-episode>)
    HORMONE_MODULATION_LINK = 10504, ///< (HormoneModulationLink <hormone> <target>)
    AFFECTIVE_LINK = 10505,          ///< (AffectiveLink <felt-sense> <atom>)
    MORAL_EVALUATION_LINK = 10506,   ///< (MoralEvaluationLink <moral-signal> <situation>)
    EMPATHY_LINK = 10507,            ///< (EmpathyLink <self-state> <other-model>)

    // Integration Nodes (10010-10019) — NPU, o9c2, Guidance, Marduk
    NPU_STATE_NODE = 10010,          ///< NPU telemetry/state snapshot
    O9C2_STATE_NODE = 10011,         ///< o9c2 persona/emergence snapshot
    GUIDANCE_NODE = 10012,           ///< External guidance directive
    PERSONA_NODE = 10013,            ///< Named o9c2 persona configuration
    MARDUK_STATE_NODE = 10014,       ///< Marduk operational state snapshot
    MARDUK_MEMORY_NODE = 10015,      ///< Marduk memory subsystem state
    MARDUK_TASK_NODE = 10016,        ///< Marduk task queue / execution state
    MARDUK_AUTONOMY_NODE = 10017,    ///< Marduk autonomy optimizer state
    TOUCHPAD_STATE_NODE = 10018,     ///< VirtualTouchpad operational state snapshot
    TOUCHPAD_GESTURE_NODE = 10019,   ///< Gesture type/event node
    TOUCHPAD_CONTACT_NODE = 10020,   ///< Contact point in manifold
    TOUCHPAD_MANIFOLD_NODE = 10021,  ///< Manifold topology/geometry state

    // Integration Links (10510-10519)
    NPU_MODULATION_LINK = 10510,     ///< (NPUModulationLink <npu-state> <hormone>)
    PERSONA_LINK = 10511,            ///< (PersonaLink <persona-node> <o9c2-state>)
    GUIDANCE_LINK = 10512,           ///< (GuidanceLink <guidance-node> <target>)
    EMERGENCE_LINK = 10513,          ///< (EmergenceLink <metric-node> <value>)
    MARDUK_MODULATION_LINK = 10514,  ///< (MardukModulationLink <marduk-state> <hormone>)
    MARDUK_MEMORY_LINK = 10515,      ///< (MardukMemoryLink <memory-node> <target>)
    MARDUK_TASK_LINK = 10516,        ///< (MardukTaskLink <task-node> <execution>)
    MARDUK_FEEDBACK_LINK = 10517,    ///< (MardukFeedbackLink <metric> <bus-channel>)
    TOUCHPAD_MODULATION_LINK = 10518, ///< (TouchpadModulationLink <touchpad-state> <hormone>)
    TOUCHPAD_GESTURE_LINK = 10519,   ///< (TouchpadGestureLink <gesture-node> <contact>)
    TOUCHPAD_FEEDBACK_LINK = 10520,  ///< (TouchpadFeedbackLink <metric> <bus-channel>)
    TOUCHPAD_MANIFOLD_LINK = 10521,  ///< (TouchpadManifoldLink <manifold-node> <topology>)

    // Nervous System Nodes (10100-10108)
    NEURAL_CHANNEL_NODE    = 10100,  ///< Named neural channel
    NEURAL_NUCLEUS_NODE    = 10101,  ///< Named neural processing nucleus
    NEURAL_PATHWAY_NODE    = 10102,  ///< Named neural pathway
    SYNAPSE_NODE           = 10103,  ///< Synaptic connection point
    REFLEX_ARC_NODE        = 10104,  ///< Named reflex arc
    PROCESSING_LEVEL_NODE  = 10105,  ///< Processing level state
    NEURAL_PATTERN_NODE    = 10106,  ///< Recognized neural activation pattern
    SENSORY_INPUT_NODE     = 10107,  ///< Sensory input source
    MOTOR_OUTPUT_NODE      = 10108,  ///< Motor output target

    // Nervous System Links (10600-10609)
    NEURAL_PATHWAY_LINK    = 10600,  ///< (NeuralPathwayLink <from-channel> <to-channel>)
    SYNAPTIC_LINK          = 10601,  ///< (SynapticLink <pre-synaptic> <post-synaptic>)
    REFLEX_LINK            = 10602,  ///< (ReflexLink <sensory> <motor> <latency>)
    NEUROENDOCRINE_LINK    = 10603,  ///< (NeuroEndocrineLink <neural-channel> <hormone>)
    SENSORY_LINK           = 10604,  ///< (SensoryLink <input-source> <neural-channel>)
    MOTOR_LINK             = 10605,  ///< (MotorLink <neural-channel> <output-target>)
    NEURAL_MODULATION_LINK = 10606,  ///< (NeuralModulationLink <nucleus> <target-channel>)
    NEURAL_FEEDBACK_LINK   = 10607,  ///< (NeuralFeedbackLink <metric> <bus-channel>)
    PLASTICITY_LINK        = 10608,  ///< (PlasticityLink <synapse> <learning-rule>)
    INTEGRATION_LINK       = 10609,  ///< (IntegrationLink <vns-state> <ves-state>)

    // Temporal Crystal System Nodes (10200-10209)
    TEMPORAL_CRYSTAL_NODE    = 10200,  ///< Time crystal neuron instance
    OSCILLATOR_NODE          = 10201,  ///< Individual oscillator at a temporal scale
    CRYSTAL_DOMAIN_NODE      = 10202,  ///< TCN spatial domain (Perception/Processing/Action)
    CRYSTAL_LAYER_NODE       = 10203,  ///< TCN layer within a domain
    TEMPORAL_SCALE_NODE      = 10204,  ///< Named temporal scale (8ms–1s)
    BRAIN_REGION_NODE        = 10205,  ///< TCNN brain region (Microtubule, Cortex, etc.)
    BRAIN_SUBSYSTEM_NODE     = 10206,  ///< TCNN functional subsystem (Cognitive, Emotion, etc.)
    HIERARCHY_LEVEL_NODE     = 10207,  ///< TCNN hierarchy level (12 levels)
    PHASE_PATTERN_NODE       = 10208,  ///< Recognized phase coherence pattern
    COGNITIVE_PROCESS_NODE   = 10209,  ///< Named cognitive process at a temporal scale

    // Temporal Crystal System Links (10700-10709)
    PHASE_COUPLING_LINK        = 10700,  ///< (PhaseCouplingLink <oscillator> <oscillator> <strength>)
    TEMPORAL_HIERARCHY_LINK    = 10701,  ///< (TemporalHierarchyLink <parent-scale> <child-scale>)
    OSCILLATOR_MODULATION_LINK = 10702,  ///< (OscillatorModulationLink <source> <oscillator>)
    CRYSTAL_PATHWAY_LINK       = 10703,  ///< (CrystalPathwayLink <domain> <domain>)
    BRAIN_REGION_LINK          = 10704,  ///< (BrainRegionLink <region> <component>)
    SUBSYSTEM_LINK             = 10705,  ///< (SubsystemLink <subsystem> <region>)
    TEMPORAL_FEEDBACK_LINK     = 10706,  ///< (TemporalFeedbackLink <metric> <bus-channel>)
    CRYSTAL_ENDOCRINE_LINK     = 10707,  ///< (CrystalEndocrineLink <crystal-state> <hormone>)
    CRYSTAL_NEURAL_LINK        = 10708,  ///< (CrystalNeuralLink <crystal-state> <neural-channel>)
    RESONANCE_LINK             = 10709,  ///< (ResonanceLink <phase-pattern> <cognitive-process>)

    // Entelechy System Nodes (10300-10315)
    TEMPERAMENT_NODE           = 10300,  ///< Cloninger temperament/character profile
    CHARACTER_NODE             = 10301,  ///< Developed character dimension state
    INTEROCEPTIVE_NODE         = 10302,  ///< Interoceptive body-state representation
    DEVELOPMENTAL_NODE         = 10303,  ///< Developmental stage/trajectory state
    NARRATIVE_NODE             = 10304,  ///< Narrative chapter/theme node
    SOCIAL_ROLE_NODE           = 10305,  ///< Social role identity
    ATTACHMENT_NODE            = 10306,  ///< Attachment style/pattern
    DISTRICT_NODE              = 10307,  ///< Cognitive district (subsystem wrapper)
    CIVIC_ANGEL_NODE           = 10308,  ///< Emergent self-model (Civic Angel)
    ENTELECHY_NODE             = 10309,  ///< Ontogenetic entelechy state
    CRITICAL_PERIOD_NODE       = 10310,  ///< Developmental critical period
    TRAUMA_NODE                = 10311,  ///< Trauma record
    LIFE_THEME_NODE            = 10312,  ///< Narrative life theme
    POLYVAGAL_STATE_NODE       = 10313,  ///< Polyvagal tier state
    ALLOSTATIC_NODE            = 10314,  ///< Allostatic load state
    SELF_MODEL_NODE            = 10315,  ///< Self-model representation

    // Active Free-energy Inference Nodes (10320-10324)
    MARKOV_BLANKET_NODE        = 10320,  ///< Markov blanket boundary definition
    GENERATIVE_MODEL_NODE      = 10321,  ///< Generative model state
    FREE_ENERGY_NODE           = 10322,  ///< Free energy decomposition
    PRECISION_NODE             = 10323,  ///< Precision weight node
    PREDICTION_ERROR_NODE      = 10324,  ///< Prediction error signal

    // Entelechy System Links (10800-10814)
    TEMPERAMENT_LINK           = 10800,  ///< (TemperamentLink <temperament> <gain-target>)
    DEVELOPMENTAL_LINK         = 10801,  ///< (DevelopmentalLink <stage> <transition>)
    NARRATIVE_LINK             = 10802,  ///< (NarrativeLink <chapter> <episode>)
    SOCIAL_ROLE_LINK           = 10803,  ///< (SocialRoleLink <role> <identity>)
    ATTACHMENT_LINK            = 10804,  ///< (AttachmentLink <style> <pattern>)
    DISTRICT_BOUNDARY_LINK     = 10805,  ///< (DistrictBoundaryLink <district> <blanket>)
    CIVIC_GOVERNANCE_LINK      = 10806,  ///< (CivicGovernanceLink <angel> <directive>)
    ENTELECHY_PROGRESS_LINK    = 10807,  ///< (EntelechyProgressLink <entelechy> <milestone>)
    INTEROCEPTIVE_LINK         = 10808,  ///< (InteroceptiveLink <body-state> <signal>)
    TRAUMA_ENCODING_LINK       = 10809,  ///< (TraumaEncodingLink <trauma> <memory>)
    LIFE_THEME_LINK            = 10810,  ///< (LifeThemeLink <theme> <chapter>)
    CRITICAL_PERIOD_LINK       = 10811,  ///< (CriticalPeriodLink <period> <dimension>)
    POLYVAGAL_LINK             = 10812,  ///< (PolyvagalLink <tier> <autonomic-state>)
    SELF_MODEL_LINK            = 10813,  ///< (SelfModelLink <model> <district>)
    ALLOSTATIC_LINK            = 10814,  ///< (AllostaticLink <load> <stressor>)

    // Active Free-energy Inference Links (10820-10823)
    BLANKET_BOUNDARY_LINK      = 10820,  ///< (BlanketBoundaryLink <blanket> <state>)
    FREE_ENERGY_LINK           = 10821,  ///< (FreeEnergyLink <energy> <district>)
    PRECISION_WEIGHTING_LINK   = 10822,  ///< (PrecisionWeightingLink <precision> <signal>)
    PREDICTION_ERROR_LINK      = 10823,  ///< (PredictionErrorLink <error> <model>)
};

[[nodiscard]] constexpr bool is_node(AtomType t) noexcept {
    return static_cast<uint16_t>(t) > 0 && static_cast<uint16_t>(t) < 1000;
}

[[nodiscard]] constexpr bool is_link(AtomType t) noexcept {
    return static_cast<uint16_t>(t) >= 1000;
}

// ============================================================================
// Truth Value - Compact representation (8 bytes)
// ============================================================================

/**
 * @brief Probabilistic truth value using half-precision floats
 *
 * Uses 16-bit floats for strength and confidence to fit in 8 bytes total.
 * For operations requiring precision, convert to float32.
 */
struct alignas(8) TruthValue {
    // Using float for now, can switch to _Float16 on supporting compilers
    float strength{0.0f};      ///< Probability/certainty [0,1]
    float confidence{0.0f};    ///< Meta-certainty about strength [0,1]

    constexpr TruthValue() = default;
    constexpr TruthValue(float s, float c) : strength(s), confidence(c) {}

    // Named constructors
    [[nodiscard]] static constexpr TruthValue simple(float s) noexcept {
        return TruthValue{s, 0.9f};
    }

    [[nodiscard]] static constexpr TruthValue default_tv() noexcept {
        return TruthValue{1.0f, 0.0f};
    }

    [[nodiscard]] static constexpr TruthValue true_tv() noexcept {
        return TruthValue{1.0f, 0.9f};
    }

    [[nodiscard]] static constexpr TruthValue false_tv() noexcept {
        return TruthValue{0.0f, 0.9f};
    }

    // Semantic queries
    [[nodiscard]] constexpr bool is_true(float threshold = 0.5f) const noexcept {
        return strength >= threshold;
    }

    [[nodiscard]] constexpr bool is_confident(float threshold = 0.5f) const noexcept {
        return confidence >= threshold;
    }

    // Count representation (for PLN formulas)
    [[nodiscard]] float count() const noexcept {
        // K is a scaling constant, typically 800
        constexpr float K = 800.0f;
        return confidence * K / (1.0f - confidence + 1e-10f);
    }

    [[nodiscard]] static TruthValue from_count(float s, float n) noexcept {
        constexpr float K = 800.0f;
        return TruthValue{s, n / (n + K)};
    }

    constexpr auto operator<=>(const TruthValue&) const = default;
};

static_assert(sizeof(TruthValue) == 8);

// ============================================================================
// Attention Value - Compact representation (8 bytes)
// ============================================================================

/**
 * @brief Economic Attention Network values
 *
 * STI: Short-Term Importance - current focus
 * LTI: Long-Term Importance - sustained relevance
 * VLTI: Very Long-Term Importance flag - prevents forgetting
 */
struct alignas(8) AttentionValue {
    float sti{0.0f};           ///< Short-term importance [-1, 1] normalized
    int16_t lti{0};            ///< Long-term importance (integer for atomics)
    uint8_t vlti{0};           ///< Very long-term importance flag
    uint8_t _pad{0};           ///< Padding for alignment

    constexpr AttentionValue() = default;
    constexpr AttentionValue(float s, int16_t l, uint8_t v = 0)
        : sti(s), lti(l), vlti(v) {}

    [[nodiscard]] static constexpr AttentionValue default_av() noexcept {
        return AttentionValue{0.0f, 0, 0};
    }

    [[nodiscard]] static constexpr AttentionValue disposable() noexcept {
        return AttentionValue{-1.0f, 0, 0};
    }

    [[nodiscard]] constexpr bool is_disposable() const noexcept {
        return vlti == 0 && sti < -0.9f;
    }

    [[nodiscard]] constexpr bool in_attentional_focus(float threshold = 0.0f) const noexcept {
        return sti >= threshold;
    }
};

static_assert(sizeof(AttentionValue) == 8);

// ============================================================================
// Atom Header - Minimal inline metadata (16 bytes)
// ============================================================================

/**
 * @brief Compact atom header stored inline in atom table
 *
 * Contains only the most frequently accessed data.
 * Extended data (name, outgoing set) stored separately.
 */
struct alignas(16) AtomHeader {
    AtomType type{AtomType::INVALID};
    uint16_t flags{0};
    uint32_t incoming_count{0};  // Number of links pointing to this atom
    uint64_t hash{0};            // Precomputed hash for fast lookup

    // Flag bits
    static constexpr uint16_t FLAG_CHECKED = 1 << 0;
    static constexpr uint16_t FLAG_MARKED = 1 << 1;
    static constexpr uint16_t FLAG_EXTERNAL = 1 << 2;

    [[nodiscard]] constexpr bool has_flag(uint16_t f) const noexcept {
        return (flags & f) != 0;
    }

    constexpr void set_flag(uint16_t f) noexcept { flags |= f; }
    constexpr void clear_flag(uint16_t f) noexcept { flags &= ~f; }
};

static_assert(sizeof(AtomHeader) == 16);

// ============================================================================
// Concepts for generic programming
// ============================================================================

template<typename T>
concept AtomLike = requires(T t) {
    { t.id() } -> std::convertible_to<AtomId>;
    { t.type() } -> std::convertible_to<AtomType>;
};

template<typename T>
concept TruthValued = requires(T t) {
    { t.truth_value() } -> std::convertible_to<TruthValue>;
};

template<typename T>
concept AttentionValued = requires(T t) {
    { t.attention_value() } -> std::convertible_to<AttentionValue>;
};

template<typename T>
concept HasOutgoing = requires(T t) {
    { t.outgoing() } -> std::convertible_to<std::span<const AtomId>>;
    { t.arity() } -> std::convertible_to<size_t>;
};

// ============================================================================
// Handle - Reference-counted atom pointer (for external use)
// ============================================================================

class AtomSpace;  // Forward declaration

/**
 * @brief Safe handle to an atom in an AtomSpace
 *
 * Handles are lightweight (16 bytes) and can be freely copied.
 * They maintain validity through generation checking.
 */
class Handle {
    AtomId id_{};
    AtomSpace* space_{nullptr};

public:
    constexpr Handle() = default;
    constexpr Handle(AtomId id, AtomSpace* space) : id_(id), space_(space) {}

    [[nodiscard]] constexpr AtomId id() const noexcept { return id_; }
    [[nodiscard]] constexpr AtomSpace* space() const noexcept { return space_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return id_.valid() && space_; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    constexpr auto operator<=>(const Handle&) const = default;
};

static_assert(sizeof(Handle) == 16);

// ============================================================================
// Utility functions
// ============================================================================

/// Fast hash combining (like boost::hash_combine)
[[nodiscard]] constexpr uint64_t hash_combine(uint64_t seed, uint64_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

/// Type name lookup (defined in types.cpp)
[[nodiscard]] std::string_view type_name(AtomType type) noexcept;

/// Parse type from name
[[nodiscard]] AtomType type_from_name(std::string_view name) noexcept;

} // namespace opencog

// Standard library specializations
template<>
struct std::hash<opencog::AtomId> {
    [[nodiscard]] size_t operator()(const opencog::AtomId& id) const noexcept {
        return std::hash<uint64_t>{}(id.value);
    }
};

template<>
struct std::hash<opencog::Handle> {
    [[nodiscard]] size_t operator()(const opencog::Handle& h) const noexcept {
        return std::hash<uint64_t>{}(h.id().value);
    }
};
