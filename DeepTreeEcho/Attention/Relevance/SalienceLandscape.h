/**
 * @file SalienceLandscape.h
 * @brief Dynamic salience landscape on the ACT framework (Vervaeke).
 *
 * Port of drzo/cogprime src/cognitive_science/salience_landscape.py into
 * plain C++17 for un9n/u9n. Implements the three core salience dimensions —
 * Aspectuality (how something is configured), Centrality (importance to the
 * agent), Temporality (temporal relevance / kairos) — and the four-level
 * salience processing pipeline: FEATURIZATION → FOREGROUNDING → FIGURATION
 * → FRAMING. All thresholds and arithmetic preserved from the source.
 *
 * Wraps dte::rr::RelevanceCore (RelevanceKernel.h) so the landscape shares
 * the same substrate salience providers (reservoir / endocrine / goals).
 *
 * Provenance: drzo/cogprime @ main (vendored 2026-07-19), namespace dte::rr.
 */
#pragma once

#include "RelevanceKernel.h"

#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

namespace dte {
namespace rr {

// ============================================================================
// Processing levels
// ============================================================================

enum class SalienceLevel : uint8_t {
    Featurization = 0, // Basic feature detection
    Foregrounding = 1, // Figure-ground separation
    Figuration    = 2, // Pattern configuration
    Framing       = 3  // Problem/context framing
};

// ============================================================================
// ACT salience vector
// ============================================================================

struct SalienceVector {
    double aspectuality = 0.0; // How something is aspectualized/configured
    double centrality   = 0.0; // How central/important to the agent
    double temporality  = 0.0; // Temporal relevance/urgency (kairos)

    double mean() const
    {
        return (aspectuality + centrality + temporality) / 3.0;
    }
};

// ============================================================================
// SalienceLandscape
// ============================================================================

class SalienceLandscape {
public:
    explicit SalienceLandscape(RelevanceCore& core) : core_(core) {}

    /**
     * Update the salience vector for an item from its features and context
     * (salience_landscape.py update_salience). Features map feature-name →
     * strength in [0,1]. Context keys used: "agent_needs::<feature>" flags
     * (any feature also listed as an agent need contributes to centrality)
     * and "temporal_horizon" for temporality.
     */
    SalienceVector
    updateSalience(const std::string& itemId,
                   const std::unordered_map<std::string, double>& features,
                   const Context& ctx = {})
    {
        // Relevance evaluation over the feature names.
        std::set<std::string> featureNames;
        for (const auto& kv : features) featureNames.insert(kv.first);
        auto [relevantFeatures, confidence] =
            core_.evaluateRelevance(featureNames, ctx);
        (void)confidence;

        SalienceVector v;
        v.aspectuality = computeAspectuality(features, relevantFeatures);
        v.centrality   = computeCentrality(features, relevantFeatures, ctx);
        v.temporality  = computeTemporality(features, relevantFeatures, ctx);
        landscape_[itemId] = v;
        return v;
    }

    /**
     * Process items at a given salience level; returns the surviving set
     * (salience_landscape.py process_level). Level thresholds preserved:
     * featurization aspectuality>0.3; foregrounding centrality>0.5;
     * figuration aspectuality>0.6 && centrality>0.4; framing mean>0.7.
     */
    std::set<std::string> processLevel(SalienceLevel level,
                                       const std::set<std::string>& items) const
    {
        std::set<std::string> passed;
        for (const auto& item : items) {
            auto it = landscape_.find(item);
            if (it == landscape_.end()) continue;
            const SalienceVector& v = it->second;
            bool ok = false;
            switch (level) {
            case SalienceLevel::Featurization:
                ok = v.aspectuality > 0.3;
                break;
            case SalienceLevel::Foregrounding:
                ok = v.centrality > 0.5;
                break;
            case SalienceLevel::Figuration:
                ok = v.aspectuality > 0.6 && v.centrality > 0.4;
                break;
            case SalienceLevel::Framing:
                ok = v.mean() > 0.7;
                break;
            }
            if (ok) passed.insert(item);
        }
        return passed;
    }

    /** Run the full 4-level cascade: featurization → … → framing. */
    std::set<std::string> cascade(const std::set<std::string>& items) const
    {
        std::set<std::string> s = items;
        s = processLevel(SalienceLevel::Featurization, s);
        s = processLevel(SalienceLevel::Foregrounding, s);
        s = processLevel(SalienceLevel::Figuration, s);
        s = processLevel(SalienceLevel::Framing, s);
        return s;
    }

    const std::unordered_map<std::string, SalienceVector>& landscape() const
    {
        return landscape_;
    }

    bool has(const std::string& itemId) const
    {
        return landscape_.count(itemId) > 0;
    }

    SalienceVector get(const std::string& itemId) const
    {
        auto it = landscape_.find(itemId);
        return it != landscape_.end() ? it->second : SalienceVector{};
    }

private:
    // aspectuality = (feature coherence + distinctiveness) / 2
    static double
    computeAspectuality(const std::unordered_map<std::string, double>& features,
                        const std::set<std::string>& relevant)
    {
        if (features.empty()) return 0.0;
        const double coherence = static_cast<double>(relevant.size()) /
                                 static_cast<double>(features.size());
        double distinctiveness = 0.0;
        if (!relevant.empty()) {
            for (const auto& f : relevant) {
                auto it = features.find(f);
                distinctiveness += it != features.end() ? it->second : 0.0;
            }
            distinctiveness /= static_cast<double>(relevant.size());
        }
        return (coherence + distinctiveness) / 2.0;
    }

    // centrality = base importance, averaged with agent-needs alignment
    static double
    computeCentrality(const std::unordered_map<std::string, double>& features,
                      const std::set<std::string>& relevant,
                      const Context& ctx)
    {
        if (features.empty()) return 0.0;
        double importance = static_cast<double>(relevant.size()) /
                            static_cast<double>(features.size());
        // Agent needs are flagged in context as "agent_needs::<feature>".
        double needsSum = 0.0;
        std::size_t needsCount = 0;
        for (const auto& kv : ctx.values) {
            constexpr const char* prefix = "agent_needs::";
            if (kv.first.rfind(prefix, 0) == 0) {
                const std::string need = kv.first.substr(13);
                auto it = features.find(need);
                needsSum += it != features.end() ? it->second : 0.0;
                ++needsCount;
            }
        }
        if (needsCount > 0) {
            importance =
                (importance + needsSum / static_cast<double>(needsCount)) /
                2.0;
        }
        return importance;
    }

    // temporality = |temporal features| / (horizon + 1), else neutral 0.5
    static double
    computeTemporality(const std::unordered_map<std::string, double>& features,
                       const std::set<std::string>& relevant,
                       const Context& ctx)
    {
        (void)features;
        double temporal = 0.5; // neutral by default
        if (ctx.has("temporal_horizon")) {
            const double horizon = ctx.get("temporal_horizon");
            std::size_t temporalFeatures = 0;
            for (const auto& f : relevant) {
                if (f.find("time") != std::string::npos ||
                    f.find("urgent") != std::string::npos) {
                    ++temporalFeatures;
                }
            }
            temporal = static_cast<double>(temporalFeatures) / (horizon + 1.0);
        }
        return std::clamp(temporal, 0.0, 1.0);
    }

    RelevanceCore& core_;
    std::unordered_map<std::string, SalienceVector> landscape_;
};

} // namespace rr
} // namespace dte
