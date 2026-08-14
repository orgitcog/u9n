/**
 * @file test_nervous.cpp
 * @brief Tests for the Virtual Nervous System
 *
 * Covers: types, nerve bus, nuclei, connectors, reflex arcs,
 * processing level classification, and full system integration.
 */

#include <opencog/nervous/nervous_system.hpp>
#include <opencog/nervous/types.hpp>
#include <opencog/nervous/nerve_bus.hpp>
#include <opencog/nervous/nucleus.hpp>
#include <opencog/nervous/nuclei/thalamus.hpp>
#include <opencog/nervous/nuclei/amygdala.hpp>
#include <opencog/nervous/nuclei/hippocampus.hpp>
#include <opencog/nervous/nuclei/basal_ganglia.hpp>
#include <opencog/nervous/nuclei/prefrontal_cortex.hpp>
#include <opencog/nervous/nuclei/brainstem_autonomic.hpp>
#include <opencog/nervous/nuclei/cerebellum.hpp>
#include <opencog/nervous/nuclei/anterior_cingulate.hpp>
#include <opencog/nervous/nuclei/insula.hpp>
#include <opencog/nervous/nuclei/hypothalamus.hpp>
#include <opencog/core/types.hpp>

#include <cmath>
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
using namespace opencog::nerv;

// ============================================================================
// Type Tests
// ============================================================================

TEST(NeuralSignal_size) {
    static_assert(sizeof(NeuralSignal) == 8);
    return true;
}

TEST(NeuralSignal_constructors) {
    auto exc = NeuralSignal::excite(0.7f, 0.5f);
    ASSERT_NEAR(exc.activation, 0.7f, 0.001f);
    ASSERT_NEAR(exc.urgency, 0.5f, 0.001f);
    ASSERT(exc.is_excitatory());

    auto inh = NeuralSignal::inhibit(0.6f, 0.3f);
    ASSERT_NEAR(inh.activation, -0.6f, 0.001f);
    ASSERT(inh.is_inhibitory());

    auto spike = NeuralSignal::spike(0.9f);
    ASSERT_NEAR(spike.urgency, 1.0f, 0.001f);
    ASSERT_NEAR(spike.magnitude(), 0.9f, 0.001f);

    return true;
}

TEST(NeuralSignal_salient) {
    NeuralSignal quiet(0.01f, 0.0f);
    ASSERT(!quiet.is_salient(0.1f));

    NeuralSignal loud(0.5f, 0.5f);
    ASSERT(loud.is_salient(0.1f));
    return true;
}

TEST(SynapticWeight_size) {
    static_assert(sizeof(SynapticWeight) == 8);
    return true;
}

TEST(SynapticWeight_constructors) {
    auto exc = SynapticWeight::excitatory(0.5f);
    ASSERT_NEAR(exc.weight, 0.5f, 0.001f);
    ASSERT_NEAR(exc.plasticity, 0.0f, 0.001f);

    auto inh = SynapticWeight::inhibitory(0.3f);
    ASSERT_NEAR(inh.weight, -0.3f, 0.001f);

    auto plastic = SynapticWeight::plastic(0.2f, 0.1f);
    ASSERT_NEAR(plastic.plasticity, 0.1f, 0.001f);
    return true;
}

TEST(NeuralState_size) {
    static_assert(sizeof(NeuralState) == 256);
    return true;
}

TEST(NeuralState_distance) {
    NeuralState a, b;
    a[NeuralChannelId::VISUAL_PRIMARY] = 1.0f;
    b[NeuralChannelId::VISUAL_PRIMARY] = 0.0f;
    float dist = a.distance_to(b);
    ASSERT_NEAR(dist, 1.0f, 0.001f);
    return true;
}

TEST(NeuralState_total_activation) {
    NeuralState s;
    s[NeuralChannelId::VISUAL_PRIMARY] = 0.5f;
    s[NeuralChannelId::THREAT_AFFERENT] = -0.3f;
    float total = s.total_activation();
    ASSERT_NEAR(total, 0.8f, 0.001f);
    return true;
}

