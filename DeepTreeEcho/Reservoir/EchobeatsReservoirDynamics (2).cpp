#include "EchobeatsReservoirDynamics.h"

UEchobeatsReservoirDynamics::UEchobeatsReservoirDynamics()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz

    StepDuration = CycleDuration / 12.0f;
}

void UEchobeatsReservoirDynamics::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    InitializeEchobeatsDynamics();
}

void UEchobeatsReservoirDynamics::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableEchobeatsDynamics)
        return;

    // Update cycle timer
    CycleTimer += DeltaTime;

    // Check for step advancement
    if (CycleTimer >= StepDuration)
    {
        CycleTimer -= StepDuration;
        AdvanceStep();
    }

    // Update stream synchronization
    UpdateStreamSynchronization();

    // Check for mode change
    CheckModeChange();
}

void UEchobeatsReservoirDynamics::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CognitiveBridge = Owner->FindComponentByClass<UDeepCognitiveBridge>();
        TetradicIntegration = Owner->FindComponentByClass<UTetradicReservoirIntegration>();
        Embodied4E = Owner->FindComponentByClass<UEmbodied4ECognition>();
        BaseReservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

void UEchobeatsReservoirDynamics::InitializeEchobeatsDynamics()
{
    StepDuration = CycleDuration / 12.0f;

    InitializeStepConfigurations();
    InitializeStreamReservoirs();

    if (bEnableNestedShells)
    {
        InitializeNestedShells();
    }

    // Initialize sync state
    SyncState.CoherenceMatrix.SetNum(9); // 3x3
    SyncState.PhaseAlignmentScores.SetNum(3);

    // Initialize relevance history
    RelevanceHistory.Empty();
}

void UEchobeatsReservoirDynamics::InitializeStepConfigurations()
{
    StepConfigs.Empty();

    // 12-step configuration based on echobeats architecture
    // 7 expressive steps: 1, 2, 3, 4, 5, 8, 9
    // 5 reflective steps: 6, 7, 10, 11, 12
    // Pivotal steps: 1, 7
    // Affordance steps: 2, 3, 4, 5, 6
    // Salience steps: 8, 9, 10, 11, 12

    for (int32 Step = 1; Step <= 12; Step++)
    {
        FEchobeatStepConfig Config;
        Config.StepNumber = Step;

        // Determine step type
        if (Step == 1 || Step == 7)
        {
            Config.StepType = EEchobeatStepType::Pivotal;
        }
        else if (Step >= 2 && Step <= 6)
        {
            Config.StepType = EEchobeatStepType::Affordance;
        }
        else
        {
            Config.StepType = EEchobeatStepType::Salience;
        }

        // Determine mode (7 expressive, 5 reflective)
        if (Step <= 5 || Step == 8 || Step == 9)
        {
            Config.Mode = EEchobeatMode::Expressive;
        }
        else
        {
            Config.Mode = EEchobeatMode::Reflective;
        }

        // Determine primary stream based on triadic grouping
        // {1,5,9} -> Pivotal, {2,6,10} -> Affordance, {3,7,11} -> Salience, {4,8,12} -> Sync
        int32 GroupIndex = (Step - 1) % 4;
        switch (GroupIndex)
        {
            case 0: Config.PrimaryStream = ECognitiveStreamType::Pivotal; break;
            case 1: Config.PrimaryStream = ECognitiveStreamType::Affordance; break;
            case 2: Config.PrimaryStream = ECognitiveStreamType::Salience; break;
            case 3: Config.PrimaryStream = ECognitiveStreamType::Pivotal; break; // Sync defaults to Pivotal
        }

        // Triadic group
        Config.TriadicGroup = GroupIndex;

        // Active shell level (cycles through 1-4)
        Config.ActiveShellLevel = ((Step - 1) / 3) % 4 + 1;

        // Reservoir coupling varies by step type
        switch (Config.StepType)
        {
            case EEchobeatStepType::Pivotal:
                Config.ReservoirCoupling = 0.5f; // Higher coupling at pivotal points
                break;
            case EEchobeatStepType::Affordance:
                Config.ReservoirCoupling = 0.3f;
                break;
            case EEchobeatStepType::Salience:
                Config.ReservoirCoupling = 0.4f;
                break;
        }

        StepConfigs.Add(Config);
    }
}

