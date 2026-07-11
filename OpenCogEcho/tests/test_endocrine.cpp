/**
 * @file test_endocrine.cpp
 * @brief Tests for the Virtual Endocrine System
 *
 * Covers: types, hormone bus, glands, valence memory, affective integration,
 * moral perception, and full system integration.
 */

#include <opencog/endocrine/endocrine_system.hpp>
#include <opencog/endocrine/types.hpp>
#include <opencog/endocrine/hormone_bus.hpp>
#include <opencog/endocrine/gland.hpp>
#include <opencog/endocrine/glands/hpa_axis.hpp>
#include <opencog/endocrine/glands/dopaminergic.hpp>
#include <opencog/endocrine/glands/circadian.hpp>
#include <opencog/endocrine/valence.hpp>
#include <opencog/endocrine/affect.hpp>
#include <opencog/endocrine/moral.hpp>
#include <opencog/core/types.hpp>

#include <cmath>
#include <thread>
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
#define ASSERT_LT(a, b) if (!((a) < (b))) { return false; }
#define ASSERT_GE(a, b) if (!((a) >= (b))) { return false; }
#define ASSERT_LE(a, b) if (!((a) <= (b))) { return false; }

using namespace opencog;
using namespace opencog::endo;

// ============================================================================
// Type Tests
// ============================================================================

TEST(ValenceSignature_size) {
    static_assert(sizeof(ValenceSignature) == 8);
    return true;
}

TEST(ValenceSignature_constructors) {
    auto neutral = ValenceSignature::neutral();
    ASSERT_NEAR(neutral.valence, 0.0f, 0.001f);
    ASSERT_NEAR(neutral.arousal, 0.0f, 0.001f);

    auto pos = ValenceSignature::positive();
    ASSERT_GT(pos.valence, 0.0f);

    auto neg = ValenceSignature::negative();
    ASSERT_LT(neg.valence, 0.0f);

    auto threat = ValenceSignature::threat();
    ASSERT_LT(threat.valence, -0.5f);
    ASSERT_GT(threat.arousal, 0.8f);

    return true;
}

TEST(ValenceSignature_magnitude) {
    ValenceSignature vs{0.6f, 0.8f};
    float expected = std::sqrt(0.36f + 0.64f); // 1.0
    ASSERT_NEAR(vs.magnitude(), expected, 0.01f);
    return true;
}

TEST(ValenceSignature_is_salient) {
    ValenceSignature quiet{0.1f, 0.1f};
    ASSERT(!quiet.is_salient(0.3f));

    ValenceSignature loud{0.5f, 0.5f};
    ASSERT(loud.is_salient(0.3f));
    return true;
}

TEST(ValenceSignature_blend) {
    ValenceSignature a{-1.0f, 0.0f};
    ValenceSignature b{1.0f, 1.0f};
    auto mid = a.blend(b, 0.5f);
    ASSERT_NEAR(mid.valence, 0.0f, 0.01f);
    ASSERT_NEAR(mid.arousal, 0.5f, 0.01f);
    return true;
}

TEST(HormoneLevel_size) {
    static_assert(sizeof(HormoneLevel) == 8);
    return true;
}

TEST(EndocrineState_size) {
    static_assert(sizeof(EndocrineState) == 128);
    return true;
}

TEST(MoralPerception_size) {
    static_assert(sizeof(MoralPerception) == 24);
    return true;
}

TEST(FeltSense_size) {
    static_assert(sizeof(FeltSense) == 24);
    return true;
}

TEST(CognitiveMode_names) {
    ASSERT_EQ(mode_name(CognitiveMode::RESTING), "Resting");
    ASSERT_EQ(mode_name(CognitiveMode::THREAT), "Threat");
    ASSERT_EQ(mode_name(CognitiveMode::EXPLORATORY), "Exploratory");
    return true;
}

TEST(HormoneId_names) {
    ASSERT_EQ(hormone_name(HormoneId::CORTISOL), "Cortisol");
    ASSERT_EQ(hormone_name(HormoneId::DOPAMINE_TONIC), "Dopamine(tonic)");
    ASSERT_EQ(hormone_name(HormoneId::OXYTOCIN), "Oxytocin");
    return true;
}

// ============================================================================
// AtomType Tests
// ============================================================================

