#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for the Temporal Crystal System (TCS)
 *
 * Implements the Time Crystal Neuron (TCN) and Time Crystal Neural Network
 * Brain (TCNN) models from Nanobrain (Bandyopadhyay), mapping hierarchical
 * nested oscillators to cognitive processes within the opencog-modern
 * architecture.
 *
 * Design principles:
 * - CrystalPhase and OscillatorLevel are 8 bytes (like TruthValue)
 * - OscillatorState is SIMD-aligned for batch operations
 * - TemporalScaleId count padded to 16 (power-of-2) for SIMD alignment
 * - Oscillators are bipolar [-1,+1] (phase angle mapped to amplitude)
 * - Phase coupling via Phase Prime Metric (PPM)
 *
 * Relationship to VES/VNS:
 *   VES = slow chemical broadcast (32ch, seconds-minutes, [0,1])
 *   VNS = fast electrical routing  (64ch, milliseconds-seconds, [-1,+1])
 *   TCS = multi-scale oscillatory  (16ch, 8ms-1s, [-1,+1], phase-coupled)
 *   All three coupled via bidirectional adapters
 */

#include <opencog/core/types.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

namespace opencog::temporal {

// ============================================================================
// SIMD alignment constant (matches endo::SIMD_ALIGN, nerv::SIMD_ALIGN)
// ============================================================================

#if defined(__AVX2__) || defined(__AVX__)
inline constexpr size_t SIMD_ALIGN = 32;
#else
inline constexpr size_t SIMD_ALIGN = 16;
#endif

// ============================================================================
// Temporal Scale Identifiers — 9 biological + 7 reserved = 16 (SIMD)
// ============================================================================

/**
 * @brief Temporal oscillation scales from Nanobrain Figure 6.14
 *
 * Nine hierarchical temporal scales spanning 8ms to 1s, padded to 16
 * for SIMD alignment. Each scale represents a characteristic oscillation
 * period of biological neuron components.
 *
 * sys-n mapping: scales 0-6 (< 0.5s) = Particular Sets (local processing)
 *                scales 7-8 (>= 0.5s) = Universal Sets (global state)
 */
enum class TemporalScaleId : uint8_t {
    // Biological temporal scales (0-8)
    ULTRA_FAST    = 0,   ///< 8ms   — Protein dynamics / spike propagation
    FAST          = 1,   ///< 26ms  — Ion channel gating / synaptic gating
    MEDIUM_FAST   = 2,   ///< 52ms  — Membrane dynamics / feature binding
    MEDIUM        = 3,   ///< 110ms — Axon initial segment / decision threshold
    MEDIUM_SLOW   = 4,   ///< 160ms — Dendritic integration / context assembly
    SLOW          = 5,   ///< 250ms — Synaptic plasticity / learning update
    VERY_SLOW     = 6,   ///< 330ms — Soma processing / concept formation
    ULTRA_SLOW    = 7,   ///< 500ms — Network synchronization / working memory
    SLOWEST       = 8,   ///< 1000ms— Global rhythm / meta-cognition

    // Reserved (9-15)
    RESERVED_9  = 9,  RESERVED_10 = 10, RESERVED_11 = 11,
    RESERVED_12 = 12, RESERVED_13 = 13, RESERVED_14 = 14,
    RESERVED_15 = 15,

