/**
 * @file test_type_conversion_bridge.cpp
 * @brief Tests for the F1.1.3 Type Conversion Layer
 *
 * Covers:
 *   - Tensor → Atom conversions (activation, weight, state, temporal)
 *   - Atom → Tensor conversions (nodes, links, state, attention)
 *   - Round-trip fidelity (tensor→atom→tensor, atom→tensor→atom)
 *   - Edge cases (empty, NaN, single element, large dimensions)
 *   - Batch conversion
 *   - Metrics tracking
 */

#include <opencog/bridge/type_conversion_bridge.hpp>
#include <opencog/bridge/tensor_to_atom.hpp>
#include <opencog/bridge/atom_to_tensor.hpp>
#include <opencog/bridge/tensor_atom_types.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/attention/attention_bank.hpp>

#include <Eigen/Core>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace test {
extern bool register_test(const std::string& name, std::function<bool()> func);
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_NE(a, b) if ((a) == (b)) { return false; }
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }
#define ASSERT_GT(a, b) if (!((a) > (b))) { return false; }

using namespace opencog;
using namespace opencog::bridge;

// ============================================================================
// Tensor → Atom: activation_to_nodes
// ============================================================================

TEST(activation_to_nodes_basic) {
    AtomSpace space;
    Eigen::VectorXf v(3);
    v << 0.5f, 0.8f, 0.3f;

    auto result = activation_to_nodes(space, v);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 3);
    ASSERT_EQ(result.elements_converted, 3);
    ASSERT_EQ(result.elements_skipped, 0);

    // Verify TruthValue strengths
    for (int i = 0; i < 3; ++i) {
        TruthValue tv = space.get_tv(result.value[static_cast<size_t>(i)]);
        ASSERT_NEAR(tv.strength, v[i], 1e-5f);
    }
    return true;
}

TEST(activation_to_nodes_threshold) {
    AtomSpace space;
    Eigen::VectorXf v(4);
    v << 0.5f, 0.005f, 0.8f, 0.001f;  // Two below default threshold 0.01

    ConversionPolicy policy;
    policy.activation_threshold = 0.01f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 2);  // Only 0.5 and 0.8
    ASSERT_EQ(result.elements_skipped, 2);
    return true;
}

TEST(activation_to_nodes_sti_mapping) {
    AtomSpace space;
    Eigen::VectorXf v(2);
    v << 0.7f, 0.3f;

    ConversionPolicy policy;
    policy.map_activation_to_sti = true;
    policy.sti_scale = 2.0f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());

    AttentionValue av0 = space.get_av(result.value[0]);
    ASSERT_NEAR(av0.sti, 0.7f * 2.0f, 1e-5f);

    AttentionValue av1 = space.get_av(result.value[1]);
    ASSERT_NEAR(av1.sti, 0.3f * 2.0f, 1e-5f);
    return true;
}

TEST(activation_to_nodes_empty) {
    AtomSpace space;
    Eigen::VectorXf v(0);
    auto result = activation_to_nodes(space, v);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::EmptyInput);
    return true;
}

TEST(activation_to_nodes_nan) {
    AtomSpace space;
    Eigen::VectorXf v(2);
    v << 0.5f, std::numeric_limits<float>::quiet_NaN();
    auto result = activation_to_nodes(space, v);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::NaNDetected);
    return true;
}

TEST(activation_to_nodes_all_below_threshold) {
    AtomSpace space;
    Eigen::VectorXf v(3);
    v << 0.001f, 0.002f, 0.003f;

    ConversionPolicy policy;
    policy.activation_threshold = 0.01f;
    auto result = activation_to_nodes(space, v, policy);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::ThresholdFiltered);
    return true;
}

