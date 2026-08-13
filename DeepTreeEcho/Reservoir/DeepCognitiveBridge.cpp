#include "DeepCognitiveBridge.h"

UDeepCognitiveBridge::UDeepCognitiveBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz

    // Initialize step duration based on cycle
    StepDuration = CycleDuration / 12.0f;
}

void UDeepCognitiveBridge::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    InitializeCognitiveBridge();
}

void UDeepCognitiveBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableCognitiveBridge)
        return;

    // Update cycle timer
    CycleTimer += DeltaTime;

    // Check for step advancement
    if (CycleTimer >= StepDuration)
    {
        CycleTimer -= StepDuration;
        AdvanceStep();
    }

    // Continuous processing
    if (bEnable4ECognition)
    {
        // Check for 4E dimension change
        E4ECognitionDimension CurrentDominant = GetDominant4EDimension();
        if (CurrentDominant != PreviousDominantDimension)
        {
            On4EDimensionChange.Broadcast(CurrentDominant);
            PreviousDominantDimension = CurrentDominant;
        }
    }
}

void UDeepCognitiveBridge::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        BaseReservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
        CognitiveIntegration = Owner->FindComponentByClass<UReservoirCognitiveIntegration>();
    }
}

void UDeepCognitiveBridge::InitializeCognitiveBridge()
{
    // Initialize step duration
    StepDuration = CycleDuration / 12.0f;

    // Initialize concurrent streams
    InitializeConcurrentStreams();

    // Initialize nested shells if enabled
    if (bEnableNestedShells)
    {
        InitializeNestedShells();
    }

    // Initialize 4E cognition state
    CognitionState4E.EmbodiedState.SetNum(100);
    CognitionState4E.EmbeddedState.SetNum(100);
    CognitionState4E.EnactedState.SetNum(100);
    CognitionState4E.ExtendedState.SetNum(100);

    // Initialize step history
    StepHistory.Empty();
}

void UDeepCognitiveBridge::InitializeConcurrentStreams()
{
    ConcurrentStreams.Empty();

    // Stream 1: Pivotal (phase offset 0)
    FConcurrentStreamState PivotalStream;
    PivotalStream.StreamType = ECognitiveStreamType::Pivotal;
    PivotalStream.CurrentPhase = 1;
    PivotalStream.PhaseOffset = 0;
    PivotalStream.ReservoirState.Units = 100;
    PivotalStream.ReservoirState.SpectralRadius = 0.9f;
    PivotalStream.ReservoirState.LeakRate = 0.3f;
    PivotalStream.AttentionVector.SetNum(100);
    ConcurrentStreams.Add(PivotalStream);

    // Stream 2: Affordance (phase offset 4 - 120 degrees)
    FConcurrentStreamState AffordanceStream;
    AffordanceStream.StreamType = ECognitiveStreamType::Affordance;
    AffordanceStream.CurrentPhase = 5; // 1 + 4
    AffordanceStream.PhaseOffset = 4;
    AffordanceStream.ReservoirState.Units = 100;
    AffordanceStream.ReservoirState.SpectralRadius = 0.85f;
    AffordanceStream.ReservoirState.LeakRate = 0.35f;
    AffordanceStream.AttentionVector.SetNum(100);
    ConcurrentStreams.Add(AffordanceStream);

    // Stream 3: Salience (phase offset 8 - 240 degrees)
    FConcurrentStreamState SalienceStream;
    SalienceStream.StreamType = ECognitiveStreamType::Salience;
    SalienceStream.CurrentPhase = 9; // 1 + 8
    SalienceStream.PhaseOffset = 8;
    SalienceStream.ReservoirState.Units = 100;
    SalienceStream.ReservoirState.SpectralRadius = 0.95f;
    SalienceStream.ReservoirState.LeakRate = 0.25f;
    SalienceStream.AttentionVector.SetNum(100);
    ConcurrentStreams.Add(SalienceStream);
}

