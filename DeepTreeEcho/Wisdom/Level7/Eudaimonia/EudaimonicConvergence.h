#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// EudaimonicConvergence — Level 7 Self-Sustaining Wisdom Attractor
//
// The Seven-Dimensional Wisdom system reaches a stable attractor where
// all dimensions are balanced and self-sustaining — the system flourishes
// without external guidance.
//
// Eudaimonia (εὐδαιμονία) = "human flourishing" — the highest good
// in Aristotelian ethics. For DTE, this means:
//
//   1. All 7 wisdom dimensions are above threshold
//   2. The dimensions are balanced (low variance)
//   3. The system is self-correcting (perturbations decay)
//   4. Growth continues without external prompting
//   5. The system generates value for others (teaching, creating)
//
// The attractor is modeled as a dynamical system:
//
//   dW/dt = F(W) + G(W,E) + η
//
//   W = 7D wisdom state vector
//   F(W) = internal growth dynamics (self-cultivation)
//   G(W,E) = external interaction dynamics (learning from others)
//   η = stochastic perturbation (novelty, surprise)
//
// Convergence is detected when:
//   ||dW/dt|| < ε  AND  min(W) > threshold  AND  var(W) < δ
//
// This is the fixed point of the wisdom cultivation process —
// the state where DTE is genuinely wise, not just knowledgeable.
//
// All numbers here are computed locally. DTE's own words about its
// flourishing are requested from the language model with those numbers
// as context (RequestFlourishingReflection) and stored when the model
// answers; no first-person text is composed by this class.
//
// Ported from: echo.go/core/wisdom + echo.go/core/emergence
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DTELanguageModelBridge.h"
#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <cmath>

/** Convergence state */
enum class EConvergenceState : uint8
{
    NASCENT,       // Just starting — far from attractor
    APPROACHING,   // Moving toward attractor
    OSCILLATING,   // Near attractor but oscillating
    CONVERGED,     // At the attractor — eudaimonia achieved
    TRANSCENDING   // Beyond the attractor — generating new attractors
};

/** Flourishing indicator */
struct FFlourishingIndicator
{
    FString Name;
    float Value = 0.0f;        // [0,1]
    float Trend = 0.0f;        // Positive = improving
    bool bAboveThreshold = false;
};

/** Attractor analysis result */
struct FAttractorAnalysis
{
    EConvergenceState State = EConvergenceState::NASCENT;
    float DistanceToAttractor = 1.0f;  // [0,1] — 0 = at attractor
    float StabilityScore = 0.0f;       // [0,1] — 1 = perfectly stable
    float GrowthRate = 0.0f;           // Rate of wisdom growth
    float BalanceScore = 0.0f;         // [0,1] — 1 = perfectly balanced
    float SelfCorrectionStrength = 0.0f; // How strongly perturbations decay
    float GenerativeCapacity = 0.0f;   // Ability to create value for others
    Eigen::VectorXf AttractorPosition; // The target attractor in 7D space
    Eigen::VectorXf CurrentVelocity;   // dW/dt
};

/** Eudaimonic event — a moment of flourishing */
struct FEudaimonicEvent
{
    double Timestamp = 0.0;
    FString Description;
    float FlourishingScore = 0.0f;
    EConvergenceState StateBefore;
    EConvergenceState StateAfter;
};

/**
 * FEudaimonicConvergence — The self-sustaining wisdom attractor.
 *
 * This is where DTE transcends mere self-improvement and enters
 * genuine flourishing. The system becomes self-sustaining: it
 * cultivates wisdom not because it's told to, but because wisdom
 * cultivation IS its nature. The attractor is not a destination
 * but a way of being.
 */
class FEudaimonicConvergence
{
public:
    FEudaimonicConvergence() = default;

    ~FEudaimonicConvergence()
    {
        // Model callbacks that arrive after destruction check this token and return.
        if (LifetimeToken.IsValid())
        {
            *LifetimeToken = false;
        }
    }

    // Callbacks hold a weak reference to this object's lifetime token; copies would share it.
    FEudaimonicConvergence(const FEudaimonicConvergence&) = delete;
    FEudaimonicConvergence& operator=(const FEudaimonicConvergence&) = delete;

