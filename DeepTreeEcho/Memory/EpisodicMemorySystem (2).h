#pragma once

/**
 * Episodic Memory System for Deep-Tree-Echo
 * 
 * Implements a multi-store memory architecture based on cognitive science:
 * - Sensory Memory: Brief iconic/echoic storage
 * - Working Memory: Active manipulation and attention
 * - Episodic Memory: Autobiographical experiences with temporal context
 * - Semantic Memory: Abstracted knowledge and concepts
 * - Procedural Memory: Skills and action sequences
 * 
 * Supports memory consolidation during simulation stream (steps 9-12)
 * and integrates with the 12-step cognitive cycle.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Cognitive/CognitiveCycleManager.h"
#include "EpisodicMemorySystem.generated.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

class USensorimotorIntegration;
class UAXIOMActiveInference;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Memory Store Types
 */
UENUM(BlueprintType)
enum class EMemoryStore : uint8
{
    /** Sensory buffer (< 1 second) */
    Sensory UMETA(DisplayName = "Sensory"),
    
    /** Working memory (active processing) */
    Working UMETA(DisplayName = "Working"),
    
    /** Episodic memory (experiences) */
    Episodic UMETA(DisplayName = "Episodic"),
    
    /** Semantic memory (knowledge) */
    Semantic UMETA(DisplayName = "Semantic"),
    
    /** Procedural memory (skills) */
    Procedural UMETA(DisplayName = "Procedural")
};

/**
 * Memory Encoding Strength
 */
UENUM(BlueprintType)
enum class EEncodingStrength : uint8
{
    /** Weak encoding (may decay quickly) */
    Weak UMETA(DisplayName = "Weak"),
    
    /** Moderate encoding */
    Moderate UMETA(DisplayName = "Moderate"),
    
    /** Strong encoding (emotionally significant) */
    Strong UMETA(DisplayName = "Strong"),
    
    /** Flashbulb (highly significant event) */
    Flashbulb UMETA(DisplayName = "Flashbulb")
};

/**
 * Consolidation Phase
 */
UENUM(BlueprintType)
enum class EConsolidationPhase : uint8
{
    /** Not consolidating */
    Idle UMETA(DisplayName = "Idle"),
    
    /** Encoding new memories */
    Encoding UMETA(DisplayName = "Encoding"),
    
    /** Replaying and strengthening */
    Replay UMETA(DisplayName = "Replay"),
    
    /** Integrating with semantic memory */
    Integration UMETA(DisplayName = "Integration"),
    
    /** Pruning weak memories */
    Pruning UMETA(DisplayName = "Pruning")
};

/**
 * Retrieval Cue Type
 */
UENUM(BlueprintType)
enum class ERetrievalCueType : uint8
{
    /** Content-based similarity */
    Content UMETA(DisplayName = "Content"),
    
    /** Temporal proximity */
    Temporal UMETA(DisplayName = "Temporal"),
    
    /** Spatial context */
    Spatial UMETA(DisplayName = "Spatial"),
    
    /** Emotional valence */
    Emotional UMETA(DisplayName = "Emotional"),
    
    /** Goal relevance */
    GoalRelevant UMETA(DisplayName = "Goal Relevant")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Memory Trace - Basic unit of memory
 */
USTRUCT(BlueprintType)
struct FMemoryTrace
{
    GENERATED_BODY()

    /** Unique trace identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 TraceID = 0;

    /** Memory store location */
    UPROPERTY(BlueprintReadWrite)
    EMemoryStore Store = EMemoryStore::Sensory;

    /** Content vector (encoded representation) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Content;

    /** Encoding strength */
    UPROPERTY(BlueprintReadWrite)
    EEncodingStrength EncodingStrength = EEncodingStrength::Weak;

    /** Activation level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Strength/consolidation level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    float CreationTime = 0.0f;

    /** Last access timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastAccessTime = 0.0f;

    /** Access count */
    UPROPERTY(BlueprintReadWrite)
    int32 AccessCount = 0;

    /** Emotional valence (-1 to 1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Emotional arousal (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalArousal = 0.0f;

    /** Associated goal IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> AssociatedGoals;

    /** Linked trace IDs (associations) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> LinkedTraces;
};

/**
 * Episode - Coherent sequence of experiences
 */
USTRUCT(BlueprintType)
struct FEpisode
{
    GENERATED_BODY()

    /** Episode identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 EpisodeID = 0;

    /** Episode label/description */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Start timestamp */
    UPROPERTY(BlueprintReadWrite)
    float StartTime = 0.0f;

