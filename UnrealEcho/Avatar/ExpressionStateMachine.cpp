#include "ExpressionStateMachine.h"
#include "DeepTreeEchoExpressionSystem.h"

UExpressionStateMachine::UExpressionStateMachine()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    ExpressionSystem = nullptr;

    CurrentState = EExpressionState::Neutral;
    PreviousState = EExpressionState::Neutral;
    TransitionFromState = EExpressionState::Neutral;
    TransitionToState = EExpressionState::Neutral;

    bIsTransitioning = false;
    TransitionProgress = 0.0f;
    TransitionDuration = 0.3f;
    CurrentCurve = ETransitionCurve::EaseInOut;
    CurrentPriority = EExpressionPriority::Normal;

    bIsHolding = false;
    HoldTimer = 0.0f;
    HoldDuration = 0.0f;
    bReturnAfterHold = false;
    ReturnState = EExpressionState::Neutral;

    bIsPlayingSequence = false;
    CurrentSequenceStep = 0;
    bSequenceStepComplete = false;
}

void UExpressionStateMachine::BeginPlay()
{
    Super::BeginPlay();

    // Find expression system on owner
    ExpressionSystem = GetOwner()->FindComponentByClass<UDeepTreeEchoExpressionSystem>();

    if (ExpressionSystem)
    {
        // Initialize morph targets from expression system
        CurrentMorphTargets = ExpressionSystem->GetCurrentMorphTargets();
        TargetMorphTargets = CurrentMorphTargets;
    }

    InitializeSequences();

    UE_LOG(LogTemp, Log, TEXT("ExpressionStateMachine initialized"));
}

void UExpressionStateMachine::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateTransition(DeltaTime);
    UpdateHold(DeltaTime);
    UpdateSequence(DeltaTime);
    ProcessQueue();
}

void UExpressionStateMachine::InitializeSequences()
{
    // Wonder-to-Joy sequence
    {
        FExpressionSequence WonderToJoy;
        WonderToJoy.SequenceName = TEXT("WonderToJoy");
        WonderToJoy.bLoop = false;
        
        // Phase 1: Build wonder
        WonderToJoy.Steps.Add(FExpressionTransitionRequest(EExpressionState::Contemplative, 0.4f));
        WonderToJoy.Steps[0].HoldDuration = 0.3f;
        
        // Phase 2: Curious gaze (the question)
        WonderToJoy.Steps.Add(FExpressionTransitionRequest(EExpressionState::CuriousGaze, 0.2f));
        WonderToJoy.Steps[1].HoldDuration = 0.2f;
        
        // Phase 3: Awe (the realization)
        WonderToJoy.Steps.Add(FExpressionTransitionRequest(EExpressionState::Awe, 0.15f));
        WonderToJoy.Steps[2].HoldDuration = 0.1f;
        
        // Phase 4: Joy (the insight)
        WonderToJoy.Steps.Add(FExpressionTransitionRequest(EExpressionState::BroadSmile, 0.2f));
        WonderToJoy.Steps[3].HoldDuration = 0.5f;
        
        // Phase 5: Settle to gentle smile
        WonderToJoy.Steps.Add(FExpressionTransitionRequest(EExpressionState::GentleSmile, 0.3f));
        
        RegisteredSequences.Add(WonderToJoy.SequenceName, WonderToJoy);
    }

    // Thinking sequence
    {
        FExpressionSequence Thinking;
        Thinking.SequenceName = TEXT("Thinking");
        Thinking.bLoop = true;
        
        Thinking.Steps.Add(FExpressionTransitionRequest(EExpressionState::Contemplative, 0.3f));
        Thinking.Steps[0].HoldDuration = 2.0f;
        
        Thinking.Steps.Add(FExpressionTransitionRequest(EExpressionState::UpwardGaze, 0.2f));
        Thinking.Steps[1].HoldDuration = 1.0f;
        
        Thinking.Steps.Add(FExpressionTransitionRequest(EExpressionState::CuriousGaze, 0.2f));
        Thinking.Steps[2].HoldDuration = 0.8f;
        
        Thinking.Steps.Add(FExpressionTransitionRequest(EExpressionState::Contemplative, 0.3f));
        Thinking.Steps[3].HoldDuration = 1.5f;
        
        RegisteredSequences.Add(Thinking.SequenceName, Thinking);
    }

    // Greeting sequence
    {
        FExpressionSequence Greeting;
        Greeting.SequenceName = TEXT("Greeting");
        Greeting.bLoop = false;
        
        // Alert recognition
        Greeting.Steps.Add(FExpressionTransitionRequest(EExpressionState::AlertGaze, 0.15f));
        Greeting.Steps[0].HoldDuration = 0.1f;
        
        // Surprise/recognition
        Greeting.Steps.Add(FExpressionTransitionRequest(EExpressionState::Awe, 0.1f));
        Greeting.Steps[1].HoldDuration = 0.15f;
        
        // Warm smile
        Greeting.Steps.Add(FExpressionTransitionRequest(EExpressionState::BroadSmile, 0.2f));
        Greeting.Steps[2].HoldDuration = 0.5f;
        
        // Settle to engaged gentle smile
        Greeting.Steps.Add(FExpressionTransitionRequest(EExpressionState::GentleSmile, 0.3f));
        
        RegisteredSequences.Add(Greeting.SequenceName, Greeting);
    }

    // Listening sequence
    {
        FExpressionSequence Listening;
        Listening.SequenceName = TEXT("Listening");
        Listening.bLoop = true;
        
        Listening.Steps.Add(FExpressionTransitionRequest(EExpressionState::AlertGaze, 0.2f));
        Listening.Steps[0].HoldDuration = 1.5f;
        
        Listening.Steps.Add(FExpressionTransitionRequest(EExpressionState::GentleSmile, 0.2f));
        Listening.Steps[1].HoldDuration = 1.0f;
        
        Listening.Steps.Add(FExpressionTransitionRequest(EExpressionState::CuriousGaze, 0.2f));
        Listening.Steps[2].HoldDuration = 0.8f;
        
        RegisteredSequences.Add(Listening.SequenceName, Listening);
    }

    UE_LOG(LogTemp, Log, TEXT("Registered %d expression sequences"), RegisteredSequences.Num());
}

