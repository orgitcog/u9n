#pragma once
/**
 * @file temporal_system.hpp
 * @brief Facade for the Temporal Crystal System (TCS)
 *
 * Unifies CrystalBus, TimeCrystalNeuron, TimeCrystalBrain, and the
 * VES/VNS adapters into a single coherent system that integrates
 * with the existing EndocrineSystem and NervousSystem tick pipeline.
 *
 * Integration with the tick pipeline:
 *   Phase 1: glands update → hormones produced
 *   Phase 2: hormone bus tick → decay, mode detection
 *   Phase 3: adapters READ → modulate targets
 *     → CrystalEndocrineAdapter reads hormones → modulates crystal bus
 *     → CrystalNeuralAdapter reads neural state → modulates crystal bus
 *   Phase 3.5: Crystal bus tick → phase evolution + coupling
 *   Phase 4: adapters WRITE → feedback to buses
 *     → CrystalEndocrineAdapter writes crystal state → hormone deltas
 *     → CrystalNeuralAdapter writes crystal state → neural deltas
 *   Phase 5: guidance → async trigger/receive
 *   Phase 6: valence → advance time
 */

#include <opencog/temporal/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>
#include <opencog/temporal/time_crystal_neuron.hpp>
#include <opencog/temporal/time_crystal_brain.hpp>
#include <opencog/temporal/endocrine_adapter.hpp>
#include <opencog/temporal/nervous_adapter.hpp>

#include <memory>
#include <optional>

namespace opencog::temporal {

/**
 * @brief System-level configuration for TCS
 */
struct TemporalSystemConfig {
    CrystalBusConfig bus_config{};
    CrystalEndocrineConfig endo_config{};
    CrystalNeuralConfig neural_config{};
    TCNStructure neuron_structure{TCNStructure::default_neuron()};
    bool enable_brain_model{true};
    bool enable_endo_coupling{true};
    bool enable_neural_coupling{true};
};

/**
 * @brief Unified facade for the Temporal Crystal System
 *
 * Owns the CrystalBus, TCN model, and TCNN brain model. Provides
 * connection points for VES and VNS via adapter creation methods.
 *
 * Usage:
 *   TemporalSystem tcs;
 *   tcs.connect_endocrine(hormone_bus);   // optional VES coupling
 *   tcs.connect_nervous(nerve_bus);       // optional VNS coupling
 *   tcs.tick(dt);                         // evolve + adapt
 */
class TemporalSystem {
public:
    explicit TemporalSystem(TemporalSystemConfig config = {}) noexcept
        : config_(config)
        , neuron_("cognitive", config.neuron_structure) {
        if (config.enable_brain_model) {
            brain_.emplace();
        }
    }

    // ---- Component access ----

    [[nodiscard]] const TimeCrystalNeuron& neuron() const noexcept { return neuron_; }
    [[nodiscard]] TimeCrystalNeuron& neuron() noexcept { return neuron_; }

    [[nodiscard]] const TimeCrystalBrain* brain() const noexcept {
        return brain_.has_value() ? &*brain_ : nullptr;
    }
    [[nodiscard]] TimeCrystalBrain* brain() noexcept {
        return brain_.has_value() ? &*brain_ : nullptr;
    }

    /// Access the neuron's crystal bus directly
    [[nodiscard]] const CrystalBus& bus() const noexcept { return neuron_.bus(); }
    [[nodiscard]] CrystalBus& bus() noexcept { return neuron_.bus(); }

    // ---- Adapter connection ----

    /// Connect to VES hormone bus — creates a CrystalEndocrineAdapter
    void connect_endocrine() noexcept {
        if (!endo_adapter_ && config_.enable_endo_coupling) {
            endo_adapter_ = std::make_unique<CrystalEndocrineAdapter>(
                neuron_.bus(), config_.endo_config);
        }
    }

