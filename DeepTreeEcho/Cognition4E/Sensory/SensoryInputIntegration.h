// SensoryInputIntegration.h
// External sensory input integration system for Deep Tree Echo avatar
// Connects environmental perception to cognitive processing

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Echobeats/EchobeatsStreamEngine.h"
#include "../../Echobeats/TensionalCouplingDynamics.h"
#include "SensoryInputIntegration.generated.h"

/**
 * Sensory modality type
 */
UENUM(BlueprintType)
enum class ESensoryModality : uint8
{
    Visual          UMETA(DisplayName = "Visual - Sight"),
    Auditory        UMETA(DisplayName = "Auditory - Hearing"),
    Proprioceptive  UMETA(DisplayName = "Proprioceptive - Body Position"),
    Vestibular      UMETA(DisplayName = "Vestibular - Balance/Motion"),
    Tactile         UMETA(DisplayName = "Tactile - Touch"),
    Interoceptive   UMETA(DisplayName = "Interoceptive - Internal State"),
    Social          UMETA(DisplayName = "Social - Agent Detection"),
    Semantic        UMETA(DisplayName = "Semantic - Language/Meaning")
};

/**
 * Attention priority level
 */
UENUM(BlueprintType)
enum class EAttentionPriority : uint8
{
    Background  UMETA(DisplayName = "Background - Passive monitoring"),
    Low         UMETA(DisplayName = "Low - Peripheral awareness"),
    Medium      UMETA(DisplayName = "Medium - Active monitoring"),
    High        UMETA(DisplayName = "High - Focused attention"),
    Critical    UMETA(DisplayName = "Critical - Immediate response required")
};

/**
 * Sensory event type
 */
UENUM(BlueprintType)
enum class ESensoryEventType : uint8
{
    Onset       UMETA(DisplayName = "Onset - New stimulus detected"),
    Change      UMETA(DisplayName = "Change - Existing stimulus modified"),
    Offset      UMETA(DisplayName = "Offset - Stimulus ended"),
    Sustained   UMETA(DisplayName = "Sustained - Continuous stimulus"),
    Predicted   UMETA(DisplayName = "Predicted - Anticipated stimulus")
};

/**
 * Sensory input data
 */
USTRUCT(BlueprintType)
struct FSensoryInputData
{
    GENERATED_BODY()

    /** Unique input ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InputID = 0;

    /** Sensory modality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESensoryModality Modality = ESensoryModality::Visual;

    /** Event type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESensoryEventType EventType = ESensoryEventType::Onset;

    /** Attention priority */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttentionPriority Priority = EAttentionPriority::Medium;

    /** Stimulus intensity (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.5f;

    /** Stimulus location (world space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    /** Stimulus direction (relative to avatar) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RelativeDirection = FVector::ForwardVector;

    /** Feature vector (modality-specific encoding) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> FeatureVector;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Duration (for sustained stimuli) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 0.0f;

    /** Source actor (if applicable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* SourceActor = nullptr;

    /** Semantic label (if applicable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SemanticLabel;

    /** Emotional valence (-1 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Novelty score (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoveltyScore = 0.5f;
};

/**
 * Processed sensory percept
 */
USTRUCT(BlueprintType)
struct FSensoryPercept
{
    GENERATED_BODY()

    /** Source input ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SourceInputID = 0;

    /** Modality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESensoryModality Modality = ESensoryModality::Visual;

    /** Integrated feature vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> IntegratedFeatures;

    /** Salience score (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Salience = 0.0f;

    /** Relevance to current goals (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GoalRelevance = 0.0f;

    /** Affordance potential (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AffordancePotential = 0.0f;

    /** Predicted outcome valence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictedValence = 0.0f;

    /** Confidence in percept */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Associated memory traces */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> MemoryTraceIDs;

    /** Bound to echobeat step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BoundEchobeatStep = 0;
};

/**
 * Multimodal binding state
 */
USTRUCT(BlueprintType)
struct FMultimodalBinding
{
    GENERATED_BODY()

    /** Binding ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BindingID = 0;

    /** Bound percept IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> BoundPerceptIDs;

    /** Modalities involved */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ESensoryModality> Modalities;

    /** Binding strength (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BindingStrength = 0.0f;

    /** Temporal coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TemporalCoherence = 1.0f;

    /** Spatial coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpatialCoherence = 1.0f;

    /** Unified object representation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> UnifiedRepresentation;

    /** Semantic interpretation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SemanticInterpretation;
};

/**
 * Attention focus state
 */
USTRUCT(BlueprintType)
struct FAttentionFocusState
{
    GENERATED_BODY()

