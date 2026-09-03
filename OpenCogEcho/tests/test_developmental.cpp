/**
 * @file test_developmental.cpp
 * @brief Tests for the DevelopmentalTrajectory system
 *
 * Covers: stage transitions, experience tracking, plasticity, trauma encoding,
 * trauma healing, critical periods, maturation, and prerequisite gating.
 */

#include <opencog/entelechy/developmental.hpp>

#include <cmath>
#include <functional>
#include <string>

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
#define ASSERT_LT(a, b) if (!((a) < (b))) { return false; }
#define ASSERT_GE(a, b) if (!((a) >= (b))) { return false; }
#define ASSERT_LE(a, b) if (!((a) <= (b))) { return false; }

using namespace opencog;
using namespace opencog::entelechy;
using namespace opencog::endo;

// ============================================================================
// Helper: make a default temperament profile for transitions
// ============================================================================

static TemperamentProfile default_temperament() {
    TemperamentProfile t{};
    return t;
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(Developmental_starts_at_NASCENT) {
    DevelopmentalTrajectory traj;
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::NASCENT));
    return true;
}

TEST(Developmental_initial_experience_is_zero) {
    DevelopmentalTrajectory traj;
    ASSERT_NEAR(traj.total_experience(), 0.0f, 0.001f);
    return true;
}

TEST(Developmental_initial_maturation_is_zero) {
    DevelopmentalTrajectory traj;
    ASSERT_NEAR(traj.maturation_level(), 0.0f, 0.001f);
    return true;
}

TEST(Developmental_plasticity_at_NASCENT_is_one) {
    DevelopmentalTrajectory traj;
    ASSERT_NEAR(traj.current_plasticity(), 1.0f, 0.01f);
    return true;
}

// ============================================================================
// Experience Tracking Tests
// ============================================================================

TEST(Developmental_adding_experience_accumulates) {
    DevelopmentalTrajectory traj;
    traj.add_experience(10.0f);
    ASSERT_NEAR(traj.total_experience(), 10.0f, 0.001f);

    traj.add_experience(25.0f);
    ASSERT_NEAR(traj.total_experience(), 35.0f, 0.001f);
    return true;
}

TEST(Developmental_stage_progress_is_experience_over_threshold) {
    DevelopmentalTrajectory traj;
    // NASCENT threshold is 100
    traj.add_experience(50.0f);
    ASSERT_NEAR(traj.stage_progress(), 0.5f, 0.01f);

    traj.add_experience(50.0f);
    ASSERT_NEAR(traj.stage_progress(), 1.0f, 0.01f);
    return true;
}

// ============================================================================
// Stage Transition Tests
// ============================================================================

TEST(Developmental_NASCENT_to_IMPRINTING_requires_100_experience) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // Not enough experience -- should not transition
    traj.add_experience(50.0f);
    bool transitioned = traj.update(100, t, 0.5f, 0.5f, 0.5f);
    ASSERT(!transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::NASCENT));

    // Enough experience -- should transition
    traj.add_experience(60.0f);
    transitioned = traj.update(200, t, 0.5f, 0.5f, 0.5f);
    ASSERT(transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::IMPRINTING));
    return true;
}

TEST(Developmental_IMPRINTING_to_SOCIALIZATION_requires_attachment_security) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // Move to IMPRINTING first
    traj.add_experience(110.0f);
    traj.update(100, t, 0.5f, 0.5f, 0.5f);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::IMPRINTING));

    // Add enough experience for SOCIALIZATION (500 needed from IMPRINTING)
    traj.add_experience(500.0f);

    // Low attachment security -- should NOT transition
    bool transitioned = traj.update(200, t, 0.5f, 0.2f, 0.5f);
    ASSERT(!transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::IMPRINTING));

    // High attachment security -- should transition
    transitioned = traj.update(300, t, 0.5f, 0.5f, 0.5f);
    ASSERT(transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::SOCIALIZATION));
    return true;
}