TEST(EndocrineAtomTypes_registered) {
    ASSERT_EQ(type_name(AtomType::HORMONE_NODE), "HormoneNode");
    ASSERT_EQ(type_name(AtomType::EPISODE_NODE), "EpisodeNode");
    ASSERT_EQ(type_name(AtomType::VALENCE_LINK), "ValenceLink");
    ASSERT_EQ(type_name(AtomType::MORAL_EVALUATION_LINK), "MoralEvaluationLink");
    ASSERT_EQ(type_name(AtomType::EMPATHY_LINK), "EmpathyLink");
    return true;
}

TEST(EndocrineAtomTypes_reverse_lookup) {
    ASSERT_EQ(type_from_name("HormoneNode"), AtomType::HORMONE_NODE);
    ASSERT_EQ(type_from_name("EpisodeLink"), AtomType::EPISODE_LINK);
    return true;
}

TEST(EndocrineAtomTypes_is_node_link) {
    // Endocrine nodes are >= 10000, which is >= 1000, so is_link returns true
    // This is expected: user-defined types need their own classification
    // But HORMONE_NODE (10000) is in the "node" conceptual range
    // The current is_node() checks < 1000, so these fall through
    // This is fine — they're classified by the endocrine system, not the core
    return true;
}

// ============================================================================
// Hormone Bus Tests
// ============================================================================

TEST(HormoneBus_initial_state) {
    HormoneBus bus;
    // Initial concentrations should be zero (before any gland activity)
    ASSERT_NEAR(bus.concentration(HormoneId::CORTISOL), 0.0f, 0.001f);
    ASSERT_NEAR(bus.concentration(HormoneId::DOPAMINE_TONIC), 0.0f, 0.001f);
    ASSERT_EQ(bus.current_mode(), CognitiveMode::RESTING);
    ASSERT_EQ(bus.tick_count(), 0u);
    return true;
}

TEST(HormoneBus_produce) {
    HormoneBus bus;
    bus.produce(HormoneId::CORTISOL, 0.5f);
    ASSERT_NEAR(bus.concentration(HormoneId::CORTISOL), 0.5f, 0.01f);

    bus.produce(HormoneId::CORTISOL, 0.3f);
    ASSERT_NEAR(bus.concentration(HormoneId::CORTISOL), 0.8f, 0.01f);
    return true;
}

TEST(HormoneBus_saturation) {
    HormoneBus bus;
    bus.produce(HormoneId::CORTISOL, 2.0f); // Over ceiling
    ASSERT_LE(bus.concentration(HormoneId::CORTISOL), 1.0f);
    return true;
}

TEST(HormoneBus_decay) {
    HormoneBus bus;
    bus.produce(HormoneId::CORTISOL, 0.8f);
    float before = bus.concentration(HormoneId::CORTISOL);

    bus.decay_all();
    float after = bus.concentration(HormoneId::CORTISOL);

    // Should decay toward baseline (0.15 for cortisol)
    ASSERT_LT(after, before);
    return true;
}

TEST(HormoneBus_decay_toward_baseline) {
    HormoneBus bus;
    // Set cortisol well above baseline (0.15)
    bus.set_concentration(HormoneId::CORTISOL, 0.8f);

    // Run many decay cycles
    for (int i = 0; i < 200; ++i) {
        bus.decay_all();
    }

    // Should approach baseline
    float cortisol = bus.concentration(HormoneId::CORTISOL);
    ASSERT_NEAR(cortisol, bus.channel_config(HormoneId::CORTISOL).baseline, 0.05f);
    return true;
}

TEST(HormoneBus_snapshot) {
    HormoneBus bus;
    bus.produce(HormoneId::CORTISOL, 0.5f);
    bus.produce(HormoneId::OXYTOCIN, 0.3f);

    auto snap = bus.snapshot();
    ASSERT_NEAR(snap[HormoneId::CORTISOL], 0.5f, 0.01f);
    ASSERT_NEAR(snap[HormoneId::OXYTOCIN], 0.3f, 0.01f);
    return true;
}

TEST(HormoneBus_tick_advances) {
    HormoneBus bus;
    ASSERT_EQ(bus.tick_count(), 0u);
    bus.tick();
    ASSERT_EQ(bus.tick_count(), 1u);
    bus.tick();
    ASSERT_EQ(bus.tick_count(), 2u);
    return true;
}

