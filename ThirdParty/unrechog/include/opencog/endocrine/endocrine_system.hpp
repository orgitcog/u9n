#pragma once
/**
 * @file endocrine_system.hpp
 * @brief Top-level facade and background agent for the Virtual Endocrine System
 *
 * EndocrineSystem owns the hormone bus, gland registry, valence memory,
 * affective integration, and moral perception layers. It provides a
 * unified interface for initialization and per-tick updates.
 *
 * Integration connectors:
 *   connect_attention() → ECANEndocrineAdapter    (ECAN parameters)
 *   connect_pln()       → PLNEndocrineAdapter     (PLN parameters)
 *   connect_npu()       → NPUEndocrineAdapter     (NPU inference parameters)
 *   connect_o9c2()      → O9C2EndocrineAdapter    (o9c2 persona/hyperparameters)
 *   connect_marduk()    → MardukEndocrineAdapter   (Marduk operational parameters)
 *   connect_touchpad()  → TouchpadEndocrineAdapter (VirtualTouchpad manifold parameters)
 *   connect_entelechy() → EntelechyEndocrineAdapter (Cloninger gains, interoception, Civic Angel)
 *   connect_afi()       → AFIEndocrineAdapter       (free energy, precision weighting, districts)
 *   connect_guidance()  → GuidanceConnector        (external Claude guidance)
 *
 * EndocrineAgent follows the ImportanceDiffusionAgent pattern with
 * start(), stop(), and run_once() for background operation.
 */

#include <opencog/endocrine/moral.hpp>
#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/npu_adapter.hpp>
#include <opencog/endocrine/o9c2_adapter.hpp>
#include <opencog/endocrine/marduk_adapter.hpp>
#include <opencog/endocrine/touchpad_adapter.hpp>
#include <opencog/entelechy/adapter.hpp>
#include <opencog/afi/adapter.hpp>
#include <opencog/endocrine/guidance_connector.hpp>
#include <opencog/endocrine/gland.hpp>
#include <opencog/endocrine/glands/hpa_axis.hpp>
#include <opencog/endocrine/glands/dopaminergic.hpp>
#include <opencog/endocrine/glands/serotonergic.hpp>
#include <opencog/endocrine/glands/noradrenergic.hpp>
#include <opencog/endocrine/glands/oxytocinergic.hpp>
#include <opencog/endocrine/glands/thyroid.hpp>
#include <opencog/endocrine/glands/circadian.hpp>
#include <opencog/endocrine/glands/pancreatic.hpp>
#include <opencog/endocrine/glands/immune.hpp>
#include <opencog/endocrine/glands/endocannabinoid.hpp>

#include <atomic>
#include <memory>

namespace opencog::endo {

// ============================================================================
// EndocrineSystem — Top-level facade
// ============================================================================

class EndocrineSystem {
public:
    explicit EndocrineSystem(AtomSpace& space, HormoneBusConfig bus_config = {})
        : space_(space)
        , bus_(bus_config)
        , glands_(bus_)
        , valence_(space)
        , affect_(valence_, bus_, space)
        , moral_(affect_, valence_, bus_, space)
    {
        // Register all default glands
        glands_.register_defaults();
    }

    // === Subsystem Access ===

    [[nodiscard]] HormoneBus& bus() noexcept { return bus_; }
    [[nodiscard]] const HormoneBus& bus() const noexcept { return bus_; }
    [[nodiscard]] GlandRegistry& glands() noexcept { return glands_; }
    [[nodiscard]] ValenceMemory& valence() noexcept { return valence_; }
    [[nodiscard]] AffectiveIntegration& affect() noexcept { return affect_; }
    [[nodiscard]] MoralPerceptionEngine& moral() noexcept { return moral_; }

    // === Typed Gland Access ===

    template<std::derived_from<VirtualGland> G>
    [[nodiscard]] G* gland() noexcept { return glands_.get_gland<G>(); }

    // === Core Connectors (ECAN + PLN) ===

