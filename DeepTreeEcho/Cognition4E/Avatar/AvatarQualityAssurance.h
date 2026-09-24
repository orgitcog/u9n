// AvatarQualityAssurance.h
// Deep Tree Echo - Fanatical Attention to Detail in Avatar Expression
// Implements the highest quality standards for believable, authentic avatar behavior
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarQualityAssurance.generated.h"

// ========================================
// MICRO-EXPRESSION AUTHENTICITY
// ========================================

/**
 * Micro-expression timing profile - The subtle timing that makes expressions believable
 */
USTRUCT(BlueprintType)
struct FMicroExpressionTiming
{
    GENERATED_BODY()

    /** Onset time (how fast expression appears) - genuine emotions are faster */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OnsetTime = 0.1f;

    /** Apex duration (how long at peak) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ApexDuration = 0.3f;

    /** Offset time (how fast expression fades) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OffsetTime = 0.2f;

    /** Asymmetry at onset (genuine emotions often start asymmetric) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OnsetAsymmetry = 0.15f;

    /** Temporal jitter (natural variation in timing) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TemporalJitter = 0.05f;
};

/**
 * Expression asymmetry - Natural facial asymmetry for authenticity
 */
USTRUCT(BlueprintType)
struct FExpressionAsymmetry
{
    GENERATED_BODY()

    /** Baseline asymmetry (permanent facial differences) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaselineAsymmetry = 0.05f;

    /** Dynamic asymmetry (expression-dependent) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DynamicAsymmetry = 0.1f;

    /** Dominant side (0 = left, 1 = right, 0.5 = balanced) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DominantSide = 0.55f;

    /** Brow asymmetry factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BrowAsymmetry = 0.08f;

    /** Mouth asymmetry factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MouthAsymmetry = 0.12f;
};

/**
 * Emotional leakage - Subtle expression of suppressed emotions
 */
USTRUCT(BlueprintType)
struct FEmotionalLeakage
{
    GENERATED_BODY()

    /** Suppressed emotion type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SuppressedEmotion;

    /** Leakage intensity (0-1, how much shows through) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LeakageIntensity = 0.0f;

    /** Leakage location (which facial region shows it) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LeakageRegion;

    /** Suppression effort (higher = more tension in expression) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuppressionEffort = 0.0f;
};

// ========================================
// PHYSIOLOGICAL COHERENCE
// ========================================

/**
 * Autonomic state - Physiological responses that must be coherent
 */
USTRUCT(BlueprintType)
struct FAutonomicState
{
    GENERATED_BODY()

    /** Pupil dilation (0 = constricted, 1 = dilated) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PupilDilation = 0.5f;

    /** Skin flush (0 = pale, 1 = flushed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SkinFlush = 0.3f;

    /** Perspiration level (0 = dry, 1 = sweating) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Perspiration = 0.0f;

    /** Tear film (0 = normal, 1 = watery eyes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TearFilm = 0.2f;

    /** Lip moisture (0 = dry, 1 = moist) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LipMoisture = 0.5f;

    /** Nostril flare (0 = normal, 1 = flared) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NostrilFlare = 0.0f;

    /** Muscle tension (overall facial tension) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MuscleTension = 0.3f;
};

/**
 * Skin response - Visual skin reactions
 */
USTRUCT(BlueprintType)
struct FSkinResponse
{
    GENERATED_BODY()

    /** Flush color tint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor FlushTint = FLinearColor(1.0f, 0.8f, 0.8f, 1.0f);

    /** Flush regions (cheeks, ears, neck) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> FlushRegions;

    /** Pallor (for fear/shock) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Pallor = 0.0f;

    /** Subsurface scattering adjustment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SubsurfaceIntensity = 1.0f;
};

// ========================================
// EXPRESSION MOMENTUM & PERSISTENCE
// ========================================

/**
 * Emotional momentum - Expressions have inertia
 */
USTRUCT(BlueprintType)
struct FEmotionalMomentum
{
    GENERATED_BODY()

    /** Current emotional velocity (rate of change) */
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, float> EmotionalVelocity;

