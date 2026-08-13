/**
 * @file RelevanceKernelTests.cpp
 * @brief Unit tests for the DeepTreeEcho/Relevance module — the CogPrime
 *        relevance realization kernel ("rr.kern") port: RelevanceCore,
 *        FrameStack, ActionGenerator, SalienceLandscape, and the
 *        RelevanceArbiter integration with reservoir/endocrine/goal hooks
 *        plus cog::grip convergence telemetry.
 *
 * Header-only module — no impl .cpp inclusion needed.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <string>
#include <unordered_map>

#include "Relevance/RelevanceArbiter.h"

using namespace dte::rr;

// ============================================================================
// RelevanceCore — five-mode salience coordination
// ============================================================================

TEST(RelevanceCoreTest, DefaultThresholdsAreHalf)
{
    RelevanceCore core;
    for (std::size_t i = 0; i < kNumRelevanceModes; ++i) {
        EXPECT_DOUBLE_EQ(core.threshold(static_cast<RelevanceMode>(i)), 0.5);
    }
}

TEST(RelevanceCoreTest, NeutralSalienceWithoutProvidersPassesThreshold)
{
    RelevanceCore core; // no providers → all items at 0.5 == threshold 0.5
    std::set<std::string> items{"a", "b", "c"};
    auto salient =
        core.updateSalience(RelevanceMode::SelectiveAttention, items);
    EXPECT_EQ(salient.size(), 3u);
}

TEST(RelevanceCoreTest, ProviderDrivenSalienceFiltersBelowThreshold)
{
    RelevanceCore core;
    core.addProvider("fixed", [](const std::string& item, const Context&) {
        return item == "hot" ? 0.9 : 0.1;
    });
    std::set<std::string> items{"hot", "cold1", "cold2"};
    auto salient =
        core.updateSalience(RelevanceMode::SelectiveAttention, items);
    EXPECT_EQ(salient.size(), 1u);
    EXPECT_TRUE(salient.count("hot"));
}

TEST(RelevanceCoreTest, WeightedProvidersAverage)
{
    RelevanceCore core;
    core.addProvider("high", [](const std::string&, const Context&) {
        return 1.0;
    }, 3.0);
    core.addProvider("low", [](const std::string&, const Context&) {
        return 0.0;
    }, 1.0);
    // weighted avg = (3*1 + 1*0)/4 = 0.75 ≥ 0.5 → salient
    std::set<std::string> items{"x"};
    auto salient = core.updateSalience(RelevanceMode::WorkingMemory, items);
    EXPECT_EQ(salient.size(), 1u);
}

TEST(RelevanceCoreTest, CrossModeResonanceReinforcesSharedItems)
{
    RelevanceCore core;
    core.addProvider("mid", [](const std::string&, const Context&) {
        return 0.55;
    });
    // Activate "shared" in working memory first.
    core.updateSalience(RelevanceMode::WorkingMemory, {"shared"});
    // Boost interaction weight from attention ← working memory.
    core.setInteractionWeight(RelevanceMode::SelectiveAttention,
                              RelevanceMode::WorkingMemory, 1.5);
    core.setThreshold(RelevanceMode::SelectiveAttention, 0.7);
    auto salient = core.updateSalience(RelevanceMode::SelectiveAttention,
                                       {"shared", "alone"});
    // shared: 0.55*1.5 = 0.825 ≥ 0.7 → in; alone: 0.55*min(1,1.5)=0.55 → out
    EXPECT_TRUE(salient.count("shared"));
    EXPECT_FALSE(salient.count("alone"));
}

TEST(RelevanceCoreTest, RestructureSalienceModulatesThreshold)
{
    RelevanceCore core;
    Context insight;
    insight.set("threshold_mod", 0.5);
    core.restructureSalience(RelevanceMode::ProblemSpace, insight);
    EXPECT_DOUBLE_EQ(core.threshold(RelevanceMode::ProblemSpace), 0.25);
}

TEST(RelevanceCoreTest, EvaluateRelevanceReturnsUnionAndConfidence)
{
    RelevanceCore core;
    core.addProvider("all", [](const std::string&, const Context&) {
        return 0.8;
    });
    auto [items, confidence] = core.evaluateRelevance({"a", "b"}, {});
    EXPECT_EQ(items.size(), 2u);
    EXPECT_NEAR(confidence, 1.0, 1e-9); // full coverage in all 5 modes
}

TEST(RelevanceCoreTest, HormonalModulationNarrowsAndWidens)
{
    RelevanceCore coreN;
    coreN.applyHormonalModulation(/*cortisol*/ 1.0, /*dopamine*/ 0.5);
    EXPECT_GT(coreN.threshold(RelevanceMode::SelectiveAttention), 0.5);

    RelevanceCore coreW;
    coreW.applyHormonalModulation(/*cortisol*/ 0.5, /*dopamine*/ 1.0);
    EXPECT_LT(coreW.threshold(RelevanceMode::SelectiveAttention), 0.5);
}

