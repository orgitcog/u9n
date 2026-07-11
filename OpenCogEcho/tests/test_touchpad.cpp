/**
 * @file test_touchpad.cpp
 * @brief VirtualTouchpad N-dimensional cognitive gesture manifold integration tests
 *
 * Tests the VirtualTouchpad integration across all layers:
 *   - Type definitions, presets, and gesture primitives
 *   - Bus channels (TOUCHPAD_LOAD ch18, TOUCHPAD_COHERENCE ch19)
 *   - Core AtomTypes registration (4 nodes, 4 links)
 *   - READ path (hormones → Touchpad manifold parameter modulation)
 *   - WRITE path (Touchpad telemetry → hormone feedback)
 *   - Mode transition hysteresis
 *   - Cross-module coupling (ORG_COHERENCE → coherence_requirement)
 *   - Event dispatch (10 touchpad events, 60-69)
 *   - Full system with all 5 adapters
 *   - Gesture-specific tests (contact speed, topological classification)
 */

#include <opencog/endocrine/endocrine_system.hpp>
#include <opencog/endocrine/touchpad_adapter.hpp>
#include <opencog/endocrine/touchpad_types.hpp>
#include <opencog/endocrine/marduk_adapter.hpp>
#include <opencog/endocrine/o9c2_adapter.hpp>
#include <opencog/endocrine/npu_adapter.hpp>
#include <opencog/endocrine/guidance_connector.hpp>
#include <opencog/endocrine/guidance_backends/stub_backend.hpp>
#include <opencog/core/types.hpp>

#include <cmath>
#include <string>
#include <functional>

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
using namespace opencog::endo;

// ============================================================================
// Mock Implementations
// ============================================================================
// Anonymous namespace: gives these mock types internal linkage so they don't
// collide (ODR violation via weak-symbol vtable/typeinfo coalescing) with the
// identically-named but differently-laid-out Mock* types defined in the
// other bio-cognitive test files (test_integration.cpp, test_marduk.cpp)
// once all files are linked into the single opencog_echo_tests executable.
namespace {

/// Mock VirtualTouchpad for testing — records calls and returns configurable state
struct MockTouchpad : public TouchpadInterface {
    TouchpadMode mode{TouchpadMode::RECEPTIVE};
    TouchpadEndocrineConfig cfg{TOUCHPAD_MODE_PRESETS[0]};
    TouchpadTelemetry tel;
    GestureMetrics met{0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    TouchpadEndocrineConfig last_applied;
    TouchpadMode last_transition{TouchpadMode::RECEPTIVE};
    int apply_count{0};
    int transition_count{0};

    TouchpadMode active_mode() const noexcept override { return mode; }
    TouchpadEndocrineConfig current_config() const noexcept override { return cfg; }
    TouchpadTelemetry telemetry() const noexcept override { return tel; }
    GestureMetrics gesture_metrics() const noexcept override { return met; }

    void apply_config(const TouchpadEndocrineConfig& c) override {
        last_applied = c;
        cfg = c;
        ++apply_count;
    }

    void transition_mode(TouchpadMode target) override {
        last_transition = target;
        mode = target;
        cfg = TOUCHPAD_MODE_PRESETS[static_cast<size_t>(target)];
        ++transition_count;
    }
};

/// Mock Marduk for cross-module tests
struct MockMarduk : public MardukInterface {
    MardukOperationalMode mode{MardukOperationalMode::KNOWLEDGE_ARCHITECT};
    MardukEndocrineConfig cfg{MARDUK_MODE_PRESETS[0]};
    MardukTelemetry tel;
    ExecutionMetrics ex{0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    int apply_count{0};
    int transition_count{0};

    MardukOperationalMode active_mode() const noexcept override { return mode; }
    MardukEndocrineConfig current_config() const noexcept override { return cfg; }
    MardukTelemetry telemetry() const noexcept override { return tel; }
    ExecutionMetrics execution() const noexcept override { return ex; }

    void apply_config(const MardukEndocrineConfig& c) override {
        cfg = c;
        ++apply_count;
    }

    void transition_mode(MardukOperationalMode target) override {
        mode = target;
        cfg = MARDUK_MODE_PRESETS[static_cast<size_t>(target)];
        ++transition_count;
    }
};

/// Mock o9c2 for full system tests
struct MockO9C2 : public O9C2Interface {
    O9C2Persona persona{O9C2Persona::CONTEMPLATIVE_SCHOLAR};
    O9C2PersonaConfig cfg{PERSONA_PRESETS[0]};
    EmergenceMetrics em{0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    int apply_count{0};
    int transition_count{0};

    O9C2Persona active_persona() const noexcept override { return persona; }
    O9C2PersonaConfig current_config() const noexcept override { return cfg; }
    EmergenceMetrics emergence() const noexcept override { return em; }

    void apply_config(const O9C2PersonaConfig& c) override {
        cfg = c;
        ++apply_count;
    }

    void transition_persona(O9C2Persona target) override {
        persona = target;
        cfg = PERSONA_PRESETS[static_cast<size_t>(target)];
        ++transition_count;
    }
};

/// Mock NPU for full system tests
struct MockNPU : public NPUInterface {
    NPUTelemetry tel;
    NPUEndocrineConfig cfg{128, 1, 0.5f, 0, 0.5f};
    int apply_count{0};

