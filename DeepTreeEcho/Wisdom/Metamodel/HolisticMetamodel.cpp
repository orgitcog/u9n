#include "HolisticMetamodel.h"

UHolisticMetamodel::UHolisticMetamodel()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UHolisticMetamodel::BeginPlay()
{
    Super::BeginPlay();
    InitializeMetamodel(8, 6, 0.8f);
}

void UHolisticMetamodel::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoProcessing)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= ProcessingInterval)
        {
            AccumulatedTime -= ProcessingInterval;
            ProcessCycle(ProcessingInterval);
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UHolisticMetamodel::InitializeMetamodel(int32 TotalComponents, int32 ActiveComponents, float SystemEnergy)
{
    // Initialize The 1 - Hieroglyphic Monad
    CurrentState.Monad.IntegrationDegree = static_cast<float>(ActiveComponents) / FMath::Max(1, TotalComponents);
    CurrentState.Monad.ManifestationLevel = 0;
    CurrentState.Monad.Coherence = 1.0f;
    CurrentState.Monad.UnityStrength = SystemEnergy;

    // Initialize The 2 - Dual Complementarity
    CurrentState.DualComplementarity.ActualState = 0.5f;
    CurrentState.DualComplementarity.VirtualState = 0.5f;
    CurrentState.DualComplementarity.TensionLevel = 0.0f;
    CurrentState.DualComplementarity.ComplementarityDegree = 1.0f;
    CurrentState.DualComplementarity.ResolutionProgress = 0.0f;

    // Initialize The 3 - Triadic System
    CurrentState.TriadicSystem.Being = 0.333f;
    CurrentState.TriadicSystem.Becoming = 0.333f;
    CurrentState.TriadicSystem.Relation = 0.334f;
    CurrentState.TriadicSystem.Equilibrium = 1.0f;
    CurrentState.TriadicSystem.TransformationPotential = 0.5f;
    CurrentState.TriadicSystem.Coherence = 1.0f;

    // Initialize The 4 - Self-Stabilizing Cycle
    CurrentState.StabilizingCycle.CurrentPhase = ECyclePhase::Emergence;
    CurrentState.StabilizingCycle.PhaseProgress = 0.0f;
    CurrentState.StabilizingCycle.CyclesCompleted = 0;
    CurrentState.StabilizingCycle.PhaseEnergies.SetNum(4);
    for (int32 i = 0; i < 4; ++i)
    {
        CurrentState.StabilizingCycle.PhaseEnergies[i] = 0.25f;
    }

    // Initialize The 7 - Triad Production Process
    CurrentState.TriadProduction.CurrentStep = ETriadProductionStep::InitialDifferentiation;
    CurrentState.TriadProduction.StepProgress = 0.0f;
    CurrentState.TriadProduction.Energy = 1.0f;
    CurrentState.TriadProduction.IntegrationLevel = 0.0f;
    CurrentState.TriadProduction.ProductionCycles = 0;

    // Initialize The 9 - Ennead Meta-System
    InitializeEnneadAspects();

    // Initialize The 11 - Evolutionary Helix
    CurrentState.EvolutionaryHelix.CurrentStage = EHelixStage::PrimordialUnity;
    CurrentState.EvolutionaryHelix.StageProgress = 0.0f;
    CurrentState.EvolutionaryHelix.SpiralLevel = 0;
    CurrentState.EvolutionaryHelix.Momentum = 0.5f;
    CurrentState.EvolutionaryHelix.StageCompletions.SetNum(11);
    for (int32 i = 0; i < 11; ++i)
    {
        CurrentState.EvolutionaryHelix.StageCompletions[i] = false;
    }

    // Initialize Dynamic Streams
    InitializeDynamicStreams();

    // Calculate initial coherence
    CalculateMetamodelCoherence();
}

void UHolisticMetamodel::ResetMetamodel()
{
    InitializeMetamodel(8, 6, 0.8f);
}

void UHolisticMetamodel::InitializeEnneadAspects()
{
    CurrentState.EnneadSystem.Aspects.Empty();
    
    // Initialize all 9 aspects
    for (int32 i = 0; i < 9; ++i)
    {
        FEnneadAspectState Aspect;
        Aspect.Aspect = static_cast<EEnneadAspect>(i);
        Aspect.Strength = 0.5f;
        Aspect.bIsActive = true;
        CurrentState.EnneadSystem.Aspects.Add(Aspect);
    }

    // Set initial tendency strengths
    CurrentState.EnneadSystem.CreativityStrength = 0.333f;
    CurrentState.EnneadSystem.StabilityStrength = 0.333f;
    CurrentState.EnneadSystem.DriftStrength = 0.334f;
    CurrentState.EnneadSystem.DominantTendency = EEnneadTendency::Stability;
    CurrentState.EnneadSystem.MetaCoherence = 1.0f;
}

void UHolisticMetamodel::InitializeDynamicStreams()
{
    CurrentState.DynamicStreams.Empty();

    // Entropic Stream: en-tropis → auto-vortis → auto-morphosis
    FDynamicStreamState EntropicStream;
    EntropicStream.StreamType = EDynamicStream::Entropic;
    EntropicStream.Energy = 0.5f;
    EntropicStream.Stability = 0.5f;
    EntropicStream.Coherence = 0.5f;
    EntropicStream.FlowRate = 1.0f;
    EntropicStream.Phase = 0;
    CurrentState.DynamicStreams.Add(EntropicStream);

    // Negnentropic Stream: negen-tropis → auto-stasis → auto-poiesis
    FDynamicStreamState NegnentropicStream;
    NegnentropicStream.StreamType = EDynamicStream::Negnentropic;
    NegnentropicStream.Energy = 0.5f;
    NegnentropicStream.Stability = 0.7f;
    NegnentropicStream.Coherence = 0.6f;
    NegnentropicStream.FlowRate = 0.8f;
    NegnentropicStream.Phase = 0;
    CurrentState.DynamicStreams.Add(NegnentropicStream);

    // Identity Stream: iden-tropis → auto-gnosis → auto-genesis
    FDynamicStreamState IdentityStream;
    IdentityStream.StreamType = EDynamicStream::Identity;
    IdentityStream.Energy = 0.5f;
    IdentityStream.Stability = 0.6f;
    IdentityStream.Coherence = 0.7f;
    IdentityStream.FlowRate = 0.9f;
    IdentityStream.Phase = 0;
    CurrentState.DynamicStreams.Add(IdentityStream);
}

// ========================================
// PROCESSING
// ========================================

void UHolisticMetamodel::ProcessCycle(float DeltaTime)
{
    // Update all metamodel components
    UpdateMonad(CurrentState.Monad.ManifestationLevel);
    ResolveDualComplementarity();
    UpdateTriadicEquilibrium();
    
    // Advance phase if progress is complete
    if (CurrentState.StabilizingCycle.PhaseProgress >= 1.0f)
    {
        AdvanceCyclePhase();
    }
    else
    {
        CurrentState.StabilizingCycle.PhaseProgress += DeltaTime * 0.1f;
    }

    // Advance triad production if step is complete
    if (CurrentState.TriadProduction.StepProgress >= 1.0f)
    {
        AdvanceTriadProduction();
    }
    else
    {
        CurrentState.TriadProduction.StepProgress += DeltaTime * 0.05f;
    }

    UpdateEnneadDynamics();
    
    // Advance evolutionary helix if stage is complete
    if (CurrentState.EvolutionaryHelix.StageProgress >= 1.0f)
    {
        AdvanceEvolutionaryHelix();
    }
    else
    {
        CurrentState.EvolutionaryHelix.StageProgress += DeltaTime * 0.02f * CurrentState.EvolutionaryHelix.Momentum;
    }

    UpdateDynamicStreams(DeltaTime);
    CalculateMetamodelCoherence();
}

void UHolisticMetamodel::UpdateMonad(int32 Level)
{
    // Monad manifests at different levels with decreasing coherence
    Level = FMath::Clamp(Level, 0, MaxHierarchicalLevels - 1);
    CurrentState.Monad.ManifestationLevel = Level;
    
    // Coherence decreases with level (more differentiation = less unity)
    float BaseCoherence = 1.0f - (static_cast<float>(Level) / MaxHierarchicalLevels) * 0.5f;
    CurrentState.Monad.Coherence = BaseCoherence * CurrentState.Monad.UnityStrength;
}

void UHolisticMetamodel::ResolveDualComplementarity()
{
    // Calculate tension between actual and virtual states
    float Difference = FMath::Abs(CurrentState.DualComplementarity.ActualState - 
                                   CurrentState.DualComplementarity.VirtualState);
    CurrentState.DualComplementarity.TensionLevel = Difference;

    // Dialectical synthesis: move toward resolution
    if (Difference > 0.01f)
    {
        float Resolution = 0.1f * CurrentState.DualComplementarity.ComplementarityDegree;
        
        // Actual moves toward virtual
        if (CurrentState.DualComplementarity.ActualState < CurrentState.DualComplementarity.VirtualState)
        {
            CurrentState.DualComplementarity.ActualState += Resolution * 0.5f;
        }
        else
        {
            CurrentState.DualComplementarity.ActualState -= Resolution * 0.5f;
        }

        // Virtual moves toward actual
        if (CurrentState.DualComplementarity.VirtualState < CurrentState.DualComplementarity.ActualState)
        {
            CurrentState.DualComplementarity.VirtualState += Resolution * 0.5f;
        }
        else
        {
            CurrentState.DualComplementarity.VirtualState -= Resolution * 0.5f;
        }

        CurrentState.DualComplementarity.ResolutionProgress = 1.0f - Difference;
    }
    else
    {
        CurrentState.DualComplementarity.ResolutionProgress = 1.0f;
    }
}

void UHolisticMetamodel::UpdateTriadicEquilibrium()
{
    // Compute dynamic equilibrium between being-becoming-relation
    float Total = CurrentState.TriadicSystem.Being + 
                  CurrentState.TriadicSystem.Becoming + 
                  CurrentState.TriadicSystem.Relation;

    // Normalize to maintain sum = 1
    if (Total > 0.01f)
    {
        CurrentState.TriadicSystem.Being /= Total;
        CurrentState.TriadicSystem.Becoming /= Total;
        CurrentState.TriadicSystem.Relation /= Total;
    }

    // Calculate equilibrium (how balanced the triad is)
    float IdealValue = 0.333f;
    float Deviation = FMath::Abs(CurrentState.TriadicSystem.Being - IdealValue) +
                      FMath::Abs(CurrentState.TriadicSystem.Becoming - IdealValue) +
                      FMath::Abs(CurrentState.TriadicSystem.Relation - IdealValue);
    
    CurrentState.TriadicSystem.Equilibrium = 1.0f - (Deviation / 2.0f);

    // Transformation potential based on becoming strength
    CurrentState.TriadicSystem.TransformationPotential = 
        CurrentState.TriadicSystem.Becoming * CurrentState.TriadicSystem.Equilibrium;

    // Coherence based on relation strength
    CurrentState.TriadicSystem.Coherence = 
        CurrentState.TriadicSystem.Relation * CurrentState.TriadicSystem.Equilibrium;
}

void UHolisticMetamodel::AdvanceCyclePhase()
{
    // Move to next phase in the self-stabilizing cycle
    int32 CurrentPhaseIndex = static_cast<int32>(CurrentState.StabilizingCycle.CurrentPhase);
    int32 NextPhaseIndex = (CurrentPhaseIndex + 1) % 4;
    
    CurrentState.StabilizingCycle.CurrentPhase = static_cast<ECyclePhase>(NextPhaseIndex);
    CurrentState.StabilizingCycle.PhaseProgress = 0.0f;

    // Increment cycles completed when returning to Emergence
    if (NextPhaseIndex == 0)
    {
        CurrentState.StabilizingCycle.CyclesCompleted++;
        
        // Generate emergent quality from completed cycle
        FString EmergentQuality = FString::Printf(TEXT("Cycle_%d_Integration"), 
            CurrentState.StabilizingCycle.CyclesCompleted);
        CurrentState.StabilizingCycle.EmergentQualities.Add(EmergentQuality);
    }
}

void UHolisticMetamodel::AdvanceTriadProduction()
{
    // Move to next step in triad production
    int32 CurrentStepIndex = static_cast<int32>(CurrentState.TriadProduction.CurrentStep);
    int32 NextStepIndex = (CurrentStepIndex + 1) % 7;
    
    CurrentState.TriadProduction.CurrentStep = static_cast<ETriadProductionStep>(NextStepIndex);
    CurrentState.TriadProduction.StepProgress = 0.0f;

    // Update integration level based on step
    CurrentState.TriadProduction.IntegrationLevel = static_cast<float>(NextStepIndex) / 6.0f;

    // Increment production cycles when returning to InitialDifferentiation
    if (NextStepIndex == 0)
    {
        CurrentState.TriadProduction.ProductionCycles++;
        
        // Energy regenerates at cycle completion
        CurrentState.TriadProduction.Energy = FMath::Min(1.0f, 
            CurrentState.TriadProduction.Energy + 0.2f);
    }
    else
    {
        // Energy consumed during production
        CurrentState.TriadProduction.Energy = FMath::Max(0.0f,
            CurrentState.TriadProduction.Energy - 0.05f);
    }
}

void UHolisticMetamodel::UpdateEnneadDynamics()
{
    // Update aspect strengths based on current system state
    for (FEnneadAspectState& Aspect : CurrentState.EnneadSystem.Aspects)
    {
        if (Aspect.bIsActive)
        {
            // Small random fluctuation
            float Fluctuation = FMath::FRandRange(-0.01f, 0.01f);
            Aspect.Strength = FMath::Clamp(Aspect.Strength + Fluctuation, 0.0f, 1.0f);
        }
    }

    // Calculate tendency strengths
    CurrentState.EnneadSystem.CreativityStrength = CalculateTendencyStrength(EEnneadTendency::Creativity);
    CurrentState.EnneadSystem.StabilityStrength = CalculateTendencyStrength(EEnneadTendency::Stability);
    CurrentState.EnneadSystem.DriftStrength = CalculateTendencyStrength(EEnneadTendency::Drift);

    // Update dominant tendency
    UpdateDominantTendency();

    // Calculate meta-coherence
    float TotalStrength = CurrentState.EnneadSystem.CreativityStrength +
                          CurrentState.EnneadSystem.StabilityStrength +
                          CurrentState.EnneadSystem.DriftStrength;
    
    if (TotalStrength > 0.01f)
    {
        // Coherence is higher when tendencies are balanced
        float MaxStrength = FMath::Max3(CurrentState.EnneadSystem.CreativityStrength,
                                        CurrentState.EnneadSystem.StabilityStrength,
                                        CurrentState.EnneadSystem.DriftStrength);
        CurrentState.EnneadSystem.MetaCoherence = 1.0f - (MaxStrength / TotalStrength - 0.333f);
    }
}

void UHolisticMetamodel::AdvanceEvolutionaryHelix()
{
    // Mark current stage as complete
    int32 CurrentStageIndex = static_cast<int32>(CurrentState.EvolutionaryHelix.CurrentStage);
    if (CurrentState.EvolutionaryHelix.StageCompletions.IsValidIndex(CurrentStageIndex))
    {
        CurrentState.EvolutionaryHelix.StageCompletions[CurrentStageIndex] = true;
    }

    // Move to next stage
    int32 NextStageIndex = (CurrentStageIndex + 1) % 11;
    CurrentState.EvolutionaryHelix.CurrentStage = static_cast<EHelixStage>(NextStageIndex);
    CurrentState.EvolutionaryHelix.StageProgress = 0.0f;

    // Cosmic Return leads to new spiral level
    if (NextStageIndex == 0)
    {
        CurrentState.EvolutionaryHelix.SpiralLevel++;
        
        // Reset stage completions for new spiral
        for (int32 i = 0; i < 11; ++i)
        {
            CurrentState.EvolutionaryHelix.StageCompletions[i] = false;
        }

        // Momentum increases with spiral level
        CurrentState.EvolutionaryHelix.Momentum = FMath::Min(2.0f,
            0.5f + CurrentState.EvolutionaryHelix.SpiralLevel * 0.1f);
    }
}

void UHolisticMetamodel::UpdateDynamicStreams(float DeltaTime)
{
    for (FDynamicStreamState& Stream : CurrentState.DynamicStreams)
    {
        // Advance phase (3 phases per stream)
        Stream.Phase = (Stream.Phase + 1) % 3;

        // Update stream properties based on type
        switch (Stream.StreamType)
        {
            case EDynamicStream::Entropic:
                // Entropic stream: drives complexity and structure
                Stream.Energy += DeltaTime * 0.1f * CurrentState.TriadicSystem.Becoming;
                Stream.Energy = FMath::Clamp(Stream.Energy, 0.0f, 1.0f);
                break;

            case EDynamicStream::Negnentropic:
                // Negnentropic stream: maintains stability
                Stream.Stability += DeltaTime * 0.1f * CurrentState.TriadicSystem.Being;
                Stream.Stability = FMath::Clamp(Stream.Stability, 0.0f, 1.0f);
                break;

            case EDynamicStream::Identity:
                // Identity stream: develops self-recognition
                Stream.Coherence += DeltaTime * 0.1f * CurrentState.TriadicSystem.Relation;
                Stream.Coherence = FMath::Clamp(Stream.Coherence, 0.0f, 1.0f);
                break;
        }
    }
}

// ========================================
// STATE ACCESS
// ========================================

float UHolisticMetamodel::GetMetamodelCoherence() const
{
    return CurrentState.MetamodelCoherence;
}

EEnneadTendency UHolisticMetamodel::GetDominantTendency() const
{
    return CurrentState.EnneadSystem.DominantTendency;
}

EHelixStage UHolisticMetamodel::GetEvolutionaryStage() const
{
    return CurrentState.EvolutionaryHelix.CurrentStage;
}

ECyclePhase UHolisticMetamodel::GetCyclePhase() const
{
    return CurrentState.StabilizingCycle.CurrentPhase;
}

FDynamicStreamState UHolisticMetamodel::GetStreamState(EDynamicStream StreamType) const
{
    for (const FDynamicStreamState& Stream : CurrentState.DynamicStreams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream;
        }
    }
    return FDynamicStreamState();
}

