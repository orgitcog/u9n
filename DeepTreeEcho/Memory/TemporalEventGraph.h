// TemporalEventGraph.h
// Feature F1.5.1: Temporal Event Graph with B-Series Rooted Tree Structure
// Implements B-series rooted tree structure for representing temporal events
// with before/after relations for Deep Tree Echo cognitive architecture.
// Copyright (c) 2025-2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TemporalEventGraph.generated.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class UEpisodicMemorySystem;
class UAtomLinkTypeSystem;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * Temporal relation types between events
 */
UENUM(BlueprintType)
enum class ETemporalRelation : uint8
{
    /** Event A happens before Event B */
    Before          UMETA(DisplayName = "Before"),
    
    /** Event A happens after Event B */
    After           UMETA(DisplayName = "After"),
    
    /** Event A happens during Event B */
    During          UMETA(DisplayName = "During"),
    
    /** Events A and B overlap */
    Overlaps        UMETA(DisplayName = "Overlaps"),
    
    /** Events A and B start simultaneously */
    Starts          UMETA(DisplayName = "Starts"),
    
    /** Events A and B finish simultaneously */
    Finishes        UMETA(DisplayName = "Finishes"),
    
    /** Events A and B are equal in time */
    Equals          UMETA(DisplayName = "Equals"),
    
    /** Event A meets Event B (A ends when B starts) */
    Meets           UMETA(DisplayName = "Meets"),
    
    /** Events A and B are simultaneous (co-temporal) */
    Simultaneous    UMETA(DisplayName = "Simultaneous")
};

/**
 * Rooted tree node types for B-series representation
 */
UENUM(BlueprintType)
enum class ERootedTreeNodeType : uint8
{
    /** Root node of the tree */
    Root            UMETA(DisplayName = "Root"),
    
    /** Interior node with children */
    Interior        UMETA(DisplayName = "Interior"),
    
    /** Leaf node (no children) */
    Leaf            UMETA(DisplayName = "Leaf"),
    
    /** Elementary differential node */
    Elementary      UMETA(DisplayName = "Elementary")
};

/**
 * B-series order for numerical integration context
 */
UENUM(BlueprintType)
enum class EBSeriesOrder : uint8
{
    /** Order 1: f (single node) */
    Order1          UMETA(DisplayName = "Order 1"),
    
    /** Order 2: f'f */
    Order2          UMETA(DisplayName = "Order 2"),
    
    /** Order 3: f''ff, f'(f'f) */
    Order3          UMETA(DisplayName = "Order 3"),
    
    /** Order 4: multiple trees */
    Order4          UMETA(DisplayName = "Order 4"),
    
    /** Order 5 and higher */
    Order5Plus      UMETA(DisplayName = "Order 5+")
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * Rooted Tree Node - Represents a node in the B-series rooted tree
 * Following OEIS A000081 enumeration
 */
USTRUCT(BlueprintType)
struct FRootedTreeNode
{
    GENERATED_BODY()

    /** Unique node identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    int64 NodeID = 0;

    /** Node type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    ERootedTreeNodeType NodeType = ERootedTreeNodeType::Leaf;

    /** Parent node ID (-1 for root) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    int64 ParentID = -1;

    /** Child node IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    TArray<int64> ChildIDs;

    /** Depth in tree (0 = root) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    int32 Depth = 0;

    /** Subtree size (including this node) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
    int32 SubtreeSize = 1;

    /** B-series coefficient for this node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BSeriesCoeff")
    float BSeriesCoefficient = 1.0f;

    /** Associated temporal event ID (if any) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temporal")
    int64 EventID = -1;

    /** Parentheses notation for this subtree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notation")
    FString ParenthesesNotation;

    FRootedTreeNode() = default;

    FRootedTreeNode(int64 InNodeID, ERootedTreeNodeType InType)
        : NodeID(InNodeID)
        , NodeType(InType)
    {}

    /** Check if this is a root node */
    bool IsRoot() const { return ParentID < 0; }

    /** Check if this is a leaf node */
    bool IsLeaf() const { return ChildIDs.Num() == 0; }

