// ActiveInferenceCognitiveIntegration.cpp
// Implementation of Active Inference Cognitive Integration for Deep Tree Echo

#include "ActiveInferenceCognitiveIntegration.h"
#include "ActiveInferenceEngine.h"
#include "NicheConstructionSystem.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Sys6/Sys6LCMClockSynchronizer.h"
#include "../Core/Sys6OperadEngine.h"
#include "../Echobeats/TensionalCouplingDynamics.h"
#include "../Core/CognitiveMemoryManager.h"
#include "../Learning/PredictiveAdaptationEngine.h"
#include "Math/UnrealMathUtility.h"

UActiveInferenceCognitiveIntegration::UActiveInferenceCognitiveIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UActiveInferenceCognitiveIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeState();
}

void UActiveInferenceCognitiveIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update stream states
    UpdateStreamStates();

    // Update 4E metrics
    if (bEnable4ECognition)
    {
        Update4EMetrics();
    }

    // Check for automatic niche construction
    if (bAutoNicheConstruction)
    {
        float Opportunity = EvaluateNicheConstructionOpportunity();
        if (Opportunity > NicheConstructionThreshold)
        {
            TriggerNicheConstruction();
        }
    }
}

void UActiveInferenceCognitiveIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        ActiveInferenceEngine = Owner->FindComponentByClass<UActiveInferenceEngine>();
        NicheSystem = Owner->FindComponentByClass<UNicheConstructionSystem>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        LCMClock = Owner->FindComponentByClass<USys6LCMClockSynchronizer>();
        OperadEngine = Owner->FindComponentByClass<USys6OperadEngine>();
        TensionalCoupling = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
        MemoryManager = Owner->FindComponentByClass<UCognitiveMemoryManager>();
        PredictiveEngine = Owner->FindComponentByClass<UPredictiveAdaptationEngine>();
    }
}

void UActiveInferenceCognitiveIntegration::InitializeState()
{
    // Initialize stream states for 3 concurrent streams
    CurrentState.StreamStates.Empty();
    for (int32 i = 0; i < 3; ++i)
    {
        FStreamSyncState StreamState;
        StreamState.StreamIndex = i;
        // Streams are phased 4 steps apart (120 degrees)
        StreamState.EchobeatStep = 1 + (i * 4);
        if (StreamState.EchobeatStep > 12)
        {
            StreamState.EchobeatStep -= 12;
        }
        StreamState.Mode4E = E4ECognitionMode::Embodied;
        StreamState.Phase = ECognitiveIntegrationPhase::Perception;
        StreamState.FreeEnergy = 0.0f;
        StreamState.NicheFitness = 0.5f;
        StreamState.StreamCoherence = 1.0f;

        CurrentState.StreamStates.Add(StreamState);
    }

    CurrentState.LCMStep = 0;
    CurrentState.EchobeatStep = 1;
    CurrentState.DyadState = TEXT("A");
    CurrentState.TriadState = 1;
    CurrentState.GlobalFreeEnergy = 0.0f;
    CurrentState.GlobalNicheFitness = 0.5f;
    CurrentState.InterStreamCoherence = 1.0f;
    CurrentState.Active4EMode = E4ECognitionMode::Embodied;
    CurrentState.Timestamp = 0.0f;

    // Initialize 4E metrics
    Current4EMetrics.EmbodimentIndex = 0.5f;
    Current4EMetrics.EmbeddednessIndex = 0.5f;
    Current4EMetrics.EnactionIndex = 0.5f;
    Current4EMetrics.ExtensionIndex = 0.5f;
    Current4EMetrics.OverallIntegration = 0.5f;
    Current4EMetrics.MarkovBlanketIntegrity = 1.0f;
    Current4EMetrics.NicheConstructionActivity = 0.0f;
}