    void connect_attention(AttentionBank& bank) {
        ecan_adapter_ = std::make_unique<ECANEndocrineAdapter>(bus_, bank);
    }

    void connect_pln(pln::PLNEngine& engine) {
        pln_adapter_ = std::make_unique<PLNEndocrineAdapter>(bus_, engine);
    }

    // === Integration Connectors (NPU + o9c2 + Guidance) ===

    /**
     * @brief Connect NPU co-processor for bidirectional hormonal modulation
     *
     * The NPU adapter reads hormone concentrations to modulate inference
     * parameters (creativity, n_predict, batch_size) and feeds telemetry
     * back as hormonal signals (errors → IL6, load → NPU_LOAD, success → DA).
     */
    void connect_npu(NPUInterface& npu) {
        npu_adapter_ = std::make_unique<NPUEndocrineAdapter>(bus_, npu);
    }

    /**
     * @brief Connect o9c2 Deep Tree Echo for persona/hyperparameter modulation
     *
     * The o9c2 adapter reads hormones to modulate ESN hyperparameters
     * (spectral_radius, input_scaling, leak_rate, membrane_permeability),
     * manages persona transitions from CognitiveMode changes, and feeds
     * emergence metrics back into the bus.
     */
    void connect_o9c2(O9C2Interface& o9c2) {
        o9c2_adapter_ = std::make_unique<O9C2EndocrineAdapter>(bus_, o9c2);
    }

    /**
     * @brief Connect Marduk Left Hemisphere for operational parameter modulation
     *
     * The Marduk adapter reads hormones to modulate task management parameters
     * (task_urgency, consolidation_depth, validation_threshold, etc.),
     * manages operational mode transitions from CognitiveMode changes, and
     * feeds execution metrics back into the bus. Hemispheric coupling with
     * o9c2 occurs via COG_COHERENCE (ch15) and ORG_COHERENCE (ch17).
     */
    void connect_marduk(MardukInterface& marduk) {
        marduk_adapter_ = std::make_unique<MardukEndocrineAdapter>(bus_, marduk);
    }

    /**
     * @brief Connect VirtualTouchpad for gesture manifold modulation
     *
     * The Touchpad adapter reads hormones to modulate manifold parameters
     * (sensitivity, dimensionality, curvature, etc.), manages operational
     * mode transitions from CognitiveMode changes, and feeds gesture metrics
     * back into the bus. Cross-module coupling with Marduk occurs via
     * ORG_COHERENCE (ch17) → coherence_requirement.
     */
    void connect_touchpad(TouchpadInterface& touchpad) {
        touchpad_adapter_ = std::make_unique<TouchpadEndocrineAdapter>(bus_, touchpad);
    }

    /**
     * @brief Connect Entelechy system for personality-shaped endocrine modulation
     *
     * The Entelechy adapter operates in three phases:
     *   Phase 0: Cloninger personality gains shape hormone production/decay
     *   Phase 7: InteroceptiveModel updates channels 20-31
     *   Phase 8: Civic Angel self-observation (self-throttled)
     *
     * Feedback signals: self-coherence crossings, free energy spikes,
     * and polyvagal tier transitions emit hormonal signals.
     */
    void connect_entelechy(entelechy::CivicAngel& angel) {
        entelechy_adapter_ = std::make_unique<EntelechyEndocrineAdapter>(bus_, angel);
    }

    /**
     * @brief Connect AFI (Active Free-energy Inference) for district monitoring
     *
     * The AFI adapter monitors cognitive districts wrapped in Markov blankets,
     * computes city-wide free energy and inter-district divergence, and maps
     * precision weights to ECAN STI values. Districts must be registered
     * separately via afi_adapter()->register_district().
     *
     * Feedback signals: city free energy spikes/drops emit hormonal signals.
     */
    void connect_afi() {
        afi_adapter_ = std::make_unique<AFIEndocrineAdapter>(bus_);
    }

