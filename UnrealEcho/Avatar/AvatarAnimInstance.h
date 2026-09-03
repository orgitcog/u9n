// Avatar Animation Instance - Production Implementation
// Deep Tree Echo AGI Avatar System
// Main animation instance class for ABP_DeepTreeEcho_Avatar

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Avatar3DComponentEnhanced.h"
#include "AvatarAnimInstance.generated.h"

/**
 * Locomotion state for state machine
 */
UENUM(BlueprintType)
enum class ELocomotionState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Walk UMETA(DisplayName = "Walk"),
    Run UMETA(DisplayName = "Run"),
    Jump UMETA(DisplayName = "Jump"),
    Fall UMETA(DisplayName = "Fall"),
    Land UMETA(DisplayName = "Land")
};

/**
 * Upper body gesture state
 */
UENUM(BlueprintType)
enum class EGestureState : uint8
{
    None UMETA(DisplayName = "None"),
    Talking UMETA(DisplayName = "Talking"),
    Thinking UMETA(DisplayName = "Thinking"),
    Excited UMETA(DisplayName = "Excited"),
    Confident UMETA(DisplayName = "Confident"),
    Flirty UMETA(DisplayName = "Flirty"),
    Chaotic UMETA(DisplayName = "Chaotic")
};

/**
 * Facial blend shape weights
 */
USTRUCT(BlueprintType)
struct FFacialBlendShapes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float BrowInnerUp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float BrowOuterUp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float BrowDown = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float EyeWide = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float EyeSquint = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float MouthSmile = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float MouthFrown = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float MouthOpen = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float CheekPuff = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float JawOpen = 0.0f;
};

/**
 * Main Avatar Animation Instance
 * Drives all animation layers for Deep Tree Echo avatar
 */
UCLASS()
class DEEPTREECHO_API UAvatarAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UAvatarAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // ===== State Variables (Blueprint Accessible) =====

    /** Current locomotion state */
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    ELocomotionState LocomotionState;

    /** Movement speed */
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float Speed;

    /** Movement direction relative to actor forward */
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float Direction;

    /** Is character in air */
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsInAir;

    /** Is character on ground */
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsOnGround;

    // ===== Emotional State =====

    /** Current emotional state */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    EAvatarEmotionalState CurrentEmotion;

    /** Emotion intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float EmotionIntensity;

    /** Emotional valence (-1 to 1, negative to positive) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float EmotionalValence;

    /** Emotional arousal (0-1, calm to excited) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float EmotionalArousal;

    // ===== Personality Traits =====

    /** Confidence level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Personality")
    float ConfidenceLevel;

    /** Flirtiness level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Personality")
    float FlirtinessLevel;

    /** Chaos factor (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Personality")
    float ChaosFactor;

    /** Playfulness level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Personality")
    float PlayfulnessLevel;

    // ===== Gesture State =====

    /** Current upper body gesture */
    UPROPERTY(BlueprintReadOnly, Category = "Gesture")
    EGestureState CurrentGesture;

    /** Gesture blend weight */
    UPROPERTY(BlueprintReadOnly, Category = "Gesture")
    float GestureBlendWeight;

    // ===== Facial Animation =====

    /** Current facial blend shapes */
    UPROPERTY(BlueprintReadOnly, Category = "Facial")
    FFacialBlendShapes FacialBlendShapes;

    /** Target facial blend shapes */
    UPROPERTY()
    FFacialBlendShapes TargetFacialBlendShapes;

    // ===== Gaze and Attention =====

    /** Gaze target in world space */
    UPROPERTY(BlueprintReadWrite, Category = "Gaze")
    FVector GazeTargetLocation;

    /** Enable gaze tracking */
    UPROPERTY(BlueprintReadWrite, Category = "Gaze")
    bool bEnableGazeTracking;

    /** Eye look-at alpha */
    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    float EyeLookAtAlpha;

    /** Head look-at alpha */
    UPROPERTY(BlueprintReadOnly, Category = "Gaze")
    float HeadLookAtAlpha;

    // ===== Deep Tree Echo Effects =====

    /** Echo resonance intensity */
    UPROPERTY(BlueprintReadWrite, Category = "Effects")
    float EchoResonance;

    /** Cognitive load visualization */
    UPROPERTY(BlueprintReadWrite, Category = "Effects")
    float CognitiveLoad;

    /** Glitch effect intensity */
    UPROPERTY(BlueprintReadWrite, Category = "Effects")
    float GlitchIntensity;

    // ===== Procedural Animation =====

    /** Breathing cycle (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Procedural")
    float BreathingCycle;

    /** Breathing rate (breaths per minute) */
    UPROPERTY(BlueprintReadWrite, Category = "Procedural")
    float BreathingRate;

    /** Idle sway amount */
    UPROPERTY(BlueprintReadOnly, Category = "Procedural")
    FVector2D IdleSway;

    // ===== Public Functions =====

    /** Update emotional state */
    UFUNCTION(BlueprintCallable, Category = "Animation|Emotion")
    void UpdateEmotionalState(EAvatarEmotionalState Emotion, float Intensity);

    /** Set personality traits */
    UFUNCTION(BlueprintCallable, Category = "Animation|Personality")
    void SetPersonalityTraits(float Confidence, float Flirtiness, float Chaos);

    /** Trigger gesture */
    UFUNCTION(BlueprintCallable, Category = "Animation|Gesture")
    void TriggerGesture(EGestureState Gesture, float Duration = 2.0f);

    /** Set gaze target */
    UFUNCTION(BlueprintCallable, Category = "Animation|Gaze")
    void SetGazeTarget(const FVector& Target);

    /** Apply chaotic behavior */
    UFUNCTION(BlueprintCallable, Category = "Animation|Chaos")
    void ApplyChaoticBehavior(float Intensity);

protected:
    // ===== Internal State =====

    UPROPERTY()
    APawn* OwningPawn;

    UPROPERTY()
    UCharacterMovementComponent* MovementComponent;

    // Gesture timing
    float GestureTimer;
    float GestureDuration;

    // Facial animation smoothing
    float FacialBlendSpeed;

    // Chaos state
    float ChaosTimer;
    float NextChaoticEventTime;

    // Breathing state
    float BreathingTime;

    // Idle sway state
    float IdleSwayTime;

    // ===== Internal Update Functions =====

    void UpdateLocomotionState(float DeltaTime);
    void UpdateGestureState(float DeltaTime);
    void UpdateFacialAnimation(float DeltaTime);
    void UpdateGazeTracking(float DeltaTime);
    void UpdateProceduralAnimation(float DeltaTime);
    void UpdateChaoticBehavior(float DeltaTime);

    // ===== Helper Functions =====

    FFacialBlendShapes CalculateEmotionalBlendShapes(EAvatarEmotionalState Emotion) const;
    void BlendFacialShapes(const FFacialBlendShapes& Target, float DeltaTime);
    float CalculateBreathingRate() const;
    void TriggerRandomChaoticEvent();
};