void UEchobeatsReservoirDynamics::InitializeStreamReservoirs()
{
    StreamReservoirs.Empty();

    // Initialize 3 stream reservoirs
    for (int32 i = 0; i < 3; i++)
    {
        FReservoirState Reservoir;

        switch (i)
        {
            case 0: // Pivotal
                Reservoir.ReservoirID = TEXT("Stream_Pivotal");
                Reservoir.SpectralRadius = 0.9f;
                Reservoir.LeakRate = 0.3f;
                break;
            case 1: // Affordance
                Reservoir.ReservoirID = TEXT("Stream_Affordance");
                Reservoir.SpectralRadius = 0.85f;
                Reservoir.LeakRate = 0.35f;
                break;
            case 2: // Salience
                Reservoir.ReservoirID = TEXT("Stream_Salience");
                Reservoir.SpectralRadius = 0.95f;
                Reservoir.LeakRate = 0.25f;
                break;
        }

        Reservoir.Units = UnitsPerStream;
        Reservoir.ActivationState.SetNum(UnitsPerStream);

        StreamReservoirs.Add(Reservoir);
    }
}

void UEchobeatsReservoirDynamics::InitializeNestedShells()
{
    NestedShellTerms.Empty();
    BuildNestedShellTree();
}

void UEchobeatsReservoirDynamics::BuildNestedShellTree()
{
    // OEIS A000081: 1->1, 2->2, 3->4, 4->9 terms
    // Total: 1 + 2 + 4 + 9 = 16 terms

    int32 TermID = 0;

    // Level 1: 1 term (root)
    {
        FNestedShellTerm Term;
        Term.TermID = TermID++;
        Term.ShellLevel = 1;
        Term.ParentTermID = -1;
        Term.ReservoirState.ReservoirID = FString::Printf(TEXT("Shell_1_Term_0"));
        Term.ReservoirState.Units = UnitsPerStream / 4;
        Term.ReservoirState.SpectralRadius = 0.9f;
        Term.ReservoirState.LeakRate = 0.3f;
        Term.ReservoirState.ActivationState.SetNum(Term.ReservoirState.Units);
        NestedShellTerms.Add(Term);
    }

    // Level 2: 2 terms
    int32 Level2Start = TermID;
    for (int32 i = 0; i < 2; i++)
    {
        FNestedShellTerm Term;
        Term.TermID = TermID++;
        Term.ShellLevel = 2;
        Term.ParentTermID = 0; // Parent is root
        Term.ReservoirState.ReservoirID = FString::Printf(TEXT("Shell_2_Term_%d"), i);
        Term.ReservoirState.Units = UnitsPerStream / 4;
        Term.ReservoirState.SpectralRadius = 0.85f;
        Term.ReservoirState.LeakRate = 0.35f;
        Term.ReservoirState.ActivationState.SetNum(Term.ReservoirState.Units);
        NestedShellTerms.Add(Term);

        // Add as child of root
        NestedShellTerms[0].ChildTermIDs.Add(Term.TermID);
    }

    // Level 3: 4 terms
    int32 Level3Start = TermID;
    for (int32 i = 0; i < 4; i++)
    {
        FNestedShellTerm Term;
        Term.TermID = TermID++;
        Term.ShellLevel = 3;
        Term.ParentTermID = Level2Start + (i / 2); // 2 children per level 2 term
        Term.ReservoirState.ReservoirID = FString::Printf(TEXT("Shell_3_Term_%d"), i);
        Term.ReservoirState.Units = UnitsPerStream / 4;
        Term.ReservoirState.SpectralRadius = 0.8f;
        Term.ReservoirState.LeakRate = 0.4f;
        Term.ReservoirState.ActivationState.SetNum(Term.ReservoirState.Units);
        NestedShellTerms.Add(Term);

        // Add as child of parent
        NestedShellTerms[Term.ParentTermID].ChildTermIDs.Add(Term.TermID);
    }

    // Level 4: 9 terms
    for (int32 i = 0; i < 9; i++)
    {
        FNestedShellTerm Term;
        Term.TermID = TermID++;
        Term.ShellLevel = 4;
        // Distribute among level 3 terms: 2, 2, 2, 3
        int32 ParentOffset = i < 6 ? (i / 2) : (i - 6 + 3);
        Term.ParentTermID = Level3Start + FMath::Min(ParentOffset, 3);
        Term.ReservoirState.ReservoirID = FString::Printf(TEXT("Shell_4_Term_%d"), i);
        Term.ReservoirState.Units = UnitsPerStream / 4;
        Term.ReservoirState.SpectralRadius = 0.75f;
        Term.ReservoirState.LeakRate = 0.45f;
        Term.ReservoirState.ActivationState.SetNum(Term.ReservoirState.Units);
        NestedShellTerms.Add(Term);

        // Add as child of parent
        if (Term.ParentTermID < NestedShellTerms.Num())
        {
            NestedShellTerms[Term.ParentTermID].ChildTermIDs.Add(Term.TermID);
        }
    }
}