    SCALE_COUNT = 16    ///< Total scales (power-of-2 for SIMD)
};

inline constexpr size_t TEMPORAL_SCALE_COUNT = 16;
inline constexpr size_t BIOLOGICAL_SCALE_COUNT = 9;

/// Period in seconds for each biological temporal scale
inline constexpr std::array<float, BIOLOGICAL_SCALE_COUNT> SCALE_PERIODS = {
    0.008f,   // ULTRA_FAST
    0.026f,   // FAST
    0.052f,   // MEDIUM_FAST
    0.110f,   // MEDIUM
    0.160f,   // MEDIUM_SLOW
    0.250f,   // SLOW
    0.330f,   // VERY_SLOW
    0.500f,   // ULTRA_SLOW
    1.000f    // SLOWEST
};

/// Frequency in Hz for each scale (1/period)
inline constexpr std::array<float, BIOLOGICAL_SCALE_COUNT> SCALE_FREQUENCIES = {
    125.0f,   // ULTRA_FAST
    38.46f,   // FAST
    19.23f,   // MEDIUM_FAST
    9.09f,    // MEDIUM
    6.25f,    // MEDIUM_SLOW
    4.0f,     // SLOW
    3.03f,    // VERY_SLOW
    2.0f,     // ULTRA_SLOW
    1.0f      // SLOWEST
};

/// sys-n boundary: scales below this index are Particular Sets (fast, local)
inline constexpr uint8_t UNIVERSAL_SET_THRESHOLD = 7;  // ULTRA_SLOW and above

/// Get period for a scale (returns 0 for reserved scales)
[[nodiscard]] constexpr float scale_period(TemporalScaleId id) noexcept {
    auto idx = static_cast<size_t>(id);
    return idx < BIOLOGICAL_SCALE_COUNT ? SCALE_PERIODS[idx] : 0.0f;
}

/// Whether this scale is a Universal Set (slow oscillator, global state)
[[nodiscard]] constexpr bool is_universal_set(TemporalScaleId id) noexcept {
    return static_cast<uint8_t>(id) >= UNIVERSAL_SET_THRESHOLD &&
           static_cast<uint8_t>(id) < BIOLOGICAL_SCALE_COUNT;
}

/// Whether this scale is a Particular Set (fast oscillator, local processing)
[[nodiscard]] constexpr bool is_particular_set(TemporalScaleId id) noexcept {
    return static_cast<uint8_t>(id) < UNIVERSAL_SET_THRESHOLD;
}

/// Human-readable scale names
[[nodiscard]] constexpr std::string_view scale_name(TemporalScaleId id) noexcept {
    constexpr std::string_view names[] = {
        "UltraFast(8ms)", "Fast(26ms)", "MediumFast(52ms)",
        "Medium(110ms)", "MediumSlow(160ms)", "Slow(250ms)",
        "VerySlow(330ms)", "UltraSlow(500ms)", "Slowest(1s)",
        "Res9", "Res10", "Res11", "Res12", "Res13", "Res14", "Res15"
    };
    auto idx = static_cast<size_t>(id);
    return idx < TEMPORAL_SCALE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Crystal Phase — 8 bytes (like TruthValue)
// ============================================================================

/**
 * @brief Phase state of a single temporal oscillator
 *
 * Represents the current phase angle and amplitude of an oscillator
 * at a specific temporal scale. Phase is normalized to [0, 2*pi),
 * amplitude is bipolar [-1, +1].
 *
 * The Phase Prime Metric (PPM) from Nanobrain Ch.3 links temporal
 * patterns to prime number structures; phase relationships between
 * scales encode information via harmonic coupling.
 */
struct alignas(8) CrystalPhase {
    float phase{0.0f};       ///< [0, 2*pi): current phase angle
    float amplitude{0.0f};   ///< [-1, +1]: current oscillation amplitude

    constexpr CrystalPhase() = default;
    constexpr CrystalPhase(float p, float a) : phase(p), amplitude(a) {}

    /// Create from frequency and time
    [[nodiscard]] static CrystalPhase from_time(float frequency, float time,
                                                  float amp = 1.0f) noexcept {
        float p = std::fmod(2.0f * std::numbers::pi_v<float> * frequency * time,
                           2.0f * std::numbers::pi_v<float>);
        return {p, amp};
    }

    /// Peak excitatory phase
    [[nodiscard]] static constexpr CrystalPhase peak() noexcept {
        return {0.0f, 1.0f};
    }

    /// Trough inhibitory phase
    [[nodiscard]] static constexpr CrystalPhase trough() noexcept {
        return {std::numbers::pi_v<float>, -1.0f};
    }

    /// Silent / resting
    [[nodiscard]] static constexpr CrystalPhase silent() noexcept {
        return {0.0f, 0.0f};
    }

    /// Current instantaneous value (amplitude * sin(phase))
    [[nodiscard]] float value() const noexcept {
        return amplitude * std::sin(phase);
    }

    /// Phase difference to another oscillator (for coupling)
    [[nodiscard]] float phase_diff(const CrystalPhase& other) const noexcept {
        float diff = phase - other.phase;
        // Normalize to [-pi, pi]
        while (diff > std::numbers::pi_v<float>) diff -= 2.0f * std::numbers::pi_v<float>;
        while (diff < -std::numbers::pi_v<float>) diff += 2.0f * std::numbers::pi_v<float>;
        return diff;
    }

    /// Phase coherence [0,1] — 1.0 = perfectly in phase
    [[nodiscard]] float coherence_with(const CrystalPhase& other) const noexcept {
        float diff = std::abs(phase_diff(other));
        return 1.0f - diff / std::numbers::pi_v<float>;
    }

    constexpr auto operator<=>(const CrystalPhase&) const = default;
};

static_assert(sizeof(CrystalPhase) == 8);

// ============================================================================
// Oscillator Level — 8 bytes (like HormoneLevel / NeuralSignal)
// ============================================================================

/**
 * @brief State of a single oscillator component
 *
 * Combines current activation with coupling strength, used for
 * inter-scale phase coupling in the crystal bus.
 */
struct alignas(8) OscillatorLevel {
    float activation{0.0f};     ///< [-1, +1]: current output
    float coupling{0.0f};       ///< [0, 1]: coupling strength to parent scale

    constexpr OscillatorLevel() = default;
    constexpr OscillatorLevel(float a, float c) : activation(a), coupling(c) {}

    /// Fully coupled excitatory
    [[nodiscard]] static constexpr OscillatorLevel excite(float a = 0.7f) noexcept {
        return {a, 1.0f};
    }

    /// Decoupled (free-running)
    [[nodiscard]] static constexpr OscillatorLevel free_running(float a = 0.3f) noexcept {
        return {a, 0.0f};
    }

    [[nodiscard]] constexpr float magnitude() const noexcept {
        return std::abs(activation);
    }

    [[nodiscard]] constexpr bool is_active(float threshold = 0.1f) const noexcept {
        return std::abs(activation) >= threshold;
    }

    constexpr auto operator<=>(const OscillatorLevel&) const = default;
};

static_assert(sizeof(OscillatorLevel) == 8);

// ============================================================================
// Oscillator State — Full snapshot (64 bytes, SIMD-aligned)
// ============================================================================

/**
 * @brief Snapshot of all temporal scale activations at a point in time
 *
 * 16 floats × 4 bytes = 64 bytes, SIMD-aligned for AVX2 batch operations.
 * Maps directly to the 9 biological scales plus 7 reserved channels.
 */
struct alignas(SIMD_ALIGN) OscillatorState {
    std::array<float, TEMPORAL_SCALE_COUNT> activations{};

    [[nodiscard]] float operator[](TemporalScaleId id) const noexcept {
        return activations[static_cast<size_t>(id)];
    }
    float& operator[](TemporalScaleId id) noexcept {
        return activations[static_cast<size_t>(id)];
    }

    /// Euclidean distance to another state
    [[nodiscard]] float distance_to(const OscillatorState& other) const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < TEMPORAL_SCALE_COUNT; ++i) {
            float d = activations[i] - other.activations[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }

    /// Mean coherence across all active scales
    [[nodiscard]] float mean_activation() const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            sum += std::abs(activations[i]);
        }
        return sum / static_cast<float>(BIOLOGICAL_SCALE_COUNT);
    }

