// PredictiveAdaptationEngine.h
// Advanced Predictive Adaptation for Deep Tree Echo
// Implements predictive processing, free energy minimization, and adaptive model updating

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PredictiveAdaptationEngine.generated.h"

// Forward declarations
class UEchobeatsStreamEngine;
class USensoryInputIntegration;
class UTensionalCouplingDynamics;
class UAdvancedEmotionBlending;
class UCognitiveMemoryManager;

/**
 * Prediction type
 */
UENUM(BlueprintType)
enum class EPredictionType : uint8
{
    Sensory         UMETA(DisplayName = "Sensory Prediction"),
    Motor           UMETA(DisplayName = "Motor Prediction"),
    Emotional       UMETA(DisplayName = "Emotional Prediction"),
    Cognitive       UMETA(DisplayName = "Cognitive Prediction"),
    Social          UMETA(DisplayName = "Social Prediction"),
    Temporal        UMETA(DisplayName = "Temporal Prediction")
};

/**
 * Adaptation strategy
 */
UENUM(BlueprintType)
enum class EAdaptationStrategy : uint8
{
    GradientDescent     UMETA(DisplayName = "Gradient Descent"),
    HebbianLearning     UMETA(DisplayName = "Hebbian Learning"),
    PredictiveCoding    UMETA(DisplayName = "Predictive Coding"),
    FreeEnergyMin       UMETA(DisplayName = "Free Energy Minimization"),
    ActiveInference     UMETA(DisplayName = "Active Inference"),
    MetaLearning        UMETA(DisplayName = "Meta-Learning")
};

/**
 * Model precision level
 */
UENUM(BlueprintType)
enum class EModelPrecision : uint8
{
    VeryLow     UMETA(DisplayName = "Very Low"),
    Low         UMETA(DisplayName = "Low"),
    Medium      UMETA(DisplayName = "Medium"),
    High        UMETA(DisplayName = "High"),
    VeryHigh    UMETA(DisplayName = "Very High")
};

/**
 * Prediction error signal
 */
USTRUCT(BlueprintType)
struct FPredictionError
{
    GENERATED_BODY()

    /** Unique error ID */
    UPROPERTY(BlueprintReadOnly)
    int32 ErrorID = 0;

    /** Prediction type */
    UPROPERTY(BlueprintReadOnly)
    EPredictionType Type = EPredictionType::Sensory;

    /** Error magnitude (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Magnitude = 0.0f;

    /** Error direction vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> Direction;

    /** Precision weighting (confidence in error) */
    UPROPERTY(BlueprintReadOnly)
    float Precision = 1.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;

    /** Source model ID */
    UPROPERTY(BlueprintReadOnly)
    FString SourceModelID;

    /** Echobeat step when error occurred */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 0;

    /** Stream index (0-2) */
    UPROPERTY(BlueprintReadOnly)
    int32 StreamIndex = 0;
};

/**
 * Generative model state
 */
USTRUCT(BlueprintType)
struct FGenerativeModelState
{
    GENERATED_BODY()

    /** Model ID */
    UPROPERTY(BlueprintReadOnly)
    FString ModelID;

    /** Model type */
    UPROPERTY(BlueprintReadOnly)
    EPredictionType Type = EPredictionType::Sensory;

    /** Current state vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> StateVector;

    /** Prior beliefs (expected state) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> PriorBeliefs;

    /** Posterior beliefs (updated state) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> PosteriorBeliefs;

    /** Model precision */
    UPROPERTY(BlueprintReadOnly)
    EModelPrecision Precision = EModelPrecision::Medium;

    /** Free energy estimate */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergy = 0.0f;

    /** Model complexity */
    UPROPERTY(BlueprintReadOnly)
    float Complexity = 0.0f;

    /** Accuracy (negative log likelihood) */
    UPROPERTY(BlueprintReadOnly)
    float Accuracy = 0.0f;

    /** Last update time */
    UPROPERTY(BlueprintReadOnly)
    float LastUpdateTime = 0.0f;