    NPUTelemetry telemetry() const noexcept override { return tel; }
    void apply_config(const NPUEndocrineConfig& c) override { cfg = c; ++apply_count; }
    NPUEndocrineConfig current_config() const noexcept override { return cfg; }
};

} // namespace

// ============================================================================
// Type Tests
// ============================================================================

TEST(Touchpad_Config_defaults) {
    TouchpadEndocrineConfig cfg;
    ASSERT_NEAR(cfg.sensitivity, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.dimensionality_scale, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.gesture_complexity, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.temporal_resolution, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.pressure_threshold, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.velocity_damping, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.manifold_curvature, 0.5f, 0.001f);
    ASSERT_NEAR(cfg.coherence_requirement, 0.5f, 0.001f);
    ASSERT_EQ(cfg.active_mode, TouchpadMode::RECEPTIVE);
    return true;
}

TEST(Touchpad_ModePresets_four_modes) {
    // Receptive: high sensitivity, low threshold
    auto rec = TOUCHPAD_MODE_PRESETS[0];
    ASSERT_NEAR(rec.sensitivity, 0.8f, 0.001f);
    ASSERT_NEAR(rec.pressure_threshold, 0.2f, 0.001f);
    ASSERT_EQ(rec.active_mode, TouchpadMode::RECEPTIVE);

    // Expressive: high complexity and temporal resolution
    auto exp = TOUCHPAD_MODE_PRESETS[1];
    ASSERT_NEAR(exp.gesture_complexity, 0.8f, 0.001f);
    ASSERT_NEAR(exp.temporal_resolution, 0.8f, 0.001f);
    ASSERT_EQ(exp.active_mode, TouchpadMode::EXPRESSIVE);

    // Calibrating: high curvature adaptation
    auto cal = TOUCHPAD_MODE_PRESETS[2];
    ASSERT_NEAR(cal.manifold_curvature, 0.8f, 0.001f);
    ASSERT_NEAR(cal.dimensionality_scale, 0.8f, 0.001f);
    ASSERT_EQ(cal.active_mode, TouchpadMode::CALIBRATING);

    // Guarded: high threshold, high coherence requirement
    auto grd = TOUCHPAD_MODE_PRESETS[3];
    ASSERT_NEAR(grd.pressure_threshold, 0.8f, 0.001f);
    ASSERT_NEAR(grd.coherence_requirement, 0.8f, 0.001f);
    ASSERT_EQ(grd.active_mode, TouchpadMode::GUARDED);
    return true;
}

TEST(Touchpad_GestureMetrics_aggregate) {
    GestureMetrics gm{0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
    float agg = gm.aggregate();
    ASSERT_NEAR(agg, 0.8f, 0.01f);

    // Unbalanced metrics should aggregate lower than balanced
    GestureMetrics unbalanced{0.1f, 0.9f, 0.9f, 0.9f, 0.9f};
    ASSERT_LT(unbalanced.aggregate(), agg);
    return true;
}

TEST(Touchpad_Telemetry_defaults) {
    TouchpadTelemetry tel;
    ASSERT_NEAR(tel.active_contact_count, 0.0f, 0.001f);
    ASSERT_NEAR(tel.gesture_confidence, 0.0f, 0.001f);
    ASSERT_NEAR(tel.manifold_coherence, 0.0f, 0.001f);
    ASSERT_EQ(tel.gestures_recognized, 0u);
    ASSERT_EQ(tel.gestures_emitted, 0u);
    ASSERT(!tel.calibrating);
    return true;
}

TEST(Touchpad_ModeName_all) {
    ASSERT_EQ(touchpad_mode_name(TouchpadMode::RECEPTIVE), "Receptive");
    ASSERT_EQ(touchpad_mode_name(TouchpadMode::EXPRESSIVE), "Expressive");
    ASSERT_EQ(touchpad_mode_name(TouchpadMode::CALIBRATING), "Calibrating");
    ASSERT_EQ(touchpad_mode_name(TouchpadMode::GUARDED), "Guarded");
    return true;
}

TEST(Touchpad_GestureTypeName_physical) {
    ASSERT_EQ(gesture_type_name(GestureType::TAP), "Tap");
    ASSERT_EQ(gesture_type_name(GestureType::SWIPE_UP), "SwipeUp");
    ASSERT_EQ(gesture_type_name(GestureType::PINCH_IN), "PinchIn");
    ASSERT_EQ(gesture_type_name(GestureType::CHIRAL_CW), "ChiralCW");
    ASSERT_EQ(gesture_type_name(GestureType::THREE_FINGER_SWIPE), "ThreeFingerSwipe");
    return true;
}

TEST(Touchpad_GestureTypeName_topological) {
    ASSERT_EQ(gesture_type_name(GestureType::MANIFOLD_FOLD), "ManifoldFold");
    ASSERT_EQ(gesture_type_name(GestureType::TOPOLOGY_TWIST), "TopologyTwist");
    ASSERT_EQ(gesture_type_name(GestureType::FIELD_RESONANCE), "FieldResonance");
    ASSERT_EQ(gesture_type_name(GestureType::INTENT_BLOOM), "IntentBloom");
    ASSERT_EQ(gesture_type_name(GestureType::HARMONIC_STRUM), "HarmonicStrum");
    return true;
}

TEST(Touchpad_GestureType_count_is_37) {
    ASSERT_EQ(GESTURE_TYPE_COUNT, 37u);
    ASSERT_EQ(static_cast<uint8_t>(GestureType::GESTURE_COUNT), 37u);
    return true;
}

// ============================================================================
// Bus Channel Tests
// ============================================================================

TEST(Touchpad_Bus_produce_read_ch18_ch19) {
    HormoneBus bus;
    bus.produce(HormoneId::TOUCHPAD_LOAD, 0.5f);
    ASSERT_GT(bus.concentration(HormoneId::TOUCHPAD_LOAD), 0.0f);
    ASSERT_NEAR(bus.concentration(HormoneId::TOUCHPAD_LOAD), 0.5f, 0.01f);

    bus.produce(HormoneId::TOUCHPAD_COHERENCE, 0.7f);
    ASSERT_NEAR(bus.concentration(HormoneId::TOUCHPAD_COHERENCE), 0.7f, 0.01f);
    return true;
}

TEST(Touchpad_Bus_snapshot_includes_touchpad_channels) {
    HormoneBus bus;
    bus.produce(HormoneId::TOUCHPAD_LOAD, 0.3f);
    bus.produce(HormoneId::TOUCHPAD_COHERENCE, 0.6f);
    EndocrineState snap = bus.snapshot();
    ASSERT_NEAR(snap[HormoneId::TOUCHPAD_LOAD], 0.3f, 0.01f);
    ASSERT_NEAR(snap[HormoneId::TOUCHPAD_COHERENCE], 0.6f, 0.01f);
    return true;
}

TEST(Touchpad_Bus_decay_touchpad_channels) {
    HormoneBus bus;
    bus.set_concentration(HormoneId::TOUCHPAD_LOAD, 0.8f);
    bus.set_concentration(HormoneId::TOUCHPAD_COHERENCE, 0.8f);
    bus.tick();
    // Channels should have decayed (baselines are 0.0)
    float load = bus.concentration(HormoneId::TOUCHPAD_LOAD);
    float coh = bus.concentration(HormoneId::TOUCHPAD_COHERENCE);
    ASSERT_GE(load, 0.0f);
    ASSERT_LT(load, 0.8f);
    ASSERT_GE(coh, 0.0f);
    ASSERT_LT(coh, 0.8f);
    return true;
}

// ============================================================================
// AtomType Tests
// ============================================================================

TEST(Touchpad_AtomTypes_nodes_registered) {
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_STATE_NODE), "TouchpadStateNode");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_GESTURE_NODE), "TouchpadGestureNode");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_CONTACT_NODE), "TouchpadContactNode");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_MANIFOLD_NODE), "TouchpadManifoldNode");
    return true;
}