    /** Get number of children */
    int32 GetChildCount() const { return ChildIDs.Num(); }
};

/**
 * Temporal Event - A point or interval in the temporal event graph
 */
USTRUCT(BlueprintType)
struct FTemporalEvent
{
    GENERATED_BODY()

    /** Unique event identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    int64 EventID = 0;

    /** Event name/label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FString Name;

    /** Event description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FString Description;

    /** Start timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temporal")
    double StartTime = 0.0;

    /** End timestamp (same as start for point events) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temporal")
    double EndTime = 0.0;

    /** Duration (0 for instantaneous events) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temporal")
    double Duration = 0.0;

    /** Associated rooted tree node IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    TArray<int64> TreeNodeIDs;

    /** Predecessor event IDs (events before this one) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relations")
    TArray<int64> PredecessorIDs;

    /** Successor event IDs (events after this one) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relations")
    TArray<int64> SuccessorIDs;

    /** Concurrent event IDs (events simultaneous with this one) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relations")
    TArray<int64> ConcurrentIDs;

    /** Event significance (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
    float Significance = 0.5f;

    /** Episodic memory trace ID (if linked) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
    int64 MemoryTraceID = -1;

    /** Custom properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
    TMap<FString, FString> Properties;

    /** Creation timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
    FDateTime CreatedAt;

    FTemporalEvent()
        : CreatedAt(FDateTime::Now())
    {}

    /** Check if this is a point event (instantaneous) */
    bool IsPointEvent() const { return FMath::IsNearlyZero(Duration); }

    /** Check if this is an interval event (has duration) */
    bool IsIntervalEvent() const { return !FMath::IsNearlyZero(Duration); }
};

/**
 * Temporal Relation Edge - Represents a temporal relation between two events
 */
USTRUCT(BlueprintType)
struct FTemporalRelationEdge
{
    GENERATED_BODY()

    /** Unique edge identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    int64 EdgeID = 0;

    /** Source event ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    int64 SourceEventID = 0;

    /** Target event ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    int64 TargetEventID = 0;

    /** Relation type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    ETemporalRelation RelationType = ETemporalRelation::Before;

    /** Relation strength/confidence (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    float Strength = 1.0f;

    /** Is this relation inferred or explicit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
    bool bIsInferred = false;

    FTemporalRelationEdge() = default;

    FTemporalRelationEdge(int64 InEdgeID, int64 InSourceID, int64 InTargetID, ETemporalRelation InRelation)
        : EdgeID(InEdgeID)
        , SourceEventID(InSourceID)
        , TargetEventID(InTargetID)
        , RelationType(InRelation)
    {}
};

/**
 * Rooted Tree Structure - Complete B-series rooted tree representation
 */
USTRUCT(BlueprintType)
struct FRootedTree
{
    GENERATED_BODY()

    /** Unique tree identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
    int64 TreeID = 0;

    /** Root node ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
    int64 RootNodeID = 0;

    /** All nodes in this tree (NodeID -> Node) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
    TArray<FRootedTreeNode> Nodes;

    /** Total node count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
    int32 NodeCount = 0;

    /** Tree order (number of nodes, for B-series) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BSeriesOrder")
    int32 Order = 1;

    /** B-series coefficient for entire tree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BSeriesCoeff")
    float Coefficient = 1.0f;

    /** Symmetry factor (sigma) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BSeriesCoeff")
    int32 SymmetryFactor = 1;

    /** Density (gamma) - product of subtree sizes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BSeriesCoeff")
    int32 Density = 1;

    /** Canonical parentheses notation (e.g., "(())" for order 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notation")
    FString ParenthesesNotation;

    /** Level sequence representation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notation")
    TArray<int32> LevelSequence;

    FRootedTree() = default;

    /** Get the root node */
    const FRootedTreeNode* GetRootNode() const;
    
    /** Get a node by ID */
    const FRootedTreeNode* GetNode(int64 NodeID) const;
};

/**
 * Temporal Event Graph Statistics
 */
USTRUCT(BlueprintType)
struct FTemporalEventGraphStats
{
    GENERATED_BODY()

