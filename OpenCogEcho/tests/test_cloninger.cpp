/**
 * @file test_cloninger.cpp
 * @brief Tests for the CloningerSystem temperament/character gain computation
 *
 * Covers: default gains, temperament influences, character influences,
 * developmental modifiers, gain clamping, determinism, and accessor methods.
 */

#include <opencog/entelechy/cloninger.hpp>

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
// Helper: index for HormoneId
// ============================================================================

static constexpr size_t idx(HormoneId id) noexcept {
    return static_cast<size_t>(id);
}

// ============================================================================
// Default Profile Tests
// ============================================================================

TEST(Cloninger_default_production_gains_near_one) {
    TemperamentProfile profile{};  // all 0.5 defaults
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // Default profile (all 0.5) should produce gains near 1.0
    // NS=0.5 -> DA gains = 0.5 + 0.5*1.0 = 1.0
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::DOPAMINE_TONIC)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::DOPAMINE_PHASIC)], 1.0f, 0.01f);
    // HA=0.5 -> SEROTONIN = 0.5 + 0.5*1.0 = 1.0
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::SEROTONIN)], 1.0f, 0.01f);
    return true;
}

TEST(Cloninger_default_decay_gains_near_one) {
    TemperamentProfile profile{};
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // P=0.5 -> DA_TONIC decay = 0.5 + 0.5*1.0 = 1.0
    ASSERT_NEAR(gains.decay_gains[idx(HormoneId::DOPAMINE_TONIC)], 1.0f, 0.01f);
    return true;
}

TEST(Cloninger_all_production_gains_default_one) {
    TemperamentProfile profile{};
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // Channels not explicitly modified should remain 1.0 (clamped from default)
    // Check a channel that is not touched by any temperament dimension
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::CRH)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::ACTH)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::INSULIN)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::GLUCAGON)], 1.0f, 0.01f);
    return true;
}

TEST(Cloninger_all_decay_gains_default_one) {
    TemperamentProfile profile{};
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // All decay gains except DA_TONIC should remain at 1.0
    ASSERT_NEAR(gains.decay_gains[idx(HormoneId::CORTISOL)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.decay_gains[idx(HormoneId::SEROTONIN)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.decay_gains[idx(HormoneId::OXYTOCIN)], 1.0f, 0.01f);
    return true;
}

// ============================================================================
// Temperament Influence Tests
// ============================================================================

TEST(Cloninger_high_novelty_seeking_boosts_DA) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // NS=1.0 -> DA gain = 0.5 + 1.0*1.0 = 1.5
    ASSERT_GT(gains.production_gains[idx(HormoneId::DOPAMINE_TONIC)], 1.0f);
    ASSERT_GT(gains.production_gains[idx(HormoneId::DOPAMINE_PHASIC)], 1.0f);
    return true;
}

TEST(Cloninger_high_harm_avoidance_boosts_SEROTONIN_CORTISOL) {
    TemperamentProfile profile{};
    profile.harm_avoidance = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // HA=1.0 -> SEROTONIN = 0.5 + 1.0*1.0 = 1.5
    ASSERT_GT(gains.production_gains[idx(HormoneId::SEROTONIN)], 1.0f);
    // HA=1.0 -> CORTISOL base = 0.6 + 1.0*0.8 = 1.4 (then SD modifier)
    ASSERT_GT(gains.production_gains[idx(HormoneId::CORTISOL)], 1.0f);
    return true;
}

TEST(Cloninger_high_reward_dependence_boosts_OXYTOCIN) {
    TemperamentProfile profile{};
    profile.reward_dependence = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // RD=1.0 -> OX base = 0.5 + 1.0*1.0 = 1.5, then C modifier
    ASSERT_GT(gains.production_gains[idx(HormoneId::OXYTOCIN)], 1.0f);
    return true;
}

TEST(Cloninger_high_persistence_boosts_DA_TONIC_decay) {
    TemperamentProfile profile{};
    profile.persistence = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // P=1.0 -> DA_TONIC decay = 0.5 + 1.0*1.0 = 1.5
    ASSERT_GT(gains.decay_gains[idx(HormoneId::DOPAMINE_TONIC)], 1.0f);
    return true;
}

// ============================================================================
// Character Influence Tests
// ============================================================================

TEST(Cloninger_high_self_directedness_reduces_CORTISOL) {
    TemperamentProfile profile{};
    profile.self_directedness = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // Baseline CORTISOL from HA=0.5 is 0.6+0.5*0.8=1.0
    // SD=1.0 multiplies by max(0.3, 1.0 - 1.0*0.5) = 0.5
    // So 1.0 * 0.5 = 0.5 -- reduced
    TemperamentProfile baseline_profile{};
    baseline_profile.self_directedness = 0.0f;
    CloningerSystem baseline_sys(baseline_profile);
    auto baseline_gains = baseline_sys.compute_gains();

    ASSERT_LT(gains.production_gains[idx(HormoneId::CORTISOL)],
              baseline_gains.production_gains[idx(HormoneId::CORTISOL)]);
    return true;
}

TEST(Cloninger_high_self_directedness_increases_T3_T4) {
    TemperamentProfile profile{};
    profile.self_directedness = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // SD=1.0 -> T3_T4 = 0.7 + 1.0*0.6 = 1.3
    ASSERT_GT(gains.production_gains[idx(HormoneId::T3_T4)], 1.0f);
    return true;
}

TEST(Cloninger_high_cooperativeness_boosts_OXYTOCIN) {
    TemperamentProfile profile{};
    profile.cooperativeness = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // RD=0.5 -> OX base = 1.0, C=1.0 -> *= 0.8+1.0*0.4 = 1.2
    // So OX = 1.0 * 1.2 = 1.2
    TemperamentProfile low_coop{};
    low_coop.cooperativeness = 0.0f;
    CloningerSystem low_sys(low_coop);
    auto low_gains = low_sys.compute_gains();

    ASSERT_GT(gains.production_gains[idx(HormoneId::OXYTOCIN)],
              low_gains.production_gains[idx(HormoneId::OXYTOCIN)]);
    return true;
}

TEST(Cloninger_high_self_transcendence_boosts_ANANDAMIDE_MELATONIN) {
    TemperamentProfile profile{};
    profile.self_transcendence = 1.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // ST=1.0 -> ANANDAMIDE = 0.5 + 1.0*1.0 = 1.5
    ASSERT_GT(gains.production_gains[idx(HormoneId::ANANDAMIDE)], 1.0f);
    // ST=1.0 -> MELATONIN = 0.7 + 1.0*0.6 = 1.3
    ASSERT_GT(gains.production_gains[idx(HormoneId::MELATONIN)], 1.0f);
    return true;
}

// ============================================================================
// Zero Temperament Tests
// ============================================================================

TEST(Cloninger_zero_novelty_seeking_DA_at_minimum) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.0f;
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // NS=0 -> DA = 0.5 + 0*1.0 = 0.5, which is >= GAIN_MIN (0.3)
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::DOPAMINE_TONIC)], 0.5f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::DOPAMINE_PHASIC)], 0.5f, 0.01f);
    return true;
}