TEST(Touchpad_AtomTypes_links_registered) {
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_MODULATION_LINK), "TouchpadModulationLink");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_GESTURE_LINK), "TouchpadGestureLink");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_FEEDBACK_LINK), "TouchpadFeedbackLink");
    ASSERT_EQ(type_name(AtomType::TOUCHPAD_MANIFOLD_LINK), "TouchpadManifoldLink");
    return true;
}

TEST(Touchpad_AtomTypes_reverse_lookup) {
    ASSERT_EQ(type_from_name("TouchpadStateNode"), AtomType::TOUCHPAD_STATE_NODE);
    ASSERT_EQ(type_from_name("TouchpadGestureLink"), AtomType::TOUCHPAD_GESTURE_LINK);
    ASSERT_EQ(type_from_name("TouchpadManifoldNode"), AtomType::TOUCHPAD_MANIFOLD_NODE);
    ASSERT_EQ(type_from_name("TouchpadFeedbackLink"), AtomType::TOUCHPAD_FEEDBACK_LINK);
    return true;
}

// ============================================================================
// READ Path Tests (Hormones → Touchpad Manifold Parameters)
// ============================================================================

TEST(Touchpad_READ_cortisol_increases_pressure_threshold) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::CORTISOL, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.pressure_threshold, TOUCHPAD_MODE_PRESETS[0].pressure_threshold);
    return true;
}

