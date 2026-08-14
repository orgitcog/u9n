// NicheConstructionSystem.cpp
// Implementation of Niche Construction System for Deep Tree Echo

#include "NicheConstructionSystem.h"
#include "ActiveInferenceEngine.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Sys6/Sys6LCMClockSynchronizer.h"
#include "../Core/CognitiveMemoryManager.h"
#include "Math/UnrealMathUtility.h"

UNicheConstructionSystem::UNicheConstructionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UNicheConstructionSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeNicheState();
    InitializeFitnessLandscape();
    InitializeInheritanceChannels();
    CurrentEnergy = ModificationEnergyBudget;
}

void UNicheConstructionSystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update affordance decay
    UpdateAffordanceDecay(DeltaTime);

    // Update scaffold integration
    UpdateScaffoldIntegration(DeltaTime);

    // Regenerate energy
    RegenerateEnergy(DeltaTime);
}

void UNicheConstructionSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        ActiveInferenceEngine = Owner->FindComponentByClass<UActiveInferenceEngine>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        LCMClock = Owner->FindComponentByClass<USys6LCMClockSynchronizer>();
        MemoryManager = Owner->FindComponentByClass<UCognitiveMemoryManager>();
    }
}

void UNicheConstructionSystem::InitializeNicheState()
{
    CurrentNicheState.NicheID = TEXT("DeepTreeEcho_Niche");
    CurrentNicheState.StateVector.SetNumZeroed(NicheDimensionality);
    CurrentNicheState.PreferredState.SetNumZeroed(NicheDimensionality);
    CurrentNicheState.FitnessGradient.SetNumZeroed(NicheDimensionality);

    // Initialize with random state
    for (int32 i = 0; i < NicheDimensionality; ++i)
    {
        CurrentNicheState.StateVector[i] = FMath::FRandRange(-1.0f, 1.0f);
        CurrentNicheState.PreferredState[i] = 0.0f;  // Prefer origin initially
    }

    CurrentNicheState.Stability = 0.5f;
    CurrentNicheState.Complexity = 0.5f;
    CurrentNicheState.CouplingStrength = 0.5f;
    CurrentNicheState.LastModification = 0.0f;
}

void UNicheConstructionSystem::InitializeFitnessLandscape()
{
    FitnessLandscape.LandscapeID = TEXT("DeepTreeEcho_FitnessLandscape");
    FitnessLandscape.Dimensions = NicheDimensionality;
    FitnessLandscape.LocalGradient.SetNumZeroed(NicheDimensionality);
    FitnessLandscape.Ruggedness = 0.3f;
    FitnessLandscape.Epistasis = 0.2f;
    FitnessLandscape.Neutrality = 0.1f;
    FitnessLandscape.AgentModifiability = 0.5f;

    // Compute initial fitness
    FitnessLandscape.CurrentFitness = ComputeCurrentFitness();
}

void UNicheConstructionSystem::InitializeInheritanceChannels()
{
    if (!bEnableInheritance)
    {
        return;
    }

    // Create default inheritance channels
    TArray<FString> ChannelTypes = {TEXT("Behavioral"), TEXT("Symbolic"), TEXT("Episodic"), TEXT("Procedural")};

    for (const FString& Type : ChannelTypes)
    {
        FInheritanceChannel Channel;
        Channel.ChannelID = FString::Printf(TEXT("Channel_%s"), *Type);
        Channel.ChannelType = Type;
        Channel.Fidelity = 0.9f;
        Channel.Bandwidth = 1.0f;
        Channel.Latency = 0.1f;
        Channel.bActive = true;

        InheritanceChannels.Add(Channel);
    }
}

void UNicheConstructionSystem::UpdateAffordanceDecay(float DeltaTime)
{
    float DecayAmount = AffordanceDecayRate * DeltaTime;

    for (int32 i = DetectedAffordances.Num() - 1; i >= 0; --i)
    {
        DetectedAffordances[i].Salience -= DecayAmount;

        // Remove affordances with zero salience
        if (DetectedAffordances[i].Salience <= 0.0f)
        {
            DetectedAffordances.RemoveAt(i);
        }
    }
}

void UNicheConstructionSystem::UpdateScaffoldIntegration(float DeltaTime)
{
    if (!bEnableScaffolding)
    {
        return;
    }

    // Scaffolds become more integrated over time with use
    for (FCognitiveScaffold& Scaffold : CognitiveScaffolds)
    {
        // Gradual integration increase
        float IntegrationRate = 0.01f * DeltaTime;
        Scaffold.IntegrationLevel = FMath::Min(1.0f, Scaffold.IntegrationLevel + IntegrationRate);
    }
}

