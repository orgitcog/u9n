// EchobeatsStreamEngine.cpp
// Implementation of 3-phase concurrent cognitive stream architecture

#include "EchobeatsStreamEngine.h"
#include "Math/UnrealMathUtility.h"

UEchobeatsStreamEngine::UEchobeatsStreamEngine()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEchobeatsStreamEngine::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeEngine();
}

void UEchobeatsStreamEngine::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsPaused || !bEnableAutoCycle)
    {
        return;
    }

    // Update step timer
    StepTimer += DeltaTime;
    float StepDuration = CycleDuration / 12.0f;

    if (StepTimer >= StepDuration)
    {
        StepTimer -= StepDuration;
        AdvanceStep();
    }
}

void UEchobeatsStreamEngine::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Sys6Engine = Owner->FindComponentByClass<USys6OperadEngine>();
        CognitiveBridge = Owner->FindComponentByClass<USys6CognitiveBridge>();

        // Bind to sys6 events
        if (Sys6Engine)
        {
            Sys6Engine->OnStepAdvanced.AddDynamic(this, &UEchobeatsStreamEngine::HandleSys6StepAdvanced);
        }

        if (CognitiveBridge)
        {
            CognitiveBridge->OnBridgeSync.AddDynamic(this, &UEchobeatsStreamEngine::HandleBridgeSync);
        }
    }
}

void UEchobeatsStreamEngine::InitializeEngine()
{
    InitializeStepConfigs();
    InitializeStreamStates();
    InitializeTensionalCouplings();

    State.CurrentStep = 1;
    State.CurrentConfig = StepConfigs[0];
    State.OverallCoherence = 1.0f;
    State.RelevanceLevel = 0.5f;
    State.CycleCount = 0;

    StepTimer = 0.0f;
    bIsPaused = false;
}