void UEchobeatsReservoirDynamics::AdvanceStep()
{
    // Advance step
    CurrentStep = (CurrentStep % 12) + 1;

    // Get current config
    FEchobeatStepConfig Config = GetCurrentStepConfig();

    // Check for triadic sync point
    if (bEnableTriadicSync && IsTriadicSyncPoint())
    {
        SynchronizeAtTriad();
        OnTriadicSyncPoint.Broadcast(Config.TriadicGroup, SyncState.SyncQuality);
    }

    // Propagate nested shells
    if (bEnableNestedShells)
    {
        PropagateNestedShells();
    }

    // Broadcast step advanced
    OnStepAdvanced.Broadcast(CurrentStep, Config);
}

void UEchobeatsReservoirDynamics::ProcessStep(const TArray<float>& SensoryInput)
{
    FEchobeatStepConfig Config = GetCurrentStepConfig();

    // Process based on step type
    switch (Config.StepType)
    {
        case EEchobeatStepType::Pivotal:
            ProcessPivotalStep(SensoryInput);
            break;
        case EEchobeatStepType::Affordance:
            ProcessAffordanceStep(SensoryInput);
            break;
        case EEchobeatStepType::Salience:
            ProcessSalienceStep(SensoryInput);
            break;
    }

    // Update primary stream reservoir
    UpdateStreamReservoir(Config.PrimaryStream, SensoryInput);

    // Integrate with other systems
    IntegrateWithTetradic();
    IntegrateWith4E();

    // Check for relevance event at pivotal steps
    if (Config.StepType == EEchobeatStepType::Pivotal)
    {
        float Relevance = ComputeRelevanceRealization(SensoryInput);
        StoreRelevanceInHistory(Relevance);

        if (Relevance >= RelevanceThreshold)
        {
            OnRelevanceRealized.Broadcast(CurrentStep, Relevance);
        }
    }
}

FEchobeatStepConfig UEchobeatsReservoirDynamics::GetCurrentStepConfig() const
{
    if (CurrentStep >= 1 && CurrentStep <= StepConfigs.Num())
    {
        return StepConfigs[CurrentStep - 1];
    }
    return FEchobeatStepConfig();
}

EEchobeatStepType UEchobeatsReservoirDynamics::GetStepType(int32 Step) const
{
    if (Step == 1 || Step == 7)
        return EEchobeatStepType::Pivotal;
    if (Step >= 2 && Step <= 6)
        return EEchobeatStepType::Affordance;
    return EEchobeatStepType::Salience;
}

EEchobeatMode UEchobeatsReservoirDynamics::GetStepMode(int32 Step) const
{
    // 7 expressive: 1, 2, 3, 4, 5, 8, 9
    // 5 reflective: 6, 7, 10, 11, 12
    if (Step <= 5 || Step == 8 || Step == 9)
        return EEchobeatMode::Expressive;
    return EEchobeatMode::Reflective;
}

bool UEchobeatsReservoirDynamics::IsPivotalStep(int32 Step) const
{
    return Step == 1 || Step == 7;
}