TEST(activation_to_nodes_normalize) {
    AtomSpace space;
    Eigen::VectorXf v(3);
    v << 2.0f, 4.0f, 6.0f;  // Not in [0,1]

    ConversionPolicy policy;
    policy.normalize = true;
    policy.activation_threshold = 0.0f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 3);

    // After normalization: [0, 0.5, 1.0]
    TruthValue tv0 = space.get_tv(result.value[0]);
    ASSERT_NEAR(tv0.strength, 0.0f, 1e-5f);

    TruthValue tv2 = space.get_tv(result.value[2]);
    ASSERT_NEAR(tv2.strength, 1.0f, 1e-5f);
    return true;
}

// ============================================================================
// Tensor → Atom: matrix_to_links
// ============================================================================

TEST(matrix_to_links_basic) {
    AtomSpace space;
    Eigen::MatrixXf m(2, 2);
    m << 0.5f, 0.3f,
         0.7f, 0.9f;

    ConversionPolicy policy;
    policy.activation_threshold = 0.0f;

    auto result = matrix_to_links(space, m, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 4);
    ASSERT_EQ(result.elements_skipped, 0);
    return true;
}

TEST(matrix_to_links_sparse) {
    AtomSpace space;
    Eigen::MatrixXf m(3, 3);
    m << 0.5f, 0.0f, 0.0f,
         0.0f, 0.8f, 0.0f,
         0.0f, 0.0f, 0.3f;

    ConversionPolicy policy;
    policy.activation_threshold = 0.01f;

    auto result = matrix_to_links(space, m, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 3);  // Only diagonal
    ASSERT_EQ(result.elements_skipped, 6);
    return true;
}

TEST(matrix_to_links_empty) {
    AtomSpace space;
    Eigen::MatrixXf m(0, 0);
    auto result = matrix_to_links(space, m);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::EmptyInput);
    return true;
}

// ============================================================================
// Tensor → Atom: state_to_atom
// ============================================================================

TEST(state_to_atom_basic) {
    AtomSpace space;
    Eigen::VectorXf state(4);
    state << 0.1f, 0.2f, 0.3f, 0.4f;

    auto result = state_to_atom(space, state, "esn_stream1");
    ASSERT(result.ok());
    ASSERT(space.contains(result.value));

    // Verify structure: StateLink → (AnchorNode, ListLink)
    auto outgoing = space.get_outgoing(result.value);
    ASSERT_EQ(outgoing.size(), 2u);

    // Anchor name
    auto anchor_name = space.get_name(outgoing[0]);
    ASSERT_EQ(std::string(anchor_name), "esn_stream1");

    // ListLink should have 4 NumberNode children
    auto list_out = space.get_outgoing(outgoing[1]);
    ASSERT_EQ(list_out.size(), 4u);
    return true;
}

TEST(state_to_atom_empty) {
    AtomSpace space;
    Eigen::VectorXf state(0);
    auto result = state_to_atom(space, state);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::EmptyInput);
    return true;
}

// ============================================================================
// Tensor → Atom: temporal_pattern_to_atoms
// ============================================================================

TEST(temporal_pattern_basic) {
    AtomSpace space;
    Eigen::MatrixXf pattern(3, 2);
    pattern << 0.5f, 0.3f,
               0.7f, 0.1f,
               0.9f, 0.8f;

    ConversionPolicy policy;
    policy.activation_threshold = 0.0f;

    auto result = temporal_pattern_to_atoms(space, pattern, "seq", policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 3);  // 3 timesteps
    return true;
}

// ============================================================================
// Atom → Tensor: nodes_to_activation
// ============================================================================

TEST(nodes_to_activation_basic) {
    AtomSpace space;

    // Create ConceptNodes with known TruthValues
    Handle a = space.add_node(AtomType::CONCEPT_NODE, "alpha", TruthValue{0.9f, 0.8f});
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "beta", TruthValue{0.4f, 0.7f});
    Handle c = space.add_node(AtomType::CONCEPT_NODE, "gamma", TruthValue{0.1f, 0.6f});

    auto result = nodes_to_activation(space, {a, b, c});
    ASSERT(result.ok());
    ASSERT_EQ(result.value.size(), 3);
    ASSERT_NEAR(result.value[0], 0.9f, 1e-5f);
    ASSERT_NEAR(result.value[1], 0.4f, 1e-5f);
    ASSERT_NEAR(result.value[2], 0.1f, 1e-5f);
    return true;
}