    /** End timestamp */
    UPROPERTY(BlueprintReadWrite)
    float EndTime = 0.0f;

    /** Constituent memory traces */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> TraceIDs;

    /** Spatial context (location) */
    UPROPERTY(BlueprintReadWrite)
    FVector SpatialContext = FVector::ZeroVector;

    /** Overall emotional valence */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Overall significance (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Significance = 0.5f;

    /** Has been consolidated */
    UPROPERTY(BlueprintReadWrite)
    bool bConsolidated = false;

    /** Replay count during consolidation */
    UPROPERTY(BlueprintReadWrite)
    int32 ReplayCount = 0;
};

/**
 * Semantic Concept - Abstracted knowledge
 */
USTRUCT(BlueprintType)
struct FSemanticConcept
{
    GENERATED_BODY()

    /** Concept identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 ConceptID = 0;

    /** Concept name */
    UPROPERTY(BlueprintReadWrite)
    FName ConceptName;

    /** Prototype vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Prototype;

    /** Variance around prototype */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Variance;

    /** Number of instances abstracted */
    UPROPERTY(BlueprintReadWrite)
    int32 InstanceCount = 0;

    /** Confidence in concept (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 0.5f;

    /** Related concept IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> RelatedConcepts;

    /** Source episode IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> SourceEpisodes;
};

/**
 * Procedural Skill - Action sequence
 */
USTRUCT(BlueprintType)
struct FProceduralSkill
{
    GENERATED_BODY()

    /** Skill identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 SkillID = 0;

    /** Skill name */
    UPROPERTY(BlueprintReadWrite)
    FName SkillName;

    /** Action sequence (motor patterns) */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> ActionSequence;

    /** Expected sensory consequences */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> ExpectedConsequences;

    /** Proficiency level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Proficiency = 0.0f;

    /** Practice count */
    UPROPERTY(BlueprintReadWrite)
    int32 PracticeCount = 0;

    /** Success rate */
    UPROPERTY(BlueprintReadWrite)
    float SuccessRate = 0.5f;

    /** Automaticity (0-1, higher = less attention needed) */
    UPROPERTY(BlueprintReadWrite)
    float Automaticity = 0.0f;
};

/**
 * Working Memory Buffer
 */
USTRUCT(BlueprintType)
struct FWorkingMemoryBuffer
{
    GENERATED_BODY()

    /** Buffer capacity (typically 4-7 items) */
    UPROPERTY(BlueprintReadWrite)
    int32 Capacity = 4;

    /** Current items (trace IDs) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> Items;

    /** Item activation levels */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Activations;

    /** Focus of attention (index in Items) */
    UPROPERTY(BlueprintReadWrite)
    int32 FocusIndex = -1;

    /** Cognitive load (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveLoad = 0.0f;
};

/**
 * Retrieval Request
 */
USTRUCT(BlueprintType)
struct FRetrievalRequest
{
    GENERATED_BODY()

    /** Cue type */
    UPROPERTY(BlueprintReadWrite)
    ERetrievalCueType CueType = ERetrievalCueType::Content;

    /** Cue content (for content-based retrieval) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CueContent;

    /** Temporal range (for temporal retrieval) */
    UPROPERTY(BlueprintReadWrite)
    FVector2D TemporalRange = FVector2D(0.0f, FLT_MAX);

    /** Spatial location (for spatial retrieval) */
    UPROPERTY(BlueprintReadWrite)
    FVector SpatialLocation = FVector::ZeroVector;

    /** Spatial radius */
    UPROPERTY(BlueprintReadWrite)
    float SpatialRadius = 100.0f;

    /** Emotional valence target */
    UPROPERTY(BlueprintReadWrite)
    float TargetValence = 0.0f;

    /** Goal ID (for goal-relevant retrieval) */
    UPROPERTY(BlueprintReadWrite)
    int32 GoalID = -1;

    /** Maximum results */
    UPROPERTY(BlueprintReadWrite)
    int32 MaxResults = 10;

    /** Minimum activation threshold */
    UPROPERTY(BlueprintReadWrite)
    float MinActivation = 0.1f;
};

/**
 * Consolidation State
 */
USTRUCT(BlueprintType)
struct FConsolidationState
{
    GENERATED_BODY()

    /** Current phase */
    UPROPERTY(BlueprintReadWrite)
    EConsolidationPhase Phase = EConsolidationPhase::Idle;

    /** Episodes pending consolidation */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> PendingEpisodes;

