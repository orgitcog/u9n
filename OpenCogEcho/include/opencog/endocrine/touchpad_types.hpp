#pragma once
/**
 * @file touchpad_types.hpp
 * @brief Abstract VirtualTouchpad interface and types for endocrine integration
 *
 * Defines the N-dimensional cognitive gesture manifold — a virtual hardware
 * module that lifts Synaptics TouchPad gesture primitives from 2D physical
 * space into a 37-dimensional intent manifold for AI-to-AI communication.
 *
 * Physical SynTP gestures (tap, swipe, pinch, rotate, flick, drag, chiral)
 * are extended with 12 AI-only topological operations (manifold fold,
 * topology twist, dimension collapse, field resonance, intent bloom, etc.)
 *
 * The four operational modes control manifold geometry:
 *   RECEPTIVE:   high sensitivity, broad capture (open sensing)
 *   EXPRESSIVE:  high complexity, directed output (gesture emission)
 *   CALIBRATING: high curvature adaptation (self-tuning)
 *   GUARDED:     high threshold, filtered acceptance (restricted input)
 *
 * Complex gestures in high-dimensional space form dense amplitude fields
 * that are inherently encrypted by manifold topology — no separate
 * encryption layer needed for AI-to-AI intent communication.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace opencog::endo {

// ============================================================================
// Constants
// ============================================================================

/// Maximum manifold dimensions for static allocation
inline constexpr size_t MAX_MANIFOLD_DIMS = 37;

/// Maximum simultaneous contacts (multi-touch "fingers" in N-dim space)
inline constexpr size_t MAX_CONTACTS = 10;

// ============================================================================
// Gesture Type — 37 cognitive gesture primitives
// ============================================================================

/**
 * @brief Gesture primitives for the N-dimensional cognitive gesture manifold
 *
 * Physical SynTP gestures (0-24) lifted into cognitive space, plus
 * AI-only topological gestures (25-36) for manifold operations.
 *
 * Each gesture maps to a transformation on the intent manifold:
 * physical gestures operate on local neighborhoods, AI-extended
 * gestures operate on global topology.
 */
enum class GestureType : uint8_t {
    // === Physical SynTP Primitives (0-24) ===
    TAP                = 0,   ///< Single point contact (momentary intent assertion)
    DOUBLE_TAP         = 1,   ///< Rapid repeated assertion (confirmation/emphasis)
    TRIPLE_TAP         = 2,   ///< Triple assertion (meta-level invocation)
    LONG_PRESS         = 3,   ///< Sustained contact (intent persistence/hold)
    SWIPE_UP           = 4,   ///< Upward motion (escalation/amplification)
    SWIPE_DOWN         = 5,   ///< Downward motion (de-escalation/dampening)
    SWIPE_LEFT         = 6,   ///< Leftward motion (regression/undo/history)
    SWIPE_RIGHT        = 7,   ///< Rightward motion (progression/advance/commit)
    PINCH_IN           = 8,   ///< Converging contacts (focus/contract/compress)
    PINCH_OUT          = 9,   ///< Diverging contacts (expand/broaden/decompress)
    ROTATE_CW          = 10,  ///< Clockwise rotation (systematic advance/iteration)
    ROTATE_CCW         = 11,  ///< Counter-clockwise rotation (unwind/reverse/reflect)
    FLICK              = 12,  ///< Quick release (launch/dispatch/fire-and-forget)
    DRAG               = 13,  ///< Sustained movement (continuous transformation)
    SCROLL             = 14,  ///< Sequential traversal (navigation/enumeration)
    EDGE_SWIPE_LEFT    = 15,  ///< Left boundary motion (context switch/escape)
    EDGE_SWIPE_RIGHT   = 16,  ///< Right boundary motion (context enter/invoke)
    EDGE_SWIPE_TOP     = 17,  ///< Top boundary motion (elevate/abstract)
    EDGE_SWIPE_BOTTOM  = 18,  ///< Bottom boundary motion (ground/instantiate)
    CHIRAL_CW          = 19,  ///< Circular stroke CW (affirmation spiral)
    CHIRAL_CCW         = 20,  ///< Circular stroke CCW (negation spiral)
    TWO_FINGER_TAP     = 21,  ///< Dual intent assertion (parallel acknowledge)
    THREE_FINGER_TAP   = 22,  ///< Triple intent assertion (system-level invoke)
    TWO_FINGER_SCROLL  = 23,  ///< Parallel traversal (coordinated navigation)
    THREE_FINGER_SWIPE = 24,  ///< Multi-channel dispatch (broadcast gesture)