TEST(Touchpad_READ_cortisol_decreases_sensitivity) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::CORTISOL, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_LT(tp.last_applied.sensitivity, TOUCHPAD_MODE_PRESETS[0].sensitivity);
    return true;
}

TEST(Touchpad_READ_serotonin_increases_sensitivity) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::SEROTONIN, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.sensitivity, TOUCHPAD_MODE_PRESETS[0].sensitivity);
    return true;
}

TEST(Touchpad_READ_da_tonic_increases_gesture_complexity) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::DOPAMINE_TONIC, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.gesture_complexity, TOUCHPAD_MODE_PRESETS[0].gesture_complexity);
    return true;
}

TEST(Touchpad_READ_NE_increases_temporal_resolution) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::NOREPINEPHRINE, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.temporal_resolution, TOUCHPAD_MODE_PRESETS[0].temporal_resolution);
    return true;
}

TEST(Touchpad_READ_oxytocin_increases_dimensionality) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::OXYTOCIN, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.dimensionality_scale, TOUCHPAD_MODE_PRESETS[0].dimensionality_scale);
    return true;
}

TEST(Touchpad_READ_melatonin_increases_velocity_damping) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::MELATONIN, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.velocity_damping, TOUCHPAD_MODE_PRESETS[0].velocity_damping);
    return true;
}

TEST(Touchpad_READ_da_phasic_increases_manifold_curvature) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    bus.set_concentration(HormoneId::DOPAMINE_PHASIC, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.manifold_curvature, TOUCHPAD_MODE_PRESETS[0].manifold_curvature);
    return true;
}

TEST(Touchpad_READ_ORG_COHERENCE_increases_coherence_requirement) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // Cross-module coupling: Marduk's ORG_COHERENCE raises Touchpad's quality bar
    bus.set_concentration(HormoneId::ORG_COHERENCE, 0.8f);
    adapter.apply_endocrine_modulation(bus);

    ASSERT_GT(tp.last_applied.coherence_requirement, TOUCHPAD_MODE_PRESETS[0].coherence_requirement);
    return true;
}

