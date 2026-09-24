/**
 * @file UnrealAvatarEmbodiment.h
 * @brief Header for 4E Embodied Cognition Avatar System in Unreal Engine
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealAvatarEmbodiment.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UCognitiveCycleManager;
class UEmbodied4ECognition;
class UEchobeatsStreamEngine;

/**
 * Body Part State - Physical state of a body part in the body schema
 */
USTRUCT(BlueprintType)
struct FBodyPartState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString PartName;

    UPROPERTY(BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float Awareness = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.5f;
};

/**
 * Embodied State - Physical body state for 4E cognition
 */
USTRUCT(BlueprintType)
struct FEmbodiedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FBodyPartState> BodySchema;

    UPROPERTY(BlueprintReadWrite)
    float ArousalLevel = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float Valence = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EnergyLevel = 0.7f;

    UPROPERTY(BlueprintReadWrite)
    float ProprioceptiveAwareness = 0.5f;
};

/**
 * Embedded State - Environmental coupling state
 */
USTRUCT(BlueprintType)
struct FEmbeddedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EnvironmentAwareness = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float SocialPresence = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FVector EnvironmentCenter = FVector::ZeroVector;
};

/**
 * Enacted State - Sensorimotor contingency state
 */
USTRUCT(BlueprintType)
struct FEnactedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    FString CurrentAction = TEXT("idle");

    UPROPERTY(BlueprintReadWrite)
    float ActionProgress = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FVector ActionTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> LearnedContingencies;
};

/**
 * Tool State - State of an integrated tool for extended cognition
 */
USTRUCT(BlueprintType)
struct FToolState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString ToolName;

    UPROPERTY(BlueprintReadWrite)
    float EngagementLevel = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bIsIntegrated = false;
};

/**
 * Extended State - Tool integration state
 */
USTRUCT(BlueprintType)
struct FExtendedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float OffloadingRatio = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    TArray<FToolState> ActiveTools;
};

/**
 * Visual Coupling State - Visual feedback state tied to cognition
 */
USTRUCT(BlueprintType)
struct FVisualCouplingState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float HairLuminance = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EyeGlow = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float TechPatternIntensity = 0.3f;

    UPROPERTY(BlueprintReadWrite)
    float Bioluminescence = 0.3f;

    UPROPERTY(BlueprintReadWrite)
    FVector ColorShift = FVector::ZeroVector;
};

/**
 * Avatar Expression State - Current expression state
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EmotionIntensities;

    UPROPERTY(BlueprintReadWrite)
    FString CognitiveMode;

    UPROPERTY(BlueprintReadWrite)
    FVector AttentionFocus = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float EyeGlowIntensity = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor AuraColor;

    UPROPERTY(BlueprintReadWrite)
    float AuraPulseRate = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float HairDynamicsMultiplier = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float BreathingRate = 12.0f;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> MicroExpressions;
};

/**
 * 4E Coherence State - Overall coherence across 4E dimensions
 */
USTRUCT(BlueprintType)
struct F4ECoherenceState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float EmbodiedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EmbeddedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EnactedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float ExtendedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float OverallCoherence = 0.5f;
};

/**
 * Sensory Input - Gathered sensory information
 */
USTRUCT(BlueprintType)
struct FSensoryInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float EnvironmentAwareness = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float SocialPresence = 0.0f;
};

