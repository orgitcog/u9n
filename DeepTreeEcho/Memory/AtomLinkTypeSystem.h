// AtomLinkTypeSystem.h
// Feature F1.4.1: Atom/Link Type System for Deep Tree Echo
// Implements typed atoms and links with inheritance hierarchy for knowledge representation
// Copyright (c) 2025-2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AtomLinkTypeSystem.generated.h"

// ============================================================================
// ATOM TYPE DEFINITIONS
// ============================================================================

/**
 * Base atom type enumeration
 * Follows OpenCog AtomSpace type conventions with Deep Tree Echo extensions
 */
UENUM(BlueprintType)
enum class EAtomTypeID : uint8
{
    // === Base Types (Root of hierarchy) ===
    /** Abstract base type - all atoms inherit from this */
    Atom                    UMETA(DisplayName = "Atom"),

    // === Node Types (Terminals) ===
    /** Abstract node type - all nodes inherit from this */
    Node                    UMETA(DisplayName = "Node"),
    /** Concept node - represents abstract concepts */
    ConceptNode             UMETA(DisplayName = "ConceptNode"),
    /** Predicate node - represents relations/properties */
    PredicateNode           UMETA(DisplayName = "PredicateNode"),
    /** Number node - represents numeric values */
    NumberNode              UMETA(DisplayName = "NumberNode"),
    /** Type node - represents types (meta-level) */
    TypeNode                UMETA(DisplayName = "TypeNode"),
    /** Variable node - represents unbound variables for pattern matching */
    VariableNode            UMETA(DisplayName = "VariableNode"),
    /** Anchor node - represents grounding points to external systems */
    AnchorNode              UMETA(DisplayName = "AnchorNode"),
    /** Time node - represents temporal points */
    TimeNode                UMETA(DisplayName = "TimeNode"),
    /** Schema node - represents procedural schemas */
    SchemaNode              UMETA(DisplayName = "SchemaNode"),
    /** Grounded schema node - executable procedure */
    GroundedSchemaNode      UMETA(DisplayName = "GroundedSchemaNode"),
    /** Grounded predicate node - executable predicate */
    GroundedPredicateNode   UMETA(DisplayName = "GroundedPredicateNode"),

    // === Link Types (Connectors) ===
    /** Abstract link type - all links inherit from this */
    Link                    UMETA(DisplayName = "Link"),
    /** Unordered link - base for unordered collections */
    UnorderedLink           UMETA(DisplayName = "UnorderedLink"),
    /** Ordered link - base for ordered collections */
    OrderedLink             UMETA(DisplayName = "OrderedLink"),
    /** List link - ordered list of atoms */
    ListLink                UMETA(DisplayName = "ListLink"),
    /** Set link - unordered set of atoms */
    SetLink                 UMETA(DisplayName = "SetLink"),

    // === Logical Links ===
    /** And link - logical conjunction */
    AndLink                 UMETA(DisplayName = "AndLink"),
    /** Or link - logical disjunction */
    OrLink                  UMETA(DisplayName = "OrLink"),
    /** Not link - logical negation */
    NotLink                 UMETA(DisplayName = "NotLink"),

    // === Inheritance Links ===
    /** Inheritance link - IS-A relation (subset/subclass) */
    InheritanceLink         UMETA(DisplayName = "InheritanceLink"),
    /** Similarity link - bidirectional similarity */
    SimilarityLink          UMETA(DisplayName = "SimilarityLink"),

    // === Evaluation Links ===
    /** Evaluation link - predicate application */
    EvaluationLink          UMETA(DisplayName = "EvaluationLink"),
    /** Execution link - schema execution */
    ExecutionLink           UMETA(DisplayName = "ExecutionLink"),

    // === Member Links ===
    /** Member link - set membership */
    MemberLink              UMETA(DisplayName = "MemberLink"),

    // === Context Links ===
    /** Context link - contextual wrapper */
    ContextLink             UMETA(DisplayName = "ContextLink"),

    // === Implication Links ===
    /** Implication link - logical implication */
    ImplicationLink         UMETA(DisplayName = "ImplicationLink"),
    /** Equivalence link - logical equivalence */
    EquivalenceLink         UMETA(DisplayName = "EquivalenceLink"),

