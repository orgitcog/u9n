// HypergraphMemorySystem.h
// Hypergraph-based unified memory architecture for Deep Tree Echo
// Implements declarative, procedural, episodic, and intentional memory as a connected graph

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EpisodicMemorySystem.h"
#include "HypergraphMemorySystem.generated.h"

// Forward declarations
class UDeepTreeEchoReservoir;
class UTaskflowCognitiveScheduler;

// ========================================
// HYPERGRAPH ENUMERATIONS
// ========================================

/**
 * Memory node type classification
 */
UENUM(BlueprintType)
enum class EMemoryNodeType : uint8
{
    /** Raw perceptual trace */
    Percept         UMETA(DisplayName = "Percept"),
    /** Episodic event memory */
    Episode         UMETA(DisplayName = "Episode"),
    /** Semantic concept/category */
    Concept         UMETA(DisplayName = "Concept"),
    /** Procedural skill/action schema */
    Skill           UMETA(DisplayName = "Skill"),
    /** Belief (propositional attitude) */
    Belief          UMETA(DisplayName = "Belief"),
    /** Desire/goal state */
    Desire          UMETA(DisplayName = "Desire"),
    /** Intention/committed plan */
    Intention       UMETA(DisplayName = "Intention"),
    /** Emotional state marker */
    Emotion         UMETA(DisplayName = "Emotion"),
    /** Spatial location/place cell */
    Place           UMETA(DisplayName = "Place"),
    /** Temporal marker/time cell */
    Time            UMETA(DisplayName = "Time"),
    /** Agent/entity representation */
    Agent           UMETA(DisplayName = "Agent"),
    /** Object representation */
    Object          UMETA(DisplayName = "Object"),
    /** Abstract relation node */
    Relation        UMETA(DisplayName = "Relation"),
    /** Schema/frame structure */
    Schema          UMETA(DisplayName = "Schema"),
    /** Meta-cognitive reflection */
    MetaCognitive   UMETA(DisplayName = "Meta-Cognitive")
};

/**
 * Semantic relation types for edges
 */
UENUM(BlueprintType)
enum class ESemanticRelation : uint8
{
    // Taxonomic relations
    IS_A            UMETA(DisplayName = "Is-A (Inheritance)"),
    INSTANCE_OF     UMETA(DisplayName = "Instance-Of"),
    SUBTYPE_OF      UMETA(DisplayName = "Subtype-Of"),

    // Mereological relations
    PART_OF         UMETA(DisplayName = "Part-Of"),
    HAS_PART        UMETA(DisplayName = "Has-Part"),
    MEMBER_OF       UMETA(DisplayName = "Member-Of"),

    // Causal relations
    CAUSES          UMETA(DisplayName = "Causes"),
    CAUSED_BY       UMETA(DisplayName = "Caused-By"),
    ENABLES         UMETA(DisplayName = "Enables"),
    PREVENTS        UMETA(DisplayName = "Prevents"),

    // Temporal relations
    BEFORE          UMETA(DisplayName = "Before"),
    AFTER           UMETA(DisplayName = "After"),
    DURING          UMETA(DisplayName = "During"),
    OVERLAPS        UMETA(DisplayName = "Overlaps"),

    // Spatial relations
    LOCATED_AT      UMETA(DisplayName = "Located-At"),
    NEAR            UMETA(DisplayName = "Near"),
    CONTAINS        UMETA(DisplayName = "Contains"),
    ADJACENT_TO     UMETA(DisplayName = "Adjacent-To"),

    // Intentional relations
    BELIEVES        UMETA(DisplayName = "Believes"),
    DESIRES         UMETA(DisplayName = "Desires"),
    INTENDS         UMETA(DisplayName = "Intends"),
    EXPECTS         UMETA(DisplayName = "Expects"),
    PERCEIVES       UMETA(DisplayName = "Perceives"),

