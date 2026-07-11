// Copyright Deep Tree Echo. All Rights Reserved.

#include "DeepTreeEchoAIController.h"
#include "UnrealBridge/CognitiveActionArbiter.h"

namespace
{
/** Map the UE-facing cognitive mode enum onto the arbiter's standalone enum. */
DeepTreeEcho::CognitiveMode ToArbiterMode(ECognitiveMode Mode)
{
    switch (Mode)
    {
        case ECognitiveMode::Reactive:      return DeepTreeEcho::CognitiveMode::Reactive;
        case ECognitiveMode::Deliberative:  return DeepTreeEcho::CognitiveMode::Deliberative;
        case ECognitiveMode::Reflective:    return DeepTreeEcho::CognitiveMode::Reflective;
        case ECognitiveMode::Creative:      return DeepTreeEcho::CognitiveMode::Creative;
        case ECognitiveMode::Integrative:   return DeepTreeEcho::CognitiveMode::Integrative;
        default:                            return DeepTreeEcho::CognitiveMode::Reactive;
    }
}
} // namespace

// ============================================================================
// Constructor
// ============================================================================

ADeepTreeEchoAIController::ADeepTreeEchoAIController()
{
    // Enable ticking so we can drive the cognitive cycle from Tick()
    // (UE default for AController has bCanEverTick = false)
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Create the perception component through the subobject system so it is
    // owned, GC-tracked, and serialized like any other default component.
    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("DeepTreeEchoPerception"));
}

// ============================================================================
// Possession lifecycle
// ============================================================================

void ADeepTreeEchoAIController::BeginPlay()
{
    AAIController::BeginPlay();
}

void ADeepTreeEchoAIController::OnPossess(APawn* InPawn)
{
    AAIController::OnPossess(InPawn);

    if (InPawn)
    {
        AcquireCognitiveCore(InPawn);

        if (CognitiveCore)
        {
            // The core initializes itself in BeginPlay.  Only initialize here
            // when possession happens first (e.g. dynamically spawned pawns);
            // re-possession must not reset accumulated cognitive state.
            if (!CognitiveCore->IsSystemInitialized())
            {
                CognitiveCore->InitializeSystem();
            }

            UE_LOG(LogTemp, Log,
                TEXT("DeepTreeEchoAIController: Possessed pawn - cognitive core acquired and initialized"));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("DeepTreeEchoAIController: Could not acquire UDeepTreeEchoCore on possessed pawn"));
        }
    }

    // Start the perception listener
    if (PerceptionComponent)
    {
        PerceptionComponent->RequestStimuliListenerUpdate();
    }
}

void ADeepTreeEchoAIController::OnUnPossess()
{
    CognitiveCore = nullptr;
    CurrentMoveTarget = nullptr;
    bManualMoveTargetOverride = false;
    LastPerceivedActors.Empty();
    LastSalienceVector.Empty();
    LastActionVector.Empty();
    CognitiveTickAccumulator = 0.0f;

    AAIController::OnUnPossess();
}

// ============================================================================
// Per-frame tick: drives cognitive cycle at CognitiveTickRate Hz
// ============================================================================

void ADeepTreeEchoAIController::Tick(float DeltaTime)
{
    AAIController::Tick(DeltaTime);

    if (!CognitiveCore)
    {
        return;
    }

    CognitiveTickAccumulator += DeltaTime;
    const float CognitiveTickInterval = 1.0f / FMath::Max(CognitiveTickRate, 1.0f);

    if (CognitiveTickAccumulator >= CognitiveTickInterval)
    {
        CognitiveTickAccumulator -= CognitiveTickInterval;
        ForceCognitiveTick();
    }
}

// ============================================================================
// Blueprint-callable API
// ============================================================================

void ADeepTreeEchoAIController::InjectPerceptionStimulus(
    const TArray<float>& SensoryData, const FString& Modality)
{
    if (CognitiveCore)
    {
        CognitiveCore->ProcessSensoryInput(SensoryData, Modality);
    }
}