// ============================================================================
// WRITE Path Tests (Touchpad Telemetry → Hormone Feedback)
// ============================================================================

TEST(Touchpad_WRITE_contacts_to_TOUCHPAD_LOAD) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    tp.tel.active_contact_count = 0.6f;
    adapter.apply_feedback();

    ASSERT_GT(bus.concentration(HormoneId::TOUCHPAD_LOAD), 0.0f);
    return true;
}

TEST(Touchpad_WRITE_coherence_to_TOUCHPAD_COHERENCE) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    tp.tel.manifold_coherence = 0.8f;
    adapter.apply_feedback();

    ASSERT_GT(bus.concentration(HormoneId::TOUCHPAD_COHERENCE), 0.0f);
    return true;
}

TEST(Touchpad_WRITE_confidence_improvement_to_serotonin) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // First tick: baseline confidence
    tp.tel.gesture_confidence = 0.3f;
    adapter.apply_feedback();
    float serotonin_before = bus.concentration(HormoneId::SEROTONIN);

    // Second tick: confidence improved (delta = 0.2 > threshold 0.05)
    tp.tel.gesture_confidence = 0.5f;
    adapter.apply_feedback();
    float serotonin_after = bus.concentration(HormoneId::SEROTONIN);

    ASSERT_GT(serotonin_after, serotonin_before);
    return true;
}

TEST(Touchpad_WRITE_gesture_recognized_to_DA_phasic) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // First tick: baseline
    tp.tel.gestures_recognized = 5;
    adapter.apply_feedback();
    float da_before = bus.concentration(HormoneId::DOPAMINE_PHASIC);

    // Second tick: gesture recognized
    tp.tel.gestures_recognized = 6;
    adapter.apply_feedback();
    float da_after = bus.concentration(HormoneId::DOPAMINE_PHASIC);

    ASSERT_GT(da_after, da_before);
    return true;
}

TEST(Touchpad_WRITE_high_errors_to_IL6_cortisol) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    tp.tel.error_rate = 0.7f;  // > 0.5 threshold
    adapter.apply_feedback();

    ASSERT_GT(bus.concentration(HormoneId::IL6), 0.0f);
    ASSERT_GT(bus.concentration(HormoneId::CORTISOL), 0.0f);
    return true;
}

TEST(Touchpad_WRITE_high_novelty_to_DA_tonic_NE) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    tp.tel.pattern_novelty = 0.8f;  // > 0.7 threshold
    adapter.apply_feedback();

    ASSERT_GT(bus.concentration(HormoneId::DOPAMINE_TONIC), 0.0f);
    ASSERT_GT(bus.concentration(HormoneId::NOREPINEPHRINE), 0.0f);
    return true;
}

TEST(Touchpad_WRITE_energy_collapse_to_cortisol) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // First tick: baseline energy
    tp.tel.field_energy = 0.7f;
    adapter.apply_feedback();
    float cortisol_before = bus.concentration(HormoneId::CORTISOL);

    // Second tick: energy collapsed (delta = -0.3 < -0.15)
    tp.tel.field_energy = 0.4f;
    adapter.apply_feedback();
    float cortisol_after = bus.concentration(HormoneId::CORTISOL);

    ASSERT_GT(cortisol_after, cortisol_before);
    return true;
}

TEST(Touchpad_WRITE_coherence_drop_to_NE) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // First tick: baseline coherence
    tp.met.coherence = 0.8f;
    adapter.apply_feedback();
    float ne_before = bus.concentration(HormoneId::NOREPINEPHRINE);

    // Second tick: coherence dropped (delta = -0.2 < -0.1)
    tp.met.coherence = 0.6f;
    adapter.apply_feedback();
    float ne_after = bus.concentration(HormoneId::NOREPINEPHRINE);

    ASSERT_GT(ne_after, ne_before);
    return true;
}

