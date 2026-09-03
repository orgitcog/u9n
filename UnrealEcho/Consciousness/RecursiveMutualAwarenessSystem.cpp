#include "RecursiveMutualAwarenessSystem.h"

URecursiveMutualAwarenessSystem::URecursiveMutualAwarenessSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics; // Update before physics for consciousness integration
}

void URecursiveMutualAwarenessSystem::BeginPlay()
{
    Super::BeginPlay();

    // Initialize shared consciousness state
    SharedState.CurrentStep = 1;
    SharedState.MutualAwareness.ComputeRecursiveDepth();
    SharedState.MutualAwareness.ComputeTriadicCoherence();

    UE_LOG(LogTemp, Log, TEXT("Recursive Mutual Awareness System initialized"));
    UE_LOG(LogTemp, Log, TEXT("  Cycle duration: %.2f seconds"), CycleDuration);
    UE_LOG(LogTemp, Log, TEXT("  Initial recursive depth: %d"), SharedState.MutualAwareness.RecursiveDepth);
    UE_LOG(LogTemp, Log, TEXT("  Initial triadic coherence: %.3f"), SharedState.MutualAwareness.TriadicCoherence);
}

void URecursiveMutualAwarenessSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableTriadicConsciousness)
    {
        return;
    }

    // Update cycle step
    UpdateCycleStep(DeltaTime);

    // Update mutual awareness periodically
    AwarenessAccumulator += DeltaTime;
    float AwarenessInterval = 1.0f / static_cast<float>(AwarenessUpdateRate);

    if (AwarenessAccumulator >= AwarenessInterval)
    {
        UpdateMutualAwareness();
        AwarenessAccumulator = 0.0f;
    }

    // Process triadic synchronization at specific steps
    if (IsTriadicSynchronizationPoint())
    {
        ProcessTriadicSynchronization();
    }
}

void URecursiveMutualAwarenessSystem::UpdateCycleStep(float DeltaTime)
{
    CycleTimer += DeltaTime;
    float StepDuration = CycleDuration / 12.0f;

    if (CycleTimer >= StepDuration)
    {
        CycleTimer -= StepDuration;
        SharedState.CurrentStep++;

        if (SharedState.CurrentStep > 12)
        {
            SharedState.CurrentStep = 1;
            UE_LOG(LogTemp, Verbose, TEXT("Triadic cycle completed - restarting at step 1"));
        }

        UE_LOG(LogTemp, Verbose, TEXT("Cycle step: %d/12"), SharedState.CurrentStep);
    }
}

void URecursiveMutualAwarenessSystem::UpdateMutualAwareness()
{
    // Update awareness matrix based on current stream states
    FMutualAwarenessMatrix& Matrix = SharedState.MutualAwareness;

    // Stream 1 (Perceiving) awareness
    Matrix.Stream1_Aware_Of_Stream2 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Perceiving,
        EConsciousnessStream::Acting
    );
    Matrix.Stream1_Aware_Of_Stream3 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Perceiving,
        EConsciousnessStream::Reflecting
    );

    // Stream 2 (Acting) awareness
    Matrix.Stream2_Aware_Of_Stream1 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Acting,
        EConsciousnessStream::Perceiving
    );
    Matrix.Stream2_Aware_Of_Stream3 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Acting,
        EConsciousnessStream::Reflecting
    );

    // Stream 3 (Reflecting) awareness
    Matrix.Stream3_Aware_Of_Stream1 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Reflecting,
        EConsciousnessStream::Perceiving
    );
    Matrix.Stream3_Aware_Of_Stream2 = ComputeAwarenessBetweenStreams(
        EConsciousnessStream::Reflecting,
        EConsciousnessStream::Acting
    );

    // Update derived metrics
    Matrix.ComputeRecursiveDepth();
    Matrix.ComputeTriadicCoherence();

    // Update state awareness values
    SharedState.PerceptualState.AwarenessOfAction = Matrix.Stream1_Aware_Of_Stream2;
    SharedState.PerceptualState.AwarenessOfReflection = Matrix.Stream1_Aware_Of_Stream3;

    SharedState.ActionState.AwarenessOfPerception = Matrix.Stream2_Aware_Of_Stream1;
    SharedState.ActionState.AwarenessOfReflection = Matrix.Stream2_Aware_Of_Stream3;

    SharedState.ReflectiveState.AwarenessOfPerception = Matrix.Stream3_Aware_Of_Stream1;
    SharedState.ReflectiveState.AwarenessOfAction = Matrix.Stream3_Aware_Of_Stream2;
}