    /// Universal set aggregate (slow oscillators >= 0.5s)
    [[nodiscard]] float universal_energy() const noexcept {
        float sum = 0.0f;
        for (size_t i = UNIVERSAL_SET_THRESHOLD; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            sum += activations[i] * activations[i];
        }
        return std::sqrt(sum);
    }

    /// Particular set aggregate (fast oscillators < 0.5s)
    [[nodiscard]] float particular_energy() const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < UNIVERSAL_SET_THRESHOLD; ++i) {
            sum += activations[i] * activations[i];
        }
        return std::sqrt(sum);
    }
};

static_assert(sizeof(OscillatorState) == 64);

// ============================================================================
// Cognitive Domain Mapping — TCN [3,4,3,3] for cognitive processes
// ============================================================================

/**
 * @brief Cognitive process categories mapped from TCN temporal scales
 *
 * Each biological temporal scale maps to a characteristic cognitive
 * process, following the Nanobrain theory that nested oscillations
 * at different time constants implement hierarchical information
 * processing.
 */
enum class CognitiveProcess : uint8_t {
    SPIKE_PROPAGATION    = 0,   ///< 8ms:   Raw signal encoding
    SYNAPTIC_GATING      = 1,   ///< 26ms:  Attention filtering
    FEATURE_BINDING      = 2,   ///< 52ms:  Cross-modal integration
    DECISION_THRESHOLD   = 3,   ///< 110ms: Action potential initiation
    CONTEXT_ASSEMBLY     = 4,   ///< 160ms: Contextual frame building
    LEARNING_UPDATE      = 5,   ///< 250ms: Hebbian weight adjustment
    CONCEPT_FORMATION    = 6,   ///< 330ms: Abstract category formation
    WORKING_MEMORY       = 7,   ///< 500ms: Sustained activation buffer
    META_COGNITION       = 8,   ///< 1000ms: Self-monitoring, strategy selection
};