    /**
     * @brief Connect external guidance agent (Main Claude)
     *
     * The guidance connector monitors the system for trigger conditions
     * (stress, moral novelty, coherence drops) and asynchronously requests
     * guidance from an external agent. Responses are applied as hormone
     * nudges and mode/persona suggestions.
     *
     * @param backend Unique ownership of the guidance transport (stub, HTTP, MMIO)
     * @param config Trigger thresholds and timing configuration
     */
    void connect_guidance(std::unique_ptr<GuidanceBackend> backend,
                          GuidanceConfig config = {}) {
        guidance_connector_ = std::make_unique<GuidanceConnector>(
            bus_, std::move(backend), config);

        // Wire cross-references so guidance can read o9c2/NPU/Marduk state
        if (o9c2_adapter_) {
            guidance_connector_->set_o9c2_source(o9c2_adapter_.get());
        }
        if (npu_adapter_) {
            guidance_connector_->set_npu_source(npu_adapter_.get());
        }
        if (marduk_adapter_) {
            guidance_connector_->set_marduk_source(marduk_adapter_.get());
        }
        if (touchpad_adapter_) {
            guidance_connector_->set_touchpad_source(touchpad_adapter_.get());
        }
    }

    // === Adapter Access ===

    [[nodiscard]] NPUEndocrineAdapter* npu_adapter() noexcept {
        return npu_adapter_.get();
    }
    [[nodiscard]] O9C2EndocrineAdapter* o9c2_adapter() noexcept {
        return o9c2_adapter_.get();
    }
    [[nodiscard]] MardukEndocrineAdapter* marduk_adapter() noexcept {
        return marduk_adapter_.get();
    }
    [[nodiscard]] TouchpadEndocrineAdapter* touchpad_adapter() noexcept {
        return touchpad_adapter_.get();
    }
    [[nodiscard]] EntelechyEndocrineAdapter* entelechy_adapter() noexcept {
        return entelechy_adapter_.get();
    }
    [[nodiscard]] AFIEndocrineAdapter* afi_adapter() noexcept {
        return afi_adapter_.get();
    }
    [[nodiscard]] GuidanceConnector* guidance() noexcept {
        return guidance_connector_.get();
    }

    // === Lifecycle ===

