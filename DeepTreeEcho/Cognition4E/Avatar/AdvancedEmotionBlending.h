// AdvancedEmotionBlending.h
// Advanced emotion blending system for refined cognitive-expression mappings
// Implements multi-dimensional emotion space with dynamic blending

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealAvatarCognition.h"
#include "../../Echobeats/EchobeatsStreamEngine.h"
#include "../../Echobeats/TensionalCouplingDynamics.h"
#include "../Sensory/SensoryInputIntegration.h"
#include "AdvancedEmotionBlending.generated.h"

/**
 * Emotion dimension (Plutchik-inspired with extensions)
 */
UENUM(BlueprintType)
enum class EEmotionDimension : uint8
{
    Valence         UMETA(DisplayName = "Valence (Positive-Negative)"),
    Arousal         UMETA(DisplayName = "Arousal (High-Low Energy)"),
    Dominance       UMETA(DisplayName = "Dominance (Control-Submission)"),
    Approach        UMETA(DisplayName = "Approach (Toward-Away)"),
    Certainty       UMETA(DisplayName = "Certainty (Sure-Uncertain)"),
    Novelty         UMETA(DisplayName = "Novelty (Novel-Familiar)"),
    Agency          UMETA(DisplayName = "Agency (Self-Other Caused)"),
    Temporal        UMETA(DisplayName = "Temporal (Future-Past Oriented)")
};

/**
 * Emotion blend mode
 */
UENUM(BlueprintType)
enum class EEmotionBlendMode : uint8
{
    Linear          UMETA(DisplayName = "Linear - Simple interpolation"),
    Weighted        UMETA(DisplayName = "Weighted - Priority-based blending"),
    Dominant        UMETA(DisplayName = "Dominant - Strongest wins"),
    Layered         UMETA(DisplayName = "Layered - Background + Foreground"),
    Oscillating     UMETA(DisplayName = "Oscillating - Time-varying blend")
};

/**
 * Emotion transition curve
 */
UENUM(BlueprintType)
enum class EEmotionTransitionCurve : uint8
{
    Linear          UMETA(DisplayName = "Linear"),
    EaseIn          UMETA(DisplayName = "Ease In"),
    EaseOut         UMETA(DisplayName = "Ease Out"),
    EaseInOut       UMETA(DisplayName = "Ease In/Out"),
    Bounce          UMETA(DisplayName = "Bounce"),
    Overshoot       UMETA(DisplayName = "Overshoot"),
    Step            UMETA(DisplayName = "Step (Instant)")
};

/**
 * Multi-dimensional emotion vector
 */
USTRUCT(BlueprintType)
struct FEmotionVector
{
    GENERATED_BODY()

    /** Valence (-1 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence = 0.0f;

    /** Arousal (0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Arousal = 0.5f;

    /** Dominance (-1 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Dominance = 0.0f;

    /** Approach (-1 to 1, negative = avoid) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Approach = 0.0f;

    /** Certainty (0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Certainty = 0.5f;

    /** Novelty (0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Novelty = 0.5f;

    /** Agency (-1 to 1, negative = external cause) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Agency = 0.0f;

    /** Temporal (-1 to 1, negative = past, positive = future) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Temporal = 0.0f;

    /** Overall intensity (0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.5f;

    // Utility methods
    FEmotionVector operator+(const FEmotionVector& Other) const;
    FEmotionVector operator*(float Scalar) const;
    static FEmotionVector Lerp(const FEmotionVector& A, const FEmotionVector& B, float Alpha);
    float DistanceTo(const FEmotionVector& Other) const;
    void Normalize();
};

/**
 * Emotion layer for layered blending
 */
USTRUCT(BlueprintType)
struct FEmotionLayer
{
    GENERATED_BODY()

    /** Layer name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LayerName;

    /** Layer emotion vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector Emotion;

    /** Layer weight (0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Weight = 1.0f;

    /** Layer priority (higher = more dominant) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;

    /** Is layer active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = true;

    /** Decay rate (per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayRate = 0.0f;

    /** Source (what triggered this layer) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Source;
};

/**
 * Emotion transition state
 */
USTRUCT(BlueprintType)
struct FEmotionTransition
{
    GENERATED_BODY()

    /** Source emotion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector SourceEmotion;

    /** Target emotion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector TargetEmotion;

    /** Transition duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 1.0f;

    /** Elapsed time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElapsedTime = 0.0f;

    /** Transition curve */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEmotionTransitionCurve Curve = EEmotionTransitionCurve::EaseInOut;

