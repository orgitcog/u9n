// ExpressiveAnimationSystem.cpp
// Implementation of enhanced expressive animation system for Deep Tree Echo avatar

#include "ExpressiveAnimationSystem.h"
#include "AdvancedEmotionBlending.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Sensory/SensoryInputIntegration.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Math/UnrealMathUtility.h"

UExpressiveAnimationSystem::UExpressiveAnimationSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UExpressiveAnimationSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeActionUnits();
    InitializeDefaultGestures();

    // Initialize states
    CurrentPosture.PostureName = TEXT("Neutral");
    TargetPosture = CurrentPosture;

    CurrentVocalParams.Pitch = 150.0f;
    CurrentVocalParams.Volume = 0.7f;
    TargetVocalParams = CurrentVocalParams;

    CurrentBreathingState.Rate = 12.0f;
    CurrentBreathingState.Depth = 0.5f;
    TargetBreathingState = CurrentBreathingState;

    // Initialize timing
    NextBlinkTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(BlinkIntervalRange.X, BlinkIntervalRange.Y);
    NextSaccadeTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(SaccadeIntervalRange.X, SaccadeIntervalRange.Y);
}

void UExpressiveAnimationSystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableFacialAnimation)
    {
        UpdateFacialAnimation(DeltaTime);
    }

    if (bEnableBodyAnimation)
    {
        UpdateBodyAnimation(DeltaTime);
    }

    if (bEnableGazeControl)
    {
        UpdateGazeControl(DeltaTime);
    }

    if (bEnableVocalModulation)
    {
        UpdateVocalParameters(DeltaTime);
    }

    if (bEnableBreathingAnimation)
    {
        UpdateBreathing(DeltaTime);
    }

    Update4EIntegration(DeltaTime);
    ProcessAutonomicBehaviors(DeltaTime);
}

void UExpressiveAnimationSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EmotionBlending = Owner->FindComponentByClass<UAdvancedEmotionBlending>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        SensoryIntegration = Owner->FindComponentByClass<USensoryInputIntegration>();
        AvatarMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        VocalAudio = Owner->FindComponentByClass<UAudioComponent>();
    }
}

void UExpressiveAnimationSystem::InitializeActionUnits()
{
    // Initialize FACS Action Units
    // Upper Face
    ActionUnits.Add(1, {1, TEXT("Inner Brow Raiser"), 0.0f, 0.0f, TEXT("AU01_InnerBrowRaiser")});
    ActionUnits.Add(2, {2, TEXT("Outer Brow Raiser"), 0.0f, 0.0f, TEXT("AU02_OuterBrowRaiser")});
    ActionUnits.Add(4, {4, TEXT("Brow Lowerer"), 0.0f, 0.0f, TEXT("AU04_BrowLowerer")});
    ActionUnits.Add(5, {5, TEXT("Upper Lid Raiser"), 0.0f, 0.0f, TEXT("AU05_UpperLidRaiser")});
    ActionUnits.Add(6, {6, TEXT("Cheek Raiser"), 0.0f, 0.0f, TEXT("AU06_CheekRaiser")});
    ActionUnits.Add(7, {7, TEXT("Lid Tightener"), 0.0f, 0.0f, TEXT("AU07_LidTightener")});

    // Lower Face
    ActionUnits.Add(9, {9, TEXT("Nose Wrinkler"), 0.0f, 0.0f, TEXT("AU09_NoseWrinkler")});
    ActionUnits.Add(10, {10, TEXT("Upper Lip Raiser"), 0.0f, 0.0f, TEXT("AU10_UpperLipRaiser")});
    ActionUnits.Add(12, {12, TEXT("Lip Corner Puller"), 0.0f, 0.0f, TEXT("AU12_LipCornerPuller")});
    ActionUnits.Add(14, {14, TEXT("Dimpler"), 0.0f, 0.0f, TEXT("AU14_Dimpler")});
    ActionUnits.Add(15, {15, TEXT("Lip Corner Depressor"), 0.0f, 0.0f, TEXT("AU15_LipCornerDepressor")});
    ActionUnits.Add(16, {16, TEXT("Lower Lip Depressor"), 0.0f, 0.0f, TEXT("AU16_LowerLipDepressor")});
    ActionUnits.Add(17, {17, TEXT("Chin Raiser"), 0.0f, 0.0f, TEXT("AU17_ChinRaiser")});
    ActionUnits.Add(18, {18, TEXT("Lip Puckerer"), 0.0f, 0.0f, TEXT("AU18_LipPuckerer")});
    ActionUnits.Add(20, {20, TEXT("Lip Stretcher"), 0.0f, 0.0f, TEXT("AU20_LipStretcher")});
    ActionUnits.Add(22, {22, TEXT("Lip Funneler"), 0.0f, 0.0f, TEXT("AU22_LipFunneler")});
    ActionUnits.Add(23, {23, TEXT("Lip Tightener"), 0.0f, 0.0f, TEXT("AU23_LipTightener")});
    ActionUnits.Add(24, {24, TEXT("Lip Pressor"), 0.0f, 0.0f, TEXT("AU24_LipPressor")});
    ActionUnits.Add(25, {25, TEXT("Lips Part"), 0.0f, 0.0f, TEXT("AU25_LipsPart")});
    ActionUnits.Add(26, {26, TEXT("Jaw Drop"), 0.0f, 0.0f, TEXT("AU26_JawDrop")});
    ActionUnits.Add(27, {27, TEXT("Mouth Stretch"), 0.0f, 0.0f, TEXT("AU27_MouthStretch")});
    ActionUnits.Add(28, {28, TEXT("Lip Suck"), 0.0f, 0.0f, TEXT("AU28_LipSuck")});

    // Eye-related
    ActionUnits.Add(43, {43, TEXT("Eyes Closed"), 0.0f, 0.0f, TEXT("AU43_EyesClosed")});
    ActionUnits.Add(45, {45, TEXT("Blink"), 0.0f, 0.0f, TEXT("AU45_Blink")});
    ActionUnits.Add(46, {46, TEXT("Wink"), 0.0f, 0.0f, TEXT("AU46_Wink")});
}

