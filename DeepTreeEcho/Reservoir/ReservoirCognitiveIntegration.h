#pragma once

/**
 * Reservoir Cognitive Integration
 * 
 * Deep integration of ReservoirCpp Echo State Network with Deep Tree Echo
 * cognitive framework. Implements reservoir computing as the substrate
 * for relevance realization and 4E embodied cognition.
 * 
 * Architecture:
 * - Multiple ESN reservoirs for different cognitive functions
 * - Hierarchical reservoir structure matching OEIS A000081 nesting
 * - Bidirectional coupling with System 5 tetradic architecture
 * - Real-time adaptation through online learning
 * 
 * Reservoir Functions:
 * - Salience Reservoir: Computes attention and salience landscapes
 * - Affordance Reservoir: Detects and evaluates environmental affordances
 * - Prediction Reservoir: Temporal prediction for active inference
 * - Integration Reservoir: Cross-modal binding and coherence
 * 
 * @see DeepTreeEchoReservoir for base ESN implementation
 * @see System5CognitiveIntegration for tetradic coupling
 * @see RelevanceRealizationComponent for salience integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "../System5/System5CognitiveIntegration.h"
#include "ReservoirCognitiveIntegration.generated.h"

/**
 * Reservoir Function Type
 */
UENUM(BlueprintType)
enum class EReservoirFunction : uint8
{
    /** Salience computation */
    Salience UMETA(DisplayName = "Salience"),
    
    /** Affordance detection */
    Affordance UMETA(DisplayName = "Affordance"),
    
    /** Temporal prediction */
    Prediction UMETA(DisplayName = "Prediction"),
    
    /** Cross-modal integration */
    Integration UMETA(DisplayName = "Integration")
};

/**
 * Hierarchical Reservoir Level
 * Corresponds to OEIS A000081 nesting structure
 */
USTRUCT(BlueprintType)
struct FHierarchicalReservoirLevel
{
    GENERATED_BODY()

    /** Level index (1-4) */
    UPROPERTY(BlueprintReadWrite)
    int32 Level = 1;

    /** Number of reservoir units at this level */
    UPROPERTY(BlueprintReadWrite)
    int32 UnitCount = 1;

    /** Reservoir state vectors */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> ReservoirStates;

    /** Level coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Coupling strength to adjacent levels */
    UPROPERTY(BlueprintReadWrite)
    float InterLevelCoupling = 0.5f;
};

/**
 * Reservoir Cognitive State
 * Captures the cognitive state computed by reservoirs
 */
USTRUCT(BlueprintType)
struct FReservoirCognitiveState
{
    GENERATED_BODY()

    /** Salience map (key: entity, value: salience 0-1) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SalienceMap;

    /** Detected affordances with confidence */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> AffordanceMap;

    /** Prediction confidence for current context */
    UPROPERTY(BlueprintReadWrite)
    float PredictionConfidence = 0.5f;

    /** Integration coherence across modalities */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationCoherence = 0.5f;

    /** Relevance frame (current cognitive context) */
    UPROPERTY(BlueprintReadWrite)
    FString RelevanceFrame;

    /** Attention allocation */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> AttentionAllocation;
};

/**
 * Reservoir Learning Parameters
 */
USTRUCT(BlueprintType)
struct FReservoirLearningParams
{
    GENERATED_BODY()

    /** Online learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001", ClampMax = "0.1"))
    float LearningRate = 0.01f;

    /** Regularization strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Regularization = 0.001f;

    /** Forgetting factor for online learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.9", ClampMax = "1.0"))
    float ForgettingFactor = 0.99f;

    /** Enable Hebbian learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableHebbianLearning = true;

    /** Hebbian learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float HebbianRate = 0.001f;
};

/**
 * Reservoir Cognitive Integration Component
 * 
 * Provides deep integration between ReservoirCpp ESN and the
 * Deep Tree Echo cognitive framework for embodied cognition.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UReservoirCognitiveIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UReservoirCognitiveIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable reservoir cognitive processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config")
    bool bEnableReservoirProcessing = true;

    /** Reservoir size for each function */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config", meta = (ClampMin = "10", ClampMax = "1000"))
    int32 ReservoirSize = 100;