    /** Primary focus target (percept ID) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PrimaryFocusID = -1;

    /** Secondary focus targets */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> SecondaryFocusIDs;

    /** Focus location (world space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector FocusLocation = FVector::ZeroVector;

    /** Focus direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector FocusDirection = FVector::ForwardVector;

    /** Attention breadth (0 = narrow, 1 = broad) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionBreadth = 0.5f;

    /** Attention depth (processing intensity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionDepth = 0.5f;

    /** Vigilance level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VigilanceLevel = 0.5f;

    /** Time at current focus */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FocusDuration = 0.0f;
};

/**
 * Sensory integration full state
 */
USTRUCT(BlueprintType)
struct FSensoryIntegrationState
{
    GENERATED_BODY()

    /** Active sensory inputs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryInputData> ActiveInputs;

    /** Processed percepts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryPercept> Percepts;

    /** Multimodal bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FMultimodalBinding> Bindings;

    /** Current attention state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAttentionFocusState AttentionState;

    /** Overall sensory load (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SensoryLoad = 0.0f;

    /** Prediction error (surprise) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictionError = 0.0f;

    /** Current echobeat step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentEchobeatStep = 1;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSensoryInputReceived, int32, InputID, ESensoryModality, Modality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPerceptFormed, int32, PerceptID, float, Salience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultimodalBindingFormed, int32, BindingID, float, Strength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttentionShifted, int32, NewFocusID, FVector, FocusLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSalienceThresholdCrossed, int32, PerceptID, float, Salience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPredictionErrorSpike, float, ErrorMagnitude);

/**
 * Sensory Input Integration Component
 * 
 * Integrates external sensory inputs with the cognitive processing system.
 * 
 * Key features:
 * - Multi-modal sensory processing (visual, auditory, proprioceptive, etc.)
 * - Attention-based filtering and prioritization
 * - Multimodal binding for unified object perception
 * - Integration with echobeats cognitive loop
 * - Predictive processing with error signals
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USensoryInputIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USensoryInputIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Feature vector size for percepts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    int32 FeatureVectorSize = 64;

    /** Maximum active inputs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    int32 MaxActiveInputs = 50;

    /** Maximum percepts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    int32 MaxPercepts = 20;

    /** Salience threshold for attention */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SalienceThreshold = 0.6f;

    /** Binding temporal window (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    float BindingTemporalWindow = 0.1f;

    /** Binding spatial threshold (units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    float BindingSpatialThreshold = 100.0f;

    /** Enable predictive processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config")
    bool bEnablePredictiveProcessing = true;

    /** Prediction learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensory|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionLearningRate = 0.1f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnSensoryInputReceived OnInputReceived;

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnPerceptFormed OnPerceptFormed;

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnMultimodalBindingFormed OnBindingFormed;

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnAttentionShifted OnAttentionShifted;

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnSalienceThresholdCrossed OnSalienceThresholdCrossed;

    UPROPERTY(BlueprintAssignable, Category = "Sensory|Events")
    FOnPredictionErrorSpike OnPredictionErrorSpike;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    UPROPERTY(BlueprintReadOnly, Category = "Sensory|Components")
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY(BlueprintReadOnly, Category = "Sensory|Components")
    UTensionalCouplingDynamics* CouplingDynamics;

    // ========================================
    // PUBLIC API - INPUT PROCESSING
    // ========================================

    /** Register a new sensory input */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Input")
    int32 RegisterSensoryInput(const FSensoryInputData& InputData);

    /** Update an existing sensory input */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Input")
    void UpdateSensoryInput(int32 InputID, const FSensoryInputData& InputData);

    /** Remove a sensory input */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Input")
    void RemoveSensoryInput(int32 InputID);

    /** Get sensory input by ID */
    UFUNCTION(BlueprintPure, Category = "Sensory|Input")
    FSensoryInputData GetSensoryInput(int32 InputID) const;

    /** Get all inputs by modality */
    UFUNCTION(BlueprintPure, Category = "Sensory|Input")
    TArray<FSensoryInputData> GetInputsByModality(ESensoryModality Modality) const;

    // ========================================
    // PUBLIC API - PERCEPT OPERATIONS
    // ========================================

    /** Process inputs into percepts */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Percepts")
    void ProcessInputsToPercepts();

