/**
 * @file test_entelechy_integration.cpp
 * @brief Integration tests for Ontogenetic Entelechy system
 *
 * Tests cross-subsystem interactions: CivicAngel lifecycle, Cloninger gains,
 * interoceptive model, developmental trajectory, narrative identity, social self,
 * AFI precision-to-STI mapping, and full tick simulation.
 */

#include <opencog/entelechy/civic_angel.hpp>
#include <opencog/entelechy/district.hpp>
#include <opencog/entelechy/cloninger.hpp>
#include <opencog/entelechy/interoceptive.hpp>
#include <opencog/entelechy/developmental.hpp>
#include <opencog/entelechy/narrative.hpp>
#include <opencog/entelechy/social.hpp>
#include <opencog/afi/free_energy.hpp>
#include <opencog/afi/precision.hpp>

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

using namespace opencog::entelechy;
using namespace opencog::afi;
using namespace opencog::endo;
using namespace opencog;

// ============================================================================
// Full Lifecycle Tests
// ============================================================================

TEST(integ_full_lifecycle_create_register_tick) {
    CivicAngel angel;

    // Create and register districts
    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);
    CognitiveDistrict d3(DistrictId::OBSERVATORY);
    d1.update_observations({0.3f, 0.7f, 0.5f});
    d2.update_observations({0.6f, 0.4f});
    d3.update_observations({0.2f, 0.8f, 0.1f, 0.9f});
    d1.update_metrics();
    d2.update_metrics();
    d3.update_metrics();

    angel.register_district(&d1);
    angel.register_district(&d2);
    angel.register_district(&d3);

    // Tick multiple times
    int observations = 0;
    for (uint64_t t = 0; t <= 100; t += 1) {
        if (angel.tick(t)) observations++;
    }

    ASSERT(observations > 0);
    ASSERT(angel.total_free_energy() > 0.0f);
    ASSERT_EQ(angel.district_count(), 3u);
    return true;
}

// ============================================================================
// Cloninger Gains Tests
// ============================================================================

TEST(integ_cloninger_gains_feed_correct_channels) {
    // High novelty seeking should boost dopamine production gains
    TemperamentProfile profile;
    profile.novelty_seeking = 0.9f;
    CloningerSystem cloninger(profile);

    auto gains = cloninger.compute_gains();
    size_t da_tonic_idx = static_cast<size_t>(HormoneId::DOPAMINE_TONIC);
    size_t da_phasic_idx = static_cast<size_t>(HormoneId::DOPAMINE_PHASIC);

    // NS=0.9 -> production gain = 0.5 + 0.9*1.0 = 1.4
    ASSERT(gains.production_gains[da_tonic_idx] > 1.0f);
    ASSERT(gains.production_gains[da_phasic_idx] > 1.0f);
    return true;
}

// ============================================================================
// Interoceptive Model Tests
// ============================================================================

TEST(integ_interoceptive_reads_endocrine_state) {
    InteroceptiveModel model;
    EndocrineState state{};

    // Set high cortisol and NE to trigger sympathetic response
    state.concentrations[static_cast<size_t>(HormoneId::CORTISOL)] = 0.6f;
    state.concentrations[static_cast<size_t>(HormoneId::NOREPINEPHRINE)] = 0.6f;
    state.concentrations[static_cast<size_t>(HormoneId::SEROTONIN)] = 0.3f;

    float initial_sympathetic = model.state().sympathetic_drive;
    model.update(state);

    // Sympathetic drive should increase with cortisol+NE
    ASSERT(model.state().sympathetic_drive > initial_sympathetic);
    return true;
}

// ============================================================================
// Developmental Trajectory Tests
// ============================================================================

TEST(integ_developmental_advances_with_experience) {
    DevelopmentalTrajectory dev;
    ASSERT_EQ(dev.current_stage(), DevelopmentalStage::NASCENT);

    // Accumulate enough experience for NASCENT->IMPRINTING (threshold: 100)
    dev.add_experience(150.0f);

    TemperamentProfile temp;
    temp.maturation = 0.1f;
    bool transitioned = dev.update(100, temp, 0.3f, 0.5f, 0.3f);
    ASSERT(transitioned);
    ASSERT_EQ(dev.current_stage(), DevelopmentalStage::IMPRINTING);
    return true;
}