void ADeepTreeEchoAIController::SetMoveTarget(AActor* NewTarget)
{
    // A non-null target pins manual control; nullptr resumes autonomous
    // (arbiter-driven) target selection on the next cognitive tick.
    bManualMoveTargetOverride = (NewTarget != nullptr);
    CurrentMoveTarget = NewTarget;

    if (NewTarget)
    {
        MoveToActor(NewTarget, NavigationAcceptanceRadius);
    }
    else
    {
        StopMovement();
    }
}

void ADeepTreeEchoAIController::ForceCognitiveTick()
{
    if (!CognitiveCore)
    {
        return;
    }

    // 1. Gather and feed perception.  This refreshes LastSalienceVector and
    //    LastPerceivedActors (index-aligned) for the arbiter below.
    FeedPerceptionToCognition();

    // 2. Run the decision arbiter: salience + cognitive mode → compact action
    //    vector [movement_urgency, focus_urgency, target_index].  Raw
    //    reservoir activations from GenerateActionOutput() have no such
    //    layout, so the arbiter owns the salience→action mapping.
    DeepTreeEcho::ArbiterConfig ArbiterCfg;
    ArbiterCfg.movement_threshold = MovementSalienceThreshold;
    ArbiterCfg.focus_threshold = FocusSalienceThreshold;
    const DeepTreeEcho::CognitiveActionArbiter Arbiter(ArbiterCfg);

    std::vector<float> Salience(static_cast<size_t>(LastSalienceVector.Num()));
    for (int32 i = 0; i < LastSalienceVector.Num(); ++i)
    {
        Salience[static_cast<size_t>(i)] = LastSalienceVector[i];
    }

    const Eigen::VectorXf Action =
        Arbiter.compute(Salience, ToArbiterMode(CognitiveCore->CurrentMode));

    LastActionVector.Empty();
    for (int i = 0; i < Action.size(); ++i)
    {
        LastActionVector.Add(Action[i]);
    }

    // 3. Translate the arbiter's action vector into world-level commands
    ApplyCognitiveActionsToWorld(LastActionVector);

    if (bDebugLogging)
    {
        UE_LOG(LogTemp, Log,
            TEXT("DeepTreeEchoAI: Cycle step=%d  action_dim=%d"),
            CognitiveCore->GetCurrentCycleStep(),
            LastActionVector.Num());
    }
}

FString ADeepTreeEchoAIController::GetCognitiveModeString() const
{
    if (!CognitiveCore)
    {
        return FString("None");
    }

    switch (CognitiveCore->CurrentMode)
    {
        case ECognitiveMode::Reactive:      return FString("Reactive");
        case ECognitiveMode::Deliberative:  return FString("Deliberative");
        case ECognitiveMode::Reflective:    return FString("Reflective");
        case ECognitiveMode::Creative:      return FString("Creative");
        case ECognitiveMode::Integrative:   return FString("Integrative");
        default:                            return FString("Unknown");
    }
}

// ============================================================================
// Private helpers
// ============================================================================

void ADeepTreeEchoAIController::AcquireCognitiveCore(APawn* InPawn)
{
    if (!InPawn)
    {
        return;
    }

    // Try to find an existing UDeepTreeEchoCore component
    CognitiveCore = InPawn->FindComponentByClass<UDeepTreeEchoCore>();

    if (!CognitiveCore)
    {
        // Component absent — log a warning; in production UE would call
        // NewObject<UDeepTreeEchoCore> here, but that requires GC and a real
        // UObject system.  The consumer should add the component in the
        // Character/Pawn blueprint or constructor instead.
        UE_LOG(LogTemp, Warning,
            TEXT("DeepTreeEchoAIController: UDeepTreeEchoCore not found on pawn. "
                 "Please add a UDeepTreeEchoCore ActorComponent to the pawn blueprint."));
    }
}