void UActiveInferenceCognitiveIntegration::UpdateStreamStates()
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    CurrentState.Timestamp = CurrentTime;

    // Update each stream state
    for (int32 i = 0; i < CurrentState.StreamStates.Num(); ++i)
    {
        FStreamSyncState& StreamState = CurrentState.StreamStates[i];

        // Get free energy from active inference engine
        if (ActiveInferenceEngine)
        {
            StreamState.FreeEnergy = ActiveInferenceEngine->GetVariationalFreeEnergy();
        }

        // Get niche fitness from niche system
        if (NicheSystem)
        {
            StreamState.NicheFitness = NicheSystem->GetFitnessLandscape().CurrentFitness;
        }

        // Update 4E mode based on echobeat step
        StreamState.Mode4E = Get4EModeForStep(StreamState.EchobeatStep);

        // Update phase based on echobeat step
        StreamState.Phase = GetPhaseForStep(StreamState.EchobeatStep);

        // Compute coherence with other streams
        float Coherence = 0.0f;
        int32 CoherenceCount = 0;
        for (int32 j = 0; j < CurrentState.StreamStates.Num(); ++j)
        {
            if (i != j)
            {
                Coherence += ComputeStreamCoherence(i, j);
                CoherenceCount++;
            }
        }
        if (CoherenceCount > 0)
        {
            StreamState.StreamCoherence = Coherence / CoherenceCount;
        }
    }

    // Update global state
    CurrentState.GlobalFreeEnergy = ComputeGlobalFreeEnergy();
    CurrentState.GlobalNicheFitness = ComputeGlobalNicheFitness();
    CurrentState.InterStreamCoherence = GetInterStreamCoherence();
    CurrentState.Active4EMode = Current4EMode;

    OnCognitiveStateChanged.Broadcast(CurrentState);
}

void UActiveInferenceCognitiveIntegration::Update4EMetrics()
{
    // Compute individual 4E indices
    Current4EMetrics.EmbodimentIndex = ComputeEmbodimentIndex();
    Current4EMetrics.EmbeddednessIndex = ComputeEmbeddednessIndex();
    Current4EMetrics.EnactionIndex = ComputeEnactionIndex();
    Current4EMetrics.ExtensionIndex = ComputeExtensionIndex();

    // Compute overall integration
    Current4EMetrics.OverallIntegration = (
        Current4EMetrics.EmbodimentIndex +
        Current4EMetrics.EmbeddednessIndex +
        Current4EMetrics.EnactionIndex +
        Current4EMetrics.ExtensionIndex
    ) / 4.0f;

    // Get Markov blanket integrity from active inference
    if (ActiveInferenceEngine)
    {
        Current4EMetrics.MarkovBlanketIntegrity = ActiveInferenceEngine->ComputeBlanketIntegrity();
    }

    // Get niche construction activity
    if (NicheSystem)
    {
        TArray<FNicheModification> History = NicheSystem->GetModificationHistory();
        float RecentActivity = 0.0f;
        float CurrentTime = GetWorld()->GetTimeSeconds();

        for (const FNicheModification& Mod : History)
        {
            float TimeSince = CurrentTime - Mod.Timestamp;
            if (TimeSince < 10.0f)  // Last 10 seconds
            {
                RecentActivity += Mod.Success * FMath::Exp(-TimeSince * 0.1f);
            }
        }
        Current4EMetrics.NicheConstructionActivity = FMath::Min(1.0f, RecentActivity);
    }

    On4EMetricsUpdated.Broadcast(Current4EMetrics);
}

FIntegratedCognitiveState UActiveInferenceCognitiveIntegration::GetIntegratedState() const
{
    return CurrentState;
}

FStreamSyncState UActiveInferenceCognitiveIntegration::GetStreamState(int32 StreamIndex) const
{
    if (StreamIndex >= 0 && StreamIndex < CurrentState.StreamStates.Num())
    {
        return CurrentState.StreamStates[StreamIndex];
    }
    return FStreamSyncState();
}

