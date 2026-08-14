// ReservoirMemoryIntegration.h
// Deep Tree Echo - Reservoir Computing Memory Integration
// Bridges hypergraph memory with echo state network dynamics
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HypergraphMemorySystem.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "ReservoirMemoryIntegration.generated.h"

// ========================================
// WORKING MEMORY STRUCTURES
// ========================================

/**
 * Working memory slot - Active thought representation
 */
USTRUCT(BlueprintType)
struct FWorkingMemorySlot
{
    GENERATED_BODY()

    /** Slot index (0-6, based on cognitive research) */
    UPROPERTY(BlueprintReadWrite)
    int32 SlotIndex = 0;

    /** Associated node ID from hypergraph */
    UPROPERTY(BlueprintReadWrite)
    int64 MemoryNodeID = 0;

    /** Reservoir activation pattern */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ActivationPattern;

    /** Attention weight (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float AttentionWeight = 0.0f;

    /** Decay timer */
    UPROPERTY(BlueprintReadWrite)
    float DecayTimer = 0.0f;

    /** Is slot active */
    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = false;

    /** Refresh count (rehearsal) */
    UPROPERTY(BlueprintReadWrite)
    int32 RefreshCount = 0;

    /** Cognitive stream association (1-3) */
    UPROPERTY(BlueprintReadWrite)
    int32 StreamAssociation = 0;
};

/**
 * Procedural memory trace - Learned sequence pattern
 */
USTRUCT(BlueprintType)
struct FProceduralTrace
{
    GENERATED_BODY()

    /** Trace ID */
    UPROPERTY(BlueprintReadWrite)
    FString TraceID;

    /** Skill node ID in hypergraph */
    UPROPERTY(BlueprintReadWrite)
    int64 SkillNodeID = 0;

    /** Sequence of activation states */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> SequenceStates;

    /** Timing between states (seconds) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> StateTiming;

    /** Proficiency level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Proficiency = 0.0f;

    /** Practice count */
    UPROPERTY(BlueprintReadWrite)
    int32 PracticeCount = 0;

    /** Last practice timestamp */
    UPROPERTY(BlueprintReadWrite)
    double LastPracticeTime = 0.0;

    /** Is sequence currently executing */
    UPROPERTY(BlueprintReadWrite)
    bool bIsExecuting = false;

    /** Current execution step */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentStep = 0;
};

/**
 * Episodic context - Temporal binding from reservoir
 */
USTRUCT(BlueprintType)
struct FEpisodicContext
{
    GENERATED_BODY()

    /** Episode node ID */
    UPROPERTY(BlueprintReadWrite)
    int64 EpisodeNodeID = 0;

    /** Reservoir state at encoding */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EncodingState;

    /** Stream states at encoding (all 3 streams) */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> StreamStates;

    /** Cycle step at encoding (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 CycleStep = 1;

    /** Temporal markers (before, during, after relationships) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> TemporalNeighbors;

    /** Contextual cue embedding */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ContextCue;

    /** Retrieval strength */
    UPROPERTY(BlueprintReadWrite)
    float RetrievalStrength = 0.5f;
};

/**
 * Semantic attractor - Stable concept in reservoir dynamics
 */
USTRUCT(BlueprintType)
struct FSemanticAttractor
{
    GENERATED_BODY()

    /** Concept node ID */
    UPROPERTY(BlueprintReadWrite)
    int64 ConceptNodeID = 0;

    /** Attractor basin center (stable state) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> AttractorCenter;

    /** Basin radius (distance to boundary) */
    UPROPERTY(BlueprintReadWrite)
    float BasinRadius = 0.5f;

    /** Stability score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Stability = 0.5f;

    /** Connected attractors (related concepts) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> ConnectedAttractors;

    /** Activation threshold to enter basin */
    UPROPERTY(BlueprintReadWrite)
    float ActivationThreshold = 0.3f;

    /** Access frequency */
    UPROPERTY(BlueprintReadWrite)
    int32 AccessCount = 0;
};

/**
 * Memory consolidation event
 */
