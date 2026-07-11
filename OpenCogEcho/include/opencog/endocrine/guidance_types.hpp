#pragma once
/**
 * @file guidance_types.hpp
 * @brief Types for external guidance communication (Main Claude integration)
 *
 * Defines the request/response protocol for the system to query an
 * external guidance agent (e.g., Claude API) for meta-cognitive
 * oversight. The GuidanceBackend abstract interface allows multiple
 * transport mechanisms (stub, HTTP, MMIO).
 *
 * The guidance layer sits above the endocrine system: it reads the
 * full hormonal state and can write back directive-driven hormone
 * nudges and mode/persona suggestions.
 */

#include <opencog/endocrine/types.hpp>
#include <opencog/endocrine/npu_types.hpp>
#include <opencog/endocrine/o9c2_types.hpp>
#include <opencog/endocrine/marduk_types.hpp>
#include <opencog/endocrine/touchpad_types.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <future>
#include <limits>
#include <string>
#include <string_view>

namespace opencog::endo {

// ============================================================================
// Guidance Request Enums
// ============================================================================

/// Urgency level for guidance requests
enum class GuidanceUrgency : uint8_t {
    LOW      = 0,  ///< Advisory, can wait many ticks
    MEDIUM   = 1,  ///< Should respond within a few ticks
    HIGH     = 2,  ///< Needs prompt response
    CRITICAL = 3,  ///< System in conflict/danger, blocking on response
};

/// Reason categories for requesting guidance
enum class GuidanceReason : uint8_t {
    MORAL_NOVELTY             = 0,  ///< Novel moral situation with no precedent
    HIGH_STRESS               = 1,  ///< Sustained elevated cortisol
    UNCERTAINTY               = 2,  ///< High uncertainty, conflicting signals
    MODE_CONFLICT             = 3,  ///< Endocrine mode conflicts with current goal
    PERSONA_QUESTION          = 4,  ///< Uncertain which o9c2 persona to use
    EMERGENCE_DROP            = 5,  ///< Emergence metrics declining
    STRATEGIC                 = 6,  ///< Periodic strategic check-in
    ERROR_RECOVERY            = 7,  ///< Error state needing guidance
    MARDUK_OVERLOAD           = 8,  ///< Marduk task queue exceeds capacity
    HEMISPHERIC_DIVERGENCE    = 9,  ///< Left-Right hemisphere coherence diverging
    TOUCHPAD_OVERLOAD         = 10, ///< Touchpad manifold overloaded (contacts/gestures exceeding capacity)
};

[[nodiscard]] constexpr std::string_view urgency_name(GuidanceUrgency u) noexcept {
    constexpr std::string_view names[] = {"Low", "Medium", "High", "Critical"};
    auto idx = static_cast<size_t>(u);
    return idx < 4 ? names[idx] : "Unknown";
}

[[nodiscard]] constexpr std::string_view reason_name(GuidanceReason r) noexcept {
    constexpr std::string_view names[] = {
        "MoralNovelty", "HighStress", "Uncertainty", "ModeConflict",
        "PersonaQuestion", "EmergenceDrop", "Strategic", "ErrorRecovery",
        "MardukOverload", "HemisphericDivergence", "TouchpadOverload"
    };
    auto idx = static_cast<size_t>(r);
    return idx < 11 ? names[idx] : "Unknown";
}

// ============================================================================
// Guidance Request — Sent to external guidance agent
// ============================================================================

/**
 * @brief Context package sent to the external guidance agent
 *
 * Contains a full snapshot of the system state: endocrine hormones,
 * cognitive mode, o9c2 persona/emergence, NPU telemetry, and the
 * triggering reason.
 */
struct GuidanceRequest {
    GuidanceUrgency urgency{GuidanceUrgency::MEDIUM};
    GuidanceReason  reason{GuidanceReason::STRATEGIC};

    // Endocrine state
    EndocrineState endocrine_snapshot;
    CognitiveMode  current_mode{CognitiveMode::RESTING};

    // o9c2 state
    O9C2Persona       current_persona{O9C2Persona::CONTEMPLATIVE_SCHOLAR};
    EmergenceMetrics  emergence;

    // NPU state
    NPUTelemetry npu_telemetry;

    // Marduk state (Left Hemisphere)
    MardukOperationalMode marduk_mode{MardukOperationalMode::KNOWLEDGE_ARCHITECT};
    MardukTelemetry       marduk_telemetry;
    ExecutionMetrics      execution_metrics;

    // VirtualTouchpad state
    TouchpadMode          touchpad_mode{TouchpadMode::RECEPTIVE};
    TouchpadTelemetry     touchpad_telemetry;
    GestureMetrics        gesture_metrics;

    // Triggering signals
    float moral_novelty{0.0f};   ///< From moral perception engine
    float stress_level{0.0f};    ///< Average cortisol over recent window

