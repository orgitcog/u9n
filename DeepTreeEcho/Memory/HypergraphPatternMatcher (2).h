// HypergraphPatternMatcher.h
// Efficient pattern matching over hypergraph structures with variable binding
// Feature F1.4.2 - Hypergraph Reasoning Foundation
// Deep Tree Echo Cognitive Architecture

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HypergraphMemorySystem.h"
#include "HypergraphPatternMatcher.generated.h"

// Forward declarations
class UHypergraphMemorySystem;

// ========================================
// PATTERN MATCHING ENUMERATIONS
// ========================================

/**
 * Pattern element type
 */
UENUM(BlueprintType)
enum class EPatternElementType : uint8
{
    /** Concrete node reference */
    ConcreteNode        UMETA(DisplayName = "Concrete Node"),
    /** Variable to be bound */
    Variable            UMETA(DisplayName = "Variable"),
    /** Wildcard matching any single element */
    Wildcard            UMETA(DisplayName = "Wildcard"),
    /** Type constraint (matches any node of type) */
    TypeConstraint      UMETA(DisplayName = "Type Constraint"),
    /** Property constraint */
    PropertyConstraint  UMETA(DisplayName = "Property Constraint")
};

/**
 * Pattern constraint operator
 */
UENUM(BlueprintType)
enum class EConstraintOperator : uint8
{
    Equals              UMETA(DisplayName = "Equals"),
    NotEquals           UMETA(DisplayName = "Not Equals"),
    GreaterThan         UMETA(DisplayName = "Greater Than"),
    LessThan            UMETA(DisplayName = "Less Than"),
    GreaterOrEqual      UMETA(DisplayName = "Greater Or Equal"),
    LessOrEqual         UMETA(DisplayName = "Less Or Equal"),
    Contains            UMETA(DisplayName = "Contains"),
    StartsWith          UMETA(DisplayName = "Starts With"),
    EndsWith            UMETA(DisplayName = "Ends With"),
    Regex               UMETA(DisplayName = "Regex Match"),
    InRange             UMETA(DisplayName = "In Range"),
    IsNull              UMETA(DisplayName = "Is Null"),
    IsNotNull           UMETA(DisplayName = "Is Not Null")
};

/**
 * Pattern matching mode
 */
UENUM(BlueprintType)
enum class EPatternMatchMode : uint8
{
    /** Find first match only */
    FirstMatch          UMETA(DisplayName = "First Match"),
    /** Find all matches */
    AllMatches          UMETA(DisplayName = "All Matches"),
    /** Count matches without returning bindings */
    CountOnly           UMETA(DisplayName = "Count Only"),
    /** Check existence (true/false) */
    ExistsCheck         UMETA(DisplayName = "Exists Check")
};

/**
 * Variable binding scope
 */
UENUM(BlueprintType)
enum class EBindingScope : uint8
{
    /** Bound within current pattern only */
    Local               UMETA(DisplayName = "Local"),
    /** Shared across pattern clauses */
    Global              UMETA(DisplayName = "Global"),
    /** Persistent across queries */
    Persistent          UMETA(DisplayName = "Persistent")
};

// ========================================
// PATTERN MATCHING STRUCTURES
// ========================================

/**
 * Property constraint for pattern elements
 */
USTRUCT(BlueprintType)
struct FPropertyConstraint
{
    GENERATED_BODY()

    /** Property key to check */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PropertyKey;

    /** Constraint operator */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConstraintOperator Operator = EConstraintOperator::Equals;

    /** Value to compare against */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Value;

    /** Secondary value (for range checks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SecondaryValue;

    /** Is this a numeric comparison */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bNumericComparison = false;
};

/**
 * Pattern element representing a node position in a pattern
 */
USTRUCT(BlueprintType)
struct FPatternElement
{
    GENERATED_BODY()

    /** Element type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPatternElementType ElementType = EPatternElementType::Variable;

    /** Variable name (if Variable type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString VariableName;

    /** Concrete node ID (if ConcreteNode type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 ConcreteNodeID = 0;

    /** Type constraint (if TypeConstraint type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMemoryNodeType NodeTypeConstraint = EMemoryNodeType::Concept;

    /** Property constraints */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPropertyConstraint> PropertyConstraints;

    /** Minimum activation level (0 = no constraint) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinActivation = 0.0f;

    /** Minimum strength (0 = no constraint) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinStrength = 0.0f;

    /** Binding scope for variables */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBindingScope Scope = EBindingScope::Local;