    /** Current episode being processed */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentEpisodeID = -1;

    /** Replay iteration */
    UPROPERTY(BlueprintReadWrite)
    int32 ReplayIteration = 0;

    /** Consolidation progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;
};

/**
 * Memory System Configuration
 */
USTRUCT(BlueprintType)
struct FMemorySystemConfig
{
    GENERATED_BODY()

    /** Sensory memory decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float SensoryDecayRate = 2.0f;

    /** Working memory capacity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "3", ClampMax = "9"))
    int32 WorkingMemoryCapacity = 4;

    /** Episodic memory consolidation threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ConsolidationThreshold = 0.5f;

    /** Memory strength decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001", ClampMax = "0.1"))
    float StrengthDecayRate = 0.01f;

    /** Replay strengthening factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "2.0"))
    float ReplayStrengthFactor = 1.2f;

    /** Maximum episodes to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100", ClampMax = "10000"))
    int32 MaxEpisodes = 1000;

    /** Maximum semantic concepts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100", ClampMax = "10000"))
    int32 MaxConcepts = 500;

    /** Enable automatic consolidation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAutoConsolidation = true;

    /** Consolidation interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float ConsolidationInterval = 10.0f;
};

/**
 * Episodic Memory System Component
 * 
 * Manages memory encoding, storage, consolidation, and retrieval
 * for the Deep-Tree-Echo cognitive architecture.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEpisodicMemorySystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UEpisodicMemorySystem();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Memory system configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    FMemorySystemConfig Config;

    /** Reference to CognitiveCycleManager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Reference to Sensorimotor Integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    USensorimotorIntegration* SensorimotorComponent;

    // ========================================
    // STATE
    // ========================================

    /** Working memory buffer */
    UPROPERTY(BlueprintReadOnly, Category = "Memory|State")
    FWorkingMemoryBuffer WorkingMemory;

    /** Consolidation state */
    UPROPERTY(BlueprintReadOnly, Category = "Memory|State")
    FConsolidationState ConsolidationState;

    /** Current episode being recorded */
    UPROPERTY(BlueprintReadOnly, Category = "Memory|State")
    int32 CurrentRecordingEpisode = -1;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize memory system */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void Initialize();

    /** Reset memory system */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void Reset();

    // ========================================
    // PUBLIC API - ENCODING
    // ========================================

    /** Encode new memory trace */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int64 EncodeTrace(const TArray<float>& Content, EEncodingStrength Strength, float EmotionalValence = 0.0f);

    /** Start recording new episode */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int32 StartEpisode(const FString& Label, FVector SpatialContext);

    /** Add trace to current episode */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    void AddTraceToEpisode(int64 TraceID);

    /** End current episode */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    void EndEpisode(float Significance = 0.5f);

    /** Encode procedural skill */
    UFUNCTION(BlueprintCallable, Category = "Memory|Encoding")
    int32 EncodeSkill(FName SkillName, const TArray<TArray<float>>& ActionSequence);

    // ========================================
    // PUBLIC API - RETRIEVAL
    // ========================================

    /** Retrieve memories by content similarity */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> RetrieveByContent(const TArray<float>& Cue, int32 MaxResults = 10);

    /** Retrieve memories by temporal range */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> RetrieveByTime(float StartTime, float EndTime, int32 MaxResults = 10);

    /** Retrieve memories by spatial context */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> RetrieveBySpatial(FVector Location, float Radius, int32 MaxResults = 10);

    /** Retrieve memories by emotional valence */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> RetrieveByEmotion(float TargetValence, float Tolerance = 0.3f, int32 MaxResults = 10);

    /** Retrieve goal-relevant memories */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> RetrieveForGoal(int32 GoalID, int32 MaxResults = 10);

    /** General retrieval with request structure */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    TArray<FMemoryTrace> Retrieve(const FRetrievalRequest& Request);

    /** Retrieve episode by ID */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    FEpisode GetEpisode(int32 EpisodeID) const;

    /** Retrieve semantic concept by name */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    FSemanticConcept GetConcept(FName ConceptName) const;

    /** Retrieve procedural skill by name */
    UFUNCTION(BlueprintCallable, Category = "Memory|Retrieval")
    FProceduralSkill GetSkill(FName SkillName) const;

    // ========================================
    // PUBLIC API - WORKING MEMORY
    // ========================================