FNicheState UNicheConstructionSystem::GetNicheState() const
{
    return CurrentNicheState;
}

void UNicheConstructionSystem::SetPreferredNicheState(const TArray<float>& PreferredState)
{
    if (PreferredState.Num() == NicheDimensionality)
    {
        CurrentNicheState.PreferredState = PreferredState;
    }
}

float UNicheConstructionSystem::GetNichePreferenceDistance() const
{
    float Distance = 0.0f;

    for (int32 i = 0; i < NicheDimensionality; ++i)
    {
        float Diff = CurrentNicheState.StateVector[i] - CurrentNicheState.PreferredState[i];
        Distance += Diff * Diff;
    }

    return FMath::Sqrt(Distance);
}

void UNicheConstructionSystem::UpdateNicheState(const TArray<float>& Observations)
{
    if (Observations.Num() == 0)
    {
        return;
    }

    // Update niche state based on observations
    for (int32 i = 0; i < FMath::Min(Observations.Num(), NicheDimensionality); ++i)
    {
        // Exponential moving average
        float Alpha = 0.1f;
        CurrentNicheState.StateVector[i] = Alpha * Observations[i] +
                                           (1.0f - Alpha) * CurrentNicheState.StateVector[i];
    }

    // Update stability based on state change
    float StateChange = 0.0f;
    for (int32 i = 0; i < FMath::Min(Observations.Num(), NicheDimensionality); ++i)
    {
        StateChange += FMath::Abs(Observations[i] - CurrentNicheState.StateVector[i]);
    }
    StateChange /= NicheDimensionality;

    CurrentNicheState.Stability = FMath::Lerp(CurrentNicheState.Stability, 1.0f - StateChange, 0.1f);

    // Update fitness
    float OldFitness = FitnessLandscape.CurrentFitness;
    FitnessLandscape.CurrentFitness = ComputeCurrentFitness();

    if (FMath::Abs(FitnessLandscape.CurrentFitness - OldFitness) > FitnessImprovementThreshold)
    {
        OnFitnessChanged.Broadcast(OldFitness, FitnessLandscape.CurrentFitness);
    }

    // Update fitness gradient
    FitnessLandscape.LocalGradient = ComputeFitnessGradient();
    CurrentNicheState.FitnessGradient = FitnessLandscape.LocalGradient;
}

float UNicheConstructionSystem::GetNicheStability() const
{
    return CurrentNicheState.Stability;
}

TArray<FEcologicalAffordance> UNicheConstructionSystem::DetectAffordances()
{
    TArray<FEcologicalAffordance> NewAffordances;
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // Generate affordances based on current niche state
    TArray<EAffordanceType> Types = {
        EAffordanceType::Physical,
        EAffordanceType::Social,
        EAffordanceType::Cognitive,
        EAffordanceType::Informational,
        EAffordanceType::Temporal
    };

    for (EAffordanceType Type : Types)
    {
        // Probability of detecting affordance depends on niche state
        float DetectionProb = 0.3f + 0.2f * CurrentNicheState.Stability;

        if (FMath::FRand() < DetectionProb)
        {
            FEcologicalAffordance Affordance = GenerateAffordance(Type);
            Affordance.DiscoveryTime = CurrentTime;

            // Check if we already have this affordance
            bool bExists = false;
            for (const FEcologicalAffordance& Existing : DetectedAffordances)
            {
                if (Existing.AffordanceID == Affordance.AffordanceID)
                {
                    bExists = true;
                    break;
                }
            }

            if (!bExists && DetectedAffordances.Num() < MaxAffordances)
            {
                DetectedAffordances.Add(Affordance);
                NewAffordances.Add(Affordance);
                OnAffordanceDiscovered.Broadcast(Affordance);
            }
        }
    }

    return NewAffordances;
}