    // Associative relations
    SIMILAR_TO      UMETA(DisplayName = "Similar-To"),
    CONTRASTS_WITH  UMETA(DisplayName = "Contrasts-With"),
    ASSOCIATED_WITH UMETA(DisplayName = "Associated-With"),
    REMINDS_OF      UMETA(DisplayName = "Reminds-Of"),

    // Functional relations
    USED_FOR        UMETA(DisplayName = "Used-For"),
    CAPABLE_OF      UMETA(DisplayName = "Capable-Of"),
    REQUIRES        UMETA(DisplayName = "Requires"),
    PRODUCES        UMETA(DisplayName = "Produces"),

    // Evaluative relations
    POSITIVE_TOWARD UMETA(DisplayName = "Positive-Toward"),
    NEGATIVE_TOWARD UMETA(DisplayName = "Negative-Toward"),
    IMPORTANT_FOR   UMETA(DisplayName = "Important-For"),

    // Meta-relations
    CONTRADICTS     UMETA(DisplayName = "Contradicts"),
    SUPPORTS        UMETA(DisplayName = "Supports"),
    DERIVED_FROM    UMETA(DisplayName = "Derived-From"),
    GENERALIZES     UMETA(DisplayName = "Generalizes"),

    // Custom/unspecified
    CUSTOM          UMETA(DisplayName = "Custom Relation")
};

/**
 * Belief modality for intentional states
 */
UENUM(BlueprintType)
enum class EBeliefModality : uint8
{
    /** Known with certainty */
    Certain         UMETA(DisplayName = "Certain"),
    /** Probable but uncertain */
    Probable        UMETA(DisplayName = "Probable"),
    /** Possible but not probable */
    Possible        UMETA(DisplayName = "Possible"),
    /** Counterfactual/hypothetical */
    Hypothetical    UMETA(DisplayName = "Hypothetical"),
    /** Previously believed, now rejected */
    Rejected        UMETA(DisplayName = "Rejected")
};

/**
 * Intention status for BDI model
 */
UENUM(BlueprintType)
enum class EIntentionStatus : uint8
{
    /** Under consideration */
    Deliberating    UMETA(DisplayName = "Deliberating"),
    /** Committed to pursue */
    Committed       UMETA(DisplayName = "Committed"),
    /** Actively executing */
    Executing       UMETA(DisplayName = "Executing"),
    /** Temporarily suspended */
    Suspended       UMETA(DisplayName = "Suspended"),
    /** Successfully achieved */
    Achieved        UMETA(DisplayName = "Achieved"),
    /** Failed or abandoned */
    Failed          UMETA(DisplayName = "Failed")
};

/**
 * Query operator for graph queries
 */
UENUM(BlueprintType)
enum class EQueryOperator : uint8
{
    AND             UMETA(DisplayName = "AND"),
    OR              UMETA(DisplayName = "OR"),
    NOT             UMETA(DisplayName = "NOT"),
    EXISTS          UMETA(DisplayName = "EXISTS"),
    FORALL          UMETA(DisplayName = "FORALL"),
    PATH            UMETA(DisplayName = "PATH (Transitive)")
};

// ========================================
// HYPERGRAPH STRUCTURES
// ========================================

/**
 * Memory node in the hypergraph
 */
USTRUCT(BlueprintType)
struct FMemoryNode
{
    GENERATED_BODY()

    /** Unique node identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 NodeID = 0;

    /** Node type */
    UPROPERTY(BlueprintReadWrite)
    EMemoryNodeType NodeType = EMemoryNodeType::Percept;

    /** Human-readable label */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Content embedding vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Embedding;

    /** Current activation level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Base strength/salience (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    double CreationTime = 0.0;

    /** Last access timestamp */
    UPROPERTY(BlueprintReadWrite)
    double LastAccessTime = 0.0;

    /** Access count for frequency tracking */
    UPROPERTY(BlueprintReadWrite)
    int32 AccessCount = 0;