E4ECognitionMode UActiveInferenceCognitiveIntegration::GetCurrent4EMode() const
{
    return Current4EMode;
}

ECognitiveIntegrationPhase UActiveInferenceCognitiveIntegration::GetCurrentPhase() const
{
    return CurrentPhase;
}

float UActiveInferenceCognitiveIntegration::GetGlobalFreeEnergy() const
{
    return CurrentState.GlobalFreeEnergy;
}

float UActiveInferenceCognitiveIntegration::GetGlobalNicheFitness() const
{
    return CurrentState.GlobalNicheFitness;
}

F4ECognitionMetrics UActiveInferenceCognitiveIntegration::Get4EMetrics() const
{
    return Current4EMetrics;
}

void UActiveInferenceCognitiveIntegration::Set4EMode(E4ECognitionMode Mode)
{
    Transition4EMode(Mode);
}

E4ECognitionMode UActiveInferenceCognitiveIntegration::Get4EModeForStep(int32 EchobeatStep) const
{
    // Map echobeat steps to 4E modes
    // The 12-step cycle maps to the 4 modes with overlap
    // Steps 1,2,3: Embodied (body-centric processing)
    // Steps 4,5,6: Embedded (environmental exploitation)
    // Steps 7,8,9: Enacted (sensorimotor contingencies)
    // Steps 10,11,12: Extended (cognitive scaffolding)

    int32 ModeIndex = ((EchobeatStep - 1) / 3) % 4;

    switch (ModeIndex)
    {
    case 0:
        return E4ECognitionMode::Embodied;
    case 1:
        return E4ECognitionMode::Embedded;
    case 2:
        return E4ECognitionMode::Enacted;
    case 3:
        return E4ECognitionMode::Extended;
    default:
        return E4ECognitionMode::Embodied;
    }
}

float UActiveInferenceCognitiveIntegration::ComputeEmbodimentIndex() const
{
    // Embodiment: degree to which cognition is shaped by body
    // Based on sensorimotor coupling and proprioceptive accuracy

    float Index = 0.5f;

    // Factor in Markov blanket integrity (body boundary)
    if (ActiveInferenceEngine)
    {
        Index = ActiveInferenceEngine->ComputeBlanketIntegrity();
    }

    // Factor in niche coupling strength
    if (NicheSystem)
    {
        FNicheState NicheState = NicheSystem->GetNicheState();
        Index = (Index + NicheState.CouplingStrength) / 2.0f;
    }

    return FMath::Clamp(Index, 0.0f, 1.0f);
}

float UActiveInferenceCognitiveIntegration::ComputeEmbeddednessIndex() const
{
    // Embeddedness: degree to which cognition exploits environmental structure

    float Index = 0.5f;

    // Factor in niche stability (stable environment = more embedded)
    if (NicheSystem)
    {
        Index = NicheSystem->GetNicheStability();
    }

    // Factor in affordance exploitation
    if (NicheSystem)
    {
        TArray<FEcologicalAffordance> Affordances = NicheSystem->GetMostSalientAffordances(5);
        float AffordanceUtilization = 0.0f;
        for (const FEcologicalAffordance& Aff : Affordances)
        {
            AffordanceUtilization += Aff.GoalRelevance * Aff.Salience;
        }
        if (Affordances.Num() > 0)
        {
            AffordanceUtilization /= Affordances.Num();
        }
        Index = (Index + AffordanceUtilization) / 2.0f;
    }

    return FMath::Clamp(Index, 0.0f, 1.0f);
}

float UActiveInferenceCognitiveIntegration::ComputeEnactionIndex() const
{
    // Enaction: degree to which cognition emerges from sensorimotor interaction

    float Index = 0.5f;

    // Factor in action-perception coupling
    if (ActiveInferenceEngine)
    {
        // Low free energy indicates good action-perception coupling
        float FE = ActiveInferenceEngine->GetVariationalFreeEnergy();
        Index = FMath::Exp(-FE);
    }

    // Factor in prediction error (lower = better enaction)
    if (PredictiveEngine)
    {
        // Get recent prediction errors
        // Index = (Index + PredictiveEngine->GetAveragePredictionAccuracy()) / 2.0f;
    }

    return FMath::Clamp(Index, 0.0f, 1.0f);
}

