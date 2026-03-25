#pragma once
/**
 * @file npu_types.hpp
 * @brief Abstract NPU interface and types for endocrine integration
 *
 * Defines the abstract NPUInterface that decouples the endocrine system
 * from the concrete ggnucash::vdev::LlamaCoprocessorDriver. Any NPU
 * implementation (MMIO-backed, stub, or remote) can satisfy this interface
 * to receive hormonal modulation and feed telemetry back to the bus.
 */

#include <cstdint>

namespace opencog::endo {

// ============================================================================
// NPU Telemetry — Read-only view of NPU operational state
// ============================================================================

/// Snapshot of NPU performance and health for endocrine feedback
struct NPUTelemetry {
    float tokens_per_second{0.0f};     ///< Current inference throughput
    float context_utilization{0.0f};   ///< [0, 1]: fraction of context window used
    bool  is_busy{false};              ///< Currently performing inference
    bool  has_error{false};            ///< In error state
    uint64_t total_tokens{0};          ///< Lifetime tokens generated
    uint64_t error_count{0};           ///< Lifetime error count
};

// ============================================================================
// NPU Endocrine Configuration — Writable parameters from hormones
// ============================================================================

/// Configuration parameters that the endocrine adapter can modulate
struct NPUEndocrineConfig {
    int32_t n_predict{128};            ///< Max tokens to generate
    int32_t batch_size{1};             ///< Processing batch size
    float   creativity{0.5f};          ///< Temperature analog [0, 1]
    int     system_prompt_index{0};    ///< Persona-driven prompt selection
    float   context_pressure{0.5f};    ///< How aggressively to use context [0, 1]
};

// ============================================================================
// NPU Interface — Abstract bridge to any NPU implementation
// ============================================================================

/**
 * @brief Abstract NPU interface for endocrine coupling
 *
 * Decouples the endocrine system from the concrete NPU driver
 * (ggnucash::vdev::LlamaCoprocessorDriver). Implementations wrap the
 * real driver and expose the minimal surface needed for hormonal
 * modulation and telemetry feedback.
 *
 * The read methods are called each tick for telemetry feedback.
 * The write methods are called each tick for hormonal modulation.
 */
struct NPUInterface {
    virtual ~NPUInterface() = default;

    // === Read (telemetry for feedback) ===

    /// Get current telemetry snapshot
    [[nodiscard]] virtual NPUTelemetry telemetry() const noexcept = 0;

    // === Write (endocrine modulation) ===

    /// Apply hormonally-modulated configuration
    virtual void apply_config(const NPUEndocrineConfig& cfg) = 0;

    /// Get current modulated configuration
    [[nodiscard]] virtual NPUEndocrineConfig current_config() const noexcept = 0;
};

} // namespace opencog::endo