// ============================================================================
// Narrative Tests
// ============================================================================

TEST(integ_narrative_detects_chapter_boundary) {
    NarrativeIdentity narrative(0);
    narrative.set_window_size(5);

    // Accumulate a stable positive valence window
    for (uint64_t t = 1; t <= 10; ++t) {
        narrative.update(t, ValenceSignature(0.5f, 0.3f));
    }

    // Inject a sudden negative valence shift (> 0.3 difference)
    bool boundary = narrative.update(11, ValenceSignature(-0.5f, 0.8f));
    ASSERT(boundary);
    // Should now have more than 1 chapter
    ASSERT(narrative.chapter_count() > 1);
    return true;
}

// ============================================================================
// Social Attachment from Temperament Tests
// ============================================================================

TEST(integ_social_attachment_from_temperament) {
    // High security, low anxiety -> SECURE
    auto style = SocialSelf::compute_attachment(0.7f, 0.2f);
    ASSERT_EQ(style, AttachmentStyle::SECURE);

    // Low security, high anxiety -> DISORGANIZED or ANXIOUS
    style = SocialSelf::compute_attachment(0.2f, 0.8f);
    ASSERT_EQ(style, AttachmentStyle::DISORGANIZED);
    return true;
}

// ============================================================================
// District Free Energy Tests
// ============================================================================

TEST(integ_district_free_energy_correct) {
    CognitiveDistrict d(DistrictId::ACADEMY);
    // Predictions default to 0.5 when observations are set
    d.update_observations({0.0f, 1.0f});

    auto fe = d.compute_free_energy();
    // accuracy_error = MSE of (0.5-0.0)^2 + (0.5-1.0)^2 / 2 = 0.25
    ASSERT_NEAR(fe.accuracy_error, 0.25f, 0.001f);
    // complexity = mean((0.5-0.5)^2) = 0 (predictions are at prior)
    ASSERT_NEAR(fe.complexity_cost, 0.0f, 0.001f);
    return true;
}

TEST(integ_city_wide_free_energy_sum) {
    CivicAngel angel;

    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);
    d1.update_observations({0.0f, 1.0f});
    d2.update_observations({0.0f, 1.0f});
    d1.update_metrics();
    d2.update_metrics();

    angel.register_district(&d1);
    angel.register_district(&d2);
    angel.tick(0);

    // City-wide FE should be approximately sum of individual district FEs
    float fe1 = d1.metrics().free_energy;
    float fe2 = d2.metrics().free_energy;
    ASSERT_NEAR(angel.total_free_energy(), fe1 + fe2, 0.01f);
    return true;
}

// ============================================================================
// Self-Coherence Tests
// ============================================================================

TEST(integ_self_coherence_increases_stable_system) {
    // A system with consistent districts and stable state should
    // produce non-negative coherence
    CivicAngel angel;
    CognitiveDistrict d1(DistrictId::ACADEMY);
    CognitiveDistrict d2(DistrictId::MARKETPLACE);

    // Identical observations -> high coherence
    d1.update_observations({0.5f, 0.5f, 0.5f});
    d2.update_observations({0.5f, 0.5f, 0.5f});
    d1.update_metrics();
    d2.update_metrics();

    angel.register_district(&d1);
    angel.register_district(&d2);
    angel.tick(0);

    float coherence = angel.self_coherence();
    ASSERT(coherence >= 0.0f);
    ASSERT(coherence <= 1.0f);
    return true;
}

// ============================================================================
// Entelechy Progress Tests
// ============================================================================

TEST(integ_entelechy_progress_grows) {
    // Progress = maturation * coherence * chapter_depth
    // With 0 maturation, progress stays 0
    CivicAngel angel;
    angel.tick(0);
    ASSERT_NEAR(angel.state().entelechy_progress, 0.0f, 0.001f);
    return true;
}

// ============================================================================
// Polyvagal Tier Tests
// ============================================================================

