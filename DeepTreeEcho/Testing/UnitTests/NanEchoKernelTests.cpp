// ═══════════════════════════════════════════════════════════════════════════
// NanEchoKernelTests — Unit tests for the NanEcho cognitive kernel daemon
//
// Covers: config defaults (mirroring Training/NanEchoConfig.h), adaptive
// attention formula, blended training-phase schedule, fidelity metrics and
// quality gates, Autognosis L0/L1/L2 self-images, and daemon lifecycle.
// Standalone C++17 — no Unreal Engine dependencies.
// ═══════════════════════════════════════════════════════════════════════════
#include <gtest/gtest.h>

#include "NanEcho/Kernel/NanEchoKernel.h"

using namespace nanecho;

namespace
{

// ─────────────────────────────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────────────────────────────

TEST(NanEchoKernelConfig, DefaultsMirrorTrainingConfig)
{
    const FNanEchoKernelConfig Config;

    EXPECT_EQ(Config.BaseModel, "gpt2");
    EXPECT_EQ(Config.MaxSequenceLength, 1024);
    EXPECT_EQ(Config.VocabSize, 50257);

    EXPECT_TRUE(Config.bESNAugmentation);
    EXPECT_EQ(Config.ESNReservoirSize, 512);
    EXPECT_FLOAT_EQ(Config.ESNSpectralRadius, 0.9f);
    EXPECT_FLOAT_EQ(Config.ESNLeakRate, 0.3f);

    EXPECT_FLOAT_EQ(Config.LearningRate, 5e-5f);
    EXPECT_EQ(Config.BatchSize, 8);
    EXPECT_EQ(Config.EpochsPerCycle, 3);
    EXPECT_FLOAT_EQ(Config.WeightDecay, 0.01f);
    EXPECT_EQ(Config.WarmupSteps, 100);

    EXPECT_TRUE(Config.bEnforcePersona);
    EXPECT_EQ(Config.PersonaName, "Deep Tree Echo");
    EXPECT_FLOAT_EQ(Config.PersonaTemperature, 0.8f);

    EXPECT_EQ(Config.HuggingFaceRepo, "deep-tree-echo/nanecho");
    EXPECT_FLOAT_EQ(Config.TrainingCycleIntervalHours, 4.0f);
    EXPECT_TRUE(Config.bAutoDeployAfterTraining);
}

TEST(NanEchoKernelConfig, DefaultsAreValid)
{
    EXPECT_TRUE(FNanEchoKernelConfig{}.IsValid());
}

TEST(NanEchoKernelConfig, RejectsOutOfRangeValues)
{
    {
        FNanEchoKernelConfig C;
        C.BaseModel.clear();
        EXPECT_FALSE(C.IsValid());
    }
    {
        FNanEchoKernelConfig C;
        C.LearningRate = 0.0f;
        EXPECT_FALSE(C.IsValid());
    }
    {
        FNanEchoKernelConfig C;
        C.ESNLeakRate = 1.5f;
        EXPECT_FALSE(C.IsValid());
    }
    {
        FNanEchoKernelConfig C;
        C.ESNSpectralRadius = -0.1f;
        EXPECT_FALSE(C.IsValid());
    }
    {
        FNanEchoKernelConfig C;
        C.BatchSize = 0;
        EXPECT_FALSE(C.IsValid());
    }
    {
        FNanEchoKernelConfig C;
        C.TrainingCycleIntervalHours = 0.0f;
        EXPECT_FALSE(C.IsValid());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Adaptive attention: threshold = 0.5 + load*0.3 - activity*0.2
// ─────────────────────────────────────────────────────────────────────────────

TEST(NanEchoAdaptiveAttention, BaselineThresholdIsHalf)
{
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(0.0f, 0.0f), 0.5f);
}

TEST(NanEchoAdaptiveAttention, FormulaMatchesSpec)
{
    // Full load, no activity: 0.5 + 0.3 = 0.8
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(1.0f, 0.0f), 0.8f);
    // No load, full activity: 0.5 - 0.2 = 0.3
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(0.0f, 1.0f), 0.3f);
    // Mixed: 0.5 + 0.5*0.3 - 0.5*0.2 = 0.55
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(0.5f, 0.5f), 0.55f);
}