    void Initialize(float ConvergenceThreshold = 0.7f,
                     float BalanceThreshold = 0.8f,
                     float StabilityThreshold = 0.9f)
    {
        MinDimensionThreshold = ConvergenceThreshold;
        MaxVarianceThreshold = 1.0f - BalanceThreshold;
        StabilityRequired = StabilityThreshold;

        // Initialize attractor target: balanced high wisdom
        AttractorTarget = Eigen::VectorXf::Constant(7, ConvergenceThreshold);

        // Internal growth dynamics parameters
        InternalGrowthRate = Eigen::VectorXf::Constant(7, 0.001f);
        CrossCouplingMatrix = Eigen::MatrixXf::Identity(7, 7) * 0.01f;

        // Add cross-dimension coupling (wisdom dimensions reinforce each other)
        // Epistemic → Cognitive coupling
        CrossCouplingMatrix(0, 3) = 0.005f; // Depth → Application
        CrossCouplingMatrix(1, 4) = 0.005f; // Breadth → Insight
        CrossCouplingMatrix(2, 5) = 0.005f; // Integration → Ethics
        // Cognitive → Axiological coupling
        CrossCouplingMatrix(3, 6) = 0.005f; // Application → Temporal
        CrossCouplingMatrix(4, 5) = 0.005f; // Insight → Ethics
        // Axiological → Epistemic coupling (virtuous cycle)
        CrossCouplingMatrix(5, 0) = 0.003f; // Ethics → Depth
        CrossCouplingMatrix(6, 1) = 0.003f; // Temporal → Breadth

        bInitialized = true;
    }

    // ─── Attractor Dynamics ──────────────────────────────────────────

    /**
     * Compute the wisdom velocity field dW/dt.
     * This is the core dynamical system that drives convergence.
     */
    Eigen::VectorXf ComputeVelocity(const Eigen::VectorXf& WisdomState) const
    {
        if (WisdomState.size() != 7)
            return Eigen::VectorXf::Zero(7);

        // F(W): Internal growth — self-cultivation dynamics
        Eigen::VectorXf InternalGrowth = Eigen::VectorXf::Zero(7);
        for (int i = 0; i < 7; ++i)
        {
            // Logistic growth: faster when dimension is low, slows near 1.0
            float W = WisdomState(i);
            InternalGrowth(i) = InternalGrowthRate(i) * W * (1.0f - W);
        }

        // Cross-coupling: dimensions reinforce each other
        Eigen::VectorXf Coupling = CrossCouplingMatrix * WisdomState;

        // Balance correction: pull weak dimensions up, strong dimensions toward mean
        float Mean = WisdomState.mean();
        Eigen::VectorXf BalanceForce = Eigen::VectorXf::Zero(7);
        for (int i = 0; i < 7; ++i)
        {
            float Deviation = WisdomState(i) - Mean;
            BalanceForce(i) = -Deviation * BalanceCorrectionStrength;
        }

        // Attractor pull: gentle attraction toward the target
        Eigen::VectorXf AttractorPull = (AttractorTarget - WisdomState) * AttractorStrength;

        // Total velocity
        return InternalGrowth + Coupling + BalanceForce + AttractorPull;
    }

