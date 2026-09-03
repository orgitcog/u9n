#pragma once
/**
 * @file nervous_system.hpp
 * @brief Top-level facade for the Virtual Nervous System
 *
 * NervousSystem owns the NerveBus, NucleusRegistry, all neural adapters,
 * and the NeuroEndocrine bridge. It provides a unified interface for
 * initialization and per-tick updates.
 *
 * Integration connectors:
 *   connect_attention() → ThalamicECANAdapter  (ECAN attention gating)
 *   connect_pln()       → CorticalPLNAdapter   (PLN inference depth)
 *   connect_npu()       → AssociationNPUAdapter (NPU generation params)
 *   connect_o9c2()      → SubcorticalO9C2Adapter (o9c2 dynamics)
 *   connect_marduk()    → PrefrontalMardukAdapter (Marduk executive)
 *   connect_touchpad()  → SensoryTouchpadAdapter  (Touchpad manifold)
 *   connect_endocrine() → NeuroEndocrineBridge    (VNS ↔ VES coupling)
 *
 * 8-phase tick pipeline:
 *   1. Nuclei update (signal production)
 *   2. Bus tick (propagation, decay, level classification)
 *   3. Adapters READ signals → modulate targets
 *   4. Adapters WRITE feedback → results back to bus
 *   5. Reflex arcs (direct sensory→motor shortcuts)
 *   6. NeuroEndocrine bridge (bidirectional VNS↔VES)
 *   7. History recording (already done in bus tick, but events here)
 *   8. Event dispatch
 */

#include <opencog/nervous/nucleus.hpp>
#include <opencog/nervous/connector.hpp>
#include <opencog/nervous/thalamic_adapter.hpp>
#include <opencog/nervous/cortical_adapter.hpp>
#include <opencog/nervous/association_adapter.hpp>
#include <opencog/nervous/subcortical_adapter.hpp>
#include <opencog/nervous/prefrontal_adapter.hpp>
#include <opencog/nervous/sensory_adapter.hpp>
#include <opencog/nervous/neuroendocrine_bridge.hpp>

#include <atomic>
#include <memory>

namespace opencog::nerv {

// ============================================================================
// NervousSystem — Top-level facade
// ============================================================================

class NervousSystem {
public:
    explicit NervousSystem(AtomSpace& space, NerveBusConfig bus_config = {})
        : space_(space)
        , bus_(bus_config)
        , nuclei_(bus_)
    {
        nuclei_.register_defaults();
    }

    // === Subsystem Access ===

    [[nodiscard]] NerveBus& bus() noexcept { return bus_; }
    [[nodiscard]] const NerveBus& bus() const noexcept { return bus_; }
    [[nodiscard]] NucleusRegistry& nuclei() noexcept { return nuclei_; }

    // === Typed Nucleus Access ===

    template<std::derived_from<NeuralNucleus> N>
    [[nodiscard]] N* nucleus() noexcept { return nuclei_.get_nucleus<N>(); }

    // === Integration Connectors ===

    void connect_attention(AttentionBank& bank) {
        thalamic_adapter_ = std::make_unique<ThalamicECANAdapter>(bus_, bank);
    }

    void connect_pln(pln::PLNEngine& engine) {
        cortical_adapter_ = std::make_unique<CorticalPLNAdapter>(bus_, engine);
    }

    void connect_npu(endo::NPUInterface& npu) {
        association_adapter_ = std::make_unique<AssociationNPUAdapter>(bus_, npu);
    }

    void connect_o9c2(endo::O9C2Interface& o9c2) {
        subcortical_adapter_ = std::make_unique<SubcorticalO9C2Adapter>(bus_, o9c2);
    }

    void connect_marduk(endo::MardukInterface& marduk) {
        prefrontal_adapter_ = std::make_unique<PrefrontalMardukAdapter>(bus_, marduk);
    }

    void connect_touchpad(endo::TouchpadInterface& touchpad) {
        sensory_adapter_ = std::make_unique<SensoryTouchpadAdapter>(bus_, touchpad);
    }