    /// Connect to VNS nerve bus — creates a CrystalNeuralAdapter
    void connect_nervous() noexcept {
        if (!neural_adapter_ && config_.enable_neural_coupling) {
            neural_adapter_ = std::make_unique<CrystalNeuralAdapter>(
                neuron_.bus(), config_.neural_config);
        }
    }

    /// Access endocrine adapter (may be null)
    [[nodiscard]] CrystalEndocrineAdapter* endo_adapter() noexcept {
        return endo_adapter_.get();
    }

    /// Access neural adapter (may be null)
    [[nodiscard]] CrystalNeuralAdapter* neural_adapter() noexcept {
        return neural_adapter_.get();
    }

    // ---- Tick ----

    /**
     * @brief Full system tick
     *
     * Evolves the crystal bus, updates TCN and brain models, and
     * runs adapter coupling if connected. Should be called after
     * VES and VNS have completed their tick phases.
     *
     * @param dt Time step in seconds
     */
    void tick(float dt) noexcept {
        // Evolve the neuron model (includes bus evolution)
        neuron_.tick(dt);

        // Evolve the brain model (uses its own bus)
        if (brain_) {
            brain_->tick(dt);
        }

        ++tick_count_;
    }

    /**
     * @brief Apply endocrine modulation (VES → TCS)
     *
     * Called during tick phase 3 of the main pipeline.
     * @param hormones Current endocrine state
     */
    void apply_endocrine_modulation(const endo::EndocrineState& hormones) noexcept {
        if (endo_adapter_) {
            endo_adapter_->apply_endocrine_modulation(hormones);
        }
    }

    /**
     * @brief Write crystal feedback to hormones (TCS → VES)
     *
     * Called during tick phase 4 of the main pipeline.
     * @param hormones Mutable endocrine state for feedback
     */
    void apply_endocrine_feedback(endo::EndocrineState& hormones) noexcept {
        if (endo_adapter_) {
            endo_adapter_->apply_feedback(hormones);
        }
    }

    /**
     * @brief Apply neural modulation (VNS → TCS)
     *
     * Called during tick phase 3 of the main pipeline.
     * @param neural Current neural state
     */
    void apply_neural_modulation(const nerv::NeuralState& neural) noexcept {
        if (neural_adapter_) {
            neural_adapter_->apply_neural_modulation(neural);
        }
    }

    /**
     * @brief Write crystal feedback to neural channels (TCS → VNS)
     *
     * Called during tick phase 4 of the main pipeline.
     * @param neural Mutable neural state for feedback
     */
    void apply_neural_feedback(nerv::NeuralState& neural) noexcept {
        if (neural_adapter_) {
            neural_adapter_->apply_feedback(neural);
        }
    }

    // ---- Metrics ----

    /// Neuron global phase coherence
    [[nodiscard]] float neuron_coherence() const noexcept {
        return neuron_.global_coherence();
    }

    /// Brain global phase coherence (if brain model enabled)
    [[nodiscard]] float brain_coherence() const noexcept {
        return brain_ ? brain_->global_coherence() : 0.0f;
    }

    /// Dominant cognitive process
    [[nodiscard]] CognitiveProcess dominant_process() const noexcept {
        return neuron_.dominant_process();
    }

    /// Whether global state (Universal Sets) dominates local (Particular Sets)
    [[nodiscard]] bool is_global_dominant() const noexcept {
        return neuron_.is_global_dominant();
    }

    [[nodiscard]] uint64_t tick_count() const noexcept { return tick_count_; }
    [[nodiscard]] const TemporalSystemConfig& config() const noexcept { return config_; }

private:
    TemporalSystemConfig config_;
    TimeCrystalNeuron neuron_;
    std::optional<TimeCrystalBrain> brain_;

    std::unique_ptr<CrystalEndocrineAdapter> endo_adapter_;
    std::unique_ptr<CrystalNeuralAdapter> neural_adapter_;

    uint64_t tick_count_{0};
};

} // namespace opencog::temporal
