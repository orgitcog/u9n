// SymbolicToNeuralEncoder.h
// Symbolic-to-Neural Encoding Interface for Deep Tree Echo
// Feature F1.1.2: Converts Unreal Engine symbolic game state into neural-compatible representations
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/CognitiveTypes.h"
#include "SymbolicToNeuralEncoder.generated.h"

// Forward declarations
class UNeuroSymbolicBridge;

// ============================================================================
// GAME STATE TYPES
// ============================================================================

/**
 * Game entity properties for encoding
 */
USTRUCT(BlueprintType)
struct FGameEntityProperties
{
    GENERATED_BODY()

    /** Entity unique identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FString EntityID;

    /** Entity type/class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FString EntityType;

    /** Spatial properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FVector Velocity = FVector::ZeroVector;

    /** Categorical properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    TMap<FString, FString> CategoricalProperties;

    /** Continuous properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    TMap<FString, float> ContinuousProperties;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    float Timestamp = 0.0f;
};

/**
 * Game event for encoding
 */
USTRUCT(BlueprintType)
struct FGameEvent
{
    GENERATED_BODY()

    /** Event unique identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FString EventID;

    /** Event type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FString EventType;

    /** Involved entities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    TArray<FString> InvolvedEntities;

    /** Event properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    TMap<FString, FString> Properties;

    /** Event magnitude/intensity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    float Magnitude = 1.0f;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    float Timestamp = 0.0f;
};

/**
 * Relational data between entities
 */
USTRUCT(BlueprintType)
struct FEntityRelation
{
    GENERATED_BODY()

    /** Source entity ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relation")
    FString SourceEntityID;

    /** Target entity ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relation")
    FString TargetEntityID;

    /** Relation type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relation")
    FString RelationType;

    /** Relation strength (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relation")
    float RelationStrength = 1.0f;

    /** Bidirectional flag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relation")
    bool bIsBidirectional = false;
};

/**
 * Complete game state snapshot
 */
USTRUCT(BlueprintType)
struct FGameStateSnapshot
{
    GENERATED_BODY()

    /** Snapshot identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString SnapshotID;

    /** All entities in the game state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FGameEntityProperties> Entities;

    /** Recent events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FGameEvent> Events;

    /** Relations between entities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FEntityRelation> Relations;

    /** Global state properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TMap<FString, float> GlobalProperties;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Timestamp = 0.0f;
};

// ============================================================================
// NEURAL ENCODING TYPES
// ============================================================================

/**
 * Tensor embedding output
 */
USTRUCT(BlueprintType)
struct FTensorEmbedding
{
    GENERATED_BODY()

    /** Embedding identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    FString EmbeddingID;

    /** Embedding vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    TArray<float> Vector;

    /** Embedding type (Entity, Event, Relation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    FString EmbeddingType;

    /** Source identifier (entity ID, event ID, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    FString SourceID;

    /** Confidence score */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    float Confidence = 1.0f;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embedding")
    float Timestamp = 0.0f;
};

/**
 * Neural state output from encoding
 */
USTRUCT(BlueprintType)
struct FEncodedNeuralState
{
    GENERATED_BODY()

    /** State identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString StateID;

    /** Primary encoded vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<float> EncodedVector;

    /** Entity embeddings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FTensorEmbedding> EntityEmbeddings;

    /** Event embeddings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FTensorEmbedding> EventEmbeddings;

    /** Relational embeddings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<FTensorEmbedding> RelationalEmbeddings;

    /** Temporal context vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    TArray<float> TemporalContext;

    /** Overall confidence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Confidence = 1.0f;

    /** Timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Timestamp = 0.0f;
};

// ============================================================================
// ENCODING CONFIGURATION
// ============================================================================

/**
 * Configuration for categorical encoding
 */
USTRUCT(BlueprintType)
struct FCategoricalEncodingConfig
{
    GENERATED_BODY()

    /** Use one-hot encoding (true) or multi-hot (false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bUseOneHot = true;

    /** Maximum categories to track */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "2", ClampMax = "1000"))
    int32 MaxCategories = 100;

    /** Enable category learning (add new categories dynamically) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableCategoryLearning = true;

    /** Unknown category handling: use zero vector or learned embedding */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bUseZeroForUnknown = false;
};

/**
 * Configuration for continuous encoding
 */
USTRUCT(BlueprintType)
struct FContinuousEncodingConfig
{
    GENERATED_BODY()