float UActiveInferenceCognitiveIntegration::ComputeExtensionIndex() const
{
    // Extension: degree to which cognition extends into environment

    float Index = 0.0f;

    // Factor in cognitive scaffolding
    if (NicheSystem)
    {
        TArray<FCognitiveScaffold> Scaffolds = NicheSystem->GetAllScaffolds();
        float ScaffoldIntegration = 0.0f;

        for (const FCognitiveScaffold& Scaffold : Scaffolds)
        {
            ScaffoldIntegration += Scaffold.IntegrationLevel * Scaffold.OffloadingDegree;
        }

        if (Scaffolds.Num() > 0)
        {
            Index = ScaffoldIntegration / Scaffolds.Num();
        }
    }

    // Factor in niche construction activity
    Index = (Index + Current4EMetrics.NicheConstructionActivity) / 2.0f;

    return FMath::Clamp(Index, 0.0f, 1.0f);
}

void UActiveInferenceCognitiveIntegration::ProcessCognitiveStep(int32 LCMStep)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float FreeEnergyBefore = CurrentState.GlobalFreeEnergy;

    // Update LCM step
    CurrentState.LCMStep = LCMStep;

    // Map LCM step to echobeat step (30 LCM steps -> 12 echobeat steps)
    int32 EchobeatStep = ((LCMStep * 12) / 30) + 1;
    EchobeatStep = FMath::Clamp(EchobeatStep, 1, 12);
    CurrentState.EchobeatStep = EchobeatStep;

    // Update dyad/triad state based on sys6 pattern
    // Double step delay pattern: A1 -> A2 -> B2 -> B3
    int32 PatternStep = (LCMStep % 4);
    switch (PatternStep)
    {
    case 0:
        CurrentState.DyadState = TEXT("A");
        CurrentState.TriadState = 1;
        break;
    case 1:
        CurrentState.DyadState = TEXT("A");
        CurrentState.TriadState = 2;
        break;
    case 2:
        CurrentState.DyadState = TEXT("B");
        CurrentState.TriadState = 2;
        break;
    case 3:
        CurrentState.DyadState = TEXT("B");
        CurrentState.TriadState = 3;
        break;
    }

    // Update stream echobeat steps (phased 4 apart)
    for (int32 i = 0; i < CurrentState.StreamStates.Num(); ++i)
    {
        int32 StreamStep = EchobeatStep + (i * 4);
        if (StreamStep > 12) StreamStep -= 12;
        CurrentState.StreamStates[i].EchobeatStep = StreamStep;
    }

    // Get phase for this step
    ECognitiveIntegrationPhase Phase = GetPhaseForStep(EchobeatStep);
    TransitionPhase(Phase);

    // Get 4E mode for this step
    E4ECognitionMode Mode = Get4EModeForStep(EchobeatStep);
    Transition4EMode(Mode);

    // Execute phase-specific processing
    int32 ActionTaken = -1;
    bool bNicheModified = false;

    switch (Phase)
    {
    case ECognitiveIntegrationPhase::Perception:
        // Perception is handled externally via RunPerceptionPhase
        break;

    case ECognitiveIntegrationPhase::Inference:
        RunInferencePhase();
        break;

    case ECognitiveIntegrationPhase::Action:
        ActionTaken = RunActionPhase();
        break;

    case ECognitiveIntegrationPhase::NicheConstruct:
        RunNicheConstructionPhase();
        bNicheModified = true;
        break;

    case ECognitiveIntegrationPhase::Learning:
        RunLearningPhase();
        break;

    case ECognitiveIntegrationPhase::Integration:
        RunIntegrationPhase();
        break;
    }

    // Forward to active inference engine
    if (ActiveInferenceEngine)
    {
        ActiveInferenceEngine->ProcessSys6Step(LCMStep);
    }

    // Forward to niche system
    if (NicheSystem)
    {
        NicheSystem->ProcessSys6Step(LCMStep);
    }

    // Update global state
    float FreeEnergyAfter = ComputeGlobalFreeEnergy();
    CurrentState.GlobalFreeEnergy = FreeEnergyAfter;
    CurrentState.GlobalNicheFitness = ComputeGlobalNicheFitness();

    // Record cycle event
    RecordCycleEvent(Phase, FreeEnergyBefore, FreeEnergyAfter, ActionTaken, bNicheModified);

    LastCycleTime = CurrentTime;
}