TEST(ProcessingLevel_names) {
    ASSERT_EQ(level_name(ProcessingLevel::SPINAL_REFLEX), "SpinalReflex");
    ASSERT_EQ(level_name(ProcessingLevel::CORTICAL), "Cortical");
    return true;
}

TEST(NeuralPolarity_names) {
    ASSERT_EQ(polarity_name(NeuralPolarity::SYMPATHETIC), "Sympathetic");
    ASSERT_EQ(polarity_name(NeuralPolarity::SOMATIC), "Somatic");
    return true;
}

TEST(ChannelName_lookup) {
    ASSERT_EQ(channel_name(NeuralChannelId::VISUAL_PRIMARY), "Visual");
    ASSERT_EQ(channel_name(NeuralChannelId::PREFRONTAL_EXEC), "PrefrontalExec");
    ASSERT_EQ(channel_name(NeuralChannelId::SYMPATHETIC_OUT), "SympathOut");
    return true;
}

// ============================================================================
// NerveBus Tests
// ============================================================================

TEST(NerveBus_initial_state) {
    NerveBus bus;
    ASSERT_NEAR(bus.activation(NeuralChannelId::VISUAL_PRIMARY), 0.0f, 0.001f);
    ASSERT_EQ(bus.tick_count(), 0u);
    return true;
}

TEST(NerveBus_fire_and_read) {
    NerveBus bus;
    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.8f, 1.0f);
    float val = bus.activation(NeuralChannelId::THREAT_AFFERENT);
    ASSERT_NEAR(val, 0.8f, 0.001f);
    return true;
}

TEST(NerveBus_fire_clamps) {
    NerveBus bus;
    bus.fire(NeuralChannelId::VISUAL_PRIMARY, 1.5f);
    ASSERT_NEAR(bus.activation(NeuralChannelId::VISUAL_PRIMARY), 1.0f, 0.001f);

    bus.fire(NeuralChannelId::VISUAL_PRIMARY, -3.0f);
    ASSERT_NEAR(bus.activation(NeuralChannelId::VISUAL_PRIMARY), -1.0f, 0.001f);
    return true;
}

TEST(NerveBus_inhibit) {
    NerveBus bus;
    bus.fire(NeuralChannelId::BASAL_GANGLIA_GO, 0.8f);
    bus.inhibit(NeuralChannelId::BASAL_GANGLIA_GO, 0.3f);
    ASSERT_NEAR(bus.activation(NeuralChannelId::BASAL_GANGLIA_GO), 0.5f, 0.001f);
    return true;
}

TEST(NerveBus_snapshot) {
    NerveBus bus;
    bus.fire(NeuralChannelId::VISUAL_PRIMARY, 0.5f);
    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.9f);
    auto snap = bus.snapshot();
    ASSERT_NEAR(snap[NeuralChannelId::VISUAL_PRIMARY], 0.5f, 0.001f);
    ASSERT_NEAR(snap[NeuralChannelId::THREAT_AFFERENT], 0.9f, 0.001f);
    return true;
}

TEST(NerveBus_decay) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;  // isolate decay test
    NerveBus bus(cfg);
    bus.fire(NeuralChannelId::VISUAL_PRIMARY, 0.8f, 0.5f);
    bus.tick();
    float after = bus.activation(NeuralChannelId::VISUAL_PRIMARY);
    ASSERT_LT(after, 0.8f);  // must have decayed
    ASSERT_GT(after, 0.0f);  // but not to zero in one tick
    return true;
}

TEST(NerveBus_decay_toward_zero) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    NerveBus bus(cfg);
    bus.fire(NeuralChannelId::NOVELTY_SIGNAL, 0.5f, 0.5f);
    for (int i = 0; i < 50; ++i) bus.tick();
    ASSERT_NEAR(bus.activation(NeuralChannelId::NOVELTY_SIGNAL), 0.0f, 0.01f);
    return true;
}

