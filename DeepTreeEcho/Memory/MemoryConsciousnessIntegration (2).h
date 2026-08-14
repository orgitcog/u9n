// MemoryConsciousnessIntegration.h
// Bridges Hypergraph Memory System with Recursive Mutual Awareness consciousness streams
// Implements memory-guided attention and consciousness-driven memory consolidation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HypergraphMemorySystem.h"
#include "../../UnrealEcho/Consciousness/RecursiveMutualAwarenessSystem.h"
#include "MemoryConsciousnessIntegration.generated.h"

// Forward declarations
class UDeepTreeEchoReservoir;

/**
 * Memory access mode for consciousness streams
 */
UENUM(BlueprintType)
enum class EMemoryAccessMode : uint8
{
    /** Perceptual encoding - Stream 1 writes to memory */
    PerceptualEncode    UMETA(DisplayName = "Perceptual Encoding"),
    /** Action retrieval - Stream 2 reads procedural memory */
    ActionRetrieval     UMETA(DisplayName = "Action Retrieval"),
    /** Reflective consolidation - Stream 3 consolidates and abstracts */
    ReflectiveConsolidate UMETA(DisplayName = "Reflective Consolidation"),
    /** Cross-stream association - All streams create links */
    CrossStreamAssociation UMETA(DisplayName = "Cross-Stream Association")
};

/**
 * Working memory slot for consciousness integration
 */
USTRUCT(BlueprintType)
struct FConsciousnessWorkingMemorySlot
{
    GENERATED_BODY()

    /** Memory node ID from hypergraph */
    UPROPERTY(BlueprintReadWrite)
    int64 NodeID = 0;

    /** Source consciousness stream */
    UPROPERTY(BlueprintReadWrite)
    EConsciousnessStream SourceStream = EConsciousnessStream::Perceiving;

    /** Current activation level */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Time since loaded */
    UPROPERTY(BlueprintReadWrite)
    float TimeLoaded = 0.0f;

    /** Relevance to current goal */
    UPROPERTY(BlueprintReadWrite)
    float GoalRelevance = 0.0f;

    /** Attentional weight from consciousness */
    UPROPERTY(BlueprintReadWrite)
    float AttentionalWeight = 0.0f;
};

/**
 * Stream memory binding - Associates stream states with memory nodes
 */
USTRUCT(BlueprintType)
struct FStreamMemoryBinding
{
    GENERATED_BODY()

    /** Stream type */
    UPROPERTY(BlueprintReadWrite)
    EConsciousnessStream StreamType = EConsciousnessStream::Perceiving;

    /** Active memory nodes for this stream */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> ActiveNodes;

    /** Memory encoding queue */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> EncodingQueue;

    /** Retrieval cue (embedding vector) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RetrievalCue;

    /** Stream-specific activation modifier */
    UPROPERTY(BlueprintReadWrite)
    float ActivationModifier = 1.0f;
};

/**
 * Memory-guided attention weights
 */
USTRUCT(BlueprintType)
struct FMemoryGuidedAttention
{
    GENERATED_BODY()

    /** Attention weights from episodic memory */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EpisodicWeights;

    /** Attention weights from semantic memory */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SemanticWeights;

    /** Attention weights from procedural memory */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> ProceduralWeights;

    /** Attention weights from intentional memory (goals) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> IntentionalWeights;

    /** Combined attention vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CombinedAttention;
};

/**
 * Consciousness-driven memory event
 */
USTRUCT(BlueprintType)
struct FConsciousnessMemoryEvent
{
    GENERATED_BODY()

    /** Event type */
    UPROPERTY(BlueprintReadWrite)
    EMemoryAccessMode AccessMode = EMemoryAccessMode::PerceptualEncode;

    /** Source stream */
    UPROPERTY(BlueprintReadWrite)
    EConsciousnessStream SourceStream = EConsciousnessStream::Perceiving;

    /** Affected memory nodes */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> AffectedNodes;

    /** Cycle step when occurred */
    UPROPERTY(BlueprintReadWrite)
    int32 CycleStep = 0;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Mutual awareness level at time of event */
    UPROPERTY(BlueprintReadWrite)
    float MutualAwarenessLevel = 0.0f;
};

/**
 * Memory Consciousness Integration Configuration
 */
USTRUCT(BlueprintType)
struct FMemoryConsciousnessConfig
{
    GENERATED_BODY()

