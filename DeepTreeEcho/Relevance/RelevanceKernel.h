/**
 * @file RelevanceKernel.h
 * @brief Relevance Realization kernel for Deep Tree Echo ("rr.kern").
 *
 * Port of the CogPrime relevance realization core (drzo/cogprime,
 * src/core/relevance_core.py + src/core/vervaeke_cognitive_core.py +
 * src/action/relevance_action.py) into plain C++17 for the un9n/u9n
 * standalone builds, implementing Vervaeke's RR framework:
 *
 *  - Five interacting relevance modes with cross-mode salience modulation
 *    and per-mode thresholds (RelevanceCore).
 *  - Cognitive frames with four ways of knowing and frame-shift (insight)
 *    dynamics (CognitiveCore).
 *  - RR-driven action generation: ATTEND / FRAME_SHIFT / EXPLORE /
 *    EXPLOIT / REFLECT with exploration-exploitation opponent processing
 *    (ActionGenerator).
 *
 * The Python original computed base salience with a random placeholder.
 * This port replaces it with pluggable salience providers so the kernel
 * arbitrates between DTE's neural and symbolic subsystems:
 *
 *  - neural  : reservoir state energy (Inference/DTEReservoirCore)
 *  - somatic : endocrine modulation   (Avatar/EndocrineBus) — cortisol
 *              narrows attention (raises thresholds), dopamine widens
 *              exploration (lowers thresholds)
 *  - symbolic: goal priority          (Executive/Cog0TaskSubsystem)
 *
 * Provenance: drzo/cogprime @ main (vendored 2026-07-19); adapted for the
 * Deep Tree Echo integration (namespace dte::rr). Real implementation, no
 * mocks: all arithmetic preserved from the source where defined, and the
 * documented placeholder replaced with the provider composition above.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace dte {
namespace rr {

// ============================================================================
// Relevance modes (relevance_core.py RelevanceMode)
// ============================================================================

enum class RelevanceMode : uint8_t {
    SelectiveAttention = 0, // Bottom-up salience
    WorkingMemory      = 1, // Active maintenance
    ProblemSpace       = 2, // Search space navigation
    SideEffects        = 3, // Action consequences
    LongTermMemory     = 4  // Organization & access
};

static constexpr std::size_t kNumRelevanceModes = 5;

inline const char* toString(RelevanceMode m)
{
    switch (m) {
    case RelevanceMode::SelectiveAttention: return "selective_attention";
    case RelevanceMode::WorkingMemory:      return "working_memory";
    case RelevanceMode::ProblemSpace:       return "problem_space";
    case RelevanceMode::SideEffects:        return "side_effects";
    case RelevanceMode::LongTermMemory:     return "long_term_memory";
    }
    return "unknown";
}

// ============================================================================
// Ways of knowing (vervaeke_cognitive_core.py KnowingMode)
// ============================================================================

enum class KnowingMode : uint8_t {
    Propositional = 0, // Knowing that
    Procedural    = 1, // Knowing how
    Perspectival  = 2, // Knowing what it's like
    Participatory = 3  // Knowing by participating
};

// ============================================================================
// Context — key/value scalar context replacing the Python dicts
// ============================================================================

struct Context {
    std::unordered_map<std::string, double> values;

    double get(const std::string& key, double fallback = 0.0) const
    {
        auto it = values.find(key);
        return it != values.end() ? it->second : fallback;
    }
    bool has(const std::string& key) const { return values.count(key) > 0; }
    void set(const std::string& key, double v) { values[key] = v; }
};

// ============================================================================
// Salience providers — the DTE substrate hooks that replace the Python
// placeholder _compute_base_salience.
// ============================================================================

/**
 * A salience provider scores an item (by id) in a context, returning a
 * weight in [0,1]. Providers are combined by weighted average.
 */
using SalienceProvider =
    std::function<double(const std::string& item, const Context& ctx)>;

struct WeightedProvider {
    SalienceProvider fn;
    double weight = 1.0;
    std::string name;
};

// ============================================================================
// RelevanceCore — five-mode salience coordination (relevance_core.py)
// ============================================================================

class RelevanceCore {
public:
    RelevanceCore()
    {
        modeWeights_.fill(1.0);
        thresholds_.fill(0.5);
        for (auto& row : interaction_) row.fill(1.0);
    }