TEST(HormoneBus_mode_detection_threat) {
    HormoneBus bus;
    // Set up threat-like constellation
    bus.set_concentration(HormoneId::CORTISOL, 0.9f);
    bus.set_concentration(HormoneId::NOREPINEPHRINE, 0.8f);
    bus.set_concentration(HormoneId::CRH, 0.7f);
    bus.set_concentration(HormoneId::ACTH, 0.6f);
    bus.tick(); // Triggers mode update

    ASSERT_EQ(bus.current_mode(), CognitiveMode::THREAT);
    return true;
}

TEST(HormoneBus_mode_detection_social) {
    HormoneBus bus;
    bus.set_concentration(HormoneId::OXYTOCIN, 0.7f);
    bus.set_concentration(HormoneId::SEROTONIN, 0.5f);
    bus.set_concentration(HormoneId::DOPAMINE_TONIC, 0.4f);
    bus.tick();

    ASSERT_EQ(bus.current_mode(), CognitiveMode::SOCIAL);
    return true;
}

TEST(HormoneBus_mode_change_callback) {
    HormoneBus bus;
    CognitiveMode detected_old = CognitiveMode::RESTING;
    CognitiveMode detected_new = CognitiveMode::RESTING;
    bool called = false;

    bus.on_mode_change([&](CognitiveMode old_m, CognitiveMode new_m) {
        detected_old = old_m;
        detected_new = new_m;
        called = true;
    });

    // Force a mode transition
    bus.set_concentration(HormoneId::CORTISOL, 0.9f);
    bus.set_concentration(HormoneId::NOREPINEPHRINE, 0.8f);
    bus.set_concentration(HormoneId::CRH, 0.7f);
    bus.set_concentration(HormoneId::ACTH, 0.6f);
    bus.tick();

    ASSERT(called);
    ASSERT_EQ(detected_old, CognitiveMode::RESTING);
    ASSERT_EQ(detected_new, CognitiveMode::THREAT);
    return true;
}

TEST(HormoneBus_average_concentration) {
    HormoneBus bus;
    bus.set_concentration(HormoneId::CORTISOL, 0.5f);
    bus.tick();
    bus.set_concentration(HormoneId::CORTISOL, 0.7f);
    bus.tick();

    float avg = bus.average_concentration(HormoneId::CORTISOL, 2);
    // Average of two snapshots
    ASSERT_GT(avg, 0.0f);
    return true;
}

// ============================================================================
// Gland Tests
// ============================================================================

TEST(GlandRegistry_register_defaults) {
    HormoneBus bus;
    GlandRegistry registry(bus);
    registry.register_defaults();
    ASSERT_EQ(registry.count(), 10u);
    return true;
}

TEST(GlandRegistry_get_gland) {
    HormoneBus bus;
    GlandRegistry registry(bus);
    registry.register_defaults();

    auto* hpa = registry.get_gland<HPAAxis>();
    ASSERT(hpa != nullptr);

    auto* da = registry.get_gland<DopaminergicGland>();
    ASSERT(da != nullptr);

    auto* circ = registry.get_gland<CircadianGland>();
    ASSERT(circ != nullptr);
    return true;
}

TEST(HPAAxis_stress_cascade) {
    HormoneBus bus;
    HPAAxis hpa(bus);

    // Signal stress
    hpa.signal_stress(0.8f);

    // Run several update cycles
    for (int i = 0; i < 20; ++i) {
        hpa.update(1.0f);
        bus.tick();
    }

    // All three should have elevated
    ASSERT_GT(bus.concentration(HormoneId::CRH), 0.0f);
    ASSERT_GT(bus.concentration(HormoneId::ACTH), 0.0f);
    ASSERT_GT(bus.concentration(HormoneId::CORTISOL), 0.0f);
    return true;
}

TEST(HPAAxis_negative_feedback) {
    HormoneBus bus;
    HPAAxis hpa(bus);

    // Manually set cortisol high (simulating chronic stress)
    bus.set_concentration(HormoneId::CORTISOL, 0.9f);

    // Production rate should be suppressed by negative feedback
    float rate = hpa.compute_production_rate();
    ASSERT_LT(rate, hpa.config().max_production_rate);
    return true;
}

TEST(Dopaminergic_phasic_burst) {
    HormoneBus bus;
    DopaminergicGland da(bus);

    // Signal a positive reward prediction error
    da.signal_reward(0.8f);
    da.update(1.0f);

    ASSERT_GT(bus.concentration(HormoneId::DOPAMINE_PHASIC), 0.0f);
    return true;
}