// ============================================================================
// Gain Clamping Tests
// ============================================================================

TEST(Cloninger_all_gains_clamped_between_min_max) {
    // Test with extreme high values
    TemperamentProfile profile{};
    profile.novelty_seeking = 1.0f;
    profile.harm_avoidance = 1.0f;
    profile.reward_dependence = 1.0f;
    profile.persistence = 1.0f;
    profile.self_directedness = 0.0f;  // maximize CORTISOL
    profile.cooperativeness = 1.0f;
    profile.self_transcendence = 1.0f;
    profile.allostatic_load = 5.0f;
    profile.trauma_encoding_strength = 1.0f;
    profile.attachment_security = 0.0f;
    profile.attachment_anxiety = 1.0f;

    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_GE(gains.production_gains[i], GAIN_MIN);
        ASSERT_LE(gains.production_gains[i], GAIN_MAX);
        ASSERT_GE(gains.decay_gains[i], GAIN_MIN);
        ASSERT_LE(gains.decay_gains[i], GAIN_MAX);
    }
    return true;
}

// ============================================================================
// Developmental Modifier Tests
// ============================================================================

TEST(Cloninger_allostatic_load_increases_CORTISOL) {
    TemperamentProfile with_load{};
    with_load.allostatic_load = 3.0f;
    CloningerSystem sys_load(with_load);
    auto gains_load = sys_load.compute_gains();

    TemperamentProfile no_load{};
    no_load.allostatic_load = 0.0f;
    CloningerSystem sys_no(no_load);
    auto gains_no = sys_no.compute_gains();

    ASSERT_GT(gains_load.production_gains[idx(HormoneId::CORTISOL)],
              gains_no.production_gains[idx(HormoneId::CORTISOL)]);
    return true;
}