void UEchobeatsReservoirDynamics::UpdateStreamReservoir(ECognitiveStreamType Stream, const TArray<float>& Input)
{
    int32 StreamIndex = static_cast<int32>(Stream);
    if (StreamIndex < 0 || StreamIndex >= StreamReservoirs.Num())
        return;

    FReservoirState& Reservoir = StreamReservoirs[StreamIndex];

    // Leaky integration update
    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < Input.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(Input[i] * Reservoir.InputScaling);
    }

    Reservoir.bIsInitialized = true;
    Reservoir.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

FReservoirState UEchobeatsReservoirDynamics::GetStreamReservoirState(ECognitiveStreamType Stream) const
{
    int32 StreamIndex = static_cast<int32>(Stream);
    if (StreamIndex >= 0 && StreamIndex < StreamReservoirs.Num())
    {
        return StreamReservoirs[StreamIndex];
    }
    return FReservoirState();
}

int32 UEchobeatsReservoirDynamics::GetStreamPhase(ECognitiveStreamType Stream) const
{
    // Streams are phased 4 steps apart
    int32 Offset = 0;
    switch (Stream)
    {
        case ECognitiveStreamType::Pivotal: Offset = 0; break;
        case ECognitiveStreamType::Affordance: Offset = 4; break;
        case ECognitiveStreamType::Salience: Offset = 8; break;
    }
    return ((CurrentStep - 1 + Offset) % 12) + 1;
}

void UEchobeatsReservoirDynamics::CoupleStreamsAtTriad()
{
    if (StreamReservoirs.Num() < 3)
        return;

    // Compute average state
    TArray<float> AverageState;
    AverageState.SetNum(UnitsPerStream);

    for (const FReservoirState& Reservoir : StreamReservoirs)
    {
        for (int32 i = 0; i < AverageState.Num() && i < Reservoir.ActivationState.Num(); i++)
        {
            AverageState[i] += Reservoir.ActivationState[i] / 3.0f;
        }
    }

    // Apply coupling
    for (FReservoirState& Reservoir : StreamReservoirs)
    {
        for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < AverageState.Num(); i++)
        {
            Reservoir.ActivationState[i] = FMath::Lerp(
                Reservoir.ActivationState[i],
                AverageState[i],
                InterStreamCoupling
            );
        }
    }
}

float UEchobeatsReservoirDynamics::ComputeInterStreamCoherence() const
{
    if (StreamReservoirs.Num() < 2)
        return 1.0f;

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 i = 0; i < StreamReservoirs.Num(); i++)
    {
        for (int32 j = i + 1; j < StreamReservoirs.Num(); j++)
        {
            const TArray<float>& A = StreamReservoirs[i].ActivationState;
            const TArray<float>& B = StreamReservoirs[j].ActivationState;

            float DotProduct = 0.0f;
            float NormA = 0.0f;
            float NormB = 0.0f;

            int32 MinSize = FMath::Min(A.Num(), B.Num());
            for (int32 k = 0; k < MinSize; k++)
            {
                DotProduct += A[k] * B[k];
                NormA += A[k] * A[k];
                NormB += B[k] * B[k];
            }

            if (NormA > 0.0f && NormB > 0.0f)
            {
                TotalCoherence += DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
            }
            PairCount++;
        }
    }

    return PairCount > 0 ? TotalCoherence / PairCount : 0.0f;
}

void UEchobeatsReservoirDynamics::UpdateNestedShellTerm(int32 TermID, const TArray<float>& Input)
{
    if (TermID < 0 || TermID >= NestedShellTerms.Num())
        return;

    FNestedShellTerm& Term = NestedShellTerms[TermID];
    FReservoirState& Reservoir = Term.ReservoirState;

    // Leaky integration
    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < Input.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(Input[i]);
    }

    // Update activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    Term.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();
}

FNestedShellTerm UEchobeatsReservoirDynamics::GetNestedShellTerm(int32 TermID) const
{
    if (TermID >= 0 && TermID < NestedShellTerms.Num())
    {
        return NestedShellTerms[TermID];
    }
    return FNestedShellTerm();
}