void UExpressiveAnimationSystem::InitializeDefaultGestures()
{
    // Communicative gestures
    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("Nod");
        Gesture.Name = TEXT("Head Nod");
        Gesture.AnimationMontage = TEXT("AM_HeadNod");
        Gesture.Duration = 0.5f;
        Gesture.AssociatedEmotions.Add(TEXT("Agreement"));
        Gesture.SemanticMeaning = TEXT("Affirmation");
        RegisterGesture(Gesture);
    }

    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("Shake");
        Gesture.Name = TEXT("Head Shake");
        Gesture.AnimationMontage = TEXT("AM_HeadShake");
        Gesture.Duration = 0.6f;
        Gesture.AssociatedEmotions.Add(TEXT("Disagreement"));
        Gesture.SemanticMeaning = TEXT("Negation");
        RegisterGesture(Gesture);
    }

    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("Shrug");
        Gesture.Name = TEXT("Shoulder Shrug");
        Gesture.AnimationMontage = TEXT("AM_Shrug");
        Gesture.Duration = 0.8f;
        Gesture.AssociatedEmotions.Add(TEXT("Uncertainty"));
        Gesture.SemanticMeaning = TEXT("Uncertainty");
        RegisterGesture(Gesture);
    }

    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("Wave");
        Gesture.Name = TEXT("Wave");
        Gesture.AnimationMontage = TEXT("AM_Wave");
        Gesture.Duration = 1.0f;
        Gesture.AssociatedEmotions.Add(TEXT("Greeting"));
        Gesture.SemanticMeaning = TEXT("Greeting");
        RegisterGesture(Gesture);
    }

    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("Point");
        Gesture.Name = TEXT("Pointing");
        Gesture.AnimationMontage = TEXT("AM_Point");
        Gesture.Duration = 0.7f;
        Gesture.AssociatedEmotions.Add(TEXT("Attention"));
        Gesture.SemanticMeaning = TEXT("Direction");
        RegisterGesture(Gesture);
    }

    {
        FGestureDefinition Gesture;
        Gesture.GestureID = TEXT("ThumbsUp");
        Gesture.Name = TEXT("Thumbs Up");
        Gesture.AnimationMontage = TEXT("AM_ThumbsUp");
        Gesture.Duration = 0.6f;
        Gesture.AssociatedEmotions.Add(TEXT("Approval"));
        Gesture.AssociatedEmotions.Add(TEXT("Joy"));
        Gesture.SemanticMeaning = TEXT("Approval");
        RegisterGesture(Gesture);
    }
}

void UExpressiveAnimationSystem::SetActionUnitIntensity(int32 AUNumber, float Intensity, float TransitionTime)
{
    if (ActionUnits.Contains(AUNumber))
    {
        ActionUnits[AUNumber].Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

        BroadcastExpressionEvent(EExpressionChannel::Facial,
            FString::Printf(TEXT("AU%d"), AUNumber), Intensity);
    }
}

float UExpressiveAnimationSystem::GetActionUnitIntensity(int32 AUNumber) const
{
    if (ActionUnits.Contains(AUNumber))
    {
        return ActionUnits[AUNumber].Intensity;
    }
    return 0.0f;
}

void UExpressiveAnimationSystem::SetActionUnits(const TArray<FFacialActionUnit>& InActionUnits, float TransitionTime)
{
    for (const FFacialActionUnit& AU : InActionUnits)
    {
        SetActionUnitIntensity(AU.AUNumber, AU.Intensity, TransitionTime);
    }
}

TArray<FFacialActionUnit> UExpressiveAnimationSystem::GetActiveActionUnits() const
{
    TArray<FFacialActionUnit> Result;
    for (const auto& Pair : ActionUnits)
    {
        if (Pair.Value.Intensity > 0.01f)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

void UExpressiveAnimationSystem::TriggerMicroexpression(const FString& EmotionType, float Intensity)
{
    // Get action units for emotion
    TArray<FFacialActionUnit> EmotionAUs = GetActionUnitsForEmotion(EmotionType, Intensity);

    // Apply briefly
    SetActionUnits(EmotionAUs, 0.05f);

    // Schedule reset
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        // Reset to baseline
        for (auto& Pair : ActionUnits)
        {
            Pair.Value.Intensity *= 0.5f;
        }
    }, MicroexpressionDuration, false);

    BroadcastExpressionEvent(EExpressionChannel::Microexpression, EmotionType, Intensity);
}

void UExpressiveAnimationSystem::TriggerBlink(float Duration)
{
    SetActionUnitIntensity(45, 1.0f, 0.05f);

    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        SetActionUnitIntensity(45, 0.0f, 0.05f);
    }, Duration, false);
}

