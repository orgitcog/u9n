#pragma once
/**
 * @file nucleus.hpp
 * @brief Neural nucleus base class and registry
 *
 * Each nucleus is an independent signal producer that reads excitatory/inhibitory
 * inputs from the NerveBus, computes an output activation, and fires its output
 * channel. Mirrors the VirtualGland / GlandRegistry pattern from the VES.
 *
 * Key differences from VirtualGland:
 * - Output is bipolar [-1,+1] (not [0,1] concentration)
 * - Self-inhibition (refractory period) instead of negative feedback
 * - Adaptation rate (threshold drift) instead of allostatic drift
 * - Threshold gating: nucleus only fires when net input exceeds threshold
 */

#include <opencog/nervous/nerve_bus.hpp>

#include <concepts>
#include <memory>
#include <vector>

namespace opencog::nerv {

// ============================================================================
// NeuralNucleus — Abstract base for all nuclei
// ============================================================================

class NeuralNucleus {
public:
    explicit NeuralNucleus(NerveBus& bus, NucleusConfig config)
        : bus_(bus)
        , config_(std::move(config))
        , adapted_threshold_(config_.threshold)
    {}

    virtual ~NeuralNucleus() = default;

    NeuralNucleus(const NeuralNucleus&) = delete;
    NeuralNucleus& operator=(const NeuralNucleus&) = delete;

    /// Advance one time step: read inputs, compute output, fire
    virtual void update(float dt) = 0;

    /// Compute current output activation from inputs
    virtual float compute_output() const = 0;

    // === Accessors ===

    [[nodiscard]] const NucleusConfig& config() const noexcept { return config_; }
    [[nodiscard]] NeuralChannelId output_channel() const noexcept { return config_.output_channel; }
    [[nodiscard]] float current_output() const noexcept { return current_output_; }
    [[nodiscard]] float adapted_threshold() const noexcept { return adapted_threshold_; }
    [[nodiscard]] const std::string& name() const noexcept { return config_.name; }

    // === Adaptation ===

    /// Shift the threshold (chronic activation causes threshold drift)
    void shift_threshold(float delta) noexcept {
        adapted_threshold_ = std::clamp(
            adapted_threshold_ + delta, 0.0f, 1.0f
        );
    }

    // === Callbacks ===

    void on_fire(std::function<void(NeuralChannelId, float)> cb) {
        fire_cb_ = std::move(cb);
    }

protected:
    NerveBus& bus_;
    NucleusConfig config_;
    float current_output_{0.0f};
    float adapted_threshold_;

    /// Sum excitatory input from bus
    [[nodiscard]] float read_excitation() const {
        float exc = 0.0f;
        for (auto& [channel, sensitivity] : config_.excitatory_inputs) {
            exc += bus_.activation(channel) * sensitivity;
        }
        return exc;
    }

    /// Sum inhibitory input from bus
    [[nodiscard]] float read_inhibition() const {
        float inh = 0.0f;
        for (auto& [channel, sensitivity] : config_.inhibitory_inputs) {
            inh += std::abs(bus_.activation(channel)) * sensitivity;
        }
        return inh;
    }

    /// Apply self-inhibition (refractory period)
    [[nodiscard]] float apply_self_inhibition(float raw_output) const {
        if (!config_.self_inhibition) return raw_output;
        float own = std::abs(bus_.activation(config_.output_channel));
        float suppression = own * config_.self_inhibition_gain;
        return raw_output * std::max(0.0f, 1.0f - suppression);
    }

    /// Fire output signal to bus and invoke callback
    void emit(float activation) {
        float clamped = std::clamp(activation, -config_.max_output, config_.max_output);
        if (std::abs(clamped) > 0.001f) {
            bus_.fire(config_.output_channel, clamped);
            if (fire_cb_) {
                fire_cb_(config_.output_channel, clamped);
            }
        }
    }

    /// Update threshold adaptation (chronic activation causes threshold drift)
    void update_adaptation(float dt) {
        float own = std::abs(bus_.activation(config_.output_channel));
        float drift = (own - adapted_threshold_) * config_.adaptation_rate * dt;
        adapted_threshold_ = std::clamp(adapted_threshold_ + drift, 0.0f, 1.0f);
    }

    std::function<void(NeuralChannelId, float)> fire_cb_;
};

// ============================================================================
// SimpleNucleus — Default implementation for basic nuclei
// ============================================================================

/**
 * @brief A straightforward nucleus: excitation - inhibition with threshold
 *        gating, self-inhibition, and adaptation. Suitable for most nuclei.
 */
class SimpleNucleus : public NeuralNucleus {
public:
    using NeuralNucleus::NeuralNucleus;

    float compute_output() const override {
        float exc = read_excitation();
        float inh = read_inhibition();
        float net = config_.base_output + exc - inh;

        // Threshold gating
        if (std::abs(net) < adapted_threshold_) {
            return 0.0f;
        }

        return apply_self_inhibition(net);
    }

    void update(float dt) override {
        current_output_ = compute_output();
        emit(current_output_ * dt);
        update_adaptation(dt);
    }
};

// ============================================================================
// NucleusRegistry — Owns and updates all nuclei
// ============================================================================

class NucleusRegistry {
public:
    explicit NucleusRegistry(NerveBus& bus) : bus_(bus) {}

    /// Register all default biological nuclei
    void register_defaults();

    /// Add a custom nucleus and return a reference to it
    template<std::derived_from<NeuralNucleus> N, typename... Args>
    N& add_nucleus(Args&&... args) {
        auto ptr = std::make_unique<N>(bus_, std::forward<Args>(args)...);
        N& ref = *ptr;
        nuclei_.push_back(std::move(ptr));
        return ref;
    }

    /// Update all nuclei by dt
    void update_all(float dt) {
        for (auto& nucleus : nuclei_) {
            nucleus->update(dt);
        }
    }

    /// Find a nucleus by type (returns nullptr if not found)
    template<std::derived_from<NeuralNucleus> N>
    [[nodiscard]] N* get_nucleus() noexcept {
        for (auto& n : nuclei_) {
            if (auto* p = dynamic_cast<N*>(n.get())) {
                return p;
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t count() const noexcept { return nuclei_.size(); }

private:
    NerveBus& bus_;
    std::vector<std::unique_ptr<NeuralNucleus>> nuclei_;
};

} // namespace opencog::nerv