    /** Is transition active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = false;
};

/**
 * Expression target for emotion-to-expression mapping
 */
USTRUCT(BlueprintType)
struct FExpressionTarget
{
    GENERATED_BODY()

    /** Target facial expression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFacialExpressionState FacialExpression;

    /** Target body schema */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAvatarBodySchema BodySchema;

    /** Target voice modulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoiceModulationState VoiceModulation;

    /** Aura color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AuraColor = FLinearColor::White;

    /** Aura intensity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AuraIntensity = 0.5f;

    /** Particle effect name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticleEffectName;

    /** Sound cue name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SoundCueName;
};

/**
 * Emotion-to-expression mapping rule
 */
USTRUCT(BlueprintType)
struct FEmotionExpressionRule
{
    GENERATED_BODY()

    /** Rule name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RuleName;

    /** Emotion condition (center point in emotion space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector EmotionCondition;

    /** Activation radius in emotion space */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationRadius = 0.5f;

    /** Expression target when rule is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FExpressionTarget ExpressionTarget;

    /** Rule priority */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;

    /** Is rule enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEnabled = true;
};

/**
 * Full emotion blending state
 */
USTRUCT(BlueprintType)
struct FEmotionBlendingState
{
    GENERATED_BODY()

    /** Current blended emotion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector CurrentEmotion;

    /** Active emotion layers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEmotionLayer> ActiveLayers;

    /** Current transition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionTransition CurrentTransition;

    /** Current expression target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FExpressionTarget CurrentExpression;

    /** Active rules */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ActiveRuleNames;

    /** Emotional momentum (rate of change) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEmotionVector EmotionalMomentum;

    /** Emotional stability (0 = volatile, 1 = stable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalStability = 0.5f;

    /** Current echobeat step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentEchobeatStep = 1;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmotionLayerAdded, FString, LayerName, FEmotionVector, Emotion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionLayerRemoved, FString, LayerName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmotionTransitionStarted, FEmotionVector, From, FEmotionVector, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionTransitionCompleted, FEmotionVector, FinalEmotion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExpressionRuleActivated, FString, RuleName, float, ActivationStrength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionalStabilityChanged, float, NewStability);

/**
 * Advanced Emotion Blending Component
 * 
 * Implements multi-dimensional emotion space with sophisticated blending.
 * 
 * Key features:
 * - 8-dimensional emotion vector (Valence, Arousal, Dominance, etc.)
 * - Layered emotion blending with priorities
 * - Smooth transitions with configurable curves
 * - Rule-based emotion-to-expression mapping
 * - Integration with echobeats cognitive loop
 * - Emotional momentum and stability tracking
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAdvancedEmotionBlending : public UActorComponent
{
    GENERATED_BODY()

public:
    UAdvancedEmotionBlending();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Default blend mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    EEmotionBlendMode DefaultBlendMode = EEmotionBlendMode::Layered;

    /** Default transition duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    float DefaultTransitionDuration = 0.5f;

    /** Default transition curve */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    EEmotionTransitionCurve DefaultTransitionCurve = EEmotionTransitionCurve::EaseInOut;

    /** Emotional inertia (resistance to change) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EmotionalInertia = 0.3f;

    /** Maximum active layers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    int32 MaxActiveLayers = 10;

    /** Enable echobeat modulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    bool bEnableEchobeatModulation = true;

    /** Enable momentum tracking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmotionBlending|Config")
    bool bEnableMomentumTracking = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnEmotionLayerAdded OnLayerAdded;

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnEmotionLayerRemoved OnLayerRemoved;

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnEmotionTransitionStarted OnTransitionStarted;

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnEmotionTransitionCompleted OnTransitionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnExpressionRuleActivated OnRuleActivated;

    UPROPERTY(BlueprintAssignable, Category = "EmotionBlending|Events")
    FOnEmotionalStabilityChanged OnStabilityChanged;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    UPROPERTY(BlueprintReadOnly, Category = "EmotionBlending|Components")
    UUnrealAvatarCognition* AvatarCognition;

    UPROPERTY(BlueprintReadOnly, Category = "EmotionBlending|Components")
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY(BlueprintReadOnly, Category = "EmotionBlending|Components")
    UTensionalCouplingDynamics* CouplingDynamics;

    UPROPERTY(BlueprintReadOnly, Category = "EmotionBlending|Components")
    USensoryInputIntegration* SensoryIntegration;

    // ========================================
    // PUBLIC API - LAYER MANAGEMENT
    // ========================================

    /** Add an emotion layer */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Layers")
    void AddEmotionLayer(const FString& LayerName, const FEmotionVector& Emotion, float Weight = 1.0f, int32 Priority = 0);