void UEchobeatsStreamEngine::InitializeStepConfigs()
{
    StepConfigs.Empty();
    StepConfigs.SetNum(12);

    // Step 1: Pivotal, Expressive, T4E (Sensory Input)
    StepConfigs[0].StepNumber = 1;
    StepConfigs[0].StepType = EEchobeatStepType::Pivotal;
    StepConfigs[0].Mode = ECognitiveMode::Expressive;
    StepConfigs[0].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[0].Term = ESystem4Term::T4_SensoryInput;
    StepConfigs[0].TriadicGroup = 0;
    StepConfigs[0].PhaseID = 0;

    // Step 2: Affordance, Expressive, T1R (Perception)
    StepConfigs[1].StepNumber = 2;
    StepConfigs[1].StepType = EEchobeatStepType::Affordance;
    StepConfigs[1].Mode = ECognitiveMode::Expressive;
    StepConfigs[1].PrimaryStream = ECognitiveStreamType::Affordance;
    StepConfigs[1].Term = ESystem4Term::T1_Perception;
    StepConfigs[1].TriadicGroup = 1;
    StepConfigs[1].PhaseID = 1;

    // Step 3: Affordance, Expressive, T2E (Idea Formation)
    StepConfigs[2].StepNumber = 3;
    StepConfigs[2].StepType = EEchobeatStepType::Affordance;
    StepConfigs[2].Mode = ECognitiveMode::Expressive;
    StepConfigs[2].PrimaryStream = ECognitiveStreamType::Salience;
    StepConfigs[2].Term = ESystem4Term::T2_IdeaFormation;
    StepConfigs[2].TriadicGroup = 2;
    StepConfigs[2].PhaseID = 2;

    // Step 4: Affordance, Expressive, T7R (Memory Encoding) - Triadic Sync
    StepConfigs[3].StepNumber = 4;
    StepConfigs[3].StepType = EEchobeatStepType::Affordance;
    StepConfigs[3].Mode = ECognitiveMode::Expressive;
    StepConfigs[3].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[3].Term = ESystem4Term::T7_MemoryEncoding;
    StepConfigs[3].TriadicGroup = 3;
    StepConfigs[3].PhaseID = 0;

    // Step 5: Affordance, Expressive, T4E (Sensory Input)
    StepConfigs[4].StepNumber = 5;
    StepConfigs[4].StepType = EEchobeatStepType::Affordance;
    StepConfigs[4].Mode = ECognitiveMode::Expressive;
    StepConfigs[4].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[4].Term = ESystem4Term::T4_SensoryInput;
    StepConfigs[4].TriadicGroup = 0;
    StepConfigs[4].PhaseID = 1;

    // Step 6: Affordance, Reflective, T1R (Perception)
    StepConfigs[5].StepNumber = 6;
    StepConfigs[5].StepType = EEchobeatStepType::Affordance;
    StepConfigs[5].Mode = ECognitiveMode::Reflective;
    StepConfigs[5].PrimaryStream = ECognitiveStreamType::Affordance;
    StepConfigs[5].Term = ESystem4Term::T1_Perception;
    StepConfigs[5].TriadicGroup = 1;
    StepConfigs[5].PhaseID = 2;

    // Step 7: Pivotal, Reflective, T2E (Idea Formation)
    StepConfigs[6].StepNumber = 7;
    StepConfigs[6].StepType = EEchobeatStepType::Pivotal;
    StepConfigs[6].Mode = ECognitiveMode::Reflective;
    StepConfigs[6].PrimaryStream = ECognitiveStreamType::Salience;
    StepConfigs[6].Term = ESystem4Term::T2_IdeaFormation;
    StepConfigs[6].TriadicGroup = 2;
    StepConfigs[6].PhaseID = 0;

    // Step 8: Salience, Expressive, T5E (Action Sequence) - Triadic Sync
    StepConfigs[7].StepNumber = 8;
    StepConfigs[7].StepType = EEchobeatStepType::Salience;
    StepConfigs[7].Mode = ECognitiveMode::Expressive;
    StepConfigs[7].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[7].Term = ESystem4Term::T5_ActionSequence;
    StepConfigs[7].TriadicGroup = 3;
    StepConfigs[7].PhaseID = 1;

    // Step 9: Salience, Expressive, T8E (Balanced Response)
    StepConfigs[8].StepNumber = 9;
    StepConfigs[8].StepType = EEchobeatStepType::Salience;
    StepConfigs[8].Mode = ECognitiveMode::Expressive;
    StepConfigs[8].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[8].Term = ESystem4Term::T8_BalancedResponse;
    StepConfigs[8].TriadicGroup = 0;
    StepConfigs[8].PhaseID = 2;

    // Step 10: Salience, Reflective, T7R (Memory Encoding)
    StepConfigs[9].StepNumber = 10;
    StepConfigs[9].StepType = EEchobeatStepType::Salience;
    StepConfigs[9].Mode = ECognitiveMode::Reflective;
    StepConfigs[9].PrimaryStream = ECognitiveStreamType::Affordance;
    StepConfigs[9].Term = ESystem4Term::T7_MemoryEncoding;
    StepConfigs[9].TriadicGroup = 1;
    StepConfigs[9].PhaseID = 0;

    // Step 11: Salience, Reflective, T5E (Action Sequence)
    StepConfigs[10].StepNumber = 11;
    StepConfigs[10].StepType = EEchobeatStepType::Salience;
    StepConfigs[10].Mode = ECognitiveMode::Reflective;
    StepConfigs[10].PrimaryStream = ECognitiveStreamType::Salience;
    StepConfigs[10].Term = ESystem4Term::T5_ActionSequence;
    StepConfigs[10].TriadicGroup = 2;
    StepConfigs[10].PhaseID = 1;

    // Step 12: Salience, Reflective, T8E (Balanced Response) - Triadic Sync
    StepConfigs[11].StepNumber = 12;
    StepConfigs[11].StepType = EEchobeatStepType::Salience;
    StepConfigs[11].Mode = ECognitiveMode::Reflective;
    StepConfigs[11].PrimaryStream = ECognitiveStreamType::Pivotal;
    StepConfigs[11].Term = ESystem4Term::T8_BalancedResponse;
    StepConfigs[11].TriadicGroup = 3;
    StepConfigs[11].PhaseID = 2;
}