    // === State Links ===
    /** State link - mutable state holder */
    StateLink               UMETA(DisplayName = "StateLink"),
    /** Define link - definition binding */
    DefineLink              UMETA(DisplayName = "DefineLink"),

    // === Temporal Links ===
    /** AtTime link - temporal annotation */
    AtTimeLink              UMETA(DisplayName = "AtTimeLink"),
    /** Before link - temporal ordering */
    BeforeLink              UMETA(DisplayName = "BeforeLink"),
    /** During link - temporal containment */
    DuringLink              UMETA(DisplayName = "DuringLink"),
    /** Sequential link - sequential composition */
    SequentialLink          UMETA(DisplayName = "SequentialLink"),

    // === Deep Tree Echo Specific Types ===
    /** Echo resonance link - memory resonance */
    EchoResonanceLink       UMETA(DisplayName = "EchoResonanceLink"),
    /** Cognitive state node - cognitive state marker */
    CognitiveStateNode      UMETA(DisplayName = "CognitiveStateNode"),
    /** Embodied schema node - 4E embodied action */
    EmbodiedSchemaNode      UMETA(DisplayName = "EmbodiedSchemaNode"),
    /** Affordance link - environmental affordance */
    AffordanceLink          UMETA(DisplayName = "AffordanceLink"),
    /** Relevance link - relevance realization */
    RelevanceLink           UMETA(DisplayName = "RelevanceLink"),
    /** Gestalt link - holistic pattern */
    GestaltLink             UMETA(DisplayName = "GestaltLink"),
    /** Reservoir activation node - ESN state */
    ReservoirActivationNode UMETA(DisplayName = "ReservoirActivationNode"),
    /** Membrane boundary link - P-system membrane */
    MembraneBoundaryLink    UMETA(DisplayName = "MembraneBoundaryLink"),

    // === Memory Types ===
    /** Episodic memory node - experiential memory */
    EpisodicMemoryNode      UMETA(DisplayName = "EpisodicMemoryNode"),
    /** Semantic memory node - factual knowledge */
    SemanticMemoryNode      UMETA(DisplayName = "SemanticMemoryNode"),
    /** Procedural memory node - skill memory */
    ProceduralMemoryNode    UMETA(DisplayName = "ProceduralMemoryNode"),
    /** Intentional memory node - BDI state */
    IntentionalMemoryNode   UMETA(DisplayName = "IntentionalMemoryNode"),

    // === Emotional Types ===
    /** Emotional state node - emotional marker */
    EmotionalStateNode      UMETA(DisplayName = "EmotionalStateNode"),
    /** Somatic marker link - embodied emotion */
    SomaticMarkerLink       UMETA(DisplayName = "SomaticMarkerLink"),

    /** Maximum type ID (for bounds checking) */
    MAX_TYPE                UMETA(Hidden)
};

// ============================================================================
// TYPE HIERARCHY STRUCTURES
// ============================================================================

/**
 * Type inheritance info - describes a type's position in the hierarchy
 */
USTRUCT(BlueprintType)
struct FAtomTypeInfo
{
    GENERATED_BODY()

    /** Type identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    EAtomTypeID TypeID = EAtomTypeID::Atom;

    /** Human-readable type name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    FString TypeName;

    /** Parent type in hierarchy (Atom for root types) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    EAtomTypeID ParentType = EAtomTypeID::Atom;

    /** Whether this is a node type (vs link type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    bool bIsNodeType = true;

    /** Whether this type is abstract (cannot be instantiated directly) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    bool bIsAbstract = false;

    /** Arity for link types (-1 for nodes, 0+ for links) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    int32 Arity = -1;

    /** Whether link is ordered */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    bool bIsOrdered = true;

    /** Description of the type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeInfo")
    FString Description;

    FAtomTypeInfo() = default;

    FAtomTypeInfo(EAtomTypeID InTypeID, const FString& InTypeName, EAtomTypeID InParentType,
                  bool InIsNodeType, bool InIsAbstract, int32 InArity, bool InIsOrdered,
                  const FString& InDescription)
        : TypeID(InTypeID)
        , TypeName(InTypeName)
        , ParentType(InParentType)
        , bIsNodeType(InIsNodeType)
        , bIsAbstract(InIsAbstract)
        , Arity(InArity)
        , bIsOrdered(InIsOrdered)
        , Description(InDescription)
    {}
};

/**
 * Truth value for atoms - probabilistic truth
 */
USTRUCT(BlueprintType)
struct FAtomTruthValue
{
    GENERATED_BODY()

