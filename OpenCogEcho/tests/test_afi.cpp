/**
 * @file test_afi.cpp
 * @brief Tests for Active Free-energy Inference (AFI) types and utilities
 *
 * Covers: MarkovBlanket construction/merge/validation, FreeEnergy computation,
 * FreeEnergyMinimizer, precision weighting, STI<->precision mapping,
 * attention allocation, and GenerativeModelState prediction error.
 */

#include <opencog/afi/types.hpp>
#include <opencog/afi/blanket.hpp>
#include <opencog/afi/free_energy.hpp>
#include <opencog/afi/precision.hpp>

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
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }

using namespace opencog::afi;
using namespace opencog;

// ============================================================================
// MarkovBlanket Tests
// ============================================================================

TEST(afi_blanket_empty_size_zero) {
    MarkovBlanket blanket;
    ASSERT_EQ(blanket.blanket_size(), 0u);
    ASSERT(blanket.empty());
    return true;
}

TEST(afi_blanket_create_with_states) {
    auto blanket = MarkovBlanketFactory::create(
        {AtomId(1), AtomId(2)},   // sensory
        {AtomId(3)},              // active
        {AtomId(4), AtomId(5)},   // internal
        {AtomId(6)}               // external
    );
    ASSERT_EQ(blanket.sensory_states.size(), 2u);
    ASSERT_EQ(blanket.active_states.size(), 1u);
    ASSERT_EQ(blanket.internal_states.size(), 2u);
    ASSERT_EQ(blanket.external_states.size(), 1u);
    return true;
}

TEST(afi_blanket_size_sensory_plus_active) {
    auto blanket = MarkovBlanketFactory::create(
        {AtomId(1), AtomId(2), AtomId(3)},  // 3 sensory
        {AtomId(4), AtomId(5)},              // 2 active
        {},
        {}
    );
    ASSERT_EQ(blanket.blanket_size(), 5u);
    return true;
}

TEST(afi_blanket_validate_no_overlap) {
    auto blanket = MarkovBlanketFactory::create(
        {AtomId(1), AtomId(2)},
        {AtomId(3), AtomId(4)},
        {AtomId(5)},
        {AtomId(6)}
    );
    ASSERT(MarkovBlanketFactory::validate(blanket));
    return true;
}

TEST(afi_blanket_merge_combines) {
    auto a = MarkovBlanketFactory::create(
        {AtomId(1)}, {AtomId(2)}, {AtomId(10)}, {AtomId(20)}
    );
    auto b = MarkovBlanketFactory::create(
        {AtomId(3)}, {AtomId(4)}, {AtomId(11)}, {AtomId(21)}
    );
    auto merged = MarkovBlanketFactory::merge(a, b);

    // Merged should have elements from both
    ASSERT(merged.internal_states.size() >= 2u);  // At least both internals
    ASSERT(merged.sensory_states.size() >= 1u);   // At least some sensory
    return true;
}

// ============================================================================
// FreeEnergy Tests
// ============================================================================

TEST(afi_free_energy_default_total_zero) {
    FreeEnergy fe;
    ASSERT_NEAR(fe.total(), 0.0f, 0.001f);
    return true;
}

TEST(afi_free_energy_accuracy_plus_complexity) {
    FreeEnergy fe{0.3f, 0.2f};
    ASSERT_NEAR(fe.total(), 0.5f, 0.001f);
    return true;
}

TEST(afi_free_energy_evidence_is_neg_total) {
    FreeEnergy fe{0.3f, 0.2f};
    ASSERT_NEAR(fe.evidence(), -0.5f, 0.001f);
    return true;
}

TEST(afi_free_energy_surprise_is_total) {
    FreeEnergy fe{0.1f, 0.4f};
    ASSERT_NEAR(fe.surprise(), 0.5f, 0.001f);
    return true;
}

TEST(afi_minimizer_compute_zero_error) {
    GenerativeModelState model;
    model.predictions = {0.5f, 0.5f, 0.5f};
    model.observations = {0.5f, 0.5f, 0.5f};

    auto fe = FreeEnergyMinimizer::compute(model);
    ASSERT_NEAR(fe.accuracy_error, 0.0f, 0.001f);
    return true;
}

TEST(afi_minimizer_compute_positive_error) {
    GenerativeModelState model;
    model.predictions = {0.2f, 0.8f};
    model.observations = {0.8f, 0.2f};

    auto fe = FreeEnergyMinimizer::compute(model);
    ASSERT(fe.accuracy_error > 0.0f);
    return true;
}

