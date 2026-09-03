/**
 * @file test_interoceptive.cpp
 * @brief Tests for the InteroceptiveModel body-state computation
 *
 * Covers: polyvagal hierarchy, cardiac coherence, respiratory rhythm,
 * gut-brain signal, immune tracking, allostatic load accumulation/recovery,
 * proprioceptive tone, thermoregulation, insular integration, bus interface,
 * and resilience modulation.
 */

#include <opencog/entelechy/interoceptive.hpp>
#include <opencog/endocrine/types.hpp>

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
// Helper: make an EndocrineState with specific hormone levels
// ============================================================================

static EndocrineState make_state() {
    EndocrineState state{};
    return state;
}

// ============================================================================
// Default State Tests
// ============================================================================

TEST(Interoceptive_default_vagal_tone) {
    InteroceptiveModel model;
    ASSERT_NEAR(model.state().vagal_tone, 0.5f, 0.001f);
    return true;
}

TEST(Interoceptive_default_sympathetic_drive) {
    InteroceptiveModel model;
    ASSERT_NEAR(model.state().sympathetic_drive, 0.3f, 0.001f);
    return true;
}

TEST(Interoceptive_default_dorsal_vagal) {
    InteroceptiveModel model;
    ASSERT_NEAR(model.state().dorsal_vagal, 0.0f, 0.001f);
    return true;
}

TEST(Interoceptive_default_dominant_tier_ventral_vagal) {
    InteroceptiveModel model;
    ASSERT_EQ(static_cast<uint8_t>(model.dominant_tier()),
              static_cast<uint8_t>(PolyvagalTier::VENTRAL_VAGAL));
    return true;
}

TEST(Interoceptive_default_material_self_positive) {
    InteroceptiveModel model;
    float ms = model.material_self();
    ASSERT_GT(ms, 0.0f);
    ASSERT_LT(ms, 1.0f);
    return true;
}

// ============================================================================
// Polyvagal Hierarchy Tests
// ============================================================================

TEST(Interoceptive_ventral_vagal_with_oxytocin_low_stress) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::OXYTOCIN] = 0.7f;
    state[HormoneId::CORTISOL] = 0.1f;
    state[HormoneId::NOREPINEPHRINE] = 0.1f;
    state[HormoneId::SEROTONIN] = 0.5f;

    // Run many ticks to converge
    for (int i = 0; i < 50; ++i) {
        model.update(state);
    }

    // Ventral vagal should be dominant
    ASSERT_GT(model.state().vagal_tone, model.state().sympathetic_drive);
    ASSERT_EQ(static_cast<uint8_t>(model.dominant_tier()),
              static_cast<uint8_t>(PolyvagalTier::VENTRAL_VAGAL));
    return true;
}

TEST(Interoceptive_sympathetic_with_high_cortisol_or_NE) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.6f;        // Above 0.4 threshold
    state[HormoneId::NOREPINEPHRINE] = 0.3f;
    state[HormoneId::OXYTOCIN] = 0.1f;
    state[HormoneId::SEROTONIN] = 0.5f;

    for (int i = 0; i < 50; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.state().sympathetic_drive, 0.3f);
    ASSERT_EQ(static_cast<uint8_t>(model.dominant_tier()),
              static_cast<uint8_t>(PolyvagalTier::SYMPATHETIC));
    return true;
}

TEST(Interoceptive_dorsal_vagal_with_overwhelming_stress) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.9f;        // > 0.7
    state[HormoneId::NOREPINEPHRINE] = 0.9f;  // > 0.7
    state[HormoneId::SEROTONIN] = 0.1f;       // < 0.2

    for (int i = 0; i < 100; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.state().dorsal_vagal, 0.5f);
    ASSERT_EQ(static_cast<uint8_t>(model.dominant_tier()),
              static_cast<uint8_t>(PolyvagalTier::DORSAL_VAGAL));
    return true;
}

// ============================================================================
// Cardiac Coherence Tests
// ============================================================================

TEST(Interoceptive_cardiac_coherence_rises_with_vagal_tone) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    // Safe conditions -> high vagal tone -> high cardiac coherence
    state[HormoneId::OXYTOCIN] = 0.7f;
    state[HormoneId::CORTISOL] = 0.1f;
    state[HormoneId::NOREPINEPHRINE] = 0.1f;
    state[HormoneId::SEROTONIN] = 0.5f;

    for (int i = 0; i < 50; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.state().cardiac_coherence, 0.5f);
    return true;
}