void UEchobeatsStreamEngine::InitializeStreamStates()
{
    State.StreamStates.Empty();
    State.StreamStates.SetNum(3);

    // Pivotal Stream - Phase 0 (steps 1, 5, 9)
    State.StreamStates[0].StreamType = ECognitiveStreamType::Pivotal;
    State.StreamStates[0].StreamStep = 0;
    State.StreamStates[0].PhaseOffset = 0;
    State.StreamStates[0].ActivationLevel = 1.0f;
    State.StreamStates[0].StreamCoherence = 1.0f;
    State.StreamStates[0].ProcessingState.SetNum(ReservoirUnitsPerStream);
    State.StreamStates[0].ReservoirState.SetNum(ReservoirUnitsPerStream);

    // Affordance Stream - Phase 1 (steps 2, 6, 10)
    State.StreamStates[1].StreamType = ECognitiveStreamType::Affordance;
    State.StreamStates[1].StreamStep = 0;
    State.StreamStates[1].PhaseOffset = 4;
    State.StreamStates[1].ActivationLevel = 0.0f;
    State.StreamStates[1].StreamCoherence = 1.0f;
    State.StreamStates[1].ProcessingState.SetNum(ReservoirUnitsPerStream);
    State.StreamStates[1].ReservoirState.SetNum(ReservoirUnitsPerStream);

    // Salience Stream - Phase 2 (steps 3, 7, 11)
    State.StreamStates[2].StreamType = ECognitiveStreamType::Salience;
    State.StreamStates[2].StreamStep = 0;
    State.StreamStates[2].PhaseOffset = 8;
    State.StreamStates[2].ActivationLevel = 0.0f;
    State.StreamStates[2].StreamCoherence = 1.0f;
    State.StreamStates[2].ProcessingState.SetNum(ReservoirUnitsPerStream);
    State.StreamStates[2].ReservoirState.SetNum(ReservoirUnitsPerStream);

    // Initialize reservoir states with small random values
    for (FCognitiveStreamState& StreamState : State.StreamStates)
    {
        for (int32 i = 0; i < ReservoirUnitsPerStream; ++i)
        {
            StreamState.ProcessingState[i] = FMath::FRandRange(-0.1f, 0.1f);
            StreamState.ReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
        }
    }
}

void UEchobeatsStreamEngine::InitializeTensionalCouplings()
{
    TensionalCouplings.Empty();

    // T4E ↔ T7R: Perception-Memory Coupling
    FTensionalCoupling PerceptionMemory;
    PerceptionMemory.CouplingName = TEXT("Perception-Memory");
    PerceptionMemory.TermA = ESystem4Term::T4_SensoryInput;
    PerceptionMemory.ModeA = ECognitiveMode::Expressive;
    PerceptionMemory.TermB = ESystem4Term::T7_MemoryEncoding;
    PerceptionMemory.ModeB = ECognitiveMode::Reflective;
    PerceptionMemory.CouplingStrength = 0.8f;
    PerceptionMemory.bIsActive = false;
    TensionalCouplings.Add(PerceptionMemory);

    // T1R ↔ T2E: Assessment-Planning Coupling
    FTensionalCoupling AssessmentPlanning;
    AssessmentPlanning.CouplingName = TEXT("Assessment-Planning");
    AssessmentPlanning.TermA = ESystem4Term::T1_Perception;
    AssessmentPlanning.ModeA = ECognitiveMode::Reflective;
    AssessmentPlanning.TermB = ESystem4Term::T2_IdeaFormation;
    AssessmentPlanning.ModeB = ECognitiveMode::Expressive;
    AssessmentPlanning.CouplingStrength = 0.7f;
    AssessmentPlanning.bIsActive = false;
    TensionalCouplings.Add(AssessmentPlanning);

    // T5E ↔ T8E: Action-Integration Coupling
    FTensionalCoupling ActionIntegration;
    ActionIntegration.CouplingName = TEXT("Action-Integration");
    ActionIntegration.TermA = ESystem4Term::T5_ActionSequence;
    ActionIntegration.ModeA = ECognitiveMode::Expressive;
    ActionIntegration.TermB = ESystem4Term::T8_BalancedResponse;
    ActionIntegration.ModeB = ECognitiveMode::Expressive;
    ActionIntegration.CouplingStrength = 0.9f;
    ActionIntegration.bIsActive = false;
    TensionalCouplings.Add(ActionIntegration);
}