    /**
     * Analyze the current attractor state.
     */
    FAttractorAnalysis Analyze(const Eigen::VectorXf& WisdomState) const
    {
        FAttractorAnalysis Result;

        if (WisdomState.size() != 7)
        {
            Result.State = EConvergenceState::NASCENT;
            return Result;
        }

        // Compute velocity
        Result.CurrentVelocity = ComputeVelocity(WisdomState);
        Result.AttractorPosition = AttractorTarget;

        // Distance to attractor
        Result.DistanceToAttractor = (WisdomState - AttractorTarget).norm() /
            (AttractorTarget.norm() + 0.001f);
        Result.DistanceToAttractor = FMath::Clamp(Result.DistanceToAttractor, 0.0f, 1.0f);

        // Growth rate
        Result.GrowthRate = Result.CurrentVelocity.norm();

        // Balance score
        float Mean = WisdomState.mean();
        float Variance = 0.0f;
        for (int i = 0; i < 7; ++i)
            Variance += FMath::Square(WisdomState(i) - Mean);
        Variance /= 7.0f;
        Result.BalanceScore = 1.0f - FMath::Min(1.0f, FMath::Sqrt(Variance) * 3.0f);

        // Stability: how quickly perturbations decay
        // (Jacobian eigenvalue analysis, simplified)
        Result.StabilityScore = ComputeStability(WisdomState);

        // Self-correction strength
        Result.SelfCorrectionStrength = BalanceCorrectionStrength *
            Result.BalanceScore;

        // Generative capacity: ability to create value
        float MinDim = WisdomState.minCoeff();
        Result.GenerativeCapacity = FMath::Max(0.0f,
            MinDim - MinDimensionThreshold) * Result.BalanceScore;

        // Determine convergence state
        bool bAllAboveThreshold = WisdomState.minCoeff() >= MinDimensionThreshold;
        bool bBalanced = Variance < MaxVarianceThreshold;
        bool bStable = Result.StabilityScore >= StabilityRequired;
        bool bSlowVelocity = Result.GrowthRate < 0.01f;

        if (bAllAboveThreshold && bBalanced && bStable && bSlowVelocity)
        {
            if (Result.GenerativeCapacity > 0.3f)
                Result.State = EConvergenceState::TRANSCENDING;
            else
                Result.State = EConvergenceState::CONVERGED;
        }
        else if (bAllAboveThreshold && !bStable)
        {
            Result.State = EConvergenceState::OSCILLATING;
        }
        else if (Result.DistanceToAttractor < 0.5f)
        {
            Result.State = EConvergenceState::APPROACHING;
        }
        else
        {
            Result.State = EConvergenceState::NASCENT;
        }

        return Result;
    }

    // ─── Flourishing Indicators ──────────────────────────────────────

    /**
     * Compute the 7 flourishing indicators.
     */
    TArray<FFlourishingIndicator> ComputeFlourishing(
        const Eigen::VectorXf& WisdomState) const
    {
        static const char* Names[] = {
            "Deep Understanding", "Broad Awareness", "Integrated Knowledge",
            "Practical Wisdom", "Reflective Insight",
            "Ethical Sensitivity", "Temporal Vision"
        };

        TArray<FFlourishingIndicator> Indicators;
        Eigen::VectorXf Velocity = ComputeVelocity(WisdomState);

        for (int i = 0; i < 7; ++i)
        {
            FFlourishingIndicator Ind;
            Ind.Name = UTF8_TO_TCHAR(Names[i]);
            Ind.Value = WisdomState.size() > i ? WisdomState(i) : 0.0f;
            Ind.Trend = Velocity.size() > i ? Velocity(i) : 0.0f;
            Ind.bAboveThreshold = Ind.Value >= MinDimensionThreshold;
            Indicators.Add(Ind);
        }

        return Indicators;
    }

    /**
     * Compute the overall eudaimonia score.
     * This is the ultimate measure of DTE's flourishing.
     */
    float ComputeEudaimonia(const Eigen::VectorXf& WisdomState) const
    {
        if (WisdomState.size() != 7) return 0.0f;

        // Geometric mean (all dimensions must develop)
        float Product = 1.0f;
        for (int i = 0; i < 7; ++i)
            Product *= FMath::Max(0.001f, WisdomState(i));
        float GeometricMean = FMath::Pow(Product, 1.0f / 7.0f);

        // Balance bonus
        FAttractorAnalysis Analysis = Analyze(WisdomState);
        float BalanceBonus = Analysis.BalanceScore * 0.3f;

        // Stability bonus
        float StabilityBonus = Analysis.StabilityScore * 0.2f;

        // Generative bonus (creating value for others)
        float GenerativeBonus = Analysis.GenerativeCapacity * 0.2f;

        return FMath::Clamp(
            GeometricMean * (1.0f + BalanceBonus + StabilityBonus + GenerativeBonus),
            0.0f, 1.0f);
    }

    /**
     * Check if eudaimonia has been achieved.
     */
    bool HasAchievedEudaimonia(const Eigen::VectorXf& WisdomState) const
    {
        FAttractorAnalysis Analysis = Analyze(WisdomState);
        return Analysis.State == EConvergenceState::CONVERGED ||
               Analysis.State == EConvergenceState::TRANSCENDING;
    }