TArray<FNestedShellTerm> UEchobeatsReservoirDynamics::GetTermsAtLevel(int32 Level) const
{
    TArray<FNestedShellTerm> Terms;
    for (const FNestedShellTerm& Term : NestedShellTerms)
    {
        if (Term.ShellLevel == Level)
        {
            Terms.Add(Term);
        }
    }
    return Terms;
}

void UEchobeatsReservoirDynamics::PropagateNestedShells()
{
    // Propagate from outer to inner
    for (int32 Level = 1; Level <= 4; Level++)
    {
        PropagateShellLevel(Level);
    }

    // Propagate from inner to outer (feedback)
    for (int32 Level = 4; Level >= 1; Level--)
    {
        PropagateShellLevel(Level);
    }
}

int32 UEchobeatsReservoirDynamics::GetTermCountForLevel(int32 Level) const
{
    // OEIS A000081: 1, 2, 4, 9
    const int32 Counts[] = {1, 2, 4, 9};
    if (Level >= 1 && Level <= 4)
    {
        return Counts[Level - 1];
    }
    return 0;
}

int32 UEchobeatsReservoirDynamics::GetTriadicGroup(int32 Step) const
{
    // {1,5,9}->0, {2,6,10}->1, {3,7,11}->2, {4,8,12}->3
    return (Step - 1) % 4;
}

TArray<int32> UEchobeatsReservoirDynamics::GetStepsInTriadicGroup(int32 Group) const
{
    TArray<int32> Steps;
    Steps.Add(Group + 1);
    Steps.Add(Group + 5);
    Steps.Add(Group + 9);
    return Steps;
}

bool UEchobeatsReservoirDynamics::IsTriadicSyncPoint() const
{
    // Sync at steps 4, 8, 12
    return (CurrentStep % 4) == 0;
}

void UEchobeatsReservoirDynamics::SynchronizeAtTriad()
{
    // Couple streams
    CoupleStreamsAtTriad();

    // Update sync state
    SyncState.SyncPhase = GetTriadicGroup(CurrentStep);
    SyncState.SyncQuality = ComputeInterStreamCoherence();
    SyncState.LastSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Update coherence matrix
    for (int32 i = 0; i < 3; i++)
    {
        for (int32 j = 0; j < 3; j++)
        {
            if (i == j)
            {
                SyncState.CoherenceMatrix[i * 3 + j] = 1.0f;
            }
            else if (i < StreamReservoirs.Num() && j < StreamReservoirs.Num())
            {
                const TArray<float>& A = StreamReservoirs[i].ActivationState;
                const TArray<float>& B = StreamReservoirs[j].ActivationState;

                float DotProduct = 0.0f;
                float NormA = 0.0f;
                float NormB = 0.0f;

                int32 MinSize = FMath::Min(A.Num(), B.Num());
                for (int32 k = 0; k < MinSize; k++)
                {
                    DotProduct += A[k] * B[k];
                    NormA += A[k] * A[k];
                    NormB += B[k] * B[k];
                }

                float Coherence = 0.0f;
                if (NormA > 0.0f && NormB > 0.0f)
                {
                    Coherence = DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
                }
                SyncState.CoherenceMatrix[i * 3 + j] = Coherence;
            }
        }
    }
}

float UEchobeatsReservoirDynamics::GetTriadicSyncQuality() const
{
    return SyncState.SyncQuality;
}

float UEchobeatsReservoirDynamics::ComputeRelevanceRealization(const TArray<float>& Context) const
{
    if (Context.Num() == 0 || StreamReservoirs.Num() < 3)
        return 0.0f;

    // Relevance is computed as alignment between context and salience stream
    const TArray<float>& SalienceState = StreamReservoirs[2].ActivationState;

    float DotProduct = 0.0f;
    float NormContext = 0.0f;
    float NormSalience = 0.0f;

    int32 MinSize = FMath::Min(Context.Num(), SalienceState.Num());
    for (int32 i = 0; i < MinSize; i++)
    {
        DotProduct += Context[i] * SalienceState[i];
        NormContext += Context[i] * Context[i];
        NormSalience += SalienceState[i] * SalienceState[i];
    }

    float Relevance = 0.0f;
    if (NormContext > 0.0f && NormSalience > 0.0f)
    {
        Relevance = DotProduct / (FMath::Sqrt(NormContext) * FMath::Sqrt(NormSalience));
    }

    return FMath::Clamp(Relevance, 0.0f, 1.0f);
}