ECognitiveIntegrationPhase UActiveInferenceCognitiveIntegration::GetPhaseForStep(int32 EchobeatStep) const
{
    // Map 12 echobeat steps to 6 phases
    // Steps 1,2: Perception
    // Steps 3,4: Inference
    // Steps 5,6: Action
    // Steps 7,8: Niche Construction
    // Steps 9,10: Learning
    // Steps 11,12: Integration

    int32 PhaseIndex = ((EchobeatStep - 1) / 2) % 6;

    switch (PhaseIndex)
    {
    case 0:
        return ECognitiveIntegrationPhase::Perception;
    case 1:
        return ECognitiveIntegrationPhase::Inference;
    case 2:
        return ECognitiveIntegrationPhase::Action;
    case 3:
        return ECognitiveIntegrationPhase::NicheConstruct;
    case 4:
        return ECognitiveIntegrationPhase::Learning;
    case 5:
        return ECognitiveIntegrationPhase::Integration;
    default:
        return ECognitiveIntegrationPhase::Perception;
    }
}

void UActiveInferenceCognitiveIntegration::TransitionPhase(ECognitiveIntegrationPhase NewPhase)
{
    if (NewPhase != CurrentPhase)
    {
        CurrentPhase = NewPhase;
        OnIntegrationPhaseChanged.Broadcast(NewPhase);
    }
}

void UActiveInferenceCognitiveIntegration::Transition4EMode(E4ECognitionMode NewMode)
{
    if (NewMode != Current4EMode)
    {
        Current4EMode = NewMode;
        CurrentState.Active4EMode = NewMode;
        On4EModeChanged.Broadcast(NewMode);
    }
}

void UActiveInferenceCognitiveIntegration::RunPerceptionPhase(const TArray<float>& Observations)
{
    TransitionPhase(ECognitiveIntegrationPhase::Perception);

    // Update beliefs from observations
    if (ActiveInferenceEngine)
    {
        ActiveInferenceEngine->RunPerceptualInference(Observations);
    }

    // Update niche state from observations
    if (NicheSystem)
    {
        NicheSystem->UpdateNicheState(Observations);
    }
}

void UActiveInferenceCognitiveIntegration::RunInferencePhase()
{
    TransitionPhase(ECognitiveIntegrationPhase::Inference);

    // Minimize variational free energy
    if (ActiveInferenceEngine)
    {
        ActiveInferenceEngine->MinimizeVariationalFreeEnergy(16);
    }

    // Evaluate policies
    if (ActiveInferenceEngine)
    {
        ActiveInferenceEngine->EvaluatePolicies();
    }
}

int32 UActiveInferenceCognitiveIntegration::RunActionPhase()
{
    TransitionPhase(ECognitiveIntegrationPhase::Action);

    int32 ActionTaken = -1;

    // Select and execute action via active inference
    if (ActiveInferenceEngine)
    {
        ActionTaken = ActiveInferenceEngine->RunActiveInference();
    }

    return ActionTaken;
}