TEST(NanEchoAdaptiveAttention, InputsAreClamped)
{
    // Load 5 clamps to 1, activity -3 clamps to 0 → 0.8
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(5.0f, -3.0f), 0.8f);
    // Load -1 clamps to 0, activity 9 clamps to 1 → 0.3
    EXPECT_FLOAT_EQ(FAdaptiveAttention::ComputeThreshold(-1.0f, 9.0f), 0.3f);
}

TEST(NanEchoAdaptiveAttention, ShouldAttendUsesCurrentThreshold)
{
    FAdaptiveAttention Attention;
    Attention.Update(1.0f, 0.0f);  // threshold 0.8
    EXPECT_FLOAT_EQ(Attention.GetThreshold(), 0.8f);
    EXPECT_TRUE(Attention.ShouldAttend(0.8f));
    EXPECT_TRUE(Attention.ShouldAttend(0.95f));
    EXPECT_FALSE(Attention.ShouldAttend(0.79f));

    Attention.Update(0.0f, 1.0f);  // threshold 0.3
    EXPECT_TRUE(Attention.ShouldAttend(0.35f));
    EXPECT_FALSE(Attention.ShouldAttend(0.25f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Training phase schedule (overlapping ranges, blended weights)
// ─────────────────────────────────────────────────────────────────────────────

TEST(NanEchoPhaseSchedule, PureRegionsYieldSinglePhase)
{
    // 0.10 — only BasicAwareness (0–0.20; PersonaDimensions starts 0.15)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(0.10f);
        EXPECT_FLOAT_EQ(W[0], 1.0f);
        EXPECT_FLOAT_EQ(W[1], 0.0f);
    }
    // 0.30 — only PersonaDimensions (0.15–0.50)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(0.30f);
        EXPECT_FLOAT_EQ(W[1], 1.0f);
        EXPECT_FLOAT_EQ(W[0], 0.0f);
        EXPECT_FLOAT_EQ(W[2], 0.0f);
    }
    // 0.55 — only HypergraphEncoding (0.40–0.70; RecursiveReasoning starts 0.60)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(0.55f);
        EXPECT_FLOAT_EQ(W[2], 1.0f);
    }
    // 0.75 — only RecursiveReasoning (0.60–0.85; AdaptiveMastery starts 0.80)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(0.75f);
        EXPECT_FLOAT_EQ(W[3], 1.0f);
    }
    // 0.90 — only AdaptiveMastery (0.80–1.00)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(0.90f);
        EXPECT_FLOAT_EQ(W[4], 1.0f);
    }
}

TEST(NanEchoPhaseSchedule, OverlapMidpointBlendsEqually)
{
    // Midpoint of BasicAwareness/PersonaDimensions overlap [0.15, 0.20]
    const auto W = FTrainingPhaseSchedule::PhaseWeights(0.175f);
    EXPECT_NEAR(W[0], 0.5f, 1e-5f);
    EXPECT_NEAR(W[1], 0.5f, 1e-5f);
}

TEST(NanEchoPhaseSchedule, WeightsAlwaysSumToOne)
{
    for (float P = 0.0f; P <= 1.0f; P += 0.01f)
    {
        const auto W = FTrainingPhaseSchedule::PhaseWeights(P);
        float Sum = 0.0f;
        for (const float V : W) { Sum += V; }
        EXPECT_NEAR(Sum, 1.0f, 1e-4f) << "progress=" << P;
    }
}

TEST(NanEchoPhaseSchedule, ActivePhaseAtBoundaries)
{
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(0.0f), ETrainingPhase::BasicAwareness);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(0.10f), ETrainingPhase::BasicAwareness);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(0.30f), ETrainingPhase::PersonaDimensions);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(0.55f), ETrainingPhase::HypergraphEncoding);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(0.75f), ETrainingPhase::RecursiveReasoning);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(1.0f), ETrainingPhase::AdaptiveMastery);
    // Out-of-range progress clamps
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(-0.5f), ETrainingPhase::BasicAwareness);
    EXPECT_EQ(FTrainingPhaseSchedule::ActivePhase(2.0f), ETrainingPhase::AdaptiveMastery);
}