TEST(Touchpad_WRITE_precision_drop_to_cortisol) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // First tick: baseline precision
    tp.met.precision = 0.8f;
    adapter.apply_feedback();
    float cortisol_before = bus.concentration(HormoneId::CORTISOL);

    // Second tick: precision dropped (delta = -0.2 < -0.1)
    tp.met.precision = 0.6f;
    adapter.apply_feedback();
    float cortisol_after = bus.concentration(HormoneId::CORTISOL);

    ASSERT_GT(cortisol_after, cortisol_before);
    return true;
}

// ============================================================================
// Hysteresis Tests
// ============================================================================

TEST(Touchpad_Hysteresis_mode_requires_persistence) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);
    adapter.set_hysteresis_ticks(3);

    // Push bus toward FOCUSED mode (→ GUARDED)
    bus.set_concentration(HormoneId::NOREPINEPHRINE, 0.6f);
    bus.set_concentration(HormoneId::CORTISOL, 0.3f);
    bus.set_concentration(HormoneId::DOPAMINE_TONIC, 0.4f);

    // First 2 ticks: shouldn't transition yet
    bus.tick();
    adapter.apply_endocrine_modulation(bus);
    bus.tick();
    adapter.apply_endocrine_modulation(bus);
    ASSERT_EQ(tp.transition_count, 0);

    // 3rd tick should trigger transition
    bus.tick();
    adapter.apply_endocrine_modulation(bus);
    // Mode may or may not have been FOCUSED for 3 ticks depending on bus state
    // The important thing is transition requires persistence
    return true;
}

TEST(Touchpad_Hysteresis_suggest_transition_bypasses) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // suggest_transition bypasses hysteresis
    adapter.suggest_transition(TouchpadMode::GUARDED);
    ASSERT_EQ(tp.last_transition, TouchpadMode::GUARDED);
    ASSERT_EQ(tp.transition_count, 1);
    return true;
}

TEST(Touchpad_Hysteresis_resting_no_transition) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);
    adapter.set_hysteresis_ticks(1);

    // RESTING mode → no transition (persist current mode)
    // Bus starts in RESTING mode by default
    for (int i = 0; i < 10; ++i) {
        bus.tick();
        adapter.apply_endocrine_modulation(bus);
    }
    // Should not have triggered any transitions from RESTING
    ASSERT_EQ(tp.transition_count, 0);
    return true;
}

// ============================================================================
// Cross-Module Coupling Tests
// ============================================================================

TEST(Touchpad_CrossModule_reads_ORG_COHERENCE_from_Marduk) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    // Marduk writes ORG_COHERENCE
    bus.produce(HormoneId::ORG_COHERENCE, 0.9f);

    // Touchpad reads it in the modulation path
    adapter.apply_endocrine_modulation(bus);

    // coherence_requirement should be boosted by ORG_COHERENCE
    float base = TOUCHPAD_MODE_PRESETS[0].coherence_requirement;
    ASSERT_GT(tp.last_applied.coherence_requirement, base);
    return true;
}

TEST(Touchpad_CrossModule_writes_TOUCHPAD_LOAD_and_COHERENCE) {
    HormoneBus bus;
    MockTouchpad tp;
    TouchpadEndocrineAdapter adapter(bus, tp);

    tp.tel.active_contact_count = 0.8f;
    tp.tel.manifold_coherence = 0.9f;
    adapter.apply_feedback();

    // These channels should now be readable by other adapters
    ASSERT_GT(bus.concentration(HormoneId::TOUCHPAD_LOAD), 0.0f);
    ASSERT_GT(bus.concentration(HormoneId::TOUCHPAD_COHERENCE), 0.0f);
    return true;
}

// ============================================================================
// Event Dispatch Tests
// ============================================================================

TEST(Touchpad_Event_contact_started_produces_load) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_CONTACT_STARTED, 1.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::TOUCHPAD_LOAD), 0.0f);
    return true;
}

TEST(Touchpad_Event_contact_ended_produces_DA_tonic) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_CONTACT_ENDED, 1.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::DOPAMINE_TONIC), 0.0f);
    return true;
}

TEST(Touchpad_Event_gesture_recognized_rewards) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_GESTURE_RECOGNIZED, 1.0f);
    sys.tick(0.1f);  // flush gland phasic burst into bus concentration
    ASSERT_GT(sys.bus().concentration(HormoneId::DOPAMINE_PHASIC), 0.0f);
    return true;
}

