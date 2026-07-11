#pragma once
/**
 * @file connector.hpp
 * @brief Neural connector interface and base class for subsystem adapters
 *
 * Provides the NeuralConnector base class for bidirectional adapters that
 * couple VNS neural signals to cognitive subsystems.
 *
 * Key difference from EndocrineConnector:
 * - Neural adapters are delta-triggered (respond to signal edges/transients)
 * - VES adapters respond to sustained concentration levels
 * - Neural adapters have both read_signals() and write_feedback() paths
 */

#include <opencog/nervous/nerve_bus.hpp>

#include <functional>
#include <vector>

namespace opencog::nerv {

// ============================================================================
// NeuralConnector — Base for any subsystem coupled to the NerveBus
// ============================================================================

class NeuralConnector {
public:
    explicit NeuralConnector(NerveBus& bus) : bus_(bus) {}
    virtual ~NeuralConnector() = default;

    NeuralConnector(const NeuralConnector&) = delete;
    NeuralConnector& operator=(const NeuralConnector&) = delete;

    /// READ path: neural signals → modulate subsystem parameters
    virtual void read_signals(const NerveBus& bus) = 0;

    /// WRITE path: subsystem telemetry → fire signals back to bus
    virtual void write_feedback() = 0;

    /// Read current bus state snapshot
    [[nodiscard]] NeuralState read_bus() const noexcept {
        return bus_.snapshot();
    }

    /// Current processing level
    [[nodiscard]] ProcessingLevel current_level() const noexcept {
        return bus_.current_level();
    }

    /// Get a specific channel activation
    [[nodiscard]] float signal(NeuralChannelId id) const noexcept {
        return bus_.activation(id);
    }

    /// Fire a signal to the bus (convenience for write path)
    void fire_signal(NeuralChannelId id, float activation, float urgency = 0.5f) {
        bus_.fire(id, activation, urgency);
    }

protected:
    NerveBus& bus_;

    /// Compute delta between current and previous value (edge detection)
    [[nodiscard]] static float delta(float current, float previous) noexcept {
        return current - previous;
    }

    /// Rising edge detection: returns magnitude if rising above threshold
    [[nodiscard]] static float rising_edge(float current, float previous,
                                            float threshold = 0.05f) noexcept {
        float d = current - previous;
        return d > threshold ? d : 0.0f;
    }

    /// Falling edge detection
    [[nodiscard]] static float falling_edge(float current, float previous,
                                             float threshold = 0.05f) noexcept {
        float d = previous - current;
        return d > threshold ? d : 0.0f;
    }
};

} // namespace opencog::nerv