USTRUCT(BlueprintType)
struct FConsolidationEvent
{
    GENERATED_BODY()

    /** Source nodes (working memory) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> SourceNodes;

    /** Target node (consolidated) */
    UPROPERTY(BlueprintReadWrite)
    int64 TargetNodeID = 0;

    /** Consolidation type */
    UPROPERTY(BlueprintReadWrite)
    FString ConsolidationType;

    /** Pattern strength at consolidation */
    UPROPERTY(BlueprintReadWrite)
    float PatternStrength = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    double Timestamp = 0.0;
};

/**
 * Memory replay configuration
 */
USTRUCT(BlueprintType)
struct FMemoryReplayConfig
{
    GENERATED_BODY()

    /** Replay speed multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpeedMultiplier = 1.0f;

    /** Intensity of replay (affects reservoir activation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReplayIntensity = 0.5f;

    /** Allow modification during replay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowModification = false;

    /** Replay in which stream (0 = all) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetStream = 0;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkingMemoryUpdated, int32, SlotIndex, int64, NodeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProceduralLearned, FString, TraceID, float, Proficiency);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEpisodicEncoded, int64, EpisodeNodeID, int32, CycleStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSemanticAttractorFormed, int64, ConceptNodeID, float, Stability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConsolidationComplete, FConsolidationEvent, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryReplayStarted, int64, NodeID, FString, MemoryType);

/**
 * ReservoirMemoryIntegration
 *
 * Bridges hypergraph memory with reservoir computing for:
 * - Working Memory: Active thought representations in reservoir dynamics
 * - Procedural Memory: Learned sequences as temporal patterns
 * - Episodic Memory: Temporal context binding through reservoir states
 * - Semantic Memory: Stable attractors representing concepts
 * - Memory Consolidation: Pattern-based transfer from working to long-term
 * - Memory Replay: Reactivating memories through reservoir dynamics
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UReservoirMemoryIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UReservoirMemoryIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Working memory capacity (default: 7 +/- 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    int32 WorkingMemoryCapacity = 7;

    /** Working memory decay rate (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float WorkingMemoryDecayRate = 18.0f;

    /** Enable automatic consolidation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    bool bAutoConsolidation = true;

    /** Consolidation threshold (pattern strength) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float ConsolidationThreshold = 0.7f;

    /** Enable procedural learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    bool bEnableProceduralLearning = true;

    /** Procedural learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float ProceduralLearningRate = 0.1f;

    /** Memory replay config */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    FMemoryReplayConfig ReplayConfig;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnWorkingMemoryUpdated OnWorkingMemoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnProceduralLearned OnProceduralLearned;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnEpisodicEncoded OnEpisodicEncoded;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnSemanticAttractorFormed OnSemanticAttractorFormed;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnConsolidationComplete OnConsolidationComplete;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnMemoryReplayStarted OnMemoryReplayStarted;

    // ========================================
    // WORKING MEMORY API
    // ========================================

    /** Attend to memory node (load into working memory) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    int32 AttendToMemory(int64 NodeID);

    /** Release working memory slot */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void ReleaseWorkingMemorySlot(int32 SlotIndex);

    /** Refresh working memory slot (rehearsal) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void RefreshWorkingMemory(int32 SlotIndex);

    /** Get working memory slot */
    UFUNCTION(BlueprintPure, Category = "Memory|Working")
    FWorkingMemorySlot GetWorkingMemorySlot(int32 SlotIndex) const;

    /** Get all active working memory slots */
    UFUNCTION(BlueprintPure, Category = "Memory|Working")
    TArray<FWorkingMemorySlot> GetActiveWorkingMemory() const;

    /** Get current working memory load (0-1) */
    UFUNCTION(BlueprintPure, Category = "Memory|Working")
    float GetWorkingMemoryLoad() const;

    /** Focus attention on slot */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void FocusAttention(int32 SlotIndex, float Intensity = 1.0f);

