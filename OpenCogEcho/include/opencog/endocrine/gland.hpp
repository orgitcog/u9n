#pragma once
/**
 * @file gland.hpp
 * @brief Virtual gland base class and gland registry
 *
 * Each virtual gland is an independent signal producer that reads
 * stimulators/inhibitors from the hormone bus, computes a production
 * rate with feedback regulation, and emits its output hormone.
 */

#include <opencog/endocrine/hormone_bus.hpp>

#include <concepts>
#include <memory>
#include <vector>

namespace opencog::endo {

// ============================================================================
// VirtualGland — Abstract base for all glands
// ============================================================================

class VirtualGland {
public:
    explicit VirtualGland(HormoneBus& bus, GlandConfig config)
        : bus_(bus)
        , config_(std::move(config))
        , allostatic_setpoint_(config_.homeostatic_setpoint)
    {}

    virtual ~VirtualGland() = default;

    VirtualGland(const VirtualGland&) = delete;
    VirtualGland& operator=(const VirtualGland&) = delete;

    /// Advance one time step: compute production, apply feedback, emit
    virtual void update(float dt) = 0;

    /// Compute current production rate from stimulators/inhibitors
    virtual float compute_production_rate() const = 0;

    // === Accessors ===

    [[nodiscard]] const GlandConfig& config() const noexcept { return config_; }
    [[nodiscard]] HormoneId output_channel() const noexcept { return config_.output_channel; }
    [[nodiscard]] float current_production_rate() const noexcept { return current_rate_; }
    [[nodiscard]] float homeostatic_setpoint() const noexcept { return allostatic_setpoint_; }

    // === Allostatic Shifting ===

    /// Shift the setpoint (chronic load causes drift)
    void shift_setpoint(float delta) noexcept {
        allostatic_setpoint_ = std::clamp(
            allostatic_setpoint_ + delta, 0.0f, 1.0f
        );
    }

    // === Callbacks ===

    void on_pulse(std::function<void(HormoneId, float)> cb) {
        pulse_cb_ = std::move(cb);
    }

protected:
    HormoneBus& bus_;
    GlandConfig config_;
    float current_rate_{0.0f};
    float allostatic_setpoint_;

    /// Sum stimulatory input from bus
    [[nodiscard]] float read_stimulation() const {
        float stim = 0.0f;
        for (auto& [channel, sensitivity] : config_.stimulators) {
            stim += bus_.concentration(channel) * sensitivity;
        }
        return stim;
    }

    /// Sum inhibitory input from bus
    [[nodiscard]] float read_inhibition() const {
        float inhib = 0.0f;
        for (auto& [channel, sensitivity] : config_.inhibitors) {
            inhib += bus_.concentration(channel) * sensitivity;
        }
        return inhib;
    }

    /// Apply negative/positive feedback based on own output level
    [[nodiscard]] float apply_feedback(float raw_rate) const {
        float output = bus_.concentration(config_.output_channel);
        float rate = raw_rate;

        // Negative feedback: high output suppresses production
        if (config_.negative_feedback) {
            float error = output - allostatic_setpoint_;
            if (error > 0.0f) {
                rate *= std::max(0.0f, 1.0f - error * config_.feedback_gain);
            }
        }

        // Positive feedback: high output amplifies production (rare)
        if (config_.positive_feedback) {
            rate *= 1.0f + output * config_.positive_feedback_gain;
        }

        return std::clamp(rate, 0.0f, config_.max_production_rate);
    }

    /// Write production to bus and fire pulse callback
    void emit(float amount) {
        if (amount > 0.0f) {
            bus_.produce(config_.output_channel, amount);
            if (pulse_cb_) {
                pulse_cb_(config_.output_channel, amount);
            }
        }
    }

    /// Update allostatic setpoint toward current output (chronic drift)
    void update_allostasis(float dt) {
        float output = bus_.concentration(config_.output_channel);
        float drift = (output - allostatic_setpoint_) * config_.allostatic_rate * dt;
        allostatic_setpoint_ = std::clamp(allostatic_setpoint_ + drift, 0.0f, 1.0f);
    }

    std::function<void(HormoneId, float)> pulse_cb_;
};

// ============================================================================
// SimpleGland — Default implementation for basic tonic glands
// ============================================================================

/**
 * @brief A straightforward gland that produces based on stimulation - inhibition
 *        with feedback and allostatic drift. Suitable for most glands.
 */
class SimpleGland : public VirtualGland {
public:
    using VirtualGland::VirtualGland;

    float compute_production_rate() const override {
        float stim = read_stimulation();
        float inhib = read_inhibition();
        float raw = config_.base_production_rate + stim - inhib;
        raw = std::max(0.0f, raw);
        return apply_feedback(raw);
    }

    void update(float dt) override {
        current_rate_ = compute_production_rate();
        emit(current_rate_ * dt);
        update_allostasis(dt);
    }
};

// ============================================================================
// GlandRegistry — Owns and updates all glands
// ============================================================================

class GlandRegistry {
public:
    explicit GlandRegistry(HormoneBus& bus) : bus_(bus) {}

    /// Register all default biological glands
    void register_defaults();

    /// Add a custom gland and return a reference to it
    template<std::derived_from<VirtualGland> G, typename... Args>
    G& add_gland(Args&&... args) {
        auto ptr = std::make_unique<G>(bus_, std::forward<Args>(args)...);
        G& ref = *ptr;
        glands_.push_back(std::move(ptr));
        return ref;
    }

    /// Update all glands by dt
    void update_all(float dt) {
        for (auto& gland : glands_) {
            gland->update(dt);
        }
    }

    /// Find a gland by type (returns nullptr if not found)
    template<std::derived_from<VirtualGland> G>
    [[nodiscard]] G* get_gland() noexcept {
        for (auto& gland : glands_) {
            if (auto* p = dynamic_cast<G*>(gland.get())) {
                return p;
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t count() const noexcept { return glands_.size(); }

private:
    HormoneBus& bus_;
    std::vector<std::unique_ptr<VirtualGland>> glands_;
};

} // namespace opencog::endo
