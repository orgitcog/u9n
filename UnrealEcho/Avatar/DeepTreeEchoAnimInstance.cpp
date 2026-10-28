#include "DeepTreeEchoAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Audio/AudioManagerComponent.h"

namespace
{
    UDeepTreeEchoExpressionSystem* FindExpressionSystem(USkeletalMeshComponent* MeshComp)
    {
        AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
        return Owner ? Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>() : nullptr;
    }

    UAudioManagerComponent* FindAudioManager(USkeletalMeshComponent* MeshComp)
    {
        AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
        return Owner ? Owner->FindComponentByClass<UAudioManagerComponent>() : nullptr;
    }

    /**
     * Ask the model for the face that fits EventDescription; when that is not possible, or
     * no event is given, re-derive the face from affect. The notify never names a face.
     */
    void RefreshFace(UDeepTreeEchoExpressionSystem* ExpressionSystem, const FString& EventDescription, float TransitionTime)
    {
        if (!ExpressionSystem)
        {
            return;
        }

        const FString Event = EventDescription.TrimStartAndEnd();
        if (!Event.IsEmpty() && ExpressionSystem->RequestExpressionForEvent(Event))
        {
            return;
        }

        ExpressionSystem->RefreshExpressionFromAffect(TransitionTime);
    }
}

UDeepTreeEchoAnimInstance::UDeepTreeEchoAnimInstance()
{
    ExpressionSystem = nullptr;
    OwnerCharacter = nullptr;
    AudioManager = nullptr;

    CurrentExpressionState = EExpressionState::Neutral;
    CurrentCognitiveState = ECognitiveState::Idle;

    EyeRotationL = FRotator::ZeroRotator;
    EyeRotationR = FRotator::ZeroRotator;
    HeadRotation = FRotator::ZeroRotator;
    GazeBlendWeight = 0.0f;

    BreathingOffset = 0.0f;
    HeadSwayOffset = FVector2D::ZeroVector;

    bIsSpeaking = false;
    SpeakingAmplitude = 0.0f;

    bIsIdle = true;
    bIsTalking = false;
    bIsThinking = false;
    bIsEngaged = false;

    EmissiveIntensity = 1.0f;
    bEmissivePulseActive = false;

    BreathingPhase = 0.0f;
    HeadSwayPhase = 0.0f;
    GazeInterpSpeed = 5.0f;
}

void UDeepTreeEchoAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // Get owner character
    OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());

    ResolveOwnerComponents();

    if (OwnerCharacter)
    {
        if (ExpressionSystem)
        {
            UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoAnimInstance: Expression system found"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoAnimInstance: No expression system found on owner"));
        }

        if (!AudioManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoAnimInstance: No audio manager on owner; the avatar will never enter the talking state"));
        }
    }
}

void UDeepTreeEchoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerCharacter)
    {
        OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
    }
    ResolveOwnerComponents();

    UpdateExpressionValues();
    UpdateGazeValues(DeltaSeconds);
    UpdateProceduralValues(DeltaSeconds);
    UpdateStateVariables();
}

void UDeepTreeEchoAnimInstance::ResolveOwnerComponents()
{
    if (!OwnerCharacter)
    {
        return;
    }

    if (!ExpressionSystem)
    {
        ExpressionSystem = OwnerCharacter->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    }

    if (!AudioManager)
    {
        AudioManager = OwnerCharacter->FindComponentByClass<UAudioManagerComponent>();
    }
}

void UDeepTreeEchoAnimInstance::UpdateExpressionValues()
{
    if (ExpressionSystem)
    {
        // Get current morph targets from expression system
        CurrentMorphTargets = ExpressionSystem->GetCurrentMorphTargets();
        CurrentExpressionState = ExpressionSystem->GetCurrentExpressionState();
        CurrentCognitiveState = ExpressionSystem->GetCurrentCognitiveState();
    }
}

void UDeepTreeEchoAnimInstance::UpdateGazeValues(float DeltaSeconds)
{
    // Gaze is primarily handled by the expression system's saccade logic
    // Here we just interpolate for smooth animation

    // For now, use a simple forward gaze with slight variation
    FRotator TargetEyeRotation = FRotator::ZeroRotator;

    // Apply saccade offset from expression system (if available)
    // This would be extended to support actual look-at targets

    EyeRotationL = FMath::RInterpTo(EyeRotationL, TargetEyeRotation, DeltaSeconds, GazeInterpSpeed);
    EyeRotationR = FMath::RInterpTo(EyeRotationR, TargetEyeRotation, DeltaSeconds, GazeInterpSpeed);

    // Head follows eyes with lag
    HeadRotation = FMath::RInterpTo(HeadRotation, TargetEyeRotation * 0.3f, DeltaSeconds, GazeInterpSpeed * 0.5f);
}