    /** Update count */
    UPROPERTY(BlueprintReadOnly)
    int32 UpdateCount = 0;
};

/**
 * Adaptation event
 */
USTRUCT(BlueprintType)
struct FAdaptationEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadOnly)
    int32 EventID = 0;

    /** Strategy used */
    UPROPERTY(BlueprintReadOnly)
    EAdaptationStrategy Strategy = EAdaptationStrategy::PredictiveCoding;

    /** Target model ID */
    UPROPERTY(BlueprintReadOnly)
    FString TargetModelID;

    /** Triggering error */
    UPROPERTY(BlueprintReadOnly)
    FPredictionError TriggeringError;

    /** Parameter changes (delta) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ParameterDeltas;

    /** Learning rate used */
    UPROPERTY(BlueprintReadOnly)
    float LearningRate = 0.0f;

    /** Free energy before */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergyBefore = 0.0f;

    /** Free energy after */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergyAfter = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Sys6 synchronized prediction state
 */
USTRUCT(BlueprintType)
struct FSys6PredictionState
{
    GENERATED_BODY()

    /** Current LCM step (0-29) */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Current echobeat step (1-12) */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 1;

    /** Dyad state (A or B) */
    UPROPERTY(BlueprintReadOnly)
    FString DyadState = TEXT("A");

    /** Triad state (1, 2, or 3) */
    UPROPERTY(BlueprintReadOnly)
    int32 TriadState = 1;

    /** Active stream predictions */
    UPROPERTY(BlueprintReadOnly)
    TArray<FGenerativeModelState> StreamPredictions;

    /** Cross-stream coherence */
    UPROPERTY(BlueprintReadOnly)
    float CrossStreamCoherence = 0.0f;

    /** Total system free energy */
    UPROPERTY(BlueprintReadOnly)
    float TotalFreeEnergy = 0.0f;
};

/**
 * Meta-learning state
 */
USTRUCT(BlueprintType)
struct FMetaLearningState
{
    GENERATED_BODY()

    /** Learning rate adaptation */
    UPROPERTY(BlueprintReadOnly)
    float AdaptedLearningRate = 0.1f;

    /** Strategy effectiveness scores */
    UPROPERTY(BlueprintReadOnly)
    TMap<EAdaptationStrategy, float> StrategyEffectiveness;

    /** Model precision adjustments */
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, float> PrecisionAdjustments;

    /** Exploration-exploitation balance */
    UPROPERTY(BlueprintReadOnly)
    float ExplorationBalance = 0.5f;

    /** Confidence in meta-model */
    UPROPERTY(BlueprintReadOnly)
    float MetaConfidence = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPredictionError, const FPredictionError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModelUpdated, const FGenerativeModelState&, Model);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAdaptationEvent, const FAdaptationEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFreeEnergyChanged, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetaLearningUpdate, const FMetaLearningState&, State);