    /** Bind working memory items (create association) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void BindWorkingMemoryItems(int32 SlotA, int32 SlotB);

    // ========================================
    // PROCEDURAL MEMORY API
    // ========================================

    /** Begin learning procedural sequence */
    UFUNCTION(BlueprintCallable, Category = "Memory|Procedural")
    FString BeginProceduralLearning(const FString& SkillName);

    /** Record step in procedural sequence */
    UFUNCTION(BlueprintCallable, Category = "Memory|Procedural")
    void RecordProceduralStep(const FString& TraceID, const TArray<float>& MotorCommand);

    /** End procedural learning and consolidate */
    UFUNCTION(BlueprintCallable, Category = "Memory|Procedural")
    int64 EndProceduralLearning(const FString& TraceID);

    /** Execute procedural sequence */
    UFUNCTION(BlueprintCallable, Category = "Memory|Procedural")
    bool ExecuteProceduralSequence(const FString& TraceID);

    /** Get procedural trace */
    UFUNCTION(BlueprintPure, Category = "Memory|Procedural")
    FProceduralTrace GetProceduralTrace(const FString& TraceID) const;

    /** Practice procedural sequence (improve proficiency) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Procedural")
    void PracticeProceduralSequence(const FString& TraceID);

    /** Get all procedural traces for skill */
    UFUNCTION(BlueprintPure, Category = "Memory|Procedural")
    TArray<FProceduralTrace> GetTracesForSkill(int64 SkillNodeID) const;

    // ========================================
    // EPISODIC MEMORY API
    // ========================================

    /** Encode current experience as episode */
    UFUNCTION(BlueprintCallable, Category = "Memory|Episodic")
    int64 EncodeEpisode(const FString& Label, const TArray<float>& PerceptualContent);

    /** Encode with explicit temporal relations */
    UFUNCTION(BlueprintCallable, Category = "Memory|Episodic")
    int64 EncodeEpisodeWithContext(const FString& Label, const TArray<float>& Content,
                                    const TArray<int64>& TemporalNeighbors);

    /** Retrieve episode by cue */
    UFUNCTION(BlueprintCallable, Category = "Memory|Episodic")
    TArray<int64> RetrieveEpisodesByCue(const TArray<float>& Cue, float Threshold = 0.5f);

    /** Retrieve episodes by temporal context */
    UFUNCTION(BlueprintCallable, Category = "Memory|Episodic")
    TArray<int64> RetrieveEpisodesByTime(double StartTime, double EndTime);

    /** Get episodic context for node */
    UFUNCTION(BlueprintPure, Category = "Memory|Episodic")
    FEpisodicContext GetEpisodicContext(int64 EpisodeNodeID) const;

    /** Reconstruct episode (replay through reservoir) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Episodic")
    bool ReconstructEpisode(int64 EpisodeNodeID);

    // ========================================
    // SEMANTIC MEMORY API
    // ========================================

    /** Form semantic attractor from repeated patterns */
    UFUNCTION(BlueprintCallable, Category = "Memory|Semantic")
    int64 FormSemanticAttractor(const TArray<float>& PatternCenter, const FString& ConceptLabel);

    /** Check if state is in attractor basin */
    UFUNCTION(BlueprintPure, Category = "Memory|Semantic")
    bool IsInAttractorBasin(const TArray<float>& State, int64 ConceptNodeID) const;

    /** Find nearest attractor to state */
    UFUNCTION(BlueprintCallable, Category = "Memory|Semantic")
    int64 FindNearestAttractor(const TArray<float>& State);

    /** Get semantic attractor */
    UFUNCTION(BlueprintPure, Category = "Memory|Semantic")
    FSemanticAttractor GetSemanticAttractor(int64 ConceptNodeID) const;

    /** Connect semantic attractors (create conceptual link) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Semantic")
    void ConnectAttractors(int64 ConceptA, int64 ConceptB, float Strength = 1.0f);

    /** Traverse semantic space (move between attractors) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Semantic")
    TArray<int64> TraverseSemanticSpace(int64 StartConcept, int64 EndConcept);

    // ========================================
    // CONSOLIDATION API
    // ========================================

    /** Run consolidation cycle */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void RunConsolidationCycle();