    /** Strength: probability of truth (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Strength = 1.0f;

    /** Confidence: certainty of the strength (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.9f;

    /** Count: number of observations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    int32 Count = 1;

    FAtomTruthValue() = default;

    FAtomTruthValue(float InStrength, float InConfidence, int32 InCount = 1)
        : Strength(InStrength)
        , Confidence(InConfidence)
        , Count(InCount)
    {}

    /** Merge two truth values using revision */
    FAtomTruthValue Revise(const FAtomTruthValue& Other) const;

    /** Calculate simple truth value (strength * confidence) */
    float GetSimpleTruth() const { return Strength * Confidence; }
};

/**
 * Attention value for atoms - importance/salience
 */
USTRUCT(BlueprintType)
struct FAtomAttentionValue
{
    GENERATED_BODY()

    /** Short-Term Importance (-1.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttentionValue", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float STI = 0.0f;

    /** Long-Term Importance (-1.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttentionValue", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float LTI = 0.0f;

    /** Very Long-Term Importance - persistence (-1.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttentionValue", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float VLTI = 0.0f;

    FAtomAttentionValue() = default;

    FAtomAttentionValue(float InSTI, float InLTI, float InVLTI = 0.0f)
        : STI(InSTI)
        , LTI(InLTI)
        , VLTI(InVLTI)
    {}

    /** Decay attention over time */
    void Decay(float DecayRate);

    /** Boost attention */
    void Boost(float BoostAmount);
};

/**
 * Typed atom handle - unique identifier with type info
 */
USTRUCT(BlueprintType)
struct FTypedAtomHandle
{
    GENERATED_BODY()

    /** Unique handle ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handle")
    int64 Handle = 0;

    /** Atom type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handle")
    EAtomTypeID TypeID = EAtomTypeID::Atom;

    /** Whether the handle is valid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handle")
    bool bIsValid = false;

    FTypedAtomHandle() = default;

    FTypedAtomHandle(int64 InHandle, EAtomTypeID InTypeID)
        : Handle(InHandle)
        , TypeID(InTypeID)
        , bIsValid(true)
    {}

    bool operator==(const FTypedAtomHandle& Other) const { return Handle == Other.Handle; }
    bool operator!=(const FTypedAtomHandle& Other) const { return Handle != Other.Handle; }
    bool operator<(const FTypedAtomHandle& Other) const { return Handle < Other.Handle; }
};

/**
 * Hash function for FTypedAtomHandle
 */
FORCEINLINE uint32 GetTypeHash(const FTypedAtomHandle& Handle)
{
    return GetTypeHash(Handle.Handle);
}

/**
 * Typed atom - complete atom with type, values, and outgoing set
 */
USTRUCT(BlueprintType)
struct FTypedAtom
{
    GENERATED_BODY()

    /** Atom handle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FTypedAtomHandle Handle;

    /** Atom type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    EAtomTypeID TypeID = EAtomTypeID::ConceptNode;

    /** Atom name (for nodes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FString Name;

    /** Truth value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FAtomTruthValue TruthValue;

    /** Attention value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FAtomAttentionValue AttentionValue;

    /** Outgoing set (for links - handles of connected atoms) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    TArray<FTypedAtomHandle> OutgoingSet;

    /** Creation timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FDateTime CreatedAt;

    /** Last modified timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    FDateTime ModifiedAt;

    /** Custom properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atom")
    TMap<FString, FString> Properties;

    FTypedAtom()
        : CreatedAt(FDateTime::Now())
        , ModifiedAt(FDateTime::Now())
    {}

    /** Check if this is a node (has no outgoing) */
    bool IsNode() const { return OutgoingSet.Num() == 0; }

    /** Check if this is a link (has outgoing) */
    bool IsLink() const { return OutgoingSet.Num() > 0; }

    /** Get arity (number of outgoing atoms) */
    int32 GetArity() const { return OutgoingSet.Num(); }
};

/**
 * Type query result - result of type hierarchy queries
 */
USTRUCT(BlueprintType)
struct FTypeQueryResult
{
    GENERATED_BODY()