TEST(Developmental_SOCIALIZATION_to_INDIVIDUATION_requires_maturation) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // Move to SOCIALIZATION
    traj.add_experience(110.0f);
    traj.update(100, t, 0.5f, 0.5f, 0.5f);  // -> IMPRINTING
    traj.add_experience(500.0f);
    traj.update(200, t, 0.5f, 0.5f, 0.5f);  // -> SOCIALIZATION
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::SOCIALIZATION));

    // Add enough experience for next stage (2000)
    traj.add_experience(2000.0f);

    // Low maturation -- should NOT transition
    TemperamentProfile low_mat = default_temperament();
    low_mat.maturation = 0.1f;
    bool transitioned = traj.update(300, low_mat, 0.5f, 0.5f, 0.5f);
    ASSERT(!transitioned);

    // High maturation -- should transition
    TemperamentProfile high_mat = default_temperament();
    high_mat.maturation = 0.3f;
    transitioned = traj.update(400, high_mat, 0.5f, 0.5f, 0.5f);
    ASSERT(transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::INDIVIDUATION));
    return true;
}

TEST(Developmental_no_transition_without_sufficient_experience) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // No experience added -- update should not transition
    bool transitioned = traj.update(100, t, 1.0f, 1.0f, 1.0f);
    ASSERT(!transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::NASCENT));
    return true;
}

TEST(Developmental_no_transition_without_prerequisites) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // Move to IMPRINTING
    traj.add_experience(110.0f);
    traj.update(100, t, 0.5f, 0.5f, 0.5f);

    // Add plenty of experience but low attachment security
    traj.add_experience(10000.0f);
    bool transitioned = traj.update(200, t, 0.5f, 0.1f, 0.5f);
    ASSERT(!transitioned);
    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::IMPRINTING));
    return true;
}

TEST(Developmental_stage_transition_changes_current_stage) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::NASCENT));

    traj.add_experience(200.0f);
    traj.update(100, t, 0.5f, 0.5f, 0.5f);

    ASSERT_EQ(static_cast<uint8_t>(traj.current_stage()),
              static_cast<uint8_t>(DevelopmentalStage::IMPRINTING));
    return true;
}

// ============================================================================
// Plasticity Tests
// ============================================================================

TEST(Developmental_plasticity_decreases_with_later_stages) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    float nascent_plasticity = traj.current_plasticity();

    // Move to IMPRINTING
    traj.add_experience(200.0f);
    traj.update(100, t, 0.5f, 0.5f, 0.5f);
    float imprinting_plasticity = traj.current_plasticity();

    ASSERT_LT(imprinting_plasticity, nascent_plasticity);
    return true;
}

TEST(Developmental_WISDOM_has_lowest_plasticity) {
    // stage_plasticity(WISDOM) = 0.1
    float wisdom_base = stage_plasticity(DevelopmentalStage::WISDOM);
    ASSERT_NEAR(wisdom_base, 0.1f, 0.01f);

    // Verify all other stages have higher plasticity
    ASSERT_GT(stage_plasticity(DevelopmentalStage::NASCENT), wisdom_base);
    ASSERT_GT(stage_plasticity(DevelopmentalStage::IMPRINTING), wisdom_base);
    ASSERT_GT(stage_plasticity(DevelopmentalStage::SOCIALIZATION), wisdom_base);
    ASSERT_GT(stage_plasticity(DevelopmentalStage::INDIVIDUATION), wisdom_base);
    ASSERT_GT(stage_plasticity(DevelopmentalStage::INTEGRATION), wisdom_base);
    ASSERT_GT(stage_plasticity(DevelopmentalStage::GENERATIVITY), wisdom_base);
    return true;
}

// ============================================================================
// Trauma Tests
// ============================================================================