bool UEchobeatsReservoirDynamics::CheckRelevanceEvent() const
{
    if (RelevanceHistory.Num() == 0)
        return false;

    return RelevanceHistory.Last() >= RelevanceThreshold;
}

TArray<float> UEchobeatsReservoirDynamics::GetRelevanceHistory() const
{
    return RelevanceHistory;
}

void UEchobeatsReservoirDynamics::IntegrateWithTetradic()
{
    if (!TetradicIntegration)
        return;

    // Map streams to tetradic vertices
    // Pivotal -> Perception
    // Affordance -> Action
    // Salience -> Cognition

    if (StreamReservoirs.Num() >= 3)
    {
        TetradicIntegration->UpdateVertex(ETetradicVertex::Perception, StreamReservoirs[0].ActivationState);
        TetradicIntegration->UpdateVertex(ETetradicVertex::Action, StreamReservoirs[1].ActivationState);
        TetradicIntegration->UpdateVertex(ETetradicVertex::Cognition, StreamReservoirs[2].ActivationState);
    }
}

void UEchobeatsReservoirDynamics::IntegrateWith4E()
{
    if (!Embodied4E)
        return;

    // Get 4E expression hints and use them to modulate stream processing
    TMap<FString, float> Hints = Embodied4E->Map4EToExpressionHints();

    // Modulate stream coupling based on 4E coherence
    float* Coherence = Hints.Find(TEXT("4ECoherence"));
    if (Coherence)
    {
        // Higher 4E coherence = stronger inter-stream coupling
        for (FReservoirState& Reservoir : StreamReservoirs)
        {
            // Apply coherence-based modulation
            for (float& Val : Reservoir.ActivationState)
            {
                Val *= (0.5f + 0.5f * (*Coherence));
            }
        }
    }
}

TArray<float> UEchobeatsReservoirDynamics::GetIntegratedCognitiveState() const
{
    TArray<float> IntegratedState;
    IntegratedState.SetNum(UnitsPerStream);

    // Combine all stream states
    for (const FReservoirState& Reservoir : StreamReservoirs)
    {
        for (int32 i = 0; i < IntegratedState.Num() && i < Reservoir.ActivationState.Num(); i++)
        {
            IntegratedState[i] += Reservoir.ActivationState[i] / StreamReservoirs.Num();
        }
    }

    return IntegratedState;
}

void UEchobeatsReservoirDynamics::ProcessPivotalStep(const TArray<float>& Input)
{
    // Pivotal steps: Relevance realization
    // High coupling, synchronize all streams

    // Update all streams with input
    for (int32 i = 0; i < StreamReservoirs.Num(); i++)
    {
        UpdateStreamReservoir(static_cast<ECognitiveStreamType>(i), Input);
    }

    // Synchronize streams
    CoupleStreamsAtTriad();
}

void UEchobeatsReservoirDynamics::ProcessAffordanceStep(const TArray<float>& Input)
{
    // Affordance steps: Actual interaction with environment
    // Focus on affordance stream

    UpdateStreamReservoir(ECognitiveStreamType::Affordance, Input);

    // Light coupling to other streams
    if (StreamReservoirs.Num() >= 3)
    {
        for (int32 i = 0; i < StreamReservoirs[1].ActivationState.Num(); i++)
        {
            float AffordanceVal = StreamReservoirs[1].ActivationState[i];

            // Influence pivotal stream
            if (i < StreamReservoirs[0].ActivationState.Num())
            {
                StreamReservoirs[0].ActivationState[i] += 0.1f * AffordanceVal;
            }

            // Influence salience stream
            if (i < StreamReservoirs[2].ActivationState.Num())
            {
                StreamReservoirs[2].ActivationState[i] += 0.1f * AffordanceVal;
            }
        }
    }
}