    /**
     * @brief Advance the entire system by one tick
     *
     * Pipeline ordering (critical for consistency):
     *
     * Phase 0: Entelechy Cloninger gains → bus production/decay modifiers
     * Phase 1: Glands produce hormones (shaped by Phase 0 gains)
     * Phase 2: Bus decays, records history, detects mode
     * Phase 3: All adapters READ hormones and modulate their targets
     * Phase 4: Bidirectional adapters WRITE telemetry/metrics back to bus
     * Phase 5: Guidance checks triggers, sends/receives async, applies responses
     * Phase 6: Advance valence memory time
     * Phase 7: Entelechy interoceptive update → write channels 20-31
     * Phase 8: Civic Angel self-observation (self-throttled)
     *
     * Phase 0 runs BEFORE gland production so that Cloninger personality
     * traits shape the sensitivity of the entire endocrine cascade.
     *
     * Phase 7 runs AFTER adapter feedback so the interoceptive model
     * reflects the complete hormonal state including all feedback effects.
     *
     * Phase 8 runs at the very end, self-throttled by the Civic Angel
     * (typically every ~10 ticks). This is the slowest-frequency update
     * in the system, consistent with self-reflection being costly.
     *
     * The separation of read (Phase 3) and write (Phase 4) ensures that
     * feedback effects appear on the NEXT tick, not the current one,
     * preventing cascading oscillations within a single tick.
     */
    void tick(float dt = 1.0f) {
        // Phase 0: Apply Cloninger personality gains (BEFORE gland production)
        if (entelechy_adapter_) {
            auto gains = entelechy_adapter_->compute_gains();
            bus_.set_production_gains(gains.production_gains);
            bus_.set_decay_gains(gains.decay_gains);
        }

        // Phase 1: Update all glands (produce hormones, shaped by Phase 0 gains)
        glands_.update_all(dt);

        // Phase 2: Tick the bus (decay, history, mode detection)
        bus_.tick();

        // Phase 3: Apply adapter modulations (READ from bus → modulate targets)
        if (ecan_adapter_) {
            ecan_adapter_->apply_endocrine_modulation(bus_);
        }
        if (pln_adapter_) {
            pln_adapter_->apply_endocrine_modulation(bus_);
        }
        if (npu_adapter_) {
            npu_adapter_->apply_endocrine_modulation(bus_);
        }
        if (o9c2_adapter_) {
            o9c2_adapter_->apply_endocrine_modulation(bus_);
        }
        if (marduk_adapter_) {
            marduk_adapter_->apply_endocrine_modulation(bus_);
        }
        if (touchpad_adapter_) {
            touchpad_adapter_->apply_endocrine_modulation(bus_);
        }
        if (entelechy_adapter_) {
            entelechy_adapter_->apply_endocrine_modulation(bus_);
        }
        if (afi_adapter_) {
            afi_adapter_->apply_endocrine_modulation(bus_);
        }

        // Phase 4: Bidirectional feedback (WRITE target state back to bus)
        if (npu_adapter_) {
            npu_adapter_->apply_feedback();
        }
        if (o9c2_adapter_) {
            o9c2_adapter_->apply_feedback();
        }
        if (marduk_adapter_) {
            marduk_adapter_->apply_feedback();
        }
        if (touchpad_adapter_) {
            touchpad_adapter_->apply_feedback();
        }
        if (entelechy_adapter_) {
            entelechy_adapter_->apply_feedback();
        }
        if (afi_adapter_) {
            afi_adapter_->apply_feedback();
        }

        // Phase 5: Guidance check (async trigger/receive/apply cycle)
        if (guidance_connector_) {
            // Feed moral novelty from moral engine's last perception
            // (moral perception runs as part of affect computation)
            guidance_connector_->tick(bus_);
        }

        // Phase 6: Advance valence memory time
        valence_.tick();

        // Phase 7: Update interoceptive model from full bus state,
        //          write computed channels 20-31 back to bus
        if (entelechy_adapter_) {
            entelechy_adapter_->update_interoceptive();
            entelechy_adapter_->write_interoceptive_to_bus();
        }

        // Phase 8: Civic Angel self-observation (self-throttled)
        if (entelechy_adapter_) {
            entelechy_adapter_->observe(tick_count_);
        }

        tick_count_++;
    }

    /**
     * @brief Signal an endocrine event (high-level trigger)
     *
     * Events are dispatched to appropriate glands to trigger hormonal
     * cascades. Includes core events (0-19), NPU events (20-29),
     * o9c2 events (30-39), guidance events (40-49), Marduk events (50-59),
     * VirtualTouchpad events (60-69), Entelechy events (70-78),
     * AFI events (80-84), and Civic Angel events (85-89).
     */
    void signal_event(EndocrineEvent event, float intensity = 1.0f) {
        intensity = std::clamp(intensity, 0.0f, 1.0f);

        switch (event) {
        // === Core Events ===

        case EndocrineEvent::THREAT_DETECTED:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.5f);
            break;

        case EndocrineEvent::REWARD_RECEIVED:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity);
            break;

        case EndocrineEvent::SOCIAL_BOND_SIGNAL:
            if (auto* oxy = glands_.get_gland<OxytocinergicGland>())
                oxy->signal_social(intensity);
            break;

        case EndocrineEvent::RESOURCE_DEPLETED:
            if (auto* panc = glands_.get_gland<PancreaticGland>())
                panc->signal_resource_demand(intensity);
            break;

        case EndocrineEvent::NOVELTY_ENCOUNTERED:
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity);
            break;