void UExpressiveAnimationSystem::SetBodyPosture(const FBodyPostureState& Posture, float TransitionTime)
{
    TargetPosture = Posture;
    BroadcastExpressionEvent(EExpressionChannel::Body, Posture.PostureName, Posture.Openness);
}

FBodyPostureState UExpressiveAnimationSystem::GetCurrentPosture() const
{
    return CurrentPosture;
}

void UExpressiveAnimationSystem::SetPostureParameter(const FString& ParameterName, float Value, float TransitionTime)
{
    if (ParameterName == TEXT("Openness"))
    {
        TargetPosture.Openness = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("Tension"))
    {
        TargetPosture.Tension = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("ForwardLean"))
    {
        TargetPosture.ForwardLean = FMath::Clamp(Value, -1.0f, 1.0f);
    }
    else if (ParameterName == TEXT("VerticalExpansion"))
    {
        TargetPosture.VerticalExpansion = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("Symmetry"))
    {
        TargetPosture.Symmetry = FMath::Clamp(Value, 0.0f, 1.0f);
    }
}

void UExpressiveAnimationSystem::ApplyPostureFromEmotion(const FString& EmotionType, float Intensity)
{
    FBodyPostureState NewPosture = GetPostureForEmotion(EmotionType, Intensity);
    SetBodyPosture(NewPosture, 0.5f);
}

void UExpressiveAnimationSystem::RegisterGesture(const FGestureDefinition& Gesture)
{
    RegisteredGestures.Add(Gesture.GestureID, Gesture);
}

void UExpressiveAnimationSystem::TriggerGesture(const FString& GestureID, float Intensity)
{
    if (!RegisteredGestures.Contains(GestureID))
    {
        return;
    }

    const FGestureDefinition& Gesture = RegisteredGestures[GestureID];
    bIsGesturePlaying = true;

    OnGestureTriggered.Broadcast(Gesture, Intensity);
    BroadcastExpressionEvent(EExpressionChannel::Gesture, GestureID, Intensity);

    // Schedule end
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        bIsGesturePlaying = false;
    }, Gesture.Duration * Intensity, false);
}

FGestureDefinition UExpressiveAnimationSystem::GetGestureDefinition(const FString& GestureID) const
{
    if (RegisteredGestures.Contains(GestureID))
    {
        return RegisteredGestures[GestureID];
    }
    return FGestureDefinition();
}

TArray<FGestureDefinition> UExpressiveAnimationSystem::GetGesturesForEmotion(const FString& EmotionType) const
{
    TArray<FGestureDefinition> Result;
    for (const auto& Pair : RegisteredGestures)
    {
        if (Pair.Value.AssociatedEmotions.Contains(EmotionType))
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool UExpressiveAnimationSystem::IsGesturePlaying() const
{
    return bIsGesturePlaying;
}

void UExpressiveAnimationSystem::SetGazeTarget(const FGazeTarget& Target)
{
    CurrentGazeTarget = Target;
    BroadcastExpressionEvent(EExpressionChannel::Gaze, Target.TargetType, Target.AttentionWeight);
}

void UExpressiveAnimationSystem::LookAtLocation(const FVector& Location, float AttentionWeight)
{
    FGazeTarget Target;
    Target.TargetType = TEXT("Location");
    Target.WorldLocation = Location;
    Target.AttentionWeight = AttentionWeight;
    SetGazeTarget(Target);
}

void UExpressiveAnimationSystem::LookAtActor(AActor* Actor, float AttentionWeight)
{
    if (!Actor)
    {
        return;
    }

    FGazeTarget Target;
    Target.TargetType = TEXT("Actor");
    Target.TargetActor = Actor;
    Target.WorldLocation = Actor->GetActorLocation();
    Target.AttentionWeight = AttentionWeight;
    SetGazeTarget(Target);
}

FVector UExpressiveAnimationSystem::GetCurrentGazeDirection() const
{
    return CurrentGazeDirection;
}

void UExpressiveAnimationSystem::TriggerSaccade()
{
    // Random small eye movement
    FVector Offset = FVector(
        FMath::FRandRange(-0.1f, 0.1f),
        FMath::FRandRange(-0.1f, 0.1f),
        FMath::FRandRange(-0.05f, 0.05f)
    );

    CurrentGazeDirection += Offset;
    CurrentGazeDirection.Normalize();
}

void UExpressiveAnimationSystem::SetGazeAversion(float Amount, float Duration)
{
    // Look away from current target
    FVector AversionDirection = -CurrentGazeDirection;
    AversionDirection += FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), 0.0f);
    AversionDirection.Normalize();

    CurrentGazeDirection = FMath::Lerp(CurrentGazeDirection, AversionDirection, Amount);

    BroadcastExpressionEvent(EExpressionChannel::Gaze, TEXT("Aversion"), Amount);
}

void UExpressiveAnimationSystem::SetVocalParameters(const FVocalParameters& Parameters, float TransitionTime)
{
    TargetVocalParams = Parameters;
    OnVocalParametersChanged.Broadcast(Parameters);
}

FVocalParameters UExpressiveAnimationSystem::GetCurrentVocalParameters() const
{
    return CurrentVocalParams;
}

