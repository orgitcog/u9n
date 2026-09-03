/**
 * @file test_civic_angel.cpp
 * @brief Tests for CivicAngel and CognitiveDistrict
 *
 * Covers: district construction, blanket management, free energy computation,
 * metric updates, CivicAngel config, self-throttling, sub-system accessors,
 * integrated state computation, and multi-observation behavior.
 */

#include <opencog/entelechy/civic_angel.hpp>
#include <opencog/entelechy/district.hpp>

#include <cmath>
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

using namespace opencog;
using namespace opencog::entelechy;

// ============================================================================
// CognitiveDistrict Tests
// ============================================================================

TEST(ca_district_construction_with_id) {
    CognitiveDistrict d(DistrictId::ACADEMY);
    ASSERT_EQ(d.id(), DistrictId::ACADEMY);
    return true;
}

TEST(ca_district_name_correct) {
    CognitiveDistrict d(DistrictId::MARKETPLACE);
    ASSERT_EQ(d.name(), std::string_view("Marketplace"));

    CognitiveDistrict d2(DistrictId::CLOCKTOWER);
    ASSERT_EQ(d2.name(), std::string_view("Clocktower"));
    return true;
}

TEST(ca_district_starts_active) {
    CognitiveDistrict d(DistrictId::HPA_QUARTER);
    ASSERT(d.is_active());
    return true;
}

TEST(ca_district_blanket_initially_empty) {
    CognitiveDistrict d(DistrictId::HARBOR);
    ASSERT(d.blanket().empty());
    ASSERT_EQ(d.blanket().blanket_size(), 0u);
    return true;
}

TEST(ca_district_compute_free_energy_empty_model) {
    CognitiveDistrict d(DistrictId::TEMPLE);
    auto fe = d.compute_free_energy();
    ASSERT_NEAR(fe.total(), 0.0f, 0.001f);
    return true;
}

TEST(ca_district_update_observations) {
    CognitiveDistrict d(DistrictId::ARCHIVES);
    std::vector<float> obs = {0.3f, 0.7f, 0.5f};
    d.update_observations(obs);

    // After update, model should have 3 predictions (initialized to 0.5)
    const auto& model = d.model();
    ASSERT_EQ(model.observations.size(), 3u);
    ASSERT_EQ(model.predictions.size(), 3u);
    ASSERT_NEAR(model.predictions[0], 0.5f, 0.001f);
    return true;
}

TEST(ca_district_update_metrics_fills) {
    CognitiveDistrict d(DistrictId::OBSERVATORY);
    // Give it some observations that differ from the default 0.5 predictions
    d.update_observations({0.0f, 1.0f, 0.0f, 1.0f});
    d.update_metrics();

    const auto& m = d.metrics();
    // Free energy should be positive (prediction errors exist)
    ASSERT(m.free_energy > 0.0f);
    // Surprise == free_energy
    ASSERT_NEAR(m.surprise, m.free_energy, 0.001f);
    return true;
}

// ============================================================================
// CivicAngel Config Tests
// ============================================================================

TEST(ca_default_config_observation_interval) {
    CivicAngel::Config cfg;
    ASSERT_EQ(cfg.observation_interval, 10u);
    ASSERT_EQ(cfg.narrative_interval, 100u);
    ASSERT_NEAR(cfg.coherence_threshold, 0.6f, 0.001f);
    ASSERT_NEAR(cfg.free_energy_alarm, 5.0f, 0.001f);
    return true;
}

TEST(ca_tick_returns_false_before_interval) {
    CivicAngel angel;
    // Tick 0 should observe (first time)
    bool observed = angel.tick(0);
    ASSERT(observed);
    // Ticks 1-9 should NOT observe (interval = 10)
    for (uint64_t t = 1; t < 10; ++t) {
        ASSERT(!angel.tick(t));
    }
    return true;
}

TEST(ca_tick_returns_true_at_interval) {
    CivicAngel angel;
    angel.tick(0);  // Initial observation
    // Tick 10 should trigger observation
    bool observed = angel.tick(10);
    ASSERT(observed);
    return true;
}

TEST(ca_no_districts_zero_free_energy) {
    CivicAngel angel;
    angel.tick(0);  // Force observation
    ASSERT_NEAR(angel.total_free_energy(), 0.0f, 0.001f);
    return true;
}

TEST(ca_register_district_increases_count) {
    CivicAngel angel;
    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);

    ASSERT_EQ(angel.district_count(), 0u);
    angel.register_district(&d1);
    ASSERT_EQ(angel.district_count(), 1u);
    angel.register_district(&d2);
    ASSERT_EQ(angel.district_count(), 2u);
    return true;
}