void UActiveInferenceCognitiveIntegration::RunNicheConstructionPhase()
{
    TransitionPhase(ECognitiveIntegrationPhase::NicheConstruct);

    if (!NicheSystem)
    {
        return;
    }

    // Detect affordances
    NicheSystem->DetectAffordances();

    // Plan niche modification based on current 4E mode
    ENicheConstructionType ConstructionType = ENicheConstructionType::Perturbation;
    ENicheDimension Dimension = ENicheDimension::Spatial;

    switch (Current4EMode)
    {
    case E4ECognitionMode::Embodied:
        ConstructionType = ENicheConstructionType::Perturbation;
        Dimension = ENicheDimension::Spatial;
        break;

    case E4ECognitionMode::Embedded:
        ConstructionType = ENicheConstructionType::Relocation;
        Dimension = ENicheDimension::Informational;
        break;

    case E4ECognitionMode::Enacted:
        ConstructionType = ENicheConstructionType::Inheritance;
        Dimension = ENicheDimension::Social;
        break;

    case E4ECognitionMode::Extended:
        ConstructionType = ENicheConstructionType::Scaffolding;
        Dimension = ENicheDimension::Cognitive;
        break;
    }

    // Plan and execute modification
    FNicheModification Modification = NicheSystem->PlanNicheModification(ConstructionType, Dimension);

    if (NicheSystem->GetCurrentEnergy() >= Modification.EnergyCost)
    {
        NicheSystem->ExecuteNicheModification(Modification);
    }
}

void UActiveInferenceCognitiveIntegration::RunLearningPhase()
{
    TransitionPhase(ECognitiveIntegrationPhase::Learning);

    // Learn from action outcomes
    if (ActiveInferenceEngine)
    {
        TArray<FActionOutcome> History = ActiveInferenceEngine->GetActionHistory();
        if (History.Num() > 0)
        {
            // Get recent experiences for learning
            TArray<FActionOutcome> RecentExperiences;
            int32 StartIdx = FMath::Max(0, History.Num() - 10);
            for (int32 i = StartIdx; i < History.Num(); ++i)
            {
                RecentExperiences.Add(History[i]);
            }

            ActiveInferenceEngine->LearnModelStructure(RecentExperiences);
        }
    }

    // Update predictive models
    if (PredictiveEngine)
    {
        // PredictiveEngine->UpdateModels();
    }
}

void UActiveInferenceCognitiveIntegration::RunIntegrationPhase()
{
    TransitionPhase(ECognitiveIntegrationPhase::Integration);

    // Synchronize streams
    SynchronizeStreams();

    // Balance tensions
    BalanceStreamTensions();

    // Update niche from cognitive state
    UpdateNicheFromCognitiveState();

    // Update 4E metrics
    Update4EMetrics();
}

void UActiveInferenceCognitiveIntegration::SynchronizeStreams()
{
    // Ensure streams maintain proper phase relationships
    for (int32 i = 0; i < CurrentState.StreamStates.Num(); ++i)
    {
        FStreamSyncState& StreamState = CurrentState.StreamStates[i];

        // Streams should be 4 steps apart
        int32 ExpectedStep = CurrentState.EchobeatStep + (i * 4);
        if (ExpectedStep > 12) ExpectedStep -= 12;

        StreamState.EchobeatStep = ExpectedStep;
        StreamState.Mode4E = Get4EModeForStep(ExpectedStep);
        StreamState.Phase = GetPhaseForStep(ExpectedStep);
    }
}

float UActiveInferenceCognitiveIntegration::GetInterStreamCoherence() const
{
    if (CurrentState.StreamStates.Num() < 2)
    {
        return 1.0f;
    }

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 i = 0; i < CurrentState.StreamStates.Num(); ++i)
    {
        for (int32 j = i + 1; j < CurrentState.StreamStates.Num(); ++j)
        {
            TotalCoherence += ComputeStreamCoherence(i, j);
            PairCount++;
        }
    }

    if (PairCount > 0)
    {
        return TotalCoherence / PairCount;
    }

    return 1.0f;
}