    /** Register a substrate salience provider (reservoir/endocrine/goal…). */
    void addProvider(const std::string& name, SalienceProvider fn,
                     double weight = 1.0)
    {
        providers_.push_back(WeightedProvider{std::move(fn), weight, name});
    }

    std::size_t providerCount() const { return providers_.size(); }

    /** Per-mode salience threshold (default 0.5, as in the source). */
    double threshold(RelevanceMode m) const
    {
        return thresholds_[static_cast<std::size_t>(m)];
    }
    void setThreshold(RelevanceMode m, double t)
    {
        thresholds_[static_cast<std::size_t>(m)] =
            std::clamp(t, 0.0, 1.0);
    }

    /** Cross-mode interaction weight (default 1.0). */
    double interactionWeight(RelevanceMode a, RelevanceMode b) const
    {
        return interaction_[static_cast<std::size_t>(a)]
                           [static_cast<std::size_t>(b)];
    }
    void setInteractionWeight(RelevanceMode a, RelevanceMode b, double w)
    {
        interaction_[static_cast<std::size_t>(a)]
                    [static_cast<std::size_t>(b)] = std::max(0.0, w);
    }

    const std::set<std::string>& activeContents(RelevanceMode m) const
    {
        return active_[static_cast<std::size_t>(m)];
    }

    /**
     * Update salience weights for contents in a mode and return the set of
     * items above the mode threshold (relevance_core.py update_salience).
     *
     * Base salience comes from the registered providers (weighted average),
     * then is modulated by every other mode's interaction weight scaled by
     * that mode's activity, then threshold-filtered.
     */
    std::set<std::string> updateSalience(RelevanceMode mode,
                                         const std::set<std::string>& contents,
                                         const Context& ctx = {})
    {
        std::unordered_map<std::string, double> salience =
            computeBaseSalience(contents, ctx);

        // Cross-mode modulation: other modes with active contents modulate
        // salience by their interaction weight (source _modulate_salience
        // multiplied by interaction_weight; activity scaling makes an idle
        // mode neutral rather than damping, preserving multiplicative form).
        const std::size_t mi = static_cast<std::size_t>(mode);
        for (std::size_t oi = 0; oi < kNumRelevanceModes; ++oi) {
            if (oi == mi) continue;
            if (active_[oi].empty()) continue; // idle mode → neutral
            const double w = interaction_[mi][oi];
            for (auto& kv : salience) {
                // Shared items get reinforced by cross-mode resonance.
                const bool shared = active_[oi].count(kv.first) > 0;
                const double factor = shared ? w : std::min(1.0, w);
                kv.second = std::clamp(kv.second * factor, 0.0, 1.0);
            }
        }

        const double thr = thresholds_[mi];
        std::set<std::string> salient;
        for (const auto& kv : salience) {
            if (kv.second >= thr) salient.insert(kv.first);
        }
        active_[mi] = salient;
        return salient;
    }

    /**
     * Dynamically restructure the salience landscape — the insight /
     * reframing mechanism (relevance_core.py restructure_salience).
     * Context keys: "threshold_mod", "interaction_mod" (defaults 1.0).
     */
    void restructureSalience(RelevanceMode mode, const Context& newContext)
    {
        const std::size_t mi = static_cast<std::size_t>(mode);
        thresholds_[mi] = std::clamp(
            thresholds_[mi] * newContext.get("threshold_mod", 1.0), 0.0, 1.0);
        const double imod = newContext.get("interaction_mod", 1.0);
        for (std::size_t oi = 0; oi < kNumRelevanceModes; ++oi) {
            if (oi == mi) continue;
            interaction_[mi][oi] = std::max(0.0, interaction_[mi][oi] * imod);
        }
        // Re-evaluate active contents under the new parameters.
        updateSalience(mode, active_[mi], newContext);
    }

    /**
     * Evaluate relevance of query items across all modes; returns the union
     * of salient items and a mean-coverage confidence in [0,1]
     * (relevance_core.py evaluate_relevance).
     */
    std::pair<std::set<std::string>, double>
    evaluateRelevance(const std::set<std::string>& query, const Context& ctx)
    {
        std::set<std::string> relevant;
        double totalConfidence = 0.0;
        if (query.empty()) return {relevant, 0.0};

        for (std::size_t mi = 0; mi < kNumRelevanceModes; ++mi) {
            auto salient = updateSalience(
                static_cast<RelevanceMode>(mi), query, ctx);
            relevant.insert(salient.begin(), salient.end());
            totalConfidence +=
                static_cast<double>(salient.size()) /
                static_cast<double>(query.size());
        }
        return {relevant,
                totalConfidence / static_cast<double>(kNumRelevanceModes)};
    }