TEST(ca_after_observation_state_updated) {
    CivicAngel angel;
    CognitiveDistrict d(DistrictId::ACADEMY);
    d.update_observations({0.1f, 0.9f, 0.1f, 0.9f});
    d.update_metrics();
    angel.register_district(&d);

    angel.tick(0);

    // After observation, total_free_energy should reflect the district
    ASSERT(angel.total_free_energy() > 0.0f);
    return true;
}

TEST(ca_self_coherence_bounded) {
    CivicAngel angel;
    angel.tick(0);
    float sc = angel.self_coherence();
    ASSERT(sc >= 0.0f);
    ASSERT(sc <= 1.0f);
    return true;
}

TEST(ca_self_complexity_grows_with_districts) {
    CivicAngel angel;
    angel.tick(0);
    float initial_complexity = angel.state().self_complexity;

    // Add active districts and re-observe
    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);
    CognitiveDistrict d3(DistrictId::OBSERVATORY);
    angel.register_district(&d1);
    angel.register_district(&d2);
    angel.register_district(&d3);

    angel.tick(10);
    ASSERT(angel.state().self_complexity > initial_complexity);
    return true;
}

TEST(ca_entelechy_progress_starts_at_zero) {
    CivicAngel angel;
    // Before any observation
    ASSERT_NEAR(angel.state().entelechy_progress, 0.0f, 0.001f);
    return true;
}

TEST(ca_developmental_stage_starts_nascent) {
    CivicAngel angel;
    angel.tick(0);
    ASSERT_EQ(angel.developmental_stage(), DevelopmentalStage::NASCENT);
    return true;
}

TEST(ca_narrative_starts_with_growth) {
    CivicAngel angel;
    angel.tick(0);
    ASSERT_EQ(angel.state().dominant_life_theme, NarrativeTheme::GROWTH);
    return true;
}

TEST(ca_social_starts_with_secure_attachment) {
    CivicAngel angel;
    ASSERT_EQ(angel.social().attachment_style(), AttachmentStyle::SECURE);
    return true;
}

TEST(ca_cloninger_accessor_valid) {
    CivicAngel angel;
    // Should be able to read temperament without crashing
    const auto& temp = angel.cloninger().temperament();
    ASSERT_NEAR(temp.novelty_seeking, 0.5f, 0.001f);
    return true;
}

TEST(ca_interoceptive_accessor_valid) {
    CivicAngel angel;
    // Default interoceptive state should have reasonable values
    const auto& is = angel.interoceptive().state();
    ASSERT(is.vagal_tone >= 0.0f);
    ASSERT(is.vagal_tone <= 1.0f);
    return true;
}

TEST(ca_self_throttle_observation) {
    CivicAngel::Config cfg;
    cfg.observation_interval = 5;
    CivicAngel angel(cfg);

    int observations = 0;
    for (uint64_t t = 0; t < 50; ++t) {
        if (angel.tick(t)) observations++;
    }

    // With interval=5 over 50 ticks, expect ~10 observations
    ASSERT(observations >= 9);
    ASSERT(observations <= 11);
    return true;
}

TEST(ca_multiple_observations_advance_state) {
    CivicAngel angel;
    CognitiveDistrict d(DistrictId::ACADEMY);
    d.update_observations({0.3f, 0.7f});
    d.update_metrics();
    angel.register_district(&d);

    // Run multiple observations
    for (uint64_t t = 0; t <= 100; t += 10) {
        angel.tick(t);
    }

    // State should have been updated
    const auto& s = angel.state();
    // total_free_energy should be non-zero since district has prediction error
    ASSERT(s.total_free_energy > 0.0f);
    return true;
}

TEST(ca_resource_fairness_equal_districts) {
    CivicAngel angel;
    // Create districts with identical observations -> identical accuracy
    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);
    d1.update_observations({0.5f, 0.5f});
    d2.update_observations({0.5f, 0.5f});
    d1.update_metrics();
    d2.update_metrics();

    angel.register_district(&d1);
    angel.register_district(&d2);
    angel.tick(0);

    // When all districts are equal, fairness should be 1.0
    ASSERT_NEAR(angel.state().resource_fairness, 1.0f, 0.001f);
    return true;
}

TEST(ca_adaptive_capacity_bounded) {
    CivicAngel angel;
    angel.tick(0);
    float ac = angel.state().adaptive_capacity;
    ASSERT(ac >= 0.0f);
    ASSERT(ac <= 1.0f);
    return true;
}