    /** Get percept by ID */
    UFUNCTION(BlueprintPure, Category = "Sensory|Percepts")
    FSensoryPercept GetPercept(int32 PerceptID) const;

    /** Get most salient percepts */
    UFUNCTION(BlueprintPure, Category = "Sensory|Percepts")
    TArray<FSensoryPercept> GetMostSalientPercepts(int32 Count) const;

    /** Get percepts by modality */
    UFUNCTION(BlueprintPure, Category = "Sensory|Percepts")
    TArray<FSensoryPercept> GetPerceptsByModality(ESensoryModality Modality) const;

    // ========================================
    // PUBLIC API - MULTIMODAL BINDING
    // ========================================

    /** Attempt to bind percepts */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Binding")
    void AttemptMultimodalBinding();

    /** Get binding by ID */
    UFUNCTION(BlueprintPure, Category = "Sensory|Binding")
    FMultimodalBinding GetBinding(int32 BindingID) const;

    /** Get all active bindings */
    UFUNCTION(BlueprintPure, Category = "Sensory|Binding")
    TArray<FMultimodalBinding> GetActiveBindings() const;

    /** Force bind specific percepts */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Binding")
    int32 ForceBindPercepts(const TArray<int32>& PerceptIDs);

    // ========================================
    // PUBLIC API - ATTENTION
    // ========================================

    /** Shift attention to percept */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Attention")
    void ShiftAttentionToPercept(int32 PerceptID);

    /** Shift attention to location */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Attention")
    void ShiftAttentionToLocation(const FVector& Location);

    /** Set attention breadth */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Attention")
    void SetAttentionBreadth(float Breadth);

    /** Set vigilance level */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Attention")
    void SetVigilanceLevel(float Level);

    /** Get current attention state */
    UFUNCTION(BlueprintPure, Category = "Sensory|Attention")
    FAttentionFocusState GetAttentionState() const;

    // ========================================
    // PUBLIC API - PREDICTIVE PROCESSING
    // ========================================

    /** Generate predictions for next step */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Prediction")
    void GeneratePredictions();

    /** Compute prediction error */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Prediction")
    float ComputePredictionError();

    /** Update prediction model */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Prediction")
    void UpdatePredictionModel();

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Process sensory input for echobeat step */
    UFUNCTION(BlueprintCallable, Category = "Sensory|Integration")
    void ProcessForEchobeatStep(int32 Step);

    /** Get sensory output for cognitive stream */
    UFUNCTION(BlueprintPure, Category = "Sensory|Integration")
    TArray<float> GetSensoryOutputForStream(ECognitiveStreamType StreamType) const;

    /** Map percept to cognitive term */
    UFUNCTION(BlueprintPure, Category = "Sensory|Integration")
    ESystem4Term MapPerceptToTerm(const FSensoryPercept& Percept) const;

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get full integration state */
    UFUNCTION(BlueprintPure, Category = "Sensory|State")
    FSensoryIntegrationState GetIntegrationState() const;

    /** Get sensory load */
    UFUNCTION(BlueprintPure, Category = "Sensory|State")
    float GetSensoryLoad() const;

    /** Get prediction error */
    UFUNCTION(BlueprintPure, Category = "Sensory|State")
    float GetPredictionError() const;

protected:
    /** Current integration state */
    FSensoryIntegrationState State;

    /** Next input ID */
    int32 NextInputID = 1;

    /** Next percept ID */
    int32 NextPerceptID = 1;

    /** Next binding ID */
    int32 NextBindingID = 1;

    /** Prediction model state */
    TArray<float> PredictionState;

    /** Previous percept states for prediction */
    TMap<int32, TArray<float>> PreviousPerceptStates;

    // Internal methods
    void FindComponentReferences();
    FSensoryPercept ProcessInputToPercept(const FSensoryInputData& Input);
    float ComputeSalience(const FSensoryInputData& Input);
    float ComputeGoalRelevance(const FSensoryInputData& Input);
    float ComputeAffordancePotential(const FSensoryInputData& Input);
    bool ShouldBindPercepts(const FSensoryPercept& A, const FSensoryPercept& B);
    FMultimodalBinding CreateBinding(const TArray<FSensoryPercept>& Percepts);
    void UpdateAttention(float DeltaTime);
    void DecayPercepts(float DeltaTime);
    void ComputeSensoryLoad();
    TArray<float> IntegrateFeatures(const TArray<FSensoryInputData>& Inputs);
};