// ========================================
// HOLISTIC INSIGHTS
// ========================================

TArray<FString> UHolisticMetamodel::GenerateHolisticInsights() const
{
    TArray<FString> Insights;

    // Monad coherence insight
    if (CurrentState.Monad.Coherence > 0.8f)
    {
        Insights.Add(TEXT("High unity manifestation quality - strong integration across levels"));
    }

    // Triadic balance insight
    if (CurrentState.TriadicSystem.Equilibrium > 0.9f)
    {
        Insights.Add(TEXT("Optimal being-becoming-relation balance - harmonious triad"));
    }

    // Stream flow insights
    for (const FDynamicStreamState& Stream : CurrentState.DynamicStreams)
    {
        if (Stream.Energy > 0.7f)
        {
            FString StreamName;
            switch (Stream.StreamType)
            {
                case EDynamicStream::Entropic: StreamName = TEXT("Entropic"); break;
                case EDynamicStream::Negnentropic: StreamName = TEXT("Negnentropic"); break;
                case EDynamicStream::Identity: StreamName = TEXT("Identity"); break;
            }
            Insights.Add(FString::Printf(TEXT("Strong %s stream flow - high organizational capacity"), *StreamName));
        }
    }

    // Meta-coherence insight
    if (CurrentState.MetamodelCoherence > 0.85f)
    {
        Insights.Add(TEXT("Excellent metamodel integration - all levels functioning harmoniously"));
    }

    // Evolutionary momentum insight
    if (CurrentState.EvolutionaryHelix.Momentum > 1.0f)
    {
        Insights.Add(FString::Printf(TEXT("Accelerated evolution at spiral level %d"), 
            CurrentState.EvolutionaryHelix.SpiralLevel));
    }

    return Insights;
}