    /** Spectral radius for reservoir dynamics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config", meta = (ClampMin = "0.1", ClampMax = "1.5"))
    float SpectralRadius = 0.9f;

    /** Input scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float InputScaling = 1.0f;

    /** Leaking rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float LeakingRate = 0.3f;

    /** Learning parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config")
    FReservoirLearningParams LearningParams;

    /** Enable hierarchical processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config")
    bool bEnableHierarchicalProcessing = true;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to base reservoir system */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|Components")
    UDeepTreeEchoReservoir* BaseReservoir;

    /** Reference to System 5 integration */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|Components")
    USystem5CognitiveIntegration* System5Integration;

    // ========================================
    // STATE
    // ========================================

    /** Current cognitive state from reservoirs */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    FReservoirCognitiveState CognitiveState;

    /** Hierarchical reservoir levels */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    TArray<FHierarchicalReservoirLevel> HierarchicalLevels;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize reservoir cognitive system */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    void InitializeReservoirCognition();

    /** Initialize hierarchical structure */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    void InitializeHierarchicalStructure();

    // ========================================
    // PUBLIC API - PROCESSING
    // ========================================

    /** Process sensory input through reservoirs */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Processing")
    void ProcessSensoryInput(const TArray<float>& SensoryInput, const FString& Modality);

    /** Compute salience landscape */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Processing")
    TMap<FString, float> ComputeSalienceLandscape(const TArray<FString>& Entities);

    /** Detect affordances in current context */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Processing")
    TMap<FString, float> DetectAffordances(const TArray<FString>& PotentialAffordances);

    /** Generate temporal prediction */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Processing")
    TArray<float> GeneratePrediction(int32 HorizonSteps);

    /** Compute cross-modal integration */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Processing")
    float ComputeIntegrationCoherence();

    // ========================================
    // PUBLIC API - LEARNING
    // ========================================

    /** Update reservoirs with feedback */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Learning")
    void UpdateWithFeedback(const TArray<float>& Target, EReservoirFunction Function);

    /** Apply Hebbian learning to reservoir weights */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Learning")
    void ApplyHebbianLearning();

    /** Adapt reservoir to new context */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Learning")
    void AdaptToContext(const FString& NewContext);

    // ========================================
    // PUBLIC API - TETRADIC COUPLING
    // ========================================

    /** Couple reservoir state with tetradic structure */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Tetradic")
    void CoupleWithTetradicState();

    /** Map reservoir output to triadic faces */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Tetradic")
    void MapToTriadicFaces();

    /** Get reservoir state for specific vertex */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Tetradic")
    TArray<float> GetVertexReservoirState(int32 VertexId) const;

    // ========================================
    // PUBLIC API - STATE ACCESS
    // ========================================

    /** Get current cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|State")
    FReservoirCognitiveState GetCognitiveState() const { return CognitiveState; }

    /** Get reservoir state summary */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|State")
    TArray<float> GetReservoirStateSummary() const;

    /** Get hierarchical coherence */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|State")
    float GetHierarchicalCoherence() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Salience reservoir state */
    TArray<float> SalienceReservoirState;

    /** Affordance reservoir state */
    TArray<float> AffordanceReservoirState;

    /** Prediction reservoir state */
    TArray<float> PredictionReservoirState;

    /** Integration reservoir state */
    TArray<float> IntegrationReservoirState;

    /** Reservoir weight matrices (simplified representation) */
    TArray<TArray<float>> ReservoirWeights;

    /** Output weight matrices */
    TArray<TArray<float>> OutputWeights;

    /** Input history for prediction */
    TArray<TArray<float>> InputHistory;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Initialize reservoir weights */
    void InitializeReservoirWeights();

    /** Update reservoir state with input */
    TArray<float> UpdateReservoirState(const TArray<float>& Input, TArray<float>& State);

    /** Compute reservoir output */
    TArray<float> ComputeReservoirOutput(const TArray<float>& State, int32 FunctionIndex);

    /** Apply leaky integration */
    void ApplyLeakyIntegration(TArray<float>& State, const TArray<float>& NewState);

    /** Compute hierarchical propagation */
    void PropagateHierarchically();

    /** Update cognitive state from reservoir outputs */
    void UpdateCognitiveState();

    /** Generate random sparse matrix */
    TArray<float> GenerateSparseRandomVector(int32 Size, float Sparsity);
};