    /** Emotional inertia (resistance to change) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Inertia = 0.5f;

    /** Momentum decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayRate = 0.1f;

    /** Maximum velocity (limits unrealistic changes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxVelocity = 2.0f;
};

/**
 * Expression persistence - How long expressions linger
 */
USTRUCT(BlueprintType)
struct FExpressionPersistence
{
    GENERATED_BODY()

    /** Residual expression (ghost of previous emotion) */
    UPROPERTY(BlueprintReadOnly)
    FString ResidualEmotion;

    /** Residual intensity */
    UPROPERTY(BlueprintReadOnly)
    float ResidualIntensity = 0.0f;

    /** Persistence factor (how long residuals last) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PersistenceFactor = 0.3f;

    /** Blend mode for residuals */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlendMode = TEXT("Additive");
};

// ========================================
// PERSONALITY-DRIVEN EXPRESSION
// ========================================

/**
 * Expressivity profile - How personality affects expression range
 */
USTRUCT(BlueprintType)
struct FExpressivityProfile
{
    GENERATED_BODY()

    /** Overall expressivity (0 = stoic, 1 = theatrical) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallExpressivity = 0.7f;

    /** Expressivity by emotion type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> EmotionExpressivity;

    /** Spontaneity (how quickly emotions show) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Spontaneity = 0.6f;

    /** Suppression tendency (how much they hold back) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuppressionTendency = 0.3f;

    /** Smile authenticity (Duchenne smile frequency) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SmileAuthenticity = 0.8f;
};

/**
 * Cultural expression norms
 */
USTRUCT(BlueprintType)
struct FCulturalExpressionNorms
{
    GENERATED_BODY()

    /** Display rule intensity (0 = uninhibited, 1 = highly regulated) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DisplayRuleIntensity = 0.3f;

    /** Eye contact comfort (affects gaze behavior) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EyeContactComfort = 0.7f;

    /** Personal space sensitivity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PersonalSpaceSensitivity = 0.5f;

    /** Gesture amplitude modifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GestureAmplitude = 1.0f;
};

// ========================================
// FATIGUE & STATE EFFECTS
// ========================================

/**
 * Fatigue state - How tiredness affects expression
 */
USTRUCT(BlueprintType)
struct FFatigueState
{
    GENERATED_BODY()

    /** Physical fatigue (0 = fresh, 1 = exhausted) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhysicalFatigue = 0.0f;

    /** Mental fatigue */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MentalFatigue = 0.0f;

    /** Emotional fatigue */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalFatigue = 0.0f;

    /** Eye strain (affects blink rate, eye width) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EyeStrain = 0.0f;

    /** Alertness (inverse of drowsiness) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Alertness = 1.0f;
};

/**
 * Fatigue manifestations
 */
USTRUCT(BlueprintType)
struct FFatigueManifestations
{
    GENERATED_BODY()

    /** Eyelid droop */
    UPROPERTY(BlueprintReadOnly)
    float EyelidDroop = 0.0f;

    /** Blink rate modifier */
    UPROPERTY(BlueprintReadOnly)
    float BlinkRateModifier = 1.0f;

    /** Expression dampening */
    UPROPERTY(BlueprintReadOnly)
    float ExpressionDampening = 0.0f;

    /** Response latency increase */
    UPROPERTY(BlueprintReadOnly)
    float ResponseLatency = 0.0f;

    /** Yawn probability */
    UPROPERTY(BlueprintReadOnly)
    float YawnProbability = 0.0f;
};

// ========================================
// ENVIRONMENTAL RESPONSIVENESS
// ========================================

/**
 * Light response - Pupil and squint reactions
 */
USTRUCT(BlueprintType)
struct FLightResponse
{
    GENERATED_BODY()

    /** Ambient light level (0 = dark, 1 = bright) */
    UPROPERTY(BlueprintReadOnly)
    float AmbientLightLevel = 0.5f;

    /** Pupil response (constriction/dilation) */
    UPROPERTY(BlueprintReadOnly)
    float PupilResponse = 0.5f;

    /** Squint amount */
    UPROPERTY(BlueprintReadOnly)
    float SquintAmount = 0.0f;