void UEchobeatsStreamEngine::AdvanceStep()
{
    int32 OldStep = State.CurrentStep;

    // Advance step (1-12)
    State.CurrentStep = (State.CurrentStep % 12) + 1;

    // Check for cycle completion
    if (State.CurrentStep == 1 && OldStep == 12)
    {
        State.CycleCount++;
        OnCycleCompleted.Broadcast(State.CycleCount);
    }

    // Update current configuration
    State.CurrentConfig = StepConfigs[State.CurrentStep - 1];

    // Update stream states
    UpdateStreamStates();

    // Detect active couplings
    if (bEnableTensionalCoupling)
    {
        DetectActiveCouplings();
    }

    // Compute overall coherence
    ComputeOverallCoherence();

    // Sync with sys6
    SyncWithSys6();

    // Fire step changed event
    OnStepChanged.Broadcast(OldStep, State.CurrentStep);

    // Check for relevance realization at pivotal steps
    if (State.CurrentConfig.StepType == EEchobeatStepType::Pivotal)
    {
        if (State.RelevanceLevel >= RelevanceThreshold)
        {
            OnRelevanceRealized.Broadcast(State.CurrentStep, State.RelevanceLevel);
        }
    }
}

void UEchobeatsStreamEngine::UpdateStreamStates()
{
    // Triadic groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Streams are phased 4 steps apart (120 degrees)

    int32 Step = State.CurrentStep;

    // Update each stream based on current step
    for (FCognitiveStreamState& StreamState : State.StreamStates)
    {
        // Compute stream step within its 4-step cycle
        int32 EffectiveStep = (Step - 1 + StreamState.PhaseOffset) % 12;
        StreamState.StreamStep = EffectiveStep / 4;

        // Compute activation level based on whether this stream is primary
        ECognitiveStreamType PrimaryStream = State.CurrentConfig.PrimaryStream;
        if (StreamState.StreamType == PrimaryStream)
        {
            StreamState.ActivationLevel = 1.0f;
            OnStreamActivated.Broadcast(StreamState.StreamType, StreamState.ActivationLevel);
        }
        else
        {
            // Decay activation
            StreamState.ActivationLevel *= 0.9f;
        }
    }
}

void UEchobeatsStreamEngine::DetectActiveCouplings()
{
    State.ActiveCouplings.Empty();

    // Get current term and mode
    ESystem4Term CurrentTerm = State.CurrentConfig.Term;
    ECognitiveMode CurrentMode = State.CurrentConfig.Mode;

    // Check each coupling
    for (FTensionalCoupling& Coupling : TensionalCouplings)
    {
        // Check if current step activates this coupling
        bool bTermAMatch = (Coupling.TermA == CurrentTerm && Coupling.ModeA == CurrentMode);
        bool bTermBMatch = (Coupling.TermB == CurrentTerm && Coupling.ModeB == CurrentMode);

        if (bTermAMatch || bTermBMatch)
        {
            Coupling.bIsActive = true;
            State.ActiveCouplings.Add(Coupling);
            OnCouplingActivated.Broadcast(Coupling.CouplingName, Coupling.CouplingStrength);
        }
        else
        {
            Coupling.bIsActive = false;
        }
    }
}

void UEchobeatsStreamEngine::ComputeOverallCoherence()
{
    // Compute coherence from stream states
    float TotalCoherence = 0.0f;
    int32 StreamCount = State.StreamStates.Num();

    for (const FCognitiveStreamState& StreamState : State.StreamStates)
    {
        TotalCoherence += StreamState.StreamCoherence;
    }

    State.OverallCoherence = (StreamCount > 0) ? (TotalCoherence / StreamCount) : 1.0f;

    // Boost coherence at triadic sync points
    if (IsAtTriadicSyncPoint())
    {
        State.OverallCoherence = FMath::Min(1.0f, State.OverallCoherence * 1.1f);
    }
}

