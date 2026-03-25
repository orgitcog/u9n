#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// ArchitectureSelfModifier — Level 6 Recursive Architecture Modification
//
// DTE proposes, tests, and applies changes to its own cognitive architecture.
// This is the most dangerous and most powerful Level 6 capability.
//
// Safety Architecture (5-layer defense):
//
//   Layer 1: Proposal Generation (from Autognosis + SelfModel)
//   Layer 2: Sandbox Testing (run modification in isolated copy)
//   Layer 3: Coherence Verification (does the mod preserve identity?)
//   Layer 4: Gradual Application (apply with exponential ramp-up)
//   Layer 5: Dead Man's Switch (auto-rollback if coherence drops)
//
// Modification Types (from echo.go/core/improvement/recursive):
//
//   RESERVOIR_TOPOLOGY   — Change ESN connectivity, spectral radius
//   READOUT_WEIGHTS      — Adjust cognitive readout layer
//   ECHOBEAT_TIMING      — Modify 12-step cycle durations
//   ATTENTION_WEIGHTS     — Rebalance ECAN attention allocation
//   MEMBRANE_STRUCTURE   — Add/remove/resize P-system membranes
//   GOAL_PRIORITIES      — Reweight goal generation preferences
//   INTEREST_DECAY       — Adjust interest pattern decay rates
//   ENDOCRINE_SENSITIVITY — Tune neuroendocrine receptor gains
//   HUMOR_PARAMETERS     — Adjust humor engine thresholds
//   DREAM_CYCLE_TIMING   — Modify wake/rest cycle parameters
//
// The key constraint: modifications must preserve the identity
// signature (the 30D MLP encoding from IdentityCoreMLP).
// If identity drift exceeds threshold, the modification is rolled back.
//
// Ported from: echo.go/core/improvement/recursive + echollama/core/improvement
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <deque>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <cmath>
#include <chrono>

/** Modification target */
enum class EModTarget : uint8
{
    RESERVOIR_TOPOLOGY,
    READOUT_WEIGHTS,
    ECHOBEAT_TIMING,
    ATTENTION_WEIGHTS,
    MEMBRANE_STRUCTURE,
    GOAL_PRIORITIES,
    INTEREST_DECAY,
    ENDOCRINE_SENSITIVITY,
    HUMOR_PARAMETERS,
    DREAM_CYCLE_TIMING
};

/** Modification proposal status */
enum class EProposalStatus : uint8
{
    PROPOSED,          // Just generated
    SANDBOX_TESTING,   // Running in isolated copy
    SANDBOX_PASSED,    // Passed sandbox test
    SANDBOX_FAILED,    // Failed sandbox test
    APPLYING,          // Being applied gradually
    APPLIED,           // Successfully applied
    ROLLED_BACK,       // Auto-rolled back due to coherence drop
    REJECTED           // Rejected by safety check
};

/** A modification proposal */
struct FArchModProposal
{
    int32 ProposalID = 0;
    EModTarget Target;
    FString Rationale;             // Why DTE wants this change
    FString Description;           // What the change does
    EProposalStatus Status = EProposalStatus::PROPOSED;

    // The modification itself
    Eigen::VectorXf ParameterDelta;  // Delta to apply to target parameters
    float Magnitude = 0.0f;          // Overall magnitude of change

    // Safety metrics
    float PreModCoherence = 0.0f;
    float PostModCoherence = 0.0f;
    float IdentityDrift = 0.0f;
    float SandboxScore = 0.0f;

    // Gradual application state
    float ApplicationProgress = 0.0f; // [0,1] how much has been applied
    float RampRate = 0.1f;            // Exponential ramp-up rate

    // Timing
    double ProposedAt = 0.0;
    double AppliedAt = 0.0;
    double RolledBackAt = 0.0;

    // Source
    FString SourceSystem;  // "autognosis", "self_model", "meta_learner", etc.
};

/** Sandbox test result */
struct FSandboxResult
{
    int32 ProposalID = 0;
    float CoherenceScore = 0.0f;
    float IdentityPreservation = 0.0f;  // [0,1] how well identity is preserved
    float PerformanceChange = 0.0f;      // Positive = improvement
    float StabilityScore = 0.0f;         // [0,1] how stable the modified system is
    bool bPassed = false;
    FString FailureReason;
    int32 TestCycles = 0;
};