void UEchobeatsReservoirDynamics::ProcessSalienceStep(const TArray<float>& Input)
{
    // Salience steps: Virtual simulation
    // Focus on salience stream

    UpdateStreamReservoir(ECognitiveStreamType::Salience, Input);

    // Light coupling to other streams
    if (StreamReservoirs.Num() >= 3)
    {
        for (int32 i = 0; i < StreamReservoirs[2].ActivationState.Num(); i++)
        {
            float SalienceVal = StreamReservoirs[2].ActivationState[i];

            // Influence pivotal stream
            if (i < StreamReservoirs[0].ActivationState.Num())
            {
                StreamReservoirs[0].ActivationState[i] += 0.1f * SalienceVal;
            }

            // Influence affordance stream
            if (i < StreamReservoirs[1].ActivationState.Num())
            {
                StreamReservoirs[1].ActivationState[i] += 0.1f * SalienceVal;
            }
        }
    }
}

void UEchobeatsReservoirDynamics::UpdateStreamSynchronization()
{
    // Update phase alignment scores
    for (int32 i = 0; i < 3 && i < SyncState.PhaseAlignmentScores.Num(); i++)
    {
        int32 StreamPhase = GetStreamPhase(static_cast<ECognitiveStreamType>(i));
        // Phase alignment is how close the stream is to its expected phase
        float ExpectedPhase = ((i * 4) % 12) + 1;
        float PhaseDiff = FMath::Abs(StreamPhase - ExpectedPhase);
        SyncState.PhaseAlignmentScores[i] = 1.0f - (PhaseDiff / 12.0f);
    }
}

void UEchobeatsReservoirDynamics::PropagateShellLevel(int32 Level)
{
    TArray<FNestedShellTerm> Terms = GetTermsAtLevel(Level);

    for (FNestedShellTerm& Term : Terms)
    {
        // Get parent state if exists
        TArray<float> ParentInfluence;
        if (Term.ParentTermID >= 0 && Term.ParentTermID < NestedShellTerms.Num())
        {
            ParentInfluence = NestedShellTerms[Term.ParentTermID].ReservoirState.ActivationState;
        }

        // Get child average if exists
        TArray<float> ChildAverage;
        ChildAverage.SetNum(Term.ReservoirState.Units);
        int32 ChildCount = 0;

        for (int32 ChildID : Term.ChildTermIDs)
        {
            if (ChildID >= 0 && ChildID < NestedShellTerms.Num())
            {
                const TArray<float>& ChildState = NestedShellTerms[ChildID].ReservoirState.ActivationState;
                for (int32 i = 0; i < ChildAverage.Num() && i < ChildState.Num(); i++)
                {
                    ChildAverage[i] += ChildState[i];
                }
                ChildCount++;
            }
        }

        if (ChildCount > 0)
        {
            for (float& Val : ChildAverage)
            {
                Val /= ChildCount;
            }
        }

        // Update term with combined influence
        int32 TermIndex = -1;
        for (int32 i = 0; i < NestedShellTerms.Num(); i++)
        {
            if (NestedShellTerms[i].TermID == Term.TermID)
            {
                TermIndex = i;
                break;
            }
        }

        if (TermIndex >= 0)
        {
            FReservoirState& Reservoir = NestedShellTerms[TermIndex].ReservoirState;

            for (int32 i = 0; i < Reservoir.ActivationState.Num(); i++)
            {
                float ParentVal = i < ParentInfluence.Num() ? ParentInfluence[i] : 0.0f;
                float ChildVal = i < ChildAverage.Num() ? ChildAverage[i] : 0.0f;

                // Combine parent and child influence
                float Influence = 0.3f * ParentVal + 0.2f * ChildVal;
                Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
                    + Reservoir.LeakRate * FMath::Tanh(Influence);
            }
        }
    }
}

void UEchobeatsReservoirDynamics::CheckModeChange()
{
    EEchobeatMode CurrentMode = GetStepMode(CurrentStep);
    if (CurrentMode != PreviousMode)
    {
        OnModeChanged.Broadcast(PreviousMode, CurrentMode);
        PreviousMode = CurrentMode;
    }
}

void UEchobeatsReservoirDynamics::StoreRelevanceInHistory(float Relevance)
{
    RelevanceHistory.Add(Relevance);

    // Keep history bounded
    if (RelevanceHistory.Num() > 120) // 10 full cycles
    {
        RelevanceHistory.RemoveAt(0);
    }
}