TEST(Interoceptive_cardiac_coherence_drops_with_sympathetic) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.7f;
    state[HormoneId::NOREPINEPHRINE] = 0.6f;
    state[HormoneId::SEROTONIN] = 0.5f;

    for (int i = 0; i < 50; ++i) {
        model.update(state);
    }

    // Sympathetic drive high -> cardiac coherence low
    ASSERT_LT(model.state().cardiac_coherence, 0.5f);
    return true;
}

// ============================================================================
// Respiratory Rhythm Tests
// ============================================================================

TEST(Interoceptive_respiratory_rhythm_influenced_by_vagal_tone) {
    InteroceptiveModel model;
    EndocrineState safe_state = make_state();
    safe_state[HormoneId::OXYTOCIN] = 0.7f;
    safe_state[HormoneId::CORTISOL] = 0.1f;
    safe_state[HormoneId::NOREPINEPHRINE] = 0.1f;
    safe_state[HormoneId::SEROTONIN] = 0.5f;

    for (int i = 0; i < 50; ++i) {
        model.update(safe_state);
    }

    // High vagal tone -> good respiratory rhythm
    ASSERT_GT(model.state().respiratory_rhythm, 0.4f);
    return true;
}

// ============================================================================
// Gut-Brain Signal Tests
// ============================================================================

TEST(Interoceptive_gut_brain_tracks_serotonin) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::SEROTONIN] = 0.9f;

    for (int i = 0; i < 100; ++i) {
        model.update(state);
    }

    // 90% of serotonin in gut -> gut-brain should be elevated
    ASSERT_GT(model.state().gut_brain_signal, 0.3f);
    return true;
}

// ============================================================================
// Immune Extended Tests
// ============================================================================

TEST(Interoceptive_immune_extended_tracks_IL6) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::IL6] = 0.8f;

    for (int i = 0; i < 50; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.state().immune_extended, 0.1f);
    return true;
}

// ============================================================================
// Allostatic Load Tests
// ============================================================================

TEST(Interoceptive_allostatic_load_starts_at_zero) {
    InteroceptiveModel model;
    ASSERT_NEAR(model.allostatic_load(), 0.0f, 0.001f);
    return true;
}

TEST(Interoceptive_allostatic_load_accumulates_with_sustained_cortisol) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.7f;

    // Run 200 ticks (needs >100 to start accumulating)
    for (int i = 0; i < 200; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.allostatic_load(), 0.0f);
    return true;
}

TEST(Interoceptive_allostatic_load_needs_100_ticks_to_start) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.7f;

    // Run exactly 100 ticks -- should NOT accumulate yet (need >100)
    for (int i = 0; i < 100; ++i) {
        model.update(state);
    }

    ASSERT_NEAR(model.allostatic_load(), 0.0f, 0.001f);
    return true;
}

TEST(Interoceptive_allostatic_load_recovers_slowly_low_cortisol) {
    InteroceptiveModel model;
    EndocrineState high_state = make_state();
    high_state[HormoneId::CORTISOL] = 0.8f;

    // Build up allostatic load
    for (int i = 0; i < 300; ++i) {
        model.update(high_state);
    }

    float load_after_stress = model.allostatic_load();
    ASSERT_GT(load_after_stress, 0.0f);

    // Now recovery with low cortisol
    EndocrineState low_state = make_state();
    low_state[HormoneId::CORTISOL] = 0.1f;

    for (int i = 0; i < 500; ++i) {
        model.update(low_state);
    }

    ASSERT_LT(model.allostatic_load(), load_after_stress);
    return true;
}

TEST(Interoceptive_allostatic_load_clamped_0_to_5) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 1.0f;

    // Run a very long time
    for (int i = 0; i < 100000; ++i) {
        model.update(state);
    }

    ASSERT_LE(model.allostatic_load(), 5.0f);
    ASSERT_GE(model.allostatic_load(), 0.0f);
    return true;
}

// ============================================================================
// Proprioceptive Tone Tests
// ============================================================================