    /** Normalization method (None, MinMax, StandardScaling) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FString NormalizationMethod = TEXT("StandardScaling");

    /** Enable adaptive normalization (update statistics online) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableAdaptiveNormalization = true;

    /** Clamp outliers to N standard deviations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float OutlierClampStdDev = 3.0f;

    /** Enable logarithmic scaling for large values */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableLogScaling = false;
};

/**
 * Configuration for relational encoding
 */
USTRUCT(BlueprintType)
struct FRelationalEncodingConfig
{
    GENERATED_BODY()

    /** Encoding method (Graph, Attention, Concatenation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FString EncodingMethod = TEXT("Graph");

    /** Maximum relations to encode per entity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxRelationsPerEntity = 10;

    /** Relation embedding dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "8", ClampMax = "512"))
    int32 RelationEmbeddingDim = 64;

    /** Enable edge type differentiation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bDifferentiateEdgeTypes = true;

    /** Attention heads for attention-based encoding */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "1", ClampMax = "16"))
    int32 AttentionHeads = 4;
};

/**
 * Configuration for temporal encoding
 */
USTRUCT(BlueprintType)
struct FTemporalEncodingConfig
{
    GENERATED_BODY()

    /** Encoding method (Sequence, Delta, Fourier) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FString EncodingMethod = TEXT("Delta");

    /** Temporal window size (number of frames/states) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "1", ClampMax = "100"))
    int32 TemporalWindowSize = 10;

    /** Enable temporal smoothing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableTemporalSmoothing = true;

    /** Smoothing factor (0-1, higher = more smoothing) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SmoothingFactor = 0.3f;

    /** Enable velocity encoding (compute derivatives) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEncodeVelocity = true;
};

/**
 * Master encoding configuration
 */
USTRUCT(BlueprintType)
struct FEncodingConfig
{
    GENERATED_BODY()

    /** Entity embedding dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "8", ClampMax = "1024"))
    int32 EntityEmbeddingDim = 128;

    /** Event embedding dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "8", ClampMax = "1024"))
    int32 EventEmbeddingDim = 64;

    /** Categorical encoding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FCategoricalEncodingConfig CategoricalConfig;

    /** Continuous encoding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FContinuousEncodingConfig ContinuousConfig;

    /** Relational encoding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FRelationalEncodingConfig RelationalConfig;

    /** Temporal encoding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    FTemporalEncodingConfig TemporalConfig;

    /** Enable batch processing optimization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableBatchProcessing = true;

    /** Batch size for parallel processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding", meta = (ClampMin = "1", ClampMax = "512"))
    int32 BatchSize = 32;

    /** Enable streaming mode for real-time updates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoding")
    bool bEnableStreamingMode = true;
};

/**
 * Encoding metrics
 */
USTRUCT(BlueprintType)
struct FEncodingMetrics
{
    GENERATED_BODY()

    /** Total encodings performed */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalEncodings = 0;

    /** Average encoding latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageLatency = 0.0f;

    /** Peak encoding latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PeakLatency = 0.0f;

    /** Total entity embeddings created */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalEntityEmbeddings = 0;

    /** Total event embeddings created */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalEventEmbeddings = 0;

    /** Total relational embeddings created */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalRelationalEmbeddings = 0;

    /** Average embeddings per encoding */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageEmbeddingsPerEncoding = 0.0f;

    /** Streaming throughput (encodings/second) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float StreamingThroughput = 0.0f;

    /** Batch processing efficiency (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float BatchEfficiency = 0.0f;
};

// ============================================================================
// SYMBOLIC TO NEURAL ENCODER COMPONENT
// ============================================================================

/**
 * Symbolic-to-Neural Encoding Interface
 * 
 * Implements Feature F1.1.2: Converts Unreal Engine's symbolic game state
 * (entities, events, properties, relations) into neural-compatible tensor
 * representations for Deep Tree Echo processing.
 * 
 * Key Features:
 * - Entity embedding system with learned representations
 * - Categorical encoding (one-hot, multi-hot)
 * - Continuous feature encoding (normalization, scaling)
 * - Relational encoding (graph-based, attention-based)
 * - Temporal state encoding (sequence, delta, velocity)
 * - Streaming encoder for real-time game state updates
 * - Target latency: <0.5ms per encoding operation
 * 
 * Encoding Types:
 * - Categorical: Entity types, properties, states
 * - Continuous: Positions, velocities, health, resources
 * - Relational: Entity-entity connections, graph structure
 * - Temporal: State sequences, change over time
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USymbolicToNeuralEncoder : public UActorComponent
{
    GENERATED_BODY()

public:
    USymbolicToNeuralEncoder();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Encoding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoder|Config")
    FEncodingConfig Config;

    /** Enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoder|Debug")
    bool bEnableDebugLogging = false;

    /** Enable performance monitoring */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoder|Debug")
    bool bEnablePerformanceMonitoring = true;