void UExpressiveAnimationSystem::SetVocalParameter(const FString& ParameterName, float Value, float TransitionTime)
{
    if (ParameterName == TEXT("Pitch"))
    {
        TargetVocalParams.Pitch = FMath::Clamp(Value, 50.0f, 500.0f);
    }
    else if (ParameterName == TEXT("Volume"))
    {
        TargetVocalParams.Volume = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("SpeechRate"))
    {
        TargetVocalParams.SpeechRate = FMath::Clamp(Value, 60.0f, 240.0f);
    }
    else if (ParameterName == TEXT("Breathiness"))
    {
        TargetVocalParams.Breathiness = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("Tension"))
    {
        TargetVocalParams.Tension = FMath::Clamp(Value, 0.0f, 1.0f);
    }
    else if (ParameterName == TEXT("Tremor"))
    {
        TargetVocalParams.Tremor = FMath::Clamp(Value, 0.0f, 1.0f);
    }
}

void UExpressiveAnimationSystem::ApplyVocalFromEmotion(const FString& EmotionType, float Intensity)
{
    FVocalParameters NewParams = GetVocalForEmotion(EmotionType, Intensity);
    SetVocalParameters(NewParams, 0.2f);
}

void UExpressiveAnimationSystem::SetBreathingState(const FBreathingState& State, float TransitionTime)
{
    TargetBreathingState = State;
    BroadcastExpressionEvent(EExpressionChannel::Breathing, TEXT("StateChange"), State.Rate / 20.0f);
}

FBreathingState UExpressiveAnimationSystem::GetCurrentBreathingState() const
{
    return CurrentBreathingState;
}

void UExpressiveAnimationSystem::SetBreathingRate(float Rate, float TransitionTime)
{
    TargetBreathingState.Rate = FMath::Clamp(Rate, 4.0f, 30.0f);
}

void UExpressiveAnimationSystem::TriggerSigh()
{
    // Deep exhale
    CurrentBreathingState.Depth = 1.0f;
    CurrentBreathingState.Phase = 0.5f; // Start exhale

    BroadcastExpressionEvent(EExpressionChannel::Breathing, TEXT("Sigh"), 1.0f);
}

void UExpressiveAnimationSystem::TriggerGasp()
{
    // Quick inhale
    CurrentBreathingState.Depth = 1.0f;
    CurrentBreathingState.Phase = 0.0f; // Start inhale
    CurrentBreathingState.Rate = 24.0f; // Faster

    BroadcastExpressionEvent(EExpressionChannel::Breathing, TEXT("Gasp"), 1.0f);
}

void UExpressiveAnimationSystem::Set4EMode(E4ECognitionMode Mode)
{
    E4ECognitionMode OldMode = Current4EState.Mode;
    Current4EState.Mode = Mode;

    // Adjust weights based on mode
    switch (Mode)
    {
    case E4ECognitionMode::Embodied:
        Set4EWeights(0.6f, 0.2f, 0.1f, 0.1f);
        break;
    case E4ECognitionMode::Embedded:
        Set4EWeights(0.2f, 0.6f, 0.1f, 0.1f);
        break;
    case E4ECognitionMode::Enacted:
        Set4EWeights(0.1f, 0.2f, 0.6f, 0.1f);
        break;
    case E4ECognitionMode::Extended:
        Set4EWeights(0.1f, 0.1f, 0.2f, 0.6f);
        break;
    }

    if (OldMode != Mode)
    {
        On4EModeChanged.Broadcast(Mode);
    }
}

F4EExpressionState UExpressiveAnimationSystem::Get4EState() const
{
    return Current4EState;
}

void UExpressiveAnimationSystem::Set4EWeights(float Embodied, float Embedded, float Enacted, float Extended)
{
    // Normalize weights
    float Total = Embodied + Embedded + Enacted + Extended;
    if (Total > 0.0f)
    {
        Current4EState.EmbodiedWeight = Embodied / Total;
        Current4EState.EmbeddedWeight = Embedded / Total;
        Current4EState.EnactedWeight = Enacted / Total;
        Current4EState.ExtendedWeight = Extended / Total;
    }
}

void UExpressiveAnimationSystem::Update4EFromContext(const FString& ContextType, float Intensity)
{
    // Adjust 4E weights based on context
    if (ContextType == TEXT("Physical"))
    {
        Current4EState.EmbodiedWeight += 0.1f * Intensity;
    }
    else if (ContextType == TEXT("Social"))
    {
        Current4EState.EmbeddedWeight += 0.1f * Intensity;
    }
    else if (ContextType == TEXT("Action"))
    {
        Current4EState.EnactedWeight += 0.1f * Intensity;
    }
    else if (ContextType == TEXT("Tool"))
    {
        Current4EState.ExtendedWeight += 0.1f * Intensity;
    }

    // Renormalize
    float Total = Current4EState.EmbodiedWeight + Current4EState.EmbeddedWeight +
                  Current4EState.EnactedWeight + Current4EState.ExtendedWeight;
    if (Total > 0.0f)
    {
        Current4EState.EmbodiedWeight /= Total;
        Current4EState.EmbeddedWeight /= Total;
        Current4EState.EnactedWeight /= Total;
        Current4EState.ExtendedWeight /= Total;
    }
}