TEST(afi_minimizer_complexity_is_kl_approx) {
    GenerativeModelState model;
    // Predictions far from 0.5 should have high complexity
    model.predictions = {0.0f, 1.0f};
    model.observations = {0.0f, 1.0f};

    auto fe = FreeEnergyMinimizer::compute(model);
    // complexity_cost = mean((pred-0.5)^2): ((0-0.5)^2 + (1-0.5)^2)/2 = 0.25
    ASSERT_NEAR(fe.complexity_cost, 0.25f, 0.001f);
    return true;
}

TEST(afi_city_free_energy_sums_districts) {
    std::vector<FreeEnergy> energies = {
        {0.1f, 0.2f},  // total = 0.3
        {0.3f, 0.1f},  // total = 0.4
        {0.0f, 0.1f},  // total = 0.1
    };

    float city_fe = FreeEnergyMinimizer::city_free_energy(energies);
    ASSERT_NEAR(city_fe, 0.8f, 0.001f);
    return true;
}

TEST(afi_update_predictions_moves_toward_obs) {
    GenerativeModelState model;
    model.predictions = {0.0f, 0.0f};
    model.observations = {1.0f, 1.0f};

    FreeEnergyMinimizer::update_predictions(model, 0.1f);

    // Should have moved 10% toward observations
    ASSERT_NEAR(model.predictions[0], 0.1f, 0.001f);
    ASSERT_NEAR(model.predictions[1], 0.1f, 0.001f);
    return true;
}

TEST(afi_update_predictions_converges) {
    GenerativeModelState model;
    model.predictions = {0.0f, 0.0f, 0.0f};
    model.observations = {0.8f, 0.5f, 0.3f};

    // Run many updates
    for (int i = 0; i < 1000; ++i) {
        FreeEnergyMinimizer::update_predictions(model, 0.1f);
    }

    // After many iterations, predictions should approximate observations
    ASSERT_NEAR(model.predictions[0], 0.8f, 0.01f);
    ASSERT_NEAR(model.predictions[1], 0.5f, 0.01f);
    ASSERT_NEAR(model.predictions[2], 0.3f, 0.01f);
    return true;
}

TEST(afi_expected_free_energy_total) {
    GenerativeModelState model;
    model.predictions = {0.5f, 0.5f};
    model.observations = {0.5f, 0.5f};

    // Predicted future observations differ from both predictions and observations
    std::vector<float> future = {0.8f, 0.2f};

    auto efe = FreeEnergyMinimizer::expected(model, future);
    ASSERT(efe.total() > 0.0f);
    ASSERT_NEAR(efe.total(), efe.ambiguity + efe.risk, 0.001f);
    return true;
}

// ============================================================================
// PrecisionWeight Tests
// ============================================================================

TEST(afi_precision_default_value) {
    PrecisionWeight pw;
    ASSERT_NEAR(pw.value, 1.0f, 0.001f);
    return true;
}

TEST(afi_precision_compute_returns_weights) {
    GenerativeModelState model;
    model.predictions = {0.5f, 0.5f, 0.5f};
    model.observations = {0.5f, 0.3f, 0.9f};

    auto weights = PrecisionWeighting::compute(model);
    ASSERT_EQ(weights.size(), 3u);
    return true;
}

TEST(afi_precision_high_error_low_precision) {
    GenerativeModelState model;
    model.predictions = {0.5f};
    model.observations = {0.5f};
    auto low_error = PrecisionWeighting::compute(model);

    model.predictions = {0.0f};
    model.observations = {1.0f};
    auto high_error = PrecisionWeighting::compute(model);

    // High prediction error -> low precision
    ASSERT(high_error[0].value < low_error[0].value);
    return true;
}

TEST(afi_precision_low_error_high_precision) {
    GenerativeModelState model;
    model.predictions = {0.5f};
    model.observations = {0.5f};

    auto weights = PrecisionWeighting::compute(model);
    // Zero error -> precision = 1 / (0 + epsilon) = 100 (clamped to PRECISION_MAX)
    ASSERT_NEAR(weights[0].value, PRECISION_MAX, 0.01f);
    return true;
}

