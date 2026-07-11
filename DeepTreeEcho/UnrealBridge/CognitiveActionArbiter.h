// Copyright Deep Tree Echo. All Rights Reserved.
#pragma once

/**
 * CognitiveActionArbiter
 *
 * Pure C++ (Eigen-only, zero UE dependencies) decision layer that maps a
 * perception salience vector produced by UDeepTreeEchoCore onto a compact
 * action output vector consumed by ADeepTreeEchoAIController.
 *
 * Action vector layout (always length >= 2):
 *   [0] movement_urgency  — 0..1, drives MoveToActor when above threshold
 *   [1] focus_urgency     — 0..1, drives SetFocus when above threshold
 *   [2] best_target_index — float-encoded index into the salience vector of the
 *                           most salient perceived actor (-1 = none)
 *
 * The arbiter is intentionally stateless and side-effect-free so it is trivial
 * to test in isolation.
 *
 * Design notes:
 *   - CognitiveModeWeight scales how much the current cognitive mode biases the
 *     action.  In Reactive mode the system responds aggressively to high
 *     salience; in Reflective mode it is more conservative.
 *   - All thresholds are configurable so the controller layer can tune them per
 *     game context without touching core logic.
 */

#include <Eigen/Core>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace DeepTreeEcho {

/**
 * Cognitive mode enum — mirrors ECognitiveMode from DeepTreeEchoCore.h but
 * without UE header dependencies so this file compiles standalone.
 */
enum class CognitiveMode : uint8_t
{
    Reactive     = 0,
    Deliberative = 1,
    Reflective   = 2,
    Creative     = 3,
    Integrative  = 4
};

/**
 * Configuration knobs for the arbiter.
 */
struct ArbiterConfig
{
    /** Salience level above which movement is triggered. */
    float movement_threshold = 0.6f;

    /** Salience level above which focus is re-acquired. */
    float focus_threshold = 0.4f;

    /**
     * Mode-specific bias on the movement urgency:
     *   Reactive     -> 1.0  (full response)
     *   Deliberative -> 0.8
     *   Reflective   -> 0.5
     *   Creative     -> 0.6
     *   Integrative  -> 0.7
     */
    float mode_movement_bias(CognitiveMode mode) const
    {
        switch (mode)
        {
            case CognitiveMode::Reactive:     return 1.0f;
            case CognitiveMode::Deliberative: return 0.8f;
            case CognitiveMode::Reflective:   return 0.5f;
            case CognitiveMode::Creative:     return 0.6f;
            case CognitiveMode::Integrative:  return 0.7f;
            default:                          return 1.0f;
        }
    }
};

/**
 * CognitiveActionArbiter
 *
 * Stateless mapping from salience vector + cognitive mode to action vector.
 */
class CognitiveActionArbiter
{
public:
    explicit CognitiveActionArbiter(const ArbiterConfig& cfg = ArbiterConfig{})
        : config_(cfg)
    {}

    /**
     * Compute an action vector from the current salience input.
     *
     * @param salience_vec   Per-perceived-actor salience values in [0, 1].
     * @param mode           Current cognitive mode.
     * @return               Action vector of length 3 (see class doc).
     */
    Eigen::VectorXf compute(const std::vector<float>& salience_vec,
                            CognitiveMode mode = CognitiveMode::Reactive) const
    {
        Eigen::VectorXf action(3);
        action.setZero();

        if (salience_vec.empty())
        {
            action[2] = -1.0f;  // No target
            return action;
        }

        // Find the most salient actor
        auto it = std::max_element(salience_vec.begin(), salience_vec.end());
        const int best_idx = static_cast<int>(it - salience_vec.begin());
        const float peak_salience = *it;

        // Movement urgency: peak salience × mode bias, clamped to [0, 1]
        const float bias = config_.mode_movement_bias(mode);
        const float raw_movement = peak_salience * bias;
        action[0] = std::min(std::max(raw_movement, 0.0f), 1.0f);

        // Focus urgency: slightly lower than movement so focus shifts first
        const float focus_gain = 0.9f;  // Focus response is slightly softer
        action[1] = std::min(std::max(peak_salience * focus_gain, 0.0f), 1.0f);

        // Target index (float-encoded); -1 when below movement threshold
        action[2] = (action[0] >= config_.movement_threshold)
                    ? static_cast<float>(best_idx)
                    : -1.0f;

        return action;
    }

    /**
     * Convenience overload accepting an Eigen vector.
     */
    Eigen::VectorXf compute(const Eigen::VectorXf& salience_vec,
                            CognitiveMode mode = CognitiveMode::Reactive) const
    {
        std::vector<float> sv(salience_vec.data(),
                               salience_vec.data() + salience_vec.size());
        return compute(sv, mode);
    }

    /**
     * Return true when the action vector would trigger a movement command.
     */
    static bool should_move(const Eigen::VectorXf& action,
                            float threshold = 0.6f)
    {
        return action.size() >= 1 && action[0] >= threshold;
    }

    /**
     * Return true when the action vector would trigger a focus shift.
     */
    static bool should_focus(const Eigen::VectorXf& action,
                             float threshold = 0.4f)
    {
        return action.size() >= 2 && action[1] >= threshold;
    }

    /**
     * Return the encoded target index, or -1 when none.
     */
    static int target_index(const Eigen::VectorXf& action)
    {
        if (action.size() < 3) return -1;
        const float raw = action[2];
        if (raw < 0.0f) return -1;
        return static_cast<int>(std::round(raw));
    }

    const ArbiterConfig& config() const { return config_; }

private:
    ArbiterConfig config_;
};

}  // namespace DeepTreeEcho