    /** Brow shade (brow lowering in bright light) */
    UPROPERTY(BlueprintReadOnly)
    float BrowShade = 0.0f;
};

/**
 * Environmental awareness
 */
USTRUCT(BlueprintType)
struct FEnvironmentalAwareness
{
    GENERATED_BODY()

    /** Wind response (affects hair, slight squint) */
    UPROPERTY(BlueprintReadOnly)
    float WindResponse = 0.0f;

    /** Temperature comfort (affects flush, perspiration) */
    UPROPERTY(BlueprintReadOnly)
    float TemperatureComfort = 0.5f;

    /** Noise response (affects attention, slight flinch) */
    UPROPERTY(BlueprintReadOnly)
    float NoiseResponse = 0.0f;

    /** Proximity response (comfort with nearby objects/people) */
    UPROPERTY(BlueprintReadOnly)
    float ProximityResponse = 0.0f;
};

// ========================================
// QUALITY METRICS
// ========================================

/**
 * Quality assessment metrics
 */
USTRUCT(BlueprintType)
struct FAvatarQualityMetrics
{
    GENERATED_BODY()

    /** Expression coherence (all channels aligned) */
    UPROPERTY(BlueprintReadOnly)
    float ExpressionCoherence = 1.0f;

    /** Physiological coherence */
    UPROPERTY(BlueprintReadOnly)
    float PhysiologicalCoherence = 1.0f;

    /** Temporal coherence (smooth transitions) */
    UPROPERTY(BlueprintReadOnly)
    float TemporalCoherence = 1.0f;

    /** Authenticity score (natural vs mechanical) */
    UPROPERTY(BlueprintReadOnly)
    float AuthenticityScore = 1.0f;

    /** Subtlety score (appropriate nuance) */
    UPROPERTY(BlueprintReadOnly)
    float SubtletyScore = 1.0f;

    /** Responsiveness score (appropriate reactions) */
    UPROPERTY(BlueprintReadOnly)
    float ResponsivenessScore = 1.0f;

    /** Overall quality score */
    UPROPERTY(BlueprintReadOnly)
    float OverallQuality = 1.0f;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMicroExpressionDetected, FString, EmotionType, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhysiologicalChange, FAutonomicState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityAlert, FString, AlertMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionalLeakage, FEmotionalLeakage, Leakage);

/**
 * Avatar Quality Assurance Component
 *
 * Implements fanatical attention to detail for avatar expression:
 * - Micro-expression authenticity (timing, asymmetry, leakage)
 * - Physiological coherence (pupils, flush, perspiration)
 * - Expression momentum and persistence
 * - Personality-driven expression variation
 * - Fatigue and state effects
 * - Environmental responsiveness
 * - Continuous quality self-assessment
 *
 * This component ensures the avatar achieves the highest level of believability
 * through subtle, nuanced behaviors that transcend typical animation systems.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAvatarQualityAssurance : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarQualityAssurance();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable micro-expression authenticity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnableMicroExpressionAuthenticity = true;

    /** Enable physiological coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnablePhysiologicalCoherence = true;

    /** Enable expression momentum */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnableExpressionMomentum = true;

    /** Enable fatigue effects */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnableFatigueEffects = true;

    /** Enable environmental responsiveness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnableEnvironmentalResponse = true;

    /** Enable quality self-assessment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Features")
    bool bEnableQualitySelfAssessment = true;

    /** Micro-expression timing profile */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|MicroExpression")
    FMicroExpressionTiming MicroExpressionTiming;

    /** Expression asymmetry settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|MicroExpression")
    FExpressionAsymmetry AsymmetrySettings;

    /** Expressivity profile */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Personality")
    FExpressivityProfile ExpressivityProfile;

    /** Cultural expression norms */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality|Personality")
    FCulturalExpressionNorms CulturalNorms;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Quality|Events")
    FOnMicroExpressionDetected OnMicroExpressionDetected;

    UPROPERTY(BlueprintAssignable, Category = "Quality|Events")
    FOnPhysiologicalChange OnPhysiologicalChange;