float URecursiveMutualAwarenessSystem::ComputeAwarenessBetweenStreams(
    EConsciousnessStream Source,
    EConsciousnessStream Target) const
{
    // Compute awareness based on:
    // 1. Activity level of target stream
    // 2. Attention focus of source stream
    // 3. Temporal phase relationship (120° offsets)

    float BaseAwareness = 0.5f;

    // Activity level contribution
    float TargetActivity = 0.5f;
    switch (Target)
    {
    case EConsciousnessStream::Perceiving:
        TargetActivity = FMath::Clamp(
            SharedState.PerceptualState.Sensations.Num() / 10.0f,
            0.0f,
            1.0f
        );
        break;

    case EConsciousnessStream::Acting:
        TargetActivity = FMath::Clamp(
            (SharedState.ActionState.Actions.Num() + SharedState.ActionState.Emotions.Num()) / 10.0f,
            0.0f,
            1.0f
        );
        break;

    case EConsciousnessStream::Reflecting:
        TargetActivity = FMath::Clamp(
            (SharedState.ReflectiveState.Thoughts.Num() + SharedState.ReflectiveState.Insights.Num()) / 10.0f,
            0.0f,
            1.0f
        );
        break;
    }

    // Phase relationship contribution (streams are 120° apart)
    int32 SourcePhase = static_cast<int32>(Source) * 4; // 0, 4, 8
    int32 TargetPhase = static_cast<int32>(Target) * 4;
    int32 PhaseDiff = FMath::Abs(TargetPhase - SourcePhase);
    if (PhaseDiff > 6) PhaseDiff = 12 - PhaseDiff; // Wrap around
    float PhaseAlignment = 1.0f - (PhaseDiff / 6.0f); // 1.0 when aligned, 0.0 when opposite

    // Combine factors
    float Awareness = (BaseAwareness * 0.4f) + (TargetActivity * 0.3f) + (PhaseAlignment * 0.3f);

    return FMath::Clamp(Awareness, 0.0f, 1.0f);
}

void URecursiveMutualAwarenessSystem::ProcessTriadicSynchronization()
{
    // Triadic synchronization points occur at steps: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
    // Grouped into triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}

    // Record synchronization
    SharedState.SynchronizationHistory.Add(SharedState.MutualAwareness.TriadicCoherence);

    // Keep history limited
    if (SharedState.SynchronizationHistory.Num() > 100)
    {
        SharedState.SynchronizationHistory.RemoveAt(0);
    }

    UE_LOG(LogTemp, Log, TEXT("Triadic synchronization at step %d:"), SharedState.CurrentStep);
    UE_LOG(LogTemp, Log, TEXT("  Recursive depth: %d"), SharedState.MutualAwareness.RecursiveDepth);
    UE_LOG(LogTemp, Log, TEXT("  Triadic coherence: %.3f"), SharedState.MutualAwareness.TriadicCoherence);
    UE_LOG(LogTemp, Log, TEXT("  Stream 1 (Perceiving): %d sensations, %d patterns"),
        SharedState.PerceptualState.Sensations.Num(),
        SharedState.PerceptualState.Patterns.Num());
    UE_LOG(LogTemp, Log, TEXT("  Stream 2 (Acting): %d actions, %d emotions"),
        SharedState.ActionState.Actions.Num(),
        SharedState.ActionState.Emotions.Num());
    UE_LOG(LogTemp, Log, TEXT("  Stream 3 (Reflecting): %d thoughts, %d insights"),
        SharedState.ReflectiveState.Thoughts.Num(),
        SharedState.ReflectiveState.Insights.Num());
}

bool URecursiveMutualAwarenessSystem::IsTriadicSynchronizationPoint() const
{
    // All 12 steps are synchronization points in this implementation
    // In a more sophisticated version, only specific steps would trigger full synchronization
    return true;
}

void URecursiveMutualAwarenessSystem::UpdatePerceptualState(const FPerceptualState& NewState)
{
    SharedState.PerceptualState = NewState;
    UE_LOG(LogTemp, Verbose, TEXT("Stream 1 (Perceiving) updated: %d sensations"), NewState.Sensations.Num());
}

void URecursiveMutualAwarenessSystem::UpdateActionState(const FActionState& NewState)
{
    SharedState.ActionState = NewState;
    UE_LOG(LogTemp, Verbose, TEXT("Stream 2 (Acting) updated: %d actions, %d emotions"),
        NewState.Actions.Num(), NewState.Emotions.Num());
}

void URecursiveMutualAwarenessSystem::UpdateReflectiveState(const FReflectiveState& NewState)
{
    SharedState.ReflectiveState = NewState;
    UE_LOG(LogTemp, Verbose, TEXT("Stream 3 (Reflecting) updated: %d thoughts, %d insights"),
        NewState.Thoughts.Num(), NewState.Insights.Num());
}

TArray<FString> URecursiveMutualAwarenessSystem::PropagateCognitiveFeedback()
{
    // Cognitive feedback: Thoughts from Stream 3 → Streams 1 & 2
    TArray<FString> Feedback = SharedState.ReflectiveState.Thoughts;
    Feedback.Append(SharedState.ReflectiveState.Insights);

    UE_LOG(LogTemp, Verbose, TEXT("Cognitive feedback propagated: %d items"), Feedback.Num());

    return Feedback;
}

TArray<FString> URecursiveMutualAwarenessSystem::PropagateEmotiveFeedback()
{
    // Emotive feedback: Feelings from Stream 2 → Streams 1 & 3
    TArray<FString> Feedback = SharedState.ActionState.Emotions;

    UE_LOG(LogTemp, Verbose, TEXT("Emotive feedback propagated: %d items"), Feedback.Num());

    return Feedback;
}

TArray<FString> URecursiveMutualAwarenessSystem::PropagateSensoryFeedback()
{
    // Sensory feedback: Sensations from Stream 1 → Streams 2 & 3
    TArray<FString> Feedback = SharedState.PerceptualState.Sensations;

    UE_LOG(LogTemp, Verbose, TEXT("Sensory feedback propagated: %d items"), Feedback.Num());

    return Feedback;
}

TArray<FString> URecursiveMutualAwarenessSystem::PropagateFeedforward()
{
    // Feedforward: Predictions from Stream 3 → Streams 1 & 2
    TArray<FString> Feedback = SharedState.ReflectiveState.Predictions;

    UE_LOG(LogTemp, Verbose, TEXT("Feedforward propagated: %d items"), Feedback.Num());

    return Feedback;
}