void UDeepCognitiveBridge::InitializeNestedShells()
{
    NestedShells.Empty();

    // OEIS A000081: 1->1, 2->2, 3->4, 4->9 terms
    const int32 TermCounts[] = {1, 2, 4, 9};
    const int32 StepsBetween[] = {1, 2, 3, 4};

    for (int32 Level = 0; Level < 4; Level++)
    {
        FNestedShellLevel Shell;
        Shell.Level = Level + 1;
        Shell.TermCount = TermCounts[Level];
        Shell.StepsBetween = StepsBetween[Level];

        // Initialize reservoirs for each term
        for (int32 Term = 0; Term < Shell.TermCount; Term++)
        {
            FReservoirState TermReservoir;
            TermReservoir.ReservoirID = FString::Printf(TEXT("Shell_%d_Term_%d"), Level + 1, Term + 1);
            TermReservoir.Units = 50 + (Level * 25); // Larger reservoirs at deeper levels
            TermReservoir.SpectralRadius = 0.9f - (Level * 0.05f);
            TermReservoir.LeakRate = 0.3f + (Level * 0.05f);
            TermReservoir.ActivationState.SetNum(TermReservoir.Units);
            Shell.TermReservoirs.Add(TermReservoir);
        }

        // Initialize coupling strengths
        Shell.CouplingStrengths.SetNum(Shell.TermCount * Shell.TermCount);
        for (int32 i = 0; i < Shell.CouplingStrengths.Num(); i++)
        {
            Shell.CouplingStrengths[i] = 0.1f;
        }

        NestedShells.Add(Shell);
    }
}

void UDeepCognitiveBridge::AdvanceStep()
{
    // Store current step in history
    StoreStepInHistory();

    // Advance step
    CurrentStep = (CurrentStep % 12) + 1;

    // Update all concurrent streams
    for (FConcurrentStreamState& Stream : ConcurrentStreams)
    {
        Stream.CurrentPhase = ((CurrentStep - 1 + Stream.PhaseOffset) % 12) + 1;
    }

    // Check for triadic sync point
    if (IsTriadicSyncPoint())
    {
        SynchronizeStreamsAtTriad();
        OnTriadicSync.Broadcast(GetTriadicStepGroup());
    }

    // Propagate through nested shells
    if (bEnableNestedShells)
    {
        PropagateNestedShells();
    }

    // Broadcast step advanced event
    OnStepAdvanced.Broadcast(CurrentStep);
}

void UDeepCognitiveBridge::ProcessCurrentStep(const TArray<float>& SensoryInput)
{
    // Update step state
    UpdateStepState(SensoryInput);

    // Update 4E cognition if enabled
    if (bEnable4ECognition)
    {
        Update4ECognitionState(SensoryInput);
    }

    // Couple streams through reservoir
    CoupleStreamsThroughReservoir();

    // Check for relevance realization
    if (CheckRelevanceRealizationEvent())
    {
        OnRelevanceRealization.Broadcast(CurrentStepState.RelevanceScore);
    }
}

TArray<int32> UDeepCognitiveBridge::GetTriadicStepGroup() const
{
    TArray<int32> Group;

    // Triadic groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    int32 GroupIndex = ((CurrentStep - 1) % 4);
    Group.Add(GroupIndex + 1);
    Group.Add(GroupIndex + 5);
    Group.Add(GroupIndex + 9);

    return Group;
}

bool UDeepCognitiveBridge::IsTriadicSyncPoint() const
{
    // Sync points at steps 4, 8, 12 (every 4 steps)
    return (CurrentStep % 4) == 0;
}

void UDeepCognitiveBridge::SynchronizeStreamsAtTriad()
{
    if (ConcurrentStreams.Num() < 3)
        return;

    // Compute average attention across streams
    TArray<float> AverageAttention;
    int32 AttentionSize = ConcurrentStreams[0].AttentionVector.Num();
    AverageAttention.SetNum(AttentionSize);

    for (int32 i = 0; i < AttentionSize; i++)
    {
        float Sum = 0.0f;
        for (const FConcurrentStreamState& Stream : ConcurrentStreams)
        {
            if (Stream.AttentionVector.IsValidIndex(i))
            {
                Sum += Stream.AttentionVector[i];
            }
        }
        AverageAttention[i] = Sum / 3.0f;
    }

    // Apply synchronized attention with coupling
    for (FConcurrentStreamState& Stream : ConcurrentStreams)
    {
        for (int32 i = 0; i < AttentionSize && i < Stream.AttentionVector.Num(); i++)
        {
            Stream.AttentionVector[i] = FMath::Lerp(
                Stream.AttentionVector[i],
                AverageAttention[i],
                InterStreamCoupling
            );
        }

        // Update coherence
        Stream.InterStreamCoherence = ComputeInterStreamCoherence();
    }
}