// ============================================================================
// FrameStack — cognitive frames + ways of knowing
// ============================================================================

TEST(FrameStackTest, ShiftAndPopFrames)
{
    FrameStack fs;
    EXPECT_FALSE(fs.hasFrame());

    CognitiveFrame f1;
    f1.salienceWeights["goal"] = 0.9;
    fs.shiftFrame(f1);
    EXPECT_TRUE(fs.hasFrame());
    EXPECT_EQ(fs.depth(), 0u);

    CognitiveFrame f2;
    f2.salienceWeights["threat"] = 1.0;
    fs.shiftFrame(f2);
    EXPECT_EQ(fs.depth(), 1u);
    EXPECT_TRUE(fs.current().salienceWeights.count("threat"));

    EXPECT_TRUE(fs.popFrame());
    EXPECT_TRUE(fs.current().salienceWeights.count("goal"));
    EXPECT_FALSE(fs.popFrame());
}

TEST(FrameStackTest, RealizeRelevanceUsesFrameWeightsAndContext)
{
    FrameStack fs;
    CognitiveFrame f;
    f.salienceWeights["food"] = 0.6;
    f.salienceWeights["noise"] = 0.3;
    f.context.set("food", 1.0); // matching context value doubles relevance
    fs.shiftFrame(f);

    std::unordered_map<std::string, double> inputs{{"food", 1.0},
                                                   {"noise", 1.0},
                                                   {"unknown", 1.0}};
    auto scores = fs.realizeRelevance(inputs, 0.5);
    // food: 0.6*2.0=1.2 ≥ 0.5 → in; noise: 0.3 (no ctx) → out; unknown → out
    EXPECT_EQ(scores.size(), 1u);
    EXPECT_NEAR(scores.at("food"), 1.2, 1e-9);
}

TEST(FrameStackTest, AddKnowingMode)
{
    FrameStack fs;
    fs.shiftFrame(CognitiveFrame{});
    fs.addKnowingMode(KnowingMode::Participatory);
    ASSERT_EQ(fs.current().activeKnowingModes.size(), 1u);
    EXPECT_EQ(fs.current().activeKnowingModes[0], KnowingMode::Participatory);
}

// ============================================================================
// ActionGenerator — RR-driven action selection
// ============================================================================

TEST(ActionGeneratorTest, AttendGeneratedForHighRelevance)
{
    ActionGenerator gen(0.7);
    CognitiveFrame frame;
    std::unordered_map<std::string, double> scores{{"target", 0.85}};
    auto actions = gen.generateActions(frame, scores, {});
    bool hasAttend = false;
    for (const auto& a : actions)
        if (a.type == RRActionType::Attend) hasAttend = true;
    EXPECT_TRUE(hasAttend);
}

TEST(ActionGeneratorTest, ExploreWhenMeanRelevanceLow)
{
    ActionGenerator gen;
    CognitiveFrame frame;
    std::unordered_map<std::string, double> scores{{"a", 0.1}, {"b", 0.2}};
    auto actions = gen.generateActions(frame, scores, {});
    bool hasExplore = false;
    for (const auto& a : actions)
        if (a.type == RRActionType::Explore) hasExplore = true;
    EXPECT_TRUE(hasExplore);
}