    /**
     * @brief Connect to the Virtual Endocrine System for bidirectional coupling
     *
     * Creates the NeuroEndocrine bridge that accumulates neural signals and
     * syncs with the hormone bus at VES tick boundaries.
     */
    void connect_endocrine(endo::HormoneBus& hormone_bus) {
        neuroendocrine_bridge_ = std::make_unique<NeuroEndocrineBridge>(bus_, hormone_bus);
    }

    // === Adapter Access ===

    [[nodiscard]] ThalamicECANAdapter* thalamic_adapter() noexcept {
        return thalamic_adapter_.get();
    }
    [[nodiscard]] CorticalPLNAdapter* cortical_adapter() noexcept {
        return cortical_adapter_.get();
    }
    [[nodiscard]] AssociationNPUAdapter* association_adapter() noexcept {
        return association_adapter_.get();
    }
    [[nodiscard]] SubcorticalO9C2Adapter* subcortical_adapter() noexcept {
        return subcortical_adapter_.get();
    }
    [[nodiscard]] PrefrontalMardukAdapter* prefrontal_adapter() noexcept {
        return prefrontal_adapter_.get();
    }
    [[nodiscard]] SensoryTouchpadAdapter* sensory_adapter() noexcept {
        return sensory_adapter_.get();
    }
    [[nodiscard]] NeuroEndocrineBridge* neuroendocrine_bridge() noexcept {
        return neuroendocrine_bridge_.get();
    }

    // === Lifecycle ===

    /**
     * @brief Advance the nervous system by one tick (10ms nominal)
     *
     * 8-phase pipeline (critical ordering):
     *
     * 1. Nuclei update: all nuclei read inputs, compute outputs, fire signals
     * 2. Bus tick: propagate via connectivity matrix, decay, update level
     * 3. Adapters READ: neural signals → modulate subsystem parameters
     * 4. Adapters WRITE: subsystem telemetry → fire feedback signals
     * 5. Reflex arcs: direct sensory→motor shortcuts (1-tick latency)
     * 6. NeuroEndocrine bridge: accumulate signals for VES sync
     * 7. Event dispatch: fire event callbacks
     * 8. Increment VNS tick counter
     *
     * Read/write separation (steps 3/4) ensures feedback appears on the
     * NEXT tick, preventing cascading oscillations within a single tick.
     */
    void tick(float dt = 1.0f) {
        // 1. Update all nuclei (produce neural signals)
        nuclei_.update_all(dt);

        // 2. Tick the bus (propagate, decay, level classification, history)
        bus_.tick();

        // 3. Adapters READ signals → modulate targets
        if (thalamic_adapter_)    thalamic_adapter_->read_signals(bus_);
        if (cortical_adapter_)    cortical_adapter_->read_signals(bus_);
        if (association_adapter_) association_adapter_->read_signals(bus_);
        if (subcortical_adapter_) subcortical_adapter_->read_signals(bus_);
        if (prefrontal_adapter_)  prefrontal_adapter_->read_signals(bus_);
        if (sensory_adapter_)     sensory_adapter_->read_signals(bus_);

        // 4. Adapters WRITE feedback → signals back to bus
        if (thalamic_adapter_)    thalamic_adapter_->write_feedback();
        if (cortical_adapter_)    cortical_adapter_->write_feedback();
        if (association_adapter_) association_adapter_->write_feedback();
        if (subcortical_adapter_) subcortical_adapter_->write_feedback();
        if (prefrontal_adapter_)  prefrontal_adapter_->write_feedback();
        if (sensory_adapter_)     sensory_adapter_->write_feedback();

        // 5. Process reflex arcs (direct sensory→motor shortcuts)
        bus_.process_reflexes();

        // 6. NeuroEndocrine bridge accumulation (syncs at VES tick boundary)
        if (neuroendocrine_bridge_) {
            neuroendocrine_bridge_->accumulate();

            // Sync every 10 VNS ticks (= 1 VES tick at 10:1 ratio)
            if (bus_.tick_count() % ves_sync_ratio_ == 0) {
                neuroendocrine_bridge_->sync();
            }
        }

        // 7-8. Event dispatch handled by bus callbacks and adapter callbacks
    }