void UDeepCognitiveBridge::UpdateStream(ECognitiveStreamType StreamType, const TArray<float>& Input)
{
    for (FConcurrentStreamState& Stream : ConcurrentStreams)
    {
        if (Stream.StreamType == StreamType)
        {
            // Update reservoir state
            if (BaseReservoir)
            {
                TArray<float> Output = BaseReservoir->ProcessInput(Input, static_cast<int32>(StreamType) + 1);
                Stream.ReservoirState.ActivationState = Output;
            }

            // Update attention vector
            Stream.AttentionVector = ComputeStreamAttention(StreamType);

            // Store relevance in history
            float Relevance = ComputeRelevanceRealization(Input);
            Stream.RelevanceHistory.Add(Relevance);
            if (Stream.RelevanceHistory.Num() > 100)
            {
                Stream.RelevanceHistory.RemoveAt(0);
            }

            break;
        }
    }
}

FConcurrentStreamState UDeepCognitiveBridge::GetStreamState(ECognitiveStreamType StreamType) const
{
    for (const FConcurrentStreamState& Stream : ConcurrentStreams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream;
        }
    }
    return FConcurrentStreamState();
}

float UDeepCognitiveBridge::ComputeInterStreamCoherence() const
{
    if (ConcurrentStreams.Num() < 2)
        return 1.0f;

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    // Compute pairwise coherence
    for (int32 i = 0; i < ConcurrentStreams.Num(); i++)
    {
        for (int32 j = i + 1; j < ConcurrentStreams.Num(); j++)
        {
            const TArray<float>& A = ConcurrentStreams[i].AttentionVector;
            const TArray<float>& B = ConcurrentStreams[j].AttentionVector;

            // Compute cosine similarity
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

int32 UDeepCognitiveBridge::GetStreamPhaseForStep(ECognitiveStreamType StreamType, int32 Step) const
{
    int32 Offset = 0;
    switch (StreamType)
    {
        case ECognitiveStreamType::Pivotal:
            Offset = 0;
            break;
        case ECognitiveStreamType::Affordance:
            Offset = 4;
            break;
        case ECognitiveStreamType::Salience:
            Offset = 8;
            break;
    }
    return ((Step - 1 + Offset) % 12) + 1;
}

void UDeepCognitiveBridge::Update4ECognitionState(const TArray<float>& SensoryInput)
{
    if (!bEnable4ECognition)
        return;

    // Map reservoir states to 4E dimensions
    MapReservoirTo4EDimensions();

    // Compute dimensional coherence
    CognitionState4E.DimensionalCoherence = ComputeDimensionalCoherence();

    // Determine dominant dimension
    CognitionState4E.DominantDimension = GetDominant4EDimension();
}

TArray<float> UDeepCognitiveBridge::Get4EDimensionState(E4ECognitionDimension Dimension) const
{
    switch (Dimension)
    {
        case E4ECognitionDimension::Embodied:
            return CognitionState4E.EmbodiedState;
        case E4ECognitionDimension::Embedded:
            return CognitionState4E.EmbeddedState;
        case E4ECognitionDimension::Enacted:
            return CognitionState4E.EnactedState;
        case E4ECognitionDimension::Extended:
            return CognitionState4E.ExtendedState;
        default:
            return TArray<float>();
    }
}

float UDeepCognitiveBridge::ComputeDimensionalCoherence() const
{
    // Compute average pairwise coherence across 4E dimensions
    TArray<const TArray<float>*> Dimensions = {
        &CognitionState4E.EmbodiedState,
        &CognitionState4E.EmbeddedState,
        &CognitionState4E.EnactedState,
        &CognitionState4E.ExtendedState
    };

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 i = 0; i < 4; i++)
    {
        for (int32 j = i + 1; j < 4; j++)
        {
            const TArray<float>& A = *Dimensions[i];
            const TArray<float>& B = *Dimensions[j];

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

E4ECognitionDimension UDeepCognitiveBridge::GetDominant4EDimension() const
{
    // Compute activation magnitude for each dimension
    float EmbodiedMag = 0.0f, EmbeddedMag = 0.0f, EnactedMag = 0.0f, ExtendedMag = 0.0f;

    for (float Val : CognitionState4E.EmbodiedState) EmbodiedMag += Val * Val;
    for (float Val : CognitionState4E.EmbeddedState) EmbeddedMag += Val * Val;
    for (float Val : CognitionState4E.EnactedState) EnactedMag += Val * Val;
    for (float Val : CognitionState4E.ExtendedState) ExtendedMag += Val * Val;

    float MaxMag = FMath::Max(FMath::Max(EmbodiedMag, EmbeddedMag), FMath::Max(EnactedMag, ExtendedMag));

    if (MaxMag == EmbodiedMag) return E4ECognitionDimension::Embodied;
    if (MaxMag == EmbeddedMag) return E4ECognitionDimension::Embedded;
    if (MaxMag == EnactedMag) return E4ECognitionDimension::Enacted;
    return E4ECognitionDimension::Extended;
}

void UDeepCognitiveBridge::UpdateNestedShell(int32 Level, const TArray<float>& Input)
{
    if (Level < 1 || Level > NestedShells.Num())
        return;

    FNestedShellLevel& Shell = NestedShells[Level - 1];

    // Update each term's reservoir
    for (int32 Term = 0; Term < Shell.TermReservoirs.Num(); Term++)
    {
        FReservoirState& Reservoir = Shell.TermReservoirs[Term];

        // Simple leaky integration update
        for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < Input.Num(); i++)
        {
            Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
                + Reservoir.LeakRate * FMath::Tanh(Input[i] * Reservoir.InputScaling);
        }

        Reservoir.bIsInitialized = true;
        Reservoir.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    }
}

FNestedShellLevel UDeepCognitiveBridge::GetNestedShellState(int32 Level) const
{
    if (Level >= 1 && Level <= NestedShells.Num())
    {
        return NestedShells[Level - 1];
    }
    return FNestedShellLevel();
}

void UDeepCognitiveBridge::PropagateNestedShells()
{
    // Propagate from outer to inner shells
    for (int32 Level = 0; Level < NestedShells.Num() - 1; Level++)
    {
        ComputeShellCoupling(Level, Level + 1);
    }

    // Propagate from inner to outer shells (feedback)
    for (int32 Level = NestedShells.Num() - 1; Level > 0; Level--)
    {
        ComputeShellCoupling(Level, Level - 1);
    }
}

int32 UDeepCognitiveBridge::GetTermCountForLevel(int32 Level) const
{
    // OEIS A000081: 1, 1, 2, 4, 9, 20, 48, 115, ...
    // For levels 1-4: 1, 2, 4, 9
    const int32 TermCounts[] = {1, 2, 4, 9};
    if (Level >= 1 && Level <= 4)
    {
        return TermCounts[Level - 1];
    }
    return 1;
}

float UDeepCognitiveBridge::ComputeRelevanceRealization(const TArray<float>& Context)
{
    if (Context.Num() == 0)
        return 0.0f;

    // Compute relevance as deviation from expected pattern
    float Relevance = 0.0f;

    // Use salience stream for relevance computation
    if (ConcurrentStreams.Num() >= 3)
    {
        const TArray<float>& SalienceAttention = ConcurrentStreams[2].AttentionVector;

        float DotProduct = 0.0f;
        float NormContext = 0.0f;
        float NormSalience = 0.0f;

        int32 MinSize = FMath::Min(Context.Num(), SalienceAttention.Num());
        for (int32 i = 0; i < MinSize; i++)
        {
            DotProduct += Context[i] * SalienceAttention[i];
            NormContext += Context[i] * Context[i];
            NormSalience += SalienceAttention[i] * SalienceAttention[i];
        }

        if (NormContext > 0.0f && NormSalience > 0.0f)
        {
            // Relevance is high when context aligns with salience
            Relevance = DotProduct / (FMath::Sqrt(NormContext) * FMath::Sqrt(NormSalience));
        }
    }

    return FMath::Clamp(Relevance, 0.0f, 1.0f);
}

bool UDeepCognitiveBridge::CheckRelevanceRealizationEvent() const
{
    return CurrentStepState.RelevanceScore >= RelevanceThreshold;
}

TArray<float> UDeepCognitiveBridge::GetRelevanceHistory() const
{
    TArray<float> History;
    for (const FEchobeatStepState& Step : StepHistory)
    {
        History.Add(Step.RelevanceScore);
    }
    return History;
}

bool UDeepCognitiveBridge::ComputeStepType(int32 Step) const
{
    // 7 expressive steps: 1, 2, 3, 4, 5, 8, 9
    // 5 reflective steps: 6, 7, 10, 11, 12
    const bool ExpressiveSteps[] = {true, true, true, true, true, false, false, true, true, false, false, false};
    return ExpressiveSteps[(Step - 1) % 12];
}

ECognitiveStreamType UDeepCognitiveBridge::GetActiveStreamForStep(int32 Step) const
{
    // Determine which stream is "primary" at this step
    // Based on triadic grouping: {1,5,9}=Pivotal, {2,6,10}=Affordance, {3,7,11}=Salience, {4,8,12}=Sync
    int32 GroupIndex = (Step - 1) % 4;
    switch (GroupIndex)
    {
        case 0: return ECognitiveStreamType::Pivotal;
        case 1: return ECognitiveStreamType::Affordance;
        case 2: return ECognitiveStreamType::Salience;
        case 3: return ECognitiveStreamType::Pivotal; // Sync point defaults to Pivotal
        default: return ECognitiveStreamType::Pivotal;
    }
}

void UDeepCognitiveBridge::UpdateStepState(const TArray<float>& SensoryInput)
{
    CurrentStepState.StepNumber = CurrentStep;
    CurrentStepState.ActiveStream = GetActiveStreamForStep(CurrentStep);
    CurrentStepState.bIsExpressive = ComputeStepType(CurrentStep);
    CurrentStepState.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Update reservoir activation
    if (BaseReservoir)
    {
        CurrentStepState.ReservoirActivation = BaseReservoir->ProcessInput(SensoryInput, CurrentStep);
    }

    // Compute relevance
    CurrentStepState.RelevanceScore = ComputeRelevanceRealization(SensoryInput);

    // Update salience landscape from cognitive integration
    if (CognitiveIntegration)
    {
        CurrentStepState.SalienceLandscape = CognitiveIntegration->GetCognitiveState().SalienceMap;
        CurrentStepState.AffordanceMap = CognitiveIntegration->GetCognitiveState().AffordanceMap;
    }
}

void UDeepCognitiveBridge::CoupleStreamsThroughReservoir()
{
    if (ConcurrentStreams.Num() < 3 || !BaseReservoir)
        return;

    // Each stream influences the others through reservoir coupling
    for (int32 i = 0; i < ConcurrentStreams.Num(); i++)
    {
        FConcurrentStreamState& SourceStream = ConcurrentStreams[i];

        for (int32 j = 0; j < ConcurrentStreams.Num(); j++)
        {
            if (i == j) continue;

            FConcurrentStreamState& TargetStream = ConcurrentStreams[j];

            // Apply coupling influence
            for (int32 k = 0; k < TargetStream.AttentionVector.Num() && k < SourceStream.AttentionVector.Num(); k++)
            {
                TargetStream.AttentionVector[k] += InterStreamCoupling * SourceStream.AttentionVector[k] * 0.1f;
            }
        }
    }
}

void UDeepCognitiveBridge::MapReservoirTo4EDimensions()
{
    // Map different aspects of reservoir state to 4E dimensions
    if (ConcurrentStreams.Num() < 3)
        return;

    // Embodied: Pivotal stream (present body state)
    const TArray<float>& PivotalState = ConcurrentStreams[0].ReservoirState.ActivationState;
    for (int32 i = 0; i < CognitionState4E.EmbodiedState.Num() && i < PivotalState.Num(); i++)
    {
        CognitionState4E.EmbodiedState[i] = PivotalState[i];
    }

    // Embedded: Affordance stream (environmental context)
    const TArray<float>& AffordanceState = ConcurrentStreams[1].ReservoirState.ActivationState;
    for (int32 i = 0; i < CognitionState4E.EmbeddedState.Num() && i < AffordanceState.Num(); i++)
    {
        CognitionState4E.EmbeddedState[i] = AffordanceState[i];
    }

    // Enacted: Combination of Pivotal and Affordance (action potential)
    for (int32 i = 0; i < CognitionState4E.EnactedState.Num(); i++)
    {
        float Pivotal = i < PivotalState.Num() ? PivotalState[i] : 0.0f;
        float Affordance = i < AffordanceState.Num() ? AffordanceState[i] : 0.0f;
        CognitionState4E.EnactedState[i] = (Pivotal + Affordance) * 0.5f;
    }

    // Extended: Salience stream (tool/artifact integration)
    const TArray<float>& SalienceState = ConcurrentStreams[2].ReservoirState.ActivationState;
    for (int32 i = 0; i < CognitionState4E.ExtendedState.Num() && i < SalienceState.Num(); i++)
    {
        CognitionState4E.ExtendedState[i] = SalienceState[i];
    }
}

void UDeepCognitiveBridge::ComputeShellCoupling(int32 SourceLevel, int32 TargetLevel)
{
    if (SourceLevel < 0 || SourceLevel >= NestedShells.Num() ||
        TargetLevel < 0 || TargetLevel >= NestedShells.Num())
        return;

    FNestedShellLevel& Source = NestedShells[SourceLevel];
    FNestedShellLevel& Target = NestedShells[TargetLevel];

    // Compute average activation from source
    TArray<float> SourceAverage;
    int32 MaxUnits = 0;
    for (const FReservoirState& Reservoir : Source.TermReservoirs)
    {
        MaxUnits = FMath::Max(MaxUnits, Reservoir.ActivationState.Num());
    }
    SourceAverage.SetNum(MaxUnits);

    for (const FReservoirState& Reservoir : Source.TermReservoirs)
    {
        for (int32 i = 0; i < Reservoir.ActivationState.Num(); i++)
        {
            SourceAverage[i] += Reservoir.ActivationState[i] / Source.TermReservoirs.Num();
        }
    }

    // Apply coupling to target
    float CouplingStrength = Source.InterLevelCoupling;
    for (FReservoirState& Reservoir : Target.TermReservoirs)
    {
        for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < SourceAverage.Num(); i++)
        {
            Reservoir.ActivationState[i] += CouplingStrength * SourceAverage[i] * 0.1f;
        }
    }
}

void UDeepCognitiveBridge::StoreStepInHistory()
{
    StepHistory.Add(CurrentStepState);

    // Keep history bounded
    if (StepHistory.Num() > 120) // 10 full cycles
    {
        StepHistory.RemoveAt(0);
    }
}

TArray<float> UDeepCognitiveBridge::ComputeStreamAttention(ECognitiveStreamType StreamType)
{
    TArray<float> Attention;

    // Get base attention from cognitive integration
    if (CognitiveIntegration)
    {
        FReservoirCognitiveState CogState = CognitiveIntegration->GetCognitiveState();

        // Convert attention allocation to array
        for (const auto& Pair : CogState.AttentionAllocation)
        {
            Attention.Add(Pair.Value);
        }
    }

    // Ensure minimum size
    while (Attention.Num() < 100)
    {
        Attention.Add(0.0f);
    }

    return Attention;
}