TEST(nodes_to_activation_empty) {
    AtomSpace space;
    auto result = nodes_to_activation(space, {});
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::EmptyInput);
    return true;
}

// ============================================================================
// Atom → Tensor: links_to_matrix
// ============================================================================

TEST(links_to_matrix_basic) {
    AtomSpace space;

    Handle pred = space.add_node(AtomType::PREDICATE_NODE, "weight");
    Handle s0 = space.add_node(AtomType::CONCEPT_NODE, "src_0");
    Handle s1 = space.add_node(AtomType::CONCEPT_NODE, "src_1");
    Handle t0 = space.add_node(AtomType::CONCEPT_NODE, "tgt_0");
    Handle t1 = space.add_node(AtomType::CONCEPT_NODE, "tgt_1");

    Handle l00 = space.add_link(AtomType::LIST_LINK, {s0, t0});
    Handle l01 = space.add_link(AtomType::LIST_LINK, {s0, t1});
    Handle l10 = space.add_link(AtomType::LIST_LINK, {s1, t0});

    Handle e00 = space.add_link(AtomType::EVALUATION_LINK, {pred, l00}, TruthValue{0.5f, 0.9f});
    Handle e01 = space.add_link(AtomType::EVALUATION_LINK, {pred, l01}, TruthValue{0.3f, 0.9f});
    Handle e10 = space.add_link(AtomType::EVALUATION_LINK, {pred, l10}, TruthValue{0.8f, 0.9f});

    auto result = links_to_matrix(space, {e00, e01, e10});
    ASSERT(result.ok());
    ASSERT_EQ(result.value.rows(), 2);  // 2 sources
    ASSERT_EQ(result.value.cols(), 2);  // 2 targets
    ASSERT_EQ(result.elements_converted, 3);
    return true;
}

// ============================================================================
// Atom → Tensor: atom_to_state
// ============================================================================

TEST(atom_to_state_roundtrip) {
    AtomSpace space;
    Eigen::VectorXf original(4);
    original << 0.1f, 0.25f, 0.5f, 0.75f;

    auto fwd = state_to_atom(space, original, "test_state");
    ASSERT(fwd.ok());

    auto rev = atom_to_state(space, fwd.value);
    ASSERT(rev.ok());
    ASSERT_EQ(rev.value.size(), 4);

    for (int i = 0; i < 4; ++i) {
        ASSERT_NEAR(rev.value[i], original[i], 1e-4f);
    }
    return true;
}

TEST(atom_to_state_not_found) {
    AtomSpace space;
    Handle invalid;
    auto result = atom_to_state(space, invalid);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::AtomNotFound);
    return true;
}

// ============================================================================
// Atom → Tensor: attention_focus_to_vector
// ============================================================================

TEST(attention_focus_basic) {
    AtomSpace space;
    AttentionBank bank(space);

    // Create atoms and stimulate them into AF
    Handle a = space.add_node(AtomType::CONCEPT_NODE, "focus_a");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "focus_b");

    bank.stimulate(a.id(), 50.0f);
    bank.stimulate(b.id(), 30.0f);

    auto result = attention_focus_to_vector(space, bank);
    ASSERT(result.ok());
    ASSERT_GT(result.value.size(), 0);
    return true;
}

TEST(attention_focus_empty) {
    AtomSpace space;
    ECANConfig config;
    config.af_boundary = 1000.0f;  // Very high threshold — no atoms in AF
    AttentionBank bank(space, config);

    auto result = attention_focus_to_vector(space, bank);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::EmptyInput);
    return true;
}