float UActiveInferenceCognitiveIntegration::ComputeStreamCoherence(int32 Stream1, int32 Stream2) const
{
    if (Stream1 < 0 || Stream1 >= CurrentState.StreamStates.Num() ||
        Stream2 < 0 || Stream2 >= CurrentState.StreamStates.Num())
    {
        return 0.0f;
    }

    const FStreamSyncState& S1 = CurrentState.StreamStates[Stream1];
    const FStreamSyncState& S2 = CurrentState.StreamStates[Stream2];

    // Coherence based on free energy similarity
    float FEDiff = FMath::Abs(S1.FreeEnergy - S2.FreeEnergy);
    float FECoherence = FMath::Exp(-FEDiff);

    // Coherence based on niche fitness similarity
    float NFDiff = FMath::Abs(S1.NicheFitness - S2.NicheFitness);
    float NFCoherence = FMath::Exp(-NFDiff);

    // Coherence based on proper phase relationship (should be 4 steps apart)
    int32 StepDiff = FMath::Abs(S1.EchobeatStep - S2.EchobeatStep);
    if (StepDiff > 6) StepDiff = 12 - StepDiff;  // Wrap around
    int32 ExpectedDiff = 4 * FMath::Abs(Stream1 - Stream2);
    if (ExpectedDiff > 6) ExpectedDiff = 12 - ExpectedDiff;
    float PhaseCoherence = (StepDiff == ExpectedDiff) ? 1.0f : 0.5f;

    return (FECoherence + NFCoherence + PhaseCoherence) / 3.0f;
}

void UActiveInferenceCognitiveIntegration::BalanceStreamTensions()
{
    if (!TensionalCoupling)
    {
        return;
    }

    // Use tensional coupling to balance streams
    // TensionalCoupling->BalanceTensions();
}

void UActiveInferenceCognitiveIntegration::TriggerActiveInferenceCycle()
{
    if (!ActiveInferenceEngine)
    {
        return;
    }

    // Run full active inference cycle
    ActiveInferenceEngine->MinimizeVariationalFreeEnergy(16);
    ActiveInferenceEngine->EvaluatePolicies();
    ActiveInferenceEngine->SelectPolicy();
}

float UActiveInferenceCognitiveIntegration::GetExpectedFreeEnergy(int32 ActionIndex) const
{
    if (!ActiveInferenceEngine)
    {
        return 0.0f;
    }

    // Get predicted outcome for action
    TArray<float> PredictedOutcome = ActiveInferenceEngine->PredictActionOutcome(ActionIndex);

    // Compute expected free energy (simplified)
    float EFE = 0.0f;
    for (float Val : PredictedOutcome)
    {
        EFE += FMath::Abs(Val);
    }

    return EFE;
}

void UActiveInferenceCognitiveIntegration::UpdateBeliefsFromObservations(const TArray<float>& Observations)
{
    if (ActiveInferenceEngine)
    {
        ActiveInferenceEngine->RunPerceptualInference(Observations);
    }
}

int32 UActiveInferenceCognitiveIntegration::SelectActionViaActiveInference()
{
    if (!ActiveInferenceEngine)
    {
        return -1;
    }

    return ActiveInferenceEngine->RunActiveInference();
}

void UActiveInferenceCognitiveIntegration::TriggerNicheConstruction()
{
    if (!NicheSystem)
    {
        return;
    }

    // Detect affordances
    NicheSystem->DetectAffordances();

    // Get most salient affordance
    TArray<FEcologicalAffordance> Affordances = NicheSystem->GetMostSalientAffordances(1);

    if (Affordances.Num() > 0)
    {
        // Exploit the most salient affordance
        NicheSystem->ExploitAffordance(Affordances[0].AffordanceID);
    }
}