    /** Query was successful */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    bool bSuccess = false;

    /** Types matching the query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    TArray<EAtomTypeID> MatchingTypes;

    /** Error message if query failed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    FString ErrorMessage;
};

/**
 * Type system statistics
 */
USTRUCT(BlueprintType)
struct FTypeSystemStats
{
    GENERATED_BODY()

    /** Total registered types */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalTypes = 0;

    /** Total node types */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 NodeTypes = 0;

    /** Total link types */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 LinkTypes = 0;

    /** Maximum inheritance depth */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MaxInheritanceDepth = 0;

    /** Total atoms created */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 TotalAtomsCreated = 0;

    /** Total type lookups */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 TotalTypeLookups = 0;

    /** Average lookup time (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AverageLookupTime = 0.0f;
};

// ============================================================================
// ATOM/LINK TYPE SYSTEM COMPONENT
// ============================================================================

/**
 * Atom/Link Type System Component
 *
 * Feature F1.4.1: Implements typed atoms and links with inheritance hierarchy
 * for knowledge representation in the Deep Tree Echo cognitive architecture.
 *
 * Key Features:
 * - Complete type hierarchy following OpenCog AtomSpace conventions
 * - Inheritance queries (is-a, subtypes, supertypes)
 * - Type validation for atom creation
 * - Deep Tree Echo specific types (cognitive, embodied, temporal)
 * - Integration with Hypergraph Memory System
 * - Blueprint-accessible API
 *
 * Scientific Foundation:
 * - OpenCog AtomSpace type system (Ben Goertzel et al.)
 * - Knowledge representation with typed hypergraphs
 * - First-order logic type theory
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAtomLinkTypeSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UAtomLinkTypeSystem();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable strict type checking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeSystem|Config")
    bool bEnableStrictTypeChecking = true;

    /** Enable type caching for performance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeSystem|Config")
    bool bEnableTypeCaching = true;

    /** Enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeSystem|Debug")
    bool bEnableDebugLogging = false;

    // ========================================
    // STATE
    // ========================================

    /** System statistics */
    UPROPERTY(BlueprintReadOnly, Category = "TypeSystem|State")
    FTypeSystemStats Stats;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize the type system with all built-in types */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem")
    void Initialize();

    /** Register the standard type hierarchy */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem")
    void RegisterStandardTypes();

    /** Reset the type system */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem")
    void Reset();

    // ========================================
    // TYPE REGISTRATION
    // ========================================

    /**
     * Register a new type in the hierarchy
     * @param TypeID The type identifier
     * @param TypeName Human-readable name
     * @param ParentType Parent type in hierarchy
     * @param bIsNodeType True for nodes, false for links
     * @param bIsAbstract True if type cannot be instantiated
     * @param Arity Expected arity for links (-1 for nodes)
     * @param bIsOrdered True for ordered links
     * @param Description Type description
     */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem|Registration")
    bool RegisterType(
        EAtomTypeID TypeID,
        const FString& TypeName,
        EAtomTypeID ParentType,
        bool bIsNodeType,
        bool bIsAbstract,
        int32 Arity,
        bool bIsOrdered,
        const FString& Description
    );