// ============================================================================
// TypeConversionBridge: High-level API
// ============================================================================

TEST(bridge_convert_activation) {
    AtomSpace space;
    TypeConversionBridge bridge(space);

    Eigen::VectorXf v(3);
    v << 0.4f, 0.6f, 0.8f;

    auto result = bridge.convert_activation(v);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 3);

    auto& m = bridge.metrics();
    ASSERT_EQ(m.tensor_to_atom_count, 1u);
    ASSERT_GT(m.total_elements_converted, 0u);
    return true;
}

TEST(bridge_convert_weights) {
    AtomSpace space;
    ConversionPolicy policy;
    policy.activation_threshold = 0.0f;
    TypeConversionBridge bridge(space, policy);

    Eigen::MatrixXf w(2, 2);
    w << 0.5f, 0.3f,
         0.7f, 0.9f;

    auto result = bridge.convert_weights(w);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 4);
    return true;
}

TEST(bridge_extract_activation) {
    AtomSpace space;
    TypeConversionBridge bridge(space);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "x1", TruthValue{0.5f, 0.9f});
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "x2", TruthValue{0.8f, 0.9f});

    auto result = bridge.extract_activation({a, b});
    ASSERT(result.ok());
    ASSERT_EQ(result.value.size(), 2);
    ASSERT_NEAR(result.value[0], 0.5f, 1e-5f);
    ASSERT_NEAR(result.value[1], 0.8f, 1e-5f);

    ASSERT_EQ(bridge.metrics().atom_to_tensor_count, 1u);
    return true;
}

// ============================================================================
// Round-Trip Validation
// ============================================================================

TEST(roundtrip_activation) {
    AtomSpace space;
    ConversionPolicy policy = ConversionPolicy::strict();
    TypeConversionBridge bridge(space, policy);

    Eigen::VectorXf v(5);
    v << 0.1f, 0.3f, 0.5f, 0.7f, 0.9f;

    ASSERT(bridge.validate_roundtrip_activation(v));
    return true;
}

TEST(roundtrip_nodes) {
    AtomSpace space;
    ConversionPolicy policy = ConversionPolicy::strict();
    TypeConversionBridge bridge(space, policy);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "rt_a", TruthValue{0.3f, 0.9f});
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "rt_b", TruthValue{0.7f, 0.9f});
    Handle c = space.add_node(AtomType::CONCEPT_NODE, "rt_c", TruthValue{0.5f, 0.9f});

    ASSERT(bridge.validate_roundtrip_nodes({a, b, c}));
    return true;
}

TEST(roundtrip_state) {
    AtomSpace space;
    TypeConversionBridge bridge(space);

    Eigen::VectorXf state(4);
    state << 0.1f, 0.25f, 0.5f, 0.75f;

    auto fwd = bridge.convert_state(state, "rt_state");
    ASSERT(fwd.ok());

    auto rev = bridge.extract_state(fwd.value);
    ASSERT(rev.ok());
    ASSERT_EQ(rev.value.size(), 4);

    for (int i = 0; i < 4; ++i) {
        ASSERT_NEAR(rev.value[i], state[i], 1e-4f);
    }
    return true;
}

// ============================================================================
// Batch Conversion
// ============================================================================

TEST(batch_convert) {
    AtomSpace space;
    TypeConversionBridge bridge(space);

    std::vector<Eigen::VectorXf> batch;
    for (int b = 0; b < 5; ++b) {
        Eigen::VectorXf v(3);
        v << 0.1f * (b + 1), 0.2f * (b + 1), 0.3f * (b + 1);
        batch.push_back(v);
    }

    auto results = bridge.batch_convert_activations(batch);
    ASSERT_EQ(static_cast<int>(results.size()), 5);
    for (auto& r : results) {
        ASSERT(r.ok());
    }

    ASSERT_EQ(bridge.metrics().tensor_to_atom_count, 5u);
    return true;
}