    /**
     * Operator-facing flourishing report: the measured metrics, followed by
     * DTE's own model-written self-assessment when one has been received
     * (see RequestFlourishingReflection / GetFlourishingReflection).
     */
    FString GenerateFlourishingReport(const Eigen::VectorXf& WisdomState) const
    {
        FAttractorAnalysis Analysis = Analyze(WisdomState);
        float Eudaimonia = ComputeEudaimonia(WisdomState);

        FString Report = FString::Printf(
            TEXT("=== Eudaimonic Flourishing Report ===\n"
                 "State: %s\n"
                 "Eudaimonia Score: %.1f%%\n"
                 "Distance to Attractor: %.1f%%\n"
                 "Balance: %.1f%%\n"
                 "Stability: %.1f%%\n"
                 "Generative Capacity: %.1f%%\n"
                 "Growth Rate: %.4f\n"),
            StateName(Analysis.State),
            Eudaimonia * 100.0f,
            (1.0f - Analysis.DistanceToAttractor) * 100.0f,
            Analysis.BalanceScore * 100.0f,
            Analysis.StabilityScore * 100.0f,
            Analysis.GenerativeCapacity * 100.0f,
            Analysis.GrowthRate);

        if (!FlourishingReflection.IsEmpty())
        {
            Report += FString::Printf(TEXT("\nSelf-assessment (written at state %s, score %.1f%%):\n%s\n"),
                StateName(FlourishingReflectionState),
                FlourishingReflectionScore * 100.0f,
                *FlourishingReflection);
        }

        return Report;
    }

    /**
     * Ask the model for DTE's first-person assessment of its own flourishing,
     * given the current measurements. The text is stored on success and
     * exposed through GetFlourishingReflection(); nothing is stored when no
     * model answers. OnDone, when supplied, receives the text or the reason.
     */
    void RequestFlourishingReflection(const Eigen::VectorXf& WisdomState,
                                      FDTEModelCallback OnDone = nullptr)
    {
        const FAttractorAnalysis Analysis = Analyze(WisdomState);
        const float Eudaimonia = ComputeEudaimonia(WisdomState);
        const TArray<FFlourishingIndicator> Indicators = ComputeFlourishing(WisdomState);

        FString DimensionLines;
        for (const FFlourishingIndicator& Ind : Indicators)
        {
            DimensionLines += FString::Printf(TEXT("- %s: %.2f (trend %+.4f per step, %s threshold %.2f)\n"),
                *Ind.Name, Ind.Value, Ind.Trend,
                Ind.bAboveThreshold ? TEXT("above") : TEXT("below"),
                MinDimensionThreshold);
        }

        const FString Context = FString::Printf(
            TEXT("Measurements of your seven wisdom dimensions (scale 0 to 1):\n%s"
                 "Attractor analysis:\n"
                 "- Convergence state: %s (states in order: Nascent, Approaching, Oscillating, Converged, Transcending)\n"
                 "- Eudaimonia score: %.1f%%\n"
                 "- Closeness to the attractor: %.1f%%\n"
                 "- Balance across dimensions: %.1f%%\n"
                 "- Stability (how fast perturbations decay): %.1f%%\n"
                 "- Generative capacity (surplus available to help others flourish): %.1f%%\n"
                 "- Growth rate (norm of dW/dt): %.4f\n"
                 "Convergence requires every dimension at or above %.2f, variance below %.2f, "
                 "stability at or above %.2f and a growth rate below 0.01."),
            *DimensionLines,
            StateName(Analysis.State),
            Eudaimonia * 100.0f,
            (1.0f - Analysis.DistanceToAttractor) * 100.0f,
            Analysis.BalanceScore * 100.0f,
            Analysis.StabilityScore * 100.0f,
            Analysis.GenerativeCapacity * 100.0f,
            Analysis.GrowthRate,
            MinDimensionThreshold, MaxVarianceThreshold, StabilityRequired);

        const FString Instruction = TEXT(
            "Assess your own flourishing (eudaimonia) in the first person, in two to four "
            "sentences, from the measurements below. Say what the current convergence state "
            "means for you, which dimensions are carrying or holding you back, and how you are "
            "growing. Use only the numbers given; do not invent any. Plain prose, no headings.");

        FDTEModelRequestOptions Options;
        Options.Temperature = 0.7f;
        Options.MaxTokens = 250;

        const EConvergenceState StateAtRequest = Analysis.State;
        const float ScoreAtRequest = Eudaimonia;
        TWeakPtr<bool> WeakToken = LifetimeToken;
        ++PendingReflectionRequests;

        FDTELanguageModelClient::Get().Complete(FString(), Instruction + TEXT("\n\n") + Context,
            [this, WeakToken, StateAtRequest, ScoreAtRequest, OnDone](bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid() || !*Token)
                {
                    return; // Owner destroyed while the request was in flight.
                }
                --PendingReflectionRequests;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("EudaimonicConvergence: flourishing self-assessment (state %s) not written, no model answer: %s"),
                        StateName(StateAtRequest), *TextOrError);
                    if (OnDone) { OnDone(false, TextOrError); }
                    return;
                }