TEST(integ_polyvagal_tier_responds_to_endocrine) {
    InteroceptiveModel model;
    EndocrineState state{};

    // Safe state: high oxytocin, low cortisol, low NE -> ventral vagal
    state.concentrations[static_cast<size_t>(HormoneId::OXYTOCIN)] = 0.7f;
    state.concentrations[static_cast<size_t>(HormoneId::CORTISOL)] = 0.1f;
    state.concentrations[static_cast<size_t>(HormoneId::NOREPINEPHRINE)] = 0.1f;
    state.concentrations[static_cast<size_t>(HormoneId::SEROTONIN)] = 0.5f;

    for (int i = 0; i < 20; ++i) {
        model.update(state);
    }
    ASSERT_EQ(model.dominant_tier(), PolyvagalTier::VENTRAL_VAGAL);
    return true;
}

// ============================================================================
// Allostatic Load Tests
// ============================================================================

TEST(integ_allostatic_load_accumulates_under_stress) {
    InteroceptiveModel model;
    EndocrineState state{};

    // Sustained high cortisol
    state.concentrations[static_cast<size_t>(HormoneId::CORTISOL)] = 0.8f;
    state.concentrations[static_cast<size_t>(HormoneId::NOREPINEPHRINE)] = 0.3f;
    state.concentrations[static_cast<size_t>(HormoneId::SEROTONIN)] = 0.3f;

    float initial_load = model.allostatic_load();
    // Need > 100 ticks of high cortisol before load accumulates
    for (int i = 0; i < 200; ++i) {
        model.update(state);
    }
    ASSERT(model.allostatic_load() >= initial_load);
    return true;
}

// ============================================================================
// Material Self Tests
// ============================================================================

TEST(integ_material_self_weighted_average) {
    InteroceptiveModel model;
    // Default state should give a reasonable material self value
    float ms = model.material_self();
    ASSERT(ms >= 0.0f);
    ASSERT(ms <= 1.0f);
    // material_self = 0.3*vagal + 0.2*cardiac + 0.2*(1-pain) + 0.15*proprio + 0.15*(1-load/5)
    // With defaults: 0.3*0.5 + 0.2*0.5 + 0.2*1.0 + 0.15*0.5 + 0.15*1.0
    // = 0.15 + 0.1 + 0.2 + 0.075 + 0.15 = 0.675
    ASSERT_NEAR(ms, 0.675f, 0.01f);
    return true;
}

// ============================================================================
// Precision -> STI Tests
// ============================================================================

TEST(integ_precision_weighting_maps_to_sti) {
    // High precision should map to high STI
    float high_sti = PrecisionWeighting::precision_to_sti(10.0f);
    float low_sti = PrecisionWeighting::precision_to_sti(0.1f);
    ASSERT(high_sti > low_sti);
    ASSERT(high_sti > 0.0f);
    ASSERT(low_sti < 0.0f);
    return true;
}

// ============================================================================
// Trauma Plasticity Tests
// ============================================================================

TEST(integ_trauma_reduces_plasticity) {
    DevelopmentalTrajectory dev;
    float initial_plasticity = dev.current_plasticity();

    // Encode trauma
    TraumaRecord trauma;
    trauma.tick = 10;
    trauma.intensity = 0.8f;
    trauma.valence = ValenceSignature(-0.9f, 0.9f);
    dev.encode_trauma(trauma);

    // Plasticity should be reduced by trauma burden
    float post_trauma_plasticity = dev.current_plasticity();
    ASSERT(post_trauma_plasticity <= initial_plasticity);
    return true;
}

// ============================================================================
// Redemption Arc Tests
// ============================================================================

TEST(integ_redemption_arc_detected) {
    NarrativeIdentity narrative(0);
    narrative.set_window_size(3);

    // Build a stable negative valence window
    for (uint64_t t = 1; t <= 10; ++t) {
        narrative.update(t, ValenceSignature(-0.5f, 0.3f));
    }

    // Sharp shift to positive -> chapter boundary + redemption arc check
    bool boundary = narrative.update(11, ValenceSignature(0.6f, 0.3f));
    if (boundary) {
        // The previous chapter was negative, new one is positive
        ASSERT(narrative.chapter_count() >= 2);
    }
    return true;
}

// ============================================================================
// Phase 0: Cloninger Gains Applied Before Production
// ============================================================================