/**
 * Sensorimotor Contingency - Learned action-outcome mapping
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString Action;

    UPROPERTY(BlueprintReadWrite)
    FSensoryInput InitialState;

    UPROPERTY(BlueprintReadWrite)
    FSensoryInput ResultState;

    UPROPERTY(BlueprintReadWrite)
    float PredictionError = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionCompleted, const FString&, ActionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsightRealized, float, Relevance);

/**
 * Unreal Avatar Embodiment Component
 * 
 * Implements full 4E embodied cognition for Unreal Engine avatars,
 * integrating with the Deep Tree Echo cognitive framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UUnrealAvatarEmbodiment : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnrealAvatarEmbodiment();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable embodiment processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    bool bEnableEmbodiment = true;

    /** Enable sensorimotor learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    bool bEnableSensorimotorLearning = true;

    /** Environment scan radius for embedded cognition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float EnvironmentScanRadius = 1000.0f;

    /** Action progress rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float ActionProgressRate = 1.0f;

    /** Visual smoothing rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float VisualSmoothingRate = 5.0f;

    /** Expression blend rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float ExpressionBlendRate = 3.0f;

    /** Prediction error threshold for learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float PredictionErrorThreshold = 0.1f;

    /** Maximum stored contingencies */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    int32 MaxStoredContingencies = 100;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when an action is completed */
    UPROPERTY(BlueprintAssignable, Category = "Embodiment|Events")
    FOnActionCompleted OnActionCompleted;

    /** Called when an insight is realized */
    UPROPERTY(BlueprintAssignable, Category = "Embodiment|Events")
    FOnInsightRealized OnInsightRealized;

    // ========================================
    // BLUEPRINT CALLABLE FUNCTIONS
    // ========================================

    /** Set emotion intensity */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Expression")
    void SetEmotion(const FString& Emotion, float Intensity);

    /** Initiate an action */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Action")
    void InitiateAction(const FString& Action, const FVector& Target);

    /** Register a tool for extended cognition */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Extended")
    void RegisterTool(const FString& ToolName, float InitialEngagement);

    /** Unregister a tool */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Extended")
    void UnregisterTool(const FString& ToolName);

    /** Get 4E coherence state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    F4ECoherenceState Get4ECoherence() const;

    /** Get expression state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    FAvatarExpressionState GetExpressionState() const;

    /** Get visual coupling state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    FVisualCouplingState GetVisualCouplingState() const;

protected:
    // Initialization
    void FindComponentReferences();
    void InitializeBodySchema();
    void Initialize4EStates();
    void InitializeVisualCoupling();

    // 4E State Updates
    void UpdateEmbodiedState(float DeltaTime);
    void UpdateEmbeddedState(float DeltaTime);
    void UpdateEnactedState(float DeltaTime);
    void UpdateExtendedState(float DeltaTime);

    // Sensorimotor Processing
    void ProcessSensorimotorLoop(float DeltaTime);
    FSensoryInput GatherSensoryInput();
    FSensoryInput PredictSensoryOutcome(const FString& Action);
    float ComputePredictionError(const FSensoryInput& Actual, const FSensoryInput& Predicted);
    void UpdateInternalModels(const FSensoryInput& Actual, const FSensoryInput& Predicted, float Error);
    void UpdateSensorimotorContingencies(float DeltaTime);

    // Visual Updates
    void UpdateVisualCoupling(float DeltaTime);
    void ApplyExpressionToAvatar(float DeltaTime);
    void UpdateTechElements(float DeltaTime);
    void UpdateBodySchemaFromMesh();

    // Action Management
    void CompleteCurrentAction();

    // Event Handlers
    UFUNCTION()
    void HandleCognitiveStepChanged(int32 NewStep, EEchobeatStepType StepType, EEchobeatMode Mode);
    
    UFUNCTION()
    void HandleRelevanceRealized(int32 Step, float Relevance);

private:
    // Component references
    UPROPERTY()
    USkeletalMeshComponent* SkeletalMesh = nullptr;

    UPROPERTY()
    UCognitiveCycleManager* CognitiveCycleManager = nullptr;

    UPROPERTY()
    UEmbodied4ECognition* Embodied4ECognition = nullptr;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine = nullptr;

    // 4E States
    FEmbodiedState EmbodiedState;
    FEmbeddedState EmbeddedState;
    FEnactedState EnactedState;
    FExtendedState ExtendedState;

    // Visual States
    FVisualCouplingState VisualState;
    FAvatarExpressionState ExpressionState;
    TMap<FString, float> TechElementStates;

    // Body Schema
    TMap<FString, FBodyPartState> BodyPartStates;

    // Sensorimotor Learning
    TArray<FSensorimotorContingency> LearnedContingencies;
    FSensoryInput LastSensoryInput;
    float LastPredictionError = 0.0f;

    // State
    bool bIsInitialized = false;
};