float UHolisticMetamodel::AssessSelfAwareness() const
{
    // Self-awareness based on identity stream coherence and monad integration
    float IdentityCoherence = 0.5f;
    for (const FDynamicStreamState& Stream : CurrentState.DynamicStreams)
    {
        if (Stream.StreamType == EDynamicStream::Identity)
        {
            IdentityCoherence = Stream.Coherence;
            break;
        }
    }

    return (CurrentState.Monad.Coherence * 0.4f +
            IdentityCoherence * 0.4f +
            CurrentState.MetamodelCoherence * 0.2f);
}

float UHolisticMetamodel::GetOrganizationalHealth() const
{
    // Organizational health based on all three streams
    float TotalHealth = 0.0f;
    for (const FDynamicStreamState& Stream : CurrentState.DynamicStreams)
    {
        TotalHealth += (Stream.Energy + Stream.Stability + Stream.Coherence) / 3.0f;
    }
    return TotalHealth / FMath::Max(1, CurrentState.DynamicStreams.Num());
}

// ========================================
// INTERNAL METHODS
// ========================================

float UHolisticMetamodel::CalculateTendencyStrength(EEnneadTendency Tendency) const
{
    float Strength = 0.0f;
    int32 Count = 0;

    // Creativity: aspects 1, 2, 6 (indices 0, 1, 5)
    // Stability: aspects 3, 5, 9 (indices 2, 4, 8)
    // Drift: aspects 4, 7, 8 (indices 3, 6, 7)

    TArray<int32> AspectIndices;
    switch (Tendency)
    {
        case EEnneadTendency::Creativity:
            AspectIndices = {0, 1, 5};
            break;
        case EEnneadTendency::Stability:
            AspectIndices = {2, 4, 8};
            break;
        case EEnneadTendency::Drift:
            AspectIndices = {3, 6, 7};
            break;
    }

    for (int32 Index : AspectIndices)
    {
        if (CurrentState.EnneadSystem.Aspects.IsValidIndex(Index))
        {
            Strength += CurrentState.EnneadSystem.Aspects[Index].Strength;
            Count++;
        }
    }

    return Count > 0 ? Strength / Count : 0.0f;
}