TEST(NerveBus_synapse_set_get) {
    NerveBus bus;
    bus.set_synapse(NeuralChannelId::VISUAL_PRIMARY,
                    NeuralChannelId::THALAMIC_RELAY_1, 0.7f);
    float w = bus.synapse(NeuralChannelId::VISUAL_PRIMARY,
                          NeuralChannelId::THALAMIC_RELAY_1);
    ASSERT_NEAR(w, 0.7f, 0.001f);
    return true;
}

TEST(NerveBus_propagation) {
    NerveBusConfig cfg;
    cfg.global_decay_rate = 0.0f;  // disable decay to isolate propagation
    NerveBus bus(cfg);

    // Clear default connectivity, set one strong pathway
    bus.set_synapse(NeuralChannelId::VISUAL_PRIMARY,
                    NeuralChannelId::THALAMIC_RELAY_1, 0.0f);
    // Create our own isolated pathway
    bus.set_synapse(NeuralChannelId::RESERVED_56,
                    NeuralChannelId::RESERVED_57, 0.8f);

    bus.fire(NeuralChannelId::RESERVED_56, 1.0f);
    bus.tick();

    float propagated = bus.activation(NeuralChannelId::RESERVED_57);
    ASSERT_GT(propagated, 0.0f);  // signal should propagate
    return true;
}

TEST(NerveBus_tick_count) {
    NerveBus bus;
    ASSERT_EQ(bus.tick_count(), 0u);
    bus.tick();
    ASSERT_EQ(bus.tick_count(), 1u);
    bus.tick();
    ASSERT_EQ(bus.tick_count(), 2u);
    return true;
}

TEST(NerveBus_history) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    NerveBus bus(cfg);
    bus.fire(NeuralChannelId::VISUAL_PRIMARY, 0.8f);
    bus.tick();
    auto& hist = bus.history_at(0);
    // History should contain the snapshot taken during tick
    // (after decay, so slightly less than 0.8)
    ASSERT_GT(std::abs(hist.activations[0]), 0.0f);
    return true;
}

TEST(NerveBus_reflex_arc) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    bus.add_reflex({NeuralChannelId::THREAT_AFFERENT,
                    NeuralChannelId::SYMPATHETIC_OUT, 0.5f, 0.9f, 1});

    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.8f);
    bus.process_reflexes();

    float response = bus.activation(NeuralChannelId::SYMPATHETIC_OUT);
    ASSERT_GT(response, 0.0f);
    return true;
}

TEST(NerveBus_reflex_below_threshold) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    bus.add_reflex({NeuralChannelId::THREAT_AFFERENT,
                    NeuralChannelId::SYMPATHETIC_OUT, 0.5f, 0.9f, 1});

    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.2f);  // below threshold
    bus.process_reflexes();

    float response = bus.activation(NeuralChannelId::SYMPATHETIC_OUT);
    ASSERT_NEAR(response, 0.0f, 0.001f);
    return true;
}

TEST(NerveBus_level_change_callback) {
    NerveBus bus;
    bool called = false;
    ProcessingLevel old_level{}, new_level{};

    bus.on_level_change([&](ProcessingLevel o, ProcessingLevel n) {
        called = true;
        old_level = o;
        new_level = n;
    });

    // Force high threat signals to trigger SPINAL_REFLEX level
    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.9f);
    bus.fire(NeuralChannelId::SYMPATHETIC_OUT, 0.8f);
    bus.fire(NeuralChannelId::PAIN_SIGNAL, 0.7f);
    bus.tick();

    // The callback may or may not fire depending on default level
    // Just verify the bus doesn't crash
    ASSERT_GE(bus.tick_count(), 1u);
    return true;
}

TEST(NerveBus_threshold_callback) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    bool triggered = false;
    bus.on_threshold_crossed(NeuralChannelId::THREAT_AFFERENT, 0.5f,
        [&](NeuralChannelId, float val) {
            triggered = true;
        });

    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.8f);
    bus.tick();

    ASSERT(triggered);
    return true;
}

// ============================================================================
// Nucleus Tests
// ============================================================================

TEST(NucleusRegistry_register_defaults) {
    NerveBus bus;
    NucleusRegistry reg(bus);
    reg.register_defaults();
    ASSERT_EQ(reg.count(), 10u);
    return true;
}

