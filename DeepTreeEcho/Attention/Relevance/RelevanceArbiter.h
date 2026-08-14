/**
 * @file RelevanceArbiter.h
 * @brief The RR arbitration layer between DTE's neural and symbolic halves.
 *
 * Composes the ported CogPrime RelevanceKernel with the modules integrated
 * in the 32-repo sweep, realizing "optimal grip" between perception and
 * action:
 *
 *   neural  (Gap A) — DTEReservoirCore state energy drives base salience
 *   somatic (Gap E) — EndocrineBus hormones modulate mode thresholds:
 *                     cortisol narrows attention, dopamine widens exploration
 *   symbolic(Gap B) — Cog0TaskSubsystem goal priorities bias salience toward
 *                     goal-relevant items
 *   convergence     — cog::grip GripDimensions track the 5-D cognitive grip
 *                     of the arbitration itself (vendored cogpy-hpp)
 *
 * The arbiter is header-only plain C++17 and dependency-injected: callers
 * provide the substrate signals via small samplers, so this compiles in the
 * standalone cmake build and can be wrapped by a UE subsystem later.
 */
#pragma once

#include "RelevanceKernel.h"
#include "SalienceLandscape.h"
#include "vendored/cog/grip/grip.hpp"

#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dte {
namespace rr {

/** Substrate signal samplers injected by the host (UE subsystem or tests). */
struct SubstrateSamplers {
    /** Per-item neural salience from reservoir state energy, in [0,1]. */
    std::function<double(const std::string& item)> reservoirSalience;
    /** Current hormone levels in [0,1]. */
    std::function<double()> cortisol;
    std::function<double()> dopamine;
    /** Per-item goal priority from the executive layer, in [0,1]. */
    std::function<double(const std::string& item)> goalPriority;
};

/**
 * RelevanceArbiter — full RR loop:
 *   sample substrates → base salience → cross-mode RR → ACT landscape →
 *   4-level cascade → action generation → grip telemetry.
 */
class RelevanceArbiter {
public:
    explicit RelevanceArbiter(SubstrateSamplers samplers,
                              double actionThreshold = 0.7)
        : samplers_(std::move(samplers)),
          landscape_(core_),
          actions_(actionThreshold),
          grip_(32)
    {
        // Neural channel: reservoir state energy (weight 1.0).
        if (samplers_.reservoirSalience) {
            core_.addProvider(
                "reservoir",
                [this](const std::string& item, const Context&) {
                    return samplers_.reservoirSalience(item);
                },
                1.0);
        }
        // Symbolic channel: executive goal priority (weight 1.0).
        if (samplers_.goalPriority) {
            core_.addProvider(
                "goals",
                [this](const std::string& item, const Context&) {
                    return samplers_.goalPriority(item);
                },
                1.0);
        }
    }

    RelevanceCore& core() { return core_; }
    SalienceLandscape& landscape() { return landscape_; }
    FrameStack& frames() { return frames_; }
    const cog::grip::GripDimensions& lastGrip() const { return lastGrip_; }

    /**
     * One arbitration cycle over candidate items with per-item features.
     * Returns the selected best action (if any) and updates grip telemetry.
     */
    std::optional<RRAction>
    arbitrate(const std::unordered_map<
                  std::string, std::unordered_map<std::string, double>>& items,
              const Context& ctx = {})
    {
        // 1) Somatic modulation first — hormones re-shape all thresholds.
        if (samplers_.cortisol && samplers_.dopamine) {
            core_.applyHormonalModulation(samplers_.cortisol(),
                                          samplers_.dopamine());
        }

        // 2) ACT landscape update per item.
        std::set<std::string> ids;
        for (const auto& kv : items) {
            landscape_.updateSalience(kv.first, kv.second, ctx);
            ids.insert(kv.first);
        }

        // 3) 4-level salience cascade; survivors are frame-relevant.
        std::set<std::string> framed = landscape_.cascade(ids);

        // 4) Score survivors for action generation: mean ACT salience,
        //    biased by substrate providers via a final RR pass.
        std::unordered_map<std::string, double> scores;
        auto [rrItems, rrConfidence] = core_.evaluateRelevance(
            framed.empty() ? ids : framed, ctx);
        for (const auto& id : (framed.empty() ? ids : framed)) {
            const double act = landscape_.get(id).mean();
            const double rrBoost = rrItems.count(id) ? 1.0 : 0.5;
            scores[id] = std::clamp(act * rrBoost, 0.0, 1.0);
        }

        // 5) Generate + select action against the current frame.
        CognitiveFrame frame;
        if (frames_.hasFrame()) frame = frames_.current();
        auto actions = actions_.generateActions(frame, scores, ctx);
        auto best = actions_.selectBestAction(actions);

        // 6) Grip telemetry: relevance-realization quality feeds the 5-D
        //    cognitive grip (composability=coverage, differentiability from
        //    reservoir+constraint channels, executability=action produced,
        //    self-awareness via frame depth, convergence via confidence).
        const double coverage =
            ids.empty() ? 0.0
                        : static_cast<double>(framed.size()) /
                              static_cast<double>(ids.size());
        grip_.set_composability(static_cast<float>(
            std::clamp(0.5 + 0.5 * coverage, 0.0, 1.0)));
        grip_.update_differentiability(
            static_cast<float>(rrConfidence),
            static_cast<float>(best ? best->confidence : 0.0));
        lastGrip_ = grip_.compute_grip();

        lastScores_ = std::move(scores);
        return best;
    }

    const std::unordered_map<std::string, double>& lastScores() const
    {
        return lastScores_;
    }

    /**
     * Apply an insight event: restructure the salience landscape for a mode
     * (threshold/interaction modulation) and optionally shift the frame.
     */
    void insight(RelevanceMode mode, double thresholdMod,
                 double interactionMod,
                 std::optional<CognitiveFrame> newFrame = std::nullopt)
    {
        Context c;
        c.set("threshold_mod", thresholdMod);
        c.set("interaction_mod", interactionMod);
        core_.restructureSalience(mode, c);
        if (newFrame) frames_.shiftFrame(std::move(*newFrame));
    }

private:
    SubstrateSamplers samplers_;
    RelevanceCore core_;
    SalienceLandscape landscape_;
    FrameStack frames_;
    ActionGenerator actions_;
    cog::grip::CognitiveGrip grip_;
    cog::grip::GripDimensions lastGrip_{};
    std::unordered_map<std::string, double> lastScores_;
};

} // namespace rr
} // namespace dte