    /** Emotional valence (-1 to 1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Emotional arousal (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalArousal = 0.0f;

    /** Confidence/certainty (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Outgoing edge IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> OutgoingEdges;

    /** Incoming edge IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> IncomingEdges;

    /** Hyperedge membership (hyperedge IDs this node belongs to) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> HyperedgeMembership;

    /** Arbitrary properties */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> Properties;
};

/**
 * Directed edge connecting two nodes
 */
USTRUCT(BlueprintType)
struct FMemoryEdge
{
    GENERATED_BODY()

    /** Unique edge identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 EdgeID = 0;

    /** Source node ID */
    UPROPERTY(BlueprintReadWrite)
    int64 SourceNodeID = 0;

    /** Target node ID */
    UPROPERTY(BlueprintReadWrite)
    int64 TargetNodeID = 0;

    /** Semantic relation type */
    UPROPERTY(BlueprintReadWrite)
    ESemanticRelation RelationType = ESemanticRelation::ASSOCIATED_WITH;

    /** Custom relation label (if CUSTOM type) */
    UPROPERTY(BlueprintReadWrite)
    FString CustomRelationLabel;

    /** Edge weight/strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Confidence in this relation (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Temporal validity start (0 = always valid) */
    UPROPERTY(BlueprintReadWrite)
    double ValidFrom = 0.0;

    /** Temporal validity end (0 = still valid) */
    UPROPERTY(BlueprintReadWrite)
    double ValidUntil = 0.0;

    /** Is this edge bidirectional */
    UPROPERTY(BlueprintReadWrite)
    bool bBidirectional = false;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    double CreationTime = 0.0;

    /** Source/provenance of this edge */
    UPROPERTY(BlueprintReadWrite)
    FString Source;
};

/**
 * Hyperedge connecting multiple nodes (n-ary relation)
 */
USTRUCT(BlueprintType)
struct FMemoryHyperedge
{
    GENERATED_BODY()

    /** Unique hyperedge identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 HyperedgeID = 0;

    /** Human-readable label */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Member node IDs (ordered for role assignment) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> MemberNodes;

    /** Role labels for each member (parallel to MemberNodes) */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> MemberRoles;

    /** Hyperedge type/schema name */
    UPROPERTY(BlueprintReadWrite)
    FString HyperedgeType;

    /** Overall weight/importance */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Confidence in this hyperedge */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    double CreationTime = 0.0;
};

/**
 * Intentional state (Belief, Desire, or Intention)
 */
USTRUCT(BlueprintType)
struct FIntentionalState
{
    GENERATED_BODY()

    /** Associated node ID */
    UPROPERTY(BlueprintReadWrite)
    int64 NodeID = 0;

    /** Propositional content (what is believed/desired/intended) */
    UPROPERTY(BlueprintReadWrite)
    FString Proposition;

    /** Content node IDs (structured representation) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> ContentNodes;

    /** Belief modality */
    UPROPERTY(BlueprintReadWrite)
    EBeliefModality Modality = EBeliefModality::Probable;

    /** Intention status (for intentions) */
    UPROPERTY(BlueprintReadWrite)
    EIntentionStatus Status = EIntentionStatus::Deliberating;

    /** Strength/intensity of the state (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Intensity = 0.5f;

    /** Priority relative to other states */
    UPROPERTY(BlueprintReadWrite)
    float Priority = 0.5f;

    /** Evidence supporting this state (node IDs) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> SupportingEvidence;

    /** Conflicting states (node IDs) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> ConflictingStates;

    /** Deadline/urgency (0 = no deadline) */
    UPROPERTY(BlueprintReadWrite)
    double Deadline = 0.0;
};

/**
 * Graph query for memory retrieval
 */
USTRUCT(BlueprintType)
struct FMemoryQuery
{
    GENERATED_BODY()

    /** Query operator */
    UPROPERTY(BlueprintReadWrite)
    EQueryOperator Operator = EQueryOperator::AND;

