#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for the Virtual Nervous System (VNS)
 *
 * Compact, cache-friendly types for fast neural signaling, routed pathways,
 * and processing level classification. All value types follow the 8-byte
 * alignment pattern established by TruthValue, AttentionValue, and VES types.
 *
 * Design principles:
 * - NeuralSignal and SynapticWeight are 8 bytes (like HormoneLevel)
 * - NeuralState is SIMD-aligned for batch operations
 * - NeuralChannelId count padded to 64 (power-of-2) for AVX2 alignment
 * - Signals are bipolar [-1,+1] (inhibitory/excitatory), unlike VES [0,1]
 * - Decay is fast linear (not exponential), modulated by urgency
 *
 * Relationship to VES:
 *   VES = slow chemical broadcast (32ch, seconds-minutes, [0,1])
 *   VNS = fast electrical routing  (64ch, milliseconds-seconds, [-1,+1])
 *   Coupled via NeuroEndocrineBridge at HYPOTHALAMIC_BRIDGE channel
 */

#include <opencog/core/types.hpp>

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace opencog::nerv {

// ============================================================================
// SIMD alignment constant (matches endo::SIMD_ALIGN)
// ============================================================================

#if defined(__AVX2__) || defined(__AVX__)
inline constexpr size_t SIMD_ALIGN = 32;
#else
inline constexpr size_t SIMD_ALIGN = 16;
#endif

// ============================================================================
// Neural Channel Identifiers
// ============================================================================

/// Virtual neural channels, padded to 64 for SIMD alignment (AVX2: 8×8)
enum class NeuralChannelId : uint8_t {
    // === Sensory Afferents (0-15) ===

    // Exteroceptive (0-7): external input pathways
    VISUAL_PRIMARY      = 0,   ///< Primary visual input (touchpad manifold → V1)
    AUDITORY_PRIMARY    = 1,   ///< Primary auditory input
    SOMATOSENSORY       = 2,   ///< Tactile/proprioceptive (touchpad contacts)
    GESTURAL_AFFERENT   = 3,   ///< Gesture recognition pipeline output
    SEMANTIC_AFFERENT   = 4,   ///< Semantic/linguistic input (NPU output)
    NOVELTY_SIGNAL      = 5,   ///< Novelty detection (from o9c2 emergence)
    THREAT_AFFERENT     = 6,   ///< Threat detection fast path (amygdala shortcut)
    SOCIAL_AFFERENT     = 7,   ///< Social signal input (oxytocin-related)

    // Interoceptive (8-15): internal state sensing
    HOMEOSTATIC_SIGNAL  = 8,   ///< Endocrine state summary (from VES)
    ERROR_SIGNAL        = 9,   ///< Error/mismatch detection
    ENERGY_STATE        = 10,  ///< Resource/energy availability
    AROUSAL_SIGNAL      = 11,  ///< Autonomic arousal level
    PAIN_SIGNAL         = 12,  ///< Damage/overload warning
    REWARD_PREDICTION   = 13,  ///< Predicted reward (from DA system)
    COHERENCE_SIGNAL    = 14,  ///< Cross-system coherence measure
    FATIGUE_SIGNAL      = 15,  ///< Processing fatigue / resource depletion

    // === Interneurons / Processing (16-39) ===

    // Thalamic relay (16-19)
    THALAMIC_GATE       = 16,  ///< Attentional gating signal (ECAN → thalamus)
    THALAMIC_RELAY_1    = 17,  ///< Sensory relay channel 1
    THALAMIC_RELAY_2    = 18,  ///< Sensory relay channel 2
    RETICULAR_ACTIVATION = 19, ///< Reticular activating system (global arousal)

    // Cortical processing (20-27)
    PREFRONTAL_EXEC     = 20,  ///< Prefrontal executive signal (Marduk)
    ASSOCIATION_CORTEX  = 21,  ///< Association cortex integration (LLM NPU)
    PATTERN_CORTEX      = 22,  ///< Pattern matching cortex (Pattern NPU)
    TEMPORAL_CORTEX     = 23,  ///< Temporal processing (sequence/time)
    REASONING_CORTEX    = 24,  ///< Reasoning pathway (PLN)
    WORKING_MEMORY      = 25,  ///< Working memory bus
    LANGUAGE_CORTEX     = 26,  ///< Language processing pathway
    SPATIAL_CORTEX      = 27,  ///< Spatial/topological processing

    // Subcortical processing (28-35)
    HIPPOCAMPAL_ENCODE  = 28,  ///< Memory encoding signal
    HIPPOCAMPAL_RECALL  = 29,  ///< Memory retrieval signal
    AMYGDALA_VALENCE    = 30,  ///< Emotional valence (fast path)
    AMYGDALA_AROUSAL    = 31,  ///< Emotional arousal (fast path)
    BASAL_GANGLIA_GO    = 32,  ///< Action selection GO signal
    BASAL_GANGLIA_NOGO  = 33,  ///< Action inhibition NOGO signal
    CEREBELLAR_PREDICT  = 34,  ///< Prediction/timing signal
    CEREBELLAR_ERROR    = 35,  ///< Prediction error signal

    // Integration (36-39)
    ANTERIOR_CINGULATE  = 36,  ///< Conflict monitoring
    INSULA_INTEROCEPTION = 37, ///< Bodily awareness / felt sense
    HYPOTHALAMIC_BRIDGE = 38,  ///< Neuroendocrine coupling (VNS ↔ VES)
    BRAINSTEM_AUTONOMIC = 39,  ///< Autonomic nervous system output

    // === Motor Efferents (40-55) ===

    // Voluntary (somatic) motor (40-47)
    MOTOR_GESTURE_EMIT  = 40,  ///< Gesture emission command (→ touchpad)
    MOTOR_NPU_COMMAND   = 41,  ///< NPU inference initiation
    MOTOR_TASK_DISPATCH = 42,  ///< Task dispatch (→ Marduk)
    MOTOR_ATTENTION     = 43,  ///< Attentional steering (→ ECAN)
    MOTOR_INFERENCE     = 44,  ///< Inference initiation (→ PLN)
    MOTOR_MEMORY_STORE  = 45,  ///< Memory encoding trigger
    MOTOR_EXPRESSION    = 46,  ///< Communication/expression output
    MOTOR_PERSONA_SHIFT = 47,  ///< Persona transition command (→ o9c2)

    // Autonomic motor (48-55)
    SYMPATHETIC_OUT     = 48,  ///< Sympathetic activation (arousal increase)
    PARASYMPATHETIC_OUT = 49,  ///< Parasympathetic activation (calming)
    ENDOCRINE_NUDGE     = 50,  ///< Direct hormone bus influence
    CIRCADIAN_DRIVE     = 51,  ///< Circadian rhythm output
    IMMUNE_RESPONSE     = 52,  ///< Immune system modulation
    METABOLIC_ADJUST    = 53,  ///< Metabolic rate adjustment
    GROWTH_SIGNAL       = 54,  ///< Long-term adaptation signal
    REPAIR_SIGNAL       = 55,  ///< Maintenance/repair trigger

    // === Reserved (56-63) ===
    RESERVED_56 = 56, RESERVED_57 = 57, RESERVED_58 = 58, RESERVED_59 = 59,
    RESERVED_60 = 60, RESERVED_61 = 61, RESERVED_62 = 62, RESERVED_63 = 63,

    CHANNEL_COUNT = 64  ///< Total channels (power-of-2 for SIMD)
};

inline constexpr size_t NEURAL_CHANNEL_COUNT = 64;

/// Human-readable channel names
[[nodiscard]] constexpr std::string_view channel_name(NeuralChannelId id) noexcept {
    constexpr std::string_view names[] = {
        // Sensory afferents (0-15)
        "Visual", "Auditory", "Somatosensory", "GesturalAff",
        "SemanticAff", "Novelty", "ThreatAff", "SocialAff",
        "Homeostatic", "Error", "Energy", "Arousal",
        "Pain", "RewardPred", "Coherence", "Fatigue",
        // Interneurons (16-39)
        "ThalamicGate", "ThalamicR1", "ThalamicR2", "ReticularAct",
        "PrefrontalExec", "AssociationCtx", "PatternCtx", "TemporalCtx",
        "ReasoningCtx", "WorkingMem", "LanguageCtx", "SpatialCtx",
        "HippEncode", "HippRecall", "AmygdalaVal", "AmygdalaAro",
        "BasalGangliaGo", "BasalGangliaNoGo", "CerebPredict", "CerebError",
        "AntCingulate", "Insula", "HypoBridge", "BrainstemANS",
        // Motor efferents (40-55)
        "MotGesture", "MotNPU", "MotTask", "MotAttention",
        "MotInference", "MotMemStore", "MotExpression", "MotPersona",
        "SympathOut", "ParasympOut", "EndoNudge", "CircadianDrv",
        "ImmuneResp", "MetabolicAdj", "GrowthSig", "RepairSig",
        // Reserved (56-63)
        "Res56", "Res57", "Res58", "Res59",
        "Res60", "Res61", "Res62", "Res63"
    };
    auto idx = static_cast<size_t>(id);
    return idx < NEURAL_CHANNEL_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Neural Signal — 8 bytes (matches HormoneLevel pattern)
// ============================================================================

/**
 * @brief Single neural channel signal
 *
 * Unlike HormoneLevel [0,1], NeuralSignal is bipolar [-1,+1] to support
 * both excitatory and inhibitory signaling. Urgency [0,1] controls how
 * rapidly the signal decays — high urgency = fast transient spike.
 */
struct alignas(8) NeuralSignal {
    float activation{0.0f};   ///< [-1, +1]: inhibitory to excitatory
    float urgency{0.0f};      ///< [0, 1]: temporal priority (decay rate modifier)

    constexpr NeuralSignal() = default;
    constexpr NeuralSignal(float a, float u) : activation(a), urgency(u) {}

    /// Excitatory signal (positive activation)
    [[nodiscard]] static constexpr NeuralSignal excite(float strength = 0.7f,
                                                        float urg = 0.5f) noexcept {
        return {strength, urg};
    }

    /// Inhibitory signal (negative activation)
    [[nodiscard]] static constexpr NeuralSignal inhibit(float strength = 0.7f,
                                                         float urg = 0.5f) noexcept {
        return {-strength, urg};
    }

    /// Spike: high urgency, strong activation
    [[nodiscard]] static constexpr NeuralSignal spike(float strength = 0.9f) noexcept {
        return {strength, 1.0f};
    }

    [[nodiscard]] constexpr float magnitude() const noexcept {
        return std::abs(activation);
    }

    [[nodiscard]] constexpr bool is_excitatory() const noexcept {
        return activation > 0.0f;
    }

    [[nodiscard]] constexpr bool is_inhibitory() const noexcept {
        return activation < 0.0f;
    }

    [[nodiscard]] constexpr bool is_salient(float threshold = 0.1f) const noexcept {
        return std::abs(activation) >= threshold;
    }

    constexpr auto operator<=>(const NeuralSignal&) const = default;
};

static_assert(sizeof(NeuralSignal) == 8);

// ============================================================================
// Neural State — Full snapshot (256 bytes, SIMD-aligned)
// ============================================================================

/// Snapshot of all neural channel activations at a point in time
struct alignas(SIMD_ALIGN) NeuralState {
    std::array<float, NEURAL_CHANNEL_COUNT> activations{};

    [[nodiscard]] float operator[](NeuralChannelId id) const noexcept {
        return activations[static_cast<size_t>(id)];
    }
    float& operator[](NeuralChannelId id) noexcept {
        return activations[static_cast<size_t>(id)];
    }

    /// Euclidean distance to another state (for level classification)
    [[nodiscard]] float distance_to(const NeuralState& other) const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            float d = activations[i] - other.activations[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }

    /// L1 norm (sum of absolute activations)
    [[nodiscard]] float total_activation() const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            sum += std::abs(activations[i]);
        }
        return sum;
    }
};