    /**
     * @brief Signal a neural event (high-level trigger)
     *
     * Events are dispatched as direct neural signals on appropriate channels.
     */
    void signal_event(NeuralEvent event, float intensity = 1.0f) {
        intensity = std::clamp(intensity, 0.0f, 1.0f);

        switch (event) {
        // === Sensory Events ===
        case NeuralEvent::SENSORY_INPUT_RECEIVED:
            bus_.fire(NeuralChannelId::VISUAL_PRIMARY, intensity * 0.5f, 0.5f);
            break;
        case NeuralEvent::GESTURE_DETECTED:
            bus_.fire(NeuralChannelId::GESTURAL_AFFERENT, intensity * 0.6f, 0.6f);
            break;
        case NeuralEvent::THREAT_PERCEIVED:
            bus_.fire(NeuralChannelId::THREAT_AFFERENT, intensity * 0.9f, 1.0f);
            break;
        case NeuralEvent::NOVELTY_DETECTED:
            bus_.fire(NeuralChannelId::NOVELTY_SIGNAL, intensity * 0.7f, 0.6f);
            break;
        case NeuralEvent::SOCIAL_SIGNAL_RECEIVED:
            bus_.fire(NeuralChannelId::SOCIAL_AFFERENT, intensity * 0.5f, 0.4f);
            break;

        // === Processing Events ===
        case NeuralEvent::ATTENTION_SHIFT:
            bus_.fire(NeuralChannelId::THALAMIC_GATE, intensity * 0.6f, 0.5f);
            break;
        case NeuralEvent::MEMORY_ENCODED:
            bus_.fire(NeuralChannelId::HIPPOCAMPAL_ENCODE, intensity * 0.5f, 0.4f);
            break;
        case NeuralEvent::MEMORY_RECALLED:
            bus_.fire(NeuralChannelId::HIPPOCAMPAL_RECALL, intensity * 0.5f, 0.4f);
            break;
        case NeuralEvent::PREDICTION_ERROR:
            bus_.fire(NeuralChannelId::CEREBELLAR_ERROR, intensity * 0.6f, 0.7f);
            bus_.fire(NeuralChannelId::ERROR_SIGNAL, intensity * 0.5f, 0.5f);
            break;
        case NeuralEvent::CONFLICT_DETECTED:
            bus_.fire(NeuralChannelId::ANTERIOR_CINGULATE, intensity * 0.7f, 0.6f);
            break;

        // === Motor Events ===
        case NeuralEvent::ACTION_INITIATED:
            bus_.fire(NeuralChannelId::BASAL_GANGLIA_GO, intensity * 0.6f, 0.5f);
            break;
        case NeuralEvent::TASK_DISPATCHED:
            bus_.fire(NeuralChannelId::MOTOR_TASK_DISPATCH, intensity * 0.5f, 0.5f);
            break;
        case NeuralEvent::INFERENCE_STARTED:
            bus_.fire(NeuralChannelId::MOTOR_INFERENCE, intensity * 0.4f, 0.4f);
            break;
        case NeuralEvent::EXPRESSION_EMITTED:
            bus_.fire(NeuralChannelId::MOTOR_EXPRESSION, intensity * 0.4f, 0.3f);
            break;
        case NeuralEvent::PERSONA_TRANSITION:
            bus_.fire(NeuralChannelId::MOTOR_PERSONA_SHIFT, intensity * 0.7f, 0.6f);
            break;

        // === Autonomic Events ===
        case NeuralEvent::SYMPATHETIC_ACTIVATION:
            bus_.fire(NeuralChannelId::SYMPATHETIC_OUT, intensity * 0.7f, 0.8f);
            break;
        case NeuralEvent::PARASYMPATHETIC_ACTIVATION:
            bus_.fire(NeuralChannelId::PARASYMPATHETIC_OUT, intensity * 0.5f, 0.3f);
            break;
        case NeuralEvent::REFLEX_ARC_TRIGGERED:
            // Reflexes are processed by bus_.process_reflexes()
            break;
        case NeuralEvent::LEVEL_TRANSITION:
            // Level transitions are handled by bus_.update_level()
            break;
        case NeuralEvent::NEUROENDOCRINE_SYNC:
            if (neuroendocrine_bridge_) neuroendocrine_bridge_->sync();
            break;

        // === System Events ===
        case NeuralEvent::PROCESSING_OVERLOAD:
            bus_.fire(NeuralChannelId::FATIGUE_SIGNAL, intensity * 0.8f, 0.7f);
            bus_.fire(NeuralChannelId::PAIN_SIGNAL, intensity * 0.3f, 0.5f);
            break;
        case NeuralEvent::FATIGUE_THRESHOLD_REACHED:
            bus_.fire(NeuralChannelId::FATIGUE_SIGNAL, intensity * 0.9f, 0.5f);
            break;
        case NeuralEvent::COHERENCE_RESTORED:
            bus_.fire(NeuralChannelId::COHERENCE_SIGNAL, intensity * 0.6f, 0.3f);
            break;
        case NeuralEvent::HOMEOSTASIS_ACHIEVED:
            bus_.fire(NeuralChannelId::HOMEOSTATIC_SIGNAL, intensity * 0.5f, 0.2f);
            bus_.fire(NeuralChannelId::COHERENCE_SIGNAL, intensity * 0.4f, 0.2f);
            break;

        default:
            break;
        }
    }

