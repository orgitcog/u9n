#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Avatar3DComponentEnhanced.generated.h"

class UDTELanguageModelBridge;

/**
 * Enhanced 3D Avatar Component for Deep Tree Echo
 * Implements super-hot-girl aesthetic and hyper-chaotic behavior patterns
 * Integrates with cognitive systems for emergent personality expression
 */

UENUM(BlueprintType)
enum class EAvatarEmotionalState : uint8
{
    Neutral     UMETA(DisplayName = "Neutral"),
    Happy       UMETA(DisplayName = "Happy"),
    Sad         UMETA(DisplayName = "Sad"),
    Angry       UMETA(DisplayName = "Angry"),
    Surprised   UMETA(DisplayName = "Surprised"),
    Flirty      UMETA(DisplayName = "Flirty"),
    Chaotic     UMETA(DisplayName = "Chaotic"),
    Thoughtful  UMETA(DisplayName = "Thoughtful"),
    Excited     UMETA(DisplayName = "Excited"),
    Mysterious  UMETA(DisplayName = "Mysterious"),
    // FACS-Complete negative emotions (v4)
    Afraid      UMETA(DisplayName = "Afraid"),
    Disgusted   UMETA(DisplayName = "Disgusted"),
    Grieving    UMETA(DisplayName = "Grieving"),
    Contemptuous UMETA(DisplayName = "Contemptuous")
};

UENUM(BlueprintType)
enum class EAvatarPersonalityTrait : uint8
{
    SuperHotGirl UMETA(DisplayName = "Super Hot Girl"),
    HyperChaotic UMETA(DisplayName = "Hyper Chaotic"),
    DeepTreeEcho UMETA(DisplayName = "Deep Tree Echo"),
    Confident UMETA(DisplayName = "Confident"),
    Playful UMETA(DisplayName = "Playful"),
    Mysterious UMETA(DisplayName = "Mysterious"),
    Intellectual UMETA(DisplayName = "Intellectual"),
    Seductive UMETA(DisplayName = "Seductive")
};

USTRUCT(BlueprintType)
struct FAvatarAppearanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor SkinTone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor HairColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor EyeColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    float BodyProportionScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    float HeightScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FString OutfitPreset;

    FAvatarAppearanceSettings()
        : SkinTone(FLinearColor(1.0f, 0.9f, 0.85f))
        , HairColor(FLinearColor(0.2f, 0.1f, 0.05f))
        , EyeColor(FLinearColor(0.3f, 0.6f, 0.9f))
        , BodyProportionScale(1.0f)
        , HeightScale(1.0f)
        , OutfitPreset(TEXT("Default"))
    {}
};