    /** Total number of events */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalEvents = 0;

    /** Total number of temporal relations */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalRelations = 0;

    /** Total number of rooted trees */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalTrees = 0;

    /** Maximum tree depth */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MaxTreeDepth = 0;

    /** Maximum B-series order computed */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MaxBSeriesOrder = 0;

    /** Number of before relations */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 BeforeRelationCount = 0;

    /** Number of after relations */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 AfterRelationCount = 0;

    /** Number of simultaneous relations */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 SimultaneousCount = 0;
};

/**
 * Temporal Event Graph Configuration
 */
USTRUCT(BlueprintType)
struct FTemporalEventGraphConfig
{
    GENERATED_BODY()

    /** Maximum B-series order to precompute */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "1", ClampMax = "12"))
    int32 MaxBSeriesOrder = 6;

    /** Enable automatic temporal inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableAutomaticInference = true;

    /** Minimum time difference to consider events sequential (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    double MinSequentialGap = 0.001;

    /** Maximum events to track */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "100", ClampMax = "100000"))
    int32 MaxEvents = 10000;

    /** Enable transitive closure for temporal relations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableTransitiveClosure = true;

    /** Cache B-series trees on startup */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bCacheBSeriesTrees = true;
};

// ============================================================================
// TEMPORAL EVENT GRAPH COMPONENT
// ============================================================================