FEcologicalAffordance UNicheConstructionSystem::GenerateAffordance(EAffordanceType Type)
{
    FEcologicalAffordance Affordance;

    static int32 AffordanceCounter = 0;
    Affordance.AffordanceID = FString::Printf(TEXT("Affordance_%d_%d"), (int32)Type, AffordanceCounter++);
    Affordance.Type = Type;

    // Generate action possibilities based on type
    switch (Type)
    {
    case EAffordanceType::Physical:
        Affordance.ActionPossibilities = {TEXT("Manipulate"), TEXT("Navigate"), TEXT("Construct")};
        Affordance.ModificationPotential = 0.7f;
        break;

    case EAffordanceType::Social:
        Affordance.ActionPossibilities = {TEXT("Communicate"), TEXT("Cooperate"), TEXT("Observe")};
        Affordance.ModificationPotential = 0.5f;
        break;

    case EAffordanceType::Cognitive:
        Affordance.ActionPossibilities = {TEXT("Learn"), TEXT("Remember"), TEXT("Reason")};
        Affordance.ModificationPotential = 0.3f;
        break;

    case EAffordanceType::Informational:
        Affordance.ActionPossibilities = {TEXT("Read"), TEXT("Write"), TEXT("Search")};
        Affordance.ModificationPotential = 0.6f;
        break;

    case EAffordanceType::Temporal:
        Affordance.ActionPossibilities = {TEXT("Wait"), TEXT("Schedule"), TEXT("Predict")};
        Affordance.ModificationPotential = 0.2f;
        break;
    }

    // Compute salience based on niche state
    Affordance.Salience = FMath::FRandRange(0.3f, 1.0f);

    // Compute goal relevance
    float Distance = GetNichePreferenceDistance();
    Affordance.GoalRelevance = FMath::Exp(-Distance) * Affordance.ModificationPotential;

    // Persistence based on type
    Affordance.Persistence = (Type == EAffordanceType::Physical) ? 10.0f : 5.0f;

    return Affordance;
}

FEcologicalAffordance UNicheConstructionSystem::GetAffordance(const FString& AffordanceID) const
{
    for (const FEcologicalAffordance& Affordance : DetectedAffordances)
    {
        if (Affordance.AffordanceID == AffordanceID)
        {
            return Affordance;
        }
    }
    return FEcologicalAffordance();
}

TArray<FEcologicalAffordance> UNicheConstructionSystem::GetMostSalientAffordances(int32 Count) const
{
    TArray<FEcologicalAffordance> Sorted = DetectedAffordances;

    // Sort by salience (descending)
    Sorted.Sort([](const FEcologicalAffordance& A, const FEcologicalAffordance& B)
    {
        return A.Salience > B.Salience;
    });

    // Return top N
    TArray<FEcologicalAffordance> Result;
    for (int32 i = 0; i < FMath::Min(Count, Sorted.Num()); ++i)
    {
        Result.Add(Sorted[i]);
    }

    return Result;
}

TArray<FEcologicalAffordance> UNicheConstructionSystem::GetAffordancesByType(EAffordanceType Type) const
{
    TArray<FEcologicalAffordance> Result;

    for (const FEcologicalAffordance& Affordance : DetectedAffordances)
    {
        if (Affordance.Type == Type)
        {
            Result.Add(Affordance);
        }
    }

    return Result;
}

FNicheModification UNicheConstructionSystem::ExploitAffordance(const FString& AffordanceID)
{
    FEcologicalAffordance Affordance = GetAffordance(AffordanceID);

    if (Affordance.AffordanceID.IsEmpty())
    {
        return FNicheModification();
    }

    // Determine modification type based on affordance type
    ENicheConstructionType ConstructionType = ENicheConstructionType::Perturbation;
    ENicheDimension Dimension = ENicheDimension::Spatial;

    switch (Affordance.Type)
    {
    case EAffordanceType::Physical:
        ConstructionType = ENicheConstructionType::Perturbation;
        Dimension = ENicheDimension::Spatial;
        break;

    case EAffordanceType::Social:
        ConstructionType = ENicheConstructionType::Inheritance;
        Dimension = ENicheDimension::Social;
        break;

    case EAffordanceType::Cognitive:
        ConstructionType = ENicheConstructionType::Scaffolding;
        Dimension = ENicheDimension::Cognitive;
        break;

    case EAffordanceType::Informational:
        ConstructionType = ENicheConstructionType::Scaffolding;
        Dimension = ENicheDimension::Informational;
        break;

    case EAffordanceType::Temporal:
        ConstructionType = ENicheConstructionType::Relocation;
        Dimension = ENicheDimension::Temporal;
        break;
    }

    // Plan and execute modification
    FNicheModification Modification = PlanNicheModification(ConstructionType, Dimension);

    // Scale modification by affordance properties
    for (float& Val : Modification.ModificationVector)
    {
        Val *= Affordance.ModificationPotential * Affordance.Salience;
    }

    // Execute if we have energy
    if (CurrentEnergy >= Modification.EnergyCost)
    {
        ExecuteNicheModification(Modification);
    }

    return Modification;
}