    /** Working memory capacity per stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2", ClampMax = "9"))
    int32 WorkingMemoryCapacity = 4;

    /** Memory-to-attention influence weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryAttentionWeight = 0.5f;

    /** Consciousness-to-encoding influence weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ConsciousnessEncodingWeight = 0.7f;

    /** Enable cross-stream memory association */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableCrossStreamAssociation = true;

    /** Enable goal-directed memory retrieval */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableGoalDirectedRetrieval = true;

    /** Retrieval similarity threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.3", ClampMax = "0.9"))
    float RetrievalThreshold = 0.5f;

    /** Encoding strength from attention */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float AttentionEncodingStrength = 1.2f;
};

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryConscousnessEvent, const FConsciousnessMemoryEvent&, Event, EConsciousnessStream, Stream);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkingMemoryUpdated, EConsciousnessStream, Stream);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCrossStreamAssociation, int64, NodeA, int64, NodeB);

/**
 * Memory Consciousness Integration Component
 *
 * Bridges the Hypergraph Memory System with the Recursive Mutual Awareness
 * consciousness streams, providing:
 *
 * - Memory-guided attention allocation for each stream
 * - Consciousness-driven memory encoding with attentional modulation
 * - Cross-stream memory association at triadic synchronization points
 * - Working memory management per consciousness stream
 * - Goal-directed memory retrieval from intentional memory
 * - Stream-specific memory access patterns (encode/retrieve/consolidate)
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UMemoryConsciousnessIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UMemoryConsciousnessIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Integration configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    FMemoryConsciousnessConfig Config;

    /** Enable automatic integration during tick */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    bool bEnableAutoIntegration = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnMemoryConscousnessEvent OnMemoryConsciousnessEvent;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnWorkingMemoryUpdated OnWorkingMemoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnCrossStreamAssociation OnCrossStreamAssociation;

    // ========================================
    // STREAM-MEMORY BINDING API
    // ========================================

    /** Bind consciousness stream state to memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Binding")
    void BindStreamToMemory(EConsciousnessStream Stream, const TArray<float>& StateEmbedding);

    /** Unbind stream from current memory context */
    UFUNCTION(BlueprintCallable, Category = "Memory|Binding")
    void UnbindStream(EConsciousnessStream Stream);

    /** Get current memory binding for stream */
    UFUNCTION(BlueprintPure, Category = "Memory|Binding")
    FStreamMemoryBinding GetStreamBinding(EConsciousnessStream Stream) const;

    /** Update stream retrieval cue */
    UFUNCTION(BlueprintCallable, Category = "Memory|Binding")
    void UpdateRetrievalCue(EConsciousnessStream Stream, const TArray<float>& Cue);

    // ========================================
    // WORKING MEMORY API
    // ========================================

    /** Load memory node into stream's working memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|WorkingMemory")
    bool LoadToWorkingMemory(EConsciousnessStream Stream, int64 NodeID);

    /** Remove from stream's working memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|WorkingMemory")
    void RemoveFromWorkingMemory(EConsciousnessStream Stream, int64 NodeID);

    /** Get working memory contents for stream */
    UFUNCTION(BlueprintPure, Category = "Memory|WorkingMemory")
    TArray<FConsciousnessWorkingMemorySlot> GetWorkingMemory(EConsciousnessStream Stream) const;

    /** Clear stream's working memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|WorkingMemory")
    void ClearWorkingMemory(EConsciousnessStream Stream);

    /** Update attentional weights for working memory items */
    UFUNCTION(BlueprintCallable, Category = "Memory|WorkingMemory")
    void UpdateWorkingMemoryAttention(EConsciousnessStream Stream, int64 NodeID, float Weight);

    // ========================================
    // MEMORY-GUIDED ATTENTION API
    // ========================================

    /** Compute memory-guided attention for stream */
    UFUNCTION(BlueprintCallable, Category = "Memory|Attention")
    FMemoryGuidedAttention ComputeMemoryGuidedAttention(EConsciousnessStream Stream);

    /** Get combined attention weights from all memory types */
    UFUNCTION(BlueprintCallable, Category = "Memory|Attention")
    TArray<float> GetCombinedAttentionVector(EConsciousnessStream Stream);

    /** Modulate perception with memory-based attention */
    UFUNCTION(BlueprintCallable, Category = "Memory|Attention")
    TArray<float> ModulateWithMemoryAttention(EConsciousnessStream Stream, const TArray<float>& Perception);

    // ========================================
    // CONSCIOUSNESS-DRIVEN ENCODING API
    // ========================================

    /** Encode current consciousness state to memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int64 EncodeConsciousnessState(EConsciousnessStream Stream, const FString& Label,
                                    const TArray<float>& StateEmbedding, float AttentionalWeight);

    /** Encode perceptual state to episodic memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int64 EncodePerceptualExperience(const FPerceptualState& State, float EmotionalValence = 0.0f);

    /** Encode action sequence to procedural memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int64 EncodeActionSequence(const FActionState& State, const TArray<TArray<float>>& ActionSequence);

    /** Encode reflective insight to semantic memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int64 EncodeReflectiveInsight(const FReflectiveState& State);

    // ========================================
    // CROSS-STREAM ASSOCIATION API
    // ========================================

    /** Create association between memories from different streams */
    UFUNCTION(BlueprintCallable, Category = "Memory|Association")
    int64 CreateCrossStreamAssociation(int64 NodeA, EConsciousnessStream StreamA,
                                        int64 NodeB, EConsciousnessStream StreamB,
                                        float AssociationStrength = 0.5f);

    /** Find cross-stream associations for a node */
    UFUNCTION(BlueprintPure, Category = "Memory|Association")
    TArray<int64> GetCrossStreamAssociations(int64 NodeID) const;

    /** Propagate activation across stream associations */
    UFUNCTION(BlueprintCallable, Category = "Memory|Association")
    void PropagateAcrossStreams(int64 SourceNodeID, float Activation);

    // ========================================
    // GOAL-DIRECTED RETRIEVAL API
    // ========================================

    /** Retrieve memories relevant to current intentions */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<int64> RetrieveGoalRelevantMemories(int32 MaxResults = 10);

    /** Retrieve memories for current perceptual context */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<int64> RetrieveContextualMemories(const TArray<float>& ContextEmbedding, int32 MaxResults = 10);

    /** Retrieve action patterns for current situation */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<int64> RetrieveActionPatterns(const TArray<float>& SituationEmbedding, int32 MaxResults = 5);

    // ========================================
    // COGNITIVE CYCLE INTEGRATION API
    // ========================================

    /** Process memory integration for current cycle step */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cycle")
    void ProcessCycleStep(int32 CycleStep, EConsciousnessStream ActiveStream);

    /** Handle triadic synchronization point */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cycle")
    void HandleTriadicSynchronization(int32 SyncPoint);

    /** Consolidate memories at end of cycle */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cycle")
    void ConsolidateCycleMemories();

    // ========================================
    // STATISTICS & DIAGNOSTICS
    // ========================================

    /** Get integration statistics */
    UFUNCTION(BlueprintPure, Category = "Memory|Stats")
    int32 GetTotalActiveBindings() const;

    /** Get memory access events for stream */
    UFUNCTION(BlueprintPure, Category = "Memory|Stats")
    TArray<FConsciousnessMemoryEvent> GetRecentEvents(EConsciousnessStream Stream, int32 Count = 10) const;