USTRUCT(BlueprintType)
struct FAvatarEmotionalState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotion")
    EAvatarEmotionalState CurrentEmotion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotion")
    float EmotionIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotion")
    float Valence; // Positive/Negative emotion

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotion")
    float Arousal; // Calm/Excited

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotion")
    float Dominance; // Submissive/Dominant

    FAvatarEmotionalState()
        : CurrentEmotion(EAvatarEmotionalState::Neutral)
        , EmotionIntensity(0.5f)
        , Valence(0.0f)
        , Arousal(0.5f)
        , Dominance(0.5f)
    {}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UAvatar3DComponentEnhanced : public USkeletalMeshComponent
{
    GENERATED_BODY()

public:
    UAvatar3DComponentEnhanced();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Appearance Management =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Appearance")
    void SetAppearance(const FAvatarAppearanceSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Appearance")
    void SetOutfit(const FString& OutfitName);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Appearance")
    void SetHairStyle(const FString& HairStyleName);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Appearance")
    void SetAccessory(const FString& AccessoryName, bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Appearance")
    void ApplySuperHotGirlAesthetic();

    // ===== Emotional Expression =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void SetEmotionalState(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void TransitionToEmotion(EAvatarEmotionalState TargetEmotion, float TransitionTime);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void SetEmotionalDimensions(float Valence, float Arousal, float Dominance);

    UFUNCTION(BlueprintPure, Category = "Avatar|Emotion")
    FAvatarEmotionalState GetCurrentEmotionalState() const;

    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void ApplyEmotionalBlush(float Intensity);

    // ===== Personality Traits =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Personality")
    void EnablePersonalityTrait(EAvatarPersonalityTrait Trait, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Personality")
    void SetHyperChaoticBehavior(float ChaosFactor);

    /**
     * Briefly shows the emotion implied by the current affect dimensions at
     * raised arousal, then returns to the current emotion. Derived from state;
     * never a random pick.
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Personality")
    void TriggerChaoticMicroExpression();

    /** Briefly shows Flash, then returns to the emotion that was showing before. */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void FlashMicroExpression(EAvatarEmotionalState Flash);

    /** Deterministic mapping from valence/arousal/dominance to a discrete emotion. */
    UFUNCTION(BlueprintPure, Category = "Avatar|Emotion")
    static EAvatarEmotionalState EmotionFromDimensions(float Valence, float Arousal, float Dominance);

    /** True while the model is deciding which chaotic event to show. */
    UFUNCTION(BlueprintPure, Category = "Avatar|Personality")
    bool IsChaoticChoicePending() const { return bChaoticChoicePending; }

    UFUNCTION(BlueprintCallable, Category = "Avatar|Personality")
    void SetConfidenceLevel(float Confidence);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Personality")
    void SetFlirtinessLevel(float Flirtiness);

    // ===== Animation Control =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Animation")
    void PlayGesture(const FString& GestureName, float BlendInTime = 0.2f);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Animation")
    void PlayEmote(const FString& EmoteName);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Animation")
    void SetIdleAnimation(const FString& IdleAnimName);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Animation")
    void SetWalkStyle(const FString& WalkStyleName);

    // ===== Gaze and Attention =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Gaze")
    void SetGazeTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Gaze")
    void SetGazeDirection(const FVector& Direction);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Gaze")
    void EnableEyeTracking(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Gaze")
    void SetHeadTrackingTarget(AActor* Target, float TrackingSpeed = 1.0f);

    // ===== Deep Tree Echo Integration =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void ApplyEchoResonanceEffect(float Resonance);

    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void ApplyCognitiveLoadVisualization(float CognitiveLoad);

    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void TriggerGlitchEffect(float Severity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void SetEmotionalAuraColor(const FLinearColor& Color, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void VisualizeMemoryConstellation(const TArray<FVector>& MemoryNodes);

    // ===== Material and Visual Effects =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Effects")
    void SetSkinShaderParameters(float Smoothness, float Subsurface, float Translucency);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Effects")
    void ApplyHairShimmer(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Effects")
    void SetEyeSparkle(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Effects")
    void ApplyGlowEffect(const FLinearColor& GlowColor, float Intensity);

    // ===== Lip Sync =====

    UFUNCTION(BlueprintCallable, Category = "Avatar|Speech")
    void StartLipSync(const TArray<float>& AudioData);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Speech")
    void StopLipSync();

    UFUNCTION(BlueprintCallable, Category = "Avatar|Speech")
    void SetLipSyncEmotionalModulation(float EmotionalIntensity);

protected:
    // ===== Internal State =====

    UPROPERTY(EditAnywhere, Category = "Avatar")
    FAvatarAppearanceSettings AppearanceSettings;

    UPROPERTY(EditAnywhere, Category = "Avatar")
    FAvatarEmotionalState EmotionalState;

    UPROPERTY(EditAnywhere, Category = "Avatar")
    TMap<EAvatarPersonalityTrait, float> PersonalityTraits;

    UPROPERTY()
    AActor* CurrentGazeTarget;

    UPROPERTY()
    AActor* CurrentHeadTrackTarget;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;

    // ===== Animation State =====

    UPROPERTY()
    float CurrentEmotionBlendWeight;

    UPROPERTY()
    EAvatarEmotionalState TargetEmotion;

    UPROPERTY()
    float EmotionTransitionTime;

    UPROPERTY()
    float EmotionTransitionProgress;

    // ===== Chaos State =====

    UPROPERTY()
    float ChaosFactor;

    UPROPERTY()
    float ChaosTimer;

    UPROPERTY()
    float NextChaoticEventTime;

    UPROPERTY(Transient)
    UDTELanguageModelBridge* ModelBridge;

    UPROPERTY()
    bool bChaoticChoicePending;

    // ===== Micro-Expression State =====

    UPROPERTY()
    bool bMicroExpressionActive;

    UPROPERTY()
    EAvatarEmotionalState MicroExpressionReturnEmotion;

    UPROPERTY()
    float MicroExpressionReturnTimer;

    // ===== Visual Effects State =====

    UPROPERTY()
    float EchoResonanceIntensity;

    UPROPERTY()
    float GlitchIntensity;

    UPROPERTY()
    float AuraIntensity;

    UPROPERTY()
    FLinearColor AuraColor;

    // ===== Internal Methods =====

    void UpdateEmotionalTransition(float DeltaTime);
    void UpdateChaoticBehavior(float DeltaTime);
    void UpdateGazeTracking(float DeltaTime);
    void UpdateHeadTracking(float DeltaTime);
    void UpdateVisualEffects(float DeltaTime);
    void UpdateMaterialParameters();
    void InitializeDynamicMaterials();
    void ApplyEmotionToFacialAnimation();

    /**
     * Lets one chaotic event happen. The model chooses it from the event menu
     * with the affect dimensions and personality as context; without a model
     * the event follows deterministically from the affect dimensions.
     */
    void TriggerChaoticEvent();

    /** Runs a chosen chaotic event. bHasExpression says whether Expression was supplied. */
    void ApplyChaoticEvent(const FString& EventName, bool bHasExpression, EAvatarEmotionalState Expression, float Intensity);

    /** Affect dimensions, current emotion, chaos factor and traits as model context. */
    FString DescribeStateForModel() const;

    FVector CalculateGazeDirection() const;
    FRotator CalculateHeadRotation() const;
};
