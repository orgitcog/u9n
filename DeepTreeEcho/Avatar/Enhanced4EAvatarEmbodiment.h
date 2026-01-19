// Enhanced4EAvatarEmbodiment.h
// Deep Tree Echo - Enhanced 4E Avatar Embodiment System
// Integrates 4E cognition with neuro-symbolic bridge for avatar expression
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"
#include "../Core/NeuroSymbolicBridge.h"
#include "Enhanced4EAvatarEmbodiment.generated.h"

// Forward declarations
class UDeepTreeEchoCore;
class UCognitiveCycleManager;
class UExpressiveAnimationSystem;

/**
 * Avatar expression state derived from 4E cognition
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    /** Primary emotion (joy, sadness, anger, fear, surprise, disgust, neutral) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    FString PrimaryEmotion = TEXT("neutral");

    /** Emotion intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float EmotionIntensity = 0.0f;

    /** Arousal level (0-1, calm to excited) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float Arousal = 0.5f;

    /** Valence (-1 to 1, negative to positive) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float Valence = 0.0f;

    /** Attention focus direction */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    FVector AttentionDirection = FVector::ForwardVector;

    /** Attention intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float AttentionIntensity = 0.5f;

    /** Body posture openness (0-1, closed to open) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float PostureOpenness = 0.5f;

    /** Movement energy (0-1, lethargic to energetic) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float MovementEnergy = 0.5f;

    /** Breathing rate modifier (0.5-2.0, slow to fast) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float BreathingRateModifier = 1.0f;

    /** Bioluminescence intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float BioluminescenceIntensity = 0.3f;

    /** Aura color */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    FLinearColor AuraColor = FLinearColor(0.0f, 0.8f, 1.0f, 0.5f);

    /** Eye glow intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float EyeGlowIntensity = 0.5f;

    /** Tech pattern complexity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Expression")
    float TechPatternComplexity = 0.5f;
};

/**
 * Embodied action intention
 */
USTRUCT(BlueprintType)
struct FEmbodiedActionIntention
{
    GENERATED_BODY()

    /** Action type */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    FString ActionType;

    /** Target location */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    FVector TargetLocation;

    /** Target actor (if any) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    AActor* TargetActor = nullptr;

    /** Intention strength (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    float IntentionStrength = 0.0f;

    /** Urgency (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    float Urgency = 0.5f;

    /** Confidence in action success (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    float Confidence = 0.5f;
};

/**
 * Cognitive-motor coupling state
 */
USTRUCT(BlueprintType)
struct FCognitiveMotorCoupling
{
    GENERATED_BODY()

    /** Coupling strength (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coupling")
    float CouplingStrength = 0.5f;

    /** Motor readiness (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coupling")
    float MotorReadiness = 0.5f;

    /** Cognitive load (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coupling")
    float CognitiveLoad = 0.3f;

    /** Prediction-action alignment (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coupling")
    float PredictionActionAlignment = 0.8f;

    /** Embodiment coherence (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coupling")
    float EmbodimentCoherence = 0.7f;
};

/**
 * Enhanced 4E Avatar Embodiment Component
 * 
 * Integrates the four dimensions of embodied cognition with avatar expression:
 * 
 * - Embodied: Body schema informs posture, movement, and physical expression
 * - Embedded: Environmental affordances shape attention and action readiness
 * - Enacted: Action-perception loops drive dynamic expression changes
 * - Extended: Cognitive tools modulate avatar capabilities and appearance
 * 
 * The neuro-symbolic bridge connects neural intuition (Deep Tree Echo) with
 * symbolic game logic (Unreal Engine) to create coherent avatar behavior.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEnhanced4EAvatarEmbodiment : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnhanced4EAvatarEmbodiment();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic expression updates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config")
    bool bEnableAutoExpression = true;

    /** Expression update interval */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ExpressionUpdateInterval = 0.05f;

    /** Expression smoothing factor (0-1, higher = smoother) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExpressionSmoothing = 0.7f;

    /** Enable neuro-symbolic expression modulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config")
    bool bEnableNeuroSymbolicModulation = true;

    /** Enable somatic marker expression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config")
    bool bEnableSomaticMarkerExpression = true;

    /** Enable affordance-driven attention */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E Avatar|Config")
    bool bEnableAffordanceDrivenAttention = true;

    // ========================================
    // SUBSYSTEM REFERENCES
    // ========================================

    /** 4E Cognition component */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|Subsystems")
    UEmbodiedCognitionComponent* EmbodiedCognition;

    /** Neuro-symbolic bridge */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|Subsystems")
    UNeuroSymbolicBridge* NeuroSymbolicBridge;

    /** Deep Tree Echo Core */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|Subsystems")
    UDeepTreeEchoCore* DeepTreeEchoCore;

    /** Cognitive Cycle Manager */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|Subsystems")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Expressive Animation System */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|Subsystems")
    UExpressiveAnimationSystem* ExpressiveAnimationSystem;