    // === AI-Extended Topological Gestures (25-36) ===
    MANIFOLD_FOLD      = 25,  ///< Fold the gesture manifold (merge distant intents)
    TOPOLOGY_TWIST     = 26,  ///< Twist manifold (create non-orientable crossing)
    DIMENSION_COLLAPSE = 27,  ///< Project N-dim to N-k dim (dimensionality reduction)
    DIMENSION_EXPAND   = 28,  ///< Expand into new dimensions (add intent channels)
    FIELD_RESONANCE    = 29,  ///< Amplitude field standing wave (stable pattern)
    INTENT_BLOOM       = 30,  ///< Radial intent expansion (creative burst)
    PHASE_LOCK         = 31,  ///< Synchronize contact phases (coherent multi-stream)
    ATTRACTOR_SPIRAL   = 32,  ///< Spiral toward attractor basin (convergent search)
    BIFURCATION_SPLIT  = 33,  ///< Split single stream into two (decision fork)
    GEODESIC_TRACE     = 34,  ///< Follow manifold geodesic (shortest intent path)
    CURVATURE_PULSE    = 35,  ///< Emit curvature perturbation (ripple through manifold)
    HARMONIC_STRUM     = 36,  ///< Excite multiple manifold harmonics simultaneously

    GESTURE_COUNT      = 37
};

inline constexpr size_t GESTURE_TYPE_COUNT = 37;