TEST(Cloninger_trauma_encoding_boosts_effective_harm_avoidance) {
    TemperamentProfile with_trauma{};
    with_trauma.harm_avoidance = 0.5f;
    with_trauma.trauma_encoding_strength = 1.0f;  // +0.3 to effective HA
    CloningerSystem sys_t(with_trauma);
    auto gains_t = sys_t.compute_gains();

    TemperamentProfile no_trauma{};
    no_trauma.harm_avoidance = 0.5f;
    no_trauma.trauma_encoding_strength = 0.0f;
    CloningerSystem sys_n(no_trauma);
    auto gains_n = sys_n.compute_gains();

    // Trauma encoding should boost serotonin (via higher effective HA)
    ASSERT_GT(gains_t.production_gains[idx(HormoneId::SEROTONIN)],
              gains_n.production_gains[idx(HormoneId::SEROTONIN)]);
    return true;
}

TEST(Cloninger_insecure_attachment_CORTISOL_up_OXYTOCIN_down) {
    TemperamentProfile insecure{};
    insecure.attachment_security = 0.1f;   // Low security
    insecure.attachment_anxiety = 0.9f;    // High anxiety
    CloningerSystem sys_i(insecure);
    auto gains_i = sys_i.compute_gains();

    TemperamentProfile secure{};
    secure.attachment_security = 0.8f;
    secure.attachment_anxiety = 0.1f;
    CloningerSystem sys_s(secure);
    auto gains_s = sys_s.compute_gains();

    // Insecure-anxious: cortisol up, oxytocin down
    ASSERT_GT(gains_i.production_gains[idx(HormoneId::CORTISOL)],
              gains_s.production_gains[idx(HormoneId::CORTISOL)]);
    ASSERT_LT(gains_i.production_gains[idx(HormoneId::OXYTOCIN)],
              gains_s.production_gains[idx(HormoneId::OXYTOCIN)]);
    return true;
}

// ============================================================================
// Multiplicative Gains Test
// ============================================================================

TEST(Cloninger_gains_are_multiplicative) {
    // Self-directedness modifies CORTISOL multiplicatively on base HA gain
    TemperamentProfile profile{};
    profile.harm_avoidance = 0.8f;
    profile.self_directedness = 0.8f;

    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // Base cortisol from HA: 0.6 + 0.8*0.8 = 1.24
    // SD multiplier: max(0.3, 1.0 - 0.8*0.5) = 0.6
    // Expected: 1.24 * 0.6 = 0.744 (before allostatic/attachment adjustments)
    float expected = (0.6f + 0.8f * 0.8f) * std::max(0.3f, 1.0f - 0.8f * 0.5f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::CORTISOL)], expected, 0.01f);
    return true;
}

TEST(Cloninger_self_directedness_multiplicatively_modifies_CORTISOL) {
    // Verify SD is multiplicative, not additive
    TemperamentProfile low_sd{};
    low_sd.harm_avoidance = 0.7f;
    low_sd.self_directedness = 0.0f;
    CloningerSystem sys_low(low_sd);
    auto gains_low = sys_low.compute_gains();

    TemperamentProfile high_sd{};
    high_sd.harm_avoidance = 0.7f;
    high_sd.self_directedness = 1.0f;
    CloningerSystem sys_high(high_sd);
    auto gains_high = sys_high.compute_gains();

    // Base cortisol is same (same HA), but SD reduces it multiplicatively
    float base_cortisol = 0.6f + 0.7f * 0.8f;  // 1.16
    float low_expected = base_cortisol * 1.0f;   // SD=0 -> multiplier=1.0
    float high_expected = base_cortisol * 0.5f;  // SD=1 -> multiplier=0.5

    ASSERT_NEAR(gains_low.production_gains[idx(HormoneId::CORTISOL)], low_expected, 0.01f);
    ASSERT_NEAR(gains_high.production_gains[idx(HormoneId::CORTISOL)], high_expected, 0.01f);
    return true;
}

// ============================================================================
// Accessor and Mutation Tests
// ============================================================================

TEST(Cloninger_update_temperament_changes_gains) {
    TemperamentProfile initial{};
    initial.novelty_seeking = 0.2f;
    CloningerSystem sys(initial);
    auto gains1 = sys.compute_gains();

    TemperamentProfile updated{};
    updated.novelty_seeking = 0.9f;
    sys.update_temperament(updated);
    auto gains2 = sys.compute_gains();

    ASSERT_NE(gains1.production_gains[idx(HormoneId::DOPAMINE_TONIC)],
              gains2.production_gains[idx(HormoneId::DOPAMINE_TONIC)]);
    return true;
}

