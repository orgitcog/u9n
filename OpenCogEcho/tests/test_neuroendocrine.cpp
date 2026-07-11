/**
 * @file test_neuroendocrine.cpp
 * @brief Tests for VNS ↔ VES NeuroEndocrine Bridge coupling
 *
 * Covers: bidirectional signal transfer, tick rate bridging,
 * accumulation, thresholds, and integrated system coupling.
 */

#include <opencog/nervous/neuroendocrine_bridge.hpp>
#include <opencog/nervous/nerve_bus.hpp>
#include <opencog/endocrine/hormone_bus.hpp>
#include <opencog/endocrine/types.hpp>
#include <opencog/nervous/types.hpp>

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
#define ASSERT_NE(a, b) if ((a) == (b)) { return false; }
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }
#define ASSERT_GT(a, b) if (!((a) > (b))) { return false; }
#define ASSERT_LT(a, b) if (!((a) < (b))) { return false; }
#define ASSERT_GE(a, b) if (!((a) >= (b))) { return false; }
#define ASSERT_LE(a, b) if (!((a) <= (b))) { return false; }

using namespace opencog;
using namespace opencog::nerv;

// ============================================================================
// Bridge Construction
// ============================================================================

TEST(NeuroEndocrine_construction) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;

    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);
    ASSERT_EQ(bridge.accumulation_count(), 0u);
    return true;
}

// ============================================================================
// VNS → VES Tests (neural → hormonal)
// ============================================================================

TEST(NeuroEndocrine_sympathetic_to_CRH) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Fire sympathetic signal
    nerve_bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.8f);

    // Accumulate and sync
    bridge.accumulate();
    bridge.sync();

    // CRH should have been produced
    float crh = hormone_bus.concentration(endo::HormoneId::CRH);
    ASSERT_GT(crh, 0.0f);
    return true;
}

TEST(NeuroEndocrine_parasympathetic_to_serotonin) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    nerve_bus.fire(NeuralChannelId::PARASYMPATHETIC_OUT, 0.7f);
    bridge.accumulate();
    bridge.sync();

    float serotonin = hormone_bus.concentration(endo::HormoneId::SEROTONIN);
    ASSERT_GT(serotonin, 0.0f);
    return true;
}

TEST(NeuroEndocrine_amygdala_neg_to_cortisol) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Negative valence (inhibitory amygdala signal)
    nerve_bus.fire(NeuralChannelId::AMYGDALA_VALENCE, -0.7f);
    bridge.accumulate();
    bridge.sync();

    float cortisol = hormone_bus.concentration(endo::HormoneId::CORTISOL);
    ASSERT_GT(cortisol, 0.0f);
    return true;
}

TEST(NeuroEndocrine_reward_to_dopamine) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    nerve_bus.fire(NeuralChannelId::REWARD_PREDICTION, 0.6f);
    bridge.accumulate();
    bridge.sync();

    float da = hormone_bus.concentration(endo::HormoneId::DOPAMINE_PHASIC);
    ASSERT_GT(da, 0.0f);
    return true;
}

TEST(NeuroEndocrine_pain_to_IL6) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    nerve_bus.fire(NeuralChannelId::PAIN_SIGNAL, 0.7f);
    bridge.accumulate();
    bridge.sync();

    float il6 = hormone_bus.concentration(endo::HormoneId::IL6);
    ASSERT_GT(il6, 0.0f);
    return true;
}

TEST(NeuroEndocrine_social_to_oxytocin) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    nerve_bus.fire(NeuralChannelId::SOCIAL_AFFERENT, 0.6f);
    bridge.accumulate();
    bridge.sync();

    float oxy = hormone_bus.concentration(endo::HormoneId::OXYTOCIN);
    ASSERT_GT(oxy, 0.0f);
    return true;
}

// ============================================================================
// VES → VNS Tests (hormonal → neural)
// ============================================================================

TEST(NeuroEndocrine_cortisol_to_arousal) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Inject cortisol above threshold
    hormone_bus.produce(endo::HormoneId::CORTISOL, 0.7f);
    bridge.accumulate();  // need at least one accumulation
    bridge.sync();

    float arousal = nerve_bus.activation(NeuralChannelId::RETICULAR_ACTIVATION);
    ASSERT_GT(arousal, 0.0f);
    return true;
}