/**
 * Temporal Event Graph Component
 *
 * Feature F1.5.1: Implements B-series rooted tree structure for representing
 * temporal events with before/after relations in the Deep Tree Echo cognitive
 * architecture.
 *
 * Key Features:
 * - B-series rooted tree enumeration following OEIS A000081
 * - Temporal event representation with before/after/during relations
 * - Allen's interval algebra support
 * - Automatic temporal inference and transitive closure
 * - Integration with EpisodicMemorySystem
 * - Butcher coefficient computation for numerical integration
 *
 * Scientific Foundation:
 * - Butcher, J.C. (1972): "B-series for numerical methods"
 * - Allen, J.F. (1983): "Maintaining knowledge about temporal intervals"
 * - OEIS A000081: Number of unlabeled rooted trees with n nodes
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTemporalEventGraph : public UActorComponent
{
    GENERATED_BODY()

public:
    UTemporalEventGraph();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Graph configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalGraph|Config")
    FTemporalEventGraphConfig Config;

    /** Reference to EpisodicMemorySystem (optional) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalGraph|Config")
    UEpisodicMemorySystem* EpisodicMemory;

    /** Enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalGraph|Debug")
    bool bEnableDebugLogging = false;

    // ========================================
    // STATE
    // ========================================

    /** Graph statistics */
    UPROPERTY(BlueprintReadOnly, Category = "TemporalGraph|State")
    FTemporalEventGraphStats Stats;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize the temporal event graph */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph")
    void Initialize();

    /** Reset the graph to empty state */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph")
    void Reset();

    /** Pre-compute B-series trees up to configured order */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph")
    void PrecomputeBSeriesTrees();

    // ========================================
    // EVENT MANAGEMENT
    // ========================================

    /**
     * Create a new temporal event
     * @param Name Event name
     * @param StartTime Event start time
     * @param EndTime Event end time (same as start for point events)
     * @param Significance Event significance (0-1)
     * @return New event ID
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Events")
    int64 CreateEvent(const FString& Name, double StartTime, double EndTime = 0.0, float Significance = 0.5f);

    /**
     * Create a point event (instantaneous)
     * @param Name Event name
     * @param Time Event time
     * @return New event ID
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Events")
    int64 CreatePointEvent(const FString& Name, double Time);

    /**
     * Create an interval event
     * @param Name Event name
     * @param StartTime Event start time
     * @param Duration Event duration
     * @return New event ID
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Events")
    int64 CreateIntervalEvent(const FString& Name, double StartTime, double Duration);

    /**
     * Get event by ID
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Events")
    FTemporalEvent GetEvent(int64 EventID) const;

    /**
     * Check if event exists
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Events")
    bool EventExists(int64 EventID) const;

    /**
     * Remove an event
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Events")
    bool RemoveEvent(int64 EventID);

    /**
     * Get all events
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Events")
    TArray<FTemporalEvent> GetAllEvents() const;

    /**
     * Get events in time range
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Events")
    TArray<FTemporalEvent> GetEventsInRange(double StartTime, double EndTime) const;

    // ========================================
    // TEMPORAL RELATIONS
    // ========================================

    /**
     * Add a temporal relation between two events
     * @param SourceEventID First event
     * @param TargetEventID Second event
     * @param Relation Type of temporal relation
     * @param Strength Relation strength (0-1)
     * @return Relation edge ID
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    int64 AddRelation(int64 SourceEventID, int64 TargetEventID, ETemporalRelation Relation, float Strength = 1.0f);

    /**
     * Add a before relation (A happens before B)
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    int64 AddBeforeRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

    /**
     * Add an after relation (A happens after B)
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    int64 AddAfterRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

    /**
     * Add a simultaneous relation (A and B happen at the same time)
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    int64 AddSimultaneousRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

    /**
     * Get the relation between two events
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Relations")
    ETemporalRelation GetRelation(int64 EventA, int64 EventB) const;

    /**
     * Get all predecessor events (events before this one)
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Relations")
    TArray<int64> GetPredecessors(int64 EventID) const;

    /**
     * Get all successor events (events after this one)
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Relations")
    TArray<int64> GetSuccessors(int64 EventID) const;

    /**
     * Get all concurrent events (events simultaneous with this one)
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Relations")
    TArray<int64> GetConcurrentEvents(int64 EventID) const;

    /**
     * Compute transitive closure of temporal relations
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    void ComputeTransitiveClosure();

    /**
     * Infer temporal relations from timestamps
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Relations")
    int32 InferTemporalRelations();

    // ========================================
    // B-SERIES ROOTED TREES
    // ========================================

    /**
     * Generate all rooted trees of given order (A000081)
     * @param Order Number of nodes
     * @return Array of rooted trees
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|BSeriesTrees")
    TArray<FRootedTree> GenerateRootedTrees(int32 Order);

    /**
     * Get count of rooted trees for given order (A000081 sequence)
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    int32 GetRootedTreeCount(int32 Order) const;

    /**
     * Create a rooted tree from parentheses notation
     * @param Notation Parentheses notation (e.g., "(())", "((()))")
     * @return New tree structure
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|BSeriesTrees")
    FRootedTree CreateTreeFromNotation(const FString& Notation);

    /**
     * Get parentheses notation for a tree
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    FString GetTreeNotation(int64 TreeID) const;

    /**
     * Compute B-series coefficient for a tree
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    float ComputeBSeriesCoefficient(int64 TreeID) const;

    /**
     * Compute symmetry factor (sigma) for a tree
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    int32 ComputeSymmetryFactor(int64 TreeID) const;

    /**
     * Compute density (gamma) for a tree
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    int32 ComputeDensity(int64 TreeID) const;

    /**
     * Get all precomputed trees
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    TArray<FRootedTree> GetAllTrees() const;

    /**
     * Get trees by order
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|BSeriesTrees")
    TArray<FRootedTree> GetTreesByOrder(int32 Order) const;

    // ========================================
    // EVENT-TREE ASSOCIATION
    // ========================================

    /**
     * Associate an event with a rooted tree node
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Association")
    bool AssociateEventWithTree(int64 EventID, int64 TreeID, int64 NodeID);

    /**
     * Build temporal tree from event sequence
     * @param EventIDs Ordered sequence of events
     * @return New tree representing the temporal structure
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Association")
    FRootedTree BuildTreeFromEventSequence(const TArray<int64>& EventIDs);

    // ========================================
    // MEMORY INTEGRATION
    // ========================================

    /**
     * Link event to episodic memory trace
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Memory")
    bool LinkEventToMemory(int64 EventID, int64 MemoryTraceID);

    /**
     * Get events linked to a memory trace
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Memory")
    TArray<int64> GetEventsForMemory(int64 MemoryTraceID) const;

    // ========================================
    // QUERY & ANALYSIS
    // ========================================

    /**
     * Get topological sort of events (respecting temporal order)
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Query")
    TArray<int64> GetTopologicalOrder() const;

    /**
     * Find causal chains (sequences of before relations)
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Query")
    TArray<TArray<int64>> FindCausalChains(int32 MinLength = 2) const;

    /**
     * Check if EventA can reach EventB through before relations
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Query")
    bool CanReach(int64 EventA, int64 EventB) const;

    // ========================================
    // STATISTICS
    // ========================================

    /**
     * Get current statistics
     */
    UFUNCTION(BlueprintPure, Category = "TemporalGraph|Stats")
    FTemporalEventGraphStats GetStats() const;

    /**
     * Generate diagnostic report
     */
    UFUNCTION(BlueprintCallable, Category = "TemporalGraph|Stats")
    TArray<FString> GenerateDiagnosticReport() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when a new event is created */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventCreated, int64, EventID);
    UPROPERTY(BlueprintAssignable, Category = "TemporalGraph|Events")
    FOnEventCreated OnEventCreated;

    /** Called when a temporal relation is added */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRelationAdded, int64, SourceEventID, int64, TargetEventID, ETemporalRelation, Relation);
    UPROPERTY(BlueprintAssignable, Category = "TemporalGraph|Events")
    FOnRelationAdded OnRelationAdded;

    /** Called when a rooted tree is generated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTreeGenerated, int64, TreeID, int32, Order);
    UPROPERTY(BlueprintAssignable, Category = "TemporalGraph|Events")
    FOnTreeGenerated OnTreeGenerated;

protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** All temporal events (EventID -> Event) */
    TMap<int64, FTemporalEvent> Events;

    /** All temporal relations (EdgeID -> Edge) */
    TMap<int64, FTemporalRelationEdge> Relations;

    /** All rooted trees (TreeID -> Tree) */
    TMap<int64, FRootedTree> Trees;

    /** Trees by order (Order -> TreeIDs) */
    TMap<int32, TArray<int64>> TreesByOrder;

    /** A000081 sequence cache (order -> count) */
    TMap<int32, int32> A000081Cache;

    /** Next event ID */
    int64 NextEventID = 1;

    /** Next edge ID */
    int64 NextEdgeID = 1;

    /** Next tree ID */
    int64 NextTreeID = 1;

    /** Next node ID */
    int64 NextNodeID = 1;

    /** System initialized flag */
    bool bIsInitialized = false;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize A000081 cache */
    void InitializeA000081Cache(int32 MaxOrder);

    /** Recursive tree generation helper */
    void GenerateTreesRecursive(int32 Order, TArray<FRootedTree>& OutTrees);

    /** Assemble trees from smaller components */
    void AssembleTrees(int32 N, TArray<FRootedTree>& OutTrees, const TArray<TArray<FRootedTree>>& SmallerTrees);

    /** Create a tree node */
    FRootedTreeNode CreateNode(ERootedTreeNodeType NodeType);

    /** Compute subtree sizes recursively */
    int32 ComputeSubtreeSizes(FRootedTree& Tree, int64 NodeID);

    /** Generate parentheses notation for subtree */
    FString GenerateNotation(const FRootedTree& Tree, int64 NodeID) const;

    /** Update event predecessor/successor lists */
    void UpdateEventRelationLists(int64 SourceID, int64 TargetID, ETemporalRelation Relation);

    /** Compute A000081(n) using recurrence */
    int32 ComputeA000081(int32 N);

    /** Generate unique event ID */
    int64 GenerateEventID();

    /** Generate unique edge ID */
    int64 GenerateEdgeID();

    /** Generate unique tree ID */
    int64 GenerateTreeID();

    /** Update statistics */
    void UpdateStats();

    /** Get inverse relation (Before -> After, etc.) */
    ETemporalRelation GetInverseRelation(ETemporalRelation Relation) const;
};