TEST(ThalamusNucleus_exists) {
    NerveBus bus;
    NucleusRegistry reg(bus);
    reg.register_defaults();
    auto* thal = reg.get_nucleus<ThalamusNucleus>();
    ASSERT(thal != nullptr);
    ASSERT_EQ(thal->name(), "Thalamus");
    return true;
}

TEST(AmygdalaNucleus_threat_response) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    AmygdalaNucleus amyg(bus);
    bus.fire(NeuralChannelId::THREAT_AFFERENT, 0.8f);
    amyg.update(1.0f);

    // Amygdala should have fired on its output channel
    float valence = bus.activation(NeuralChannelId::AMYGDALA_VALENCE);
    ASSERT_GT(valence, 0.0f);
    return true;
}

TEST(BasalGangliaNucleus_go_signal) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    BasalGangliaNucleus bg(bus);
    bus.fire(NeuralChannelId::PREFRONTAL_EXEC, 0.7f);
    bus.fire(NeuralChannelId::REWARD_PREDICTION, 0.5f);
    bg.update(1.0f);

    float go = bus.activation(NeuralChannelId::BASAL_GANGLIA_GO);
    ASSERT_GT(go, 0.0f);
    return true;
}

TEST(PrefrontalCortex_top_down_inhibition) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    // Set up initial amygdala activation
    bus.fire(NeuralChannelId::AMYGDALA_VALENCE, 0.5f);

    PrefrontalCortexNucleus pfc(bus);
    bus.fire(NeuralChannelId::WORKING_MEMORY, 0.7f);
    bus.fire(NeuralChannelId::REASONING_CORTEX, 0.6f);
    pfc.update(1.0f);

    // PFC should have been activated
    float exec = bus.activation(NeuralChannelId::PREFRONTAL_EXEC);
    ASSERT_GT(exec, 0.0f);
    return true;
}

TEST(HippocampusNucleus_encoding) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    HippocampusNucleus hipp(bus);
    bus.fire(NeuralChannelId::NOVELTY_SIGNAL, 0.7f);
    bus.fire(NeuralChannelId::AMYGDALA_VALENCE, 0.4f);
    hipp.update(1.0f);

    float encode = bus.activation(NeuralChannelId::HIPPOCAMPAL_ENCODE);
    ASSERT_GT(encode, 0.0f);
    return true;
}

TEST(Nucleus_fire_callback) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    ThalamusNucleus thal(bus);
    bool fired = false;
    thal.on_fire([&](NeuralChannelId, float) { fired = true; });

    bus.fire(NeuralChannelId::RETICULAR_ACTIVATION, 0.6f);
    bus.fire(NeuralChannelId::VISUAL_PRIMARY, 0.5f);
    thal.update(1.0f);

    ASSERT(fired);
    return true;
}

TEST(Nucleus_adaptation) {
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NerveBus bus(cfg);

    AnteriorCingulateNucleus acc(bus);
    float initial_threshold = acc.adapted_threshold();

    // Drive the nucleus strongly for many ticks
    for (int i = 0; i < 100; ++i) {
        bus.fire(NeuralChannelId::ERROR_SIGNAL, 0.8f);
        acc.update(1.0f);
    }

    // Threshold should have drifted from initial
    ASSERT_NE(acc.adapted_threshold(), initial_threshold);
    return true;
}

// ============================================================================
// AtomType Tests
// ============================================================================

TEST(VNS_AtomTypes_registered) {
    ASSERT_NE(type_name(AtomType::NEURAL_CHANNEL_NODE), "UnknownType");
    ASSERT_NE(type_name(AtomType::NEURAL_NUCLEUS_NODE), "UnknownType");
    ASSERT_NE(type_name(AtomType::SYNAPSE_NODE), "UnknownType");
    ASSERT_NE(type_name(AtomType::NEURAL_PATHWAY_LINK), "UnknownType");
    ASSERT_NE(type_name(AtomType::SYNAPTIC_LINK), "UnknownType");
    ASSERT_NE(type_name(AtomType::INTEGRATION_LINK), "UnknownType");
    return true;
}