void ADeepTreeEchoAIController::FeedPerceptionToCognition()
{
    if (!CognitiveCore || !PerceptionComponent)
    {
        return;
    }

    // Gather currently perceived actors (sight sense)
    TArray<AActor*> PerceivedActors;
    PerceptionComponent->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

    // Build a simple salience vector: one float per perceived actor
    // representing approximate threat / interest level.  Cache the actors
    // index-aligned with the salience values so the arbiter's target index
    // can be resolved back to an actor.
    TArray<float> SalienceData;
    SalienceData.Reserve(PerceivedActors.Num());
    LastPerceivedActors.Empty();
    LastPerceivedActors.Reserve(PerceivedActors.Num());

    APawn* ControlledPawn = GetPawn();
    FVector PawnLocation = ControlledPawn ? ControlledPawn->GetActorLocation() : FVector();

    float HighestSalience = 0.0f;
    AActor* MostSalientActor = nullptr;

    for (AActor* Actor : PerceivedActors)
    {
        if (!Actor) { continue; }

        // Compute distance-based salience: closer = more salient
        float Distance = FVector::Dist(PawnLocation, Actor->GetActorLocation());
        float Salience = FMath::Clamp(1.0f - Distance / 5000.0f, 0.0f, 1.0f);

        SalienceData.Add(Salience);
        LastPerceivedActors.Add(Actor);

        if (Salience > HighestSalience)
        {
            HighestSalience = Salience;
            MostSalientActor = Actor;
        }
    }

    LastSalienceVector = SalienceData;

    // Feed the raw salience vector as a "Visual" modality stimulus
    if (SalienceData.Num() > 0)
    {
        CognitiveCore->ProcessSensoryInput(SalienceData, FString("Visual"));
    }

    // Update relevance frame with the highest-salience target
    if (MostSalientActor && HighestSalience >= FocusSalienceThreshold)
    {
        CognitiveCore->AllocateAttention(FString("PerceivedActor"), HighestSalience);

        // Focus gaze on the most salient actor
        if (HighestSalience >= FocusSalienceThreshold)
        {
            SetFocus(MostSalientActor);
        }
    }
    else
    {
        // Nothing salient — clear focus
        ClearFocus();
    }
}

void ADeepTreeEchoAIController::ApplyCognitiveActionsToWorld(
    const TArray<float>& ActionVector)
{
    if (ActionVector.Num() < 3)
    {
        return;  // Expect [movement_urgency, focus_urgency, target_index]
    }

    // Convention for the action vector layout (matches CognitiveActionArbiter):
    //   [0] = movement salience / urgency
    //   [1] = focus shift salience (0 when below the focus threshold)
    //   [2] = target index in LastPerceivedActors (float-encoded int, -1 = none)
    //   [3..] = reserved for future subsystem outputs

    const float MovementUrgency = ActionVector[0];
    const float FocusUrgency    = ActionVector[1];
    const int32 TargetIndex     = FMath::RoundToInt(ActionVector[2]);

    // --- Autonomous target selection ---
    // Unless a Blueprint pinned a manual target via SetMoveTarget, resolve the
    // arbiter-selected index against the perception cache (index-aligned with
    // LastSalienceVector).  An out-of-range / -1 index clears the target.
    if (!bManualMoveTargetOverride)
    {
        CurrentMoveTarget = LastPerceivedActors.IsValidIndex(TargetIndex)
                                ? LastPerceivedActors[TargetIndex]
                                : nullptr;
    }

    // --- Movement ---
    if (MovementUrgency >= MovementSalienceThreshold && CurrentMoveTarget)
    {
        MoveToActor(CurrentMoveTarget, NavigationAcceptanceRadius);
    }
    else if (MovementUrgency < 0.1f)
    {
        StopMovement();
    }

    // --- Focus ---
    if (FocusUrgency >= FocusSalienceThreshold && CurrentMoveTarget)
    {
        SetFocus(CurrentMoveTarget);
    }
}