TEST(Developmental_trauma_encoding_stores_records) {
    DevelopmentalTrajectory traj;

    TraumaRecord trauma;
    trauma.tick = 50;
    trauma.intensity = 0.7f;
    trauma.valence = ValenceSignature(-0.8f, 0.9f);

    traj.encode_trauma(trauma);

    ASSERT_EQ(traj.trauma_history().size(), 1u);
    return true;
}

TEST(Developmental_trauma_intensity_scaled_by_plasticity) {
    DevelopmentalTrajectory traj;
    // At NASCENT, plasticity = 1.0
    float plasticity = traj.current_plasticity();
    ASSERT_NEAR(plasticity, 1.0f, 0.01f);

    TraumaRecord trauma;
    trauma.tick = 50;
    trauma.intensity = 0.5f;

    traj.encode_trauma(trauma);

    // Encoded intensity = 0.5 * 1.0 = 0.5
    ASSERT_NEAR(traj.trauma_history().back().intensity, 0.5f, 0.01f);
    return true;
}

TEST(Developmental_trauma_burden_sums_unhealed) {
    DevelopmentalTrajectory traj;

    TraumaRecord t1;
    t1.tick = 10;
    t1.intensity = 0.3f;
    traj.encode_trauma(t1);

    TraumaRecord t2;
    t2.tick = 20;
    t2.intensity = 0.5f;
    traj.encode_trauma(t2);

    // Both unhealed, but encode_trauma() scales intensity by current_plasticity()
    // (NASCENT stage plasticity 1.0, reduced by 0.2 * existing trauma_burden()
    // for "protective rigidification"): t1 encodes at 0.3*1.0=0.3 (burden 0 so
    // far); t2 then sees burden=0.3, so plasticity = 1.0*(1-0.2*0.3) = 0.94,
    // encoding at 0.5*0.94=0.47. burden = 0.3 + 0.47 = 0.77 (not the naive
    // 0.3+0.5=0.8, which ignores the documented plasticity/rigidification
    // scaling in encode_trauma()).
    ASSERT_NEAR(traj.trauma_burden(), 0.77f, 0.01f);
    return true;
}

TEST(Developmental_healing_progresses_with_safety) {
    DevelopmentalTrajectory traj;

    TraumaRecord trauma;
    trauma.tick = 10;
    trauma.intensity = 0.3f;  // Low intensity heals faster
    traj.encode_trauma(trauma);

    float burden_before = traj.trauma_burden();

    // Advance healing with high safety many times
    for (int i = 0; i < 10000; ++i) {
        traj.advance_healing(100 + i, 1.0f);
    }

    float burden_after = traj.trauma_burden();
    ASSERT_LT(burden_after, burden_before);
    return true;
}

// ============================================================================
// Critical Period Tests
// ============================================================================

TEST(Developmental_critical_periods_exist_for_each_stage_except_WISDOM) {
    // NASCENT through GENERATIVITY should have critical periods
    for (uint8_t s = 0; s < 6; ++s) {
        DevelopmentalStage stage = static_cast<DevelopmentalStage>(s);
        auto dims = detail::critical_dimensions(stage);
        ASSERT(!dims.empty());
    }
    // WISDOM should have no critical periods
    auto wisdom_dims = detail::critical_dimensions(DevelopmentalStage::WISDOM);
    ASSERT(wisdom_dims.empty());
    return true;
}

TEST(Developmental_in_critical_period_true_during_active) {
    DevelopmentalTrajectory traj;
    // At NASCENT, there should be an active critical period
    ASSERT(traj.in_critical_period());
    return true;
}

// ============================================================================
// Maturation Tests
// ============================================================================

TEST(Developmental_maturation_grows_over_time) {
    DevelopmentalTrajectory traj;
    TemperamentProfile t = default_temperament();

    // Initial maturation is 0
    ASSERT_NEAR(traj.maturation_level(), 0.0f, 0.001f);

    // Run many updates
    for (int i = 0; i < 10000; ++i) {
        traj.update(i, t, 0.5f, 0.5f, 0.5f);
    }

    ASSERT_GT(traj.maturation_level(), 0.0f);
    return true;
}