        case EndocrineEvent::GOAL_ACHIEVED:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.8f);
            break;

        case EndocrineEvent::CONFLICT_DETECTED:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.5f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            break;

        case EndocrineEvent::UNCERTAINTY_HIGH:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.5f);
            break;

        case EndocrineEvent::ERROR_DETECTED:
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity);
            break;

        case EndocrineEvent::NOISE_EXCESSIVE:
            if (auto* ecb = glands_.get_gland<EndocannabinoidGland>())
                ecb->signal_noise(intensity);
            break;

        case EndocrineEvent::LIGHT_SIGNAL:
            if (auto* circ = glands_.get_gland<CircadianGland>())
                circ->signal_light(intensity);
            break;

        // === NPU Events ===

        case EndocrineEvent::NPU_INFERENCE_STARTED:
            bus_.produce(HormoneId::NPU_LOAD, intensity * 0.3f);
            break;

        case EndocrineEvent::NPU_INFERENCE_COMPLETE:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.3f);
            break;

        case EndocrineEvent::NPU_INFERENCE_ERROR:
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity);
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            break;

        case EndocrineEvent::NPU_HIGH_LOAD:
            bus_.produce(HormoneId::NPU_LOAD, intensity * 0.5f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.2f);
            break;

        case EndocrineEvent::NPU_MODEL_LOADED:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.1f);
            break;

        // === o9c2 Events ===

        case EndocrineEvent::O9C2_PERSONA_TRANSITION:
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.2f);
            break;

        case EndocrineEvent::O9C2_EMERGENCE_SPIKE:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.4f);
            break;

        case EndocrineEvent::O9C2_COHERENCE_DROP:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.4f);
            break;

        case EndocrineEvent::O9C2_WISDOM_GAIN:
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.15f);
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.2f);
            break;

        case EndocrineEvent::O9C2_INSTABILITY:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            break;

        // === Guidance Events ===

        case EndocrineEvent::GUIDANCE_REQUESTED:
            // Requesting guidance is slightly stressful (uncertainty)
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.1f);
            break;

        case EndocrineEvent::GUIDANCE_RECEIVED:
            // Receiving guidance is mildly rewarding (resolution)
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.2f);
            break;

        case EndocrineEvent::GUIDANCE_TIMEOUT:
            // Timeout increases stress and uncertainty
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.2f);
            break;

        case EndocrineEvent::GUIDANCE_CONFLICT:
            // Conflicting guidance is stressful
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.4f);
            break;

        // === Marduk Events (Left Hemisphere) ===

        case EndocrineEvent::MARDUK_TASK_STARTED:
            bus_.produce(HormoneId::MARDUK_LOAD, intensity * 0.2f);
            break;

        case EndocrineEvent::MARDUK_TASK_COMPLETED:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.3f);
            break;

        case EndocrineEvent::MARDUK_TASK_FAILED:
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity * 0.5f);
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            break;

        case EndocrineEvent::MARDUK_MEMORY_CONSOLIDATED:
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.1f);
            break;

        case EndocrineEvent::MARDUK_AUTONOMY_CYCLE:
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.05f);
            break;

        case EndocrineEvent::MARDUK_OPTIMIZATION_COMPLETE:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.4f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            break;

        case EndocrineEvent::MARDUK_COGNITIVE_OVERLOAD:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.5f);
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity * 0.3f);
            bus_.produce(HormoneId::MARDUK_LOAD, intensity * 0.5f);
            break;

        case EndocrineEvent::MARDUK_LIGHTFACE_SPIKE:
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.15f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.2f);
            break;

        case EndocrineEvent::MARDUK_DARKFACE_SYNTHESIS:
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            bus_.produce(HormoneId::ANANDAMIDE, intensity * 0.05f);
            break;

        case EndocrineEvent::MARDUK_HEMISPHERIC_SYNC:
            // Successful Left-Right synchronization is deeply rewarding
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.3f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.1f);
            break;

        // === VirtualTouchpad Events ===

        case EndocrineEvent::TOUCHPAD_CONTACT_STARTED:
            bus_.produce(HormoneId::TOUCHPAD_LOAD, intensity * 0.2f);
            break;

        case EndocrineEvent::TOUCHPAD_CONTACT_ENDED:
            // Contact release — mild processing signal
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.02f);
            break;

        case EndocrineEvent::TOUCHPAD_GESTURE_RECOGNIZED:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.2f);
            break;

        case EndocrineEvent::TOUCHPAD_GESTURE_EMITTED:
            bus_.produce(HormoneId::TOUCHPAD_LOAD, intensity * 0.15f);
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.05f);
            break;

        case EndocrineEvent::TOUCHPAD_CALIBRATION_STARTED:
            bus_.produce(HormoneId::MELATONIN, intensity * 0.1f);
            break;

        case EndocrineEvent::TOUCHPAD_CALIBRATION_COMPLETE:
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.15f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.08f);
            break;

        case EndocrineEvent::TOUCHPAD_COHERENCE_DROP:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.2f);
            break;

        case EndocrineEvent::TOUCHPAD_FIELD_RESONANCE:
            // Standing wave = stable pattern = rewarding
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.3f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            break;

        case EndocrineEvent::TOUCHPAD_TOPOLOGY_CHANGE:
            // Manifold change = novelty
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.1f);
            break;

        case EndocrineEvent::TOUCHPAD_OVERLOAD:
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.4f);
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity * 0.3f);
            bus_.produce(HormoneId::TOUCHPAD_LOAD, intensity * 0.5f);
            break;

        // === Entelechy Events (Ontogenetic Self-Model) ===

        case EndocrineEvent::INTEROCEPTIVE_ALARM:
            // Interoceptive distress → stress + immune alert
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.4f);
            if (auto* immune = glands_.get_gland<ImmuneMonitor>())
                immune->signal_error(intensity * 0.2f);
            break;

        case EndocrineEvent::POLYVAGAL_STATE_CHANGE:
            // Autonomic tier shift → mild arousal (state transition cost)
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.2f);
            break;

        case EndocrineEvent::DEVELOPMENTAL_TRANSITION:
            // Stage advance → significant reward + serotonin (growth)
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.5f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.15f);
            break;

        case EndocrineEvent::CHAPTER_BOUNDARY:
            // Narrative chapter close → mild consolidation signal
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            bus_.produce(HormoneId::MELATONIN, intensity * 0.05f);
            break;

        case EndocrineEvent::TRAUMA_ENCODED:
            // Trauma encoding → strong stress cascade
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.7f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.5f);
            bus_.produce(HormoneId::IL6, intensity * 0.3f);
            break;

        case EndocrineEvent::TRAUMA_HEALING:
            // Healing progress → gentle reward + serotonin
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.15f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.05f);
            break;

        case EndocrineEvent::ATTACHMENT_SHIFT:
            // Attachment change → moderate arousal (identity relevance)
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.1f);
            break;

        case EndocrineEvent::IDENTITY_CRYSTALLIZED:
            // Self-model coherence threshold → deep reward + serotonin
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.4f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.15f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.1f);
            break;

        case EndocrineEvent::SELF_TRANSCENDENCE_SPIKE:
            // Self-transcendence → anandamide + serotonin (flow/awe)
            bus_.produce(HormoneId::ANANDAMIDE, intensity * 0.15f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            break;

        // === AFI Events (Active Free-energy Inference) ===

        case EndocrineEvent::FREE_ENERGY_SPIKE:
            // District free energy alarm → stress + vigilance
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            break;

        case EndocrineEvent::PREDICTION_FAILURE:
            // Generative model failure → mild stress + novelty
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.2f);
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.4f);
            break;

        case EndocrineEvent::PRECISION_REWEIGHT:
            // Precision recalculation → mild arousal (attention shift)
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.1f);
            break;

        case EndocrineEvent::BLANKET_RECONFIGURED:
            // Markov blanket change → novelty + mild dopamine
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.3f);
            bus_.produce(HormoneId::DOPAMINE_TONIC, intensity * 0.05f);
            break;

        case EndocrineEvent::MODEL_UPDATE:
            // Generative model parameters updated → mild reward
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.1f);
            break;

        // === Civic Angel Events ===

        case EndocrineEvent::CITY_COHERENCE_HIGH:
            // City-wide coherence → deep well-being signal
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.1f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.05f);
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.2f);
            break;

        case EndocrineEvent::CITY_COHERENCE_LOW:
            // City-wide incoherence → stress signal
            if (auto* hpa = glands_.get_gland<HPAAxis>())
                hpa->signal_stress(intensity * 0.3f);
            break;

        case EndocrineEvent::CIVIC_ANGEL_OBSERVATION:
            // Self-model observation cycle → minimal metabolic cost
            bus_.produce(HormoneId::GLUCAGON, intensity * 0.02f);
            break;

        case EndocrineEvent::RESOURCE_REALLOCATION:
            // Attention reallocation → mild arousal
            if (auto* ne = glands_.get_gland<NoradrenergicGland>())
                ne->signal_novelty(intensity * 0.15f);
            break;

        case EndocrineEvent::ENTELECHY_MILESTONE:
            // Developmental milestone reached → strong reward cascade
            if (auto* da = glands_.get_gland<DopaminergicGland>())
                da->signal_reward(intensity * 0.5f);
            bus_.produce(HormoneId::SEROTONIN, intensity * 0.15f);
            bus_.produce(HormoneId::OXYTOCIN, intensity * 0.1f);
            bus_.produce(HormoneId::ANANDAMIDE, intensity * 0.05f);
            break;

        default:
            break;  // Unknown event — ignore gracefully
        }
    }

    // === Configuration ===

    [[nodiscard]] const HormoneBusConfig& config() const noexcept {
        return bus_.config();
    }