TEST(ActionGeneratorTest, ExploitWhenHighConfidenceItemsExist)
{
    ActionGenerator gen;
    CognitiveFrame frame;
    std::unordered_map<std::string, double> scores{{"known", 0.9},
                                                   {"other", 0.5}};
    auto actions = gen.generateActions(frame, scores, {});
    bool hasExploit = false;
    for (const auto& a : actions) {
        if (a.type == RRActionType::Exploit) {
            hasExploit = true;
            ASSERT_EQ(a.targets.size(), 1u);
            EXPECT_EQ(a.targets[0], "known");
        }
    }
    EXPECT_TRUE(hasExploit);
}

TEST(ActionGeneratorTest, FrameShiftOnContextDivergence)
{
    ActionGenerator gen;
    CognitiveFrame frame;
    frame.context.set("indoor", 1.0);
    frame.context.set("calm", 1.0);
    Context ctx; // entirely different context keys
    ctx.set("outdoor", 1.0);
    ctx.set("storm", 1.0);
    auto actions = gen.generateActions(frame, {}, ctx);
    bool hasShift = false;
    for (const auto& a : actions)
        if (a.type == RRActionType::FrameShift) hasShift = true;
    EXPECT_TRUE(hasShift);
}

TEST(ActionGeneratorTest, SelectBestActionByWeightedScore)
{
    ActionGenerator gen;
    RRAction low;
    low.type = RRActionType::Explore;
    low.expectedRelevance = 0.5;
    low.confidence = 0.6;
    RRAction high;
    high.type = RRActionType::Exploit;
    high.expectedRelevance = 0.9;
    high.confidence = 0.9;
    auto best = gen.selectBestAction({low, high});
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best->type, RRActionType::Exploit);
}

TEST(ActionGeneratorTest, SelectBestOnEmptyReturnsNullopt)
{
    ActionGenerator gen;
    EXPECT_FALSE(gen.selectBestAction({}).has_value());
}

// ============================================================================
// SalienceLandscape — ACT vectors + 4-level cascade
// ============================================================================

TEST(SalienceLandscapeTest, UpdateSalienceProducesACTVector)
{
    RelevanceCore core;
    core.addProvider("all", [](const std::string&, const Context&) {
        return 0.9;
    });
    SalienceLandscape land(core);
    std::unordered_map<std::string, double> features{{"edge", 0.8},
                                                     {"motion", 0.9}};
    auto v = land.updateSalience("object1", features);
    EXPECT_GT(v.aspectuality, 0.0);
    EXPECT_GT(v.centrality, 0.0);
    EXPECT_NEAR(v.temporality, 0.5, 1e-9); // neutral without horizon
    EXPECT_TRUE(land.has("object1"));
}

TEST(SalienceLandscapeTest, AgentNeedsRaiseCentrality)
{
    RelevanceCore core;
    core.addProvider("all", [](const std::string&, const Context&) {
        return 0.9;
    });
    SalienceLandscape land(core);
    std::unordered_map<std::string, double> features{{"food", 1.0}};

    Context plain;
    auto vPlain = land.updateSalience("apple", features, plain);

    Context hungry;
    hungry.set("agent_needs::food", 1.0);
    auto vNeed = land.updateSalience("apple2", features, hungry);

    EXPECT_GE(vNeed.centrality, vPlain.centrality);
}

TEST(SalienceLandscapeTest, TemporalHorizonDrivesTemporality)
{
    RelevanceCore core;
    core.addProvider("all", [](const std::string&, const Context&) {
        return 0.9;
    });
    SalienceLandscape land(core);
    std::unordered_map<std::string, double> features{{"urgent_signal", 1.0}};
    Context ctx;
    ctx.set("temporal_horizon", 0.0); // 1 temporal feature / (0+1) = 1.0
    auto v = land.updateSalience("alarm", features, ctx);
    EXPECT_NEAR(v.temporality, 1.0, 1e-9);
}