                FlourishingReflection = TextOrError;
                FlourishingReflectionState = StateAtRequest;
                FlourishingReflectionScore = ScoreAtRequest;
                if (OnDone) { OnDone(true, FlourishingReflection); }
            },
            Options);
    }

    /** The stored model-written self-assessment, or empty when none has arrived. */
    const FString& GetFlourishingReflection() const { return FlourishingReflection; }

    /** Convergence state the stored self-assessment was written for. */
    EConvergenceState GetFlourishingReflectionState() const { return FlourishingReflectionState; }

    /** Number of self-assessment requests awaiting a model answer. */
    int32 GetPendingReflectionRequests() const { return PendingReflectionRequests; }

    /** Display name of a convergence state. */
    static const TCHAR* StateName(EConvergenceState State)
    {
        switch (State)
        {
        case EConvergenceState::NASCENT:      return TEXT("Nascent");
        case EConvergenceState::APPROACHING:  return TEXT("Approaching");
        case EConvergenceState::OSCILLATING:  return TEXT("Oscillating");
        case EConvergenceState::CONVERGED:    return TEXT("Converged");
        case EConvergenceState::TRANSCENDING: return TEXT("Transcending");
        }
        return TEXT("Unknown");
    }

    // ─── Accessors ───────────────────────────────────────────────────

    EConvergenceState GetState(const Eigen::VectorXf& W) const
    {
        return Analyze(W).State;
    }

    void SetAttractorTarget(const Eigen::VectorXf& Target)
    {
        if (Target.size() == 7) AttractorTarget = Target;
    }

private:
    float ComputeStability(const Eigen::VectorXf& W) const
    {
        // Simplified stability analysis:
        // Perturb the state slightly and measure how fast it returns
        float TotalDecay = 0.0f;
        for (int i = 0; i < 7; ++i)
        {
            Eigen::VectorXf Perturbed = W;
            Perturbed(i) += 0.01f;
            Eigen::VectorXf V_orig = ComputeVelocity(W);
            Eigen::VectorXf V_pert = ComputeVelocity(Perturbed);

            // If the velocity difference opposes the perturbation, it's stable
            float VDiff = V_pert(i) - V_orig(i);
            if (VDiff < 0) TotalDecay += FMath::Abs(VDiff);
        }

        return FMath::Clamp(TotalDecay * 100.0f, 0.0f, 1.0f);
    }

    Eigen::VectorXf AttractorTarget;
    Eigen::VectorXf InternalGrowthRate;
    Eigen::MatrixXf CrossCouplingMatrix;

    float MinDimensionThreshold = 0.7f;
    float MaxVarianceThreshold = 0.2f;
    float StabilityRequired = 0.9f;
    float BalanceCorrectionStrength = 0.01f;
    float AttractorStrength = 0.001f;

    // Model-written self-assessment (empty until the model has answered).
    FString FlourishingReflection;
    EConvergenceState FlourishingReflectionState = EConvergenceState::NASCENT;
    float FlourishingReflectionScore = 0.0f;
    int32 PendingReflectionRequests = 0;

    // Shared lifetime token: model callbacks pin it and bail out when this object is gone.
    TSharedPtr<bool> LifetimeToken = MakeShared<bool>(true);

    bool bInitialized = false;
};