private:
    AtomSpace& space_;
    HormoneBus bus_;
    GlandRegistry glands_;
    ValenceMemory valence_;
    AffectiveIntegration affect_;
    MoralPerceptionEngine moral_;

    // Core adapters
    std::unique_ptr<ECANEndocrineAdapter> ecan_adapter_;
    std::unique_ptr<PLNEndocrineAdapter> pln_adapter_;

    // Integration adapters
    std::unique_ptr<NPUEndocrineAdapter> npu_adapter_;
    std::unique_ptr<O9C2EndocrineAdapter> o9c2_adapter_;
    std::unique_ptr<MardukEndocrineAdapter> marduk_adapter_;
    std::unique_ptr<TouchpadEndocrineAdapter> touchpad_adapter_;
    std::unique_ptr<EntelechyEndocrineAdapter> entelechy_adapter_;
    std::unique_ptr<AFIEndocrineAdapter> afi_adapter_;
    std::unique_ptr<GuidanceConnector> guidance_connector_;

    // Tick counter for Phase 8 (Civic Angel observation scheduling)
    uint64_t tick_count_{0};
};

// ============================================================================
// EndocrineAgent — Background agent (follows ImportanceDiffusionAgent pattern)
// ============================================================================

class EndocrineAgent {
public:
    explicit EndocrineAgent(EndocrineSystem& system)
        : system_(system)
    {}

    ~EndocrineAgent() { stop(); }

    EndocrineAgent(const EndocrineAgent&) = delete;
    EndocrineAgent& operator=(const EndocrineAgent&) = delete;

    void start() { running_.store(true, std::memory_order_release); }
    void stop() { running_.store(false, std::memory_order_release); }

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

    /// Execute one update cycle (call from event loop or thread)
    void run_once() {
        if (running_.load(std::memory_order_acquire)) {
            system_.tick(1.0f);
        }
    }

    void set_interval_ms(unsigned int ms) { interval_ms_ = ms; }
    [[nodiscard]] unsigned int interval_ms() const noexcept { return interval_ms_; }

private:
    EndocrineSystem& system_;
    std::atomic<bool> running_{false};
    unsigned int interval_ms_{100};
};

} // namespace opencog::endo
