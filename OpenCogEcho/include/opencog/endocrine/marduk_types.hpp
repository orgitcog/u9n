#pragma once
/**
 * @file marduk_types.hpp
 * @brief Abstract Marduk interface and types for endocrine integration
 *
 * Defines operational modes, execution metrics, and the abstract
 * MardukInterface that allows the endocrine system to modulate the
 * Marduk cognitive architecture's parameters and receive execution
 * metric feedback.
 *
 * Marduk is the Left Hemisphere cognitive agent — complementing
 * Deep Tree Echo (o9c2) as the Right Hemisphere:
 * - Marduk: categorical logic, task graphs, structured memory, meta-optimization
 * - o9c2:   novelty, pattern recognition, emergence, multi-scale dynamics
 *
 * The four operational modes correspond to coherent points in Marduk's
 * four-subsystem space (Memory, Task, AI, Autonomy).
 */

#include <cmath>
#include <cstdint>
#include <string_view>

namespace opencog::endo {

// ============================================================================
// Marduk Operational Mode Identifiers
// ============================================================================

/// Named operational modes — coherent cognitive styles for the Left Hemisphere
enum class MardukOperationalMode : uint8_t {
    KNOWLEDGE_ARCHITECT     = 0,  ///< Deep memory consolidation, high retention
    TASK_EXECUTOR           = 1,  ///< High urgency, focused task completion
    VERIFICATION_GUARDIAN   = 2,  ///< Strict validation, conservative execution
    BLUEPRINT_DESIGNER      = 3,  ///< Creative exploration, high synthesis
};

inline constexpr size_t MARDUK_MODE_COUNT = 4;

[[nodiscard]] constexpr std::string_view marduk_mode_name(MardukOperationalMode m) noexcept {
    constexpr std::string_view names[] = {
        "Knowledge Architect", "Task Executor",
        "Verification Guardian", "Blueprint Designer"
    };
    auto idx = static_cast<size_t>(m);
    return idx < MARDUK_MODE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Marduk Endocrine Configuration
// ============================================================================

/**
 * @brief Core parameters for the Marduk cognitive architecture
 *
 * These control the dynamics across Marduk's four subsystems:
 * - Memory: consolidation_depth, memory_retention
 * - Task: task_urgency, task_exploration
 * - AI: synthesis_intensity, integration_openness
 * - Autonomy: autonomy_rate, validation_threshold
 */
struct MardukEndocrineConfig {
    float task_urgency{0.5f};           ///< [0, 1]: priority pressure on task queue
    float task_exploration{0.5f};       ///< [0, 1]: willingness to branch to novel tasks
    float consolidation_depth{0.5f};    ///< [0, 1]: depth of memory encoding
    float memory_retention{0.5f};       ///< [0, 1]: strength of memory persistence
    float validation_threshold{0.5f};   ///< [0, 1]: quality bar for accepting results
    float synthesis_intensity{0.5f};    ///< [0, 1]: DarkFace constraint integration force
    float autonomy_rate{0.5f};          ///< [0, 1]: self-optimization cycle speed
    float integration_openness{0.5f};   ///< [0, 1]: receptivity to external input
    MardukOperationalMode active_mode{MardukOperationalMode::KNOWLEDGE_ARCHITECT};
};

/// Named preset configurations for Marduk's four operational modes
inline constexpr MardukEndocrineConfig MARDUK_MODE_PRESETS[MARDUK_MODE_COUNT] = {
    // Knowledge Architect: high consolidation, high retention, moderate exploration
    {0.4f, 0.5f, 0.8f, 0.8f, 0.6f, 0.6f, 0.5f, 0.6f,
     MardukOperationalMode::KNOWLEDGE_ARCHITECT},
    // Task Executor: high urgency, low exploration, high autonomy
    {0.8f, 0.3f, 0.4f, 0.5f, 0.5f, 0.4f, 0.7f, 0.4f,
     MardukOperationalMode::TASK_EXECUTOR},
    // Verification Guardian: high validation, low exploration, low autonomy
    {0.5f, 0.2f, 0.6f, 0.7f, 0.9f, 0.3f, 0.3f, 0.3f,
     MardukOperationalMode::VERIFICATION_GUARDIAN},
    // Blueprint Designer: high exploration, high synthesis, moderate autonomy
    {0.4f, 0.8f, 0.5f, 0.5f, 0.4f, 0.8f, 0.5f, 0.7f,
     MardukOperationalMode::BLUEPRINT_DESIGNER},
};

// ============================================================================
// Marduk Telemetry
// ============================================================================

/**
 * @brief Runtime telemetry from the Marduk cognitive agent
 *
 * Read by the endocrine adapter to produce feedback signals
 * (write-back path from Marduk → hormone bus).
 */
struct MardukTelemetry {
    float task_queue_depth{0.0f};       ///< [0, 1]: normalized task queue fullness
    float consolidation_progress{0.0f}; ///< [0, 1]: current memory encoding progress
    float autonomy_intensity{0.0f};     ///< [0, 1]: current self-optimization load
    float synthesis_coherence{0.0f};    ///< [0, 1]: DarkFace synthesis quality
    float exploration_intensity{0.0f};  ///< [0, 1]: LightFace exploration activity
    bool  lightface_active{false};      ///< LightFace exploration burst in progress
    bool  darkface_active{false};       ///< DarkFace synthesis cycle in progress
    uint32_t tasks_completed{0};        ///< Cumulative task completion count
    uint32_t optimization_cycles{0};    ///< Cumulative autonomy optimization cycles
    uint32_t meta_depth{0};             ///< Current meta-cognitive recursion depth
};

// ============================================================================
// Execution Metrics — Complement to EmergenceMetrics
// ============================================================================

/**
 * @brief Quantifiable execution metrics from the Marduk architecture
 *
 * These complement o9c2's EmergenceMetrics (Right Hemisphere quality):
 * - organization: structural coherence of task graphs and memory
 * - thoroughness: completeness of processing and validation
 * - reliability: consistency and error rate
 * - efficiency: resource utilization and throughput
 * - scalability: ability to handle increasing complexity
 *
 * EmergenceMetrics  (o9c2/RH): wisdom, complexity, coherence, stability, adaptability
 * ExecutionMetrics (Marduk/LH): organization, thoroughness, reliability, efficiency, scalability
 */
struct ExecutionMetrics {
    float organization{0.0f};    ///< [0, 1]: structural coherence
    float thoroughness{0.0f};    ///< [0, 1]: processing completeness
    float reliability{0.0f};     ///< [0, 1]: consistency / low error rate
    float efficiency{0.0f};      ///< [0, 1]: resource utilization
    float scalability{0.0f};     ///< [0, 1]: complexity handling capacity

    /// Aggregate execution score (geometric mean — same pattern as EmergenceMetrics)
    [[nodiscard]] float aggregate() const noexcept {
        float o = std::max(0.001f, organization);
        float t = std::max(0.001f, thoroughness);
        float r = std::max(0.001f, reliability);
        float e = std::max(0.001f, efficiency);
        float s = std::max(0.001f, scalability);
        return std::pow(o * t * r * e * s, 0.2f);
    }
};

// ============================================================================
// Marduk Interface — Abstract bridge to the Marduk cognitive architecture
// ============================================================================

/**
 * @brief Abstract Marduk interface for endocrine coupling
 *
 * The endocrine adapter reads execution metrics for feedback to the
 * hormone bus and writes hormonally-modulated parameters to control
 * the cognitive dynamics of the Left Hemisphere.
 */
struct MardukInterface {
    virtual ~MardukInterface() = default;

    // === Read (execution metrics for feedback) ===

    /// Current active operational mode
    [[nodiscard]] virtual MardukOperationalMode active_mode() const noexcept = 0;

    /// Current configuration (may be interpolated, not a pure preset)
    [[nodiscard]] virtual MardukEndocrineConfig current_config() const noexcept = 0;

    /// Current telemetry readings
    [[nodiscard]] virtual MardukTelemetry telemetry() const noexcept = 0;

    /// Current execution metrics
    [[nodiscard]] virtual ExecutionMetrics execution() const noexcept = 0;

    // === Write (endocrine modulation) ===

    /// Apply hormonally-modulated parameters
    virtual void apply_config(const MardukEndocrineConfig& cfg) = 0;

    /// Request an operational mode transition (may be gradual, not instant)
    virtual void transition_mode(MardukOperationalMode target) = 0;
};

} // namespace opencog::endo