void UEchobeatsStreamEngine::ProcessSensoryInput(const TArray<float>& SensoryInput)
{
    // Route input to appropriate stream based on current step
    switch (State.CurrentConfig.PrimaryStream)
    {
    case ECognitiveStreamType::Pivotal:
        ProcessPivotalStream(SensoryInput);
        break;
    case ECognitiveStreamType::Affordance:
        ProcessAffordanceStream(SensoryInput);
        break;
    case ECognitiveStreamType::Salience:
        ProcessSalienceStream(SensoryInput);
        break;
    }

    // Compute relevance realization
    State.RelevanceLevel = ComputeRelevanceRealization(SensoryInput);
}

void UEchobeatsStreamEngine::ProcessPivotalStream(const TArray<float>& Input)
{
    if (State.StreamStates.Num() < 1) return;

    FCognitiveStreamState& PivotalState = State.StreamStates[0];

    // Simple reservoir update (placeholder for full ESN)
    int32 InputSize = FMath::Min(Input.Num(), PivotalState.ProcessingState.Num());
    for (int32 i = 0; i < InputSize; ++i)
    {
        float OldState = PivotalState.ProcessingState[i];
        float NewState = FMath::Tanh(Input[i] + 0.9f * OldState);
        PivotalState.ProcessingState[i] = NewState;
    }

    // Update reservoir state
    for (int32 i = 0; i < PivotalState.ReservoirState.Num(); ++i)
    {
        int32 InputIdx = i % InputSize;
        PivotalState.ReservoirState[i] = FMath::Tanh(
            PivotalState.ProcessingState[InputIdx] * 0.5f +
            PivotalState.ReservoirState[i] * 0.9f
        );
    }
}

void UEchobeatsStreamEngine::ProcessAffordanceStream(const TArray<float>& Input)
{
    if (State.StreamStates.Num() < 2) return;

    FCognitiveStreamState& AffordanceState = State.StreamStates[1];

    // Affordance stream focuses on environmental interaction
    int32 InputSize = FMath::Min(Input.Num(), AffordanceState.ProcessingState.Num());
    for (int32 i = 0; i < InputSize; ++i)
    {
        float OldState = AffordanceState.ProcessingState[i];
        float NewState = FMath::Tanh(Input[i] * 1.2f + 0.85f * OldState);
        AffordanceState.ProcessingState[i] = NewState;
    }

    // Update reservoir state with affordance-specific dynamics
    for (int32 i = 0; i < AffordanceState.ReservoirState.Num(); ++i)
    {
        int32 InputIdx = i % InputSize;
        AffordanceState.ReservoirState[i] = FMath::Tanh(
            AffordanceState.ProcessingState[InputIdx] * 0.6f +
            AffordanceState.ReservoirState[i] * 0.85f
        );
    }
}

void UEchobeatsStreamEngine::ProcessSalienceStream(const TArray<float>& Input)
{
    if (State.StreamStates.Num() < 3) return;

    FCognitiveStreamState& SalienceState = State.StreamStates[2];

    // Salience stream focuses on virtual simulation
    int32 InputSize = FMath::Min(Input.Num(), SalienceState.ProcessingState.Num());
    for (int32 i = 0; i < InputSize; ++i)
    {
        float OldState = SalienceState.ProcessingState[i];
        float NewState = FMath::Tanh(Input[i] * 0.8f + 0.95f * OldState);
        SalienceState.ProcessingState[i] = NewState;
    }

    // Update reservoir state with salience-specific dynamics
    for (int32 i = 0; i < SalienceState.ReservoirState.Num(); ++i)
    {
        int32 InputIdx = i % InputSize;
        SalienceState.ReservoirState[i] = FMath::Tanh(
            SalienceState.ProcessingState[InputIdx] * 0.4f +
            SalienceState.ReservoirState[i] * 0.95f
        );
    }
}