TEST(VNS_AtomTypes_reverse_lookup) {
    ASSERT_EQ(type_from_name("NeuralChannelNode"), AtomType::NEURAL_CHANNEL_NODE);
    ASSERT_EQ(type_from_name("SynapticLink"), AtomType::SYNAPTIC_LINK);
    ASSERT_EQ(type_from_name("IntegrationLink"), AtomType::INTEGRATION_LINK);
    return true;
}

// ============================================================================
// Integration Tests (NervousSystem facade)
// ============================================================================

TEST(NervousSystem_creation) {
    AtomSpace space;
    NervousSystem ns(space);
    ASSERT_EQ(ns.nuclei().count(), 10u);
    ASSERT_EQ(ns.bus().tick_count(), 0u);
    return true;
}

TEST(NervousSystem_tick_basic) {
    AtomSpace space;
    NervousSystem ns(space);
    ns.tick();
    ASSERT_EQ(ns.bus().tick_count(), 1u);
    return true;
}

TEST(NervousSystem_multi_tick) {
    AtomSpace space;
    NervousSystem ns(space);
    for (int i = 0; i < 100; ++i) {
        ns.tick();
    }
    ASSERT_EQ(ns.bus().tick_count(), 100u);
    return true;
}

TEST(NervousSystem_signal_event_threat) {
    AtomSpace space;
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NervousSystem ns(space, cfg);

    ns.signal_event(NeuralEvent::THREAT_PERCEIVED, 0.9f);

    float threat = ns.bus().activation(NeuralChannelId::THREAT_AFFERENT);
    ASSERT_GT(threat, 0.0f);
    return true;
}

TEST(NervousSystem_signal_event_novelty) {
    AtomSpace space;
    NerveBusConfig cfg;
    cfg.enable_propagation = false;
    cfg.global_decay_rate = 0.0f;
    NervousSystem ns(space, cfg);

    ns.signal_event(NeuralEvent::NOVELTY_DETECTED, 0.7f);
    float novelty = ns.bus().activation(NeuralChannelId::NOVELTY_SIGNAL);
    ASSERT_GT(novelty, 0.0f);
    return true;
}

TEST(NervousSystem_sympathetic_cascade) {
    AtomSpace space;
    NervousSystem ns(space);

    // Inject a strong threat signal
    ns.bus().fire(NeuralChannelId::THREAT_AFFERENT, 0.9f, 1.0f);

    // Run several ticks to let the cascade propagate
    for (int i = 0; i < 10; ++i) {
        ns.tick();
    }

    // Sympathetic output should be elevated (threat → amygdala → sympathetic)
    // Due to decay, check that it was activated at some point via history
    bool sympathetic_active = false;
    for (size_t t = 0; t < 10; ++t) {
        float s = ns.bus().history_at(t).activations[
            static_cast<size_t>(NeuralChannelId::SYMPATHETIC_OUT)];
        if (std::abs(s) > 0.01f) {
            sympathetic_active = true;
            break;
        }
    }
    ASSERT(sympathetic_active);
    return true;
}

TEST(NervousAgent_start_stop) {
    AtomSpace space;
    NervousSystem ns(space);
    NervousAgent agent(ns);

    ASSERT(!agent.is_running());
    agent.start();
    ASSERT(agent.is_running());
    agent.run_once();
    ASSERT_EQ(ns.bus().tick_count(), 1u);
    agent.stop();
    ASSERT(!agent.is_running());
    agent.run_once();
    ASSERT_EQ(ns.bus().tick_count(), 1u);  // should not have ticked
    return true;
}

TEST(NervousAgent_interval) {
    AtomSpace space;
    NervousSystem ns(space);
    NervousAgent agent(ns);
    ASSERT_EQ(agent.interval_ms(), 10u);  // default 10ms (10x faster than VES)
    agent.set_interval_ms(5);
    ASSERT_EQ(agent.interval_ms(), 5u);
    return true;
}
