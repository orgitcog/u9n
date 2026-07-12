// ═══════════════════════════════════════════════════════════════════════════
// NanEchoKernel — Cognitive kernel daemon for the NanEcho training pipeline
//
// Standalone C++17 (no Unreal Engine dependencies). Implements the cognitive
// kernel that orchestrates echoself's NanEcho model training as described in
// NANECHO.md, combined with the Autognosis hierarchical self-image system
// described in AUTOGNOSIS.md:
//
//   - Adaptive attention:  threshold = 0.5 + load*0.3 - activity*0.2
//   - Five blended training phases (Basic Awareness → Adaptive Mastery)
//   - Weighted fidelity metrics with automated quality gates
//   - Autognosis self-images at L0 (observation), L1 (pattern analysis),
//     L2 (meta-cognitive analysis)
//   - Daemon lifecycle: Configure → Start → Tick → Stop
//
// Configuration mirrors Training/NanEchoConfig.h (FNanEchoTrainingConfig);
// the UE USTRUCT can be converted field-for-field to FNanEchoKernelConfig
// at the engine boundary.
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace nanecho
{

// ─────────────────────────────────────────────────────────────────────────────
// Configuration (plain-C++ mirror of FNanEchoTrainingConfig)
// ─────────────────────────────────────────────────────────────────────────────

struct FNanEchoKernelConfig
{
    // Model architecture
    std::string BaseModel = "gpt2";
    int32_t MaxSequenceLength = 1024;
    int32_t VocabSize = 50257;

    // ESN augmentation
    bool bESNAugmentation = true;
    int32_t ESNReservoirSize = 512;
    float ESNSpectralRadius = 0.9f;
    float ESNLeakRate = 0.3f;

    // Training hyperparameters
    float LearningRate = 5e-5f;
    int32_t BatchSize = 8;
    int32_t EpochsPerCycle = 3;
    float WeightDecay = 0.01f;
    int32_t WarmupSteps = 100;

    // Persona integration
    bool bEnforcePersona = true;
    std::string PersonaName = "Deep Tree Echo";
    float PersonaTemperature = 0.8f;

    // CI/CD integration
    std::string HuggingFaceRepo = "deep-tree-echo/nanecho";
    float TrainingCycleIntervalHours = 4.0f;
    bool bAutoDeployAfterTraining = true;

    /** Validate ranges; returns true when the config is usable. */
    bool IsValid() const
    {
        return !BaseModel.empty()
            && MaxSequenceLength > 0
            && VocabSize > 0
            && ESNReservoirSize > 0
            && ESNSpectralRadius >= 0.0f && ESNSpectralRadius <= 2.0f
            && ESNLeakRate >= 0.0f && ESNLeakRate <= 1.0f
            && LearningRate > 0.0f
            && BatchSize > 0
            && EpochsPerCycle > 0
            && WeightDecay >= 0.0f
            && WarmupSteps >= 0
            && PersonaTemperature >= 0.0f && PersonaTemperature <= 2.0f
            && TrainingCycleIntervalHours > 0.0f;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Adaptive attention (NANECHO.md: "Adaptive Attention Mechanism")
// ─────────────────────────────────────────────────────────────────────────────

class FAdaptiveAttention
{
public:
    /** threshold = 0.5 + cognitive_load*0.3 - recent_activity*0.2, clamped to [0,1]. */
    static float ComputeThreshold(float CognitiveLoad, float RecentActivity)
    {
        const float Load = std::clamp(CognitiveLoad, 0.0f, 1.0f);
        const float Activity = std::clamp(RecentActivity, 0.0f, 1.0f);
        return std::clamp(0.5f + Load * 0.3f - Activity * 0.2f, 0.0f, 1.0f);
    }

    void Update(float CognitiveLoad, float RecentActivity)
    {
        CurrentLoad = std::clamp(CognitiveLoad, 0.0f, 1.0f);
        CurrentActivity = std::clamp(RecentActivity, 0.0f, 1.0f);
        CurrentThreshold = ComputeThreshold(CurrentLoad, CurrentActivity);
    }

    /** A stimulus gains focus when its salience meets the adaptive threshold. */
    bool ShouldAttend(float Salience) const { return Salience >= CurrentThreshold; }

    float GetThreshold() const { return CurrentThreshold; }
    float GetCognitiveLoad() const { return CurrentLoad; }
    float GetRecentActivity() const { return CurrentActivity; }

private:
    float CurrentLoad = 0.0f;
    float CurrentActivity = 0.0f;
    float CurrentThreshold = 0.5f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Training phases (NANECHO.md: "Training Phases", overlapping progress ranges)
// ─────────────────────────────────────────────────────────────────────────────

enum class ETrainingPhase : uint8_t
{
    BasicAwareness = 0,   // 0–20%:  Echo Self identity and basic terms
    PersonaDimensions,    // 15–50%: the eight persona dimensions
    HypergraphEncoding,   // 40–70%: neural-symbolic pattern encoding
    RecursiveReasoning,   // 60–85%: multi-level cognitive processing
    AdaptiveMastery,      // 80–100%: full Echo Self representation
    Count
};

inline const char* ToString(ETrainingPhase Phase)
{
    switch (Phase)
    {
    case ETrainingPhase::BasicAwareness:     return "BasicAwareness";
    case ETrainingPhase::PersonaDimensions:  return "PersonaDimensions";
    case ETrainingPhase::HypergraphEncoding: return "HypergraphEncoding";
    case ETrainingPhase::RecursiveReasoning: return "RecursiveReasoning";
    case ETrainingPhase::AdaptiveMastery:    return "AdaptiveMastery";
    default:                                 return "Unknown";
    }
}

/**
 * Maps training progress in [0,1] onto blended phase weights.
 *
 * Adjacent phases overlap (e.g. PersonaDimensions begins at 15% while
 * BasicAwareness runs to 20%); inside an overlap the outgoing phase ramps
 * down linearly while the incoming phase ramps up, and weights are
 * normalized to sum to 1.
 */
class FTrainingPhaseSchedule
{
public:
    static constexpr std::size_t NumPhases = static_cast<std::size_t>(ETrainingPhase::Count);

    struct FPhaseRange { float Start; float End; };

    static constexpr std::array<FPhaseRange, NumPhases> Ranges = {{
        {0.00f, 0.20f},  // BasicAwareness
        {0.15f, 0.50f},  // PersonaDimensions
        {0.40f, 0.70f},  // HypergraphEncoding
        {0.60f, 0.85f},  // RecursiveReasoning
        {0.80f, 1.00f},  // AdaptiveMastery
    }};

    /** Normalized weight per phase at the given progress. */
    static std::array<float, NumPhases> PhaseWeights(float Progress)
    {
        const float P = std::clamp(Progress, 0.0f, 1.0f);
        std::array<float, NumPhases> Weights{};
        float Sum = 0.0f;

        for (std::size_t i = 0; i < NumPhases; ++i)
        {
            const auto& R = Ranges[i];
            if (P < R.Start || P > R.End)
            {
                Weights[i] = 0.0f;
                continue;
            }

            float W = 1.0f;
            // Ramp in across the overlap with the previous phase.
            if (i > 0 && Ranges[i - 1].End > R.Start && P < Ranges[i - 1].End)
            {
                W = std::min(W, (P - R.Start) / (Ranges[i - 1].End - R.Start));
            }
            // Ramp out across the overlap with the next phase.
            if (i + 1 < NumPhases && Ranges[i + 1].Start < R.End && P > Ranges[i + 1].Start)
            {
                W = std::min(W, (R.End - P) / (R.End - Ranges[i + 1].Start));
            }
            Weights[i] = std::max(W, 0.0f);
            Sum += Weights[i];
        }

        if (Sum > 0.0f)
        {
            for (auto& W : Weights) { W /= Sum; }
        }
        else
        {
            Weights[NumPhases - 1] = 1.0f;  // degenerate guard; P==1 handled above
        }
        return Weights;
    }

    /** Dominant phase at the given progress (ties resolve to the later phase). */
    static ETrainingPhase ActivePhase(float Progress)
    {
        const auto Weights = PhaseWeights(Progress);
        std::size_t Best = 0;
        for (std::size_t i = 1; i < NumPhases; ++i)
        {
            if (Weights[i] >= Weights[Best]) { Best = i; }
        }
        return static_cast<ETrainingPhase>(Best);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Persona dimensions (NANECHO.md: "Persona Dimensions")
// ─────────────────────────────────────────────────────────────────────────────

enum class EPersonaDimension : uint8_t
{
    Cognitive = 0,
    Introspective,
    Adaptive,
    Recursive,
    Synergistic,
    Holographic,
    NeuralSymbolic,
    Dynamic,
    Count
};

// ─────────────────────────────────────────────────────────────────────────────
// Fidelity metrics and quality gates (NANECHO.md: "Evaluation and Fidelity")
// ─────────────────────────────────────────────────────────────────────────────

struct FFidelityMetrics
{
    float IdentityRecognition = 0.0f;      // weight 0.25
    float PersonaConsistency = 0.0f;       // weight 0.20
    float AdaptiveAttention = 0.0f;        // weight 0.20
    float RecursiveReasoning = 0.0f;       // weight 0.15
    float HypergraphComprehension = 0.0f;  // weight 0.10
    float CognitiveSynergy = 0.0f;         // weight 0.10

    float Composite() const
    {
        return IdentityRecognition * 0.25f
             + PersonaConsistency * 0.20f
             + AdaptiveAttention * 0.20f
             + RecursiveReasoning * 0.15f
             + HypergraphComprehension * 0.10f
             + CognitiveSynergy * 0.10f;
    }
};

struct FQualityGates
{
    float MinIdentityScore = 0.8f;
    float MinPersonaCoherence = 0.75f;
    float MinAdaptiveCapability = 0.7f;
    float MaxTrainingLoss = 2.0f;

    struct FVerdict
    {
        bool bPassed = false;
        std::vector<std::string> Failures;
    };

    FVerdict Evaluate(const FFidelityMetrics& Metrics, float TrainingLoss) const
    {
        FVerdict Verdict;
        if (Metrics.IdentityRecognition < MinIdentityScore)
        {
            Verdict.Failures.push_back("identity_below_minimum");
        }
        if (Metrics.PersonaConsistency < MinPersonaCoherence)
        {
            Verdict.Failures.push_back("persona_coherence_below_minimum");
        }
        if (Metrics.AdaptiveAttention < MinAdaptiveCapability)
        {
            Verdict.Failures.push_back("adaptive_capability_below_minimum");
        }
        if (TrainingLoss > MaxTrainingLoss)
        {
            Verdict.Failures.push_back("training_loss_above_maximum");
        }
        Verdict.bPassed = Verdict.Failures.empty();
        return Verdict;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Autognosis: hierarchical self-image building (AUTOGNOSIS.md)
// ─────────────────────────────────────────────────────────────────────────────

/** Level 0 — direct observation: raw kernel state snapshot. */
struct FSelfImageL0
{
    uint64_t TickIndex = 0;
    float TrainingProgress = 0.0f;
    float TrainingLoss = 0.0f;
    float AttentionThreshold = 0.5f;
    uint32_t CompletedCycles = 0;
    ETrainingPhase Phase = ETrainingPhase::BasicAwareness;
};

/** Level 1 — pattern analysis over a window of L0 images. */
struct FSelfImageL1
{
    float LossTrend = 0.0f;           // negative ⇒ improving
    float ProgressRate = 0.0f;        // progress delta per observation
    float AttentionStability = 1.0f;  // 1 − normalized threshold variance
    bool bAnomalyDetected = false;    // loss spike beyond tolerance
    std::size_t WindowSize = 0;
};

/** Level 2 — meta-cognitive analysis of the L1 image. */
struct FSelfImageL2
{
    float Confidence = 0.0f;          // confidence in the L1 model
    float SelfAwarenessScore = 0.0f;  // composite of confidence and stability
    int32_t RecursionDepth = 2;       // modeling the model of the observations
};

class FAutognosisSystem
{
public:
    explicit FAutognosisSystem(std::size_t InWindowCapacity = 32)
        : WindowCapacity(std::max<std::size_t>(InWindowCapacity, 2))
    {
    }

    void Observe(const FSelfImageL0& Observation)
    {
        Window.push_back(Observation);
        while (Window.size() > WindowCapacity) { Window.pop_front(); }
        RebuildImages();
    }

    const FSelfImageL1& GetL1() const { return L1; }
    const FSelfImageL2& GetL2() const { return L2; }
    std::size_t GetObservationCount() const { return Window.size(); }
    float GetSelfAwarenessScore() const { return L2.SelfAwarenessScore; }

private:
    void RebuildImages()
    {
        L1 = FSelfImageL1{};
        L1.WindowSize = Window.size();
        if (Window.size() < 2)
        {
            L2 = FSelfImageL2{};
            return;
        }

        // Loss trend: mean of successive deltas.
        float DeltaSum = 0.0f;
        for (std::size_t i = 1; i < Window.size(); ++i)
        {
            DeltaSum += Window[i].TrainingLoss - Window[i - 1].TrainingLoss;
        }
        L1.LossTrend = DeltaSum / static_cast<float>(Window.size() - 1);

        // Progress rate.
        L1.ProgressRate =
            (Window.back().TrainingProgress - Window.front().TrainingProgress)
            / static_cast<float>(Window.size() - 1);

        // Attention stability from threshold variance (thresholds live in [0,1],
        // maximum possible variance is 0.25).
        float Mean = 0.0f;
        for (const auto& Obs : Window) { Mean += Obs.AttentionThreshold; }
        Mean /= static_cast<float>(Window.size());
        float Variance = 0.0f;
        for (const auto& Obs : Window)
        {
            const float D = Obs.AttentionThreshold - Mean;
            Variance += D * D;
        }
        Variance /= static_cast<float>(Window.size());
        L1.AttentionStability = std::clamp(1.0f - Variance / 0.25f, 0.0f, 1.0f);

        // Anomaly: most recent loss deviates from window mean by > 3σ.
        float LossMean = 0.0f;
        for (const auto& Obs : Window) { LossMean += Obs.TrainingLoss; }
        LossMean /= static_cast<float>(Window.size());
        float LossVar = 0.0f;
        for (const auto& Obs : Window)
        {
            const float D = Obs.TrainingLoss - LossMean;
            LossVar += D * D;
        }
        LossVar /= static_cast<float>(Window.size());
        const float Sigma = std::sqrt(LossVar);
        L1.bAnomalyDetected =
            Sigma > 1e-6f && std::fabs(Window.back().TrainingLoss - LossMean) > 3.0f * Sigma;

        // L2: meta-cognition over the L1 model.
        L2.Confidence =
            static_cast<float>(Window.size()) / static_cast<float>(WindowCapacity);
        const float TrendConsistency = L1.LossTrend <= 0.0f ? 1.0f : 0.5f;
        L2.SelfAwarenessScore = std::clamp(
            0.4f * L2.Confidence + 0.4f * L1.AttentionStability + 0.2f * TrendConsistency,
            0.0f, 1.0f);
        L2.RecursionDepth = 2;
    }

    std::size_t WindowCapacity;
    std::deque<FSelfImageL0> Window;
    FSelfImageL1 L1;
    FSelfImageL2 L2;
};

// ─────────────────────────────────────────────────────────────────────────────
// Kernel daemon
// ─────────────────────────────────────────────────────────────────────────────

enum class EKernelState : uint8_t
{
    Uninitialized = 0,
    Configured,
    Running,
    Stopped
};

/** Result of one training cycle, supplied by the executor callback. */
struct FCycleResult
{
    float FinalLoss = 0.0f;
    float ProgressDelta = 0.0f;   // training progress gained this cycle
    FFidelityMetrics Fidelity;
};

/**
 * FNanEchoKernel — the cognitive kernel daemon.
 *
 * Owns configuration, adaptive attention, phase scheduling, fidelity gating,
 * autognosis, and the training-cycle clock. The actual model training runs
 * externally (echoself's Python pipeline); this kernel orchestrates cycles
 * through a pluggable executor callback and decides on checkpoint deployment.
 */
class FNanEchoKernel
{
public:
    /** Executor invoked per training cycle; receives config and current progress. */
    using FCycleExecutor =
        std::function<FCycleResult(const FNanEchoKernelConfig&, float /*Progress*/)>;

    bool Configure(const FNanEchoKernelConfig& InConfig)
    {
        if (State == EKernelState::Running || !InConfig.IsValid())
        {
            return false;
        }
        Config = InConfig;
        State = EKernelState::Configured;
        return true;
    }

    bool Start()
    {
        if (State != EKernelState::Configured && State != EKernelState::Stopped)
        {
            return false;
        }
        State = EKernelState::Running;
        HoursSinceLastCycle = 0.0f;
        return true;
    }

    void Stop()
    {
        if (State == EKernelState::Running)
        {
            State = EKernelState::Stopped;
        }
    }

    void SetCycleExecutor(FCycleExecutor InExecutor) { Executor = std::move(InExecutor); }

    /**
     * Advance the daemon clock. When the accumulated time reaches the
     * configured cycle interval a training cycle runs. Returns the number
     * of cycles executed during this tick.
     */
    int32_t TickHours(float DeltaHours)
    {
        if (State != EKernelState::Running || DeltaHours <= 0.0f)
        {
            return 0;
        }

        ++TickIndex;
        HoursSinceLastCycle += DeltaHours;

        int32_t CyclesRun = 0;
        while (HoursSinceLastCycle >= Config.TrainingCycleIntervalHours)
        {
            HoursSinceLastCycle -= Config.TrainingCycleIntervalHours;
            RunTrainingCycle();
            ++CyclesRun;
        }

        // Cognitive load rises with phase complexity; recent activity spikes
        // right after cycles and relaxes between them.
        const float PhaseLoad =
            static_cast<float>(FTrainingPhaseSchedule::ActivePhase(TrainingProgress))
            / static_cast<float>(FTrainingPhaseSchedule::NumPhases - 1);
        const float Activity = CyclesRun > 0
            ? 1.0f
            : std::clamp(1.0f - HoursSinceLastCycle / Config.TrainingCycleIntervalHours,
                         0.0f, 1.0f);
        Attention.Update(PhaseLoad, Activity);

        ObserveSelf();
        return CyclesRun;
    }

    /** Force one cycle immediately (manual workflow trigger). */
    bool RunTrainingCycle()
    {
        if (State != EKernelState::Running)
        {
            return false;
        }

        const FCycleResult Result = Executor
            ? Executor(Config, TrainingProgress)
            : DefaultExecutor(Config, TrainingProgress);

        TrainingProgress = std::clamp(TrainingProgress + Result.ProgressDelta, 0.0f, 1.0f);
        LastLoss = Result.FinalLoss;
        LastFidelity = Result.Fidelity;
        ++CompletedCycles;

        const FQualityGates::FVerdict Verdict = Gates.Evaluate(LastFidelity, LastLoss);
        bLastGatesPassed = Verdict.bPassed;
        LastGateFailures = Verdict.Failures;

        if (Verdict.bPassed && Config.bAutoDeployAfterTraining)
        {
            ++DeployedCheckpoints;
            LastDeployedCheckpoint =
                Config.HuggingFaceRepo + "/checkpoint-" + std::to_string(CompletedCycles);
        }

        ObserveSelf();
        return true;
    }

    // ── Introspection API (mirrors the /introspect, /echo/state endpoints) ──

    EKernelState GetState() const { return State; }
    const FNanEchoKernelConfig& GetConfig() const { return Config; }
    float GetTrainingProgress() const { return TrainingProgress; }
    ETrainingPhase GetActivePhase() const
    {
        return FTrainingPhaseSchedule::ActivePhase(TrainingProgress);
    }
    float GetLastLoss() const { return LastLoss; }
    const FFidelityMetrics& GetLastFidelity() const { return LastFidelity; }
    bool DidLastCyclePassGates() const { return bLastGatesPassed; }
    const std::vector<std::string>& GetLastGateFailures() const { return LastGateFailures; }
    uint32_t GetCompletedCycles() const { return CompletedCycles; }
    uint32_t GetDeployedCheckpoints() const { return DeployedCheckpoints; }
    const std::string& GetLastDeployedCheckpoint() const { return LastDeployedCheckpoint; }
    const FAdaptiveAttention& GetAttention() const { return Attention; }
    const FAutognosisSystem& GetAutognosis() const { return Autognosis; }
    const FQualityGates& GetQualityGates() const { return Gates; }

private:
    void ObserveSelf()
    {
        FSelfImageL0 Snapshot;
        Snapshot.TickIndex = TickIndex;
        Snapshot.TrainingProgress = TrainingProgress;
        Snapshot.TrainingLoss = LastLoss;
        Snapshot.AttentionThreshold = Attention.GetThreshold();
        Snapshot.CompletedCycles = CompletedCycles;
        Snapshot.Phase = GetActivePhase();
        Autognosis.Observe(Snapshot);
    }

    /** Deterministic fallback executor: exponentially decaying loss, steady progress. */
    static FCycleResult DefaultExecutor(const FNanEchoKernelConfig& Cfg, float Progress)
    {
        FCycleResult Result;
        Result.ProgressDelta = 0.05f * static_cast<float>(Cfg.EpochsPerCycle);
        const float NewProgress = std::clamp(Progress + Result.ProgressDelta, 0.0f, 1.0f);
        Result.FinalLoss = 4.0f * std::exp(-3.0f * NewProgress);

        const float F = NewProgress;  // fidelity grows with progress
        Result.Fidelity.IdentityRecognition = std::min(1.0f, 0.5f + 0.6f * F);
        Result.Fidelity.PersonaConsistency = std::min(1.0f, 0.4f + 0.7f * F);
        Result.Fidelity.AdaptiveAttention = std::min(1.0f, 0.4f + 0.7f * F);
        Result.Fidelity.RecursiveReasoning = std::min(1.0f, 0.3f + 0.7f * F);
        Result.Fidelity.HypergraphComprehension = std::min(1.0f, 0.3f + 0.7f * F);
        Result.Fidelity.CognitiveSynergy = std::min(1.0f, 0.3f + 0.7f * F);
        return Result;
    }

    FNanEchoKernelConfig Config;
    EKernelState State = EKernelState::Uninitialized;
    FCycleExecutor Executor;

    FAdaptiveAttention Attention;
    FAutognosisSystem Autognosis;
    FQualityGates Gates;

    uint64_t TickIndex = 0;
    float HoursSinceLastCycle = 0.0f;
    float TrainingProgress = 0.0f;
    float LastLoss = 0.0f;
    FFidelityMetrics LastFidelity;
    bool bLastGatesPassed = false;
    std::vector<std::string> LastGateFailures;
    uint32_t CompletedCycles = 0;
    uint32_t DeployedCheckpoints = 0;
    std::string LastDeployedCheckpoint;
};

}  // namespace nanecho