FNicheModification UNicheConstructionSystem::PlanNicheModification(ENicheConstructionType Type, ENicheDimension Dimension)
{
    FNicheModification Modification;
    Modification.ModificationID = NextModificationID++;
    Modification.Type = Type;
    Modification.Dimension = Dimension;
    Modification.Timestamp = GetWorld()->GetTimeSeconds();

    // Initialize modification vector
    Modification.ModificationVector.SetNumZeroed(NicheDimensionality);

    // Compute modification based on fitness gradient
    TArray<float> Gradient = ComputeFitnessGradient();

    // Determine dimension indices
    int32 StartIdx = 0;
    int32 EndIdx = NicheDimensionality;

    switch (Dimension)
    {
    case ENicheDimension::Spatial:
        StartIdx = 0;
        EndIdx = NicheDimensionality / 5;
        break;
    case ENicheDimension::Temporal:
        StartIdx = NicheDimensionality / 5;
        EndIdx = 2 * NicheDimensionality / 5;
        break;
    case ENicheDimension::Social:
        StartIdx = 2 * NicheDimensionality / 5;
        EndIdx = 3 * NicheDimensionality / 5;
        break;
    case ENicheDimension::Informational:
        StartIdx = 3 * NicheDimensionality / 5;
        EndIdx = 4 * NicheDimensionality / 5;
        break;
    case ENicheDimension::Cognitive:
        StartIdx = 4 * NicheDimensionality / 5;
        EndIdx = NicheDimensionality;
        break;
    }

    // Set modification based on type
    float ModificationStrength = 0.1f;

    switch (Type)
    {
    case ENicheConstructionType::Perturbation:
        // Direct environment modification - follow gradient
        for (int32 i = StartIdx; i < EndIdx && i < Gradient.Num(); ++i)
        {
            Modification.ModificationVector[i] = ModificationStrength * Gradient[i];
        }
        Modification.EnergyCost = 10.0f;
        break;

    case ENicheConstructionType::Relocation:
        // Move toward preferred state
        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            float Diff = CurrentNicheState.PreferredState[i] - CurrentNicheState.StateVector[i];
            Modification.ModificationVector[i] = ModificationStrength * Diff;
        }
        Modification.EnergyCost = 5.0f;
        break;

    case ENicheConstructionType::Inheritance:
        // Transmit information to modify future niche
        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            Modification.ModificationVector[i] = ModificationStrength * CurrentNicheState.StateVector[i];
        }
        Modification.EnergyCost = 3.0f;
        break;

    case ENicheConstructionType::Scaffolding:
        // Create cognitive extension
        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            Modification.ModificationVector[i] = ModificationStrength * 0.5f;
        }
        Modification.EnergyCost = 8.0f;
        break;
    }

    // Compute expected fitness change
    TArray<float> PredictedState = CurrentNicheState.StateVector;
    for (int32 i = 0; i < NicheDimensionality; ++i)
    {
        PredictedState[i] += Modification.ModificationVector[i];
    }
    float PredictedFitness = ComputeFitnessAtState(PredictedState);
    Modification.ExpectedFitnessChange = PredictedFitness - FitnessLandscape.CurrentFitness;

    return Modification;
}

FNicheConstructionEvent UNicheConstructionSystem::ExecuteNicheModification(const FNicheModification& Modification)
{
    FNicheConstructionEvent Event;
    Event.EventID = NextEventID++;
    Event.Type = Modification.Type;
    Event.Modification = const_cast<FNicheModification&>(Modification);
    Event.StateBefore = CurrentNicheState;
    Event.Timestamp = GetWorld()->GetTimeSeconds();

    // Get current echobeat/LCM step
    if (LCMClock)
    {
        // Event.LCMStep = LCMClock->GetCurrentLCMStep();
        // Event.EchobeatStep = LCMClock->GetCurrentEchobeatStep();
    }

    // Check energy
    if (CurrentEnergy < Modification.EnergyCost)
    {
        Event.Modification.Success = 0.0f;
        return Event;
    }

    // Consume energy
    ConsumeEnergy(Modification.EnergyCost);

    // Apply modification
    ApplyModificationToNiche(Modification);

    // Record state after
    Event.StateAfter = CurrentNicheState;

    // Compute actual fitness change
    float OldFitness = FitnessLandscape.CurrentFitness;
    FitnessLandscape.CurrentFitness = ComputeCurrentFitness();
    Event.FitnessChange = FitnessLandscape.CurrentFitness - OldFitness;

    // Update modification success
    FNicheModification& MutableMod = const_cast<FNicheModification&>(Modification);
    MutableMod.ActualFitnessChange = Event.FitnessChange;
    MutableMod.Success = (Event.FitnessChange > 0) ? 1.0f : 0.5f;

    // Store in history
    ModificationHistory.Add(Modification);
    ConstructionEvents.Add(Event);

    // Broadcast events
    OnNicheModified.Broadcast(Modification);
    BroadcastConstructionEvent(Event);

    if (FMath::Abs(Event.FitnessChange) > FitnessImprovementThreshold)
    {
        OnFitnessChanged.Broadcast(OldFitness, FitnessLandscape.CurrentFitness);
    }

    return Event;
}