    // Context
    std::string context_description;  ///< Free-form description of situation
    uint64_t    tick{0};              ///< System tick when request was generated
};

// ============================================================================
// Guidance Response — Received from external guidance agent
// ============================================================================

/// Directive types that guidance can issue
enum class GuidanceDirective : uint8_t {
    NO_ACTION              = 0,  ///< Continue current trajectory
    SUGGEST_MODE           = 1,  ///< Suggest cognitive mode transition
    SUGGEST_PERSONA        = 2,  ///< Suggest o9c2 persona transition
    ADJUST_PARAMETERS      = 3,  ///< Provide specific parameter overrides
    REDUCE_STRESS          = 4,  ///< Initiate stress reduction protocol
    INCREASE_CAUTION       = 5,  ///< Elevate vigilance and conservatism
    ENCOURAGE_EXPLORE      = 6,  ///< Encourage exploration and creativity
    STRATEGIC_PAUSE        = 7,  ///< Suggest pause for reflective maintenance
    SUGGEST_MARDUK_MODE    = 8,  ///< Suggest Marduk operational mode transition
    MARDUK_STRATEGIC_PAUSE = 9,  ///< Pause Marduk task processing for consolidation
    SUGGEST_TOUCHPAD_MODE  = 10, ///< Suggest VirtualTouchpad operational mode transition
    TOUCHPAD_CALIBRATE     = 11, ///< Trigger touchpad manifold calibration
};

[[nodiscard]] constexpr std::string_view directive_name(GuidanceDirective d) noexcept {
    constexpr std::string_view names[] = {
        "NoAction", "SuggestMode", "SuggestPersona", "AdjustParameters",
        "ReduceStress", "IncreaseCaution", "EncourageExplore", "StrategicPause",
        "SuggestMardukMode", "MardukStrategicPause",
        "SuggestTouchpadMode", "TouchpadCalibrate"
    };
    auto idx = static_cast<size_t>(d);
    return idx < 12 ? names[idx] : "Unknown";
}

/**
 * @brief Response from external guidance agent
 *
 * Contains a directive, optional mode/persona suggestions, optional
 * parameter overrides (NaN = no change), and direct hormone nudges.
 */
struct GuidanceResponse {
    GuidanceDirective directive{GuidanceDirective::NO_ACTION};

    // Suggestions
    CognitiveMode suggested_mode{CognitiveMode::RESTING};
    O9C2Persona   suggested_persona{O9C2Persona::CONTEMPLATIVE_SCHOLAR};
    MardukOperationalMode suggested_marduk_mode{MardukOperationalMode::KNOWLEDGE_ARCHITECT};
    TouchpadMode suggested_touchpad_mode{TouchpadMode::RECEPTIVE};

    // Parameter overrides (NaN = no change)
    float override_creativity{std::numeric_limits<float>::quiet_NaN()};
    float override_spectral_radius{std::numeric_limits<float>::quiet_NaN()};
    float override_input_scaling{std::numeric_limits<float>::quiet_NaN()};
    float override_leak_rate{std::numeric_limits<float>::quiet_NaN()};
    float override_task_urgency{std::numeric_limits<float>::quiet_NaN()};
    float override_validation_threshold{std::numeric_limits<float>::quiet_NaN()};
    float override_sensitivity{std::numeric_limits<float>::quiet_NaN()};
    float override_coherence_requirement{std::numeric_limits<float>::quiet_NaN()};

    // Direct hormone nudges (0 = no change, positive = produce, negative = suppress)
    std::array<float, HORMONE_COUNT> hormone_nudges{};

    // Metadata
    std::string rationale;  ///< Explanation from guidance
    bool valid{false};      ///< Whether response was successfully received

    /// Check if a parameter override is set (not NaN)
    [[nodiscard]] static bool has_override(float val) noexcept {
        return !std::isnan(val);
    }
};

// ============================================================================
// Guidance Backend — Abstract transport interface
// ============================================================================

/**
 * @brief Abstract backend for guidance communication
 *
 * Multiple implementations are possible:
 * - StubGuidanceBackend: Returns preset responses (testing)
 * - HTTPGuidanceBackend: POSTs to Claude API endpoint (production)
 * - MMIOGuidanceBackend: Uses VirtualPCB registers (hardware simulation)
 */
struct GuidanceBackend {
    virtual ~GuidanceBackend() = default;

    /// Send request and return a future for the response
    virtual std::future<GuidanceResponse> request(const GuidanceRequest& req) = 0;

    /// Is the backend currently available?
    [[nodiscard]] virtual bool is_available() const noexcept = 0;

    /// Backend name for diagnostics
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

// ============================================================================
// Guidance Connector Configuration
// ============================================================================

struct GuidanceConfig {
    float stress_threshold{0.6f};       ///< Cortisol avg to trigger stress request
    size_t stress_window{20};           ///< Ticks to average for stress detection
    size_t stress_persist_ticks{10};    ///< Consecutive stressed ticks before trigger
    float moral_novelty_threshold{0.6f};///< Moral novelty to trigger request
    float coherence_floor{0.2f};        ///< COG_COHERENCE below this triggers request
    size_t cooldown_ticks{50};          ///< Minimum ticks between guidance requests
    size_t periodic_interval{500};      ///< Ticks between strategic check-ins

    // Marduk-specific thresholds
    float marduk_overload_threshold{0.85f};        ///< Task queue depth triggering overload
    float hemispheric_divergence_threshold{0.4f};   ///< |COG_COH - ORG_COH| triggering divergence

    // Touchpad-specific thresholds
    float touchpad_overload_threshold{0.85f};          ///< Touchpad load triggering overload
};

} // namespace opencog::endo