[[nodiscard]] constexpr std::string_view cognitive_process_name(CognitiveProcess p) noexcept {
    constexpr std::string_view names[] = {
        "SpikePropagation", "SynapticGating", "FeatureBinding",
        "DecisionThreshold", "ContextAssembly", "LearningUpdate",
        "ConceptFormation", "WorkingMemory", "MetaCognition"
    };
    auto idx = static_cast<size_t>(p);
    return idx < 9 ? names[idx] : "Unknown";
}

// ============================================================================
// Brain Hierarchy Level — 12 levels from TCNN model
// ============================================================================

/**
 * @brief Hierarchical levels from Nanobrain Figure 7.15
 *
 * 12 levels spanning molecular (microtubule) to systemic (vascular),
 * each with characteristic spatial scale and component types.
 * Mapped to cognitive subsystems in the opencog-modern architecture.
 */
enum class HierarchyLevel : uint8_t {
    MICROTUBULE       = 1,   ///< Molecular: tubulin dimers, protein dynamics
    NEURON            = 2,   ///< Cellular: generalized neuron [3,4,3,3]
    CORTICAL_BRANCHES = 3,   ///< Columnar: cortical columns, layers
    CORTEX_DOMAIN     = 4,   ///< Regional: lobes, Brodmann areas
    CEREBELLUM        = 5,   ///< Organ: timing, coordination, prediction
    HYPOTHALAMUS      = 6,   ///< Nuclear: homeostatic regulation
    HIPPOCAMPUS       = 7,   ///< Nuclear: memory encoding/retrieval
    THALAMIC_BODY     = 8,   ///< Relay: sensory gating, attention routing
    SKIN_NERVE_NET    = 9,   ///< Peripheral: external sensing
    CRANIAL_NERVE     = 10,  ///< Peripheral: I/O pathways
    THORACIC_NERVE    = 11,  ///< Spinal: reflex arcs
    BLOOD_VESSEL      = 12,  ///< Vascular: resource flow
};

inline constexpr size_t HIERARCHY_LEVEL_COUNT = 12;

/// Cognitive subsystem mapping for each hierarchy level
[[nodiscard]] constexpr std::string_view hierarchy_cognitive_name(HierarchyLevel level) noexcept {
    constexpr std::string_view names[] = {
        "", // 0 unused
        "AtomRepresentation",     // 1: primitive symbol encoding
        "ConceptUnit",            // 2: single concept processing
        "ConceptColumn",          // 3: conceptual clustering
        "ReasoningDomain",        // 4: logical inference regions
        "PredictionEngine",       // 5: timing / prediction
        "HomeostaticRegulator",   // 6: resource management
        "MemorySystem",           // 7: encoding / recall
        "AttentionRelay",         // 8: gating / routing
        "PeripheralSensing",      // 9: external input
        "IOChannel",              // 10: input/output pathways
        "ReflexProcessor",        // 11: fast reflex arcs
        "ResourceFlow"            // 12: energy supply
    };
    auto idx = static_cast<size_t>(level);
    return idx <= HIERARCHY_LEVEL_COUNT ? names[idx] : "Unknown";
}