TEST(Interoceptive_proprioceptive_degrades_with_nociceptive) {
    InteroceptiveModel model;
    EndocrineState state = make_state();

    // Inject pain
    model.inject_pain(0.9f);
    model.update(state);

    // Proprioceptive should be reduced by pain
    ASSERT_LT(model.state().proprioceptive_tone, 0.5f);
    return true;
}

TEST(Interoceptive_proprioceptive_degrades_with_dorsal_vagal) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.9f;
    state[HormoneId::NOREPINEPHRINE] = 0.9f;
    state[HormoneId::SEROTONIN] = 0.1f;

    // Drive dorsal vagal high
    for (int i = 0; i < 80; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.state().dorsal_vagal, 0.3f);
    // Proprioceptive tone should be degraded
    ASSERT_LT(model.state().proprioceptive_tone, 0.5f);
    return true;
}

// ============================================================================
// Thermoregulatory Tests
// ============================================================================

TEST(Interoceptive_thermoregulatory_disrupted_by_allostatic_load) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.9f;

    // Build up allostatic load over many ticks. Allostatic load accumulates
    // deliberately slowly (McEwen's "chronic stress" model: +0.0002/tick at
    // resilience=0.5, only after 100 ticks of sustained high cortisol) to
    // represent long-term wear rather than acute stress, so reaching a load
    // high enough to visibly disrupt thermoregulatory (< 0.5) requires on
    // the order of 10^4 ticks, not the 500 originally used here (which only
    // reaches allostatic_load ≈ 0.08 / thermoregulatory ≈ 0.98).
    for (int i = 0; i < 15000; ++i) {
        model.update(state);
    }

    ASSERT_GT(model.allostatic_load(), 0.0f);
    // Thermoregulatory should be reduced (inversely related to allostatic_load/5)
    ASSERT_LT(model.state().thermoregulatory, 0.5f);
    return true;
}

// ============================================================================
// Insular Integration Tests
// ============================================================================

TEST(Interoceptive_insular_integration_equals_material_self) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::OXYTOCIN] = 0.5f;
    state[HormoneId::CORTISOL] = 0.2f;
    state[HormoneId::SEROTONIN] = 0.5f;

    model.update(state);

    // After update, insular_integration should equal material_self()
    ASSERT_NEAR(model.state().insular_integration, model.material_self(), 0.001f);
    return true;
}

// ============================================================================
// Bus Interface Tests
// ============================================================================

TEST(Interoceptive_to_bus_values_returns_12_values) {
    InteroceptiveModel model;
    auto values = model.to_bus_values();
    ASSERT_EQ(values.size(), 12u);
    return true;
}

TEST(Interoceptive_default_channel_configs_returns_12_configs) {
    auto configs = InteroceptiveModel::default_channel_configs();
    ASSERT_EQ(configs.size(), 12u);
    return true;
}

TEST(Interoceptive_channel_configs_correct_channel_numbers) {
    auto configs = InteroceptiveModel::default_channel_configs();
    for (size_t i = 0; i < 12; ++i) {
        ASSERT_EQ(configs[i].channel, static_cast<uint8_t>(20 + i));
    }
    return true;
}

TEST(Interoceptive_allostatic_load_channel_ceiling_5) {
    auto configs = InteroceptiveModel::default_channel_configs();
    // ALLOSTATIC_LOAD is ch28, index 8 in the array
    ASSERT_NEAR(configs[8].ceiling, 5.0f, 0.01f);
    return true;
}

TEST(Interoceptive_nociceptive_signal_fast_half_life) {
    auto configs = InteroceptiveModel::default_channel_configs();
    // NOCICEPTIVE_SIGNAL is ch30, index 10
    ASSERT_NEAR(configs[10].half_life, 5.0f, 0.01f);
    return true;
}

TEST(Interoceptive_vagal_tone_slow_half_life) {
    auto configs = InteroceptiveModel::default_channel_configs();
    // VAGAL_TONE is ch20, index 0
    ASSERT_NEAR(configs[0].half_life, 50.0f, 0.01f);
    return true;
}

// ============================================================================
// State Range Tests
// ============================================================================