static_assert(sizeof(NeuralState) == 256);

// ============================================================================
// Synaptic Weight — 8 bytes
// ============================================================================

/// Connection strength between two neural channels
struct alignas(8) SynapticWeight {
    float weight{0.0f};       ///< [-1, +1]: inhibitory to excitatory coupling
    float plasticity{0.0f};   ///< [0, 1]: Hebbian learning rate

    constexpr SynapticWeight() = default;
    constexpr SynapticWeight(float w, float p) : weight(w), plasticity(p) {}

    /// Fixed excitatory connection
    [[nodiscard]] static constexpr SynapticWeight excitatory(float w = 0.5f) noexcept {
        return {w, 0.0f};
    }

    /// Fixed inhibitory connection
    [[nodiscard]] static constexpr SynapticWeight inhibitory(float w = 0.5f) noexcept {
        return {-w, 0.0f};
    }

    /// Plastic connection that learns
    [[nodiscard]] static constexpr SynapticWeight plastic(float w = 0.3f,
                                                           float p = 0.1f) noexcept {
        return {w, p};
    }

    constexpr auto operator<=>(const SynapticWeight&) const = default;
};

static_assert(sizeof(SynapticWeight) == 8);

// ============================================================================
// Processing Level — Emergent neural activation classification
// ============================================================================

