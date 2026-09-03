#include "DeepTreeEchoAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UDeepTreeEchoAnimInstance::UDeepTreeEchoAnimInstance()
{
    ExpressionSystem = nullptr;
    OwnerCharacter = nullptr;

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

    if (OwnerCharacter)
    {
        // Find expression system component
        ExpressionSystem = OwnerCharacter->FindComponentByClass<UDeepTreeEchoExpressionSystem>();

        if (ExpressionSystem)
        {
            UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoAnimInstance: Expression system found"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoAnimInstance: No expression system found on owner"));
        }
    }
}

void UDeepTreeEchoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerCharacter)
    {
        OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
        if (OwnerCharacter && !ExpressionSystem)
        {
            ExpressionSystem = OwnerCharacter->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
        }
    }

    UpdateExpressionValues();
    UpdateGazeValues(DeltaSeconds);
    UpdateProceduralValues(DeltaSeconds);
    UpdateStateVariables();
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
    // Update state machine variables based on cognitive state
    bIsIdle = (CurrentCognitiveState == ECognitiveState::Idle);
    bIsTalking = (CurrentCognitiveState == ECognitiveState::Communicating);
    bIsThinking = (CurrentCognitiveState == ECognitiveState::Processing || 
                   CurrentCognitiveState == ECognitiveState::Reasoning ||
                   CurrentCognitiveState == ECognitiveState::Reflecting);
    bIsEngaged = (CurrentCognitiveState == ECognitiveState::Engaging ||
                  CurrentCognitiveState == ECognitiveState::Listening);

    // Update speaking state based on expression
    bIsSpeaking = (CurrentExpressionState == EExpressionState::SpeakingVowel ||
                   CurrentExpressionState == EExpressionState::SpeakingConsonant);
    
    // Get speaking amplitude from morph targets
    SpeakingAmplitude = CurrentMorphTargets.MouthOpen;
}

// ========== Animation Notifies ==========

void UAnimNotify_TriggerExpression::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp)
        return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
        return;

    UDeepTreeEchoExpressionSystem* ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    if (ExpressionSystem)
    {
        ExpressionSystem->SetExpressionState(TargetExpression, TransitionTime);
    }
}

void UAnimNotify_TriggerMicroExpression::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp)
        return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
        return;

    UDeepTreeEchoExpressionSystem* ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    if (ExpressionSystem)
    {
        ExpressionSystem->TriggerMicroExpression(MicroExpressionType, Duration);
    }
}

void UAnimNotifyState_WonderToJoy::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp)
        return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
        return;

    UDeepTreeEchoExpressionSystem* ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    if (ExpressionSystem)
    {
        ExpressionSystem->TriggerWonderToJoyTransition(TotalDuration);
    }
}

void UAnimNotifyState_WonderToJoy::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    // The expression system handles the completion internally
}