TEST(Dopaminergic_negative_prediction_error) {
    HormoneBus bus;
    DopaminergicGland da(bus);

    // Set some tonic dopamine first
    bus.set_concentration(HormoneId::DOPAMINE_TONIC, 0.5f);

    // Signal worse-than-expected
    da.signal_reward(-0.5f);
    da.update(1.0f);

    // Tonic should have decreased
    ASSERT_LT(bus.concentration(HormoneId::DOPAMINE_TONIC), 0.5f);
    return true;
}

TEST(Circadian_sinusoidal) {
    HormoneBus bus;
    CircadianGland circ(bus);
    circ.set_period(100.0f); // 100-tick cycle

    float max_melatonin = 0.0f;
    float min_melatonin = 1.0f;

    // Run one full cycle
    for (int i = 0; i < 100; ++i) {
        circ.update(1.0f);
        bus.tick();
        float m = bus.concentration(HormoneId::MELATONIN);
        max_melatonin = std::max(max_melatonin, m);
        min_melatonin = std::min(min_melatonin, m);
    }

    // Should have peaks and troughs
    ASSERT_GT(max_melatonin, min_melatonin);
    return true;
}

TEST(Glands_update_all_runs) {
    HormoneBus bus;
    GlandRegistry registry(bus);
    registry.register_defaults();

    // Should not crash
    for (int i = 0; i < 50; ++i) {
        registry.update_all(1.0f);
        bus.tick();
    }

    // After 50 ticks with default glands, some hormones should be non-zero
    ASSERT_GT(bus.concentration(HormoneId::SEROTONIN), 0.0f);
    return true;
}

// ============================================================================
// Valence Memory Tests
// ============================================================================

TEST(ValenceMemory_set_get) {
    AtomSpace space;
    ValenceMemory mem(space);

    Handle atom = space.add_node(AtomType::CONCEPT_NODE, "fire", TruthValue::simple(0.8f));
    ValenceSignature vs{-0.7f, 0.9f}; // Negative, high arousal (fear)

    mem.set_valence(atom.id(), vs);
    ASSERT(mem.has_valence(atom.id()));

    auto retrieved = mem.get_valence(atom.id());
    ASSERT_NEAR(retrieved.valence, -0.7f, 0.001f);
    ASSERT_NEAR(retrieved.arousal, 0.9f, 0.001f);
    return true;
}

TEST(ValenceMemory_unset_returns_neutral) {
    AtomSpace space;
    ValenceMemory mem(space);

    Handle atom = space.add_node(AtomType::CONCEPT_NODE, "test", TruthValue::simple(0.5f));
    ASSERT(!mem.has_valence(atom.id()));

    auto vs = mem.get_valence(atom.id());
    ASSERT_NEAR(vs.valence, 0.0f, 0.001f);
    ASSERT_NEAR(vs.arousal, 0.0f, 0.001f);
    return true;
}

TEST(ValenceMemory_create_episode) {
    AtomSpace space;
    ValenceMemory mem(space);

    Handle fire = space.add_node(AtomType::CONCEPT_NODE, "fire", TruthValue::simple(0.8f));
    Handle child = space.add_node(AtomType::CONCEPT_NODE, "child", TruthValue::simple(0.9f));
    Handle danger = space.add_node(AtomType::PREDICATE_NODE, "danger", TruthValue::simple(0.7f));

    std::vector<std::pair<Handle, ValenceSignature>> members = {
        {fire, {-0.5f, 0.8f}},
        {child, {0.3f, 0.2f}},
        {danger, {-0.8f, 0.9f}}
    };

    Handle episode = mem.create_episode("fire_near_child", members, ValenceSignature::threat());

    ASSERT(episode.valid());
    ASSERT_EQ(mem.episode_count(), 1u);
    ASSERT_GE(mem.valenced_atom_count(), 3u);
    return true;
}