TEST(Interoceptive_state_bounded_0_1_except_allostatic) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.5f;
    state[HormoneId::NOREPINEPHRINE] = 0.5f;
    state[HormoneId::SEROTONIN] = 0.5f;
    state[HormoneId::OXYTOCIN] = 0.5f;
    state[HormoneId::IL6] = 0.5f;

    for (int i = 0; i < 100; ++i) {
        model.update(state);
    }

    auto& s = model.state();
    ASSERT_GE(s.vagal_tone, 0.0f);         ASSERT_LE(s.vagal_tone, 1.0f);
    ASSERT_GE(s.sympathetic_drive, 0.0f);  ASSERT_LE(s.sympathetic_drive, 1.0f);
    ASSERT_GE(s.dorsal_vagal, 0.0f);       ASSERT_LE(s.dorsal_vagal, 1.0f);
    ASSERT_GE(s.cardiac_coherence, 0.0f);  ASSERT_LE(s.cardiac_coherence, 1.0f);
    ASSERT_GE(s.respiratory_rhythm, 0.0f); ASSERT_LE(s.respiratory_rhythm, 1.0f);
    ASSERT_GE(s.gut_brain_signal, 0.0f);   ASSERT_LE(s.gut_brain_signal, 1.0f);
    ASSERT_GE(s.immune_extended, 0.0f);    ASSERT_LE(s.immune_extended, 1.0f);
    ASSERT_GE(s.allostatic_load, 0.0f);    ASSERT_LE(s.allostatic_load, 5.0f);
    ASSERT_GE(s.proprioceptive_tone, 0.0f); ASSERT_LE(s.proprioceptive_tone, 1.0f);
    ASSERT_GE(s.nociceptive_signal, 0.0f); ASSERT_LE(s.nociceptive_signal, 1.0f);
    ASSERT_GE(s.thermoregulatory, 0.0f);   ASSERT_LE(s.thermoregulatory, 1.0f);
    return true;
}

// ============================================================================
// Resilience Tests
// ============================================================================

TEST(Interoceptive_resilience_affects_allostatic_recovery) {
    // High resilience -> faster recovery
    InteroceptiveModel model_high;
    model_high.set_resilience(0.9f);
    EndocrineState high_stress = make_state();
    high_stress[HormoneId::CORTISOL] = 0.8f;

    for (int i = 0; i < 300; ++i) {
        model_high.update(high_stress);
    }
    float load_high = model_high.allostatic_load();

    // Low resilience -> more accumulated load (resilience reduces accumulation rate)
    InteroceptiveModel model_low;
    model_low.set_resilience(0.1f);

    for (int i = 0; i < 300; ++i) {
        model_low.update(high_stress);
    }
    float load_low = model_low.allostatic_load();

    // Lower resilience should accumulate MORE load
    ASSERT_GT(load_low, load_high);
    return true;
}

TEST(Interoceptive_set_resilience_works) {
    InteroceptiveModel model;
    ASSERT_NEAR(model.resilience(), 0.5f, 0.001f);

    model.set_resilience(0.8f);
    ASSERT_NEAR(model.resilience(), 0.8f, 0.001f);

    // Should clamp
    model.set_resilience(1.5f);
    ASSERT_NEAR(model.resilience(), 1.0f, 0.001f);

    model.set_resilience(-0.5f);
    ASSERT_NEAR(model.resilience(), 0.0f, 0.001f);
    return true;
}

// ============================================================================
// Convergence Test
// ============================================================================

TEST(Interoceptive_multiple_updates_converge_to_stable) {
    InteroceptiveModel model;
    EndocrineState state = make_state();
    state[HormoneId::CORTISOL] = 0.3f;
    state[HormoneId::NOREPINEPHRINE] = 0.3f;
    state[HormoneId::SEROTONIN] = 0.5f;
    state[HormoneId::OXYTOCIN] = 0.3f;

    // Run many ticks
    for (int i = 0; i < 200; ++i) {
        model.update(state);
    }
    float vt1 = model.state().vagal_tone;
    float sd1 = model.state().sympathetic_drive;

    // Run more ticks
    for (int i = 0; i < 200; ++i) {
        model.update(state);
    }
    float vt2 = model.state().vagal_tone;
    float sd2 = model.state().sympathetic_drive;

    // Should converge (differences very small after settling)
    ASSERT_NEAR(vt1, vt2, 0.05f);
    ASSERT_NEAR(sd1, sd2, 0.05f);
    return true;
}