TEST(Cloninger_temperament_accessor_returns_stored_profile) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.42f;
    profile.harm_avoidance = 0.73f;
    CloningerSystem sys(profile);

    ASSERT_NEAR(sys.temperament().novelty_seeking, 0.42f, 0.001f);
    ASSERT_NEAR(sys.temperament().harm_avoidance, 0.73f, 0.001f);
    return true;
}

// ============================================================================
// Extreme Value Tests
// ============================================================================

TEST(Cloninger_all_zero_temperament_valid_gains) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.0f;
    profile.harm_avoidance = 0.0f;
    profile.reward_dependence = 0.0f;
    profile.persistence = 0.0f;
    profile.self_directedness = 0.0f;
    profile.cooperativeness = 0.0f;
    profile.self_transcendence = 0.0f;

    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_GE(gains.production_gains[i], GAIN_MIN);
        ASSERT_LE(gains.production_gains[i], GAIN_MAX);
    }
    return true;
}

TEST(Cloninger_all_one_temperament_valid_gains) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 1.0f;
    profile.harm_avoidance = 1.0f;
    profile.reward_dependence = 1.0f;
    profile.persistence = 1.0f;
    profile.self_directedness = 1.0f;
    profile.cooperativeness = 1.0f;
    profile.self_transcendence = 1.0f;

    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_GE(gains.production_gains[i], GAIN_MIN);
        ASSERT_LE(gains.production_gains[i], GAIN_MAX);
    }
    return true;
}

// ============================================================================
// GainProfile Structural Tests
// ============================================================================

TEST(Cloninger_GainProfile_size_matches_HORMONE_COUNT) {
    GainProfile gains;
    ASSERT_EQ(gains.production_gains.size(), static_cast<size_t>(HORMONE_COUNT));
    ASSERT_EQ(gains.decay_gains.size(), static_cast<size_t>(HORMONE_COUNT));
    return true;
}

// ============================================================================
// Determinism Tests
// ============================================================================

TEST(Cloninger_gains_are_deterministic) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.7f;
    profile.harm_avoidance = 0.3f;
    profile.self_transcendence = 0.9f;

    CloningerSystem sys(profile);
    auto gains1 = sys.compute_gains();
    auto gains2 = sys.compute_gains();

    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_EQ(gains1.production_gains[i], gains2.production_gains[i]);
        ASSERT_EQ(gains1.decay_gains[i], gains2.decay_gains[i]);
    }
    return true;
}

TEST(Cloninger_multiple_compute_gains_same_result) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.6f;
    profile.cooperativeness = 0.8f;

    CloningerSystem sys(profile);
    auto g1 = sys.compute_gains();
    auto g2 = sys.compute_gains();
    auto g3 = sys.compute_gains();

    ASSERT_EQ(g1.production_gains[idx(HormoneId::DOPAMINE_TONIC)],
              g3.production_gains[idx(HormoneId::DOPAMINE_TONIC)]);
    ASSERT_EQ(g1.production_gains[idx(HormoneId::OXYTOCIN)],
              g2.production_gains[idx(HormoneId::OXYTOCIN)]);
    return true;
}

// ============================================================================
// Resilience Independence Test
// ============================================================================

TEST(Cloninger_resilience_does_not_directly_affect_gains) {
    // Resilience is used elsewhere (allostatic recovery), not in gain computation
    TemperamentProfile low_res{};
    low_res.resilience = 0.1f;
    CloningerSystem sys_low(low_res);
    auto gains_low = sys_low.compute_gains();

    TemperamentProfile high_res{};
    high_res.resilience = 0.9f;
    CloningerSystem sys_high(high_res);
    auto gains_high = sys_high.compute_gains();

    // Resilience alone should not change gains (no allostatic load or attachment issues)
    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_EQ(gains_low.production_gains[i], gains_high.production_gains[i]);
        ASSERT_EQ(gains_low.decay_gains[i], gains_high.decay_gains[i]);
    }
    return true;
}

// ============================================================================
// Default Unused Channels
// ============================================================================

TEST(Cloninger_unused_channels_default_to_one) {
    TemperamentProfile profile{};
    profile.novelty_seeking = 0.8f;  // modifies DA channels only
    CloningerSystem sys(profile);
    auto gains = sys.compute_gains();

    // Channels not touched by any dimension: CRH, ACTH, NE, INSULIN, GLUCAGON, IL6
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::CRH)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::ACTH)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::NOREPINEPHRINE)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::INSULIN)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::GLUCAGON)], 1.0f, 0.01f);
    ASSERT_NEAR(gains.production_gains[idx(HormoneId::IL6)], 1.0f, 0.01f);
    return true;
}