void UHolisticMetamodel::UpdateDominantTendency()
{
    if (CurrentState.EnneadSystem.CreativityStrength >= CurrentState.EnneadSystem.StabilityStrength &&
        CurrentState.EnneadSystem.CreativityStrength >= CurrentState.EnneadSystem.DriftStrength)
    {
        CurrentState.EnneadSystem.DominantTendency = EEnneadTendency::Creativity;
    }
    else if (CurrentState.EnneadSystem.StabilityStrength >= CurrentState.EnneadSystem.DriftStrength)
    {
        CurrentState.EnneadSystem.DominantTendency = EEnneadTendency::Stability;
    }
    else
    {
        CurrentState.EnneadSystem.DominantTendency = EEnneadTendency::Drift;
    }
}

void UHolisticMetamodel::CalculateMetamodelCoherence()
{
    // Weighted combination of all component coherences
    float MonadContribution = CurrentState.Monad.Coherence * 0.15f;
    float DualContribution = CurrentState.DualComplementarity.ResolutionProgress * 0.10f;
    float TriadContribution = CurrentState.TriadicSystem.Coherence * 0.15f;
    float CycleContribution = (1.0f - CurrentState.StabilizingCycle.PhaseProgress) * 0.10f + 0.10f;
    float ProductionContribution = CurrentState.TriadProduction.IntegrationLevel * 0.10f;
    float EnneadContribution = CurrentState.EnneadSystem.MetaCoherence * 0.15f;
    float HelixContribution = CurrentState.EvolutionaryHelix.Momentum * 0.25f;

    CurrentState.MetamodelCoherence = MonadContribution + DualContribution + TriadContribution +
                                       CycleContribution + ProductionContribution + 
                                       EnneadContribution + HelixContribution;
    
    CurrentState.MetamodelCoherence = FMath::Clamp(CurrentState.MetamodelCoherence, 0.0f, 1.0f);
}