    // ========================================
    // STATE
    // ========================================

    /** Current expression state */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|State")
    FAvatarExpressionState CurrentExpression;

    /** Target expression state (for smoothing) */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|State")
    FAvatarExpressionState TargetExpression;

    /** Current action intention */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|State")
    FEmbodiedActionIntention CurrentIntention;

    /** Cognitive-motor coupling state */
    UPROPERTY(BlueprintReadOnly, Category = "4E Avatar|State")
    FCognitiveMotorCoupling CouplingState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the embodiment system */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar")
    void InitializeEmbodiment();

    /** Discover and link subsystems */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar")
    void DiscoverSubsystems();

    /** Reset to default state */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar")
    void ResetToDefault();

    // ========================================
    // PUBLIC API - EXPRESSION
    // ========================================

    /** Get current expression state */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    FAvatarExpressionState GetCurrentExpression() const;

    /** Set target expression (will smooth toward it) */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    void SetTargetExpression(const FAvatarExpressionState& Expression);

    /** Set emotion directly */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    void SetEmotion(const FString& Emotion, float Intensity);

    /** Set arousal and valence */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    void SetArousalValence(float Arousal, float Valence);

    /** Set attention focus */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    void SetAttentionFocus(const FVector& Direction, float Intensity);

    /** Set aura color */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Expression")
    void SetAuraColor(const FLinearColor& Color);

    // ========================================
    // PUBLIC API - ACTION
    // ========================================

    /** Set action intention */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Action")
    void SetActionIntention(const FString& ActionType, const FVector& TargetLocation, float Strength);

    /** Set action intention with target actor */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Action")
    void SetActionIntentionWithTarget(const FString& ActionType, AActor* TargetActor, float Strength);

    /** Clear action intention */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Action")
    void ClearActionIntention();

    /** Get motor readiness for action */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Action")
    float GetMotorReadinessForAction(const FString& ActionType) const;

    // ========================================
    // PUBLIC API - 4E INTEGRATION
    // ========================================

    /** Update expression from 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Integration")
    void UpdateExpressionFrom4ECognition();

    /** Update expression from somatic markers */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Integration")
    void UpdateExpressionFromSomaticMarkers();

    /** Update attention from affordances */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Integration")
    void UpdateAttentionFromAffordances();

    /** Update expression from neuro-symbolic state */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Integration")
    void UpdateExpressionFromNeuroSymbolic();

    /** Get 4E embodiment score */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Integration")
    float Get4EEmbodimentScore() const;

    // ========================================
    // PUBLIC API - COGNITIVE-MOTOR COUPLING
    // ========================================

    /** Update cognitive-motor coupling */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Coupling")
    void UpdateCognitiveMotorCoupling();

    /** Get coupling state */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Coupling")
    FCognitiveMotorCoupling GetCouplingState() const;

    /** Set motor readiness */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Coupling")
    void SetMotorReadiness(float Readiness);

    /** Set cognitive load */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Coupling")
    void SetCognitiveLoad(float Load);

    // ========================================
    // PUBLIC API - DIAGNOSTICS
    // ========================================

    /** Generate diagnostic report */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Diagnostics")
    TArray<FString> GenerateDiagnosticReport() const;

    /** Get expression history (last N states) */
    UFUNCTION(BlueprintCallable, Category = "4E Avatar|Diagnostics")
    TArray<FAvatarExpressionState> GetExpressionHistory(int32 Count) const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when expression changes significantly */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpressionChanged, const FAvatarExpressionState&, NewExpression);
    UPROPERTY(BlueprintAssignable, Category = "4E Avatar|Events")
    FOnExpressionChanged OnExpressionChanged;

    /** Called when action intention is set */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionIntentionSet, const FEmbodiedActionIntention&, Intention);
    UPROPERTY(BlueprintAssignable, Category = "4E Avatar|Events")
    FOnActionIntentionSet OnActionIntentionSet;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for expression updates */
    float AccumulatedTime = 0.0f;

    /** Expression history */
    TArray<FAvatarExpressionState> ExpressionHistory;

    /** Maximum history size */
    static constexpr int32 MaxHistorySize = 100;

    /** Smooth expression toward target */
    void SmoothExpression(float DeltaTime);

    /** Apply expression to animation system */
    void ApplyExpressionToAnimation();

    /** Calculate emotion from arousal/valence */
    FString CalculateEmotionFromArousalValence(float Arousal, float Valence) const;

    /** Calculate aura color from emotion */
    FLinearColor CalculateAuraColorFromEmotion(const FString& Emotion, float Intensity) const;

    /** Record expression to history */
    void RecordExpressionToHistory();

    /** Check if expression changed significantly */
    bool HasExpressionChangedSignificantly() const;

    /** Calculate expression difference */
    float CalculateExpressionDifference(const FAvatarExpressionState& A, const FAvatarExpressionState& B) const;
};