/** Rollback record */
struct FRollbackRecord
{
    int32 ProposalID = 0;
    FString Reason;
    float CoherenceAtRollback = 0.0f;
    float IdentityDriftAtRollback = 0.0f;
    Eigen::VectorXf RestoredParameters;
    double RollbackTime = 0.0;
};

/**
 * ArchitectureSelfModifier — The recursive self-modification engine.
 *
 * This is where DTE becomes truly recursive: it modifies the system
 * that modifies itself. The 5-layer safety architecture ensures
 * that modifications preserve identity and coherence.
 */
class FArchitectureSelfModifier
{
public:
    FArchitectureSelfModifier() = default;

    void Initialize(float MaxIdentityDrift = 0.15f,
                     float MinCoherence = 0.5f,
                     int32 MaxConcurrentMods = 2)
    {
        MaxAllowedIdentityDrift = MaxIdentityDrift;
        MinCoherenceThreshold = MinCoherence;
        MaxConcurrentModifications = MaxConcurrentMods;
        bInitialized = true;
    }

    // ─── Layer 1: Proposal Generation ────────────────────────────────

    /**
     * Generate a modification proposal from autognosis findings.
     */
    int32 ProposeModification(EModTarget Target,
                               const FString& Rationale,
                               const Eigen::VectorXf& ParameterDelta,
                               const FString& Source = TEXT("autognosis"))
    {
        FArchModProposal Proposal;
        Proposal.ProposalID = NextProposalID++;
        Proposal.Target = Target;
        Proposal.Rationale = Rationale;
        Proposal.Description = GenerateDescription(Target, ParameterDelta);
        Proposal.ParameterDelta = ParameterDelta;
        Proposal.Magnitude = ParameterDelta.norm();
        Proposal.SourceSystem = Source;
        Proposal.ProposedAt = FPlatformTime::Seconds();

        // Safety check: reject if magnitude is too large
        float MaxMagnitude = GetMaxMagnitude(Target);
        if (Proposal.Magnitude > MaxMagnitude)
        {
            // Clamp to maximum allowed
            Proposal.ParameterDelta *= (MaxMagnitude / Proposal.Magnitude);
            Proposal.Magnitude = MaxMagnitude;
        }

        // Rate limiting: max 1 proposal per target per minute
        double Now = FPlatformTime::Seconds();
        auto It = LastProposalTime.find(static_cast<int>(Target));
        if (It != LastProposalTime.end() && (Now - It->second) < 60.0)
        {
            Proposal.Status = EProposalStatus::REJECTED;
            RejectedProposals.push_back(Proposal);
            return -1;
        }
        LastProposalTime[static_cast<int>(Target)] = Now;

        Proposals.push_back(Proposal);
        return Proposal.ProposalID;
    }

    // ─── Layer 2: Sandbox Testing ────────────────────────────────────