    /**
     * Endocrine modulation hook (Gap E wiring): cortisol narrows attention
     * by raising thresholds; dopamine widens exploration by lowering them.
     * Deltas are relative to the 0.5 baseline and bounded.
     */
    void applyHormonalModulation(double cortisol01, double dopamine01)
    {
        const double narrow = std::clamp(cortisol01, 0.0, 1.0) - 0.5;
        const double widen  = std::clamp(dopamine01, 0.0, 1.0) - 0.5;
        // Net threshold shift: +0.2 max narrowing, -0.2 max widening.
        const double shift = 0.4 * (narrow - widen) / 2.0;
        for (std::size_t mi = 0; mi < kNumRelevanceModes; ++mi) {
            thresholds_[mi] = std::clamp(thresholds_[mi] + shift, 0.05, 0.95);
        }
    }

private:
    std::unordered_map<std::string, double>
    computeBaseSalience(const std::set<std::string>& contents,
                        const Context& ctx) const
    {
        std::unordered_map<std::string, double> out;
        if (providers_.empty()) {
            // No substrate attached: neutral salience at threshold midpoint
            // (deterministic replacement for the Python random placeholder).
            for (const auto& item : contents) out[item] = 0.5;
            return out;
        }
        double wsum = 0.0;
        for (const auto& p : providers_) wsum += p.weight;
        for (const auto& item : contents) {
            double acc = 0.0;
            for (const auto& p : providers_) {
                acc += p.weight * std::clamp(p.fn(item, ctx), 0.0, 1.0);
            }
            out[item] = wsum > 0.0 ? acc / wsum : 0.0;
        }
        return out;
    }

    std::array<double, kNumRelevanceModes> modeWeights_{};
    std::array<double, kNumRelevanceModes> thresholds_{};
    std::array<std::array<double, kNumRelevanceModes>, kNumRelevanceModes>
        interaction_{};
    std::array<std::set<std::string>, kNumRelevanceModes> active_{};
    std::vector<WeightedProvider> providers_;
};

// ============================================================================
// CognitiveFrame + frame stack (vervaeke_cognitive_core.py)
// ============================================================================

struct CognitiveFrame {
    std::unordered_map<std::string, double> salienceWeights;
    std::vector<KnowingMode> activeKnowingModes;
    Context context;
};

class FrameStack {
public:
    bool hasFrame() const { return current_.has_value(); }
    const CognitiveFrame& current() const { return *current_; }
    CognitiveFrame& current() { return *current_; }
    std::size_t depth() const { return history_.size(); }

    /** Shift to a new frame, pushing the current one (shift_frame). */
    void shiftFrame(CognitiveFrame newFrame)
    {
        if (current_) history_.push_back(*current_);
        current_ = std::move(newFrame);
    }

    /** Pop back to the previous frame; returns false at stack bottom. */
    bool popFrame()
    {
        if (history_.empty()) return false;
        current_ = history_.back();
        history_.pop_back();
        return true;
    }

    void addKnowingMode(KnowingMode m)
    {
        if (current_) current_->activeKnowingModes.push_back(m);
    }

    /**
     * Frame-relative relevance realization (realize_relevance): scores
     * inputs against the current frame's salience weights, modulated by
     * context match (match ×2.0, mismatch ×0.5), threshold-filtered.
     */
    std::unordered_map<std::string, double>
    realizeRelevance(const std::unordered_map<std::string, double>& inputs,
                     double salienceThreshold = 0.5) const
    {
        std::unordered_map<std::string, double> out;
        if (!current_) return out;
        for (const auto& kv : inputs) {
            auto it = current_->salienceWeights.find(kv.first);
            if (it == current_->salienceWeights.end()) continue;
            double contextFactor = 1.0;
            if (current_->context.has(kv.first)) {
                contextFactor =
                    std::abs(current_->context.get(kv.first) - kv.second) <
                            1e-9
                        ? 2.0
                        : 0.5;
            }
            const double score = it->second * contextFactor;
            if (score >= salienceThreshold) out[kv.first] = score;
        }
        return out;
    }

private:
    std::optional<CognitiveFrame> current_;
    std::vector<CognitiveFrame> history_;
};

// ============================================================================
// RR-driven action generation (relevance_action.py)
// ============================================================================

