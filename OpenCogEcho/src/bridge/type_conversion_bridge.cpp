/**
 * @file type_conversion_bridge.cpp
 * @brief Implementation of the high-level TypeConversionBridge
 *
 * Feature F1.1.3: Type Conversion Layer
 */

#include <opencog/bridge/type_conversion_bridge.hpp>

#include <cmath>

namespace opencog::bridge {

// ============================================================================
// Construction
// ============================================================================

TypeConversionBridge::TypeConversionBridge(AtomSpace& space,
                                           ConversionPolicy policy)
    : space_(space)
    , policy_(std::move(policy))
{}

// ============================================================================
// Internal metrics recording
// ============================================================================

template<typename T>
void TypeConversionBridge::record(const ConversionResult<T>& result,
                                   bool is_tensor_to_atom,
                                   std::chrono::steady_clock::time_point start)
{
    auto elapsed = std::chrono::steady_clock::now() - start;
    double us = std::chrono::duration<double, std::micro>(elapsed).count();
    metrics_.total_latency_us += us;

    if (is_tensor_to_atom)
        ++metrics_.tensor_to_atom_count;
    else
        ++metrics_.atom_to_tensor_count;

    if (result.ok()) {
        metrics_.total_elements_converted += static_cast<uint64_t>(result.elements_converted);
        metrics_.total_elements_skipped += static_cast<uint64_t>(result.elements_skipped);
    } else {
        ++metrics_.error_count;
    }
}

// ============================================================================
// Tensor → Atom
// ============================================================================

ConversionResult<std::vector<Handle>>
TypeConversionBridge::convert_activation(const Eigen::VectorXf& activation) {
    auto start = std::chrono::steady_clock::now();
    auto result = activation_to_nodes(space_, activation, policy_);
    record(result, true, start);
    return result;
}

ConversionResult<std::vector<Handle>>
TypeConversionBridge::convert_weights(const Eigen::MatrixXf& weights) {
    auto start = std::chrono::steady_clock::now();
    auto result = matrix_to_links(space_, weights, policy_);
    record(result, true, start);
    return result;
}

ConversionResult<Handle>
TypeConversionBridge::convert_state(const Eigen::VectorXf& state,
                                     const std::string& label) {
    auto start = std::chrono::steady_clock::now();
    auto result = state_to_atom(space_, state, label, policy_);
    record(result, true, start);
    return result;
}

ConversionResult<std::vector<Handle>>
TypeConversionBridge::convert_temporal(const Eigen::MatrixXf& pattern,
                                        const std::string& label) {
    auto start = std::chrono::steady_clock::now();
    auto result = temporal_pattern_to_atoms(space_, pattern, label, policy_);
    record(result, true, start);
    return result;
}

// ============================================================================
// Atom → Tensor
// ============================================================================

ConversionResult<Eigen::VectorXf>
TypeConversionBridge::extract_activation(const std::vector<Handle>& nodes) {
    auto start = std::chrono::steady_clock::now();
    auto result = nodes_to_activation(space_, nodes);
    record(result, false, start);
    return result;
}

ConversionResult<Eigen::MatrixXf>
TypeConversionBridge::extract_weights(const std::vector<Handle>& links) {
    auto start = std::chrono::steady_clock::now();
    auto result = links_to_matrix(space_, links);
    record(result, false, start);
    return result;
}

ConversionResult<Eigen::VectorXf>
TypeConversionBridge::extract_state(Handle state_link) {
    auto start = std::chrono::steady_clock::now();
    auto result = atom_to_state(space_, state_link);
    record(result, false, start);
    return result;
}

ConversionResult<Eigen::VectorXf>
TypeConversionBridge::extract_attention_focus(const AttentionBank& bank,
                                               size_t max_size) {
    auto start = std::chrono::steady_clock::now();
    auto result = attention_focus_to_vector(space_, bank, max_size);
    record(result, false, start);
    return result;
}

// ============================================================================
// Round-Trip Validation
// ============================================================================

bool TypeConversionBridge::validate_roundtrip_activation(const Eigen::VectorXf& original) {
    // Use a strict policy for round-trip: no thresholding
    ConversionPolicy strict = policy_;
    strict.activation_threshold = 0.0f;
    strict.normalize = false;

    // Tensor → Atom
    auto fwd = activation_to_nodes(space_, original, strict);
    if (!fwd.ok()) return false;

    // Atom → Tensor
    auto rev = nodes_to_activation(space_, fwd.value);
    if (!rev.ok()) return false;

    // Compare element-wise
    if (rev.value.size() != original.size()) return false;

    // The stored TruthValue strength is clamped to [0,1], so we only
    // compare values that survive that clamping.
    for (int i = 0; i < original.size(); ++i) {
        float expected = std::clamp(original[i], 0.0f, 1.0f);
        if (std::abs(rev.value[i] - expected) > strict.round_trip_epsilon)
            return false;
    }
    return true;
}

bool TypeConversionBridge::validate_roundtrip_nodes(const std::vector<Handle>& original) {
    // Atom → Tensor
    auto fwd = nodes_to_activation(space_, original);
    if (!fwd.ok()) return false;

    // Tensor → Atom (create new nodes with a distinct prefix to avoid name collisions)
    ConversionPolicy tmp = policy_;
    tmp.activation_threshold = 0.0f;
    tmp.name_prefix = "_roundtrip_check";

    auto rev = activation_to_nodes(space_, fwd.value, tmp);
    if (!rev.ok()) return false;

    // Extract again
    auto check = nodes_to_activation(space_, rev.value);
    if (!check.ok()) return false;

    if (check.value.size() != fwd.value.size()) return false;

    for (int i = 0; i < fwd.value.size(); ++i) {
        float expected = std::clamp(fwd.value[i], 0.0f, 1.0f);
        if (std::abs(check.value[i] - expected) > policy_.round_trip_epsilon)
            return false;
    }
    return true;
}

// ============================================================================
// Batch Conversion
// ============================================================================

std::vector<ConversionResult<std::vector<Handle>>>
TypeConversionBridge::batch_convert_activations(
    const std::vector<Eigen::VectorXf>& activations)
{
    std::vector<ConversionResult<std::vector<Handle>>> results;
    results.reserve(activations.size());

    for (auto& vec : activations) {
        results.push_back(convert_activation(vec));
    }
    return results;
}

// Explicit template instantiations for the private record method
template void TypeConversionBridge::record<std::vector<Handle>>(
    const ConversionResult<std::vector<Handle>>&, bool, std::chrono::steady_clock::time_point);
template void TypeConversionBridge::record<Handle>(
    const ConversionResult<Handle>&, bool, std::chrono::steady_clock::time_point);
template void TypeConversionBridge::record<Eigen::VectorXf>(
    const ConversionResult<Eigen::VectorXf>&, bool, std::chrono::steady_clock::time_point);
template void TypeConversionBridge::record<Eigen::MatrixXf>(
    const ConversionResult<Eigen::MatrixXf>&, bool, std::chrono::steady_clock::time_point);

} // namespace opencog::bridge
