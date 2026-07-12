/**
 * @file tensor_to_atom.cpp
 * @brief Implementation of Eigen tensor → OpenCog atom conversions
 *
 * Feature F1.1.3: Type Conversion Layer
 */

#include <opencog/bridge/tensor_to_atom.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <sstream>
#include <string>

namespace opencog::bridge {

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Build a node name like "neuron_42"
[[nodiscard]] std::string make_name(const std::string& prefix, int index) {
    return prefix + "_" + std::to_string(index);
}

/// Convert float to string with full precision via std::to_chars
[[nodiscard]] std::string float_to_string(float val) {
    std::array<char, 32> buf{};
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    if (ec == std::errc{})
        return std::string(buf.data(), ptr);
    // Fallback (should never happen for finite floats)
    return std::to_string(val);
}

/// Compute confidence from activation magnitude when policy requests it
[[nodiscard]] float derive_confidence(float activation, const ConversionPolicy& policy) {
    if (policy.derive_confidence_from_magnitude) {
        // Map absolute activation to confidence via sigmoid-like curve
        float abs_val = std::abs(activation);
        return abs_val / (abs_val + 1.0f);  // soft saturation in [0, 1)
    }
    return policy.default_confidence;
}

/// Check a vector for NaN/Inf
[[nodiscard]] ConversionError validate_vector(const Eigen::VectorXf& v) {
    for (int i = 0; i < v.size(); ++i) {
        if (std::isnan(v[i])) return ConversionError::NaNDetected;
        if (std::isinf(v[i])) return ConversionError::InfDetected;
    }
    return ConversionError::None;
}

/// Check a matrix for NaN/Inf
[[nodiscard]] ConversionError validate_matrix(const Eigen::MatrixXf& m) {
    for (int r = 0; r < m.rows(); ++r)
        for (int c = 0; c < m.cols(); ++c) {
            if (std::isnan(m(r, c))) return ConversionError::NaNDetected;
            if (std::isinf(m(r, c))) return ConversionError::InfDetected;
        }
    return ConversionError::None;
}

/// Normalize a vector to [0, 1] in-place (returns a copy)
[[nodiscard]] Eigen::VectorXf normalize_vec(const Eigen::VectorXf& v) {
    float mn = v.minCoeff();
    float mx = v.maxCoeff();
    if (mx - mn < 1e-12f) return Eigen::VectorXf::Constant(v.size(), 0.5f);
    return (v.array() - mn) / (mx - mn);
}

} // anonymous namespace

// ============================================================================
// activation_to_nodes
// ============================================================================

ConversionResult<std::vector<Handle>>
activation_to_nodes(AtomSpace& space,
                    const Eigen::VectorXf& vec,
                    const ConversionPolicy& policy)
{
    if (vec.size() == 0)
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::EmptyInput);

    if (auto err = validate_vector(vec); err != ConversionError::None)
        return ConversionResult<std::vector<Handle>>::fail(err);

    Eigen::VectorXf data = policy.normalize ? normalize_vec(vec) : vec;

    std::vector<Handle> handles;
    handles.reserve(static_cast<size_t>(data.size()));
    int skipped = 0;

    for (int i = 0; i < data.size(); ++i) {
        float val = data[i];
        if (std::abs(val) < policy.activation_threshold) {
            ++skipped;
            continue;
        }

        std::string name = make_name(policy.name_prefix, i);
        float strength = std::clamp(val, 0.0f, 1.0f);
        float confidence = derive_confidence(val, policy);

        Handle h = space.add_node(AtomType::CONCEPT_NODE, name,
                                  TruthValue{strength, confidence});
        // Ensure TV is updated even if the atom already existed (AtomTable
        // deduplicates by type+name and returns the existing atom unchanged)
        space.set_tv(h, TruthValue{strength, confidence});

        if (policy.map_activation_to_sti) {
            AttentionValue av{val * policy.sti_scale, 0, 0};
            space.set_av(h, av);
        }

        handles.push_back(h);
    }

    if (handles.empty())
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::ThresholdFiltered);

    auto quality = (skipped == 0) ? ConversionQuality::Lossless
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<std::vector<Handle>>::success(
        std::move(handles), static_cast<int>(handles.size()), skipped, quality);
}

// ============================================================================
// matrix_to_links
// ============================================================================

ConversionResult<std::vector<Handle>>
matrix_to_links(AtomSpace& space,
                const Eigen::MatrixXf& mat,
                const ConversionPolicy& policy)
{
    if (mat.rows() == 0 || mat.cols() == 0)
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::EmptyInput);

    if (auto err = validate_matrix(mat); err != ConversionError::None)
        return ConversionResult<std::vector<Handle>>::fail(err);

    // Predicate node for the weight relation
    Handle pred = space.add_node(AtomType::PREDICATE_NODE,
                                 policy.name_prefix + "_weight");

    std::vector<Handle> links;
    links.reserve(static_cast<size_t>(mat.rows() * mat.cols()));
    int skipped = 0;

    for (int r = 0; r < mat.rows(); ++r) {
        for (int c = 0; c < mat.cols(); ++c) {
            float w = mat(r, c);
            if (std::abs(w) < policy.activation_threshold) {
                ++skipped;
                continue;
            }

            Handle src = space.add_node(AtomType::CONCEPT_NODE,
                                        make_name(policy.name_prefix + "_src", r));
            Handle tgt = space.add_node(AtomType::CONCEPT_NODE,
                                        make_name(policy.name_prefix + "_tgt", c));
            Handle list = space.add_link(AtomType::LIST_LINK, {src, tgt});

            float strength = std::clamp(std::abs(w), 0.0f, 1.0f);
            float confidence = derive_confidence(w, policy);
            Handle eval = space.add_link(AtomType::EVALUATION_LINK, {pred, list},
                                         TruthValue{strength, confidence});
            // Ensure TV is updated for pre-existing links
            space.set_tv(eval, TruthValue{strength, confidence});
            links.push_back(eval);
        }
    }

    if (links.empty())
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::ThresholdFiltered);

    auto quality = (skipped == 0) ? ConversionQuality::Lossless
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<std::vector<Handle>>::success(
        std::move(links), static_cast<int>(links.size()), skipped, quality);
}

