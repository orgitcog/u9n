#include "WisdomCultivation.h"

UWisdomCultivation::UWisdomCultivation()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UWisdomCultivation::BeginPlay()
{
    Super::BeginPlay();
    InitializeWisdom();
}

void UWisdomCultivation::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoProcessing)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= ProcessingInterval)
        {
            AccumulatedTime -= ProcessingInterval;
            
            ProcessSalienceLandscape(ProcessingInterval);
            ProcessAffordanceLandscape(ProcessingInterval);
            ProcessOpponentDynamics(ProcessingInterval);
            UpdateRelevanceRealization();
            CalculateOverallWisdom();
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UWisdomCultivation::InitializeWisdom()
{
    // Initialize landscapes
    CurrentState.SalienceLandscape.Items.Empty();
    CurrentState.SalienceLandscape.FocusTarget = TEXT("");
    CurrentState.SalienceLandscape.Coherence = 1.0f;
    CurrentState.SalienceLandscape.Complexity = 0.5f;
    CurrentState.SalienceLandscape.TotalSalience = 0.0f;

    CurrentState.AffordanceLandscape.Affordances.Empty();
    CurrentState.AffordanceLandscape.SelectedAffordance = TEXT("");
    CurrentState.AffordanceLandscape.Richness = 0.5f;
    CurrentState.AffordanceLandscape.ActionReadiness = 0.5f;

    // Initialize subsystems
    InitializeOpponentProcesses();
    InitializeKnowingStates();
    InitializeCognitiveVirtues();
    InitializeWisdomDimensions();

    // Initialize relevance realization
    CurrentState.RelevanceRealization.Capacity = 0.5f;
    CurrentState.RelevanceRealization.Accuracy = 0.5f;
    CurrentState.RelevanceRealization.Efficiency = 0.5f;
    CurrentState.RelevanceRealization.Adaptability = 0.5f;

    CurrentState.OverallWisdom = 0.0f;
}

void UWisdomCultivation::ResetWisdom()
{
    InitializeWisdom();
}

void UWisdomCultivation::InitializeOpponentProcesses()
{
    CurrentState.OpponentProcesses.Empty();

    // Exploration-Exploitation
    FOpponentProcessState ExploreExploit;
    ExploreExploit.ProcessType = EOpponentProcess::ExplorationExploitation;
    ExploreExploit.Balance = 0.0f;
    ExploreExploit.PoleAStrength = 0.5f;
    ExploreExploit.PoleBStrength = 0.5f;
    ExploreExploit.OscillationFrequency = 1.0f;
    ExploreExploit.Phase = 0.0f;
    CurrentState.OpponentProcesses.Add(ExploreExploit);

    // Focus-Diffusion
    FOpponentProcessState FocusDiffuse;
    FocusDiffuse.ProcessType = EOpponentProcess::FocusDiffusion;
    FocusDiffuse.Balance = 0.0f;
    FocusDiffuse.PoleAStrength = 0.5f;
    FocusDiffuse.PoleBStrength = 0.5f;
    FocusDiffuse.OscillationFrequency = 0.8f;
    FocusDiffuse.Phase = 0.0f;
    CurrentState.OpponentProcesses.Add(FocusDiffuse);

    // Assimilation-Accommodation
    FOpponentProcessState AssimAccom;
    AssimAccom.ProcessType = EOpponentProcess::AssimilationAccommodation;
    AssimAccom.Balance = 0.0f;
    AssimAccom.PoleAStrength = 0.5f;
    AssimAccom.PoleBStrength = 0.5f;
    AssimAccom.OscillationFrequency = 0.5f;
    AssimAccom.Phase = 0.0f;
    CurrentState.OpponentProcesses.Add(AssimAccom);

    // Abstraction-Concretization
    FOpponentProcessState AbsConcrete;
    AbsConcrete.ProcessType = EOpponentProcess::AbstractionConcretization;
    AbsConcrete.Balance = 0.0f;
    AbsConcrete.PoleAStrength = 0.5f;
    AbsConcrete.PoleBStrength = 0.5f;
    AbsConcrete.OscillationFrequency = 0.7f;
    AbsConcrete.Phase = 0.0f;
    CurrentState.OpponentProcesses.Add(AbsConcrete);

    // Analysis-Synthesis
    FOpponentProcessState AnalSynth;
    AnalSynth.ProcessType = EOpponentProcess::AnalysisSynthesis;
    AnalSynth.Balance = 0.0f;
    AnalSynth.PoleAStrength = 0.5f;
    AnalSynth.PoleBStrength = 0.5f;
    AnalSynth.OscillationFrequency = 0.6f;
    AnalSynth.Phase = 0.0f;
    CurrentState.OpponentProcesses.Add(AnalSynth);
}

void UWisdomCultivation::InitializeKnowingStates()
{
    CurrentState.KnowingStates.Empty();

    for (int32 i = 0; i < 4; ++i)
    {
        FKnowingState Knowing;
        Knowing.Type = static_cast<EKnowingType>(i);
        Knowing.Strength = 0.5f;
        Knowing.Integration = 0.5f;
        Knowing.Engagement = 0.5f;
        CurrentState.KnowingStates.Add(Knowing);
    }
}

void UWisdomCultivation::InitializeCognitiveVirtues()
{
    CurrentState.CognitiveVirtues.Empty();

    for (int32 i = 0; i < 5; ++i)
    {
        FCognitiveVirtueState Virtue;
        Virtue.Virtue = static_cast<ECognitiveVirtue>(i);
        Virtue.Strength = 0.5f;
        Virtue.Development = 0.0f;
        Virtue.Exercise = 0.0f;
        CurrentState.CognitiveVirtues.Add(Virtue);
    }
}

void UWisdomCultivation::InitializeWisdomDimensions()
{
    CurrentState.WisdomDimensions.Empty();

    for (int32 i = 0; i < 4; ++i)
    {
        FWisdomDimensionState Dimension;
        Dimension.Dimension = static_cast<EWisdomDimension>(i);
        Dimension.Level = 0.0f;
        Dimension.GrowthRate = 0.0f;
        Dimension.Integration = 0.0f;
        CurrentState.WisdomDimensions.Add(Dimension);
    }
}

// ========================================
// SALIENCE
// ========================================

void UWisdomCultivation::AddSalienceItem(const FString& Id, float InitialSalience, float Relevance)
{
    FSalienceItem Item;
    Item.Id = Id;
    Item.Salience = FMath::Clamp(InitialSalience, 0.0f, 1.0f);
    Item.Relevance = FMath::Clamp(Relevance, 0.0f, 1.0f);
    Item.Urgency = 0.0f;
    Item.Novelty = 1.0f;
    Item.TimeSinceAttention = 0.0f;

    CurrentState.SalienceLandscape.Items.Add(Item);
    CurrentState.SalienceLandscape.TotalSalience += Item.Salience;
}

void UWisdomCultivation::UpdateSalience(const FString& Id, float DeltaSalience)
{
    for (FSalienceItem& Item : CurrentState.SalienceLandscape.Items)
    {
        if (Item.Id == Id)
        {
            float OldSalience = Item.Salience;
            Item.Salience = FMath::Clamp(Item.Salience + DeltaSalience, 0.0f, 1.0f);
            CurrentState.SalienceLandscape.TotalSalience += (Item.Salience - OldSalience);
            return;
        }
    }
}

FSalienceItem UWisdomCultivation::GetMostSalientItem() const
{
    FSalienceItem MostSalient;
    float MaxSalience = -1.0f;

    for (const FSalienceItem& Item : CurrentState.SalienceLandscape.Items)
    {
        if (Item.Salience > MaxSalience)
        {
            MaxSalience = Item.Salience;
            MostSalient = Item;
        }
    }

    return MostSalient;
}

void UWisdomCultivation::FocusAttention(const FString& Id)
{
    CurrentState.SalienceLandscape.FocusTarget = Id;

    for (FSalienceItem& Item : CurrentState.SalienceLandscape.Items)
    {
        if (Item.Id == Id)
        {
            Item.TimeSinceAttention = 0.0f;
            Item.Novelty = FMath::Max(0.0f, Item.Novelty - 0.1f);
        }
    }
}

FSalienceLandscape UWisdomCultivation::GetSalienceLandscape() const
{
    return CurrentState.SalienceLandscape;
}

// ========================================
// AFFORDANCES
// ========================================

void UWisdomCultivation::AddAffordance(const FString& Id, const FString& Action, float Availability, float ExpectedUtility)
{
    FAffordance Affordance;
    Affordance.Id = Id;
    Affordance.Action = Action;
    Affordance.Availability = FMath::Clamp(Availability, 0.0f, 1.0f);
    Affordance.ExpectedUtility = FMath::Clamp(ExpectedUtility, 0.0f, 1.0f);
    Affordance.Risk = 0.0f;
    Affordance.SkillRequirement = 0.5f;
    Affordance.ContextualFit = 0.5f;

    CurrentState.AffordanceLandscape.Affordances.Add(Affordance);
}

void UWisdomCultivation::UpdateAffordanceAvailability(const FString& Id, float Availability)
{
    for (FAffordance& Affordance : CurrentState.AffordanceLandscape.Affordances)
    {
        if (Affordance.Id == Id)
        {
            Affordance.Availability = FMath::Clamp(Availability, 0.0f, 1.0f);
            return;
        }
    }
}

FAffordance UWisdomCultivation::GetBestAffordance() const
{
    FAffordance Best;
    float MaxScore = -1.0f;

    for (const FAffordance& Affordance : CurrentState.AffordanceLandscape.Affordances)
    {
        // Score = utility * availability * contextual fit - risk
        float Score = Affordance.ExpectedUtility * Affordance.Availability * 
                      Affordance.ContextualFit - Affordance.Risk * 0.5f;
        
        if (Score > MaxScore)
        {
            MaxScore = Score;
            Best = Affordance;
        }
    }

    return Best;
}

void UWisdomCultivation::SelectAffordance(const FString& Id)
{
    CurrentState.AffordanceLandscape.SelectedAffordance = Id;
}

FAffordanceLandscape UWisdomCultivation::GetAffordanceLandscape() const
{
    return CurrentState.AffordanceLandscape;
}

// ========================================
// OPPONENT PROCESSES
// ========================================

FOpponentProcessState UWisdomCultivation::GetOpponentProcessState(EOpponentProcess ProcessType) const
{
    for (const FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        if (Process.ProcessType == ProcessType)
        {
            return Process;
        }
    }
    return FOpponentProcessState();
}

void UWisdomCultivation::SetOpponentBalance(EOpponentProcess ProcessType, float Balance)
{
    for (FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        if (Process.ProcessType == ProcessType)
        {
            Process.Balance = FMath::Clamp(Balance, -1.0f, 1.0f);
            
            // Update pole strengths based on balance
            Process.PoleAStrength = 0.5f + Process.Balance * 0.5f;
            Process.PoleBStrength = 0.5f - Process.Balance * 0.5f;
            return;
        }
    }
}

float UWisdomCultivation::GetExplorationExploitationBalance() const
{
    for (const FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        if (Process.ProcessType == EOpponentProcess::ExplorationExploitation)
        {
            return Process.Balance;
        }
    }
    return 0.0f;
}

// ========================================
// KNOWING
// ========================================

FKnowingState UWisdomCultivation::GetKnowingState(EKnowingType Type) const
{
    int32 Index = static_cast<int32>(Type);
    if (CurrentState.KnowingStates.IsValidIndex(Index))
    {
        return CurrentState.KnowingStates[Index];
    }
    return FKnowingState();
}

void UWisdomCultivation::UpdateKnowingStrength(EKnowingType Type, float Strength)
{
    int32 Index = static_cast<int32>(Type);
    if (CurrentState.KnowingStates.IsValidIndex(Index))
    {
        CurrentState.KnowingStates[Index].Strength = FMath::Clamp(Strength, 0.0f, 1.0f);
    }
}

void UWisdomCultivation::IntegrateKnowing()
{
    // Calculate average strength
    float TotalStrength = 0.0f;
    for (const FKnowingState& Knowing : CurrentState.KnowingStates)
    {
        TotalStrength += Knowing.Strength;
    }
    float AverageStrength = TotalStrength / FMath::Max(1, CurrentState.KnowingStates.Num());

    // Update integration based on how balanced the knowing types are
    for (FKnowingState& Knowing : CurrentState.KnowingStates)
    {
        float Deviation = FMath::Abs(Knowing.Strength - AverageStrength);
        Knowing.Integration = 1.0f - Deviation;
    }
}

// ========================================
// COGNITIVE VIRTUES
// ========================================

FCognitiveVirtueState UWisdomCultivation::GetCognitiveVirtueState(ECognitiveVirtue Virtue) const
{
    int32 Index = static_cast<int32>(Virtue);
    if (CurrentState.CognitiveVirtues.IsValidIndex(Index))
    {
        return CurrentState.CognitiveVirtues[Index];
    }
    return FCognitiveVirtueState();
}

void UWisdomCultivation::ExerciseCognitiveVirtue(ECognitiveVirtue Virtue, float Intensity)
{
    int32 Index = static_cast<int32>(Virtue);
    if (CurrentState.CognitiveVirtues.IsValidIndex(Index))
    {
        FCognitiveVirtueState& VirtueState = CurrentState.CognitiveVirtues[Index];
        VirtueState.Exercise = FMath::Clamp(Intensity, 0.0f, 1.0f);
        
        // Development grows with exercise
        VirtueState.Development = FMath::Min(1.0f, VirtueState.Development + Intensity * 0.01f);
        
        // Strength is influenced by development and exercise
        VirtueState.Strength = (VirtueState.Development * 0.7f + VirtueState.Exercise * 0.3f);
    }
}

float UWisdomCultivation::GetOverallVirtueDevelopment() const
{
    float Total = 0.0f;
    for (const FCognitiveVirtueState& Virtue : CurrentState.CognitiveVirtues)
    {
        Total += Virtue.Development;
    }
    return Total / FMath::Max(1, CurrentState.CognitiveVirtues.Num());
}

// ========================================
// WISDOM DIMENSIONS
// ========================================

FWisdomDimensionState UWisdomCultivation::GetWisdomDimensionState(EWisdomDimension Dimension) const
{
    int32 Index = static_cast<int32>(Dimension);
    if (CurrentState.WisdomDimensions.IsValidIndex(Index))
    {
        return CurrentState.WisdomDimensions[Index];
    }
    return FWisdomDimensionState();
}

void UWisdomCultivation::CultivateWisdomDimension(EWisdomDimension Dimension, float Growth)
{
    int32 Index = static_cast<int32>(Dimension);
    if (CurrentState.WisdomDimensions.IsValidIndex(Index))
    {
        FWisdomDimensionState& DimensionState = CurrentState.WisdomDimensions[Index];
        DimensionState.GrowthRate = Growth;
        DimensionState.Level = FMath::Min(1.0f, DimensionState.Level + Growth * 0.01f);
    }
}

float UWisdomCultivation::GetOverallWisdomLevel() const
{
    return CurrentState.OverallWisdom;
}

// ========================================
// RELEVANCE REALIZATION
// ========================================

void UWisdomCultivation::RealizeRelevance()
{
    // Relevance realization integrates salience and affordance landscapes
    // with opponent processes and knowing types

    // Calculate salience-affordance alignment
    float SalienceAffordanceAlignment = 0.0f;
    if (!CurrentState.SalienceLandscape.FocusTarget.IsEmpty() &&
        !CurrentState.AffordanceLandscape.SelectedAffordance.IsEmpty())
    {
        // Check if focus and action are aligned
        SalienceAffordanceAlignment = 0.8f; // Simplified
    }

    // Calculate opponent process balance quality
    float OpponentBalance = 0.0f;
    for (const FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        // Better balance = closer to 0
        OpponentBalance += 1.0f - FMath::Abs(Process.Balance);
    }
    OpponentBalance /= FMath::Max(1, CurrentState.OpponentProcesses.Num());

    // Update relevance realization state
    CurrentState.RelevanceRealization.Accuracy = 
        (SalienceAffordanceAlignment * 0.4f + OpponentBalance * 0.3f + 
         GetOverallVirtueDevelopment() * 0.3f);

    CurrentState.RelevanceRealization.Efficiency = 
        CurrentState.SalienceLandscape.Coherence * CurrentState.AffordanceLandscape.ActionReadiness;

    CurrentState.RelevanceRealization.Capacity = 
        (CurrentState.RelevanceRealization.Accuracy + 
         CurrentState.RelevanceRealization.Efficiency) * 0.5f;
}

FRelevanceRealizationState UWisdomCultivation::GetRelevanceRealizationState() const
{
    return CurrentState.RelevanceRealization;
}

float UWisdomCultivation::AssessRelevanceAccuracy() const
{
    return CurrentState.RelevanceRealization.Accuracy;
}

// ========================================
// INTERNAL PROCESSING
// ========================================

void UWisdomCultivation::ProcessSalienceLandscape(float DeltaTime)
{
    // Decay salience over time
    for (FSalienceItem& Item : CurrentState.SalienceLandscape.Items)
    {
        Item.Salience = FMath::Max(0.0f, Item.Salience - SalienceDecayRate * DeltaTime);
        Item.TimeSinceAttention += DeltaTime;
        
        // Novelty decays with time
        Item.Novelty = FMath::Max(0.0f, Item.Novelty - 0.01f * DeltaTime);
    }

    // Recalculate total salience
    CurrentState.SalienceLandscape.TotalSalience = 0.0f;
    for (const FSalienceItem& Item : CurrentState.SalienceLandscape.Items)
    {
        CurrentState.SalienceLandscape.TotalSalience += Item.Salience;
    }

    // Update landscape complexity
    CurrentState.SalienceLandscape.Complexity = 
        FMath::Min(1.0f, CurrentState.SalienceLandscape.Items.Num() / 10.0f);

    // Update landscape coherence (inverse of complexity for now)
    CurrentState.SalienceLandscape.Coherence = 1.0f - CurrentState.SalienceLandscape.Complexity * 0.5f;
}

void UWisdomCultivation::ProcessAffordanceLandscape(float DeltaTime)
{
    // Update affordance landscape richness
    CurrentState.AffordanceLandscape.Richness = 
        FMath::Min(1.0f, CurrentState.AffordanceLandscape.Affordances.Num() / 10.0f);

    // Update action readiness based on best affordance
    FAffordance Best = GetBestAffordance();
    CurrentState.AffordanceLandscape.ActionReadiness = Best.Availability * Best.ContextualFit;
}

void UWisdomCultivation::ProcessOpponentDynamics(float DeltaTime)
{
    for (FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        // Advance phase
        Process.Phase += OpponentOscillationSpeed * Process.OscillationFrequency * DeltaTime;
        if (Process.Phase > 2.0f * PI)
        {
            Process.Phase -= 2.0f * PI;
        }

        // Natural oscillation around balance point
        float Oscillation = FMath::Sin(Process.Phase) * 0.1f;
        Process.Balance = FMath::Clamp(Process.Balance + Oscillation * DeltaTime, -1.0f, 1.0f);

        // Update pole strengths
        Process.PoleAStrength = 0.5f + Process.Balance * 0.5f;
        Process.PoleBStrength = 0.5f - Process.Balance * 0.5f;
    }
}

void UWisdomCultivation::UpdateRelevanceRealization()
{
    RealizeRelevance();

    // Update adaptability based on opponent process dynamics
    float TotalOscillation = 0.0f;
    for (const FOpponentProcessState& Process : CurrentState.OpponentProcesses)
    {
        TotalOscillation += Process.OscillationFrequency;
    }
    CurrentState.RelevanceRealization.Adaptability = 
        TotalOscillation / FMath::Max(1, CurrentState.OpponentProcesses.Num());
}

void UWisdomCultivation::CalculateOverallWisdom()
{
    // Wisdom = integration of all dimensions
    float DimensionSum = 0.0f;
    for (const FWisdomDimensionState& Dimension : CurrentState.WisdomDimensions)
    {
        DimensionSum += Dimension.Level;
    }
    float DimensionAverage = DimensionSum / FMath::Max(1, CurrentState.WisdomDimensions.Num());

    // Factor in knowing integration
    float KnowingIntegration = 0.0f;
    for (const FKnowingState& Knowing : CurrentState.KnowingStates)
    {
        KnowingIntegration += Knowing.Integration;
    }
    KnowingIntegration /= FMath::Max(1, CurrentState.KnowingStates.Num());

    // Factor in virtue development
    float VirtueDevelopment = GetOverallVirtueDevelopment();

    // Factor in relevance realization
    float RelevanceCapacity = CurrentState.RelevanceRealization.Capacity;

    // Overall wisdom is weighted combination
    CurrentState.OverallWisdom = 
        DimensionAverage * 0.3f +
        KnowingIntegration * 0.2f +
        VirtueDevelopment * 0.25f +
        RelevanceCapacity * 0.25f;
}