    FPatternElement() = default;

    /** Create a variable element */
    static FPatternElement Variable(const FString& Name)
    {
        FPatternElement Elem;
        Elem.ElementType = EPatternElementType::Variable;
        Elem.VariableName = Name;
        return Elem;
    }

    /** Create a concrete node element */
    static FPatternElement Concrete(int64 NodeID)
    {
        FPatternElement Elem;
        Elem.ElementType = EPatternElementType::ConcreteNode;
        Elem.ConcreteNodeID = NodeID;
        return Elem;
    }

    /** Create a type constraint element */
    static FPatternElement TypedVariable(const FString& Name, EMemoryNodeType Type)
    {
        FPatternElement Elem;
        Elem.ElementType = EPatternElementType::TypeConstraint;
        Elem.VariableName = Name;
        Elem.NodeTypeConstraint = Type;
        return Elem;
    }

    /** Create a wildcard element */
    static FPatternElement Wildcard()
    {
        FPatternElement Elem;
        Elem.ElementType = EPatternElementType::Wildcard;
        return Elem;
    }
};

/**
 * Edge pattern for matching relationships
 */
USTRUCT(BlueprintType)
struct FEdgePattern
{
    GENERATED_BODY()

    /** Source node pattern element */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPatternElement Source;

    /** Target node pattern element */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPatternElement Target;

    /** Relation type constraint (optional) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ESemanticRelation> AllowedRelations;

    /** Custom relation label constraint (for CUSTOM relations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomRelationLabel;

    /** Whether to allow transitive matching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTransitive = false;

    /** Maximum transitive depth (if transitive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxTransitiveDepth = 5;

    /** Minimum edge weight (0 = no constraint) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinWeight = 0.0f;

    /** Whether direction matters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDirected = true;

    /** Variable name to bind the matching edge ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EdgeVariableName;
};

/**
 * Hyperedge pattern for matching n-ary relations
 */
USTRUCT(BlueprintType)
struct FHyperedgePattern
{
    GENERATED_BODY()

    /** Member node patterns (ordered by role) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPatternElement> Members;

    /** Expected roles (parallel to Members, empty for any) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ExpectedRoles;

    /** Hyperedge type constraint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString HyperedgeTypeConstraint;

    /** Minimum hyperedge weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinWeight = 0.0f;

    /** Variable name to bind the matching hyperedge ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString HyperedgeVariableName;

    /** Allow partial matches (subset of members) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowPartialMatch = false;

    /** Minimum members to match for partial matching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinMembersForPartial = 2;
};

/**
 * Complete graph pattern (conjunction of edge and hyperedge patterns)
 */
USTRUCT(BlueprintType)
struct FGraphPattern
{
    GENERATED_BODY()

    /** Pattern identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternID;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    /** Edge patterns to match */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEdgePattern> EdgePatterns;

    /** Hyperedge patterns to match */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHyperedgePattern> HyperedgePatterns;

    /** Standalone node patterns (for existence checks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPatternElement> NodePatterns;

    /** Cross-variable constraints (e.g., "?x.property == ?y.property") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> CrossConstraints;

    /** Negated patterns (NOT EXISTS) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEdgePattern> NegatedEdgePatterns;

    /** Optional patterns (may or may not match) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEdgePattern> OptionalEdgePatterns;

    /** Priority for pattern ordering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;
};

/**
 * Variable binding - maps a variable name to a bound value
 */
USTRUCT(BlueprintType)
struct FVariableBinding
{
    GENERATED_BODY()

    /** Variable name */
    UPROPERTY(BlueprintReadWrite)
    FString VariableName;

    /** Bound node ID (if node binding) */
    UPROPERTY(BlueprintReadWrite)
    int64 BoundNodeID = 0;

    /** Bound edge ID (if edge binding) */
    UPROPERTY(BlueprintReadWrite)
    int64 BoundEdgeID = 0;

    /** Bound hyperedge ID (if hyperedge binding) */
    UPROPERTY(BlueprintReadWrite)
    int64 BoundHyperedgeID = 0;

    /** Whether this is a node binding */
    UPROPERTY(BlueprintReadWrite)
    bool bIsNodeBinding = true;

    /** Whether this is an edge binding */
    UPROPERTY(BlueprintReadWrite)
    bool bIsEdgeBinding = false;