    /**
     * Check if a type is registered
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Registration")
    bool IsTypeRegistered(EAtomTypeID TypeID) const;

    /**
     * Get type info for a registered type
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Registration")
    FAtomTypeInfo GetTypeInfo(EAtomTypeID TypeID) const;

    /**
     * Get all registered types
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Registration")
    TArray<EAtomTypeID> GetAllRegisteredTypes() const;

    // ========================================
    // TYPE HIERARCHY QUERIES
    // ========================================

    /**
     * Check if TypeA is a subtype of TypeB (TypeA inherits from TypeB)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    bool IsSubtypeOf(EAtomTypeID TypeA, EAtomTypeID TypeB) const;

    /**
     * Check if TypeA is a supertype of TypeB (TypeB inherits from TypeA)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    bool IsSupertypeOf(EAtomTypeID TypeA, EAtomTypeID TypeB) const;

    /**
     * Get the parent type
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    EAtomTypeID GetParentType(EAtomTypeID TypeID) const;

    /**
     * Get all ancestor types (parents, grandparents, etc.)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    TArray<EAtomTypeID> GetAncestorTypes(EAtomTypeID TypeID) const;

    /**
     * Get all direct subtypes (immediate children)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    TArray<EAtomTypeID> GetDirectSubtypes(EAtomTypeID TypeID) const;

    /**
     * Get all descendant types (children, grandchildren, etc.)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    TArray<EAtomTypeID> GetDescendantTypes(EAtomTypeID TypeID) const;

    /**
     * Get the inheritance depth of a type (Atom = 0)
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    int32 GetInheritanceDepth(EAtomTypeID TypeID) const;

    /**
     * Find the most specific common supertype of two types
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Hierarchy")
    EAtomTypeID FindCommonSupertype(EAtomTypeID TypeA, EAtomTypeID TypeB) const;

    // ========================================
    // TYPE CLASSIFICATION
    // ========================================

    /**
     * Check if type is a node type
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    bool IsNodeType(EAtomTypeID TypeID) const;

    /**
     * Check if type is a link type
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    bool IsLinkType(EAtomTypeID TypeID) const;

    /**
     * Check if type is abstract
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    bool IsAbstractType(EAtomTypeID TypeID) const;

    /**
     * Get all node types
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    TArray<EAtomTypeID> GetAllNodeTypes() const;

    /**
     * Get all link types
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    TArray<EAtomTypeID> GetAllLinkTypes() const;

    /**
     * Get the expected arity for a link type
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Classification")
    int32 GetExpectedArity(EAtomTypeID TypeID) const;

    // ========================================
    // ATOM CREATION (with type validation)
    // ========================================

    /**
     * Create a typed node
     */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem|Creation")
    FTypedAtom CreateTypedNode(
        EAtomTypeID TypeID,
        const FString& Name,
        const FAtomTruthValue& TruthValue
    );

    /**
     * Create a typed link
     */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem|Creation")
    FTypedAtom CreateTypedLink(
        EAtomTypeID TypeID,
        const TArray<FTypedAtomHandle>& OutgoingSet,
        const FAtomTruthValue& TruthValue
    );

    /**
     * Validate that an atom conforms to its type constraints
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Validation")
    bool ValidateAtom(const FTypedAtom& Atom, FString& OutError) const;

    /**
     * Validate that a link's outgoing set meets type requirements
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Validation")
    bool ValidateLinkOutgoing(EAtomTypeID LinkType, const TArray<FTypedAtomHandle>& OutgoingSet, FString& OutError) const;

    // ========================================
    // TYPE NAME CONVERSION
    // ========================================

    /**
     * Convert type ID to string name
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Conversion")
    FString TypeIDToString(EAtomTypeID TypeID) const;

    /**
     * Parse type ID from string name
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Conversion")
    EAtomTypeID StringToTypeID(const FString& TypeName) const;

    // ========================================
    // STATISTICS
    // ========================================

    /**
     * Get type system statistics
     */
    UFUNCTION(BlueprintPure, Category = "TypeSystem|Stats")
    FTypeSystemStats GetStats() const;

    /**
     * Generate diagnostic report
     */
    UFUNCTION(BlueprintCallable, Category = "TypeSystem|Stats")
    TArray<FString> GenerateDiagnosticReport() const;

protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Type registry: TypeID -> TypeInfo */
    TMap<EAtomTypeID, FAtomTypeInfo> TypeRegistry;

    /** Name to TypeID mapping (cached for performance) */
    TMap<FString, EAtomTypeID> NameToTypeMap;

    /** Subtype cache: TypeID -> Direct subtypes */
    TMap<EAtomTypeID, TArray<EAtomTypeID>> SubtypeCache;

    /** Ancestor cache: TypeID -> All ancestors */
    TMap<EAtomTypeID, TArray<EAtomTypeID>> AncestorCache;

    /** Next handle ID for atom creation */
    int64 NextHandleID = 1;

    /** System initialized flag */
    bool bIsInitialized = false;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Build type caches after registration */
    void BuildTypeCaches();

    /** Rebuild subtype cache for a type */
    void RebuildSubtypeCache(EAtomTypeID TypeID);

    /** Calculate inheritance depth recursively */
    int32 CalculateDepth(EAtomTypeID TypeID) const;

    /** Generate unique handle ID */
    int64 GenerateHandleID();

    /** Update statistics */
    void UpdateStats();
};