    /** Node type filter (empty = any) */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMemoryNodeType> NodeTypes;

    /** Relation type filter (empty = any) */
    UPROPERTY(BlueprintReadWrite)
    TArray<ESemanticRelation> RelationTypes;

    /** Content embedding for similarity search */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ContentCue;

    /** Minimum similarity threshold */
    UPROPERTY(BlueprintReadWrite)
    float SimilarityThreshold = 0.5f;

    /** Minimum activation threshold */
    UPROPERTY(BlueprintReadWrite)
    float ActivationThreshold = 0.0f;

    /** Start node for path queries */
    UPROPERTY(BlueprintReadWrite)
    int64 StartNodeID = 0;

    /** End node for path queries */
    UPROPERTY(BlueprintReadWrite)
    int64 EndNodeID = 0;

    /** Maximum path length for transitive queries */
    UPROPERTY(BlueprintReadWrite)
    int32 MaxPathLength = 5;

    /** Maximum results to return */
    UPROPERTY(BlueprintReadWrite)
    int32 MaxResults = 100;

    /** Sub-queries for compound queries */
    UPROPERTY(BlueprintReadWrite)
    TArray<FMemoryQuery> SubQueries;
};

/**
 * Query result with relevance scoring
 */
USTRUCT(BlueprintType)
struct FMemoryQueryResult
{
    GENERATED_BODY()

    /** Matching node IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> MatchingNodes;

    /** Relevance scores (parallel to MatchingNodes) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RelevanceScores;

    /** Paths found (for path queries) */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<int64>> Paths;

    /** Total matches before limit */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalMatches = 0;

    /** Query execution time in milliseconds */
    UPROPERTY(BlueprintReadWrite)
    float ExecutionTimeMs = 0.0f;
};

/**
 * Spreading activation configuration
 */
USTRUCT(BlueprintType)
struct FSpreadingActivationConfig
{
    GENERATED_BODY()

    /** Decay factor per hop (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DecayFactor = 0.5f;

    /** Maximum propagation depth */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxDepth = 3;

    /** Minimum activation to propagate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float MinActivation = 0.05f;

    /** Use edge weights in spreading */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseEdgeWeights = true;

    /** Relation types to follow (empty = all) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ESemanticRelation> AllowedRelations;

    /** Parallel spreading using Taskflow */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bParallelSpreading = true;
};

/**
 * Memory consolidation configuration
 */
USTRUCT(BlueprintType)
struct FConsolidationConfig
{
    GENERATED_BODY()

    /** Strength decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001", ClampMax = "0.1"))
    float StrengthDecayRate = 0.001f;

    /** Activation decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ActivationDecayRate = 0.1f;

    /** Minimum strength before pruning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float PruningThreshold = 0.05f;

    /** Replay strengthening factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "2.0"))
    float ReplayStrength = 1.2f;

    /** Similarity threshold for concept abstraction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5", ClampMax = "0.99"))
    float AbstractionThreshold = 0.8f;

    /** Enable reservoir-based pattern detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableReservoirIntegration = true;
};

/**
 * Hypergraph memory statistics
 */
USTRUCT(BlueprintType)
struct FHypergraphStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 TotalNodes = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalEdges = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalHyperedges = 0;

    UPROPERTY(BlueprintReadOnly)
    TMap<EMemoryNodeType, int32> NodesByType;

    UPROPERTY(BlueprintReadOnly)
    TMap<ESemanticRelation, int32> EdgesByRelation;

    UPROPERTY(BlueprintReadOnly)
    float AverageActivation = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float AverageStrength = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 IntentionalStates = 0;

    UPROPERTY(BlueprintReadOnly)
    float MemoryUsageBytes = 0.0f;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNodeCreated, int64, NodeID, EMemoryNodeType, NodeType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEdgeCreated, int64, EdgeID, int64, SourceID, int64, TargetID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNodeActivated, int64, NodeID, float, Activation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConsolidationCycle, int32, NodesProcessed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIntentionUpdated, int64, NodeID, EIntentionStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternDetected, FString, PatternType, TArray<int64>, InvolvedNodes);