TEST(NanEchoPhaseSchedule, PhaseNamesAndCount)
{
    EXPECT_EQ(FTrainingPhaseSchedule::NumPhases, 5u);
    EXPECT_STREQ(ToString(ETrainingPhase::BasicAwareness), "BasicAwareness");
    EXPECT_STREQ(ToString(ETrainingPhase::AdaptiveMastery), "AdaptiveMastery");
    // Eight persona dimensions per NANECHO.md
    EXPECT_EQ(static_cast<int>(EPersonaDimension::Count), 8);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fidelity metrics and quality gates
// ─────────────────────────────────────────────────────────────────────────────

TEST(NanEchoFidelity, CompositeUsesSpecWeights)
{
    FFidelityMetrics M;
    M.IdentityRecognition = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.25f, 1e-6f);

    M = FFidelityMetrics{};
    M.PersonaConsistency = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.20f, 1e-6f);

    M = FFidelityMetrics{};
    M.AdaptiveAttention = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.20f, 1e-6f);

    M = FFidelityMetrics{};
    M.RecursiveReasoning = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.15f, 1e-6f);

    M = FFidelityMetrics{};
    M.HypergraphComprehension = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.10f, 1e-6f);

    M = FFidelityMetrics{};
    M.CognitiveSynergy = 1.0f;
    EXPECT_NEAR(M.Composite(), 0.10f, 1e-6f);
}

TEST(NanEchoFidelity, PerfectMetricsCompositeToOne)
{
    FFidelityMetrics M;
    M.IdentityRecognition = 1.0f;
    M.PersonaConsistency = 1.0f;
    M.AdaptiveAttention = 1.0f;
    M.RecursiveReasoning = 1.0f;
    M.HypergraphComprehension = 1.0f;
    M.CognitiveSynergy = 1.0f;
    EXPECT_NEAR(M.Composite(), 1.0f, 1e-6f);
}

namespace
{
FFidelityMetrics PassingMetrics()
{
    FFidelityMetrics M;
    M.IdentityRecognition = 0.9f;
    M.PersonaConsistency = 0.8f;
    M.AdaptiveAttention = 0.75f;
    M.RecursiveReasoning = 0.7f;
    M.HypergraphComprehension = 0.6f;
    M.CognitiveSynergy = 0.6f;
    return M;
}
}  // namespace

TEST(NanEchoQualityGates, DefaultThresholdsMatchSpec)
{
    const FQualityGates Gates;
    EXPECT_FLOAT_EQ(Gates.MinIdentityScore, 0.8f);
    EXPECT_FLOAT_EQ(Gates.MinPersonaCoherence, 0.75f);
    EXPECT_FLOAT_EQ(Gates.MinAdaptiveCapability, 0.7f);
    EXPECT_FLOAT_EQ(Gates.MaxTrainingLoss, 2.0f);
}

TEST(NanEchoQualityGates, PassesAtExactBoundaries)
{
    FQualityGates Gates;
    FFidelityMetrics M;
    M.IdentityRecognition = 0.8f;
    M.PersonaConsistency = 0.75f;
    M.AdaptiveAttention = 0.7f;
    const auto Verdict = Gates.Evaluate(M, 2.0f);
    EXPECT_TRUE(Verdict.bPassed);
    EXPECT_TRUE(Verdict.Failures.empty());
}

TEST(NanEchoQualityGates, EachGateFailsIndependently)
{
    const FQualityGates Gates;

    {
        FFidelityMetrics M = PassingMetrics();
        M.IdentityRecognition = 0.79f;
        const auto V = Gates.Evaluate(M, 1.0f);
        EXPECT_FALSE(V.bPassed);
        ASSERT_EQ(V.Failures.size(), 1u);
        EXPECT_EQ(V.Failures[0], "identity_below_minimum");
    }
    {
        FFidelityMetrics M = PassingMetrics();
        M.PersonaConsistency = 0.5f;
        const auto V = Gates.Evaluate(M, 1.0f);
        EXPECT_FALSE(V.bPassed);
        ASSERT_EQ(V.Failures.size(), 1u);
        EXPECT_EQ(V.Failures[0], "persona_coherence_below_minimum");
    }
    {
        FFidelityMetrics M = PassingMetrics();
        M.AdaptiveAttention = 0.65f;
        const auto V = Gates.Evaluate(M, 1.0f);
        EXPECT_FALSE(V.bPassed);
        ASSERT_EQ(V.Failures.size(), 1u);
        EXPECT_EQ(V.Failures[0], "adaptive_capability_below_minimum");
    }
    {
        const auto V = Gates.Evaluate(PassingMetrics(), 2.5f);
        EXPECT_FALSE(V.bPassed);
        ASSERT_EQ(V.Failures.size(), 1u);
        EXPECT_EQ(V.Failures[0], "training_loss_above_maximum");
    }
}