// ============================================================================
// state_to_atom
// ============================================================================

ConversionResult<Handle>
state_to_atom(AtomSpace& space,
              const Eigen::VectorXf& state,
              const std::string& label,
              const ConversionPolicy& policy)
{
    if (state.size() == 0)
        return ConversionResult<Handle>::fail(ConversionError::EmptyInput);

    if (auto err = validate_vector(state); err != ConversionError::None)
        return ConversionResult<Handle>::fail(err);

    // Create number nodes for each element
    std::vector<Handle> elements;
    elements.reserve(static_cast<size_t>(state.size()));
    for (int i = 0; i < state.size(); ++i) {
        std::string num_name = float_to_string(state[i]);
        Handle num = space.add_node(AtomType::NUMBER_NODE, num_name);
        elements.push_back(num);
    }

    // Pack elements into a ListLink
    std::vector<AtomId> ids;
    ids.reserve(elements.size());
    for (auto& h : elements) ids.push_back(h.id());
    Handle list = space.add_link(AtomType::LIST_LINK, std::span<const AtomId>{ids});

    // Anchor for the state
    Handle anchor = space.add_node(AtomType::ANCHOR_NODE, label);

    // Remove any previous StateLink for this anchor so we don't accumulate
    // stale state atoms (StateLink semantics expect one state per key).
    auto incoming = space.get_incoming_by_type(anchor, AtomType::STATE_LINK);
    for (auto& old_link : incoming) {
        space.remove(old_link, false);
    }

    // StateLink: (StateLink anchor list)
    Handle state_link = space.add_link(AtomType::STATE_LINK, {anchor, list});

    // Set truth value encoding the vector's L2 norm (normalized to [0,1])
    float norm = state.norm();
    float max_possible = std::sqrt(static_cast<float>(state.size())); // max norm if all 1s
    float normalized_norm = (max_possible > 0.0f) ? std::clamp(norm / max_possible, 0.0f, 1.0f) : 0.0f;
    space.set_tv(state_link, TruthValue{normalized_norm, policy.default_confidence});

    if (policy.map_activation_to_sti) {
        float energy = state.squaredNorm() / static_cast<float>(state.size());
        space.set_av(state_link, AttentionValue{energy * policy.sti_scale, 0, 0});
    }

    return ConversionResult<Handle>::success(
        state_link, static_cast<int>(state.size()), 0, ConversionQuality::HighPrecision);
}

// ============================================================================
// temporal_pattern_to_atoms
// ============================================================================

ConversionResult<std::vector<Handle>>
temporal_pattern_to_atoms(AtomSpace& space,
                          const Eigen::MatrixXf& pattern,
                          const std::string& label,
                          const ConversionPolicy& policy)
{
    if (pattern.rows() == 0 || pattern.cols() == 0)
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::EmptyInput);

    if (auto err = validate_matrix(pattern); err != ConversionError::None)
        return ConversionResult<std::vector<Handle>>::fail(err);

    std::vector<Handle> at_time_links;
    at_time_links.reserve(static_cast<size_t>(pattern.rows()));
    int total_skipped = 0;

    for (int t = 0; t < pattern.rows(); ++t) {
        // Create a NumberNode for the timestep
        Handle time_node = space.add_node(AtomType::NUMBER_NODE, std::to_string(t));

        // Create ConceptNodes for features at this timestep
        std::vector<Handle> feature_handles;
        int skipped_this_step = 0;
        for (int f = 0; f < pattern.cols(); ++f) {
            float val = pattern(t, f);
            if (std::abs(val) < policy.activation_threshold) {
                ++skipped_this_step;
                continue;
            }
            std::string name = make_name(label + "_t" + std::to_string(t), f);
            float strength = std::clamp(val, 0.0f, 1.0f);
            Handle h = space.add_node(AtomType::CONCEPT_NODE, name,
                                      TruthValue{strength, policy.default_confidence});
            // Ensure TV is updated for pre-existing atoms
            space.set_tv(h, TruthValue{strength, policy.default_confidence});
            feature_handles.push_back(h);
        }
        total_skipped += skipped_this_step;

        if (feature_handles.empty()) continue;

        // Pack features into a ListLink
        std::vector<AtomId> ids;
        ids.reserve(feature_handles.size());
        for (auto& h : feature_handles) ids.push_back(h.id());
        Handle list = space.add_link(AtomType::LIST_LINK, std::span<const AtomId>{ids});

        // AtTimeLink: (AtTimeLink time_node list)
        Handle at_time = space.add_link(AtomType::AT_TIME_LINK, {time_node, list});
        at_time_links.push_back(at_time);
    }

    if (at_time_links.empty())
        return ConversionResult<std::vector<Handle>>::fail(ConversionError::ThresholdFiltered);

    auto quality = (total_skipped == 0) ? ConversionQuality::Lossless
                                         : ConversionQuality::NormalPrecision;
    return ConversionResult<std::vector<Handle>>::success(
        std::move(at_time_links), static_cast<int>(at_time_links.size()),
        total_skipped, quality);
}

} // namespace opencog::bridge