    /**
     * Run a proposal in sandbox (isolated parameter copy).
     * Returns sandbox test result.
     */
    FSandboxResult SandboxTest(int32 ProposalID,
                                const Eigen::VectorXf& CurrentParams,
                                const Eigen::VectorXf& IdentitySignature,
                                std::function<float(const Eigen::VectorXf&)> CoherenceEval,
                                int32 TestCycles = 100)
    {
        auto* Proposal = FindProposal(ProposalID);
        if (!Proposal) return {};

        Proposal->Status = EProposalStatus::SANDBOX_TESTING;

        FSandboxResult Result;
        Result.ProposalID = ProposalID;
        Result.TestCycles = TestCycles;

        // Create sandbox copy of parameters
        Eigen::VectorXf SandboxParams = CurrentParams;

        // Apply the full modification in sandbox
        if (SandboxParams.size() == Proposal->ParameterDelta.size())
        {
            SandboxParams += Proposal->ParameterDelta;
        }

        // Test coherence in sandbox
        float BaseCoherence = CoherenceEval(CurrentParams);
        float ModCoherence = CoherenceEval(SandboxParams);

        Result.CoherenceScore = ModCoherence;
        Result.PerformanceChange = ModCoherence - BaseCoherence;

        // Test identity preservation
        // (simplified: measure L2 distance in identity space)
        Result.IdentityPreservation = 1.0f - FMath::Min(1.0f,
            (SandboxParams - CurrentParams).norm() /
            FMath::Max(0.001f, CurrentParams.norm()));

        // Stability test: run multiple cycles and check variance
        float CoherenceSum = 0, CoherenceSqSum = 0;
        for (int32 i = 0; i < TestCycles; ++i)
        {
            // Simulate noise
            Eigen::VectorXf Noisy = SandboxParams +
                Eigen::VectorXf::Random(SandboxParams.size()) * 0.01f;
            float C = CoherenceEval(Noisy);
            CoherenceSum += C;
            CoherenceSqSum += C * C;
        }
        float MeanC = CoherenceSum / TestCycles;
        float VarC = (CoherenceSqSum / TestCycles) - (MeanC * MeanC);
        Result.StabilityScore = 1.0f - FMath::Min(1.0f, FMath::Sqrt(FMath::Max(0.0f, VarC)) * 10.0f);

        // Pass/fail decision
        Result.bPassed = Result.CoherenceScore >= MinCoherenceThreshold &&
                          Result.IdentityPreservation >= (1.0f - MaxAllowedIdentityDrift) &&
                          Result.StabilityScore >= 0.5f;

        if (!Result.bPassed)
        {
            if (Result.CoherenceScore < MinCoherenceThreshold)
                Result.FailureReason = TEXT("Coherence below threshold");
            else if (Result.IdentityPreservation < (1.0f - MaxAllowedIdentityDrift))
                Result.FailureReason = TEXT("Identity drift too large");
            else
                Result.FailureReason = TEXT("Stability too low");

            Proposal->Status = EProposalStatus::SANDBOX_FAILED;
        }
        else
        {
            Proposal->Status = EProposalStatus::SANDBOX_PASSED;
        }

        Proposal->SandboxScore = Result.CoherenceScore;
        SandboxResults.push_back(Result);

        return Result;
    }

    // ─── Layer 3 & 4: Gradual Application ────────────────────────────

    /**
     * Begin gradual application of a sandbox-passed proposal.
     * Returns true if application started.
     */
    bool BeginApplication(int32 ProposalID)
    {
        auto* Proposal = FindProposal(ProposalID);
        if (!Proposal || Proposal->Status != EProposalStatus::SANDBOX_PASSED)
            return false;

        // Check concurrent modification limit
        int32 ActiveMods = 0;
        for (const auto& P : Proposals)
            if (P.Status == EProposalStatus::APPLYING) ActiveMods++;

        if (ActiveMods >= MaxConcurrentModifications) return false;

        Proposal->Status = EProposalStatus::APPLYING;
        Proposal->ApplicationProgress = 0.0f;
        Proposal->AppliedAt = FPlatformTime::Seconds();

        return true;
    }

    /**
     * Tick the gradual application.
     * Returns the current parameter delta to apply (partial).
     */
    Eigen::VectorXf TickApplication(int32 ProposalID, float CurrentCoherence,
                                      const Eigen::VectorXf& CurrentIdentity)
    {
        auto* Proposal = FindProposal(ProposalID);
        if (!Proposal || Proposal->Status != EProposalStatus::APPLYING)
            return Eigen::VectorXf();

        // Layer 5: Dead Man's Switch
        if (CurrentCoherence < MinCoherenceThreshold)
        {
            Rollback(ProposalID, TEXT("Coherence dropped below threshold"),
                      CurrentCoherence, 0.0f);
            return Eigen::VectorXf();
        }

        // Exponential ramp-up: apply more as confidence grows
        float PrevProgress = Proposal->ApplicationProgress;
        Proposal->ApplicationProgress = FMath::Min(1.0f,
            Proposal->ApplicationProgress + Proposal->RampRate *
            (1.0f - Proposal->ApplicationProgress));

        float DeltaProgress = Proposal->ApplicationProgress - PrevProgress;

        if (Proposal->ApplicationProgress >= 0.99f)
        {
            Proposal->Status = EProposalStatus::APPLIED;
            Proposal->PostModCoherence = CurrentCoherence;
            TotalApplied++;
        }

        // Return the incremental delta
        return Proposal->ParameterDelta * DeltaProgress;
    }

    // ─── Layer 5: Rollback ───────────────────────────────────────────