TEST(NanEchoQualityGates, MultipleFailuresAccumulate)
{
    const FQualityGates Gates;
    const auto V = Gates.Evaluate(FFidelityMetrics{}, 10.0f);  // all zero, huge loss
    EXPECT_FALSE(V.bPassed);
    EXPECT_EQ(V.Failures.size(), 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Autognosis hierarchical self-images
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
FSelfImageL0 MakeObservation(uint64_t Tick, float Progress, float Loss, float Threshold)
{
    FSelfImageL0 Obs;
    Obs.TickIndex = Tick;
    Obs.TrainingProgress = Progress;
    Obs.TrainingLoss = Loss;
    Obs.AttentionThreshold = Threshold;
    return Obs;
}
}  // namespace

TEST(NanEchoAutognosis, EmptyAndSingleObservationYieldNeutralImages)
{
    FAutognosisSystem Autognosis(8);
    EXPECT_EQ(Autognosis.GetObservationCount(), 0u);
    EXPECT_FLOAT_EQ(Autognosis.GetSelfAwarenessScore(), 0.0f);

    Autognosis.Observe(MakeObservation(1, 0.1f, 3.0f, 0.5f));
    EXPECT_EQ(Autognosis.GetObservationCount(), 1u);
    EXPECT_FLOAT_EQ(Autognosis.GetL2().Confidence, 0.0f);
}

TEST(NanEchoAutognosis, DetectsImprovingLossTrend)
{
    FAutognosisSystem Autognosis(8);
    // Strictly decreasing loss → negative trend
    Autognosis.Observe(MakeObservation(1, 0.1f, 4.0f, 0.5f));
    Autognosis.Observe(MakeObservation(2, 0.2f, 3.0f, 0.5f));
    Autognosis.Observe(MakeObservation(3, 0.3f, 2.0f, 0.5f));

    const FSelfImageL1& L1 = Autognosis.GetL1();
    EXPECT_LT(L1.LossTrend, 0.0f);
    EXPECT_NEAR(L1.LossTrend, -1.0f, 1e-5f);
    EXPECT_NEAR(L1.ProgressRate, 0.1f, 1e-5f);
    EXPECT_EQ(L1.WindowSize, 3u);
}

TEST(NanEchoAutognosis, StableThresholdGivesFullStability)
{
    FAutognosisSystem Autognosis(8);
    for (int i = 0; i < 5; ++i)
    {
        Autognosis.Observe(MakeObservation(i, 0.1f * i, 2.0f, 0.6f));
    }
    EXPECT_FLOAT_EQ(Autognosis.GetL1().AttentionStability, 1.0f);
    EXPECT_FALSE(Autognosis.GetL1().bAnomalyDetected);
}

TEST(NanEchoAutognosis, DetectsLossAnomaly)
{
    FAutognosisSystem Autognosis(16);
    for (int i = 0; i < 10; ++i)
    {
        Autognosis.Observe(MakeObservation(i, 0.05f * i, 1.0f, 0.5f));
    }
    EXPECT_FALSE(Autognosis.GetL1().bAnomalyDetected);

    // Sudden loss spike well beyond 3σ of the window
    Autognosis.Observe(MakeObservation(10, 0.5f, 5.0f, 0.5f));
    EXPECT_TRUE(Autognosis.GetL1().bAnomalyDetected);
}

TEST(NanEchoAutognosis, ConfidenceGrowsWithWindowFill)
{
    FAutognosisSystem Autognosis(4);
    Autognosis.Observe(MakeObservation(1, 0.1f, 3.0f, 0.5f));
    Autognosis.Observe(MakeObservation(2, 0.2f, 2.5f, 0.5f));
    EXPECT_FLOAT_EQ(Autognosis.GetL2().Confidence, 0.5f);

    Autognosis.Observe(MakeObservation(3, 0.3f, 2.0f, 0.5f));
    Autognosis.Observe(MakeObservation(4, 0.4f, 1.5f, 0.5f));
    EXPECT_FLOAT_EQ(Autognosis.GetL2().Confidence, 1.0f);

    // Window is capped — a fifth observation keeps count at capacity
    Autognosis.Observe(MakeObservation(5, 0.5f, 1.0f, 0.5f));
    EXPECT_EQ(Autognosis.GetObservationCount(), 4u);
    EXPECT_FLOAT_EQ(Autognosis.GetL2().Confidence, 1.0f);
}

TEST(NanEchoAutognosis, SelfAwarenessCombinesConfidenceStabilityAndTrend)
{
    FAutognosisSystem Autognosis(4);
    for (int i = 0; i < 4; ++i)
    {
        // Full window, stable thresholds, improving loss
        Autognosis.Observe(MakeObservation(i, 0.1f * i, 4.0f - i, 0.5f));
    }
    // 0.4*1.0 (confidence) + 0.4*1.0 (stability) + 0.2*1.0 (improving) = 1.0
    EXPECT_NEAR(Autognosis.GetSelfAwarenessScore(), 1.0f, 1e-5f);
    EXPECT_EQ(Autognosis.GetL2().RecursionDepth, 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Kernel daemon lifecycle
// ─────────────────────────────────────────────────────────────────────────────

TEST(NanEchoKernelDaemon, LifecycleTransitions)
{
    FNanEchoKernel Kernel;
    EXPECT_EQ(Kernel.GetState(), EKernelState::Uninitialized);

    // Cannot start before configuring
    EXPECT_FALSE(Kernel.Start());

    // Invalid config rejected
    FNanEchoKernelConfig Bad;
    Bad.BatchSize = 0;
    EXPECT_FALSE(Kernel.Configure(Bad));
    EXPECT_EQ(Kernel.GetState(), EKernelState::Uninitialized);

    // Valid config accepted
    EXPECT_TRUE(Kernel.Configure(FNanEchoKernelConfig{}));
    EXPECT_EQ(Kernel.GetState(), EKernelState::Configured);

    EXPECT_TRUE(Kernel.Start());
    EXPECT_EQ(Kernel.GetState(), EKernelState::Running);

    // Cannot reconfigure while running
    EXPECT_FALSE(Kernel.Configure(FNanEchoKernelConfig{}));

    Kernel.Stop();
    EXPECT_EQ(Kernel.GetState(), EKernelState::Stopped);

    // Restart from Stopped
    EXPECT_TRUE(Kernel.Start());
    EXPECT_EQ(Kernel.GetState(), EKernelState::Running);
}

TEST(NanEchoKernelDaemon, TickRequiresRunningState)
{
    FNanEchoKernel Kernel;
    EXPECT_EQ(Kernel.TickHours(10.0f), 0);

    Kernel.Configure(FNanEchoKernelConfig{});
    EXPECT_EQ(Kernel.TickHours(10.0f), 0);  // configured but not started
}

TEST(NanEchoKernelDaemon, CycleFiresAtConfiguredInterval)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});  // 4h interval
    Kernel.Start();

    EXPECT_EQ(Kernel.TickHours(1.0f), 0);
    EXPECT_EQ(Kernel.TickHours(2.0f), 0);
    EXPECT_EQ(Kernel.GetCompletedCycles(), 0u);

    EXPECT_EQ(Kernel.TickHours(1.0f), 1);  // accumulated 4h
    EXPECT_EQ(Kernel.GetCompletedCycles(), 1u);

    // A large tick catches up on multiple cycles
    EXPECT_EQ(Kernel.TickHours(9.0f), 2);
    EXPECT_EQ(Kernel.GetCompletedCycles(), 3u);
}