/**
 * @brief Hierarchical processing level from neural activation patterns
 *
 * Analogous to CognitiveMode in VES, but reflects the anatomical hierarchy
 * of the nervous system rather than hormonal constellations:
 *   SPINAL_REFLEX → fastest, direct sensory→motor arc (1-2 ticks)
 *   BRAINSTEM     → fast, autonomic regulation (3-5 ticks)
 *   LIMBIC        → medium, emotional processing (5-10 ticks)
 *   CORTICAL      → slow, deliberate reasoning (10+ ticks)
 */
enum class ProcessingLevel : uint8_t {
    SPINAL_REFLEX   = 0,  ///< Direct sensory→motor, minimal processing
    BRAINSTEM       = 1,  ///< Autonomic regulation, arousal control
    LIMBIC          = 2,  ///< Emotional evaluation, memory encoding
    CORTICAL        = 3,  ///< Deliberate reasoning, executive control
};

[[nodiscard]] constexpr std::string_view level_name(ProcessingLevel level) noexcept {
    constexpr std::string_view names[] = {
        "SpinalReflex", "Brainstem", "Limbic", "Cortical"
    };
    auto idx = static_cast<size_t>(level);
    return idx < 4 ? names[idx] : "Unknown";
}

inline constexpr size_t PROCESSING_LEVEL_COUNT = 4;

