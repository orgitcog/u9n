#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// AARRelationNode — Agent-Arena-Relation Self-Loop for Unreal Engine
// Ported from echoself/NanEcho/dte_nodes/aar_relation.py
// Maps to: Relation (Self) in AAR architecture
// The continuous interplay between Agent and Arena via recurrent feedback
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "EchoReservoirNode.h"
#include "CognitiveReadoutNode.h"
#include <Eigen/Dense>

/**
 * Configuration for the AAR Relation feedback loop.
 */
struct FAARRelationConfig
{
    /** Dimensionality of the readout output fed back to reservoir */
    int32 FeedbackDim = 30;

    /** Dimensionality of the reservoir input */
    int32 ReservoirInputDim = 32;

    /** Feedback scaling factor (controls how much output influences next state) */
    float FeedbackScaling = 0.5f;

    /** Coherence threshold — below this, the self-loop is considered unstable */
    float CoherenceThreshold = 0.15f;

    /** Maximum feedback delta per step (safety clamp) */
    float MaxDeltaClamp = 0.2f;

    /** Enable dead man's switch (halt if coherence drops too low) */
    bool bDeadManSwitch = true;
};

/**
 * Coherence metrics for the AAR self-loop.
 */
struct FAARCoherenceMetrics
{
    /** Cosine similarity between consecutive reservoir states */
    float StateContinuity = 1.0f;

    /** Norm of the feedback signal */
    float FeedbackMagnitude = 0.0f;

    /** Running average of state continuity */
    float SmoothedCoherence = 1.0f;

    /** Number of steps executed */
    int64 StepCount = 0;

    /** Whether the self-loop is in a coherent state */
    bool bIsCoherent = true;
};

/**
 * AARRelationNode — The Self in the AAR cognitive architecture.
 *
 * Implements the feedback loop between Agent (Readout) and Arena (Reservoir).
 * The readout output is projected back into the reservoir input space,
 * creating a recurrent self-loop that embodies the "self" — the continuous
 * interplay between acting and being.
 *
 * DTE Philosophy: "memory of the closed past brought into the pivotal present
 * and projected into the open future." The AAR relation IS this projection.
 *
 * Safety features:
 * - Dead man's switch: halts if coherence < threshold
 * - Delta clamping: limits feedback magnitude per step
 * - Coherence monitoring: tracks state continuity
 */
class FAARRelationNode
{
public:
    FAARRelationNode() = default;

    /** Initialize the AAR relation with feedback projection matrix */
    void Initialize(const FAARRelationConfig& Config)
    {
        Cfg = Config;

        // Feedback projection: OutputDim -> ReservoirInputDim
        W_fb = Eigen::MatrixXf::Random(Cfg.ReservoirInputDim, Cfg.FeedbackDim) * 0.1f;

        PreviousState = Eigen::VectorXf::Zero(1); // Will be resized on first step
        Metrics = FAARCoherenceMetrics();
        bInitialized = true;
    }

    /**
     * Execute one AAR cycle: Reservoir -> Readout -> Feedback -> Reservoir
     *
     * @param ExternalInput - External sensory input
     * @param Reservoir - Reference to the reservoir (Arena)
     * @param Readout - Reference to the readout (Agent)
     * @return Readout output (action preferences + emotions + style)
     */
    Eigen::VectorXf Step(
        const Eigen::VectorXf& ExternalInput,
        FEchoReservoirNode& Reservoir,
        const FCognitiveReadoutNode& Readout)
    {
        check(bInitialized);

        // Combine external input with feedback
        Eigen::VectorXf CombinedInput = ExternalInput;
        if (LastOutput.size() == Cfg.FeedbackDim)
        {
            Eigen::VectorXf Feedback = W_fb * LastOutput * Cfg.FeedbackScaling;

            // Safety: clamp feedback delta
            float FbNorm = Feedback.norm();
            if (FbNorm > Cfg.MaxDeltaClamp * CombinedInput.norm())
            {
                Feedback *= (Cfg.MaxDeltaClamp * CombinedInput.norm()) / FbNorm;
            }

            // Add feedback to input (element-wise, truncated to input dim)
            int MixDim = FMath::Min(CombinedInput.size(), Feedback.size());
            for (int i = 0; i < MixDim; ++i)
                CombinedInput(i) += Feedback(i);
        }

        // Step reservoir (Arena)
        Eigen::VectorXf NewState = Reservoir.Step(CombinedInput);

        // Compute readout (Agent)
        Eigen::VectorXf Output = Readout.Forward(NewState);

        // Update coherence metrics
        UpdateCoherence(NewState);

        // Dead man's switch
        if (Cfg.bDeadManSwitch && !Metrics.bIsCoherent)
        {
            UE_LOG(LogTemp, Warning, TEXT("AAR: Coherence below threshold (%.3f < %.3f). Halting feedback."),
                Metrics.SmoothedCoherence, Cfg.CoherenceThreshold);
            // Return output but don't update feedback
            return Output;
        }

        // Store output for next feedback cycle
        LastOutput = Output;
        Metrics.StepCount++;

        return Output;
    }

    /** Get current coherence metrics */
    const FAARCoherenceMetrics& GetCoherence() const { return Metrics; }

    /** Reset the AAR relation */
    void Reset()
    {
        LastOutput = Eigen::VectorXf();
        PreviousState = Eigen::VectorXf::Zero(1);
        Metrics = FAARCoherenceMetrics();
    }

    /** Check if the self-loop is coherent */
    bool IsCoherent() const { return Metrics.bIsCoherent; }

    bool IsInitialized() const { return bInitialized; }

private:
    void UpdateCoherence(const Eigen::VectorXf& NewState)
    {
        if (PreviousState.size() == NewState.size() && NewState.norm() > 1e-10f)
        {
            // Cosine similarity between consecutive states
            float Dot = PreviousState.dot(NewState);
            float Norms = PreviousState.norm() * NewState.norm();
            Metrics.StateContinuity = (Norms > 1e-10f) ? (Dot / Norms) : 0.0f;
        }

        // Exponential moving average of coherence
        float Alpha = 0.05f;
        Metrics.SmoothedCoherence = (1.0f - Alpha) * Metrics.SmoothedCoherence
                                    + Alpha * Metrics.StateContinuity;

        Metrics.FeedbackMagnitude = LastOutput.size() > 0 ? LastOutput.norm() : 0.0f;
        Metrics.bIsCoherent = Metrics.SmoothedCoherence >= Cfg.CoherenceThreshold;

        PreviousState = NewState;
    }

    FAARRelationConfig Cfg;
    Eigen::MatrixXf W_fb;           // Feedback projection matrix
    Eigen::VectorXf LastOutput;     // Previous readout output
    Eigen::VectorXf PreviousState;  // Previous reservoir state
    FAARCoherenceMetrics Metrics;
    bool bInitialized = false;
};
