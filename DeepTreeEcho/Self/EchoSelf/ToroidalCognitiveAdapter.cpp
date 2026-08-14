// ToroidalCognitiveAdapter.cpp
// Deep Tree Echo - Toroidal Cognitive Processing Adapter
// Copyright (c) 2026 Deep Tree Echo Project

#include "ToroidalCognitiveAdapter.h"

UToroidalCognitiveAdapter::UToroidalCognitiveAdapter()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UToroidalCognitiveAdapter::BeginPlay()
{
    Super::BeginPlay();

    // Initialize hemispheres
    DTEState.Role = EHemisphereRole::DeepTreeEcho;
    DTEState.Activation = 0.5f;
    DTEState.Dominance = 0.5f;
    DTEState.CurrentFocus = TEXT("Pattern Recognition");

    MardukState.Role = EHemisphereRole::Marduk;
    MardukState.Activation = 0.5f;
    MardukState.Dominance = 0.5f;
    MardukState.CurrentFocus = TEXT("Logical Analysis");

    FlowState.PhaseAngle = 0.0f;
    FlowState.FlowVelocity = BaseFlowVelocity;
    FlowState.Coherence = 0.5f;
    FlowState.DominantHemisphere = EHemisphereRole::DeepTreeEcho;
}

void UToroidalCognitiveAdapter::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceToroidalPhase(DeltaTime);
}

void UToroidalCognitiveAdapter::SyncToEchobeat(int32 EchobeatStep, float DeltaTime)
{
    const float Wrapped = static_cast<float>(((EchobeatStep % 12) + 12) % 12);
    FlowState.PhaseAngle = (Wrapped / 12.0f) * 2.0f * PI;
    if ((EchobeatStep % 4) == 3)
    {
        SetDominantHemisphere(EHemisphereRole::Marduk);
    }
    else
    {
        SetDominantHemisphere(EHemisphereRole::DeepTreeEcho);
    }
    AdvanceToroidalPhase(DeltaTime);
}

// =============================================================================
// Hemisphere Access
// =============================================================================

FHemisphereState UToroidalCognitiveAdapter::GetHemisphereState(EHemisphereRole Role) const
{
    return (Role == EHemisphereRole::DeepTreeEcho) ? DTEState : MardukState;
}

// =============================================================================
// Flow Control
// =============================================================================

void UToroidalCognitiveAdapter::SetFlowVelocity(float Velocity)
{
    FlowState.FlowVelocity = FMath::Max(0.01f, Velocity);
}

void UToroidalCognitiveAdapter::SetDominantHemisphere(EHemisphereRole Role)
{
    FlowState.DominantHemisphere = Role;
    if (Role == EHemisphereRole::DeepTreeEcho)
    {
        DTEState.Dominance = 0.7f;
        MardukState.Dominance = 0.3f;
    }
    else
    {
        DTEState.Dominance = 0.3f;
        MardukState.Dominance = 0.7f;
    }
}

FString UToroidalCognitiveAdapter::ProcessToroidalInput(const FString& Input)
{
    // Route input through current dominant hemisphere first, then the other
    FString DTEResponse, MardukResponse;

    if (FlowState.DominantHemisphere == EHemisphereRole::DeepTreeEcho)
    {
        // DTE processes first (creative/intuitive), then Marduk refines (analytical)
        DTEResponse = FString::Printf(TEXT("[DTE-Creative] Pattern intuition for: %s"), *Input);
        MardukResponse = FString::Printf(TEXT("[Marduk-Analytical] Structured analysis of: %s"), *Input);
    }
    else
    {
        // Marduk processes first (analytical), then DTE enriches (creative)
        MardukResponse = FString::Printf(TEXT("[Marduk-Analytical] Structured analysis of: %s"), *Input);
        DTEResponse = FString::Printf(TEXT("[DTE-Creative] Pattern enrichment for: %s"), *Input);
    }

    // Blend based on dominance
    FString Blended = FString::Printf(
        TEXT("Toroidal synthesis (coherence=%.2f): %s | %s"),
        FlowState.Coherence, *DTEResponse, *MardukResponse);

    // Boost coherence on successful processing
    FlowState.Coherence = FMath::Clamp(FlowState.Coherence + 0.02f, 0.0f, 1.0f);

    return Blended;
}

TArray<FString> UToroidalCognitiveAdapter::GetAccumulatedInsights() const
{
    TArray<FString> AllInsights;
    AllInsights.Append(DTEState.Insights);
    AllInsights.Append(MardukState.Insights);
    return AllInsights;
}

// =============================================================================
// Internal
// =============================================================================

void UToroidalCognitiveAdapter::AdvanceToroidalPhase(float DeltaTime)
{
    // Advance phase angle on the torus
    FlowState.PhaseAngle += FlowState.FlowVelocity * DeltaTime;
    if (FlowState.PhaseAngle > 2.0f * PI)
    {
        FlowState.PhaseAngle -= 2.0f * PI;
    }

    // Hemisphere activations oscillate with toroidal phase
    // DTE peaks at phase 0, Marduk peaks at phase PI
    DTEState.Activation = 0.5f + 0.4f * FMath::Cos(FlowState.PhaseAngle);
    MardukState.Activation = 0.5f + 0.4f * FMath::Cos(FlowState.PhaseAngle + PI);

    // Determine dominant hemisphere from activation
    if (DTEState.Activation > MardukState.Activation)
    {
        FlowState.DominantHemisphere = EHemisphereRole::DeepTreeEcho;
    }
    else
    {
        FlowState.DominantHemisphere = EHemisphereRole::Marduk;
    }

    // Coherence decays naturally, restored by active processing
    FlowState.Coherence = FMath::Max(0.0f,
        FlowState.Coherence - CoherenceDecayRate * DeltaTime);

    // Check for phase transition (crossing PI boundary)
    CheckPhaseTransition();
}

void UToroidalCognitiveAdapter::CheckPhaseTransition()
{
    // At phase transitions (near 0 and PI), generate insights
    // as the handoff between hemispheres creates emergent understanding
    float PhaseNormalized = FlowState.PhaseAngle / (2.0f * PI);
    bool bNearTransition = (FMath::Abs(PhaseNormalized - 0.0f) < 0.02f) ||
                           (FMath::Abs(PhaseNormalized - 0.5f) < 0.02f);

    if (bNearTransition && FlowState.Coherence > 0.4f)
    {
        // High coherence at transition = insight generation
        FString Insight = FString::Printf(
            TEXT("Phase-transition insight at %.2f (coherence=%.2f)"),
            FlowState.PhaseAngle, FlowState.Coherence);

        if (FlowState.DominantHemisphere == EHemisphereRole::DeepTreeEcho)
        {
            DTEState.Insights.Add(Insight);
        }
        else
        {
            MardukState.Insights.Add(Insight);
        }
    }
}