    // ========================================
    // STATE
    // ========================================

    /** Current encoding metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Encoder|State")
    FEncodingMetrics Metrics;

    /** Reference to neuro-symbolic bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Encoder|State")
    UNeuroSymbolicBridge* Bridge;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize the encoder */
    UFUNCTION(BlueprintCallable, Category = "Encoder")
    void Initialize();

    /** Discover and link to neuro-symbolic bridge */
    UFUNCTION(BlueprintCallable, Category = "Encoder")
    void DiscoverBridge();

    /** Reset metrics and learned embeddings */
    UFUNCTION(BlueprintCallable, Category = "Encoder")
    void ResetEncoder();

    /** Reset metrics only */
    UFUNCTION(BlueprintCallable, Category = "Encoder")
    void ResetMetrics();

    // ========================================
    // CORE ENCODING API (Feature F1.1.2)
    // ========================================

    /**
     * Encode game entity properties to neural embedding
     * Target latency: <0.5ms
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|API")
    FTensorEmbedding EncodeEntity(const FGameEntityProperties& Entity);

    /**
     * Encode game event to neural embedding
     * Handles temporal and contextual encoding
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|API")
    FTensorEmbedding EncodeEvent(const FGameEvent& Event);

    /**
     * Encode entity relation to neural embedding
     * Supports various relational encoding methods
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|API")
    FTensorEmbedding EncodeRelation(const FEntityRelation& Relation);

    /**
     * Encode complete game state snapshot to neural state
     * Includes all entities, events, and relations
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|API")
    FEncodedNeuralState EncodeGameState(const FGameStateSnapshot& GameState);

    // ========================================
    // CATEGORICAL ENCODING
    // ========================================

    /**
     * Encode categorical property to one-hot vector
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Categorical")
    TArray<float> EncodeCategoricalOneHot(const FString& Category, const FString& PropertyName);

    /**
     * Encode multiple categorical properties to multi-hot vector
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Categorical")
    TArray<float> EncodeCategoricalMultiHot(const TArray<FString>& Categories, const FString& PropertyName);

    /**
     * Get or create categorical embedding for category
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Categorical")
    TArray<float> GetCategoryEmbedding(const FString& Category, const FString& PropertyName);

    // ========================================
    // CONTINUOUS ENCODING
    // ========================================

    /**
     * Encode continuous value with normalization
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Continuous")
    float EncodeContinuousValue(float Value, const FString& PropertyName);

    /**
     * Encode continuous vector (e.g., position, velocity)
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Continuous")
    TArray<float> EncodeContinuousVector(const FVector& Vector, const FString& PropertyName);

    /**
     * Encode multiple continuous properties to feature vector
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Continuous")
    TArray<float> EncodeContinuousProperties(const TMap<FString, float>& Properties);

    // ========================================
    // RELATIONAL ENCODING
    // ========================================

    /**
     * Encode graph structure from entity relations
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Relational")
    TArray<float> EncodeGraphStructure(const TArray<FEntityRelation>& Relations);

    /**
     * Encode entity context using attention mechanism
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Relational")
    TArray<float> EncodeEntityContext(const FString& EntityID, const TArray<FEntityRelation>& Relations);

    /**
     * Create relational embedding from entity pair
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Relational")
    TArray<float> CreateRelationalEmbedding(const FString& SourceID, const FString& TargetID, const FString& RelationType);

    // ========================================
    // TEMPORAL ENCODING
    // ========================================

    /**
     * Encode temporal sequence of states
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Temporal")
    TArray<float> EncodeTemporalSequence(const TArray<FGameStateSnapshot>& StateSequence);

    /**
     * Encode state delta (change from previous state)
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Temporal")
    TArray<float> EncodeStateDelta(const FGameStateSnapshot& CurrentState, const FGameStateSnapshot& PreviousState);

    /**
     * Encode velocity/change rate
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Temporal")
    TArray<float> EncodeVelocity(const FVector& CurrentPos, const FVector& PreviousPos, float DeltaTime);

    // ========================================
    // STREAMING ENCODER
    // ========================================

    /**
     * Stream encode entity update (incremental encoding)
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Streaming")
    void StreamEncodeEntityUpdate(const FGameEntityProperties& Entity);

    /**
     * Stream encode event (add to temporal buffer)
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Streaming")
    void StreamEncodeEvent(const FGameEvent& Event);

    /**
     * Flush streaming buffer and get aggregated neural state
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Streaming")
    FEncodedNeuralState FlushStreamingBuffer();

    /**
     * Get current streaming buffer size
     */
    UFUNCTION(BlueprintPure, Category = "Encoder|Streaming")
    int32 GetStreamingBufferSize() const;