// ============================================================================
// Cosmos S5 Polarity — Neural pathway classification
// ============================================================================

/// Cosmos System 5 neural polarity types
enum class NeuralPolarity : uint8_t {
    SYMPATHETIC     = 0,  ///< Fast, event-driven (fight-or-flight)
    PARASYMPATHETIC = 1,  ///< Slow, homeostatic (rest-and-digest)
    SOMATIC         = 2,  ///< Voluntary, deliberate (conscious control)
};

[[nodiscard]] constexpr std::string_view polarity_name(NeuralPolarity p) noexcept {
    constexpr std::string_view names[] = {
        "Sympathetic", "Parasympathetic", "Somatic"
    };
    return names[static_cast<size_t>(p)];
}

// ============================================================================
// Nucleus Configuration
// ============================================================================

struct NucleusConfig {
    std::string name;
    NeuralChannelId output_channel{NeuralChannelId::THALAMIC_GATE};

    float base_output{0.0f};
    float max_output{1.0f};
    float threshold{0.1f};       ///< Minimum input to produce output

    /// Input channels that excite this nucleus: (channel, sensitivity)
    std::vector<std::pair<NeuralChannelId, float>> excitatory_inputs;
    /// Input channels that inhibit this nucleus: (channel, sensitivity)
    std::vector<std::pair<NeuralChannelId, float>> inhibitory_inputs;

    /// Self-regulation: own output inhibits own activity
    bool self_inhibition{true};
    float self_inhibition_gain{0.3f};

    /// Adaptation rate (allostatic-like threshold drift)
    float adaptation_rate{0.002f};
};

// ============================================================================
// NerveBus Configuration
// ============================================================================

struct NerveBusConfig {
    float tick_interval_ms{10.0f};       ///< Nominal update interval (10x faster than VES)
    float global_decay_rate{0.3f};       ///< Base linear decay per tick
    bool enable_simd{true};              ///< Use SIMD for batch operations
    size_t history_length{100};          ///< Ring buffer size (shorter than VES's 1000)
    bool enable_propagation{true};       ///< Apply connectivity matrix routing
};

// ============================================================================
// Neural Event — Signals that trigger neural cascades
// ============================================================================

enum class NeuralEvent : uint8_t {
    // Sensory events (0-9)
    SENSORY_INPUT_RECEIVED      = 0,
    GESTURE_DETECTED            = 1,
    THREAT_PERCEIVED            = 2,
    NOVELTY_DETECTED            = 3,
    SOCIAL_SIGNAL_RECEIVED      = 4,

    // Processing events (10-19)
    ATTENTION_SHIFT             = 10,
    MEMORY_ENCODED              = 11,
    MEMORY_RECALLED             = 12,
    PREDICTION_ERROR            = 13,
    CONFLICT_DETECTED           = 14,

    // Motor events (20-29)
    ACTION_INITIATED            = 20,
    TASK_DISPATCHED             = 21,
    INFERENCE_STARTED           = 22,
    EXPRESSION_EMITTED          = 23,
    PERSONA_TRANSITION          = 24,

    // Autonomic events (30-39)
    SYMPATHETIC_ACTIVATION      = 30,
    PARASYMPATHETIC_ACTIVATION  = 31,
    REFLEX_ARC_TRIGGERED        = 32,
    LEVEL_TRANSITION            = 33,
    NEUROENDOCRINE_SYNC         = 34,

    // System events (40-49)
    PROCESSING_OVERLOAD         = 40,
    FATIGUE_THRESHOLD_REACHED   = 41,
    COHERENCE_RESTORED          = 42,
    HOMEOSTASIS_ACHIEVED        = 43,
};

// ============================================================================
// Reflex Arc — Direct sensory→motor shortcut
// ============================================================================

struct ReflexArc {
    NeuralChannelId sensory_channel;
    NeuralChannelId motor_channel;
    float threshold{0.5f};    ///< Minimum sensory activation to trigger
    float gain{0.8f};         ///< Motor response strength
    float latency_ticks{1};   ///< Processing delay (1 = next tick)
};

} // namespace opencog::nerv