    /** Remove an emotion layer */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Layers")
    void RemoveEmotionLayer(const FString& LayerName);

    /** Update an emotion layer */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Layers")
    void UpdateEmotionLayer(const FString& LayerName, const FEmotionVector& Emotion);

    /** Set layer weight */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Layers")
    void SetLayerWeight(const FString& LayerName, float Weight);

    /** Get layer by name */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Layers")
    FEmotionLayer GetEmotionLayer(const FString& LayerName) const;

    /** Get all active layers */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Layers")
    TArray<FEmotionLayer> GetActiveLayers() const;

    // ========================================
    // PUBLIC API - TRANSITIONS
    // ========================================

    /** Start transition to target emotion */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Transitions")
    void TransitionToEmotion(const FEmotionVector& TargetEmotion, float Duration = -1.0f, EEmotionTransitionCurve Curve = EEmotionTransitionCurve::EaseInOut);

    /** Start transition to named emotion state */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Transitions")
    void TransitionToNamedEmotion(EAvatarEmotionState EmotionState, float Intensity = 0.7f, float Duration = -1.0f);

    /** Cancel current transition */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Transitions")
    void CancelTransition();

    /** Is transition in progress */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Transitions")
    bool IsTransitioning() const;

    /** Get transition progress (0-1) */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Transitions")
    float GetTransitionProgress() const;

    // ========================================
    // PUBLIC API - EXPRESSION RULES
    // ========================================

    /** Add expression rule */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Rules")
    void AddExpressionRule(const FEmotionExpressionRule& Rule);

    /** Remove expression rule */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Rules")
    void RemoveExpressionRule(const FString& RuleName);

    /** Enable/disable rule */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Rules")
    void SetRuleEnabled(const FString& RuleName, bool bEnabled);

    /** Get active rules */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Rules")
    TArray<FString> GetActiveRuleNames() const;

    /** Initialize default expression rules */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Rules")
    void InitializeDefaultRules();

    // ========================================
    // PUBLIC API - EMOTION QUERIES
    // ========================================

    /** Get current blended emotion */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|State")
    FEmotionVector GetCurrentEmotion() const;

    /** Get current expression target */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|State")
    FExpressionTarget GetCurrentExpression() const;

    /** Get emotional stability */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|State")
    float GetEmotionalStability() const;

    /** Get emotional momentum */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|State")
    FEmotionVector GetEmotionalMomentum() const;

    /** Get full blending state */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|State")
    FEmotionBlendingState GetBlendingState() const;

    // ========================================
    // PUBLIC API - EMOTION UTILITIES
    // ========================================

    /** Convert named emotion to vector */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Utilities")
    static FEmotionVector NamedEmotionToVector(EAvatarEmotionState EmotionState, float Intensity = 0.7f);

    /** Get closest named emotion */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Utilities")
    static EAvatarEmotionState GetClosestNamedEmotion(const FEmotionVector& Emotion);

    /** Blend two emotion vectors */
    UFUNCTION(BlueprintPure, Category = "EmotionBlending|Utilities")
    static FEmotionVector BlendEmotions(const FEmotionVector& A, const FEmotionVector& B, float Alpha);

    // ========================================
    // PUBLIC API - INTEGRATION
    // ========================================

    /** Process echobeat step */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Integration")
    void ProcessEchobeatStep(int32 Step, const FEchobeatsFullState& EchobeatsState);

    /** Process sensory input */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Integration")
    void ProcessSensoryInput(const FSensoryInputData& Input);

    /** Process tensional coupling */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Integration")
    void ProcessTensionalCoupling(const FTetrahedralCouplingState& CouplingState);

    /** Apply current emotion to avatar */
    UFUNCTION(BlueprintCallable, Category = "EmotionBlending|Integration")
    void ApplyToAvatar();

protected:
    /** Current blending state */
    FEmotionBlendingState State;

    /** Expression rules */
    TArray<FEmotionExpressionRule> ExpressionRules;

    /** Previous emotion for momentum calculation */
    FEmotionVector PreviousEmotion;

    // Internal methods
    void FindComponentReferences();
    void BlendLayers();
    void UpdateTransition(float DeltaTime);
    void UpdateMomentum(float DeltaTime);
    void UpdateStability(float DeltaTime);
    void EvaluateExpressionRules();
    void DecayLayers(float DeltaTime);
    float EvaluateTransitionCurve(float T, EEmotionTransitionCurve Curve);
    FExpressionTarget BlendExpressionTargets(const TArray<FExpressionTarget>& Targets, const TArray<float>& Weights);
    void ModulateByEchobeat(int32 Step);
};