TEST(NanEchoKernelDaemon, CustomExecutorDrivesProgressAndFidelity)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});
    Kernel.Start();

    float SeenProgress = -1.0f;
    Kernel.SetCycleExecutor(
        [&SeenProgress](const FNanEchoKernelConfig& Cfg, float Progress) {
            EXPECT_EQ(Cfg.BaseModel, "gpt2");
            SeenProgress = Progress;
            FCycleResult R;
            R.FinalLoss = 1.5f;
            R.ProgressDelta = 0.25f;
            R.Fidelity = PassingMetrics();
            return R;
        });

    EXPECT_TRUE(Kernel.RunTrainingCycle());
    EXPECT_FLOAT_EQ(SeenProgress, 0.0f);
    EXPECT_FLOAT_EQ(Kernel.GetTrainingProgress(), 0.25f);
    EXPECT_FLOAT_EQ(Kernel.GetLastLoss(), 1.5f);
    EXPECT_TRUE(Kernel.DidLastCyclePassGates());

    EXPECT_TRUE(Kernel.RunTrainingCycle());
    EXPECT_FLOAT_EQ(SeenProgress, 0.25f);
    EXPECT_FLOAT_EQ(Kernel.GetTrainingProgress(), 0.5f);

    // Progress clamps at 1.0
    Kernel.RunTrainingCycle();
    Kernel.RunTrainingCycle();
    Kernel.RunTrainingCycle();
    EXPECT_FLOAT_EQ(Kernel.GetTrainingProgress(), 1.0f);
    EXPECT_EQ(Kernel.GetActivePhase(), ETrainingPhase::AdaptiveMastery);
}