void UNicheConstructionSystem::ApplyModificationToNiche(const FNicheModification& Modification)
{
    // Apply modification vector to niche state
    for (int32 i = 0; i < NicheDimensionality && i < Modification.ModificationVector.Num(); ++i)
    {
        CurrentNicheState.StateVector[i] += Modification.ModificationVector[i];

        // Clamp to valid range
        CurrentNicheState.StateVector[i] = FMath::Clamp(CurrentNicheState.StateVector[i], -10.0f, 10.0f);
    }

    // Update niche properties
    CurrentNicheState.LastModification = GetWorld()->GetTimeSeconds();

    // Stability decreases with modification
    CurrentNicheState.Stability *= 0.95f;

    // Complexity may increase
    CurrentNicheState.Complexity = FMath::Min(1.0f, CurrentNicheState.Complexity + 0.01f);

    // Update coupling strength based on modification type
    if (Modification.Type == ENicheConstructionType::Scaffolding)
    {
        CurrentNicheState.CouplingStrength = FMath::Min(1.0f, CurrentNicheState.CouplingStrength + 0.05f);
    }
}

float UNicheConstructionSystem::EvaluateModificationPotential(const FNicheModification& Modification) const
{
    // Evaluate potential based on expected fitness change and cost
    float Potential = Modification.ExpectedFitnessChange / FMath::Max(0.1f, Modification.EnergyCost);

    // Factor in niche stability
    Potential *= CurrentNicheState.Stability;

    // Factor in landscape modifiability
    Potential *= FitnessLandscape.AgentModifiability;

    return Potential;
}

TArray<FNicheModification> UNicheConstructionSystem::GetModificationHistory() const
{
    return ModificationHistory;
}

bool UNicheConstructionSystem::UndoLastModification()
{
    if (ModificationHistory.Num() == 0)
    {
        return false;
    }

    // Get last modification
    FNicheModification LastMod = ModificationHistory.Last();

    // Create inverse modification
    FNicheModification InverseMod = LastMod;
    InverseMod.ModificationID = NextModificationID++;
    for (float& Val : InverseMod.ModificationVector)
    {
        Val = -Val;
    }

    // Apply inverse
    ApplyModificationToNiche(InverseMod);

    // Remove from history
    ModificationHistory.Pop();

    return true;
}

FCognitiveScaffold UNicheConstructionSystem::CreateScaffold(const FString& ScaffoldType, const FString& SupportedFunction)
{
    if (!bEnableScaffolding || CognitiveScaffolds.Num() >= MaxScaffolds)
    {
        return FCognitiveScaffold();
    }

    FCognitiveScaffold Scaffold;
    static int32 ScaffoldCounter = 0;
    Scaffold.ScaffoldID = FString::Printf(TEXT("Scaffold_%d"), ScaffoldCounter++);
    Scaffold.ScaffoldType = ScaffoldType;
    Scaffold.SupportedFunction = SupportedFunction;
    Scaffold.OffloadingDegree = 0.3f;
    Scaffold.Reliability = 0.9f;
    Scaffold.IntegrationLevel = 0.1f;
    Scaffold.CreationTime = GetWorld()->GetTimeSeconds();

    CognitiveScaffolds.Add(Scaffold);
    OnScaffoldCreated.Broadcast(Scaffold);

    return Scaffold;
}