    /** Consolidate specific working memory pattern */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    int64 ConsolidateWorkingMemoryPattern(const TArray<int32>& SlotIndices);

    /** Get recent consolidation events */
    UFUNCTION(BlueprintPure, Category = "Memory|Consolidation")
    TArray<FConsolidationEvent> GetRecentConsolidations(int32 Count = 10) const;

    /** Trigger sleep-like consolidation (intensive replay) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void TriggerOfflineConsolidation(float Duration);

    // ========================================
    // MEMORY REPLAY API
    // ========================================

    /** Start memory replay */
    UFUNCTION(BlueprintCallable, Category = "Memory|Replay")
    bool StartMemoryReplay(int64 NodeID, EMemoryNodeType MemoryType);

    /** Stop memory replay */
    UFUNCTION(BlueprintCallable, Category = "Memory|Replay")
    void StopMemoryReplay();

    /** Is replay active */
    UFUNCTION(BlueprintPure, Category = "Memory|Replay")
    bool IsReplayActive() const;

    /** Get replay progress (0-1) */
    UFUNCTION(BlueprintPure, Category = "Memory|Replay")
    float GetReplayProgress() const;

    // ========================================
    // RESERVOIR INTEGRATION API
    // ========================================

    /** Get current reservoir state for memory operations */
    UFUNCTION(BlueprintPure, Category = "Memory|Reservoir")
    TArray<float> GetCurrentReservoirState(int32 StreamID = 0) const;

    /** Inject memory pattern into reservoir */
    UFUNCTION(BlueprintCallable, Category = "Memory|Reservoir")
    void InjectMemoryPattern(const TArray<float>& Pattern, int32 StreamID = 0);

    /** Get pattern similarity to current reservoir state */
    UFUNCTION(BlueprintPure, Category = "Memory|Reservoir")
    float GetPatternSimilarity(const TArray<float>& Pattern) const;

    /** Detect if reservoir entered attractor basin */
    UFUNCTION(BlueprintPure, Category = "Memory|Reservoir")
    int64 DetectAttractorBasin() const;

protected:
    // Component references
    UPROPERTY()
    UHypergraphMemorySystem* HypergraphMemory;

    UPROPERTY()
    UDeepTreeEchoReservoir* Reservoir;

    // Working memory storage
    TArray<FWorkingMemorySlot> WorkingMemorySlots;

    // Procedural memory storage
    TMap<FString, FProceduralTrace> ProceduralTraces;

    // Episodic context storage
    TMap<int64, FEpisodicContext> EpisodicContexts;

    // Semantic attractor storage
    TMap<int64, FSemanticAttractor> SemanticAttractors;

    // Consolidation history
    TArray<FConsolidationEvent> ConsolidationHistory;

    // Replay state
    bool bReplayActive = false;
    int64 ReplayNodeID = 0;
    EMemoryNodeType ReplayType = EMemoryNodeType::Episode;
    float ReplayProgress = 0.0f;
    int32 ReplayCurrentStep = 0;
    TArray<TArray<float>> ReplaySequence;

    // Procedural execution state
    bool bProceduralExecuting = false;
    FString ExecutingTraceID;
    float ProceduralTimer = 0.0f;

    // Trace ID generator
    int32 NextTraceID = 0;

    // Consolidation timer
    float ConsolidationTimer = 0.0f;
    float ConsolidationInterval = 30.0f;

    // Internal methods
    void FindComponentReferences();
    void UpdateWorkingMemory(float DeltaTime);
    void UpdateProceduralExecution(float DeltaTime);
    void UpdateReplay(float DeltaTime);
    void DecayWorkingMemorySlots(float DeltaTime);
    int32 FindFreeWorkingMemorySlot();
    int32 FindLeastImportantSlot();
    float ComputePatternSimilarity(const TArray<float>& A, const TArray<float>& B) const;
    TArray<float> ComputePatternAverage(const TArray<TArray<float>>& Patterns) const;
    FString GenerateTraceID();
};