// ============================================================================
// Metrics
// ============================================================================

TEST(metrics_tracking) {
    AtomSpace space;
    TypeConversionBridge bridge(space);

    Eigen::VectorXf v(2);
    v << 0.5f, 0.8f;

    bridge.convert_activation(v);
    bridge.convert_activation(v);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "m1", TruthValue{0.5f, 0.9f});
    bridge.extract_activation({a});

    auto& m = bridge.metrics();
    ASSERT_EQ(m.tensor_to_atom_count, 2u);
    ASSERT_EQ(m.atom_to_tensor_count, 1u);
    ASSERT_GT(m.total_latency_us, 0.0);
    ASSERT_GT(m.average_latency_us(), 0.0);

    bridge.reset_metrics();
    ASSERT_EQ(bridge.metrics().tensor_to_atom_count, 0u);
    return true;
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(single_element_vector) {
    AtomSpace space;
    Eigen::VectorXf v(1);
    v << 0.42f;

    ConversionPolicy policy;
    policy.activation_threshold = 0.0f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), 1);

    TruthValue tv = space.get_tv(result.value[0]);
    ASSERT_NEAR(tv.strength, 0.42f, 1e-5f);
    return true;
}

TEST(large_vector) {
    AtomSpace space;
    int N = 500;
    Eigen::VectorXf v = Eigen::VectorXf::Random(N).cwiseAbs();
    // Clamp to [0, 1]
    v = v.array().min(1.0f).max(0.0f);

    ConversionPolicy policy;
    policy.activation_threshold = 0.0f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());
    ASSERT_EQ(static_cast<int>(result.value.size()), N);
    return true;
}

TEST(inf_detection) {
    AtomSpace space;
    Eigen::VectorXf v(2);
    v << 0.5f, std::numeric_limits<float>::infinity();
    auto result = activation_to_nodes(space, v);
    ASSERT(!result.ok());
    ASSERT_EQ(result.error, ConversionError::InfDetected);
    return true;
}

TEST(derive_confidence_from_magnitude) {
    AtomSpace space;
    Eigen::VectorXf v(2);
    v << 0.1f, 0.9f;

    ConversionPolicy policy;
    policy.derive_confidence_from_magnitude = true;
    policy.activation_threshold = 0.0f;

    auto result = activation_to_nodes(space, v, policy);
    ASSERT(result.ok());

    // Confidence = |val| / (|val| + 1)
    TruthValue tv0 = space.get_tv(result.value[0]);
    ASSERT_NEAR(tv0.confidence, 0.1f / 1.1f, 1e-4f);

    TruthValue tv1 = space.get_tv(result.value[1]);
    ASSERT_NEAR(tv1.confidence, 0.9f / 1.9f, 1e-4f);
    return true;
}

// ============================================================================
// ConversionPolicy presets
// ============================================================================

TEST(policy_strict) {
    auto p = ConversionPolicy::strict();
    ASSERT_NEAR(p.activation_threshold, 0.0f, 1e-9f);
    ASSERT(!p.normalize);
    ASSERT_NEAR(p.round_trip_epsilon, 1e-6f, 1e-9f);
    return true;
}

TEST(policy_relaxed) {
    auto p = ConversionPolicy::relaxed();
    ASSERT_NEAR(p.activation_threshold, 0.1f, 1e-5f);
    ASSERT(p.normalize);
    return true;
}

// ============================================================================
// Error name helper
// ============================================================================

TEST(error_name_coverage) {
    ASSERT_EQ(error_name(ConversionError::None), "None");
    ASSERT_EQ(error_name(ConversionError::EmptyInput), "EmptyInput");
    ASSERT_EQ(error_name(ConversionError::NaNDetected), "NaNDetected");
    ASSERT_EQ(error_name(ConversionError::InternalError), "InternalError");
    return true;
}