/**
 * Predictive Adaptation Engine
 * 
 * Implements advanced predictive processing and adaptation for the Deep Tree Echo
 * cognitive architecture. Based on the Free Energy Principle and Active Inference.
 * 
 * Key features:
 * - Hierarchical generative models for prediction
 * - Precision-weighted prediction error processing
 * - Free energy minimization through model updating
 * - Active inference for action selection
 * - Meta-learning for strategy optimization
 * - Sys6 synchronized prediction across 30-step LCM cycle
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UPredictiveAdaptationEngine : public UActorComponent
{
    GENERATED_BODY()

public:
    UPredictiveAdaptationEngine();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Base learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Learning")
    float BaseLearningRate = 0.1f;

    /** Minimum learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Learning")
    float MinLearningRate = 0.001f;

    /** Maximum learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Learning")
    float MaxLearningRate = 0.5f;

    /** Prediction error threshold for adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Learning")
    float ErrorThreshold = 0.1f;

    /** Free energy target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|FreeEnergy")
    float FreeEnergyTarget = 0.0f;

    /** Complexity penalty weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|FreeEnergy")
    float ComplexityPenalty = 0.1f;

    /** Enable meta-learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|MetaLearning")
    bool bEnableMetaLearning = true;

    /** Meta-learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|MetaLearning")
    float MetaLearningRate = 0.01f;

    /** Enable sys6 synchronization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Sys6")
    bool bEnableSys6Sync = true;

    /** State vector dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 StateVectorDimension = 64;

    /** Maximum models per type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 MaxModelsPerType = 8;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionError OnPredictionError;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnModelUpdated OnModelUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAdaptationEvent OnAdaptationEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFreeEnergyChanged OnFreeEnergyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMetaLearningUpdate OnMetaLearningUpdate;

    // ========================================
    // PUBLIC API - PREDICTION
    // ========================================

    /** Generate prediction for given type */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    TArray<float> GeneratePrediction(EPredictionType Type, const TArray<float>& Context);

    /** Generate prediction for specific model */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    TArray<float> GenerateModelPrediction(const FString& ModelID, const TArray<float>& Context);

    /** Compute prediction error */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    FPredictionError ComputePredictionError(EPredictionType Type, const TArray<float>& Predicted, const TArray<float>& Observed);

    /** Process incoming sensory data */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    void ProcessSensoryInput(const TArray<float>& SensoryData, EPredictionType Type);

    /** Get current prediction for type */
    UFUNCTION(BlueprintPure, Category = "Prediction")
    TArray<float> GetCurrentPrediction(EPredictionType Type) const;

    // ========================================
    // PUBLIC API - MODEL MANAGEMENT
    // ========================================

    /** Create new generative model */
    UFUNCTION(BlueprintCallable, Category = "Models")
    FString CreateModel(EPredictionType Type, const FString& ModelName);

    /** Get model state */
    UFUNCTION(BlueprintPure, Category = "Models")
    FGenerativeModelState GetModelState(const FString& ModelID) const;

    /** Get all models of type */
    UFUNCTION(BlueprintPure, Category = "Models")
    TArray<FGenerativeModelState> GetModelsOfType(EPredictionType Type) const;

    /** Update model with observation */
    UFUNCTION(BlueprintCallable, Category = "Models")
    void UpdateModel(const FString& ModelID, const TArray<float>& Observation);

    /** Set model precision */
    UFUNCTION(BlueprintCallable, Category = "Models")
    void SetModelPrecision(const FString& ModelID, EModelPrecision Precision);

    /** Remove model */
    UFUNCTION(BlueprintCallable, Category = "Models")
    void RemoveModel(const FString& ModelID);

    // ========================================
    // PUBLIC API - ADAPTATION
    // ========================================

    /** Trigger adaptation for model */
    UFUNCTION(BlueprintCallable, Category = "Adaptation")
    FAdaptationEvent TriggerAdaptation(const FString& ModelID, const FPredictionError& Error);

    /** Set adaptation strategy */
    UFUNCTION(BlueprintCallable, Category = "Adaptation")
    void SetAdaptationStrategy(EAdaptationStrategy Strategy);

    /** Get current adaptation strategy */
    UFUNCTION(BlueprintPure, Category = "Adaptation")
    EAdaptationStrategy GetAdaptationStrategy() const;

    /** Get adaptation history */
    UFUNCTION(BlueprintPure, Category = "Adaptation")
    TArray<FAdaptationEvent> GetAdaptationHistory(int32 Count) const;

    /** Get current learning rate */
    UFUNCTION(BlueprintPure, Category = "Adaptation")
    float GetCurrentLearningRate() const;

    // ========================================
    // PUBLIC API - FREE ENERGY
    // ========================================

    /** Compute free energy for model */
    UFUNCTION(BlueprintPure, Category = "FreeEnergy")
    float ComputeFreeEnergy(const FString& ModelID) const;

    /** Get total system free energy */
    UFUNCTION(BlueprintPure, Category = "FreeEnergy")
    float GetTotalFreeEnergy() const;

    /** Minimize free energy through action */
    UFUNCTION(BlueprintCallable, Category = "FreeEnergy")
    TArray<float> MinimizeFreeEnergyAction(const FString& ModelID);

    /** Minimize free energy through perception */
    UFUNCTION(BlueprintCallable, Category = "FreeEnergy")
    void MinimizeFreeEnergyPerception(const FString& ModelID);

    // ========================================
    // PUBLIC API - SYS6 INTEGRATION
    // ========================================

    /** Process sys6 step */
    UFUNCTION(BlueprintCallable, Category = "Sys6")
    void ProcessSys6Step(int32 LCMStep);

    /** Get sys6 prediction state */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    FSys6PredictionState GetSys6PredictionState() const;

    /** Synchronize predictions across streams */
    UFUNCTION(BlueprintCallable, Category = "Sys6")
    void SynchronizeStreamPredictions();

    /** Get stream-specific prediction */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    TArray<float> GetStreamPrediction(int32 StreamIndex) const;

    // ========================================
    // PUBLIC API - META-LEARNING
    // ========================================

    /** Get meta-learning state */
    UFUNCTION(BlueprintPure, Category = "MetaLearning")
    FMetaLearningState GetMetaLearningState() const;

    /** Update meta-learning from adaptation result */
    UFUNCTION(BlueprintCallable, Category = "MetaLearning")
    void UpdateMetaLearning(const FAdaptationEvent& Event);

    /** Get recommended strategy */
    UFUNCTION(BlueprintPure, Category = "MetaLearning")
    EAdaptationStrategy GetRecommendedStrategy() const;

    /** Reset meta-learning */
    UFUNCTION(BlueprintCallable, Category = "MetaLearning")
    void ResetMetaLearning();

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Process echobeat step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void ProcessEchobeatStep(int32 Step);

    /** Get prediction for echobeat phase */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    TArray<float> GetEchobeatPhasePrediction(int32 Phase) const;

    /** Update from echobeats stream state */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void UpdateFromStreamState(int32 StreamIndex, const TArray<float>& StreamState);