    /** Whether this is a hyperedge binding */
    UPROPERTY(BlueprintReadWrite)
    bool bIsHyperedgeBinding = false;

    /** Binding confidence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;
};

/**
 * Binding environment - complete set of variable bindings for a match
 */
USTRUCT(BlueprintType)
struct FBindingEnvironment
{
    GENERATED_BODY()

    /** All variable bindings */
    UPROPERTY(BlueprintReadWrite)
    TArray<FVariableBinding> Bindings;

    /** Match confidence (product of individual binding confidences) */
    UPROPERTY(BlueprintReadWrite)
    float MatchConfidence = 1.0f;

    /** Match relevance score */
    UPROPERTY(BlueprintReadWrite)
    float RelevanceScore = 1.0f;

    /** Whether this is a complete match */
    UPROPERTY(BlueprintReadWrite)
    bool bIsComplete = false;

    /** Get binding for variable */
    const FVariableBinding* GetBinding(const FString& VariableName) const
    {
        for (const FVariableBinding& Binding : Bindings)
        {
            if (Binding.VariableName == VariableName)
            {
                return &Binding;
            }
        }
        return nullptr;
    }

    /** Check if variable is bound */
    bool IsBound(const FString& VariableName) const
    {
        return GetBinding(VariableName) != nullptr;
    }

    /** Get bound node ID for variable (returns 0 if not bound) */
    int64 GetBoundNodeID(const FString& VariableName) const
    {
        const FVariableBinding* Binding = GetBinding(VariableName);
        return Binding && Binding->bIsNodeBinding ? Binding->BoundNodeID : 0;
    }
};

/**
 * Pattern match result
 */
USTRUCT(BlueprintType)
struct FPatternMatchResult
{
    GENERATED_BODY()

    /** Pattern that was matched */
    UPROPERTY(BlueprintReadWrite)
    FString PatternID;

    /** All successful binding environments */
    UPROPERTY(BlueprintReadWrite)
    TArray<FBindingEnvironment> MatchingBindings;

    /** Total number of matches found */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalMatches = 0;

    /** Whether any matches were found */
    UPROPERTY(BlueprintReadWrite)
    bool bHasMatches = false;

    /** Execution time in milliseconds */
    UPROPERTY(BlueprintReadWrite)
    float ExecutionTimeMs = 0.0f;

    /** Number of nodes examined */
    UPROPERTY(BlueprintReadWrite)
    int32 NodesExamined = 0;

    /** Number of edges examined */
    UPROPERTY(BlueprintReadWrite)
    int32 EdgesExamined = 0;

    /** Error message (if any) */
    UPROPERTY(BlueprintReadWrite)
    FString ErrorMessage;
};

/**
 * Pattern matching configuration
 */
USTRUCT(BlueprintType)
struct FPatternMatchConfig
{
    GENERATED_BODY()

    /** Matching mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPatternMatchMode MatchMode = EPatternMatchMode::AllMatches;

    /** Maximum matches to return (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxMatches = 1000;

    /** Timeout in milliseconds (0 = no timeout) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeoutMs = 1000.0f;

    /** Enable parallel matching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableParallel = true;

    /** Include optional patterns */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIncludeOptional = true;

    /** Sort results by relevance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSortByRelevance = true;

    /** Minimum match confidence threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinConfidence = 0.0f;

    /** Use index acceleration (if available) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseIndexes = true;
};

/**
 * Pattern index entry for acceleration
 */
USTRUCT()
struct FPatternIndexEntry
{
    GENERATED_BODY()

    /** Node type -> Node IDs */
    TMap<EMemoryNodeType, TArray<int64>> NodesByType;

    /** Relation type -> Edge IDs */
    TMap<ESemanticRelation, TArray<int64>> EdgesByRelation;

    /** Label -> Node IDs */
    TMap<FString, TArray<int64>> NodesByLabel;

    /** Property key -> Node IDs */
    TMap<FString, TArray<int64>> NodesByProperty;