float UActiveInferenceCognitiveIntegration::EvaluateNicheConstructionOpportunity() const
{
    float Opportunity = 0.0f;

    // High free energy suggests need for niche construction
    if (ActiveInferenceEngine)
    {
        float FE = ActiveInferenceEngine->GetVariationalFreeEnergy();
        Opportunity += FE * FreeEnergyWeight;
    }

    // Low niche fitness suggests need for niche construction
    if (NicheSystem)
    {
        float Fitness = NicheSystem->GetFitnessLandscape().CurrentFitness;
        Opportunity += (1.0f - Fitness) * NicheFitnessWeight;
    }

    // Available energy affects opportunity
    if (NicheSystem)
    {
        float EnergyRatio = NicheSystem->GetCurrentEnergy() / NicheSystem->GetEnergyBudget();
        Opportunity *= EnergyRatio;
    }

    return FMath::Clamp(Opportunity, 0.0f, 1.0f);
}

float UActiveInferenceCognitiveIntegration::GetNicheCognitionCoupling() const
{
    if (!NicheSystem)
    {
        return 0.0f;
    }

    return NicheSystem->GetNicheState().CouplingStrength;
}

void UActiveInferenceCognitiveIntegration::UpdateNicheFromCognitiveState()
{
    if (!NicheSystem || !ActiveInferenceEngine)
    {
        return;
    }

    // Get belief states from active inference
    TArray<FBeliefState> Beliefs = ActiveInferenceEngine->GetAllBeliefStates();

    // Convert beliefs to observations for niche
    TArray<float> CognitiveState;
    for (const FBeliefState& Belief : Beliefs)
    {
        for (float Val : Belief.Mean)
        {
            CognitiveState.Add(Val);
        }
    }

    // Update niche with cognitive state
    if (CognitiveState.Num() > 0)
    {
        NicheSystem->UpdateNicheState(CognitiveState);
    }
}

void UActiveInferenceCognitiveIntegration::RecordCycleEvent(ECognitiveIntegrationPhase Phase,
                                                            float FEBefore, float FEAfter,
                                                            int32 Action, bool bNicheModified)
{
    FCognitiveCycleEvent Event;
    Event.EventID = NextEventID++;
    Event.LCMStep = CurrentState.LCMStep;
    Event.EchobeatStep = CurrentState.EchobeatStep;
    Event.Phase = Phase;
    Event.Mode4E = Current4EMode;
    Event.FreeEnergyBefore = FEBefore;
    Event.FreeEnergyAfter = FEAfter;
    Event.ActionTaken = Action;
    Event.bNicheModified = bNicheModified;
    Event.Timestamp = GetWorld()->GetTimeSeconds();

    CycleHistory.Add(Event);

    // Keep history bounded
    if (CycleHistory.Num() > 1000)
    {
        CycleHistory.RemoveAt(0, 100);
    }

    OnCognitiveCycleEvent.Broadcast(Event);
}

float UActiveInferenceCognitiveIntegration::ComputeGlobalFreeEnergy() const
{
    float GlobalFE = 0.0f;

    // Average free energy across streams
    for (const FStreamSyncState& Stream : CurrentState.StreamStates)
    {
        GlobalFE += Stream.FreeEnergy;
    }

    if (CurrentState.StreamStates.Num() > 0)
    {
        GlobalFE /= CurrentState.StreamStates.Num();
    }

    return GlobalFE;
}

float UActiveInferenceCognitiveIntegration::ComputeGlobalNicheFitness() const
{
    float GlobalFitness = 0.0f;

    // Average fitness across streams
    for (const FStreamSyncState& Stream : CurrentState.StreamStates)
    {
        GlobalFitness += Stream.NicheFitness;
    }

    if (CurrentState.StreamStates.Num() > 0)
    {
        GlobalFitness /= CurrentState.StreamStates.Num();
    }

    return GlobalFitness;
}
