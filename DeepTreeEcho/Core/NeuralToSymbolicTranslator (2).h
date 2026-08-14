// NeuralToSymbolicTranslator.h
// Neural-to-Symbolic Translation Layer for Deep Tree Echo
// Feature F1.1.1: Converts neural network outputs into symbolic representations
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/CognitiveTypes.h"
#include "NeuralToSymbolicTranslator.generated.h"

// Forward declarations
class UNeuroSymbolicBridge;

// ============================================================================
// SYMBOLIC REPRESENTATION TYPES
// ============================================================================

/**
 * Symbolic atom representing a discrete concept from neural activation
 */
USTRUCT(BlueprintType)
struct FSymbolicAtom
{
    GENERATED_BODY()

    /** Unique identifier for this atom */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FString AtomID;

    /** Atom type (Concept, Action, State, Entity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FString AtomType;

    /** Confidence score (0-1) from neural activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    float Confidence = 0.0f;

    /** Source feature index from neural tensor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    int32 SourceFeatureIndex = -1;

    /** Activation value that created this atom */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    float ActivationValue = 0.0f;

    /** Timestamp of creation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    float Timestamp = 0.0f;

    /** Metadata properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    TMap<FString, FString> Properties;
};

/**
 * Symbolic predicate representing a relation or rule
 */
USTRUCT(BlueprintType)
struct FPredicate
{
    GENERATED_BODY()

    /** Predicate identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    FString PredicateID;

    /** Predicate name (e.g., "IsActive", "HasProperty") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    FString PredicateName;

    /** Arguments (atom IDs) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    TArray<FString> Arguments;

    /** Truth value (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    float TruthValue = 0.0f;

    /** Confidence score */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    float Confidence = 0.0f;

    /** Source activation pattern */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predicate")
    TArray<int32> SourceFeatureIndices;
};

/**
 * Neural state containing tensor and activation data
 */
USTRUCT(BlueprintType)
struct FNeuralState
{
    GENERATED_BODY()

    /** State identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString StateID;

    /** Primary activation vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<float> Activations;

    /** Hidden state vector (if applicable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<float> HiddenState;

    /** Layer-wise activations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<TArray<float>> LayerActivations;

    /** Overall confidence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Confidence = 0.0f;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Timestamp = 0.0f;
};

/**
 * Activation map containing named activation vectors
 */
USTRUCT(BlueprintType)
struct FActivationMap
{
    GENERATED_BODY()

    /** Named activation vectors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activations")
    TMap<FString, TArray<float>> Activations;

    /** Confidence scores per activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activations")
    TMap<FString, float> ConfidenceScores;

    /** Metadata */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activations")
    TMap<FString, FString> Metadata;
};

/**
 * Symbolic state containing translated atoms and predicates
 */
USTRUCT(BlueprintType)
struct FSymbolicState
{
    GENERATED_BODY()

    /** State identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString StateID;

    /** Symbolic atoms */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FSymbolicAtom> Atoms;

    /** Predicates (relations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FPredicate> Predicates;

    /** Overall confidence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Confidence = 0.0f;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Timestamp = 0.0f;
};

// ============================================================================
// TRANSLATION CONFIGURATION
// ============================================================================

/**
 * Configuration for neural-to-symbolic translation
 */
USTRUCT(BlueprintType)
struct FTranslationConfig
{
    GENERATED_BODY()

    /** Activation threshold for creating atoms (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationThreshold = 0.3f;

    /** Confidence threshold for keeping atoms (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ConfidenceThreshold = 0.5f;

    /** Maximum atoms to generate per translation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 MaxAtomsPerTranslation = 100;

    /** Enable batch processing optimization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation")
    bool bEnableBatchProcessing = true;

    /** Batch size for parallel processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "1", ClampMax = "512"))
    int32 BatchSize = 32;

    /** Enable uncertainty propagation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation")
    bool bPropagateUncertainty = true;

    /** Discretization bins for activation values */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "2", ClampMax = "256"))
    int32 DiscretizationBins = 10;

    /** Co-activation threshold for predicate inference (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CoActivationThreshold = 0.2f;
};

/**
 * Translation metrics
 */
USTRUCT(BlueprintType)
struct FTranslationMetrics
{
    GENERATED_BODY()

    /** Total translations performed */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalTranslations = 0;

    /** Average translation latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageLatency = 0.0f;

    /** Peak translation latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PeakLatency = 0.0f;

    /** Total atoms created */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalAtomsCreated = 0;

    /** Total predicates created */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalPredicatesCreated = 0;

    /** Average atoms per translation */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageAtomsPerTranslation = 0.0f;

    /** Batch processing efficiency (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float BatchEfficiency = 0.0f;
};

// ============================================================================
// NEURAL TO SYMBOLIC TRANSLATOR COMPONENT
// ============================================================================

/**
 * Neural-to-Symbolic Translation Layer
 * 
 * Implements Feature F1.1.1: Converts neural network outputs (tensors,
 * activations, predictions) into symbolic representations that Unreal Engine
 * can process.
 * 
 * Key Features:
 * - Tensor-to-symbol mapping with activation threshold discretization
 * - Symbolic atom factory for neural outputs
 * - Confidence/uncertainty propagation
 * - Batch translation for efficiency
 * - Target latency: <0.5ms per translation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNeuralToSymbolicTranslator : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeuralToSymbolicTranslator();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Translation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translator|Config")
    FTranslationConfig Config;

    /** Enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translator|Debug")
    bool bEnableDebugLogging = false;

    /** Enable performance monitoring */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Translator|Debug")
    bool bEnablePerformanceMonitoring = true;