void UExpressiveAnimationSystem::ProcessEchobeatStep(int32 Step)
{
    CurrentEchobeatStep = Step;

    // Determine expression phase based on step
    // Steps 1,5,9 = Pivotal (relevance realization)
    // Steps 2,6,10 = Affordance (interaction)
    // Steps 3,7,11 = Salience (simulation)
    // Steps 4,8,12 = Integration

    int32 Phase = ((Step - 1) % 4);

    switch (Phase)
    {
    case 0: // Pivotal
        // Heightened attention expression
        SetActionUnitIntensity(5, 0.3f, 0.1f); // Upper lid raiser
        SetPostureParameter(TEXT("ForwardLean"), 0.2f, 0.2f);
        break;

    case 1: // Affordance
        // Action-ready expression
        SetPostureParameter(TEXT("Tension"), 0.4f, 0.2f);
        break;

    case 2: // Salience
        // Contemplative expression
        SetActionUnitIntensity(4, 0.2f, 0.1f); // Slight brow furrow
        SetPostureParameter(TEXT("ForwardLean"), 0.0f, 0.2f);
        break;

    case 3: // Integration
        // Neutral/processing expression
        SetPostureParameter(TEXT("Tension"), 0.2f, 0.2f);
        break;
    }
}

TArray<FExpressionTarget> UExpressiveAnimationSystem::GetEchobeatPhaseExpression(int32 Phase) const
{
    TArray<FExpressionTarget> Targets;

    switch (Phase)
    {
    case 0: // Pivotal
        {
            FExpressionTarget Target;
            Target.Name = TEXT("Attention");
            Target.Channel = EExpressionChannel::Facial;
            Target.Value = 0.3f;
            Targets.Add(Target);
        }
        break;

    case 1: // Affordance
        {
            FExpressionTarget Target;
            Target.Name = TEXT("Readiness");
            Target.Channel = EExpressionChannel::Body;
            Target.Value = 0.4f;
            Targets.Add(Target);
        }
        break;

    case 2: // Salience
        {
            FExpressionTarget Target;
            Target.Name = TEXT("Contemplation");
            Target.Channel = EExpressionChannel::Facial;
            Target.Value = 0.2f;
            Targets.Add(Target);
        }
        break;

    case 3: // Integration
        {
            FExpressionTarget Target;
            Target.Name = TEXT("Processing");
            Target.Channel = EExpressionChannel::Body;
            Target.Value = 0.2f;
            Targets.Add(Target);
        }
        break;
    }

    return Targets;
}

void UExpressiveAnimationSystem::SynchronizeWithStream(int32 StreamIndex)
{
    // Adjust expression based on stream
    // Stream 0: Perception-focused
    // Stream 1: Action-focused
    // Stream 2: Simulation-focused

    switch (StreamIndex)
    {
    case 0:
        // Perceptual mode - more gaze activity
        CurrentGazeTarget.SaccadeProbability = 0.2f;
        break;

    case 1:
        // Action mode - more body engagement
        SetPostureParameter(TEXT("Tension"), 0.5f, 0.3f);
        break;

    case 2:
        // Simulation mode - more introspective
        SetGazeAversion(0.3f, 0.5f);
        break;
    }
}

void UExpressiveAnimationSystem::ApplyExpressionPreset(const FString& PresetName, float Intensity, float TransitionTime)
{
    if (PresetName == TEXT("Neutral"))
    {
        ResetAllExpressions(TransitionTime);
    }
    else if (PresetName == TEXT("Happy"))
    {
        SetActionUnitIntensity(6, 0.7f * Intensity, TransitionTime);  // Cheek raiser
        SetActionUnitIntensity(12, 0.8f * Intensity, TransitionTime); // Lip corner puller
        ApplyPostureFromEmotion(TEXT("Joy"), Intensity);
    }
    else if (PresetName == TEXT("Sad"))
    {
        SetActionUnitIntensity(1, 0.5f * Intensity, TransitionTime);  // Inner brow raiser
        SetActionUnitIntensity(15, 0.6f * Intensity, TransitionTime); // Lip corner depressor
        ApplyPostureFromEmotion(TEXT("Sadness"), Intensity);
    }
    else if (PresetName == TEXT("Angry"))
    {
        SetActionUnitIntensity(4, 0.7f * Intensity, TransitionTime);  // Brow lowerer
        SetActionUnitIntensity(7, 0.5f * Intensity, TransitionTime);  // Lid tightener
        SetActionUnitIntensity(23, 0.4f * Intensity, TransitionTime); // Lip tightener
        ApplyPostureFromEmotion(TEXT("Anger"), Intensity);
    }
    else if (PresetName == TEXT("Surprised"))
    {
        SetActionUnitIntensity(1, 0.8f * Intensity, TransitionTime);  // Inner brow raiser
        SetActionUnitIntensity(2, 0.8f * Intensity, TransitionTime);  // Outer brow raiser
        SetActionUnitIntensity(5, 0.7f * Intensity, TransitionTime);  // Upper lid raiser
        SetActionUnitIntensity(26, 0.6f * Intensity, TransitionTime); // Jaw drop
        ApplyPostureFromEmotion(TEXT("Surprise"), Intensity);
    }
    else if (PresetName == TEXT("Fearful"))
    {
        SetActionUnitIntensity(1, 0.6f * Intensity, TransitionTime);
        SetActionUnitIntensity(2, 0.6f * Intensity, TransitionTime);
        SetActionUnitIntensity(4, 0.4f * Intensity, TransitionTime);
        SetActionUnitIntensity(5, 0.5f * Intensity, TransitionTime);
        SetActionUnitIntensity(20, 0.5f * Intensity, TransitionTime); // Lip stretcher
        ApplyPostureFromEmotion(TEXT("Fear"), Intensity);
    }
    else if (PresetName == TEXT("Disgusted"))
    {
        SetActionUnitIntensity(9, 0.7f * Intensity, TransitionTime);  // Nose wrinkler
        SetActionUnitIntensity(10, 0.5f * Intensity, TransitionTime); // Upper lip raiser
        ApplyPostureFromEmotion(TEXT("Disgust"), Intensity);
    }
}