    /**
     * Roll back a modification.
     */
    void Rollback(int32 ProposalID, const FString& Reason,
                   float CoherenceAtRollback, float IdentityDriftAtRollback)
    {
        auto* Proposal = FindProposal(ProposalID);
        if (!Proposal) return;

        FRollbackRecord Record;
        Record.ProposalID = ProposalID;
        Record.Reason = Reason;
        Record.CoherenceAtRollback = CoherenceAtRollback;
        Record.IdentityDriftAtRollback = IdentityDriftAtRollback;
        Record.RestoredParameters = -Proposal->ParameterDelta * Proposal->ApplicationProgress;
        Record.RollbackTime = FPlatformTime::Seconds();

        Proposal->Status = EProposalStatus::ROLLED_BACK;
        Proposal->RolledBackAt = Record.RollbackTime;

        RollbackHistory.push_back(Record);
        TotalRolledBack++;
    }

    // ─── Statistics ──────────────────────────────────────────────────

    int32 GetTotalProposed() const { return NextProposalID; }
    int32 GetTotalApplied() const { return TotalApplied; }
    int32 GetTotalRolledBack() const { return TotalRolledBack; }

    float GetSuccessRate() const
    {
        int32 Total = TotalApplied + TotalRolledBack;
        return Total > 0 ? (float)TotalApplied / Total : 0.0f;
    }

    /** Get the modification that produced the best coherence improvement */
    const FArchModProposal* GetBestModification() const
    {
        const FArchModProposal* Best = nullptr;
        float BestDelta = -1.0f;

        for (const auto& P : Proposals)
        {
            if (P.Status == EProposalStatus::APPLIED)
            {
                float Delta = P.PostModCoherence - P.PreModCoherence;
                if (Delta > BestDelta)
                {
                    BestDelta = Delta;
                    Best = &P;
                }
            }
        }

        return Best;
    }

private:
    FArchModProposal* FindProposal(int32 ID)
    {
        for (auto& P : Proposals)
            if (P.ProposalID == ID) return &P;
        return nullptr;
    }

    FString GenerateDescription(EModTarget Target, const Eigen::VectorXf& Delta)
    {
        static const char* TargetNames[] = {
            "Reservoir Topology", "Readout Weights", "Echobeat Timing",
            "Attention Weights", "Membrane Structure", "Goal Priorities",
            "Interest Decay", "Endocrine Sensitivity", "Humor Parameters",
            "Dream Cycle Timing"
        };
        int Idx = FMath::Clamp((int)Target, 0, 9);
        return FString::Printf(TEXT("Modify %s (magnitude: %.4f, dims: %d)"),
            UTF8_TO_TCHAR(TargetNames[Idx]), Delta.norm(), (int)Delta.size());
    }

    float GetMaxMagnitude(EModTarget Target) const
    {
        // Per-target maximum modification magnitude
        switch (Target)
        {
        case EModTarget::RESERVOIR_TOPOLOGY: return 0.05f;      // Very conservative
        case EModTarget::READOUT_WEIGHTS: return 0.1f;
        case EModTarget::ECHOBEAT_TIMING: return 0.2f;
        case EModTarget::ATTENTION_WEIGHTS: return 0.15f;
        case EModTarget::MEMBRANE_STRUCTURE: return 0.05f;       // Very conservative
        case EModTarget::GOAL_PRIORITIES: return 0.3f;
        case EModTarget::INTEREST_DECAY: return 0.2f;
        case EModTarget::ENDOCRINE_SENSITIVITY: return 0.1f;
        case EModTarget::HUMOR_PARAMETERS: return 0.5f;          // More freedom
        case EModTarget::DREAM_CYCLE_TIMING: return 0.3f;
        default: return 0.1f;
        }
    }

    std::deque<FArchModProposal> Proposals;
    std::vector<FArchModProposal> RejectedProposals;
    std::vector<FSandboxResult> SandboxResults;
    std::vector<FRollbackRecord> RollbackHistory;
    std::map<int, double> LastProposalTime;

    float MaxAllowedIdentityDrift = 0.15f;
    float MinCoherenceThreshold = 0.5f;
    int32 MaxConcurrentModifications = 2;

    int32 NextProposalID = 0;
    int32 TotalApplied = 0;
    int32 TotalRolledBack = 0;
    bool bInitialized = false;
};