enum class RRActionType : uint8_t {
    Attend = 0,    // Shift attention
    FrameShift,    // Change cognitive frame
    Explore,       // Gather new information
    Exploit,       // Use existing knowledge
    Reflect        // Meta-cognitive action
};

struct RRAction {
    RRActionType type = RRActionType::Attend;
    std::vector<std::string> targets;
    std::vector<KnowingMode> knowingModes;
    double expectedRelevance = 0.0;
    double confidence = 0.0;
};

class ActionGenerator {
public:
    explicit ActionGenerator(double relevanceThreshold = 0.7)
        : relevanceThreshold_(relevanceThreshold)
    {
    }

    std::vector<RRAction>
    generateActions(const CognitiveFrame& currentFrame,
                    const std::unordered_map<std::string, double>& scores,
                    const Context& ctx) const
    {
        std::vector<RRAction> actions;

        // ATTEND for highly relevant items.
        for (const auto& kv : scores) {
            if (kv.second >= relevanceThreshold_) {
                RRAction a;
                a.type = RRActionType::Attend;
                a.targets = {kv.first};
                a.knowingModes = {KnowingMode::Perspectival};
                a.expectedRelevance = kv.second;
                a.confidence = std::min(1.0, kv.second);
                actions.push_back(std::move(a));
            }
        }

        // FRAME_SHIFT when context diverges from the frame (<50% overlap).
        if (shouldShiftFrame(currentFrame, ctx)) {
            RRAction a;
            a.type = RRActionType::FrameShift;
            a.knowingModes = {KnowingMode::Participatory};
            a.expectedRelevance =
                std::min(1.0, static_cast<double>(ctx.values.size()) / 10.0);
            a.confidence = 0.8;
            actions.push_back(std::move(a));
        }

        // EXPLORE when mean relevance is low (<0.4) or nothing scored.
        if (shouldExplore(scores)) {
            RRAction a;
            a.type = RRActionType::Explore;
            for (const auto& kv : scores)
                if (kv.second < 0.3) a.targets.push_back(kv.first);
            a.knowingModes = {KnowingMode::Procedural};
            a.expectedRelevance = 0.5;
            a.confidence = 0.6;
            actions.push_back(std::move(a));
        }

        // EXPLOIT when any item scores > 0.8.
        if (shouldExploit(scores)) {
            RRAction a;
            a.type = RRActionType::Exploit;
            for (const auto& kv : scores)
                if (kv.second > 0.8) a.targets.push_back(kv.first);
            a.knowingModes = {KnowingMode::Propositional,
                              KnowingMode::Procedural};
            a.expectedRelevance = 0.9;
            a.confidence = 0.9;
            actions.push_back(std::move(a));
        }

        return actions;
    }

    /** Best action by 0.7·relevance + 0.3·confidence (select_best_action). */
    std::optional<RRAction>
    selectBestAction(const std::vector<RRAction>& actions) const
    {
        if (actions.empty()) return std::nullopt;
        auto score = [](const RRAction& a) {
            return 0.7 * a.expectedRelevance + 0.3 * a.confidence;
        };
        const RRAction* best = &actions.front();
        for (const auto& a : actions)
            if (score(a) > score(*best)) best = &a;
        return *best;
    }

    double relevanceThreshold() const { return relevanceThreshold_; }

private:
    static bool shouldShiftFrame(const CognitiveFrame& frame,
                                 const Context& ctx)
    {
        if (frame.context.values.empty() && ctx.values.empty()) return false;
        std::size_t overlap = 0;
        for (const auto& kv : frame.context.values)
            if (ctx.values.count(kv.first)) ++overlap;
        const double denom = static_cast<double>(
            std::max(frame.context.values.size(), ctx.values.size()));
        return denom > 0.0 &&
               (static_cast<double>(overlap) / denom) < 0.5;
    }

    static bool
    shouldExplore(const std::unordered_map<std::string, double>& scores)
    {
        if (scores.empty()) return true;
        double sum = 0.0;
        for (const auto& kv : scores) sum += kv.second;
        return (sum / static_cast<double>(scores.size())) < 0.4;
    }

    static bool
    shouldExploit(const std::unordered_map<std::string, double>& scores)
    {
        for (const auto& kv : scores)
            if (kv.second > 0.8) return true;
        return false;
    }

    double relevanceThreshold_;
};

} // namespace rr
} // namespace dte
