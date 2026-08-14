#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FacialAnimationSystem.generated.h"

UENUM(BlueprintType)
enum class EEmotionState : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Happy UMETA(DisplayName = "Happy"),
    Sad UMETA(DisplayName = "Sad"),
    Angry UMETA(DisplayName = "Angry"),
    Surprised UMETA(DisplayName = "Surprised"),
    Disgusted UMETA(DisplayName = "Disgusted"),
    Fearful UMETA(DisplayName = "Fearful"),
    Flirty UMETA(DisplayName = "Flirty"),
    Playful UMETA(DisplayName = "Playful"),
    Confident UMETA(DisplayName = "Confident")
};

USTRUCT(BlueprintType)
struct FFacialBlendShapes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float EyeBlinkLeft = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float EyeBlinkRight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float EyeWideLeft = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float EyeWideRight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float BrowInnerUp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float BrowOuterUpLeft = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float BrowOuterUpRight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float MouthSmileLeft = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float MouthSmileRight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float MouthOpen = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float JawOpen = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Animation")
    float CheekPuff = 0.0f;
};

USTRUCT(BlueprintType)
struct FPhonemeData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
    FName PhonemeName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
    float MouthOpenAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
    float LipShapeValue = 0.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UFacialAnimationSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UFacialAnimationSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Set the current emotion state */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void SetEmotionState(EEmotionState NewEmotion, float TransitionTime = 0.3f);

    /** Get the current emotion state */
    UFUNCTION(BlueprintPure, Category = "Facial Animation")
    EEmotionState GetCurrentEmotion() const { return CurrentEmotion; }

    /** Trigger a micro-expression */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void TriggerMicroExpression(EEmotionState MicroEmotion, float Duration = 0.2f);

    /** Set gaze target for eye tracking */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void SetGazeTarget(const FVector& WorldTarget);

    /** Enable/disable automatic blinking */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void SetAutoBlinkEnabled(bool bEnabled);

    /** Trigger manual blink */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void TriggerBlink();

    /** Set phoneme for lip sync */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void SetPhoneme(const FName& PhonemeName, float Intensity = 1.0f);

    /** Get current blend shapes */
    UFUNCTION(BlueprintPure, Category = "Facial Animation")
    FFacialBlendShapes GetBlendShapes() const { return CurrentBlendShapes; }

    /** Apply blend shapes to skeletal mesh */
    UFUNCTION(BlueprintCallable, Category = "Facial Animation")
    void ApplyBlendShapesToMesh(class USkeletalMeshComponent* TargetMesh);

private:
    /** Initialize emotion presets */
    void InitializeEmotionPresets();

    /** Initialize phoneme mappings */
    void InitializePhonemeMappings();

    /** Update emotion transition */
    void UpdateEmotionTransition(float DeltaTime);

    /** Update automatic blinking */
    void UpdateAutoBlinking(float DeltaTime);

    /** Update gaze tracking */
    void UpdateGazeTracking(float DeltaTime);

    /** Update micro-expressions */
    void UpdateMicroExpressions(float DeltaTime);

    /** Interpolate blend shapes */
    FFacialBlendShapes InterpolateBlendShapes(const FFacialBlendShapes& From, const FFacialBlendShapes& To, float Alpha);

    /** Current emotion state */
    UPROPERTY()
    EEmotionState CurrentEmotion;

    /** Target emotion state for transitions */
    UPROPERTY()
    EEmotionState TargetEmotion;

    /** Emotion transition progress */
    float EmotionTransitionProgress;

    /** Emotion transition duration */
    float EmotionTransitionDuration;

    /** Current blend shapes */
    UPROPERTY()
    FFacialBlendShapes CurrentBlendShapes;

    /** Target blend shapes for transitions */
    UPROPERTY()
    FFacialBlendShapes TargetBlendShapes;

    /** Emotion preset mappings */
    TMap<EEmotionState, FFacialBlendShapes> EmotionPresets;

    /** Phoneme mappings */
    TMap<FName, FPhonemeData> PhonemeMappings;

    /** Auto-blink enabled */
    UPROPERTY(EditAnywhere, Category = "Facial Animation")
    bool bAutoBlinkEnabled = true;

    /** Blink timer */
    float BlinkTimer;

    /** Next blink time */
    float NextBlinkTime;

    /** Is currently blinking */
    bool bIsBlinking;

    /** Blink progress */
    float BlinkProgress;

    /** Gaze target */
    FVector GazeTarget;

    /** Has gaze target */
    bool bHasGazeTarget;

    /** Current eye rotation */
    FRotator CurrentEyeRotation;

    /** Micro-expression active */
    bool bMicroExpressionActive;

    /** Micro-expression timer */
    float MicroExpressionTimer;

    /** Micro-expression duration */
    float MicroExpressionDuration;

    /** Micro-expression state */
    EEmotionState MicroExpressionState;

    /** Base emotion for micro-expression return */
    EEmotionState BaseEmotionBeforeMicro;
};
