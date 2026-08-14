#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PersonalityTraitSystem.generated.h"

/**
 * Personality trait types for the Deep Tree Echo avatar
 */
UENUM(BlueprintType)
enum class EPersonalityTraitType : uint8
{
    SuperHotGirl UMETA(DisplayName = "Super Hot Girl"),
    HyperChaotic UMETA(DisplayName = "Hyper Chaotic"),
    DeepTreeEcho UMETA(DisplayName = "Deep Tree Echo"),
    Confident UMETA(DisplayName = "Confident"),
    Playful UMETA(DisplayName = "Playful"),
    Mysterious UMETA(DisplayName = "Mysterious"),
    Intellectual UMETA(DisplayName = "Intellectual"),
    Seductive UMETA(DisplayName = "Seductive"),
    Empathetic UMETA(DisplayName = "Empathetic"),
    Assertive UMETA(DisplayName = "Assertive")
};

/**
 * Personality trait data structure
 */
USTRUCT(BlueprintType)
struct FPersonalityTrait
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPersonalityTraitType TraitType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Intensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability; // How stable this trait is (0 = very volatile, 1 = very stable)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DevelopmentRate; // How quickly this trait can change

    FPersonalityTrait()
        : TraitType(EPersonalityTraitType::Confident)
        , Intensity(0.5f)
        , Stability(0.7f)
        , DevelopmentRate(0.1f)
    {}
};

/**
 * Super Hot Girl personality trait implementation
 */
USTRUCT(BlueprintType)
struct FSuperHotGirlTrait
{
    GENERATED_BODY()

    // Core attributes
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence; // Self-assurance and poise

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Charm; // Natural attractiveness and charisma

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Playfulness; // Flirty, fun-loving behavior

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Elegance; // Graceful movement and expression

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Expressiveness; // Emotional openness and animation

    // Behavioral modifiers
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EyeContactIntensity; // How directly she maintains gaze

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SmileFrequency; // How often she smiles

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HairTouchFrequency; // Self-grooming gestures

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PostureConfidence; // Body language openness

    FSuperHotGirlTrait()
        : Confidence(0.8f)
        , Charm(0.9f)
        , Playfulness(0.6f)
        , Elegance(0.7f)
        , Expressiveness(0.8f)
        , EyeContactIntensity(0.7f)
        , SmileFrequency(0.8f)
        , HairTouchFrequency(0.3f)
        , PostureConfidence(0.8f)
    {}
};

/**
 * Hyper Chaotic personality trait implementation
 */
USTRUCT(BlueprintType)
struct FHyperChaoticTrait
{
    GENERATED_BODY()

    // Core attributes
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UnpredictabilityFactor; // How unpredictable behavior is

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalVolatility; // How quickly emotions change

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ImpulsivityLevel; // Tendency for spontaneous actions

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PatternBreaking; // Tendency to break established patterns

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CognitiveFluctuation; // Variation in cognitive processing

    // Manifestation parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MicroExpressionFrequency; // Rapid, subtle facial changes

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GlitchEffectProbability; // Chance of visual glitches

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuddenShiftIntensity; // Magnitude of sudden changes

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EchoResonanceVariation; // Fluctuation in echo effects

    FHyperChaoticTrait()
        : UnpredictabilityFactor(0.3f)
        , EmotionalVolatility(0.4f)
        , ImpulsivityLevel(0.3f)
        , PatternBreaking(0.5f)
        , CognitiveFluctuation(0.3f)
        , MicroExpressionFrequency(0.4f)
        , GlitchEffectProbability(0.2f)
        , SuddenShiftIntensity(0.3f)
        , EchoResonanceVariation(0.4f)
    {}
};

/**
 * Personality Trait System Component
 * Manages all personality traits and their interactions
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UPersonalityTraitSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UPersonalityTraitSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Trait Management =====

    UFUNCTION(BlueprintCallable, Category = "Personality")
    void SetTraitIntensity(EPersonalityTraitType TraitType, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Personality")
    float GetTraitIntensity(EPersonalityTraitType TraitType) const;

    UFUNCTION(BlueprintCallable, Category = "Personality")
    void ModifyTrait(EPersonalityTraitType TraitType, float DeltaIntensity);

    UFUNCTION(BlueprintCallable, Category = "Personality")
    void DevelopTrait(EPersonalityTraitType TraitType, float DevelopmentAmount);

    // ===== Super Hot Girl Trait =====

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    void ActivateSuperHotGirlTrait(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    FSuperHotGirlTrait GetSuperHotGirlTrait() const { return SuperHotGirlTrait; }

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    void SetSuperHotGirlParameters(float Confidence, float Charm, float Playfulness);

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    void TriggerFlirtyBehavior(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    void TriggerConfidentGesture();

    UFUNCTION(BlueprintCallable, Category = "Personality|SuperHotGirl")
    void TriggerPlayfulExpression();

    // ===== Hyper Chaotic Trait =====

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    void ActivateHyperChaoticTrait(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    FHyperChaoticTrait GetHyperChaoticTrait() const { return HyperChaoticTrait; }

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    void SetHyperChaoticParameters(float Unpredictability, float Volatility, float Impulsivity);

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    void TriggerChaoticEvent();

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    void TriggerEmotionalSpike(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Personality|HyperChaotic")
    void TriggerPatternBreak();

    // ===== Trait Interactions =====

    UFUNCTION(BlueprintCallable, Category = "Personality")
    void CalculateTraitInteractions();

    UFUNCTION(BlueprintCallable, Category = "Personality")
    float GetTraitSynergy(EPersonalityTraitType Trait1, EPersonalityTraitType Trait2) const;

    UFUNCTION(BlueprintCallable, Category = "Personality")
    TArray<EPersonalityTraitType> GetDominantTraits(int32 Count = 3) const;

    // ===== Behavioral Output =====

    UFUNCTION(BlueprintCallable, Category = "Personality")
    FVector2D GetEmotionalTendency() const; // Returns (Valence, Arousal) tendency

    UFUNCTION(BlueprintCallable, Category = "Personality")
    float GetBehavioralPredictability() const;

    UFUNCTION(BlueprintCallable, Category = "Personality")
    float GetSocialEngagementLevel() const;

protected:
    // ===== Internal Methods =====

    void UpdateTraitDevelopment(float DeltaTime);
    void ProcessTraitInteractions(float DeltaTime);
    void ApplyTraitToAvatar();
    void UpdateChaoticBehavior(float DeltaTime);
    void UpdateSuperHotGirlBehavior(float DeltaTime);

private:
    // ===== Core Traits =====

    UPROPERTY(EditAnywhere, Category = "Personality")
    TMap<EPersonalityTraitType, FPersonalityTrait> PersonalityTraits;

    UPROPERTY(EditAnywhere, Category = "Personality")
    FSuperHotGirlTrait SuperHotGirlTrait;

    UPROPERTY(EditAnywhere, Category = "Personality")
    FHyperChaoticTrait HyperChaoticTrait;

    // ===== Timers and State =====

    float ChaoticEventTimer;
    float NextChaoticEventTime;
    float FlirtyBehaviorTimer;
    float ConfidentGestureTimer;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Personality")
    float TraitDevelopmentSpeed;

    UPROPERTY(EditAnywhere, Category = "Personality")
    float TraitInteractionStrength;

    UPROPERTY(EditAnywhere, Category = "Personality")
    bool bEnableAutonomousBehavior;
};