TEST(SalienceLandscapeTest, CascadeFiltersProgressively)
{
    RelevanceCore core;
    core.addProvider("all", [](const std::string&, const Context&) {
        return 0.95;
    });
    SalienceLandscape land(core);

    // Strong item: high features, urgent, needed.
    Context ctx;
    ctx.set("agent_needs::signal", 1.0);
    ctx.set("temporal_horizon", 0.0);
    land.updateSalience(
        "strong", {{"signal", 0.95}, {"urgent_signal", 0.9}}, ctx);
    // Weak item: single faint feature.
    land.updateSalience("weak", {{"blur", 0.1}}, {});

    auto survivors = land.cascade({"strong", "weak"});
    EXPECT_TRUE(survivors.count("strong"));
    EXPECT_FALSE(survivors.count("weak"));
}

// ============================================================================
// RelevanceArbiter — substrate integration + grip telemetry
// ============================================================================

namespace {

SubstrateSamplers makeSamplers(double cortisol, double dopamine)
{
    SubstrateSamplers s;
    s.reservoirSalience = [](const std::string& item) {
        // Deterministic pseudo-reservoir: "prey" resonates strongly.
        return item.find("prey") != std::string::npos ? 0.95 : 0.4;
    };
    s.cortisol = [cortisol]() { return cortisol; };
    s.dopamine = [dopamine]() { return dopamine; };
    s.goalPriority = [](const std::string& item) {
        return item.find("prey") != std::string::npos ? 0.9 : 0.2;
    };
    return s;
}

std::unordered_map<std::string, std::unordered_map<std::string, double>>
makeScene()
{
    return {
        {"prey_rabbit",
         {{"motion", 0.9}, {"urgent_signal", 0.8}, {"shape", 0.85}}},
        {"rock", {{"shape", 0.3}}},
    };
}

} // namespace

TEST(RelevanceArbiterTest, ArbitrateSelectsActionForSalientScene)
{
    RelevanceArbiter arb(makeSamplers(0.5, 0.5));
    Context ctx;
    ctx.set("agent_needs::motion", 1.0);
    ctx.set("temporal_horizon", 0.0);
    auto best = arb.arbitrate(makeScene(), ctx);
    ASSERT_TRUE(best.has_value());
    // The prey should dominate scoring.
    ASSERT_TRUE(arb.lastScores().count("prey_rabbit"));
    if (arb.lastScores().count("rock")) {
        EXPECT_GE(arb.lastScores().at("prey_rabbit"),
                  arb.lastScores().at("rock"));
    }
}

TEST(RelevanceArbiterTest, CortisolNarrowsThresholds)
{
    RelevanceArbiter calm(makeSamplers(0.5, 0.5));
    RelevanceArbiter stressed(makeSamplers(1.0, 0.0));
    Context ctx;
    calm.arbitrate(makeScene(), ctx);
    stressed.arbitrate(makeScene(), ctx);
    EXPECT_GT(
        stressed.core().threshold(RelevanceMode::SelectiveAttention),
        calm.core().threshold(RelevanceMode::SelectiveAttention));
}

TEST(RelevanceArbiterTest, GripTelemetryUpdatesAfterArbitration)
{
    RelevanceArbiter arb(makeSamplers(0.5, 0.5));
    Context ctx;
    ctx.set("agent_needs::motion", 1.0);
    arb.arbitrate(makeScene(), ctx);
    const auto& g = arb.lastGrip();
    EXPECT_GT(g.composability, 0.0f);
    EXPECT_GE(g.product(), 0.0f);
}

TEST(RelevanceArbiterTest, InsightRestructuresAndShiftsFrame)
{
    RelevanceArbiter arb(makeSamplers(0.5, 0.5));
    const double before =
        arb.core().threshold(RelevanceMode::ProblemSpace);
    CognitiveFrame hunting;
    hunting.salienceWeights["prey_rabbit"] = 1.0;
    arb.insight(RelevanceMode::ProblemSpace, 0.5, 1.2, hunting);
    EXPECT_LT(arb.core().threshold(RelevanceMode::ProblemSpace), before);
    EXPECT_TRUE(arb.frames().hasFrame());
}