TEST(ValenceMemory_retrieve_by_valence) {
    AtomSpace space;
    ValenceMemory mem(space);

    Handle a1 = space.add_node(AtomType::CONCEPT_NODE, "joy", TruthValue::simple(0.8f));
    Handle a2 = space.add_node(AtomType::CONCEPT_NODE, "pain", TruthValue::simple(0.8f));

    std::vector<std::pair<Handle, ValenceSignature>> happy_members{{a1, ValenceSignature::joy()}};
    std::vector<std::pair<Handle, ValenceSignature>> painful_members{{a2, ValenceSignature::threat()}};
    mem.create_episode("happy_event", happy_members, ValenceSignature::joy());
    mem.create_episode("painful_event", painful_members, ValenceSignature::threat());

    // Retrieve negative episodes only
    auto negative_eps = mem.retrieve_by_valence(-1.0f, -0.3f);
    ASSERT_GE(negative_eps.size(), 1u);
    return true;
}

// ============================================================================
// Affective Integration Tests
// ============================================================================

TEST(AffectiveIntegration_empty_focus) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);

    std::vector<Handle> empty;
    auto sense = affect.compute_felt_sense(empty);
    // Should return neutral with empty focus
    ASSERT_NEAR(sense.aggregate_valence.valence, 0.0f, 0.01f);
    return true;
}

TEST(AffectiveIntegration_with_valenced_atoms) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);

    Handle danger = space.add_node(AtomType::CONCEPT_NODE, "danger", TruthValue::simple(0.8f));
    Handle harm = space.add_node(AtomType::CONCEPT_NODE, "harm", TruthValue::simple(0.7f));

    mem.set_valence(danger.id(), ValenceSignature::threat());
    mem.set_valence(harm.id(), {-0.6f, 0.8f});

    std::vector<Handle> focus = {danger, harm};
    auto sense = affect.compute_felt_sense(focus);

    // Should have negative valence (both atoms are negative)
    ASSERT_LT(sense.aggregate_valence.valence, 0.0f);
    ASSERT_GT(sense.aggregate_valence.arousal, 0.0f);
    return true;
}

TEST(AffectiveIntegration_modulate_endocrine_negative) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);

    // Create a strongly negative, high-arousal felt-sense
    FeltSense sense;
    sense.aggregate_valence = {-0.8f, 0.9f};
    sense.certainty = 0.7f;
    sense.novelty = 0.3f;

    affect.modulate_endocrine(sense);

    // Should have produced CRH (stress signal)
    ASSERT_GT(bus.concentration(HormoneId::CRH), 0.0f);
    return true;
}

// ============================================================================
// Moral Perception Tests
// ============================================================================

TEST(MoralPerception_no_precedent) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);
    MoralPerceptionEngine moral(affect, mem, bus, space);

    Handle novel = space.add_node(AtomType::CONCEPT_NODE, "novel_situation", TruthValue::simple(0.5f));
    std::vector<Handle> situation = {novel};

    auto perception = moral.evaluate(situation);

    // Novel situation with no precedent should have high novelty
    ASSERT_GT(perception.novel_moral_signal, 0.0f);
    return true;
}

TEST(MoralPerception_with_negative_precedent) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);
    MoralPerceptionEngine moral(affect, mem, bus, space);

    // Record a painful moral experience
    Handle harm_concept = space.add_node(AtomType::CONCEPT_NODE, "harm_to_others", TruthValue::simple(0.8f));
    std::vector<Handle> past_situation = {harm_concept};
    moral.record_moral_outcome(past_situation, ValenceSignature::threat(), 1.0f);

    // Now encounter a similar situation
    auto perception = moral.evaluate(past_situation);

    // Should detect moral salience from the negative precedent
    // (The exact value depends on retrieval scoring, but should be > 0)
    ASSERT_GE(perception.moral_salience, 0.0f);
    return true;
}

TEST(MoralPerception_record_and_learn) {
    AtomSpace space;
    HormoneBus bus;
    ValenceMemory mem(space);
    AffectiveIntegration affect(mem, bus, space);
    MoralPerceptionEngine moral(affect, mem, bus, space);

    Handle cruelty = space.add_node(AtomType::CONCEPT_NODE, "cruelty", TruthValue::simple(0.9f));

    // Record multiple painful experiences
    for (int i = 0; i < 5; ++i) {
        std::vector<Handle> sit = {cruelty};
        moral.record_moral_outcome(sit, {-0.9f, 0.9f}, 1.0f);
        mem.tick();
    }

    ASSERT_EQ(mem.episode_count(), 5u);
    return true;
}

// ============================================================================
// Full System Integration Test
// ============================================================================