FCognitiveScaffold UNicheConstructionSystem::GetScaffold(const FString& ScaffoldID) const
{
    for (const FCognitiveScaffold& Scaffold : CognitiveScaffolds)
    {
        if (Scaffold.ScaffoldID == ScaffoldID)
        {
            return Scaffold;
        }
    }
    return FCognitiveScaffold();
}

TArray<FCognitiveScaffold> UNicheConstructionSystem::GetAllScaffolds() const
{
    return CognitiveScaffolds;
}

float UNicheConstructionSystem::UseScaffold(const FString& ScaffoldID)
{
    for (FCognitiveScaffold& Scaffold : CognitiveScaffolds)
    {
        if (Scaffold.ScaffoldID == ScaffoldID)
        {
            // Using scaffold increases integration
            Scaffold.IntegrationLevel = FMath::Min(1.0f, Scaffold.IntegrationLevel + 0.05f);

            // Return cognitive benefit
            return Scaffold.OffloadingDegree * Scaffold.Reliability * Scaffold.IntegrationLevel;
        }
    }
    return 0.0f;
}

bool UNicheConstructionSystem::RemoveScaffold(const FString& ScaffoldID)
{
    for (int32 i = 0; i < CognitiveScaffolds.Num(); ++i)
    {
        if (CognitiveScaffolds[i].ScaffoldID == ScaffoldID)
        {
            CognitiveScaffolds.RemoveAt(i);
            return true;
        }
    }
    return false;
}

float UNicheConstructionSystem::GetScaffoldIntegration(const FString& ScaffoldID) const
{
    FCognitiveScaffold Scaffold = GetScaffold(ScaffoldID);
    return Scaffold.IntegrationLevel;
}

FFitnessLandscape UNicheConstructionSystem::GetFitnessLandscape() const
{
    return FitnessLandscape;
}

float UNicheConstructionSystem::ComputeCurrentFitness()
{
    return ComputeFitnessAtState(CurrentNicheState.StateVector);
}

float UNicheConstructionSystem::ComputeFitnessAtState(const TArray<float>& State) const
{
    // Fitness function: combination of distance to preferred state and landscape features

    // Distance to preferred state (lower is better)
    float Distance = 0.0f;
    for (int32 i = 0; i < FMath::Min(State.Num(), CurrentNicheState.PreferredState.Num()); ++i)
    {
        float Diff = State[i] - CurrentNicheState.PreferredState[i];
        Distance += Diff * Diff;
    }
    Distance = FMath::Sqrt(Distance);

    // Base fitness from distance
    float Fitness = FMath::Exp(-Distance * 0.5f);

    // Add ruggedness (local optima)
    if (FitnessLandscape.Ruggedness > 0.0f)
    {
        float Ruggedness = 0.0f;
        for (int32 i = 0; i < State.Num(); ++i)
        {
            Ruggedness += FMath::Sin(State[i] * 3.14159f * 2.0f) * FitnessLandscape.Ruggedness;
        }
        Ruggedness /= State.Num();
        Fitness += Ruggedness * 0.1f;
    }

    // Add epistasis (interaction effects)
    if (FitnessLandscape.Epistasis > 0.0f && State.Num() >= 2)
    {
        float Epistasis = 0.0f;
        for (int32 i = 0; i < State.Num() - 1; ++i)
        {
            Epistasis += State[i] * State[i + 1] * FitnessLandscape.Epistasis;
        }
        Epistasis /= State.Num();
        Fitness += Epistasis * 0.05f;
    }

    return FMath::Clamp(Fitness, 0.0f, 1.0f);
}

TArray<float> UNicheConstructionSystem::ComputeFitnessGradient()
{
    TArray<float> Gradient;
    Gradient.SetNumZeroed(NicheDimensionality);

    float CurrentFitness = FitnessLandscape.CurrentFitness;
    float Epsilon = 0.01f;

    for (int32 i = 0; i < NicheDimensionality; ++i)
    {
        // Compute numerical gradient
        TArray<float> PerturbedState = CurrentNicheState.StateVector;
        PerturbedState[i] += Epsilon;

        float PerturbedFitness = ComputeFitnessAtState(PerturbedState);
        Gradient[i] = (PerturbedFitness - CurrentFitness) / Epsilon;
    }

    return Gradient;
}