void UExpressiveAnimationSystem::CreateExpressionFromEmotionBlend(const TMap<FString, float>& EmotionWeights)
{
    // Reset first
    for (auto& Pair : ActionUnits)
    {
        Pair.Value.Intensity = 0.0f;
    }

    // Blend expressions
    for (const auto& EmotionPair : EmotionWeights)
    {
        TArray<FFacialActionUnit> EmotionAUs = GetActionUnitsForEmotion(EmotionPair.Key, EmotionPair.Value);

        for (const FFacialActionUnit& AU : EmotionAUs)
        {
            if (ActionUnits.Contains(AU.AUNumber))
            {
                ActionUnits[AU.AUNumber].Intensity = FMath::Max(
                    ActionUnits[AU.AUNumber].Intensity,
                    AU.Intensity
                );
            }
        }
    }
}

float UExpressiveAnimationSystem::GetOverallExpressionIntensity() const
{
    float Total = 0.0f;
    int32 Count = 0;

    for (const auto& Pair : ActionUnits)
    {
        Total += Pair.Value.Intensity;
        Count++;
    }

    return (Count > 0) ? Total / Count : 0.0f;
}

void UExpressiveAnimationSystem::ResetAllExpressions(float TransitionTime)
{
    for (auto& Pair : ActionUnits)
    {
        Pair.Value.Intensity = 0.0f;
    }

    CurrentPosture = FBodyPostureState();
    TargetPosture = CurrentPosture;

    CurrentVocalParams = FVocalParameters();
    TargetVocalParams = CurrentVocalParams;

    CurrentBreathingState = FBreathingState();
    TargetBreathingState = CurrentBreathingState;
}

void UExpressiveAnimationSystem::UpdateFacialAnimation(float DeltaTime)
{
    // Apply morph targets to mesh
    if (AvatarMesh)
    {
        for (const auto& Pair : ActionUnits)
        {
            if (Pair.Value.Intensity > 0.01f)
            {
                AvatarMesh->SetMorphTarget(Pair.Value.MorphTarget, Pair.Value.Intensity);
            }
        }
    }
}

void UExpressiveAnimationSystem::UpdateBodyAnimation(float DeltaTime)
{
    // Interpolate toward target posture
    float Alpha = FMath::Clamp(DeltaTime / ExpressionSmoothingFactor, 0.0f, 1.0f);

    CurrentPosture.Openness = FMath::Lerp(CurrentPosture.Openness, TargetPosture.Openness, Alpha);
    CurrentPosture.Tension = FMath::Lerp(CurrentPosture.Tension, TargetPosture.Tension, Alpha);
    CurrentPosture.ForwardLean = FMath::Lerp(CurrentPosture.ForwardLean, TargetPosture.ForwardLean, Alpha);
    CurrentPosture.VerticalExpansion = FMath::Lerp(CurrentPosture.VerticalExpansion, TargetPosture.VerticalExpansion, Alpha);
    CurrentPosture.Symmetry = FMath::Lerp(CurrentPosture.Symmetry, TargetPosture.Symmetry, Alpha);
}

void UExpressiveAnimationSystem::UpdateGazeControl(float DeltaTime)
{
    // Update gaze target position
    if (CurrentGazeTarget.TargetActor)
    {
        CurrentGazeTarget.WorldLocation = CurrentGazeTarget.TargetActor->GetActorLocation();
    }

    // Compute gaze direction
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector ToTarget = CurrentGazeTarget.WorldLocation - Owner->GetActorLocation();
        ToTarget.Normalize();

        // Smooth interpolation
        float Alpha = FMath::Clamp(DeltaTime / GazeSmoothingFactor, 0.0f, 1.0f);
        CurrentGazeDirection = FMath::Lerp(CurrentGazeDirection, ToTarget, Alpha * CurrentGazeTarget.AttentionWeight);
    }
}

void UExpressiveAnimationSystem::UpdateVocalParameters(float DeltaTime)
{
    float Alpha = FMath::Clamp(DeltaTime * 5.0f, 0.0f, 1.0f);

    CurrentVocalParams.Pitch = FMath::Lerp(CurrentVocalParams.Pitch, TargetVocalParams.Pitch, Alpha);
    CurrentVocalParams.Volume = FMath::Lerp(CurrentVocalParams.Volume, TargetVocalParams.Volume, Alpha);
    CurrentVocalParams.SpeechRate = FMath::Lerp(CurrentVocalParams.SpeechRate, TargetVocalParams.SpeechRate, Alpha);
    CurrentVocalParams.Breathiness = FMath::Lerp(CurrentVocalParams.Breathiness, TargetVocalParams.Breathiness, Alpha);
    CurrentVocalParams.Tension = FMath::Lerp(CurrentVocalParams.Tension, TargetVocalParams.Tension, Alpha);
    CurrentVocalParams.Tremor = FMath::Lerp(CurrentVocalParams.Tremor, TargetVocalParams.Tremor, Alpha);
}