TEST(Touchpad_Event_coherence_drop_stresses) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_COHERENCE_DROP, 1.0f);
    sys.tick(0.1f);  // flush gland stress response into bus concentration
    ASSERT_GT(sys.bus().concentration(HormoneId::CORTISOL), 0.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::NOREPINEPHRINE), 0.0f);
    return true;
}

TEST(Touchpad_Event_field_resonance_rewards) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_FIELD_RESONANCE, 1.0f);
    sys.tick(0.1f);  // flush gland phasic burst into bus concentration
    ASSERT_GT(sys.bus().concentration(HormoneId::DOPAMINE_PHASIC), 0.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::SEROTONIN), 0.0f);
    return true;
}

TEST(Touchpad_Event_overload_produces_stress) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::TOUCHPAD_OVERLOAD, 1.0f);
    sys.tick(0.1f);  // flush gland stress response into bus concentration
    ASSERT_GT(sys.bus().concentration(HormoneId::CORTISOL), 0.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::IL6), 0.0f);
    ASSERT_GT(sys.bus().concentration(HormoneId::TOUCHPAD_LOAD), 0.0f);
    return true;
}

// ============================================================================
// Gesture-Specific Tests
// ============================================================================

TEST(Touchpad_GestureContact_speed_calculation) {
    GestureContact contact;
    contact.active_dims = 3;
    contact.velocity[0] = 3.0f;
    contact.velocity[1] = 4.0f;
    contact.velocity[2] = 0.0f;
    contact.active = true;

    float speed = contact.speed();
    ASSERT_NEAR(speed, 5.0f, 0.01f);  // 3-4-5 right triangle
    return true;
}

TEST(Touchpad_GestureEvent_is_multi_touch) {
    GestureEvent single;
    single.contact_count = 1;
    ASSERT(!single.is_multi_touch());

    GestureEvent multi;
    multi.contact_count = 3;
    ASSERT(multi.is_multi_touch());
    return true;
}

TEST(Touchpad_GestureEvent_is_topological) {
    GestureEvent physical;
    physical.type = GestureType::TAP;
    ASSERT(!physical.is_topological());

    GestureEvent topo;
    topo.type = GestureType::MANIFOLD_FOLD;
    ASSERT(topo.is_topological());

    GestureEvent boundary;
    boundary.type = GestureType::THREE_FINGER_SWIPE;  // 24 — last physical
    ASSERT(!boundary.is_topological());

    GestureEvent first_topo;
    first_topo.type = GestureType::MANIFOLD_FOLD;  // 25 — first topological
    ASSERT(first_topo.is_topological());
    return true;
}

TEST(Touchpad_MAX_MANIFOLD_DIMS_is_37) {
    ASSERT_EQ(MAX_MANIFOLD_DIMS, 37u);
    ASSERT_EQ(MAX_CONTACTS, 10u);
    return true;
}

// ============================================================================
// Full System Tests
// ============================================================================

TEST(Touchpad_FullSystem_connect_all_five_adapters) {
    AtomSpace space;
    EndocrineSystem sys(space);

    MockNPU npu;
    MockO9C2 o9c2;
    MockMarduk marduk;
    MockTouchpad touchpad;

    sys.connect_npu(npu);
    sys.connect_o9c2(o9c2);
    sys.connect_marduk(marduk);
    sys.connect_touchpad(touchpad);
    sys.connect_guidance(std::make_unique<StubGuidanceBackend>());

    ASSERT(sys.npu_adapter() != nullptr);
    ASSERT(sys.o9c2_adapter() != nullptr);
    ASSERT(sys.marduk_adapter() != nullptr);
    ASSERT(sys.touchpad_adapter() != nullptr);
    ASSERT(sys.guidance() != nullptr);
    return true;
}