void UDeepTreeEchoAnimInstance::UpdateProceduralValues(float DeltaSeconds)
{
    // Breathing and head sway are autonomic motion, not reactions; their timing is fixed.

    // Breathing animation
    const float BreathingRate = 0.2f;  // Breaths per second
    const float BreathingAmplitude = 0.02f;

    BreathingPhase += DeltaSeconds * BreathingRate * 2.0f * PI;
    if (BreathingPhase > 2.0f * PI)
    {
        BreathingPhase -= 2.0f * PI;
    }
    BreathingOffset = FMath::Sin(BreathingPhase) * BreathingAmplitude;

    // Head sway (subtle idle movement)
    const float HeadSwayRate = 0.1f;
    const float HeadSwayAmplitude = 1.0f;  // Degrees

    HeadSwayPhase += DeltaSeconds * HeadSwayRate * 2.0f * PI;
    if (HeadSwayPhase > 2.0f * PI)
    {
        HeadSwayPhase -= 2.0f * PI;
    }

    // Use different frequencies for X and Y to create natural-looking movement
    HeadSwayOffset.X = FMath::Sin(HeadSwayPhase) * HeadSwayAmplitude;
    HeadSwayOffset.Y = FMath::Sin(HeadSwayPhase * 0.7f + 0.5f) * HeadSwayAmplitude * 0.5f;
}

void UDeepTreeEchoAnimInstance::UpdateStateVariables()
{
    // Speaking is real speech playback only: audio from the owner's audio manager, or an
    // explicit mouth amplitude fed to the expression system by whoever is playing audio.
    // The Speaking* expression states are not consulted; they are an output of the same
    // playback, never a source.
    const bool bAudioPlaying = AudioManager && AudioManager->IsSpeaking();
    const bool bExplicitMouthLive = ExpressionSystem && ExpressionSystem->IsSpeechPlaying();
    bIsSpeaking = bAudioPlaying || bExplicitMouthLive;

    // Mouth-open amplitude is only meaningful while speaking; otherwise an expression's
    // open mouth (awe, laughter) must not read as speech.
    SpeakingAmplitude = bIsSpeaking ? FMath::Clamp(CurrentMorphTargets.MouthOpen, 0.0f, 1.0f) : 0.0f;

    // Talking animation runs only while something is being heard
    bIsTalking = bIsSpeaking;

    // Other state machine variables follow the cognitive state
    bIsThinking = (CurrentCognitiveState == ECognitiveState::Processing ||
                   CurrentCognitiveState == ECognitiveState::Reasoning ||
                   CurrentCognitiveState == ECognitiveState::Reflecting);

    // A communicating phase without audio is an engaged (listening) pose, not a talking one
    bIsEngaged = !bIsTalking &&
                 (CurrentCognitiveState == ECognitiveState::Engaging ||
                  CurrentCognitiveState == ECognitiveState::Listening ||
                  CurrentCognitiveState == ECognitiveState::Communicating);

    bIsIdle = !bIsTalking && !bIsThinking && !bIsEngaged;
}

// ========== Animation Notifies ==========

void UAnimNotify_RefreshExpression::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    RefreshFace(FindExpressionSystem(MeshComp), EventDescription, TransitionTime);
}

void UAnimNotify_Blink::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (UDeepTreeEchoExpressionSystem* ExpressionSystem = FindExpressionSystem(MeshComp))
    {
        ExpressionSystem->TriggerBlink();
    }
}

void UAnimNotifyState_ExpressionForEvent::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    RefreshFace(FindExpressionSystem(MeshComp), EventDescription, ReturnTransitionTime);
}

void UAnimNotifyState_ExpressionForEvent::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    // The event's span is over: back to the measured affect
    if (UDeepTreeEchoExpressionSystem* ExpressionSystem = FindExpressionSystem(MeshComp))
    {
        ExpressionSystem->RefreshExpressionFromAffect(ReturnTransitionTime);
    }
}

void UAnimNotifyState_SpeechMouthSync::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
    UDeepTreeEchoExpressionSystem* ExpressionSystem = FindExpressionSystem(MeshComp);
    UAudioManagerComponent* AudioManager = FindAudioManager(MeshComp);

    // Only real playback opens the mouth; the span itself proves nothing is being said.
    if (ExpressionSystem && AudioManager && AudioManager->IsSpeaking())
    {
        ExpressionSystem->SetSpeakingAmplitude(FMath::Clamp(Amplitude, 0.0f, 1.0f));
    }
}
