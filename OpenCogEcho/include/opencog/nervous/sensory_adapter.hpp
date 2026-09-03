#pragma once
/**
 * @file sensory_adapter.hpp
 * @brief SensoryTouchpadAdapter — Bidirectional coupling to VirtualTouchpad
 *
 * READ: SOMATOSENSORY → manifold sensitivity, curvature parameters
 * WRITE: Gesture events → GESTURAL_AFFERENT, contact state → SOMATOSENSORY
 *
 * Uses the abstract TouchpadInterface from the endocrine system for decoupling.
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/endocrine/touchpad_types.hpp>

namespace opencog::nerv {

class SensoryTouchpadAdapter : public NeuralConnector {
public:
    SensoryTouchpadAdapter(NerveBus& bus, endo::TouchpadInterface& touchpad)
        : NeuralConnector(bus)
        , touchpad_(touchpad)
    {
        base_config_ = touchpad_.current_config();
        prev_somatosensory_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        endo::TouchpadEndocrineConfig cfg = base_config_;

        float somato = bus.activation(NeuralChannelId::SOMATOSENSORY);
        float gesture_emit = bus.activation(NeuralChannelId::MOTOR_GESTURE_EMIT);
        float gate = bus.activation(NeuralChannelId::THALAMIC_GATE);

        // Somatosensory activation → manifold sensitivity
        float somato_delta = rising_edge(somato, prev_somatosensory_, 0.03f);
        cfg.sensitivity = std::clamp(
            base_config_.sensitivity +
            std::abs(somato) * 0.3f + somato_delta * 0.2f,
            0.0f, 1.0f);

        // Thalamic gate → complexity threshold (open gate = accept more complex gestures)
        cfg.gesture_complexity = std::clamp(
            base_config_.gesture_complexity - std::abs(gate) * 0.2f,
            0.0f, 1.0f);

        // Gesture emit motor signal → emission intensity
        if (std::abs(gesture_emit) > 0.1f) {
            cfg.dimensionality_scale = std::clamp(
                base_config_.dimensionality_scale + std::abs(gesture_emit) * 0.4f,
                0.0f, 1.0f);
        }

        touchpad_.apply_config(cfg);
        prev_somatosensory_ = somato;
    }

    void write_feedback() override {
        auto tel = touchpad_.telemetry();

        // Active gestures → gestural afferent
        if (tel.active_contact_count > 0 && tel.active_contact_count != prev_contacts_) {
            float intensity = tel.active_contact_count /
                              static_cast<float>(endo::MAX_CONTACTS);
            bus_.fire(NeuralChannelId::GESTURAL_AFFERENT, intensity * 0.6f, 0.5f);
        }

        // Manifold coherence changes → somatosensory
        if (std::abs(tel.manifold_coherence - prev_curvature_) > 0.05f) {
            bus_.fire(NeuralChannelId::SOMATOSENSORY,
                      tel.manifold_coherence * 0.3f, 0.3f);
        }

        // Pattern novelty spike → novelty signal
        if (tel.pattern_novelty > prev_complexity_ + 0.1f) {
            bus_.fire(NeuralChannelId::NOVELTY_SIGNAL,
                      (tel.pattern_novelty - prev_complexity_) * 0.5f, 0.4f);
        }

        prev_contacts_ = tel.active_contact_count;
        prev_curvature_ = tel.manifold_coherence;
        prev_complexity_ = tel.pattern_novelty;
    }

private:
    endo::TouchpadInterface& touchpad_;
    endo::TouchpadEndocrineConfig base_config_;
    float prev_somatosensory_{0.0f};
    float prev_contacts_{0.0f};
    float prev_curvature_{0.0f};
    float prev_complexity_{0.0f};
};

} // namespace opencog::nerv