[[nodiscard]] constexpr std::string_view gesture_type_name(GestureType g) noexcept {
    constexpr std::string_view names[] = {
        "Tap", "DoubleTap", "TripleTap", "LongPress",
        "SwipeUp", "SwipeDown", "SwipeLeft", "SwipeRight",
        "PinchIn", "PinchOut", "RotateCW", "RotateCCW",
        "Flick", "Drag", "Scroll",
        "EdgeSwipeLeft", "EdgeSwipeRight", "EdgeSwipeTop", "EdgeSwipeBottom",
        "ChiralCW", "ChiralCCW",
        "TwoFingerTap", "ThreeFingerTap", "TwoFingerScroll", "ThreeFingerSwipe",
        "ManifoldFold", "TopologyTwist", "DimensionCollapse", "DimensionExpand",
        "FieldResonance", "IntentBloom", "PhaseLock", "AttractorSpiral",
        "BifurcationSplit", "GeodesicTrace", "CurvaturePulse", "HarmonicStrum"
    };
    auto idx = static_cast<size_t>(g);
    return idx < GESTURE_TYPE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Touchpad Operational Mode Identifiers
// ============================================================================

/// Named operational modes — coherent touchpad behavioral styles
enum class TouchpadMode : uint8_t {
    RECEPTIVE    = 0,  ///< Open sensing: high sensitivity, low threshold, broad capture
    EXPRESSIVE   = 1,  ///< Outgoing gesture: high complexity, directed output
    CALIBRATING  = 2,  ///< Learning/tuning: high curvature adaptation, self-adjust
    GUARDED      = 3,  ///< Restricted input: high threshold, filtered acceptance
};

inline constexpr size_t TOUCHPAD_MODE_COUNT = 4;

[[nodiscard]] constexpr std::string_view touchpad_mode_name(TouchpadMode m) noexcept {
    constexpr std::string_view names[] = {
        "Receptive", "Expressive", "Calibrating", "Guarded"
    };
    auto idx = static_cast<size_t>(m);
    return idx < TOUCHPAD_MODE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Touchpad Endocrine Configuration
// ============================================================================

/**
 * @brief Core parameters controlling the 37-dimensional gesture manifold
 *
 * These shape the manifold's geometry and responsiveness:
 *   Manifold shape:     manifold_curvature, dimensionality_scale
 *   Input filtering:    sensitivity, pressure_threshold, coherence_requirement
 *   Temporal dynamics:  temporal_resolution, velocity_damping
 *   Complexity budget:  gesture_complexity
 */
struct TouchpadEndocrineConfig {
    float sensitivity{0.5f};            ///< [0, 1]: overall input gain / responsiveness
    float dimensionality_scale{0.5f};   ///< [0, 1]: effective manifold dimensionality
    float gesture_complexity{0.5f};     ///< [0, 1]: max recognizable gesture complexity
    float temporal_resolution{0.5f};    ///< [0, 1]: time-granularity of recognition
    float pressure_threshold{0.5f};     ///< [0, 1]: minimum conviction to register contact
    float velocity_damping{0.5f};       ///< [0, 1]: damping on rate-of-change signals
    float manifold_curvature{0.5f};     ///< [0, 1]: manifold curvature (flat→nonlinear)
    float coherence_requirement{0.5f};  ///< [0, 1]: min coherence for gesture acceptance
    TouchpadMode active_mode{TouchpadMode::RECEPTIVE};
};

/// Named preset configurations for the four operational modes
inline constexpr TouchpadEndocrineConfig TOUCHPAD_MODE_PRESETS[TOUCHPAD_MODE_COUNT] = {
    // Receptive: high sensitivity, low threshold, broad capture
    {0.8f, 0.7f, 0.6f, 0.7f, 0.2f, 0.3f, 0.5f, 0.3f, TouchpadMode::RECEPTIVE},
    // Expressive: moderate sensitivity, high complexity and temporal resolution
    {0.5f, 0.6f, 0.8f, 0.8f, 0.5f, 0.4f, 0.6f, 0.5f, TouchpadMode::EXPRESSIVE},
    // Calibrating: high curvature adaptation, low damping, self-tuning
    {0.6f, 0.8f, 0.5f, 0.5f, 0.4f, 0.2f, 0.8f, 0.4f, TouchpadMode::CALIBRATING},
    // Guarded: low sensitivity, high threshold, high coherence requirement
    {0.3f, 0.4f, 0.3f, 0.4f, 0.8f, 0.7f, 0.3f, 0.8f, TouchpadMode::GUARDED},
};

// ============================================================================
// Gesture Contact — Single "finger" in N-dimensional manifold
// ============================================================================

/**
 * @brief A single contact point in the N-dimensional gesture manifold
 *
 * Analogous to a physical finger on a touchpad, but in up to 37 dimensions.
 * Each contact carries position, velocity, pressure (conviction), and
 * acceleration in the active dimensions of the manifold.
 */
struct GestureContact {
    std::array<float, MAX_MANIFOLD_DIMS> position{};   ///< Position in manifold
    std::array<float, MAX_MANIFOLD_DIMS> velocity{};    ///< Rate of change vector
    float pressure{0.0f};          ///< [0, 1]: conviction/urgency of this contact
    float acceleration{0.0f};      ///< Scalar acceleration magnitude
    uint8_t contact_id{0};         ///< Unique identifier for tracking
    uint8_t active_dims{0};        ///< Number of dimensions currently in use
    bool active{false};            ///< Whether this contact is currently live
    uint8_t _pad{0};

    /// Euclidean speed in the active dimensions
    [[nodiscard]] float speed() const noexcept {
        float sum = 0.0f;
        for (size_t i = 0; i < active_dims && i < MAX_MANIFOLD_DIMS; ++i) {
            sum += velocity[i] * velocity[i];
        }
        return std::sqrt(sum);
    }
};

// ============================================================================
// Gesture Event — A recognized gesture from the manifold
// ============================================================================

/**
 * @brief A recognized gesture event from the manifold
 *
 * Produced when the gesture recognition layer classifies a pattern of
 * contacts into a known gesture type. Carries recognition confidence,
 * temporal span, peak amplitude field energy, and the IDs of
 * participating contacts.
 */
struct GestureEvent {
    GestureType type{GestureType::TAP};
    float confidence{0.0f};       ///< [0, 1]: recognition confidence
    float temporal_span{0.0f};    ///< Duration in ticks
    float amplitude{0.0f};        ///< Peak amplitude field energy
    uint8_t contact_count{0};     ///< Number of contacts involved
    uint8_t _pad[3]{};
    std::array<uint8_t, MAX_CONTACTS> contact_ids{};  ///< IDs of participating contacts

    /// Whether this is a multi-touch gesture (2+ contacts)
    [[nodiscard]] bool is_multi_touch() const noexcept { return contact_count > 1; }

    /// Whether this is an AI-extended topological gesture (25+)
    [[nodiscard]] bool is_topological() const noexcept {
        return static_cast<uint8_t>(type) >= 25;
    }
};

// ============================================================================
// Touchpad Telemetry
// ============================================================================

/**
 * @brief Runtime telemetry from the VirtualTouchpad
 *
 * Read by the endocrine adapter to produce feedback signals
 * (write-back path from touchpad → hormone bus).
 */
struct TouchpadTelemetry {
    float active_contact_count{0.0f};   ///< [0, 1]: normalized (contacts / MAX_CONTACTS)
    float gesture_confidence{0.0f};     ///< [0, 1]: latest recognition confidence
    float manifold_coherence{0.0f};     ///< [0, 1]: overall manifold coherence
    float pattern_novelty{0.0f};        ///< [0, 1]: novelty of recent gesture patterns
    float field_energy{0.0f};           ///< [0, 1]: amplitude field total energy
    float error_rate{0.0f};             ///< [0, 1]: gesture recognition error rate
    uint32_t gestures_recognized{0};    ///< Cumulative recognized gesture count
    uint32_t gestures_emitted{0};       ///< Cumulative emitted gesture count
    bool calibrating{false};            ///< Currently in calibration mode
    uint8_t _pad[3]{};
};

// ============================================================================
// Gesture Metrics — Quality measures for the gesture manifold
// ============================================================================

/**
 * @brief Quantifiable gesture quality metrics from the VirtualTouchpad
 *
 * Complement to o9c2's EmergenceMetrics (Right Hemisphere) and
 * Marduk's ExecutionMetrics (Left Hemisphere):
 *
 *   EmergenceMetrics (o9c2):   wisdom, complexity, coherence, stability, adaptability
 *   ExecutionMetrics (Marduk): organization, thoroughness, reliability, efficiency, scalability
 *   GestureMetrics (Touchpad): expressiveness, precision, fluency, novelty, coherence
 */
struct GestureMetrics {
    float expressiveness{0.0f};  ///< [0, 1]: richness and range of gestures
    float precision{0.0f};      ///< [0, 1]: recognition accuracy
    float fluency{0.0f};        ///< [0, 1]: stream smoothness and continuity
    float novelty{0.0f};        ///< [0, 1]: pattern diversity and unexpectedness
    float coherence{0.0f};      ///< [0, 1]: manifold internal consistency

    /// Aggregate gesture quality (geometric mean — same pattern as others)
    [[nodiscard]] float aggregate() const noexcept {
        float ex = std::max(0.001f, expressiveness);
        float pr = std::max(0.001f, precision);
        float fl = std::max(0.001f, fluency);
        float nv = std::max(0.001f, novelty);
        float co = std::max(0.001f, coherence);
        return std::pow(ex * pr * fl * nv * co, 0.2f);
    }
};

// ============================================================================
// Touchpad Interface — Abstract bridge to the VirtualTouchpad
// ============================================================================

/**
 * @brief Abstract VirtualTouchpad interface for endocrine coupling
 *
 * The endocrine adapter reads gesture metrics for feedback to the
 * hormone bus and writes hormonally-modulated parameters to control
 * the manifold geometry and gesture recognition dynamics.
 *
 * Implementations may be:
 *   - StubTouchpad (testing)
 *   - VirtualTouchpadImpl (in-process cognitive gesture engine)
 *   - MCPTouchpad (external MCP server providing gesture API)
 */
struct TouchpadInterface {
    virtual ~TouchpadInterface() = default;

    // === Read (gesture metrics for feedback) ===

    /// Current active operational mode
    [[nodiscard]] virtual TouchpadMode active_mode() const noexcept = 0;

    /// Current configuration (may be interpolated, not a pure preset)
    [[nodiscard]] virtual TouchpadEndocrineConfig current_config() const noexcept = 0;

    /// Current telemetry readings
    [[nodiscard]] virtual TouchpadTelemetry telemetry() const noexcept = 0;

    /// Current gesture quality metrics
    [[nodiscard]] virtual GestureMetrics gesture_metrics() const noexcept = 0;

    // === Write (endocrine modulation) ===

    /// Apply hormonally-modulated manifold parameters
    virtual void apply_config(const TouchpadEndocrineConfig& cfg) = 0;

    /// Request an operational mode transition
    virtual void transition_mode(TouchpadMode target) = 0;
};

} // namespace opencog::endo