/**
 * HypergraphMemorySystem
 *
 * Unified hypergraph-based memory architecture implementing:
 * - Declarative memory (semantic concepts, facts)
 * - Procedural memory (skills, action schemas)
 * - Episodic memory (events, experiences)
 * - Intentional memory (beliefs, desires, intentions - BDI)
 *
 * Features:
 * - Typed nodes with semantic relation edges
 * - Hyperedges for n-ary relations
 * - Spreading activation with relation-aware propagation
 * - Graph queries with pattern matching
 * - Reservoir computing integration for pattern detection
 * - Taskflow-accelerated parallel processing
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UHypergraphMemorySystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UHypergraphMemorySystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Spreading activation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    FSpreadingActivationConfig SpreadingConfig;

    /** Consolidation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    FConsolidationConfig ConsolidationConfig;

    /** Default embedding dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    int32 EmbeddingDimension = 128;

    /** Enable automatic consolidation during tick */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    bool bEnableAutoConsolidation = true;

    /** Consolidation interval in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    float ConsolidationInterval = 5.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnNodeCreated OnNodeCreated;

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnEdgeCreated OnEdgeCreated;

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnNodeActivated OnNodeActivated;

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnConsolidationCycle OnConsolidationCycle;

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnIntentionUpdated OnIntentionUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Hypergraph|Events")
    FOnPatternDetected OnPatternDetected;

    // ========================================
    // NODE MANAGEMENT
    // ========================================

    /** Create a new memory node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    int64 CreateNode(EMemoryNodeType NodeType, const FString& Label,
                     const TArray<float>& Embedding, float InitialStrength = 0.5f);

    /** Get node by ID */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Nodes")
    FMemoryNode GetNode(int64 NodeID) const;

    /** Update node embedding */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    void UpdateNodeEmbedding(int64 NodeID, const TArray<float>& NewEmbedding);

    /** Set node activation */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    void SetNodeActivation(int64 NodeID, float Activation);

    /** Boost node activation (adds to current) */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    void BoostNodeActivation(int64 NodeID, float Amount);

    /** Set node property */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    void SetNodeProperty(int64 NodeID, const FString& Key, const FString& Value);

    /** Get node property */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Nodes")
    FString GetNodeProperty(int64 NodeID, const FString& Key) const;

    /** Delete node (and connected edges) */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Nodes")
    bool DeleteNode(int64 NodeID);

    /** Check if node exists */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Nodes")
    bool NodeExists(int64 NodeID) const;

    // ========================================
    // EDGE MANAGEMENT
    // ========================================

    /** Create directed edge between nodes */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Edges")
    int64 CreateEdge(int64 SourceNodeID, int64 TargetNodeID,
                     ESemanticRelation RelationType, float Weight = 1.0f);

    /** Create bidirectional edge */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Edges")
    int64 CreateBidirectionalEdge(int64 NodeA, int64 NodeB,
                                   ESemanticRelation RelationType, float Weight = 1.0f);

    /** Create custom relation edge */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Edges")
    int64 CreateCustomEdge(int64 SourceNodeID, int64 TargetNodeID,
                           const FString& CustomRelation, float Weight = 1.0f);

    /** Get edge by ID */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Edges")
    FMemoryEdge GetEdge(int64 EdgeID) const;

    /** Get edges between two nodes */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Edges")
    TArray<FMemoryEdge> GetEdgesBetween(int64 NodeA, int64 NodeB) const;

    /** Get outgoing edges from node */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Edges")
    TArray<FMemoryEdge> GetOutgoingEdges(int64 NodeID) const;

    /** Get incoming edges to node */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Edges")
    TArray<FMemoryEdge> GetIncomingEdges(int64 NodeID) const;

    /** Update edge weight */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Edges")
    void UpdateEdgeWeight(int64 EdgeID, float NewWeight);

    /** Delete edge */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Edges")
    bool DeleteEdge(int64 EdgeID);

    // ========================================
    // HYPEREDGE MANAGEMENT
    // ========================================

    /** Create hyperedge connecting multiple nodes */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Hyperedges")
    int64 CreateHyperedge(const TArray<int64>& MemberNodes, const TArray<FString>& Roles,
                          const FString& HyperedgeType, float Weight = 1.0f);

    /** Get hyperedge by ID */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Hyperedges")
    FMemoryHyperedge GetHyperedge(int64 HyperedgeID) const;

    /** Get hyperedges containing node */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Hyperedges")
    TArray<FMemoryHyperedge> GetNodeHyperedges(int64 NodeID) const;

    /** Delete hyperedge */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Hyperedges")
    bool DeleteHyperedge(int64 HyperedgeID);

    // ========================================
    // INTENTIONAL MEMORY (BDI)
    // ========================================

    /** Create belief node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Intentional")
    int64 CreateBelief(const FString& Proposition, const TArray<int64>& ContentNodes,
                       EBeliefModality Modality = EBeliefModality::Probable, float Confidence = 0.7f);

    /** Create desire/goal node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Intentional")
    int64 CreateDesire(const FString& Goal, const TArray<int64>& ContentNodes,
                       float Intensity = 0.5f, float Priority = 0.5f);

    /** Create intention node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Intentional")
    int64 CreateIntention(const FString& Plan, const TArray<int64>& ContentNodes,
                          int64 DesireNodeID, float Priority = 0.5f);

    /** Update intention status */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Intentional")
    void UpdateIntentionStatus(int64 IntentionNodeID, EIntentionStatus NewStatus);

    /** Get intentional state for node */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Intentional")
    FIntentionalState GetIntentionalState(int64 NodeID) const;

    /** Get active beliefs */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Intentional")
    TArray<int64> GetActiveBeliefs(float MinConfidence = 0.5f) const;

    /** Get active desires */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Intentional")
    TArray<int64> GetActiveDesires(float MinIntensity = 0.3f) const;

    /** Get active intentions */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Intentional")
    TArray<int64> GetActiveIntentions() const;

    /** Check belief consistency */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Intentional")
    TArray<int64> FindContradictingBeliefs(int64 BeliefNodeID) const;

    // ========================================
    // SPREADING ACTIVATION
    // ========================================

    /** Spread activation from source node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Activation")
    void SpreadActivation(int64 SourceNodeID, float InitialActivation);

    /** Spread activation from multiple sources */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Activation")
    void SpreadActivationMultiple(const TArray<int64>& SourceNodes,
                                   const TArray<float>& InitialActivations);

    /** Decay all activations */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Activation")
    void DecayActivations(float DeltaTime);

    /** Get most active nodes */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Activation")
    TArray<int64> GetMostActiveNodes(int32 Count = 10) const;

    /** Reset all activations */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Activation")
    void ResetActivations();

    // ========================================
    // GRAPH QUERIES
    // ========================================

    /** Execute memory query */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Query")
    FMemoryQueryResult ExecuteQuery(const FMemoryQuery& Query);

    /** Find nodes by type */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Query")
    TArray<int64> FindNodesByType(EMemoryNodeType NodeType, int32 MaxResults = 100) const;

    /** Find nodes by similarity to embedding */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Query")
    TArray<int64> FindSimilarNodes(const TArray<float>& Embedding,
                                    float Threshold = 0.5f, int32 MaxResults = 10);

    /** Find path between nodes */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Query")
    TArray<int64> FindPath(int64 StartNodeID, int64 EndNodeID,
                           int32 MaxLength = 5, const TArray<ESemanticRelation>& AllowedRelations = {});

    /** Get neighbors of node (connected via any edge) */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Query")
    TArray<int64> GetNeighbors(int64 NodeID, bool bIncludeIncoming = true,
                                bool bIncludeOutgoing = true) const;

    /** Get neighbors via specific relation */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Query")
    TArray<int64> GetNeighborsByRelation(int64 NodeID, ESemanticRelation Relation,
                                          bool bOutgoing = true) const;

    // ========================================
    // CONSOLIDATION & MAINTENANCE
    // ========================================

    /** Run consolidation cycle */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Consolidation")
    void RunConsolidation();

    /** Prune weak memories */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Consolidation")
    int32 PruneWeakMemories(float StrengthThreshold = -1.0f);

    /** Strengthen frequently accessed memories */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Consolidation")
    void StrengthenFrequentMemories();

    /** Detect and create semantic abstractions */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Consolidation")
    TArray<int64> DetectAbstractions(float SimilarityThreshold = 0.8f);

    /** Integrate patterns from reservoir computing */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Consolidation")
    void IntegrateReservoirPatterns(const TArray<TArray<float>>& DetectedPatterns);

    // ========================================
    // RESERVOIR INTEGRATION
    // ========================================

    /** Process temporal sequence through reservoir and encode result */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Reservoir")
    int64 EncodeTemporalPattern(const TArray<TArray<float>>& Sequence,
                                 const FString& Label, int32 StreamID = 1);

    /** Get reservoir-processed embedding for content */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Reservoir")
    TArray<float> GetReservoirEmbedding(const TArray<float>& Input, int32 StreamID = 1);

    // ========================================
    // STATISTICS & DEBUGGING
    // ========================================

    /** Get hypergraph statistics */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Stats")
    FHypergraphStats GetStatistics() const;

    /** Export graph to DOT format */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Debug")
    FString ExportToDOT() const;

    /** Export graph to JSON */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Debug")
    bool ExportToJSON(const FString& FilePath) const;

    /** Import graph from JSON */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Debug")
    bool ImportFromJSON(const FString& FilePath);

    /** Clear all memory */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Debug")
    void ClearAll();