    /** Load trace into working memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    bool LoadToWorkingMemory(int64 TraceID);

    /** Remove trace from working memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void RemoveFromWorkingMemory(int64 TraceID);

    /** Set focus of attention */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    void SetAttentionFocus(int32 ItemIndex);

    /** Get current working memory contents */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    TArray<FMemoryTrace> GetWorkingMemoryContents() const;

    /** Get cognitive load */
    UFUNCTION(BlueprintCallable, Category = "Memory|Working")
    float GetCognitiveLoad() const;

    // ========================================
    // PUBLIC API - CONSOLIDATION
    // ========================================

    /** Start consolidation process */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void StartConsolidation();

    /** Process one consolidation step */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void ProcessConsolidationStep();

    /** Stop consolidation process */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void StopConsolidation();

    /** Get consolidation progress */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    float GetConsolidationProgress() const;

    /** Abstract episode to semantic concept */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    int32 AbstractToConcept(int32 EpisodeID, FName ConceptName);

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Memory|CognitiveCycle")
    void ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream);

    /** Get memory-guided attention weights */
    UFUNCTION(BlueprintCallable, Category = "Memory|CognitiveCycle")
    TArray<float> GetMemoryGuidedAttention(const TArray<float>& CurrentPerception) const;

    // ========================================
    // PUBLIC API - ASSOCIATIONS
    // ========================================

    /** Create association between traces */
    UFUNCTION(BlueprintCallable, Category = "Memory|Association")
    void CreateAssociation(int64 TraceA, int64 TraceB, float Strength = 0.5f);

    /** Get associated traces */
    UFUNCTION(BlueprintCallable, Category = "Memory|Association")
    TArray<int64> GetAssociatedTraces(int64 TraceID) const;

    /** Spread activation through associations */
    UFUNCTION(BlueprintCallable, Category = "Memory|Association")
    void SpreadActivation(int64 SourceTrace, float ActivationAmount, int32 MaxDepth = 3);

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when new episode is encoded */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEpisodeEncoded, int32, EpisodeID);
    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnEpisodeEncoded OnEpisodeEncoded;

    /** Called when memory is retrieved */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryRetrieved, int64, TraceID, float, Activation);
    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnMemoryRetrieved OnMemoryRetrieved;

    /** Called when consolidation completes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConsolidationComplete, int32, EpisodeID);
    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnConsolidationComplete OnConsolidationComplete;

    /** Called when semantic concept is formed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConceptFormed, int32, ConceptID, FName, ConceptName);
    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnConceptFormed OnConceptFormed;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL DATA
    // ========================================

    /** All memory traces */
    TMap<int64, FMemoryTrace> AllTraces;

    /** Sensory buffer (recent traces) */
    TArray<int64> SensoryBuffer;

    /** All episodes */
    TMap<int32, FEpisode> Episodes;

    /** All semantic concepts */
    TMap<int32, FSemanticConcept> Concepts;

    /** Concept name to ID mapping */
    TMap<FName, int32> ConceptNameToID;

    /** All procedural skills */
    TMap<int32, FProceduralSkill> Skills;

    /** Skill name to ID mapping */
    TMap<FName, int32> SkillNameToID;

    /** Next trace ID */
    int64 NextTraceID = 1;

    /** Next episode ID */
    int32 NextEpisodeID = 1;

    /** Next concept ID */
    int32 NextConceptID = 1;

    /** Next skill ID */
    int32 NextSkillID = 1;

    /** Accumulated time */
    float AccumulatedTime = 0.0f;

    /** Last consolidation time */
    float LastConsolidationTime = 0.0f;

    /** Last processed cognitive step */
    int32 LastProcessedStep = -1;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Decay sensory memory */
    void DecaySensoryMemory(float DeltaTime);

    /** Decay memory strengths */
    void DecayMemoryStrengths(float DeltaTime);

    /** Update working memory activations */
    void UpdateWorkingMemoryActivations(float DeltaTime);

    /** Replay episode for consolidation */
    void ReplayEpisode(int32 EpisodeID);

    /** Integrate episode with semantic memory */
    void IntegrateWithSemanticMemory(int32 EpisodeID);

    /** Prune weak memories */
    void PruneWeakMemories();

    /** Compute content similarity */
    float ComputeSimilarity(const TArray<float>& A, const TArray<float>& B) const;

    /** Get trace by ID */
    FMemoryTrace* GetTrace(int64 TraceID);
    const FMemoryTrace* GetTrace(int64 TraceID) const;

    /** Move trace between stores */
    void MoveToStore(int64 TraceID, EMemoryStore NewStore);
};