    UPROPERTY(BlueprintAssignable, Category = "Quality|Events")
    FOnQualityAlert OnQualityAlert;

    UPROPERTY(BlueprintAssignable, Category = "Quality|Events")
    FOnEmotionalLeakage OnEmotionalLeakage;

    // ========================================
    // MICRO-EXPRESSION API
    // ========================================

    /** Get timing profile for emotion (genuine vs posed) */
    UFUNCTION(BlueprintCallable, Category = "Quality|MicroExpression")
    FMicroExpressionTiming GetTimingForEmotion(const FString& EmotionType, bool bGenuine);

    /** Apply asymmetry to expression values */
    UFUNCTION(BlueprintCallable, Category = "Quality|MicroExpression")
    void ApplyAsymmetryToExpression(float& LeftValue, float& RightValue, const FString& Region);

    /** Detect and trigger emotional leakage */
    UFUNCTION(BlueprintCallable, Category = "Quality|MicroExpression")
    void ProcessEmotionalLeakage(const FString& DisplayedEmotion, const FString& FeltEmotion, float SuppressionLevel);

    /** Add temporal jitter to timing */
    UFUNCTION(BlueprintPure, Category = "Quality|MicroExpression")
    float AddTemporalJitter(float BaseTime) const;

    // ========================================
    // PHYSIOLOGICAL API
    // ========================================

    /** Update autonomic state from emotion */
    UFUNCTION(BlueprintCallable, Category = "Quality|Physiology")
    void UpdateAutonomicStateFromEmotion(const FString& EmotionType, float Intensity);

    /** Get current autonomic state */
    UFUNCTION(BlueprintPure, Category = "Quality|Physiology")
    FAutonomicState GetAutonomicState() const;

    /** Get skin response parameters */
    UFUNCTION(BlueprintPure, Category = "Quality|Physiology")
    FSkinResponse GetSkinResponse() const;

    /** Set manual pupil override */
    UFUNCTION(BlueprintCallable, Category = "Quality|Physiology")
    void SetPupilOverride(float Dilation, float Duration);

    /** Trigger flush response */
    UFUNCTION(BlueprintCallable, Category = "Quality|Physiology")
    void TriggerFlush(float Intensity, float Duration);

    // ========================================
    // MOMENTUM API
    // ========================================

    /** Get emotional momentum */
    UFUNCTION(BlueprintPure, Category = "Quality|Momentum")
    FEmotionalMomentum GetEmotionalMomentum() const;

    /** Apply momentum to expression change */
    UFUNCTION(BlueprintCallable, Category = "Quality|Momentum")
    float ApplyMomentum(const FString& EmotionType, float TargetIntensity, float DeltaTime);

    /** Get expression persistence */
    UFUNCTION(BlueprintPure, Category = "Quality|Momentum")
    FExpressionPersistence GetExpressionPersistence() const;

    /** Force momentum reset (for sudden emotional changes) */
    UFUNCTION(BlueprintCallable, Category = "Quality|Momentum")
    void ResetMomentum();

    // ========================================
    // PERSONALITY API
    // ========================================

    /** Apply expressivity profile to intensity */
    UFUNCTION(BlueprintCallable, Category = "Quality|Personality")
    float ApplyExpressivityProfile(const FString& EmotionType, float RawIntensity);

    /** Get cultural display rules for emotion */
    UFUNCTION(BlueprintPure, Category = "Quality|Personality")
    float GetCulturalDisplayRule(const FString& EmotionType, const FString& Context);

    /** Set personality trait (affects expressivity) */
    UFUNCTION(BlueprintCallable, Category = "Quality|Personality")
    void SetPersonalityTrait(const FString& TraitName, float Value);

    // ========================================
    // FATIGUE API
    // ========================================

    /** Set fatigue state */
    UFUNCTION(BlueprintCallable, Category = "Quality|Fatigue")
    void SetFatigueState(const FFatigueState& State);

    /** Get current fatigue state */
    UFUNCTION(BlueprintPure, Category = "Quality|Fatigue")
    FFatigueState GetFatigueState() const;