/// Base oscillation period for each hierarchy level (seconds)
[[nodiscard]] constexpr float hierarchy_period(HierarchyLevel level) noexcept {
    constexpr float periods[] = {
        0.0f,   // 0 unused
        0.001f, // MICROTUBULE
        0.008f, // NEURON
        0.050f, // CORTICAL_BRANCHES
        0.100f, // CORTEX_DOMAIN
        0.200f, // CEREBELLUM
        0.300f, // HYPOTHALAMUS
        0.400f, // HIPPOCAMPUS
        0.500f, // THALAMIC_BODY
        0.700f, // SKIN_NERVE_NET
        0.800f, // CRANIAL_NERVE
        0.900f, // THORACIC_NERVE
        1.000f  // BLOOD_VESSEL
    };
    auto idx = static_cast<size_t>(level);
    return idx <= HIERARCHY_LEVEL_COUNT ? periods[idx] : 0.0f;
}

[[nodiscard]] constexpr std::string_view hierarchy_level_name(HierarchyLevel level) noexcept {
    constexpr std::string_view names[] = {
        "", "Microtubule", "Neuron", "CorticalBranches", "CortexDomain",
        "Cerebellum", "Hypothalamus", "Hippocampus", "ThalamicBody",
        "SkinNerveNet", "CranialNerve", "ThoracicNerve", "BloodVessel"
    };
    auto idx = static_cast<size_t>(level);
    return idx <= HIERARCHY_LEVEL_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Brain Region — 8 canonical brain regions from TCNN model
// ============================================================================

enum class BrainRegionId : uint8_t {
    MICROTUBULE     = 0,   ///< Molecular foundations
    CORTEX_DOMAIN   = 1,   ///< Cortical lobes and areas
    CEREBELLUM      = 2,   ///< Cerebellar coordination
    HYPOTHALAMUS    = 3,   ///< Homeostatic centers
    HIPPOCAMPUS     = 4,   ///< Memory nuclei
    THALAMIC_BODY   = 5,   ///< Sensory relay
    CRANIAL_NERVE   = 6,   ///< Cranial nerve pathways
    BLOOD_VESSEL    = 7,   ///< Vascular supply

    REGION_COUNT    = 8    ///< Total regions
};

inline constexpr size_t BRAIN_REGION_COUNT = 8;

[[nodiscard]] constexpr std::string_view region_name(BrainRegionId id) noexcept {
    constexpr std::string_view names[] = {
        "Microtubule", "CortexDomain", "Cerebellum", "Hypothalamus",
        "Hippocampus", "ThalamicBody", "CranialNerve", "BloodVessel"
    };
    auto idx = static_cast<size_t>(id);
    return idx < BRAIN_REGION_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Functional Subsystem — 7 subsystems from TCNN model
// ============================================================================

enum class SubsystemId : uint8_t {
    PROPRIOCEPTION  = 0,   ///< Body position / kinesthetic sense
    HOMEOSTATIC     = 1,   ///< Autonomic regulation
    EMOTION         = 2,   ///< Affective processing / personality
    OLFACTORY       = 3,   ///< Chemical sensing
    ENTORHINAL      = 4,   ///< Spatial/object navigation
    SPINAL          = 5,   ///< Somatosensory pathways
    COGNITIVE       = 6,   ///< Higher cognitive (added for opencog)

    SUBSYSTEM_COUNT = 7    ///< Total subsystems
};

inline constexpr size_t FUNCTIONAL_SUBSYSTEM_COUNT = 7;

[[nodiscard]] constexpr std::string_view subsystem_name(SubsystemId id) noexcept {
    constexpr std::string_view names[] = {
        "Proprioception", "Homeostatic", "Emotion",
        "Olfactory", "Entorhinal", "Spinal", "Cognitive"
    };
    auto idx = static_cast<size_t>(id);
    return idx < FUNCTIONAL_SUBSYSTEM_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Crystal Component — Minimal representation of a neuron component
// ============================================================================

/**
 * @brief A single component in the time crystal hierarchy
 *
 * Each component has a name (abbreviation from Nanobrain), a temporal
 * scale, a cognitive domain mapping, and a category.
 */
struct CrystalComponent {
    std::string_view abbreviation;   ///< Nanobrain abbreviation (e.g., "Ax", "PNN")
    std::string_view full_name;      ///< Full name
    TemporalScaleId scale;           ///< Characteristic temporal scale
    CognitiveProcess cognitive_map;  ///< Cognitive domain mapping
    uint8_t category;                ///< 0=feedback, 1=rhythm, 2=morph, 3=mech, 4=anatomic, 5=junction
};

// ============================================================================
// TCN Structure Notation — [a,b,c,d]
// ============================================================================

/**
 * @brief Neuron structure parameters [a,b,c,d]
 *
 * Encodes the hierarchical nesting of the time crystal neuron:
 * - a: Number of spatial domains (default 3: dendrite, soma, axon)
 * - b: Number of functional layers per domain
 * - c: Number of temporal scales per layer
 * - d: Number of component types per scale
 *
 * Total oscillators = a * b * c * d
 */
struct TCNStructure {
    uint8_t domains{3};        ///< a: spatial domains
    uint8_t layers{4};         ///< b: layers per domain
    uint8_t scales{3};         ///< c: temporal scales per layer
    uint8_t components{3};     ///< d: component types per scale

    constexpr TCNStructure() = default;
    constexpr TCNStructure(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
        : domains(a), layers(b), scales(c), components(d) {}

    /// Default: Neuron [3,4,3,3] from Nanobrain Fig 6.14
    [[nodiscard]] static constexpr TCNStructure default_neuron() noexcept {
        return {3, 4, 3, 3};
    }

    /// Total number of oscillator components
    [[nodiscard]] constexpr uint32_t total_oscillators() const noexcept {
        return static_cast<uint32_t>(domains) * layers * scales * components;
    }

    /// Number of layers total
    [[nodiscard]] constexpr uint32_t total_layers() const noexcept {
        return static_cast<uint32_t>(domains) * layers;
    }
};

static_assert(sizeof(TCNStructure) == 4);

// ============================================================================
// Crystal Bus Configuration
// ============================================================================

struct CrystalBusConfig {
    float tick_interval_ms{50.0f};      ///< Nominal update interval (between VES 100ms and VNS 10ms)
    float global_coupling{0.5f};        ///< Default inter-scale coupling strength [0,1]
    float phase_noise{0.01f};           ///< Random phase perturbation per tick
    bool enable_simd{true};             ///< Use SIMD for batch phase updates
    size_t history_length{200};         ///< Ring buffer size for snapshots
    bool enable_ppm{true};              ///< Enable Phase Prime Metric coupling
};

// ============================================================================
// Crystal Event — Signals that trigger oscillator cascades
// ============================================================================

enum class CrystalEvent : uint8_t {
    // Phase events (0-9)
    PHASE_LOCK_ACHIEVED     = 0,   ///< Two+ scales achieved phase coherence
    PHASE_LOCK_LOST         = 1,   ///< Phase coherence dropped below threshold
    RESONANCE_DETECTED      = 2,   ///< Harmonic resonance across scales
    FREQUENCY_SHIFT         = 3,   ///< Scale frequency drift detected
    AMPLITUDE_SPIKE         = 4,   ///< Sudden amplitude increase

    // Hierarchy events (10-19)
    UNIVERSAL_SET_ACTIVE    = 10,  ///< Slow oscillator became dominant
    PARTICULAR_SET_BURST    = 11,  ///< Fast oscillator burst detected
    HIERARCHY_SYNC          = 12,  ///< Cross-level synchronization
    HIERARCHY_DESYNC        = 13,  ///< Cross-level desynchronization

    // Cognitive events (20-29)
    CONCEPT_CRYSTALLIZED    = 20,  ///< Stable pattern formed at concept level
    ATTENTION_PHASE_SHIFT   = 21,  ///< Attention changed phase alignment
    MEMORY_ENCODING_TRIGGER = 22,  ///< Hippocampal theta-gamma coupling detected
    LEARNING_WINDOW_OPEN    = 23,  ///< Plasticity-favorable phase alignment
    META_COGNITIVE_CYCLE    = 24,  ///< Global rhythm completed a full cycle

    // Integration events (30-39)
    ENDOCRINE_SYNC          = 30,  ///< Crystal ↔ VES phase alignment
    NEURAL_SYNC             = 31,  ///< Crystal ↔ VNS phase alignment
    CROSS_SYSTEM_RESONANCE  = 32,  ///< All three systems in resonance
};

} // namespace opencog::temporal
