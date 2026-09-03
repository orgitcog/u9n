#pragma once
/**
 * @file type_conversion_bridge.hpp
 * @brief High-level orchestrator for Neural Tensor ↔ OpenCog Atom conversions
 *
 * Feature F1.1.3: Type Conversion Layer
 *
 * TypeConversionBridge provides:
 *   - Unified API combining tensor_to_atom and atom_to_tensor
 *   - Round-trip validation (tensor→atom→tensor, atom→tensor→atom)
 *   - Batch conversion
 *   - Conversion metrics tracking
 */

#include <opencog/bridge/tensor_atom_types.hpp>
#include <opencog/bridge/tensor_to_atom.hpp>
#include <opencog/bridge/atom_to_tensor.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/attention/attention_bank.hpp>

#include <Eigen/Core>

#include <chrono>
#include <string>
#include <vector>

namespace opencog::bridge {

/**
 * @brief Central bridge between Eigen tensors and OpenCog AtomSpace.
 *
 * Owns a reference to an AtomSpace and provides high-level conversion
 * methods with metrics tracking and round-trip validation.
 */
class TypeConversionBridge {
public:
    explicit TypeConversionBridge(AtomSpace& space,
                                  ConversionPolicy policy = {});
    ~TypeConversionBridge() = default;

    TypeConversionBridge(const TypeConversionBridge&) = delete;
    TypeConversionBridge& operator=(const TypeConversionBridge&) = delete;

    // ========================================================================
    // Tensor → Atom
    // ========================================================================

    /// Convert activation vector to ConceptNodes
    [[nodiscard]] ConversionResult<std::vector<Handle>>
    convert_activation(const Eigen::VectorXf& activation);

    /// Convert weight matrix to EvaluationLinks
    [[nodiscard]] ConversionResult<std::vector<Handle>>
    convert_weights(const Eigen::MatrixXf& weights);

    /// Convert ESN state to StateLink
    [[nodiscard]] ConversionResult<Handle>
    convert_state(const Eigen::VectorXf& state, const std::string& label = "reservoir");

    /// Convert temporal pattern to AtTimeLinks
    [[nodiscard]] ConversionResult<std::vector<Handle>>
    convert_temporal(const Eigen::MatrixXf& pattern, const std::string& label = "temporal");

    // ========================================================================
    // Atom → Tensor
    // ========================================================================

    /// Convert ConceptNodes to activation vector
    [[nodiscard]] ConversionResult<Eigen::VectorXf>
    extract_activation(const std::vector<Handle>& nodes);

    /// Convert EvaluationLinks to weight matrix
    [[nodiscard]] ConversionResult<Eigen::MatrixXf>
    extract_weights(const std::vector<Handle>& links);

    /// Convert StateLink to state vector
    [[nodiscard]] ConversionResult<Eigen::VectorXf>
    extract_state(Handle state_link);

    /// Convert attentional focus to activation vector
    [[nodiscard]] ConversionResult<Eigen::VectorXf>
    extract_attention_focus(const AttentionBank& bank, size_t max_size = 0);

    // ========================================================================
    // Round-Trip Validation
    // ========================================================================

    /**
     * @brief Validate tensor→atom→tensor round-trip fidelity.
     *
     * Converts the vector to atoms, then back. Returns true if
     * the maximum element-wise error is within policy.round_trip_epsilon.
     */
    [[nodiscard]] bool validate_roundtrip_activation(const Eigen::VectorXf& original);

    /**
     * @brief Validate atom→tensor→atom round-trip fidelity.
     *
     * Converts nodes to a vector, then back. Returns true if
     * the reconstructed atoms have matching TruthValue strengths.
     */
    [[nodiscard]] bool validate_roundtrip_nodes(const std::vector<Handle>& original);

    // ========================================================================
    // Batch Conversion
    // ========================================================================

    /**
     * @brief Convert multiple activation vectors in batch.
     * @return One result per input vector
     */
    [[nodiscard]] std::vector<ConversionResult<std::vector<Handle>>>
    batch_convert_activations(const std::vector<Eigen::VectorXf>& activations);

    // ========================================================================
    // Configuration & Metrics
    // ========================================================================

    [[nodiscard]] const ConversionPolicy& policy() const noexcept { return policy_; }
    void set_policy(ConversionPolicy policy) noexcept { policy_ = std::move(policy); }

    [[nodiscard]] const ConversionMetrics& metrics() const noexcept { return metrics_; }
    void reset_metrics() noexcept { metrics_.reset(); }

    [[nodiscard]] AtomSpace& atom_space() noexcept { return space_; }
    [[nodiscard]] const AtomSpace& atom_space() const noexcept { return space_; }

private:
    AtomSpace& space_;
    ConversionPolicy policy_;
    ConversionMetrics metrics_;

    /// Record a completed conversion in metrics
    template<typename T>
    void record(const ConversionResult<T>& result, bool is_tensor_to_atom,
                std::chrono::steady_clock::time_point start);
};

} // namespace opencog::bridge