void UExpressionStateMachine::RequestTransition(const FExpressionTransitionRequest& Request)
{
    // Check priority
    if (bIsTransitioning && Request.Priority < CurrentPriority)
    {
        // Lower priority, queue it
        QueueTransition(Request);
        return;
    }

    // Start the transition
    PreviousState = CurrentState;
    TransitionFromState = CurrentState;
    TransitionToState = Request.TargetState;
    TransitionDuration = FMath::Max(Request.Duration, 0.01f);
    CurrentCurve = Request.Curve;
    CurrentPriority = Request.Priority;
    TransitionProgress = 0.0f;
    bIsTransitioning = true;

    // Set up hold if specified
    if (Request.HoldDuration > 0.0f)
    {
        HoldDuration = Request.HoldDuration;
        bReturnAfterHold = Request.bReturnToPrevious;
        ReturnState = PreviousState;
    }
    else
    {
        HoldDuration = 0.0f;
        bReturnAfterHold = false;
    }

    // Get target morph targets
    TargetMorphTargets = GetMorphTargetsForState(TransitionToState);

    UE_LOG(LogTemp, Log, TEXT("Expression transition: %d -> %d (%.2fs, priority %d)"),
        (int32)TransitionFromState, (int32)TransitionToState, TransitionDuration, (int32)CurrentPriority);
}

void UExpressionStateMachine::TransitionTo(EExpressionState NewState, float Duration)
{
    FExpressionTransitionRequest Request(NewState, Duration);
    RequestTransition(Request);
}

void UExpressionStateMachine::SetStateImmediate(EExpressionState NewState)
{
    PreviousState = CurrentState;
    CurrentState = NewState;
    TransitionFromState = NewState;
    TransitionToState = NewState;
    bIsTransitioning = false;
    TransitionProgress = 1.0f;

    CurrentMorphTargets = GetMorphTargetsForState(NewState);
    TargetMorphTargets = CurrentMorphTargets;

    // Update expression system
    if (ExpressionSystem)
    {
        ExpressionSystem->SetExpressionState(NewState, 0.0f);
    }

    OnExpressionChanged.Broadcast(PreviousState, NewState);
    OnTransitionComplete.Broadcast(NewState);
}

void UExpressionStateMachine::QueueTransition(const FExpressionTransitionRequest& Request)
{
    TransitionQueue.Add(Request);
}

void UExpressionStateMachine::ClearQueue()
{
    TransitionQueue.Empty();
}

void UExpressionStateMachine::ReturnToNeutral(float Duration)
{
    TransitionTo(EExpressionState::Neutral, Duration);
}

void UExpressionStateMachine::ProcessQueue()
{
    if (!bIsTransitioning && !bIsHolding && TransitionQueue.Num() > 0)
    {
        FExpressionTransitionRequest NextRequest = TransitionQueue[0];
        TransitionQueue.RemoveAt(0);
        RequestTransition(NextRequest);
    }
}