protected:
    // Component references
    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USensoryInputIntegration* SensoryIntegration;

    UPROPERTY()
    UTensionalCouplingDynamics* CouplingDynamics;

    UPROPERTY()
    UAdvancedEmotionBlending* EmotionBlending;

    UPROPERTY()
    UCognitiveMemoryManager* MemoryManager;

    // Internal state
    TMap<FString, FGenerativeModelState> GenerativeModels;
    TArray<FPredictionError> RecentErrors;
    TArray<FAdaptationEvent> AdaptationHistory;
    FSys6PredictionState Sys6State;
    FMetaLearningState MetaState;
    EAdaptationStrategy CurrentStrategy = EAdaptationStrategy::PredictiveCoding;
    float CurrentLearningRate = 0.1f;
    int32 NextModelID = 1;
    int32 NextErrorID = 1;
    int32 NextEventID = 1;

    // Internal methods
    void FindComponentReferences();
    void InitializeDefaultModels();
    void UpdatePredictions(float DeltaTime);
    void ProcessPredictionErrors();
    void ApplyAdaptation(const FString& ModelID, const FPredictionError& Error);
    void UpdateMetaState();
    float ComputePrecisionWeight(EModelPrecision Precision) const;
    TArray<float> ComputeGradient(const FString& ModelID, const FPredictionError& Error);
    void ApplyGradientDescent(const FString& ModelID, const TArray<float>& Gradient);
    void ApplyHebbianUpdate(const FString& ModelID, const TArray<float>& PreState, const TArray<float>& PostState);
    void ApplyPredictiveCodingUpdate(const FString& ModelID, const FPredictionError& Error);
    void ApplyFreeEnergyMinimization(const FString& ModelID);
    void ApplyActiveInference(const FString& ModelID);
    void ComputeSys6StepParameters(int32 LCMStep);
    float ComputeCrossStreamCoherence() const;
};