protected:
    // Component references
    UPROPERTY()
    UDeepTreeEchoReservoir* ReservoirComponent;

    UPROPERTY()
    UTaskflowCognitiveScheduler* TaskflowScheduler;

    UPROPERTY()
    UEpisodicMemorySystem* LegacyMemorySystem;

    // Graph storage
    TMap<int64, FMemoryNode> Nodes;
    TMap<int64, FMemoryEdge> Edges;
    TMap<int64, FMemoryHyperedge> Hyperedges;
    TMap<int64, FIntentionalState> IntentionalStates;

    // ID generators
    int64 NextNodeID = 1;
    int64 NextEdgeID = 1;
    int64 NextHyperedgeID = 1;

    // Indices for fast lookup
    TMap<EMemoryNodeType, TArray<int64>> NodeTypeIndex;
    TMap<ESemanticRelation, TArray<int64>> EdgeRelationIndex;

    // Consolidation state
    float LastConsolidationTime = 0.0f;
    float AccumulatedTime = 0.0f;

    // Thread safety
    FCriticalSection GraphLock;

    // Internal methods
    void FindComponentReferences();
    float ComputeSimilarity(const TArray<float>& A, const TArray<float>& B) const;
    void PropagateActivationInternal(int64 NodeID, float Activation, int32 Depth,
                                      TSet<int64>& Visited);
    void UpdateNodeIndex(int64 NodeID, EMemoryNodeType OldType, EMemoryNodeType NewType);
    void UpdateEdgeIndex(int64 EdgeID, ESemanticRelation Relation, bool bAdd);
    void RecordAccess(int64 NodeID);
    TArray<int64> BreadthFirstSearch(int64 StartNodeID, int64 EndNodeID, int32 MaxDepth,
                                      const TSet<ESemanticRelation>& AllowedRelations) const;
};
