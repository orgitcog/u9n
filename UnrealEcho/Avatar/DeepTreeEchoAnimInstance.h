#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "DeepTreeEchoAnimInstance.generated.h"

class UAudioManagerComponent;

/**
 * Deep-Tree-Echo Animation Instance
 *
 * Animation Blueprint base class for the Deep-Tree-Echo avatar
 * Integrates with the expression system for facial animation and procedural movement
 *
 * Speaking state is read from real speech playback (UAudioManagerComponent::IsSpeaking on the
 * owner, or an explicit mouth amplitude fed to the expression system by whoever plays audio).
 * No talking animation runs while nothing is being heard.
 */
UCLASS()
class DEEPTREECHO_API UDeepTreeEchoAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UDeepTreeEchoAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // ========== Expression System Integration ==========

    /** Reference to the expression system component */
    UPROPERTY(BlueprintReadOnly, Category = "Expression")
    UDeepTreeEchoExpressionSystem* ExpressionSystem;

    /** Current morph targets from expression system */
    UPROPERTY(BlueprintReadOnly, Category = "Expression")
    FDeepTreeEchoMorphTargets CurrentMorphTargets;

    /** Current expression state */
    UPROPERTY(BlueprintReadOnly, Category = "Expression")
    EExpressionState CurrentExpressionState;

    /** Current cognitive state */
    UPROPERTY(BlueprintReadOnly, Category = "Expression")
    ECognitiveState CurrentCognitiveState;

    // ========== Eye/Gaze Control ==========

    /** Eye look-at rotation (for bone modification) */
    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    FRotator EyeRotationL;

    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    FRotator EyeRotationR;

    /** Head look-at rotation */
    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    FRotator HeadRotation;

    /** Gaze blend weight (0 = forward, 1 = full look-at) */
    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    float GazeBlendWeight;

    // ========== Procedural Animation Values ==========

    /** Breathing offset for spine/chest bones */
    UPROPERTY(BlueprintReadOnly, Category = "Procedural")
    float BreathingOffset;

    /** Head sway offset */
    UPROPERTY(BlueprintReadOnly, Category = "Procedural")
    FVector2D HeadSwayOffset;

    /** True only while speech audio is actually playing (or an explicit mouth amplitude is live). */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    bool bIsSpeaking;

    /** Mouth-open amplitude (0-1) while speaking; 0 otherwise. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    float SpeakingAmplitude;

    // ========== State Machine Variables ==========

    /** Is in idle state */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsIdle;

    /** Is in talking state: identical to bIsSpeaking, never derived from a cognitive phase alone. */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsTalking;

    /** Is in thinking state */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsThinking;

    /** Is in engaged state */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsEngaged;

    // ========== Emissive/Tech Elements ==========

    /** Current emissive intensity for tech elements */
    UPROPERTY(BlueprintReadOnly, Category = "Emissive")
    float EmissiveIntensity;

    /** Emissive pulse active */
    UPROPERTY(BlueprintReadOnly, Category = "Emissive")
    bool bEmissivePulseActive;

protected:
    /** Update expression-related animation values */
    void UpdateExpressionValues();

    /** Update gaze/look-at values */
    void UpdateGazeValues(float DeltaSeconds);

    /** Update procedural animation values */
    void UpdateProceduralValues(float DeltaSeconds);

    /** Update state machine variables */
    void UpdateStateVariables();

    /** Resolve the owner's expression system and audio manager when they are not cached yet. */
    void ResolveOwnerComponents();

private:
    /** Cached owner character */
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    /** Speech playback source on the owner; the only thing that makes the avatar "speak". */
    UPROPERTY()
    UAudioManagerComponent* AudioManager;

    /** Breathing phase accumulator */
    float BreathingPhase;

    /** Head sway phase accumulator */
    float HeadSwayPhase;

    /** Gaze interpolation speed */
    float GazeInterpSpeed;
};

/**
 * Animation Notify: refresh the face at a beat of the animation.
 *
 * The notify decides when the face is re-evaluated, never what it shows. With an
 * EventDescription the language model is asked which expression fits the event this
 * moment of the animation represents (UDeepTreeEchoExpressionSystem::RequestExpressionForEvent);
 * otherwise, or when no model bridge is available, the face is re-derived from the owner's
 * measured affect (RefreshExpressionFromAffect).
 */
UCLASS(meta = (DisplayName = "DTE Refresh Expression"))
class DEEPTREECHO_API UAnimNotify_RefreshExpression : public UAnimNotify
{
    GENERATED_BODY()

public:
    /** What this moment of the animation represents (e.g. "notices the visitor"). Empty: affect only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FString EventDescription;

    /** Transition time used when the face is re-derived from affect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    float TransitionTime = 0.3f;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Animation Notify: blink on a beat of the animation.
 *
 * Blinking is autonomic timing, not a reaction, so synchronising it with an animation is
 * allowed. No other microexpression can be placed from an animation asset: which one fits
 * is chosen by the model or derived from affect.
 */
UCLASS(meta = (DisplayName = "DTE Blink"))
class DEEPTREECHO_API UAnimNotify_Blink : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Animation Notify State: the face follows an event for the span of the notify.
 *
 * On begin the model is asked which expression fits EventDescription (falling back to an
 * affect refresh when no bridge is available); on end the face returns to the measured
 * affect. Replaces fixed choreographies such as the former wonder-to-joy sequence.
 */
UCLASS(meta = (DisplayName = "DTE Expression For Event"))
class DEEPTREECHO_API UAnimNotifyState_ExpressionForEvent : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    /** The event this span of the animation represents (e.g. "a realisation dawns"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FString EventDescription;

    /** Transition time back to the affect-derived face when the span ends. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    float ReturnTransitionTime = 0.5f;

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Animation Notify State: mouth-open span of a speech animation.
 *
 * Feeds the expression system a speaking amplitude for the span, but only while the owner's
 * UAudioManagerComponent is actually playing speech. With no audio the mouth stays closed.
 */
UCLASS(meta = (DisplayName = "DTE Speech Mouth Sync"))
class DEEPTREECHO_API UAnimNotifyState_SpeechMouthSync : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    /** Mouth-open amplitude (0-1) to hold while speech audio plays during this span. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Amplitude = 0.6f;

    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
};