TEST(afi_precision_to_sti_zero) {
    // precision=0 -> sti=-1
    float sti = PrecisionWeighting::precision_to_sti(0.0f);
    ASSERT_NEAR(sti, -1.0f, 0.001f);
    return true;
}

TEST(afi_precision_to_sti_one) {
    // precision=1 -> sti=0
    float sti = PrecisionWeighting::precision_to_sti(1.0f);
    ASSERT_NEAR(sti, 0.0f, 0.001f);
    return true;
}

TEST(afi_precision_to_sti_high) {
    // High precision -> sti near 1
    float sti = PrecisionWeighting::precision_to_sti(100.0f);
    ASSERT(sti > 0.9f);
    ASSERT(sti < 1.0f);
    return true;
}

TEST(afi_sti_to_precision_zero_sti) {
    // sti=0 -> precision=1
    float p = PrecisionWeighting::sti_to_precision(0.0f);
    ASSERT_NEAR(p, 1.0f, 0.05f);
    return true;
}

TEST(afi_sti_precision_roundtrip) {
    // precision_to_sti(sti_to_precision(x)) ~ x for moderate values
    float original_sti = 0.5f;
    float precision = PrecisionWeighting::sti_to_precision(original_sti);
    float recovered_sti = PrecisionWeighting::precision_to_sti(precision);
    ASSERT_NEAR(recovered_sti, original_sti, 0.1f);
    return true;
}

TEST(afi_attention_allocation_normalizes) {
    std::vector<PrecisionWeight> weights = {
        PrecisionWeight(AtomId(1), 2.0f),
        PrecisionWeight(AtomId(2), 3.0f),
        PrecisionWeight(AtomId(3), 5.0f),
    };

    auto alloc = PrecisionWeighting::attention_allocation(weights, 1.0f);
    ASSERT_EQ(alloc.size(), 3u);

    // Sum should equal total_budget (1.0)
    float sum = alloc[0] + alloc[1] + alloc[2];
    ASSERT_NEAR(sum, 1.0f, 0.001f);
    return true;
}

TEST(afi_attention_allocation_uniform_equal_weights) {
    std::vector<PrecisionWeight> weights = {
        PrecisionWeight(AtomId(1), 1.0f),
        PrecisionWeight(AtomId(2), 1.0f),
        PrecisionWeight(AtomId(3), 1.0f),
    };

    auto alloc = PrecisionWeighting::attention_allocation(weights, 3.0f);
    // All equal -> uniform distribution
    ASSERT_NEAR(alloc[0], 1.0f, 0.001f);
    ASSERT_NEAR(alloc[1], 1.0f, 0.001f);
    ASSERT_NEAR(alloc[2], 1.0f, 0.001f);
    return true;
}

TEST(afi_precision_clamped_to_range) {
    GenerativeModelState model;
    // Error = 0 -> raw precision would be 1/epsilon = very large
    model.predictions = {0.5f};
    model.observations = {0.5f};
    auto weights = PrecisionWeighting::compute(model);
    ASSERT(weights[0].value <= PRECISION_MAX);
    ASSERT(weights[0].value >= PRECISION_MIN);
    return true;
}

// ============================================================================
// GenerativeModelState Tests
// ============================================================================

TEST(afi_model_prediction_error_matching) {
    GenerativeModelState model;
    model.predictions = {0.5f, 0.3f, 0.8f};
    model.observations = {0.5f, 0.3f, 0.8f};
    ASSERT_NEAR(model.prediction_error(), 0.0f, 0.001f);
    return true;
}

TEST(afi_model_prediction_error_mismatched) {
    GenerativeModelState model;
    model.predictions = {0.0f, 0.0f};
    model.observations = {1.0f, 1.0f};
    // MSE = (1^2 + 1^2) / 2 = 1.0
    ASSERT_NEAR(model.prediction_error(), 1.0f, 0.001f);
    return true;
}

TEST(afi_model_weighted_prediction_error) {
    GenerativeModelState model;
    model.predictions = {0.0f, 0.0f};
    model.observations = {1.0f, 1.0f};
    model.weights = {
        PrecisionWeight(AtomId(1), 2.0f),  // High weight on first signal
        PrecisionWeight(AtomId(2), 0.5f),  // Low weight on second signal
    };

    float wpe = model.weighted_prediction_error();
    // Weighted: (2*1 + 0.5*1) / (2 + 0.5) = 2.5 / 2.5 = 1.0
    ASSERT(wpe > 0.0f);
    return true;
}