    // ========================================
    // STATE
    // ========================================

    /** Current translation metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Translator|State")
    FTranslationMetrics Metrics;

    /** Reference to neuro-symbolic bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Translator|State")
    UNeuroSymbolicBridge* Bridge;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize the translator */
    UFUNCTION(BlueprintCallable, Category = "Translator")
    void Initialize();

    /** Discover and link to neuro-symbolic bridge */
    UFUNCTION(BlueprintCallable, Category = "Translator")
    void DiscoverBridge();

    /** Reset metrics */
    UFUNCTION(BlueprintCallable, Category = "Translator")
    void ResetMetrics();

    // ========================================
    // CORE TRANSLATION API (Feature F1.1.1)
    // ========================================

    /**
     * Translate a tensor (activation vector) to a symbolic atom
     * Target latency: <0.5ms
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|API")
    FSymbolicAtom TranslateTensor(const TArray<float>& Tensor);

    /**
     * Translate activation map to array of predicates
     * Handles multiple named activations efficiently
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|API")
    TArray<FPredicate> TranslateActivations(const FActivationMap& Activations);

    /**
     * Translate complete neural state to symbolic state
     * Includes atoms, predicates, and confidence propagation
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|API")
    FSymbolicState TranslateNeuralState(const FNeuralState& State);

    // ========================================
    // BATCH TRANSLATION (Efficiency Optimization)
    // ========================================

    /**
     * Batch translate multiple tensors to atoms
     * Optimized for parallel processing
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Batch")
    TArray<FSymbolicAtom> BatchTranslateTensors(const TArray<TArray<float>>& Tensors);

    /**
     * Batch translate multiple neural states
     * Processes multiple states efficiently
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Batch")
    TArray<FSymbolicState> BatchTranslateStates(const TArray<FNeuralState>& States);

    // ========================================
    // ATOM FACTORY
    // ========================================

    /**
     * Create symbolic atom from single activation value
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Factory")
    FSymbolicAtom CreateAtomFromActivation(float ActivationValue, int32 FeatureIndex, const FString& AtomType);

    /**
     * Create multiple atoms from activation vector
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Factory")
    TArray<FSymbolicAtom> CreateAtomsFromActivationVector(const TArray<float>& Activations, const FString& AtomType);

    /**
     * Create predicate from atom pair
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Factory")
    FPredicate CreatePredicateFromAtoms(const FSymbolicAtom& Atom1, const FSymbolicAtom& Atom2, const FString& PredicateName);

    // ========================================
    // DISCRETIZATION
    // ========================================

    /**
     * Discretize activation value into bins
     */
    UFUNCTION(BlueprintPure, Category = "Translator|Discretization")
    int32 DiscretizeActivation(float ActivationValue) const;

    /**
     * Apply activation threshold to determine atom creation
     */
    UFUNCTION(BlueprintPure, Category = "Translator|Discretization")
    bool ShouldCreateAtom(float ActivationValue) const;

    /**
     * Calculate confidence from activation
     */
    UFUNCTION(BlueprintPure, Category = "Translator|Discretization")
    float CalculateConfidence(float ActivationValue) const;

    // ========================================
    // UNCERTAINTY PROPAGATION
    // ========================================

    /**
     * Propagate uncertainty from neural to symbolic representation
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Uncertainty")
    float PropagateUncertainty(float NeuralConfidence, int32 FeatureCount) const;

    /**
     * Calculate combined uncertainty for predicate
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Uncertainty")
    float CalculatePredicateUncertainty(const TArray<FSymbolicAtom>& InputAtoms) const;

    // ========================================
    // METRICS
    // ========================================

    /**
     * Get current translation metrics
     */
    UFUNCTION(BlueprintPure, Category = "Translator|Metrics")
    FTranslationMetrics GetMetrics() const;

    /**
     * Check if meeting latency target (<0.5ms)
     */
    UFUNCTION(BlueprintPure, Category = "Translator|Metrics")
    bool IsMeetingLatencyTarget() const;

    /**
     * Generate diagnostic report
     */
    UFUNCTION(BlueprintCallable, Category = "Translator|Metrics")
    TArray<FString> GenerateDiagnosticReport() const;

protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Atom ID counter */
    int32 AtomIDCounter = 0;

    /** Predicate ID counter */
    int32 PredicateIDCounter = 0;

    /** Latency samples for averaging */
    TArray<float> LatencySamples;

    /** Maximum latency samples to keep */
    static constexpr int32 MaxLatencySamples = 1000;

    /** Maximum random range for ID generation */
    static constexpr int32 MaxIDRandomRange = 10000;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Generate unique atom ID */
    FString GenerateAtomID();

    /** Generate unique predicate ID */
    FString GeneratePredicateID();

    /** Check if atom is valid (has non-empty ID) */
    bool IsValidAtom(const FSymbolicAtom& Atom) const;

    /** Record translation latency */
    void RecordLatency(float LatencyMs);

    /** Update metrics */
    void UpdateMetrics();

    /** Infer predicates from atoms */
    TArray<FPredicate> InferPredicatesFromAtoms(const TArray<FSymbolicAtom>& Atoms);

    /** Extract dominant features from activation vector */
    TArray<int32> ExtractDominantFeatures(const TArray<float>& Activations, int32 MaxFeatures) const;

    /** Calculate activation statistics */
    void CalculateActivationStats(const TArray<float>& Activations, float& OutMean, float& OutStdDev) const;
};
