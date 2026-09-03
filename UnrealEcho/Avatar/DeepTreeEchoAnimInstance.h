#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "DeepTreeEchoAnimInstance.generated.h"

/**
 * Deep-Tree-Echo Animation Instance
 * 
 * Animation Blueprint base class for the Deep-Tree-Echo avatar
 * Integrates with the expression system for facial animation and procedural movement
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

    /** Is currently speaking */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    bool bIsSpeaking;

    /** Speaking amplitude (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    float SpeakingAmplitude;

    // ========== State Machine Variables ==========

    /** Is in idle state */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsIdle;

    /** Is in talking state */
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

private:
    /** Cached owner character */
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    /** Breathing phase accumulator */
    float BreathingPhase;

    /** Head sway phase accumulator */
    float HeadSwayPhase;

    /** Gaze interpolation speed */
    float GazeInterpSpeed;
};

/**
 * Animation Notify for expression triggers
 */
UCLASS()
class DEEPTREECHO_API UAnimNotify_TriggerExpression : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    EExpressionState TargetExpression;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    float TransitionTime = 0.3f;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Animation Notify for microexpression triggers
 */
UCLASS()
class DEEPTREECHO_API UAnimNotify_TriggerMicroExpression : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Microexpression")
    EMicroExpression MicroExpressionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Microexpression")
    float Duration = 0.2f;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Animation Notify State for Wonder-to-Joy transition
 */
UCLASS()
class DEEPTREECHO_API UAnimNotifyState_WonderToJoy : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