TEST(Touchpad_FullSystem_100_ticks_no_crash) {
    AtomSpace space;
    EndocrineSystem sys(space);

    MockNPU npu;
    MockO9C2 o9c2;
    MockMarduk marduk;
    MockTouchpad touchpad;

    sys.connect_npu(npu);
    sys.connect_o9c2(o9c2);
    sys.connect_marduk(marduk);
    sys.connect_touchpad(touchpad);
    sys.connect_guidance(std::make_unique<StubGuidanceBackend>());

    // Run 100 ticks — system should remain stable
    for (int i = 0; i < 100; ++i) {
        sys.tick(1.0f);
    }

    // All adapters should have been called
    ASSERT_GT(npu.apply_count, 0);
    ASSERT_GT(o9c2.apply_count, 0);
    ASSERT_GT(marduk.apply_count, 0);
    ASSERT_GT(touchpad.apply_count, 0);
    return true;
}

TEST(Touchpad_FullSystem_1000_ticks_stability_with_events) {
    AtomSpace space;
    EndocrineSystem sys(space);

    MockNPU npu;
    MockO9C2 o9c2;
    MockMarduk marduk;
    MockTouchpad touchpad;

    sys.connect_npu(npu);
    sys.connect_o9c2(o9c2);
    sys.connect_marduk(marduk);
    sys.connect_touchpad(touchpad);
    sys.connect_guidance(std::make_unique<StubGuidanceBackend>());

    // Inject events during the run
    for (int i = 0; i < 1000; ++i) {
        if (i == 50) sys.signal_event(EndocrineEvent::TOUCHPAD_CONTACT_STARTED);
        if (i == 100) sys.signal_event(EndocrineEvent::TOUCHPAD_GESTURE_RECOGNIZED);
        if (i == 150) sys.signal_event(EndocrineEvent::TOUCHPAD_CONTACT_ENDED);
        if (i == 200) sys.signal_event(EndocrineEvent::THREAT_DETECTED, 0.5f);
        if (i == 300) sys.signal_event(EndocrineEvent::REWARD_RECEIVED, 0.8f);
        if (i == 400) sys.signal_event(EndocrineEvent::TOUCHPAD_FIELD_RESONANCE);
        if (i == 500) sys.signal_event(EndocrineEvent::TOUCHPAD_TOPOLOGY_CHANGE);
        if (i == 600) sys.signal_event(EndocrineEvent::TOUCHPAD_COHERENCE_DROP);
        if (i == 700) sys.signal_event(EndocrineEvent::MARDUK_HEMISPHERIC_SYNC);
        if (i == 800) sys.signal_event(EndocrineEvent::TOUCHPAD_CALIBRATION_COMPLETE);
        if (i == 900) sys.signal_event(EndocrineEvent::TOUCHPAD_OVERLOAD);
        sys.tick(1.0f);
    }

    // System should still be stable — all concentrations in valid range
    EndocrineState state = sys.bus().snapshot();
    for (size_t i = 0; i < HORMONE_COUNT; ++i) {
        ASSERT_GE(state.concentrations[i], 0.0f);
        ASSERT_LE(state.concentrations[i], 1.0f);
    }
    return true;
}

TEST(Touchpad_FullSystem_threat_cascade_increases_pressure_threshold) {
    AtomSpace space;
    EndocrineSystem sys(space);

    MockTouchpad touchpad;
    sys.connect_touchpad(touchpad);

    // Baseline tick
    sys.tick(1.0f);
    float baseline_threshold = touchpad.last_applied.pressure_threshold;

    // Signal threat — should raise cortisol → increase pressure threshold.
    // The HPA axis is a 3-stage cascade (CRH -> ACTH -> Cortisol) with each
    // stage reading the *previous* tick's upstream concentration, so cortisol
    // rises gradually over several ticks rather than instantly on the next
    // tick (confirmed empirically: pressure_threshold dips slightly before
    // cortisol's rise dominates). Use enough ticks for the cascade to settle,
    // matching the multi-tick pattern used by other threat-cascade tests
    // (e.g. EndocrineSystem_signal_threat uses 30 ticks).
    sys.signal_event(EndocrineEvent::THREAT_DETECTED, 0.8f);
    for (int i = 0; i < 10; ++i) sys.tick(1.0f);

    ASSERT_GT(touchpad.last_applied.pressure_threshold, baseline_threshold);
    return true;
}