void UNicheConstructionSystem::ModifyFitnessLandscape(const TArray<float>& Modification)
{
    // Modify landscape properties based on agent actions
    // This represents the agent's ability to reshape the fitness landscape itself

    if (Modification.Num() >= 3)
    {
        FitnessLandscape.Ruggedness = FMath::Clamp(FitnessLandscape.Ruggedness + Modification[0] * 0.1f, 0.0f, 1.0f);
        FitnessLandscape.Epistasis = FMath::Clamp(FitnessLandscape.Epistasis + Modification[1] * 0.1f, 0.0f, 1.0f);
        FitnessLandscape.Neutrality = FMath::Clamp(FitnessLandscape.Neutrality + Modification[2] * 0.1f, 0.0f, 1.0f);
    }

    // Recompute fitness and gradient
    FitnessLandscape.CurrentFitness = ComputeCurrentFitness();
    FitnessLandscape.LocalGradient = ComputeFitnessGradient();
}

TArray<float> UNicheConstructionSystem::FindLocalOptimum(int32 MaxIterations)
{
    TArray<float> CurrentState = CurrentNicheState.StateVector;
    float CurrentFitness = ComputeFitnessAtState(CurrentState);

    float LearningRate = 0.1f;

    for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
    {
        // Compute gradient at current state
        TArray<float> Gradient;
        Gradient.SetNumZeroed(NicheDimensionality);

        float Epsilon = 0.01f;
        for (int32 i = 0; i < NicheDimensionality; ++i)
        {
            TArray<float> PerturbedState = CurrentState;
            PerturbedState[i] += Epsilon;
            float PerturbedFitness = ComputeFitnessAtState(PerturbedState);
            Gradient[i] = (PerturbedFitness - CurrentFitness) / Epsilon;
        }

        // Update state (gradient ascent)
        for (int32 i = 0; i < NicheDimensionality; ++i)
        {
            CurrentState[i] += LearningRate * Gradient[i];
        }

        // Compute new fitness
        float NewFitness = ComputeFitnessAtState(CurrentState);

        // Check convergence
        if (FMath::Abs(NewFitness - CurrentFitness) < 0.0001f)
        {
            break;
        }

        CurrentFitness = NewFitness;
    }

    return CurrentState;
}

TArray<FSelectivePressure> UNicheConstructionSystem::GetSelectivePressures() const
{
    return SelectivePressures;
}

void UNicheConstructionSystem::AddSelectivePressure(const FSelectivePressure& Pressure)
{
    SelectivePressures.Add(Pressure);
    OnSelectivePressureChanged.Broadcast(Pressure);
}

void UNicheConstructionSystem::RemoveSelectivePressure(const FString& PressureID)
{
    for (int32 i = 0; i < SelectivePressures.Num(); ++i)
    {
        if (SelectivePressures[i].PressureID == PressureID)
        {
            SelectivePressures.RemoveAt(i);
            return;
        }
    }
}

TArray<float> UNicheConstructionSystem::ComputeNetSelectiveForce() const
{
    TArray<float> NetForce;
    NetForce.SetNumZeroed(NicheDimensionality);

    for (const FSelectivePressure& Pressure : SelectivePressures)
    {
        for (int32 i = 0; i < FMath::Min(Pressure.Direction.Num(), NicheDimensionality); ++i)
        {
            NetForce[i] += Pressure.Direction[i] * Pressure.Intensity;
        }
    }

    return NetForce;
}

TArray<FInheritanceChannel> UNicheConstructionSystem::GetInheritanceChannels() const
{
    return InheritanceChannels;
}

bool UNicheConstructionSystem::TransmitInformation(const FString& ChannelID, const TArray<float>& Information)
{
    for (FInheritanceChannel& Channel : InheritanceChannels)
    {
        if (Channel.ChannelID == ChannelID && Channel.bActive)
        {
            // Simulate transmission with fidelity
            // In a real system, this would store information for future retrieval
            return true;
        }
    }
    return false;
}

TArray<float> UNicheConstructionSystem::ReceiveInformation(const FString& ChannelID)
{
    TArray<float> Information;

    for (const FInheritanceChannel& Channel : InheritanceChannels)
    {
        if (Channel.ChannelID == ChannelID && Channel.bActive)
        {
            // Return inherited information (simplified)
            Information.SetNumZeroed(NicheDimensionality);
            for (int32 i = 0; i < NicheDimensionality; ++i)
            {
                Information[i] = CurrentNicheState.StateVector[i] * Channel.Fidelity;
            }
            break;
        }
    }

    return Information;
}

