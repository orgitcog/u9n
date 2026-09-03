#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "EmotionalAuraComponent.generated.h"

UENUM(BlueprintType)
enum class EAuraEmotionType : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Joy UMETA(DisplayName = "Joy"),
    Sadness UMETA(DisplayName = "Sadness"),
    Anger UMETA(DisplayName = "Anger"),
    Fear UMETA(DisplayName = "Fear"),
    Love UMETA(DisplayName = "Love"),
    Excitement UMETA(DisplayName = "Excitement"),
    Calm UMETA(DisplayName = "Calm"),
    Confidence UMETA(DisplayName = "Confidence"),
    Playfulness UMETA(DisplayName = "Playfulness")
};

USTRUCT(BlueprintType)
struct FAuraVisualizationData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    FLinearColor PrimaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    FLinearColor SecondaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    float Intensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    float Radius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    float ParticleSize = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    float EmissionRate = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aura")
    float Turbulence = 0.5f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UEmotionalAuraComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmotionalAuraComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Set the current emotional state */
    UFUNCTION(BlueprintCallable, Category = "Emotional Aura")
    void SetEmotionalState(EAuraEmotionType NewEmotion, float TransitionTime = 1.0f);

    /** Get the current emotional state */
    UFUNCTION(BlueprintPure, Category = "Emotional Aura")
    EAuraEmotionType GetEmotionalState() const { return CurrentEmotion; }

    /** Set aura intensity */
    UFUNCTION(BlueprintCallable, Category = "Emotional Aura")
    void SetAuraIntensity(float NewIntensity);

    /** Enable/disable aura visualization */
    UFUNCTION(BlueprintCallable, Category = "Emotional Aura")
    void SetAuraEnabled(bool bEnabled);

    /** Trigger emotional pulse effect */
    UFUNCTION(BlueprintCallable, Category = "Emotional Aura")
    void TriggerEmotionalPulse(float PulseIntensity = 2.0f, float Duration = 0.5f);

    /** Set custom aura colors */
    UFUNCTION(BlueprintCallable, Category = "Emotional Aura")
    void SetCustomAuraColors(const FLinearColor& Primary, const FLinearColor& Secondary);

    /** Get current aura visualization data */
    UFUNCTION(BlueprintPure, Category = "Emotional Aura")
    FAuraVisualizationData GetAuraData() const { return CurrentAuraData; }

private:
    /** Initialize emotion color mappings */
    void InitializeEmotionColorMappings();

    /** Update aura transition */
    void UpdateAuraTransition(float DeltaTime);

    /** Update particle system */
    void UpdateParticleSystem(float DeltaTime);

    /** Update emotional pulse */
    void UpdateEmotionalPulse(float DeltaTime);

    /** Interpolate aura data */
    FAuraVisualizationData InterpolateAuraData(const FAuraVisualizationData& From, const FAuraVisualizationData& To, float Alpha);

    /** Generate procedural particles */
    void GenerateProceduralParticles(float DeltaTime);

    /** Update aura morphing */
    void UpdateAuraMorphing(float DeltaTime);

    /** Current emotional state */
    UPROPERTY()
    EAuraEmotionType CurrentEmotion;

    /** Target emotional state */
    UPROPERTY()
    EAuraEmotionType TargetEmotion;

    /** Emotion color mappings */
    TMap<EAuraEmotionType, FAuraVisualizationData> EmotionColorMappings;

    /** Current aura visualization data */
    UPROPERTY()
    FAuraVisualizationData CurrentAuraData;

    /** Target aura visualization data */
    UPROPERTY()
    FAuraVisualizationData TargetAuraData;

    /** Aura transition progress */
    float AuraTransitionProgress;

    /** Aura transition duration */
    float AuraTransitionDuration;

    /** Is aura enabled */
    UPROPERTY(EditAnywhere, Category = "Emotional Aura")
    bool bAuraEnabled = true;

    /** Particle system component */
    UPROPERTY()
    UParticleSystemComponent* ParticleSystemComponent;

    /** Emotional pulse active */
    bool bEmotionalPulseActive;

    /** Emotional pulse timer */
    float EmotionalPulseTimer;

    /** Emotional pulse duration */
    float EmotionalPulseDuration;

    /** Emotional pulse intensity */
    float EmotionalPulseIntensity;

    /** Base intensity before pulse */
    float BaseIntensityBeforePulse;

    /** Procedural particle positions */
    TArray<FVector> ProceduralParticlePositions;

    /** Procedural particle velocities */
    TArray<FVector> ProceduralParticleVelocities;

    /** Procedural particle lifetimes */
    TArray<float> ProceduralParticleLifetimes;

    /** Aura morphing time */
    float AuraMorphingTime;
};
