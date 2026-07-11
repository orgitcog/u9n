/**
 * @file test_pln.cpp
 * @brief Tests for PLN formulas and inference
 */

#include <opencog/pln/formulas.hpp>
#include <opencog/pln/inference.hpp>
#include <cmath>

namespace test {
extern bool register_test(const std::string& name, std::function<bool()> func);
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_GT(a, b) if (!((a) > (b))) { return false; }
#define ASSERT_LT(a, b) if (!((a) < (b))) { return false; }
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }

using namespace opencog;
using namespace opencog::pln;

// ============================================================================
// Formula Tests
// ============================================================================

TEST(PLN_deduction_basic) {
    // A->B (0.9, 0.8), B->C (0.8, 0.9)
    TruthValue ab{0.9f, 0.8f};
    TruthValue bc{0.8f, 0.9f};

    TruthValue ac = deduction(ab, bc, 0.5f, 0.5f);

    // Result should be valid
    ASSERT_GT(ac.strength, 0.0f);
    ASSERT_LT(ac.strength, 1.0f);
    ASSERT_GT(ac.confidence, 0.0f);
    return true;
}

TEST(PLN_deduction_high_confidence) {
    TruthValue ab{1.0f, 1.0f};
    TruthValue bc{1.0f, 1.0f};

    TruthValue ac = deduction(ab, bc, 1.0f, 1.0f);

    ASSERT_NEAR(ac.strength, 1.0f, 0.01f);
    return true;
}

TEST(PLN_inversion) {
    TruthValue ab{0.8f, 0.9f};

    TruthValue ba = inversion(ab, 0.3f, 0.5f);

    ASSERT_GT(ba.strength, 0.0f);
    ASSERT_LT(ba.strength, 1.0f);
    ASSERT_GT(ba.confidence, 0.0f);
    return true;
}

TEST(PLN_revision_basic) {
    TruthValue tv1{0.6f, 0.5f};
    TruthValue tv2{0.8f, 0.6f};

    TruthValue merged = revision(tv1, tv2);

    // Merged should have higher confidence than either
    ASSERT_GT(merged.confidence, tv1.confidence);
    ASSERT_GT(merged.confidence, tv2.confidence);

    // Strength should be between the two
    ASSERT_GT(merged.strength, std::min(tv1.strength, tv2.strength));
    ASSERT_LT(merged.strength, std::max(tv1.strength, tv2.strength));
    return true;
}

TEST(PLN_revision_same_strength) {
    TruthValue tv1{0.7f, 0.5f};
    TruthValue tv2{0.7f, 0.5f};

    TruthValue merged = revision(tv1, tv2);

    ASSERT_NEAR(merged.strength, 0.7f, 0.01f);
    ASSERT_GT(merged.confidence, tv1.confidence);
    return true;
}

TEST(PLN_abduction) {
    TruthValue ab{0.8f, 0.9f};
    TruthValue cb{0.7f, 0.8f};

    TruthValue ac = abduction(ab, cb, 0.4f, 0.5f);

    ASSERT_GT(ac.strength, 0.0f);
    ASSERT_LT(ac.strength, 1.0f);
    // Abduction has lower confidence
    ASSERT_LT(ac.confidence, ab.confidence);
    return true;
}

TEST(PLN_modus_ponens) {
    TruthValue a{0.9f, 0.8f};
    TruthValue ab{0.8f, 0.9f};

    TruthValue b = modus_ponens(a, ab);

    ASSERT_GT(b.strength, 0.0f);
    ASSERT_LT(b.strength, 1.0f);
    return true;
}

TEST(PLN_fuzzy_and) {
    TruthValue a{0.8f, 0.9f};
    TruthValue b{0.7f, 0.8f};

    TruthValue result = fuzzy_and(a, b);

    ASSERT_NEAR(result.strength, 0.8f * 0.7f, 0.01f);
    ASSERT_EQ(result.confidence, std::min(a.confidence, b.confidence));
    return true;
}

TEST(PLN_fuzzy_or) {
    TruthValue a{0.8f, 0.9f};
    TruthValue b{0.7f, 0.8f};

    TruthValue result = fuzzy_or(a, b);

    // P(A or B) = P(A) + P(B) - P(A)*P(B)
    float expected = 0.8f + 0.7f - 0.8f * 0.7f;
    ASSERT_NEAR(result.strength, expected, 0.01f);
    return true;
}

TEST(PLN_fuzzy_not) {
    TruthValue a{0.8f, 0.9f};

    TruthValue result = fuzzy_not(a);

    ASSERT_NEAR(result.strength, 0.2f, 0.01f);
    ASSERT_EQ(result.confidence, a.confidence);
    return true;
}