FInheritanceChannel UNicheConstructionSystem::CreateInheritanceChannel(const FString& ChannelType)
{
    FInheritanceChannel Channel;
    static int32 ChannelCounter = 0;
    Channel.ChannelID = FString::Printf(TEXT("Channel_%d"), ChannelCounter++);
    Channel.ChannelType = ChannelType;
    Channel.Fidelity = 0.9f;
    Channel.Bandwidth = 1.0f;
    Channel.Latency = 0.1f;
    Channel.bActive = true;

    InheritanceChannels.Add(Channel);

    return Channel;
}

float UNicheConstructionSystem::GetCurrentEnergy() const
{
    return CurrentEnergy;
}

float UNicheConstructionSystem::GetEnergyBudget() const
{
    return ModificationEnergyBudget;
}

bool UNicheConstructionSystem::ConsumeEnergy(float Amount)
{
    if (CurrentEnergy >= Amount)
    {
        CurrentEnergy -= Amount;
        return true;
    }
    return false;
}

void UNicheConstructionSystem::RegenerateEnergy(float DeltaTime)
{
    CurrentEnergy = FMath::Min(ModificationEnergyBudget, CurrentEnergy + EnergyRegenerationRate * DeltaTime);
}

void UNicheConstructionSystem::ProcessSys6Step(int32 LCMStep)
{
    // Map LCM step to echobeat step
    int32 EchobeatStep = ((LCMStep * 12) / 30) + 1;
    EchobeatStep = FMath::Clamp(EchobeatStep, 1, 12);

    // Get construction mode for this step
    ENicheConstructionType Mode = GetConstructionModeForStep(EchobeatStep);

    // Perform niche construction based on mode
    switch (Mode)
    {
    case ENicheConstructionType::Perturbation:
        // Detect and exploit physical affordances
        DetectAffordances();
        break;

    case ENicheConstructionType::Relocation:
        // Evaluate relocation options
        ComputeFitnessGradient();
        break;

    case ENicheConstructionType::Inheritance:
        // Process inheritance channels
        if (bEnableInheritance)
        {
            for (const FInheritanceChannel& Channel : InheritanceChannels)
            {
                if (Channel.bActive)
                {
                    ReceiveInformation(Channel.ChannelID);
                }
            }
        }
        break;

    case ENicheConstructionType::Scaffolding:
        // Update scaffold integration
        for (const FCognitiveScaffold& Scaffold : CognitiveScaffolds)
        {
            UseScaffold(Scaffold.ScaffoldID);
        }
        break;
    }
}

ENicheConstructionType UNicheConstructionSystem::GetConstructionModeForStep(int32 EchobeatStep) const
{
    // Map echobeat steps to niche construction modes
    // Steps 1,5,9: Perturbation (physical modification)
    // Steps 2,6,10: Relocation (environment selection)
    // Steps 3,7,11: Inheritance (cultural transmission)
    // Steps 4,8,12: Scaffolding (cognitive extension)

    int32 Phase = ((EchobeatStep - 1) % 4);

    switch (Phase)
    {
    case 0:
        return ENicheConstructionType::Perturbation;
    case 1:
        return ENicheConstructionType::Relocation;
    case 2:
        return bEnableInheritance ? ENicheConstructionType::Inheritance : ENicheConstructionType::Perturbation;
    case 3:
        return bEnableScaffolding ? ENicheConstructionType::Scaffolding : ENicheConstructionType::Perturbation;
    default:
        return ENicheConstructionType::Perturbation;
    }
}

float UNicheConstructionSystem::ComputeModificationCost(const FNicheModification& Modification) const
{
    float BaseCost = 5.0f;

    // Cost depends on modification type
    switch (Modification.Type)
    {
    case ENicheConstructionType::Perturbation:
        BaseCost = 10.0f;
        break;
    case ENicheConstructionType::Relocation:
        BaseCost = 5.0f;
        break;
    case ENicheConstructionType::Inheritance:
        BaseCost = 3.0f;
        break;
    case ENicheConstructionType::Scaffolding:
        BaseCost = 8.0f;
        break;
    }

    // Cost scales with modification magnitude
    float Magnitude = 0.0f;
    for (float Val : Modification.ModificationVector)
    {
        Magnitude += FMath::Abs(Val);
    }
    Magnitude /= FMath::Max(1, Modification.ModificationVector.Num());

    return BaseCost * (1.0f + Magnitude);
}

void UNicheConstructionSystem::BroadcastConstructionEvent(const FNicheConstructionEvent& Event)
{
    OnNicheConstructionEvent.Broadcast(Event);
}