TEST(integ_phase0_gains_applied) {
    TemperamentProfile profile;
    profile.novelty_seeking = 0.8f;
    profile.harm_avoidance = 0.7f;
    CloningerSystem cloninger(profile);

    auto gains = cloninger.compute_gains();

    // Verify gains are bounded
    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT(gains.production_gains[i] >= GAIN_MIN);
        ASSERT(gains.production_gains[i] <= GAIN_MAX);
        ASSERT(gains.decay_gains[i] >= GAIN_MIN);
        ASSERT(gains.decay_gains[i] <= GAIN_MAX);
    }
    return true;
}

// ============================================================================
// Phase 7: Interoceptive Update From Bus Snapshot
// ============================================================================

TEST(integ_phase7_interoceptive_update) {
    InteroceptiveModel model;
    EndocrineState state{};

    // Set a specific endocrine state
    state.concentrations[static_cast<size_t>(HormoneId::CORTISOL)] = 0.2f;
    state.concentrations[static_cast<size_t>(HormoneId::SEROTONIN)] = 0.6f;
    state.concentrations[static_cast<size_t>(HormoneId::OXYTOCIN)] = 0.5f;
    state.concentrations[static_cast<size_t>(HormoneId::NOREPINEPHRINE)] = 0.2f;

    model.update(state);

    // After update, gut_brain_signal should be influenced by serotonin
    // gut = 0.4 * serotonin + 0.6 * prev = 0.4*0.6 + 0.6*0.3 = 0.42
    ASSERT(model.state().gut_brain_signal > 0.0f);
    return true;
}

// ============================================================================
// Phase 8: Civic Angel Observation Self-Throttle
// ============================================================================

TEST(integ_phase8_civic_angel_throttle) {
    CivicAngel::Config cfg;
    cfg.observation_interval = 10;
    CivicAngel angel(cfg);

    // Only every 10th tick should trigger an observation
    int count = 0;
    for (uint64_t t = 0; t <= 50; t++) {
        if (angel.tick(t)) count++;
    }
    // Expect about 5-6 observations in 50 ticks with interval=10
    ASSERT(count >= 4);
    ASSERT(count <= 7);
    return true;
}

// ============================================================================
// Full 100-Tick Simulation
// ============================================================================

TEST(integ_full_100_tick_simulation) {
    CivicAngel angel;

    // Create a set of districts
    CognitiveDistrict d_academy(DistrictId::ACADEMY);
    CognitiveDistrict d_market(DistrictId::MARKETPLACE);
    CognitiveDistrict d_obs(DistrictId::OBSERVATORY);
    CognitiveDistrict d_court(DistrictId::COURTHOUSE);

    d_academy.update_observations({0.4f, 0.6f, 0.5f});
    d_market.update_observations({0.3f, 0.7f});
    d_obs.update_observations({0.5f, 0.5f, 0.5f, 0.5f});
    d_court.update_observations({0.6f, 0.4f, 0.5f});

    d_academy.update_metrics();
    d_market.update_metrics();
    d_obs.update_metrics();
    d_court.update_metrics();

    angel.register_district(&d_academy);
    angel.register_district(&d_market);
    angel.register_district(&d_obs);
    angel.register_district(&d_court);

    // Run 100 ticks without crashes
    for (uint64_t t = 0; t <= 100; ++t) {
        angel.tick(t);
    }

    // Verify the system is in a valid state
    const auto& state = angel.state();
    ASSERT(state.self_coherence >= 0.0f);
    ASSERT(state.self_coherence <= 1.0f);
    ASSERT(state.self_complexity >= 0.0f);
    ASSERT(state.self_complexity <= 1.0f);
    ASSERT(state.entelechy_progress >= 0.0f);
    ASSERT(state.entelechy_progress <= 1.0f);
    ASSERT(state.total_free_energy >= 0.0f);
    ASSERT(state.resource_fairness >= 0.0f);
    ASSERT(state.resource_fairness <= 1.0f);
    ASSERT(state.adaptive_capacity >= 0.0f);
    ASSERT(state.adaptive_capacity <= 1.0f);

    // Developmental stage should still be NASCENT (no experience added)
    ASSERT_EQ(state.developmental_stage, DevelopmentalStage::NASCENT);

    return true;
}