    // === Configuration ===

    [[nodiscard]] const NerveBusConfig& config() const noexcept {
        return bus_.config();
    }

    /// Set the VNS:VES tick ratio (default 10:1)
    void set_ves_sync_ratio(size_t ratio) noexcept { ves_sync_ratio_ = ratio; }

private:
    AtomSpace& space_;
    NerveBus bus_;
    NucleusRegistry nuclei_;

    // Core adapters
    std::unique_ptr<ThalamicECANAdapter> thalamic_adapter_;
    std::unique_ptr<CorticalPLNAdapter> cortical_adapter_;

    // Integration adapters
    std::unique_ptr<AssociationNPUAdapter> association_adapter_;
    std::unique_ptr<SubcorticalO9C2Adapter> subcortical_adapter_;
    std::unique_ptr<PrefrontalMardukAdapter> prefrontal_adapter_;
    std::unique_ptr<SensoryTouchpadAdapter> sensory_adapter_;

    // NeuroEndocrine bridge
    std::unique_ptr<NeuroEndocrineBridge> neuroendocrine_bridge_;

    // VNS:VES tick sync ratio (10 VNS ticks = 1 VES tick)
    size_t ves_sync_ratio_{10};
};

// ============================================================================
// NervousAgent — Background agent (follows EndocrineAgent pattern)
// ============================================================================

class NervousAgent {
public:
    explicit NervousAgent(NervousSystem& system)
        : system_(system)
    {}

    ~NervousAgent() { stop(); }

    NervousAgent(const NervousAgent&) = delete;
    NervousAgent& operator=(const NervousAgent&) = delete;

    void start() { running_.store(true, std::memory_order_release); }
    void stop() { running_.store(false, std::memory_order_release); }

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

    void run_once() {
        if (running_.load(std::memory_order_acquire)) {
            system_.tick(1.0f);
        }
    }

    void set_interval_ms(unsigned int ms) { interval_ms_ = ms; }
    [[nodiscard]] unsigned int interval_ms() const noexcept { return interval_ms_; }

private:
    NervousSystem& system_;
    std::atomic<bool> running_{false};
    unsigned int interval_ms_{10};  // 10ms default (10x faster than VES)
};

} // namespace opencog::nerv