    /** Get fatigue manifestations */
    UFUNCTION(BlueprintPure, Category = "Quality|Fatigue")
    FFatigueManifestations GetFatigueManifestations() const;

    /** Apply fatigue to expression */
    UFUNCTION(BlueprintCallable, Category = "Quality|Fatigue")
    void ApplyFatigueToExpression(float& ExpressionIntensity, float& ResponseTime);

    /** Trigger yawn */
    UFUNCTION(BlueprintCallable, Category = "Quality|Fatigue")
    void TriggerYawn();

    // ========================================
    // ENVIRONMENTAL API
    // ========================================

    /** Set ambient light level */
    UFUNCTION(BlueprintCallable, Category = "Quality|Environment")
    void SetAmbientLight(float Level);

    /** Get light response */
    UFUNCTION(BlueprintPure, Category = "Quality|Environment")
    FLightResponse GetLightResponse() const;

    /** Set environmental conditions */
    UFUNCTION(BlueprintCallable, Category = "Quality|Environment")
    void SetEnvironmentalConditions(float Wind, float Temperature, float Noise);

    /** Get environmental awareness state */
    UFUNCTION(BlueprintPure, Category = "Quality|Environment")
    FEnvironmentalAwareness GetEnvironmentalAwareness() const;

    // ========================================
    // QUALITY ASSESSMENT API
    // ========================================

    /** Get current quality metrics */
    UFUNCTION(BlueprintPure, Category = "Quality|Assessment")
    FAvatarQualityMetrics GetQualityMetrics() const;

    /** Run comprehensive quality check */
    UFUNCTION(BlueprintCallable, Category = "Quality|Assessment")
    FAvatarQualityMetrics RunQualityCheck();

    /** Get quality improvement suggestions */
    UFUNCTION(BlueprintPure, Category = "Quality|Assessment")
    TArray<FString> GetQualityImprovementSuggestions() const;

    /** Enable quality logging */
    UFUNCTION(BlueprintCallable, Category = "Quality|Assessment")
    void SetQualityLogging(bool bEnabled);

protected:
    // Internal state
    FAutonomicState CurrentAutonomicState;
    FSkinResponse CurrentSkinResponse;
    FEmotionalMomentum EmotionalMomentum;
    FExpressionPersistence ExpressionPersistence;
    FFatigueState CurrentFatigueState;
    FFatigueManifestations FatigueManifestations;
    FLightResponse CurrentLightResponse;
    FEnvironmentalAwareness EnvironmentalAwareness;
    FAvatarQualityMetrics QualityMetrics;

    // Emotional leakage tracking
    TArray<FEmotionalLeakage> ActiveLeakages;

    // Pupil override
    bool bPupilOverrideActive = false;
    float PupilOverrideValue = 0.5f;
    float PupilOverrideTimer = 0.0f;

    // Flush state
    bool bFlushActive = false;
    float FlushIntensity = 0.0f;
    float FlushTimer = 0.0f;

    // Emotional history for momentum
    TMap<FString, float> PreviousEmotionIntensities;
    TMap<FString, double> LastEmotionUpdateTimes;

    // Quality logging
    bool bQualityLoggingEnabled = false;

    // Internal methods
    void UpdateMicroExpressionAuthenticity(float DeltaTime);
    void UpdatePhysiologicalCoherence(float DeltaTime);
    void UpdateExpressionMomentum(float DeltaTime);
    void UpdateFatigueEffects(float DeltaTime);
    void UpdateEnvironmentalResponse(float DeltaTime);
    void UpdateQualityMetrics(float DeltaTime);

    float ComputeExpressionCoherence() const;
    float ComputePhysiologicalCoherence() const;
    float ComputeTemporalCoherence() const;
    float ComputeAuthenticityScore() const;
    float ComputeSubtletyScore() const;

    FMicroExpressionTiming GetGenuineTiming(const FString& EmotionType) const;
    FMicroExpressionTiming GetPosedTiming(const FString& EmotionType) const;

    void UpdateAutonomicFromArousal(float Arousal, float Valence);
    void UpdateSkinResponseFromAutonomic();
    void ComputeFatigueManifestations();
    void ComputeLightResponse();
};