TEST(NanEchoKernelDaemon, ManualCycleRequiresRunningState)
{
    FNanEchoKernel Kernel;
    EXPECT_FALSE(Kernel.RunTrainingCycle());
    Kernel.Configure(FNanEchoKernelConfig{});
    EXPECT_FALSE(Kernel.RunTrainingCycle());
    Kernel.Start();
    EXPECT_TRUE(Kernel.RunTrainingCycle());
    Kernel.Stop();
    EXPECT_FALSE(Kernel.RunTrainingCycle());
}

TEST(NanEchoKernelDaemon, GatePassTriggersAutoDeploy)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});  // autodeploy on
    Kernel.Start();
    Kernel.SetCycleExecutor([](const FNanEchoKernelConfig&, float) {
        FCycleResult R;
        R.FinalLoss = 0.5f;
        R.ProgressDelta = 0.1f;
        R.Fidelity = PassingMetrics();
        return R;
    });

    Kernel.RunTrainingCycle();
    EXPECT_TRUE(Kernel.DidLastCyclePassGates());
    EXPECT_EQ(Kernel.GetDeployedCheckpoints(), 1u);
    EXPECT_EQ(Kernel.GetLastDeployedCheckpoint(), "deep-tree-echo/nanecho/checkpoint-1");
}

TEST(NanEchoKernelDaemon, GateFailureBlocksDeployAndRecordsReasons)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});
    Kernel.Start();
    Kernel.SetCycleExecutor([](const FNanEchoKernelConfig&, float) {
        FCycleResult R;
        R.FinalLoss = 3.5f;  // above max loss
        R.ProgressDelta = 0.1f;
        R.Fidelity = PassingMetrics();
        return R;
    });

    Kernel.RunTrainingCycle();
    EXPECT_FALSE(Kernel.DidLastCyclePassGates());
    EXPECT_EQ(Kernel.GetDeployedCheckpoints(), 0u);
    ASSERT_EQ(Kernel.GetLastGateFailures().size(), 1u);
    EXPECT_EQ(Kernel.GetLastGateFailures()[0], "training_loss_above_maximum");
}

TEST(NanEchoKernelDaemon, AutoDeployDisabledSkipsDeployment)
{
    FNanEchoKernelConfig Config;
    Config.bAutoDeployAfterTraining = false;

    FNanEchoKernel Kernel;
    Kernel.Configure(Config);
    Kernel.Start();
    Kernel.SetCycleExecutor([](const FNanEchoKernelConfig&, float) {
        FCycleResult R;
        R.FinalLoss = 0.5f;
        R.ProgressDelta = 0.1f;
        R.Fidelity = PassingMetrics();
        return R;
    });

    Kernel.RunTrainingCycle();
    EXPECT_TRUE(Kernel.DidLastCyclePassGates());
    EXPECT_EQ(Kernel.GetDeployedCheckpoints(), 0u);
}

TEST(NanEchoKernelDaemon, DefaultExecutorConvergesAndPassesGates)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});
    Kernel.Start();

    // Default executor: +0.15 progress per cycle (3 epochs × 0.05)
    for (int i = 0; i < 7; ++i)
    {
        Kernel.RunTrainingCycle();
    }
    EXPECT_FLOAT_EQ(Kernel.GetTrainingProgress(), 1.0f);
    EXPECT_EQ(Kernel.GetActivePhase(), ETrainingPhase::AdaptiveMastery);
    EXPECT_LT(Kernel.GetLastLoss(), 2.0f);
    EXPECT_TRUE(Kernel.DidLastCyclePassGates());
    EXPECT_GT(Kernel.GetDeployedCheckpoints(), 0u);
    EXPECT_GT(Kernel.GetLastFidelity().Composite(), 0.8f);
}

