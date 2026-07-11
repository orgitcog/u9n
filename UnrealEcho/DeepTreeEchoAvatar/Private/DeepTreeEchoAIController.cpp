// Copyright Deep Tree Echo. All Rights Reserved.

#include "DeepTreeEchoAIController.h"

// ============================================================================
// Constructor
// ============================================================================

ADeepTreeEchoAIController::ADeepTreeEchoAIController()
{
    // Enable ticking so we can drive the cognitive cycle from Tick()
    // (UE default for AController has bCanEverTick = false)
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Create a perception component so the controller can receive AI stimuli
    PerceptionComponent = new UAIPerceptionComponent();
}

// ============================================================================
// Possession lifecycle
// ============================================================================

void ADeepTreeEchoAIController::BeginPlay()
{
    AController::BeginPlay();
}

void ADeepTreeEchoAIController::OnPossess(APawn* InPawn)
{
    AAIController::OnPossess(InPawn);

    if (InPawn)
    {
        AcquireCognitiveCore(InPawn);

        if (CognitiveCore)
        {
            CognitiveCore->InitializeSystem();

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
    AActor::Tick(DeltaTime);

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

    // 1. Gather and feed perception
    FeedPerceptionToCognition();

    // 2. Generate action vector from the cognitive core
    TArray<float> ActionVector = CognitiveCore->GenerateActionOutput();
    LastActionVector = ActionVector;

    // 3. Translate action vector to world-level commands
    ApplyCognitiveActionsToWorld(ActionVector);

    if (bDebugLogging)
    {
        UE_LOG(LogTemp, Log,
            TEXT("DeepTreeEchoAI: Cycle step=%d  action_dim=%d"),
            CognitiveCore->GetCurrentCycleStep(),
            ActionVector.Num());
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
    // representing approximate threat / interest level.
    TArray<float> SalienceData;
    SalienceData.Reserve(PerceivedActors.Num() * 3); // x, y, salience per actor

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
    if (ActionVector.Num() < 2)
    {
        return;  // Need at least [movement_salience, focus_salience]
    }

    // Convention for the action vector layout (matches CognitiveActionArbiter):
    //   [0] = movement salience / urgency
    //   [1] = focus shift salience
    //   [2] = target index in LastSalienceVector (float-encoded int)
    //   [3..] = reserved for future subsystem outputs

    const float MovementUrgency = ActionVector[0];
    const float FocusUrgency    = ActionVector.Num() > 1 ? ActionVector[1] : 0.0f;

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