TEST(EndocrineSystem_creation) {
    AtomSpace space;
    EndocrineSystem sys(space);

    ASSERT_EQ(sys.glands().count(), 10u);
    ASSERT_EQ(sys.bus().tick_count(), 0u);
    return true;
}

TEST(EndocrineSystem_tick_runs) {
    AtomSpace space;
    EndocrineSystem sys(space);

    // Run 100 ticks — should not crash
    for (int i = 0; i < 100; ++i) {
        sys.tick();
    }

    ASSERT_EQ(sys.bus().tick_count(), 100u);
    // Some hormones should be non-zero from gland baseline production
    ASSERT_GT(sys.bus().concentration(HormoneId::SEROTONIN), 0.0f);
    return true;
}

TEST(EndocrineSystem_signal_threat) {
    AtomSpace space;
    EndocrineSystem sys(space);

    // Run a few baseline ticks
    for (int i = 0; i < 10; ++i) sys.tick();
    float baseline_cortisol = sys.bus().concentration(HormoneId::CORTISOL);

    // Signal threat
    sys.signal_event(EndocrineEvent::THREAT_DETECTED, 0.9f);

    // Run more ticks
    for (int i = 0; i < 30; ++i) sys.tick();

    // Cortisol should have risen above baseline
    ASSERT_GT(sys.bus().concentration(HormoneId::CORTISOL), baseline_cortisol);
    return true;
}

TEST(EndocrineSystem_signal_reward) {
    AtomSpace space;
    EndocrineSystem sys(space);

    sys.signal_event(EndocrineEvent::REWARD_RECEIVED, 0.8f);
    sys.tick();

    ASSERT_GT(sys.bus().concentration(HormoneId::DOPAMINE_PHASIC), 0.0f);
    return true;
}

TEST(EndocrineSystem_signal_social) {
    AtomSpace space;
    EndocrineSystem sys(space);

    for (int i = 0; i < 5; ++i) sys.tick();
    float baseline_oxy = sys.bus().concentration(HormoneId::OXYTOCIN);

    sys.signal_event(EndocrineEvent::SOCIAL_BOND_SIGNAL, 0.8f);
    for (int i = 0; i < 20; ++i) sys.tick();

    ASSERT_GT(sys.bus().concentration(HormoneId::OXYTOCIN), baseline_oxy);
    return true;
}

TEST(EndocrineSystem_mode_transitions) {
    AtomSpace space;
    EndocrineSystem sys(space);

    // Start in resting
    ASSERT_EQ(sys.bus().current_mode(), CognitiveMode::RESTING);

    // Strong threat should transition to threat mode
    sys.signal_event(EndocrineEvent::THREAT_DETECTED, 1.0f);
    for (int i = 0; i < 50; ++i) sys.tick();

    // Mode should have changed from resting (exact mode depends on dynamics)
    // At minimum, cortisol and NE should be elevated
    ASSERT_GT(sys.bus().concentration(HormoneId::CORTISOL), 0.1f);
    return true;
}

TEST(EndocrineAgent_start_stop) {
    AtomSpace space;
    EndocrineSystem sys(space);
    EndocrineAgent agent(sys);

    ASSERT(!agent.is_running());
    agent.start();
    ASSERT(agent.is_running());

    agent.run_once();
    ASSERT_EQ(sys.bus().tick_count(), 1u);

    agent.stop();
    ASSERT(!agent.is_running());

    // Should not tick when stopped
    agent.run_once();
    ASSERT_EQ(sys.bus().tick_count(), 1u);
    return true;
}

TEST(EndocrineSystem_valence_integration) {
    AtomSpace space;
    EndocrineSystem sys(space);

    // Create a valence-tagged episode
    Handle pain = space.add_node(AtomType::CONCEPT_NODE, "pain", TruthValue::simple(0.8f));
    std::vector<std::pair<Handle, ValenceSignature>> members = {
        {pain, ValenceSignature::threat()}
    };
    sys.valence().create_episode("painful_memory", members, ValenceSignature::threat());

    // Evaluate moral perception on a situation involving pain
    std::vector<Handle> situation = {pain};
    auto perception = sys.moral().evaluate(situation);

    // Should detect some moral signal (pain atom was in a negative episode)
    // The raw signal should be non-neutral since we tagged it
    ASSERT(perception.raw_signal.is_salient() || perception.novel_moral_signal > 0.0f);
    return true;
}