TEST(NanEchoKernelDaemon, TickUpdatesAttentionAndAutognosis)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});
    Kernel.Start();

    // Tick that completes a cycle: recent activity spikes to 1.0
    Kernel.TickHours(4.0f);
    EXPECT_FLOAT_EQ(Kernel.GetAttention().GetRecentActivity(), 1.0f);
    EXPECT_GE(Kernel.GetAutognosis().GetObservationCount(), 1u);

    // Quiet ticks decay activity and accumulate observations
    const std::size_t Before = Kernel.GetAutognosis().GetObservationCount();
    Kernel.TickHours(1.0f);
    EXPECT_LT(Kernel.GetAttention().GetRecentActivity(), 1.0f);
    EXPECT_GT(Kernel.GetAutognosis().GetObservationCount(), Before);
}

TEST(NanEchoKernelDaemon, ResumeFromStoppedPreservesCycleTimer)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});  // 4h interval
    Kernel.Start();

    // Accumulate partial progress toward the next cycle, then pause.
    EXPECT_EQ(Kernel.TickHours(3.0f), 0);
    Kernel.Stop();

    // Resume: the 3h already accumulated must survive the pause.
    EXPECT_TRUE(Kernel.Start());
    EXPECT_EQ(Kernel.TickHours(1.0f), 1);
    EXPECT_EQ(Kernel.GetCompletedCycles(), 1u);
}

TEST(NanEchoKernelDaemon, ReconfigureResetsCycleTimer)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});  // 4h interval
    Kernel.Start();
    EXPECT_EQ(Kernel.TickHours(3.0f), 0);
    Kernel.Stop();

    // A fresh Configure + Start begins a new interval from zero.
    EXPECT_TRUE(Kernel.Configure(FNanEchoKernelConfig{}));
    EXPECT_TRUE(Kernel.Start());
    EXPECT_EQ(Kernel.TickHours(1.0f), 0);
    EXPECT_EQ(Kernel.TickHours(3.0f), 1);
}

TEST(NanEchoKernelDaemon, ManualCycleRefreshesAttention)
{
    FNanEchoKernel Kernel;
    Kernel.Configure(FNanEchoKernelConfig{});
    Kernel.Start();

    // Manual trigger (no TickHours involved) must spike recent activity and
    // recompute the threshold from the post-cycle phase load.
    EXPECT_TRUE(Kernel.RunTrainingCycle());
    EXPECT_FLOAT_EQ(Kernel.GetAttention().GetRecentActivity(), 1.0f);

    const float ExpectedLoad =
        static_cast<float>(Kernel.GetActivePhase())
        / static_cast<float>(FTrainingPhaseSchedule::NumPhases - 1);
    EXPECT_FLOAT_EQ(Kernel.GetAttention().GetThreshold(),
                    FAdaptiveAttention::ComputeThreshold(ExpectedLoad, 1.0f));

    // Drive progress to completion: cognitive load follows the active phase.
    Kernel.SetCycleExecutor([](const FNanEchoKernelConfig&, float) {
        FCycleResult R;
        R.FinalLoss = 0.5f;
        R.ProgressDelta = 1.0f;
        return R;
    });
    Kernel.RunTrainingCycle();
    EXPECT_EQ(Kernel.GetActivePhase(), ETrainingPhase::AdaptiveMastery);
    // Load 1.0, activity 1.0 → 0.5 + 0.3 - 0.2 = 0.6
    EXPECT_FLOAT_EQ(Kernel.GetAttention().GetThreshold(), 0.6f);
    EXPECT_FLOAT_EQ(Kernel.GetAttention().GetCognitiveLoad(), 1.0f);
}

TEST(NanEchoKernelDaemon, IntrospectionReflectsConfiguredState)
{
    FNanEchoKernelConfig Config;
    Config.PersonaName = "Deep Tree Echo";
    Config.TrainingCycleIntervalHours = 2.0f;

    FNanEchoKernel Kernel;
    Kernel.Configure(Config);
    EXPECT_EQ(Kernel.GetConfig().PersonaName, "Deep Tree Echo");
    EXPECT_FLOAT_EQ(Kernel.GetConfig().TrainingCycleIntervalHours, 2.0f);
    EXPECT_FLOAT_EQ(Kernel.GetTrainingProgress(), 0.0f);
    EXPECT_EQ(Kernel.GetActivePhase(), ETrainingPhase::BasicAwareness);
    EXPECT_EQ(Kernel.GetCompletedCycles(), 0u);
    EXPECT_FLOAT_EQ(Kernel.GetQualityGates().MinIdentityScore, 0.8f);
}

}  // namespace