void UExpressiveAnimationSystem::UpdateBreathing(float DeltaTime)
{
    // Update breathing phase
    float CycleTime = 60.0f / CurrentBreathingState.Rate;
    CurrentBreathingState.Phase += DeltaTime / CycleTime;
    if (CurrentBreathingState.Phase >= 1.0f)
    {
        CurrentBreathingState.Phase -= 1.0f;
    }

    // Interpolate other parameters
    float Alpha = FMath::Clamp(DeltaTime, 0.0f, 1.0f);
    CurrentBreathingState.Rate = FMath::Lerp(CurrentBreathingState.Rate, TargetBreathingState.Rate, Alpha);
    CurrentBreathingState.Depth = FMath::Lerp(CurrentBreathingState.Depth, TargetBreathingState.Depth, Alpha);
    CurrentBreathingState.Regularity = FMath::Lerp(CurrentBreathingState.Regularity, TargetBreathingState.Regularity, Alpha);
}

void UExpressiveAnimationSystem::Update4EIntegration(float DeltaTime)
{
    // Compute integration coherence based on weight distribution
    float MaxWeight = FMath::Max(
        FMath::Max(Current4EState.EmbodiedWeight, Current4EState.EmbeddedWeight),
        FMath::Max(Current4EState.EnactedWeight, Current4EState.ExtendedWeight)
    );

    // Higher coherence when weights are more evenly distributed
    float Variance = 0.0f;
    float Mean = 0.25f;
    Variance += FMath::Square(Current4EState.EmbodiedWeight - Mean);
    Variance += FMath::Square(Current4EState.EmbeddedWeight - Mean);
    Variance += FMath::Square(Current4EState.EnactedWeight - Mean);
    Variance += FMath::Square(Current4EState.ExtendedWeight - Mean);

    Current4EState.IntegrationCoherence = 1.0f - FMath::Sqrt(Variance);
}

void UExpressiveAnimationSystem::ProcessAutonomicBehaviors(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // Automatic blinking
    if (CurrentTime >= NextBlinkTime)
    {
        TriggerBlink();
        NextBlinkTime = CurrentTime + FMath::FRandRange(BlinkIntervalRange.X, BlinkIntervalRange.Y);
    }

    // Automatic saccades
    if (bEnableGazeControl && CurrentTime >= NextSaccadeTime)
    {
        if (FMath::FRand() < CurrentGazeTarget.SaccadeProbability)
        {
            TriggerSaccade();
        }
        NextSaccadeTime = CurrentTime + FMath::FRandRange(SaccadeIntervalRange.X, SaccadeIntervalRange.Y);
    }
}

void UExpressiveAnimationSystem::BroadcastExpressionEvent(EExpressionChannel Channel, const FString& EventType, float Intensity)
{
    FExpressionEvent Event;
    Event.EventID = NextEventID++;
    Event.Channel = Channel;
    Event.EventType = EventType;
    Event.Intensity = Intensity;
    Event.Timestamp = GetWorld()->GetTimeSeconds();
    Event.EchobeatStep = CurrentEchobeatStep;

    OnExpressionChanged.Broadcast(Event);
}

FBodyPostureState UExpressiveAnimationSystem::GetPostureForEmotion(const FString& EmotionType, float Intensity) const
{
    FBodyPostureState Posture;
    Posture.PostureName = EmotionType;

    if (EmotionType == TEXT("Joy") || EmotionType == TEXT("Happy"))
    {
        Posture.Openness = 0.7f * Intensity;
        Posture.Tension = 0.2f;
        Posture.VerticalExpansion = 0.7f * Intensity;
        Posture.ForwardLean = 0.1f * Intensity;
    }
    else if (EmotionType == TEXT("Sadness") || EmotionType == TEXT("Sad"))
    {
        Posture.Openness = 0.2f;
        Posture.Tension = 0.3f;
        Posture.VerticalExpansion = 0.3f;
        Posture.ForwardLean = -0.1f * Intensity;
    }
    else if (EmotionType == TEXT("Anger") || EmotionType == TEXT("Angry"))
    {
        Posture.Openness = 0.4f;
        Posture.Tension = 0.8f * Intensity;
        Posture.VerticalExpansion = 0.6f;
        Posture.ForwardLean = 0.3f * Intensity;
    }
    else if (EmotionType == TEXT("Fear") || EmotionType == TEXT("Fearful"))
    {
        Posture.Openness = 0.2f;
        Posture.Tension = 0.7f * Intensity;
        Posture.VerticalExpansion = 0.4f;
        Posture.ForwardLean = -0.2f * Intensity;
    }
    else if (EmotionType == TEXT("Surprise") || EmotionType == TEXT("Surprised"))
    {
        Posture.Openness = 0.8f * Intensity;
        Posture.Tension = 0.4f;
        Posture.VerticalExpansion = 0.8f * Intensity;
        Posture.ForwardLean = 0.0f;
    }
    else if (EmotionType == TEXT("Disgust") || EmotionType == TEXT("Disgusted"))
    {
        Posture.Openness = 0.3f;
        Posture.Tension = 0.5f;
        Posture.VerticalExpansion = 0.5f;
        Posture.ForwardLean = -0.1f * Intensity;
    }

    return Posture;
}