    /** Last index update time */
    double LastUpdateTime = 0.0;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternMatched, const FString&, PatternID, int32, MatchCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBindingCreated, const FString&, VariableName, int64, BoundID, bool, bIsNode);

/**
 * HypergraphPatternMatcher
 * 
 * Efficient pattern matching engine for hypergraph structures with:
 * - Variable binding and unification
 * - Type and property constraints
 * - Transitive closure support
 * - Negation and optional patterns
 * - Index-accelerated matching
 * - Parallel execution support
 * 
 * Implements a backtracking algorithm with constraint propagation
 * optimized for the Deep Tree Echo hypergraph memory system.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UHypergraphPatternMatcher : public UActorComponent
{
    GENERATED_BODY()

public:
    UHypergraphPatternMatcher();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Default match configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PatternMatcher|Config")
    FPatternMatchConfig DefaultConfig;

    /** Enable automatic index maintenance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PatternMatcher|Config")
    bool bEnableAutoIndexing = true;

    /** Index update interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PatternMatcher|Config")
    float IndexUpdateInterval = 5.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "PatternMatcher|Events")
    FOnPatternMatched OnPatternMatched;

    UPROPERTY(BlueprintAssignable, Category = "PatternMatcher|Events")
    FOnBindingCreated OnBindingCreated;

    // ========================================
    // PATTERN REGISTRATION
    // ========================================

    /** Register a pattern for reuse */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Patterns")
    bool RegisterPattern(const FGraphPattern& Pattern);

    /** Unregister a pattern */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Patterns")
    bool UnregisterPattern(const FString& PatternID);

    /** Get registered pattern */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Patterns")
    FGraphPattern GetPattern(const FString& PatternID) const;

    /** List all registered pattern IDs */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Patterns")
    TArray<FString> GetRegisteredPatternIDs() const;

    /** Clear all registered patterns */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Patterns")
    void ClearAllPatterns();

    // ========================================
    // PATTERN MATCHING
    // ========================================

    /** Match a pattern against the hypergraph */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Matching")
    FPatternMatchResult MatchPattern(const FGraphPattern& Pattern, const FPatternMatchConfig& Config);

    /** Match a registered pattern by ID */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Matching")
    FPatternMatchResult MatchPatternByID(const FString& PatternID, const FPatternMatchConfig& Config);

    /** Match using default configuration */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Matching")
    FPatternMatchResult MatchPatternDefault(const FGraphPattern& Pattern);

    /** Quick existence check for a pattern */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Matching")
    bool PatternExists(const FGraphPattern& Pattern);

    /** Count pattern matches */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Matching")
    int32 CountPatternMatches(const FGraphPattern& Pattern);

    // ========================================
    // SIMPLE PATTERN BUILDERS
    // ========================================

    /** Create a simple edge pattern (A --relation--> B) */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Builders")
    static FEdgePattern CreateEdgePattern(
        const FString& SourceVar,
        const FString& TargetVar,
        ESemanticRelation Relation);

    /** Create a typed node pattern */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Builders")
    static FPatternElement CreateTypedNodePattern(
        const FString& VariableName,
        EMemoryNodeType NodeType);

    /** Create a simple graph pattern from a single edge */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Builders")
    static FGraphPattern CreateSimpleEdgeGraphPattern(
        const FString& SourceVar,
        const FString& TargetVar,
        ESemanticRelation Relation,
        const FString& PatternID = TEXT(""));

    /** Create a triangle pattern (A->B, B->C, A->C) */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Builders")
    static FGraphPattern CreateTrianglePattern(
        const FString& VarA,
        const FString& VarB,
        const FString& VarC,
        ESemanticRelation RelationAB,
        ESemanticRelation RelationBC,
        ESemanticRelation RelationAC);

    /** Create a path pattern (A->B->C->...->N) */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Builders")
    static FGraphPattern CreatePathPattern(
        const TArray<FString>& VariableNames,
        const TArray<ESemanticRelation>& Relations);

    // ========================================
    // BINDING OPERATIONS
    // ========================================

    /** Extend binding environment with new binding */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Bindings")
    static FBindingEnvironment ExtendBinding(
        const FBindingEnvironment& Env,
        const FString& VariableName,
        int64 NodeID);

    /** Merge two binding environments */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Bindings")
    static FBindingEnvironment MergeBindings(
        const FBindingEnvironment& Env1,
        const FBindingEnvironment& Env2);

    /** Check if bindings are compatible (no conflicts) */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Bindings")
    static bool BindingsAreCompatible(
        const FBindingEnvironment& Env1,
        const FBindingEnvironment& Env2);

    /** Apply bindings to a pattern element */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Bindings")
    int64 ResolvePatternElement(
        const FPatternElement& Element,
        const FBindingEnvironment& Env) const;