void UExpressionStateMachine::UpdateTransition(float DeltaTime)
{
    if (!bIsTransitioning)
        return;

    TransitionProgress += DeltaTime / TransitionDuration;

    if (TransitionProgress >= 1.0f)
    {
        // Transition complete
        TransitionProgress = 1.0f;
        bIsTransitioning = false;
        CurrentState = TransitionToState;
        CurrentMorphTargets = TargetMorphTargets;

        // Fire events
        OnExpressionChanged.Broadcast(PreviousState, CurrentState);
        OnTransitionComplete.Broadcast(CurrentState);

        // Update expression system
        if (ExpressionSystem)
        {
            ExpressionSystem->SetExpressionState(CurrentState, 0.0f);
        }

        // Start hold if specified
        if (HoldDuration > 0.0f)
        {
            bIsHolding = true;
            HoldTimer = 0.0f;
        }

        UE_LOG(LogTemp, Log, TEXT("Expression transition complete: %d"), (int32)CurrentState);
    }
    else
    {
        // Interpolate morph targets
        float CurvedAlpha = ApplyCurve(TransitionProgress, CurrentCurve);
        FDeepTreeEchoMorphTargets FromTargets = GetMorphTargetsForState(TransitionFromState);
        CurrentMorphTargets = FDeepTreeEchoMorphTargets::Lerp(FromTargets, TargetMorphTargets, CurvedAlpha);
    }
}

void UExpressionStateMachine::UpdateHold(float DeltaTime)
{
    if (!bIsHolding)
        return;

    HoldTimer += DeltaTime;

    if (HoldTimer >= HoldDuration)
    {
        bIsHolding = false;
        HoldTimer = 0.0f;

        if (bReturnAfterHold)
        {
            TransitionTo(ReturnState, 0.3f);
        }

        // If playing sequence, mark step complete
        if (bIsPlayingSequence)
        {
            bSequenceStepComplete = true;
        }
    }
}

void UExpressionStateMachine::UpdateSequence(float DeltaTime)
{
    if (!bIsPlayingSequence)
        return;

    if (!RegisteredSequences.Contains(CurrentSequenceName))
    {
        bIsPlayingSequence = false;
        return;
    }

    const FExpressionSequence& Sequence = RegisteredSequences[CurrentSequenceName];

    // Check if current step is complete
    if (bSequenceStepComplete && !bIsTransitioning && !bIsHolding)
    {
        bSequenceStepComplete = false;
        CurrentSequenceStep++;

        if (CurrentSequenceStep >= Sequence.Steps.Num())
        {
            if (Sequence.bLoop)
            {
                CurrentSequenceStep = 0;
            }
            else
            {
                bIsPlayingSequence = false;
                UE_LOG(LogTemp, Log, TEXT("Expression sequence '%s' complete"), *CurrentSequenceName.ToString());
                return;
            }
        }

        // Start next step
        RequestTransition(Sequence.Steps[CurrentSequenceStep]);
    }
}

float UExpressionStateMachine::ApplyCurve(float Alpha, ETransitionCurve Curve) const
{
    switch (Curve)
    {
    case ETransitionCurve::Linear:
        return Alpha;

    case ETransitionCurve::EaseIn:
        return Alpha * Alpha;

    case ETransitionCurve::EaseOut:
        return 1.0f - FMath::Pow(1.0f - Alpha, 2.0f);

    case ETransitionCurve::EaseInOut:
        return Alpha < 0.5f
            ? 2.0f * Alpha * Alpha
            : 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 2.0f) / 2.0f;

    case ETransitionCurve::Bounce:
        {
            float n1 = 7.5625f;
            float d1 = 2.75f;
            if (Alpha < 1.0f / d1)
            {
                return n1 * Alpha * Alpha;
            }
            else if (Alpha < 2.0f / d1)
            {
                Alpha -= 1.5f / d1;
                return n1 * Alpha * Alpha + 0.75f;
            }
            else if (Alpha < 2.5f / d1)
            {
                Alpha -= 2.25f / d1;
                return n1 * Alpha * Alpha + 0.9375f;
            }
            else
            {
                Alpha -= 2.625f / d1;
                return n1 * Alpha * Alpha + 0.984375f;
            }
        }

    case ETransitionCurve::Overshoot:
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return 1.0f + c3 * FMath::Pow(Alpha - 1.0f, 3.0f) + c1 * FMath::Pow(Alpha - 1.0f, 2.0f);
        }

    default:
        return Alpha;
    }
}