    // ========================================
    // BATCH ENCODING
    // ========================================

    /**
     * Batch encode multiple entities
     * Optimized for parallel processing
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Batch")
    TArray<FTensorEmbedding> BatchEncodeEntities(const TArray<FGameEntityProperties>& Entities);

    /**
     * Batch encode multiple events
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Batch")
    TArray<FTensorEmbedding> BatchEncodeEvents(const TArray<FGameEvent>& Events);

    /**
     * Batch encode multiple game states
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Batch")
    TArray<FEncodedNeuralState> BatchEncodeGameStates(const TArray<FGameStateSnapshot>& States);

    // ========================================
    // EMBEDDING MANAGEMENT
    // ========================================

    /**
     * Register custom entity type embedding
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Embeddings")
    void RegisterEntityTypeEmbedding(const FString& EntityType, const TArray<float>& Embedding);

    /**
     * Get entity type embedding (or create if doesn't exist)
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Embeddings")
    TArray<float> GetEntityTypeEmbedding(const FString& EntityType);

    /**
     * Clear all learned embeddings
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Embeddings")
    void ClearEmbeddings();

    // ========================================
    // METRICS
    // ========================================

    /**
     * Get current encoding metrics
     */
    UFUNCTION(BlueprintPure, Category = "Encoder|Metrics")
    FEncodingMetrics GetMetrics() const;

    /**
     * Check if meeting latency target (<0.5ms)
     */
    UFUNCTION(BlueprintPure, Category = "Encoder|Metrics")
    bool IsMeetingLatencyTarget() const;

    /**
     * Generate diagnostic report
     */
    UFUNCTION(BlueprintCallable, Category = "Encoder|Metrics")
    TArray<FString> GenerateDiagnosticReport() const;

protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Entity type embeddings (learned) */
    TMap<FString, TArray<float>> EntityTypeEmbeddings;

    /** Category mappings for categorical encoding */
    TMap<FString, TMap<FString, int32>> CategoryMappings; // PropertyName -> Category -> Index

    /** Category embedding vectors (for unknown category handling) */
    TMap<FString, TMap<FString, TArray<float>>> CategoryEmbeddings;

    /** Continuous property statistics (for normalization) */
    TMap<FString, TPair<float, float>> ContinuousStats; // PropertyName -> (Mean, StdDev)

    /** Streaming buffer for real-time updates */
    TArray<FTensorEmbedding> StreamingBuffer;

    /** Previous game state for delta encoding */
    FGameStateSnapshot PreviousGameState;

    /** Embedding ID counter */
    int32 EmbeddingIDCounter = 0;

    /** Latency samples for averaging */
    TArray<float> LatencySamples;

    /** Maximum latency samples to keep */
    static constexpr int32 MaxLatencySamples = 1000;

    /** Maximum streaming buffer size */
    static constexpr int32 MaxStreamingBufferSize = 1000;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Generate unique embedding ID */
    FString GenerateEmbeddingID();

    /** Initialize entity type embedding */
    TArray<float> InitializeEntityEmbedding(const FString& EntityType);

    /** Update continuous property statistics */
    void UpdateContinuousStats(const FString& PropertyName, float Value);

    /** Normalize continuous value using statistics */
    float NormalizeContinuous(float Value, const FString& PropertyName);

    /** Apply temporal smoothing to vector */
    TArray<float> ApplyTemporalSmoothing(const TArray<float>& Current, const TArray<float>& Previous);

    /** Compute attention weights for relational encoding */
    TArray<float> ComputeAttentionWeights(const TArray<FTensorEmbedding>& Embeddings, const TArray<float>& QueryEmbedding);

    /** Aggregate embeddings using attention */
    TArray<float> AggregateWithAttention(const TArray<FTensorEmbedding>& Embeddings, const TArray<float>& AttentionWeights);

    /** Record encoding latency */
    void RecordLatency(float LatencyMs);

    /** Update metrics */
    void UpdateMetrics();

    /** Get or initialize category index */
    int32 GetOrCreateCategoryIndex(const FString& Category, const FString& PropertyName);

    /** Create zero vector of specified dimension */
    TArray<float> CreateZeroVector(int32 Dimension) const;

    /** Create random embedding vector */
    TArray<float> CreateRandomEmbedding(int32 Dimension) const;

    /** Concatenate multiple vectors */
    TArray<float> ConcatenateVectors(const TArray<TArray<float>>& Vectors) const;

    /** Compute vector mean */
    TArray<float> ComputeVectorMean(const TArray<TArray<float>>& Vectors) const;
};