    // ========================================
    // INDEX MANAGEMENT
    // ========================================

    /** Rebuild all indexes */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Index")
    void RebuildIndexes();

    /** Update indexes incrementally */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Index")
    void UpdateIndexes();

    /** Clear all indexes */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Index")
    void ClearIndexes();

    /** Get index statistics */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Index")
    FString GetIndexStats() const;

    // ========================================
    // HYPERGRAPH REFERENCE
    // ========================================

    /** Set the hypergraph to match against */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Setup")
    void SetHypergraph(UHypergraphMemorySystem* Hypergraph);

    /** Get the current hypergraph */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Setup")
    UHypergraphMemorySystem* GetHypergraph() const;

    /** Auto-discover hypergraph from owner */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Setup")
    bool AutoDiscoverHypergraph();

    // ========================================
    // DIAGNOSTICS
    // ========================================

    /** Get pattern matching statistics */
    UFUNCTION(BlueprintPure, Category = "PatternMatcher|Diagnostics")
    FString GetMatchingStats() const;

    /** Reset statistics */
    UFUNCTION(BlueprintCallable, Category = "PatternMatcher|Diagnostics")
    void ResetStats();

protected:
    // ========================================
    // INTERNAL MATCHING ENGINE
    // ========================================

    /** Core matching algorithm with backtracking */
    TArray<FBindingEnvironment> MatchPatternInternal(
        const FGraphPattern& Pattern,
        const FPatternMatchConfig& Config);

    /** Match a single edge pattern */
    TArray<FBindingEnvironment> MatchEdgePattern(
        const FEdgePattern& EdgePattern,
        const FBindingEnvironment& CurrentBindings,
        const FPatternMatchConfig& Config);

    /** Match a single hyperedge pattern */
    TArray<FBindingEnvironment> MatchHyperedgePattern(
        const FHyperedgePattern& HyperedgePattern,
        const FBindingEnvironment& CurrentBindings,
        const FPatternMatchConfig& Config);

    /** Match a standalone node pattern */
    TArray<FBindingEnvironment> MatchNodePattern(
        const FPatternElement& NodePattern,
        const FBindingEnvironment& CurrentBindings,
        const FPatternMatchConfig& Config);

    /** Check if node satisfies pattern element constraints */
    bool NodeSatisfiesElement(
        const FMemoryNode& Node,
        const FPatternElement& Element) const;

    /** Check if edge satisfies edge pattern constraints */
    bool EdgeSatisfiesPattern(
        const FMemoryEdge& Edge,
        const FEdgePattern& Pattern) const;

    /** Check property constraint */
    bool CheckPropertyConstraint(
        const FMemoryNode& Node,
        const FPropertyConstraint& Constraint) const;

    /** Evaluate cross-variable constraint */
    bool EvaluateCrossConstraint(
        const FString& Constraint,
        const FBindingEnvironment& Env) const;

    /** Get candidate nodes for a pattern element */
    TArray<int64> GetCandidateNodes(
        const FPatternElement& Element,
        const FBindingEnvironment& CurrentBindings) const;

    /** Get candidate edges for an edge pattern */
    TArray<int64> GetCandidateEdges(
        const FEdgePattern& Pattern,
        const FBindingEnvironment& CurrentBindings) const;

    /** Compute transitive closure for edge pattern */
    TArray<FBindingEnvironment> ComputeTransitiveClosure(
        const FEdgePattern& EdgePattern,
        const FBindingEnvironment& CurrentBindings,
        const FPatternMatchConfig& Config);

    /** Order pattern clauses for efficient matching */
    void OptimizePatternOrder(FGraphPattern& Pattern);

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Reference to hypergraph memory system */
    UPROPERTY()
    UHypergraphMemorySystem* HypergraphRef;

    /** Registered patterns */
    UPROPERTY()
    TMap<FString, FGraphPattern> RegisteredPatterns;

    /** Pattern index for acceleration */
    FPatternIndexEntry PatternIndex;

    /** Index update timer */
    float IndexUpdateTimer = 0.0f;

    /** Statistics */
    int64 TotalMatchAttempts = 0;
    int64 TotalMatchesFound = 0;
    double TotalMatchTimeMs = 0.0;
    int32 CacheHits = 0;
    int32 CacheMisses = 0;
};