protected:
    // Component references
    UPROPERTY()
    UHypergraphMemorySystem* MemorySystem;

    UPROPERTY()
    URecursiveMutualAwarenessSystem* ConsciousnessSystem;

    UPROPERTY()
    UDeepTreeEchoReservoir* ReservoirComponent;

    // Stream bindings
    TMap<EConsciousnessStream, FStreamMemoryBinding> StreamBindings;

    // Working memory per stream
    TMap<EConsciousnessStream, TArray<FConsciousnessWorkingMemorySlot>> StreamWorkingMemory;

    // Cross-stream associations (maps node pairs to edge IDs)
    TMap<int64, TArray<int64>> CrossStreamEdges;

    // Event history
    TArray<FConsciousnessMemoryEvent> EventHistory;
    int32 MaxEventHistorySize = 100;

    // Current cycle step
    int32 CurrentCycleStep = 0;

    // Accumulated time
    float AccumulatedTime = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void InitializeBindings();
    void UpdateWorkingMemoryDecay(float DeltaTime);
    void ProcessPerceivingStream(float DeltaTime);
    void ProcessActingStream(float DeltaTime);
    void ProcessReflectingStream(float DeltaTime);
    void RecordEvent(const FConsciousnessMemoryEvent& Event);
    TArray<float> ComputeStreamEmbedding(EConsciousnessStream Stream) const;
    EMemoryNodeType GetNodeTypeForStream(EConsciousnessStream Stream) const;
    ESemanticRelation GetRelationForStreamPair(EConsciousnessStream StreamA, EConsciousnessStream StreamB) const;
};