TEST(NeuroEndocrine_serotonin_to_parasympathetic) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    hormone_bus.produce(endo::HormoneId::SEROTONIN, 0.6f);
    bridge.accumulate();
    bridge.sync();

    float para = nerve_bus.activation(NeuralChannelId::PARASYMPATHETIC_OUT);
    ASSERT_GT(para, 0.0f);
    return true;
}

TEST(NeuroEndocrine_NE_to_thalamic_gate) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    hormone_bus.produce(endo::HormoneId::NOREPINEPHRINE, 0.5f);
    bridge.accumulate();
    bridge.sync();

    float gate = nerve_bus.activation(NeuralChannelId::THALAMIC_GATE);
    ASSERT_GT(gate, 0.0f);
    return true;
}

TEST(NeuroEndocrine_DA_to_go_signal) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    hormone_bus.produce(endo::HormoneId::DOPAMINE_TONIC, 0.6f);
    bridge.accumulate();
    bridge.sync();

    float go = nerve_bus.activation(NeuralChannelId::BASAL_GANGLIA_GO);
    ASSERT_GT(go, 0.0f);
    return true;
}

// ============================================================================
// Tick Rate Bridging Tests
// ============================================================================

TEST(NeuroEndocrine_accumulation) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Accumulate 10 ticks (1 VES tick worth)
    nerve_bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.3f);
    for (int i = 0; i < 10; ++i) {
        bridge.accumulate();
    }
    ASSERT_EQ(bridge.accumulation_count(), 10u);

    bridge.sync();
    ASSERT_EQ(bridge.accumulation_count(), 0u);  // reset after sync

    float crh = hormone_bus.concentration(endo::HormoneId::CRH);
    ASSERT_GT(crh, 0.0f);
    return true;
}

TEST(NeuroEndocrine_below_threshold_no_transfer) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Very weak signal — below sympathetic threshold
    nerve_bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.05f);
    bridge.accumulate();
    bridge.sync();

    float crh = hormone_bus.concentration(endo::HormoneId::CRH);
    ASSERT_NEAR(crh, 0.0f, 0.001f);
    return true;
}

TEST(NeuroEndocrine_threshold_configuration) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Set high threshold
    bridge.set_sympathetic_threshold(0.9f);
    nerve_bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.5f);
    bridge.accumulate();
    bridge.sync();

    float crh = hormone_bus.concentration(endo::HormoneId::CRH);
    ASSERT_NEAR(crh, 0.0f, 0.001f);  // below threshold
    return true;
}

TEST(NeuroEndocrine_homeostatic_summary) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    nerve_cfg.global_decay_rate = 0.0f;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Set multiple hormones
    hormone_bus.produce(endo::HormoneId::CORTISOL, 0.3f);
    hormone_bus.produce(endo::HormoneId::SEROTONIN, 0.3f);
    hormone_bus.produce(endo::HormoneId::NOREPINEPHRINE, 0.3f);

    bridge.accumulate();
    bridge.sync();

    // Homeostatic signal should reflect the summary
    float homeostatic = nerve_bus.activation(NeuralChannelId::HOMEOSTATIC_SIGNAL);
    ASSERT_GT(std::abs(homeostatic), 0.0f);
    return true;
}

// ============================================================================
// Bidirectional Integration Test
// ============================================================================

TEST(NeuroEndocrine_bidirectional_stress_cycle) {
    NerveBusConfig nerve_cfg;
    nerve_cfg.enable_propagation = false;
    NerveBus nerve_bus(nerve_cfg);
    endo::HormoneBus hormone_bus;
    NeuroEndocrineBridge bridge(nerve_bus, hormone_bus);

    // Step 1: Neural threat → VES cortisol
    nerve_bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.8f);
    nerve_bus.fire(NeuralChannelId::AMYGDALA_VALENCE, -0.7f);
    bridge.accumulate();
    bridge.sync();

    float cortisol = hormone_bus.concentration(endo::HormoneId::CORTISOL);
    ASSERT_GT(cortisol, 0.0f);

    // Step 2: VES cortisol → VNS reticular activation
    // (cortisol still present from step 1)
    bridge.accumulate();
    bridge.sync();

    float reticular = nerve_bus.activation(NeuralChannelId::RETICULAR_ACTIVATION);
    // If cortisol was above 0.5, reticular should be elevated
    if (cortisol > 0.5f) {
        ASSERT_GT(reticular, 0.0f);
    }

    return true;
}