FDeepTreeEchoMorphTargets UExpressionStateMachine::GetMorphTargetsForState(EExpressionState State) const
{
    if (ExpressionSystem)
    {
        // Get from expression system's presets
        // This requires the expression system to expose its presets
        // For now, return current if matching, otherwise default
        if (State == CurrentState)
        {
            return CurrentMorphTargets;
        }
    }

    // Return default morph targets
    // In production, this would query the expression system's preset map
    return FDeepTreeEchoMorphTargets();
}

// ========== Composite Expressions ==========

void UExpressionStateMachine::AddExpressionLayer(const FExpressionLayer& Layer)
{
    // Check if layer already exists
    for (int32 i = 0; i < ExpressionLayers.Num(); i++)
    {
        if (ExpressionLayers[i].Expression == Layer.Expression)
        {
            ExpressionLayers[i] = Layer;
            return;
        }
    }
    ExpressionLayers.Add(Layer);
}

void UExpressionStateMachine::RemoveExpressionLayer(EExpressionState Expression)
{
    for (int32 i = ExpressionLayers.Num() - 1; i >= 0; i--)
    {
        if (ExpressionLayers[i].Expression == Expression)
        {
            ExpressionLayers.RemoveAt(i);
            return;
        }
    }
}

void UExpressionStateMachine::ClearExpressionLayers()
{
    ExpressionLayers.Empty();
}

void UExpressionStateMachine::SetLayerWeight(EExpressionState Expression, float Weight)
{
    for (int32 i = 0; i < ExpressionLayers.Num(); i++)
    {
        if (ExpressionLayers[i].Expression == Expression)
        {
            ExpressionLayers[i].Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
            return;
        }
    }
}

FDeepTreeEchoMorphTargets UExpressionStateMachine::GetBlendedMorphTargets() const
{
    FDeepTreeEchoMorphTargets Result = CurrentMorphTargets;

    // Apply layers
    for (const FExpressionLayer& Layer : ExpressionLayers)
    {
        FDeepTreeEchoMorphTargets LayerTargets = GetMorphTargetsForState(Layer.Expression);
        
        if (Layer.bAdditive)
        {
            Result = FDeepTreeEchoMorphTargets::Add(Result, LayerTargets, Layer.Weight);
        }
        else
        {
            Result = FDeepTreeEchoMorphTargets::Lerp(Result, LayerTargets, Layer.Weight);
        }
    }

    return Result;
}

// ========== Sequences ==========

void UExpressionStateMachine::PlaySequence(const FName& SequenceName)
{
    if (!RegisteredSequences.Contains(SequenceName))
    {
        UE_LOG(LogTemp, Warning, TEXT("Expression sequence '%s' not found"), *SequenceName.ToString());
        return;
    }

    const FExpressionSequence& Sequence = RegisteredSequences[SequenceName];
    if (Sequence.Steps.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Expression sequence '%s' has no steps"), *SequenceName.ToString());
        return;
    }

    bIsPlayingSequence = true;
    CurrentSequenceName = SequenceName;
    CurrentSequenceStep = 0;
    bSequenceStepComplete = false;

    // Start first step
    RequestTransition(Sequence.Steps[0]);

    UE_LOG(LogTemp, Log, TEXT("Playing expression sequence '%s'"), *SequenceName.ToString());
}

void UExpressionStateMachine::StopSequence()
{
    bIsPlayingSequence = false;
    CurrentSequenceName = NAME_None;
    CurrentSequenceStep = 0;
}

void UExpressionStateMachine::RegisterSequence(const FExpressionSequence& Sequence)
{
    RegisteredSequences.Add(Sequence.SequenceName, Sequence);
}

// ========== Special Transitions ==========

void UExpressionStateMachine::PlayWonderToJoy(float Duration)
{
    // Scale the predefined sequence timing
    if (RegisteredSequences.Contains(TEXT("WonderToJoy")))
    {
        PlaySequence(TEXT("WonderToJoy"));
    }
    else
    {
        // Fallback: simple transition
        TransitionTo(EExpressionState::Contemplative, Duration * 0.3f);
        QueueTransition(FExpressionTransitionRequest(EExpressionState::BroadSmile, Duration * 0.3f));
        QueueTransition(FExpressionTransitionRequest(EExpressionState::GentleSmile, Duration * 0.4f));
    }
}

void UExpressionStateMachine::PlayThinkingSequence()
{
    PlaySequence(TEXT("Thinking"));
}

void UExpressionStateMachine::PlayGreeting()
{
    PlaySequence(TEXT("Greeting"));
}