TEST(PLN_fuzzy_and_n) {
    std::vector<TruthValue> tvs = {
        {0.8f, 0.9f},
        {0.7f, 0.8f},
        {0.9f, 0.85f}
    };

    TruthValue result = fuzzy_and_n(tvs);

    ASSERT_NEAR(result.strength, 0.8f * 0.7f * 0.9f, 0.01f);
    ASSERT_EQ(result.confidence, 0.8f);  // Minimum
    return true;
}

TEST(PLN_fuzzy_or_n) {
    std::vector<TruthValue> tvs = {
        {0.3f, 0.9f},
        {0.4f, 0.8f}
    };

    TruthValue result = fuzzy_or_n(tvs);

    // Should be higher than individual strengths
    ASSERT_GT(result.strength, 0.4f);
    return true;
}

// ============================================================================
// Batch Operation Tests
// ============================================================================

TEST(PLN_batch_revision) {
    std::vector<TruthValue> tv1 = {
        {0.6f, 0.5f},
        {0.7f, 0.6f},
        {0.8f, 0.4f}
    };
    std::vector<TruthValue> tv2 = {
        {0.7f, 0.5f},
        {0.6f, 0.7f},
        {0.9f, 0.5f}
    };
    std::vector<TruthValue> out(3);

    batch_revision(tv1, tv2, out);

    for (size_t i = 0; i < 3; ++i) {
        // Each output should have higher confidence than inputs
        ASSERT_GT(out[i].confidence, std::min(tv1[i].confidence, tv2[i].confidence));
    }
    return true;
}

// ============================================================================
// Inference Engine Tests
// ============================================================================

TEST(PLNEngine_creation) {
    AtomSpace space;
    PLNEngine engine(space);

    ASSERT_EQ(engine.rules().size(), 0u);
    return true;
}

TEST(PLNEngine_add_rules) {
    AtomSpace space;
    PLNEngine engine(space);

    engine.add_rules(rules::get_standard_rules());

    ASSERT_GT(engine.rules().size(), 0u);
    return true;
}

TEST(PLNEngine_standard_rules) {
    auto standard = rules::get_standard_rules();

    ASSERT_GT(standard.size(), 0u);

    // Check some expected rules exist
    bool has_deduction = false;
    bool has_modus_ponens = false;

    for (const auto& rule : standard) {
        if (rule.name == "deduction") has_deduction = true;
        if (rule.name == "modus-ponens") has_modus_ponens = true;
    }

    ASSERT(has_deduction);
    ASSERT(has_modus_ponens);
    return true;
}

TEST(PLNEngine_config) {
    AtomSpace space;
    InferenceConfig config;
    config.max_iterations = 500;
    config.max_results = 50;

    PLNEngine engine(space, config);

    ASSERT_EQ(engine.config().max_iterations, 500u);
    ASSERT_EQ(engine.config().max_results, 50u);
    return true;
}

TEST(PLNEngine_forward_chain_empty) {
    AtomSpace space;
    PLNEngine engine(space);

    Handle invalid;
    auto results = engine.forward_chain(invalid);

    ASSERT(results.empty());
    return true;
}

TEST(PLNEngine_backward_chain_grounded) {
    AtomSpace space;

    // Create a grounded fact
    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat", TruthValue{0.9f, 0.9f});

    PLNEngine engine(space);

    // Backward chaining should find it immediately
    auto result = engine.backward_chain(cat);

    ASSERT(result.has_value());
    ASSERT_EQ(result->conclusion.id(), cat.id());
    return true;
}

TEST(PLNEngine_reset_stats) {
    AtomSpace space;
    PLNEngine engine(space);

    engine.reset_stats();

    ASSERT_EQ(engine.total_inferences(), 0u);
    ASSERT_EQ(engine.cache_hits(), 0u);
    return true;
}

// ============================================================================
// Incremental Inference Tests
// ============================================================================

TEST(IncrementalInference_creation) {
    AtomSpace space;
    PLNEngine engine(space);
    IncrementalInference incr(engine);

    ASSERT(!incr.has_pending());
    return true;
}

TEST(IncrementalInference_add_stimulus) {
    AtomSpace space;
    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");

    PLNEngine engine(space);
    IncrementalInference incr(engine);

    incr.add_stimulus(cat);
    ASSERT(incr.has_pending());
    return true;
}

TEST(IncrementalInference_step) {
    AtomSpace space;
    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");

    PLNEngine engine(space);
    IncrementalInference incr(engine);

    incr.add_stimulus(cat);
    auto results = incr.step();

    // After processing, should no longer have that stimulus
    ASSERT(!incr.has_pending());
    return true;
}

TEST(IncrementalInference_run) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");

    PLNEngine engine(space);
    IncrementalInference incr(engine);

    for (Handle h : space.get_atoms_by_type(AtomType::CONCEPT_NODE)) {
        incr.add_stimulus(h);
    }

    auto results = incr.run(10);

    // Should complete without error
    return true;
}