float UEchobeatsStreamEngine::ComputeRelevanceRealization(const TArray<float>& Input)
{
    // Compute relevance based on input salience and stream coherence
    float InputMagnitude = 0.0f;
    for (float Val : Input)
    {
        InputMagnitude += FMath::Abs(Val);
    }
    InputMagnitude = (Input.Num() > 0) ? (InputMagnitude / Input.Num()) : 0.0f;

    // Combine with stream coherence
    float Relevance = InputMagnitude * State.OverallCoherence;

    // Boost at pivotal steps
    if (State.CurrentConfig.StepType == EEchobeatStepType::Pivotal)
    {
        Relevance *= 1.2f;
    }

    return FMath::Clamp(Relevance, 0.0f, 1.0f);
}

void UEchobeatsStreamEngine::ProcessTensionalCoupling(const FTensionalCoupling& Coupling)
{
    // Process coupling between streams
    // This would involve cross-stream information transfer
}

void UEchobeatsStreamEngine::SyncWithSys6()
{
    if (!CognitiveBridge) return;

    // Get bridge state
    FSys6BridgeState BridgeState = CognitiveBridge->GetBridgeState();

    // Sync cognitive step
    if (BridgeState.CognitiveStep != State.CurrentStep)
    {
        // Allow bridge to influence step timing
        // This creates bidirectional synchronization
    }
}

// State query implementations
FEchobeatsFullState UEchobeatsStreamEngine::GetFullState() const
{
    return State;
}

int32 UEchobeatsStreamEngine::GetCurrentStep() const
{
    return State.CurrentStep;
}

FEchobeatStepConfig UEchobeatsStreamEngine::GetCurrentStepConfig() const
{
    return State.CurrentConfig;
}

FCognitiveStreamState UEchobeatsStreamEngine::GetStreamState(ECognitiveStreamType StreamType) const
{
    for (const FCognitiveStreamState& StreamState : State.StreamStates)
    {
        if (StreamState.StreamType == StreamType)
        {
            return StreamState;
        }
    }
    return FCognitiveStreamState();
}

TArray<FTensionalCoupling> UEchobeatsStreamEngine::GetActiveCouplings() const
{
    return State.ActiveCouplings;
}

TArray<float> UEchobeatsStreamEngine::GetStreamOutput(ECognitiveStreamType StreamType) const
{
    for (const FCognitiveStreamState& StreamState : State.StreamStates)
    {
        if (StreamState.StreamType == StreamType)
        {
            return StreamState.ReservoirState;
        }
    }
    return TArray<float>();
}

int32 UEchobeatsStreamEngine::GetTriadicGroup(int32 Step) const
{
    // Triadic groups: {1,5,9}=0, {2,6,10}=1, {3,7,11}=2, {4,8,12}=3
    return ((Step - 1) % 4);
}

bool UEchobeatsStreamEngine::IsAtTriadicSyncPoint() const
{
    // Sync points at steps 4, 8, 12
    return (State.CurrentStep == 4 || State.CurrentStep == 8 || State.CurrentStep == 12);
}

float UEchobeatsStreamEngine::GetOverallCoherence() const
{
    return State.OverallCoherence;
}

void UEchobeatsStreamEngine::JumpToStep(int32 Step)
{
    if (Step < 1 || Step > 12) return;

    int32 OldStep = State.CurrentStep;
    State.CurrentStep = Step;
    State.CurrentConfig = StepConfigs[Step - 1];
    UpdateStreamStates();
    OnStepChanged.Broadcast(OldStep, State.CurrentStep);
}

void UEchobeatsStreamEngine::PauseCycle()
{
    bIsPaused = true;
}

void UEchobeatsStreamEngine::ResumeCycle()
{
    bIsPaused = false;
}

void UEchobeatsStreamEngine::ResetCycle()
{
    InitializeEngine();
}

void UEchobeatsStreamEngine::HandleSys6StepAdvanced(int32 OldStep, int32 NewStep)
{
    // Sync with sys6 step changes
    SyncWithSys6();
}

void UEchobeatsStreamEngine::HandleBridgeSync(int32 Sys6Step, int32 CognitiveStep)
{
    // Handle bridge synchronization events
    if (CognitiveStep != State.CurrentStep)
    {
        // Consider adjusting step based on bridge
    }
}