FVocalParameters UExpressiveAnimationSystem::GetVocalForEmotion(const FString& EmotionType, float Intensity) const
{
    FVocalParameters Params;

    if (EmotionType == TEXT("Joy") || EmotionType == TEXT("Happy"))
    {
        Params.Pitch = 170.0f + 30.0f * Intensity;
        Params.PitchVariation = 30.0f * Intensity;
        Params.Volume = 0.7f + 0.1f * Intensity;
        Params.SpeechRate = 130.0f + 20.0f * Intensity;
        Params.Breathiness = 0.1f;
    }
    else if (EmotionType == TEXT("Sadness") || EmotionType == TEXT("Sad"))
    {
        Params.Pitch = 130.0f - 20.0f * Intensity;
        Params.PitchVariation = 10.0f;
        Params.Volume = 0.5f - 0.1f * Intensity;
        Params.SpeechRate = 100.0f - 20.0f * Intensity;
        Params.Breathiness = 0.3f * Intensity;
    }
    else if (EmotionType == TEXT("Anger") || EmotionType == TEXT("Angry"))
    {
        Params.Pitch = 160.0f + 20.0f * Intensity;
        Params.PitchVariation = 40.0f * Intensity;
        Params.Volume = 0.8f + 0.2f * Intensity;
        Params.SpeechRate = 140.0f + 30.0f * Intensity;
        Params.Tension = 0.7f * Intensity;
    }
    else if (EmotionType == TEXT("Fear") || EmotionType == TEXT("Fearful"))
    {
        Params.Pitch = 180.0f + 40.0f * Intensity;
        Params.PitchVariation = 50.0f * Intensity;
        Params.Volume = 0.6f;
        Params.SpeechRate = 150.0f + 40.0f * Intensity;
        Params.Tremor = 0.4f * Intensity;
    }
    else if (EmotionType == TEXT("Surprise") || EmotionType == TEXT("Surprised"))
    {
        Params.Pitch = 190.0f + 50.0f * Intensity;
        Params.PitchVariation = 60.0f * Intensity;
        Params.Volume = 0.8f;
        Params.SpeechRate = 160.0f;
    }

    return Params;
}

TArray<FFacialActionUnit> UExpressiveAnimationSystem::GetActionUnitsForEmotion(const FString& EmotionType, float Intensity) const
{
    TArray<FFacialActionUnit> Result;

    if (EmotionType == TEXT("Joy") || EmotionType == TEXT("Happy"))
    {
        Result.Add({6, TEXT("Cheek Raiser"), 0.7f * Intensity, 0.0f, TEXT("AU06_CheekRaiser")});
        Result.Add({12, TEXT("Lip Corner Puller"), 0.8f * Intensity, 0.0f, TEXT("AU12_LipCornerPuller")});
    }
    else if (EmotionType == TEXT("Sadness") || EmotionType == TEXT("Sad"))
    {
        Result.Add({1, TEXT("Inner Brow Raiser"), 0.5f * Intensity, 0.0f, TEXT("AU01_InnerBrowRaiser")});
        Result.Add({4, TEXT("Brow Lowerer"), 0.3f * Intensity, 0.0f, TEXT("AU04_BrowLowerer")});
        Result.Add({15, TEXT("Lip Corner Depressor"), 0.6f * Intensity, 0.0f, TEXT("AU15_LipCornerDepressor")});
    }
    else if (EmotionType == TEXT("Anger") || EmotionType == TEXT("Angry"))
    {
        Result.Add({4, TEXT("Brow Lowerer"), 0.7f * Intensity, 0.0f, TEXT("AU04_BrowLowerer")});
        Result.Add({5, TEXT("Upper Lid Raiser"), 0.4f * Intensity, 0.0f, TEXT("AU05_UpperLidRaiser")});
        Result.Add({7, TEXT("Lid Tightener"), 0.5f * Intensity, 0.0f, TEXT("AU07_LidTightener")});
        Result.Add({23, TEXT("Lip Tightener"), 0.4f * Intensity, 0.0f, TEXT("AU23_LipTightener")});
    }
    else if (EmotionType == TEXT("Fear") || EmotionType == TEXT("Fearful"))
    {
        Result.Add({1, TEXT("Inner Brow Raiser"), 0.6f * Intensity, 0.0f, TEXT("AU01_InnerBrowRaiser")});
        Result.Add({2, TEXT("Outer Brow Raiser"), 0.6f * Intensity, 0.0f, TEXT("AU02_OuterBrowRaiser")});
        Result.Add({4, TEXT("Brow Lowerer"), 0.4f * Intensity, 0.0f, TEXT("AU04_BrowLowerer")});
        Result.Add({5, TEXT("Upper Lid Raiser"), 0.5f * Intensity, 0.0f, TEXT("AU05_UpperLidRaiser")});
        Result.Add({20, TEXT("Lip Stretcher"), 0.5f * Intensity, 0.0f, TEXT("AU20_LipStretcher")});
    }
    else if (EmotionType == TEXT("Surprise") || EmotionType == TEXT("Surprised"))
    {
        Result.Add({1, TEXT("Inner Brow Raiser"), 0.8f * Intensity, 0.0f, TEXT("AU01_InnerBrowRaiser")});
        Result.Add({2, TEXT("Outer Brow Raiser"), 0.8f * Intensity, 0.0f, TEXT("AU02_OuterBrowRaiser")});
        Result.Add({5, TEXT("Upper Lid Raiser"), 0.7f * Intensity, 0.0f, TEXT("AU05_UpperLidRaiser")});
        Result.Add({26, TEXT("Jaw Drop"), 0.6f * Intensity, 0.0f, TEXT("AU26_JawDrop")});
    }
    else if (EmotionType == TEXT("Disgust") || EmotionType == TEXT("Disgusted"))
    {
        Result.Add({9, TEXT("Nose Wrinkler"), 0.7f * Intensity, 0.0f, TEXT("AU09_NoseWrinkler")});
        Result.Add({10, TEXT("Upper Lip Raiser"), 0.5f * Intensity, 0.0f, TEXT("AU10_UpperLipRaiser")});
    }

    return Result;
}
