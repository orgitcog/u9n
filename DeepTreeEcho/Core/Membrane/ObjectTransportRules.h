#pragma once

/**
 * @file ObjectTransportRules.h
 * @brief Feature F1.3.2: Object/Symbol Transport Rules
 *
 * Implements rules for transporting objects and symbols between membrane compartments
 * in P-system membrane computing:
 * - IN: Transport object to a child membrane
 * - OUT: Transport object to the parent membrane
 * - HERE: Transform object within the current membrane
 *
 * Based on:
 * - Păun, G. (2000). "Computing with Membranes"
 * - Păun, G. (2002). "Membrane Computing: An Introduction"
 * - Ciobanu et al. (2006). "Applications of Membrane Computing"
 *
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObjectTransportRules.generated.h"

/**
 * @brief Transport direction enumeration
 * Defines where objects/symbols are transported in the membrane hierarchy
 */
UENUM(BlueprintType)
enum class ETransportDirection : uint8
{
    Here UMETA(DisplayName = "Here", ToolTip = "Transform within current membrane"),
    In UMETA(DisplayName = "In", ToolTip = "Transport to a child membrane"),
    Out UMETA(DisplayName = "Out", ToolTip = "Transport to parent membrane"),
    InAll UMETA(DisplayName = "In All", ToolTip = "Transport to all child membranes"),
    Dissolve UMETA(DisplayName = "Dissolve", ToolTip = "Release to parent and dissolve membrane")
};

/**
 * @brief Rule execution mode
 */
UENUM(BlueprintType)
enum class ERuleExecutionMode : uint8
{
    Deterministic UMETA(DisplayName = "Deterministic", ToolTip = "First matching rule executes"),
    MaximalParallel UMETA(DisplayName = "Maximal Parallel", ToolTip = "All applicable rules execute maximally"),
    Sequential UMETA(DisplayName = "Sequential", ToolTip = "Rules execute in priority order"),
    Probabilistic UMETA(DisplayName = "Probabilistic", ToolTip = "Rules selected probabilistically")
};

/**
 * @brief Symbol representation in the membrane
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneSymbol
{
    GENERATED_BODY()

    /** Symbol identifier/name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol")
    FName SymbolName;

    /** Multiplicity/count of this symbol */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol", meta = (ClampMin = "0"))
    int32 Multiplicity = 1;

    /** Optional payload data for the symbol */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol")
    TMap<FString, FString> Payload;

    /** Symbol creation timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Symbol")
    float CreationTime = 0.0f;

    /** Is this a catalyst (not consumed by rules) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol")
    bool bIsCatalyst = false;

    FMembraneSymbol() = default;
    
    FMembraneSymbol(FName InName, int32 InMultiplicity = 1)
        : SymbolName(InName), Multiplicity(InMultiplicity) {}

    bool operator==(const FMembraneSymbol& Other) const
    {
        return SymbolName == Other.SymbolName;
    }

    bool IsValid() const { return !SymbolName.IsNone() && Multiplicity > 0; }
};

/**
 * @brief Multiset of symbols (region contents)
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FSymbolMultiset
{
    GENERATED_BODY()

    /** Symbols in this multiset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiset")
    TArray<FMembraneSymbol> Symbols;

    /** Add a symbol (or increase multiplicity if exists) */
    void AddSymbol(const FMembraneSymbol& Symbol);

    /** Remove a symbol (or decrease multiplicity) */
    bool RemoveSymbol(const FMembraneSymbol& Symbol);

    /** Check if multiset contains symbol with required multiplicity */
    bool Contains(const FMembraneSymbol& Symbol) const;

    /** Check if multiset contains all symbols in pattern */
    bool ContainsAll(const FSymbolMultiset& Pattern) const;

    /** Get total count of all symbols */
    int32 GetTotalCount() const;

    /** Get count of specific symbol */
    int32 GetSymbolCount(FName SymbolName) const;

    /** Clear all symbols */
    void Clear() { Symbols.Empty(); }

    /** Check if empty */
    bool IsEmpty() const { return Symbols.Num() == 0 || GetTotalCount() == 0; }
};

/**
 * @brief Transport rule target specification
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FTransportTarget
{
    GENERATED_BODY()

    /** Direction of transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
    ETransportDirection Direction = ETransportDirection::Here;

    /** Specific target membrane ID (empty = any valid target) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
    FName TargetMembraneId;

    /** Target selection by label (for In direction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
    FName TargetLabel;

    FTransportTarget() = default;

    FTransportTarget(ETransportDirection InDirection, FName InTarget = NAME_None)
        : Direction(InDirection), TargetMembraneId(InTarget) {}
};

/**
 * @brief A single transport rule definition
 * Format: u -> v, target
 * Where u is consumed symbols, v is produced symbols, target is transport direction
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FTransportRule
{
    GENERATED_BODY()

    /** Unique rule identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FName RuleId;

    /** Left-hand side: symbols consumed by this rule */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FSymbolMultiset LeftHandSide;

    /** Right-hand side: symbols produced by this rule */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FSymbolMultiset RightHandSide;

    /** Transport target for produced symbols */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FTransportTarget Target;

    /** Rule priority (higher = executes first) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (ClampMin = "0"))
    int32 Priority = 0;

    /** Probability weight for probabilistic execution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ProbabilityWeight = 1.0f;

    /** Is this rule enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    bool bEnabled = true;

    /** Catalyst symbols required but not consumed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FSymbolMultiset Catalysts;

    /** Inhibitor symbols that prevent rule execution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FSymbolMultiset Inhibitors;

    /** Optional promoter symbols that enable the rule */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FSymbolMultiset Promoters;

    /** Rule description for debugging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FString Description;

    FTransportRule() = default;

    /** Check if rule is valid (has LHS and target) */
    bool IsValid() const;

    /** Get rule as string representation */
    FString ToString() const;
};

/**
 * @brief Membrane compartment representation
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneCompartment
{
    GENERATED_BODY()

    /** Unique membrane identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    FName MembraneId;

    /** Membrane label (for rule targeting) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    FName Label;

    /** Parent membrane ID (empty for skin membrane) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    FName ParentId;

    /** Child membrane IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    TArray<FName> ChildIds;

    /** Symbols contained in this membrane */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    FSymbolMultiset Contents;

    /** Rules associated with this membrane */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    TArray<FTransportRule> Rules;

    /** Is this membrane active/alive */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    bool bIsActive = true;

    /** Permeability flags for different symbol types */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane")
    TMap<FName, bool> Permeability;

    /** Membrane creation time */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane")
    float CreationTime = 0.0f;

    FMembraneCompartment() = default;

    FMembraneCompartment(FName InId, FName InLabel = NAME_None)
        : MembraneId(InId), Label(InLabel) {}

    /** Check if this is the skin (outermost) membrane */
    bool IsSkin() const { return ParentId.IsNone(); }

    /** Check if membrane has children */
    bool HasChildren() const { return ChildIds.Num() > 0; }
};

/**
 * @brief Result of a rule application
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FRuleApplicationResult
{
    GENERATED_BODY()

    /** Was the rule successfully applied */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bSuccess = false;

    /** Rule that was applied */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FName RuleId;

    /** Source membrane */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FName SourceMembraneId;

    /** Target membrane (for In/Out directions) */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FName TargetMembraneId;

    /** Symbols consumed */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FSymbolMultiset ConsumedSymbols;

    /** Symbols produced */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FSymbolMultiset ProducedSymbols;

    /** Error message if failed */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FString ErrorMessage;

    /** Execution timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float Timestamp = 0.0f;
};

/**
 * @brief Transport rule execution statistics
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FTransportStatistics
{
    GENERATED_BODY()

    /** Total rules evaluated */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 TotalRulesEvaluated = 0;

    /** Rules successfully applied */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 RulesApplied = 0;

    /** Rules blocked by missing symbols */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 RulesBlockedBySymbols = 0;

    /** Rules blocked by inhibitors */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 RulesBlockedByInhibitors = 0;

    /** Total symbols transported */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 TotalSymbolsTransported = 0;

    /** IN transports count */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 InTransports = 0;

    /** OUT transports count */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 OutTransports = 0;

    /** HERE transforms count */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 HereTransforms = 0;

    /** Computation steps executed */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    int32 ComputationSteps = 0;

    /** Last step time in seconds */
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    float LastStepTime = 0.0f;

    void Reset()
    {
        TotalRulesEvaluated = 0;
        RulesApplied = 0;
        RulesBlockedBySymbols = 0;
        RulesBlockedByInhibitors = 0;
        TotalSymbolsTransported = 0;
        InTransports = 0;
        OutTransports = 0;
        HereTransforms = 0;
        ComputationSteps = 0;
        LastStepTime = 0.0f;
    }
};

/**
 * @brief Object Transport Rules Component
 * 
 * Manages P-system membrane computing with transport rules for symbols:
 * - IN: Move symbols into child membranes
 * - OUT: Move symbols to parent membrane
 * - HERE: Transform symbols within current membrane
 * 
 * Supports:
 * - Hierarchical membrane structures
 * - Catalyst and inhibitor rules
 * - Multiple execution modes
 * - Rule priority and probabilistic selection
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class DEEPTREEECHO_API UObjectTransportRules : public UActorComponent
{
    GENERATED_BODY()

public:
    UObjectTransportRules();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    // ==================== Initialization ====================

    /**
     * @brief Initialize the transport system with a skin membrane
     * @param SkinLabel Label for the outermost membrane
     * @return True if initialization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool Initialize(FName SkinLabel = TEXT("Skin"));

    /**
     * @brief Reset the entire membrane system
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    void Reset();

    // ==================== Membrane Management ====================

    /**
     * @brief Create a new membrane compartment
     * @param ParentId Parent membrane ID (empty for skin)
     * @param Label Membrane label for rule targeting
     * @return ID of created membrane, NAME_None if failed
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FName CreateMembrane(FName ParentId, FName Label = NAME_None);

    /**
     * @brief Remove a membrane and handle its contents
     * @param MembraneId Membrane to remove
     * @param bReleaseContents If true, release contents to parent
     * @return True if successfully removed
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool RemoveMembrane(FName MembraneId, bool bReleaseContents = true);

    /**
     * @brief Get a membrane by ID
     * @param MembraneId Membrane identifier
     * @return Pointer to membrane, nullptr if not found
     */
    const FMembraneCompartment* GetMembrane(FName MembraneId) const;

    /**
     * @brief Get all membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    TArray<FName> GetAllMembraneIds() const;

    /**
     * @brief Get child membranes of a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    TArray<FName> GetChildMembranes(FName MembraneId) const;

    /**
     * @brief Get the skin (outermost) membrane ID
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FName GetSkinMembraneId() const { return SkinMembraneId; }

    // ==================== Symbol Management ====================

    /**
     * @brief Add symbols to a membrane
     * @param MembraneId Target membrane
     * @param Symbol Symbol to add
     * @return True if added successfully
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool AddSymbol(FName MembraneId, const FMembraneSymbol& Symbol);

    /**
     * @brief Add symbols from a multiset to a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool AddSymbols(FName MembraneId, const FSymbolMultiset& Symbols);

    /**
     * @brief Remove a symbol from a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool RemoveSymbol(FName MembraneId, const FMembraneSymbol& Symbol);

    /**
     * @brief Get symbols in a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FSymbolMultiset GetSymbols(FName MembraneId) const;

    /**
     * @brief Check if membrane contains specific symbols
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool MembraneContains(FName MembraneId, const FSymbolMultiset& Symbols) const;

    // ==================== Rule Management ====================

    /**
     * @brief Add a transport rule to a membrane
     * @param MembraneId Target membrane
     * @param Rule Rule to add
     * @return True if added successfully
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool AddRule(FName MembraneId, const FTransportRule& Rule);

    /**
     * @brief Remove a rule from a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool RemoveRule(FName MembraneId, FName RuleId);

    /**
     * @brief Get all rules in a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    TArray<FTransportRule> GetRules(FName MembraneId) const;

    /**
     * @brief Create a simple transport rule
     * @param RuleId Unique rule identifier
     * @param LeftSymbol Symbol to consume (simple form)
     * @param RightSymbol Symbol to produce
     * @param Direction Transport direction
     * @param Priority Rule priority
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FTransportRule CreateSimpleRule(
        FName RuleId,
        FName LeftSymbol,
        FName RightSymbol,
        ETransportDirection Direction = ETransportDirection::Here,
        int32 Priority = 0);

    // ==================== Rule Execution ====================

    /**
     * @brief Execute one computation step
     * Applies all applicable rules according to execution mode
     * @return Number of rules applied
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    int32 ExecuteStep();

    /**
     * @brief Execute multiple computation steps
     * @param NumSteps Maximum steps to execute
     * @return Actual number of steps executed
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    int32 ExecuteSteps(int32 NumSteps);

    /**
     * @brief Check if a rule can be applied in a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool CanApplyRule(FName MembraneId, const FTransportRule& Rule) const;

    /**
     * @brief Get all applicable rules in a membrane
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    TArray<FTransportRule> GetApplicableRules(FName MembraneId) const;

    /**
     * @brief Apply a specific rule in a membrane
     * @param MembraneId Source membrane
     * @param RuleId Rule to apply
     * @return Application result
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FRuleApplicationResult ApplyRule(FName MembraneId, FName RuleId);

    // ==================== Transport Operations ====================

    /**
     * @brief Transport symbols using IN direction
     * @param SourceMembraneId Source membrane
     * @param Symbols Symbols to transport
     * @param TargetChildId Specific child membrane (empty = first valid child)
     * @return True if transport succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool TransportIn(FName SourceMembraneId, const FSymbolMultiset& Symbols, FName TargetChildId = NAME_None);

    /**
     * @brief Transport symbols using OUT direction
     * @param SourceMembraneId Source membrane
     * @param Symbols Symbols to transport
     * @return True if transport succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool TransportOut(FName SourceMembraneId, const FSymbolMultiset& Symbols);

    /**
     * @brief Transform symbols using HERE direction (within same membrane)
     * @param MembraneId Target membrane
     * @param ConsumedSymbols Symbols to consume
     * @param ProducedSymbols Symbols to produce
     * @return True if transform succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool TransformHere(FName MembraneId, const FSymbolMultiset& ConsumedSymbols, const FSymbolMultiset& ProducedSymbols);

    // ==================== Configuration ====================

    /** Rule execution mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    ERuleExecutionMode ExecutionMode = ERuleExecutionMode::MaximalParallel;

    /** Enable automatic stepping in tick */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bAutoStep = false;

    /** Steps per tick when auto-stepping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "1", ClampMax = "100"))
    int32 StepsPerTick = 1;

    /** Maximum depth for membrane hierarchy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxHierarchyDepth = 10;

    /** Random seed for probabilistic execution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 RandomSeed = 0;

    /** Enable detailed logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableLogging = false;

    // ==================== Statistics ====================

    /**
     * @brief Get transport statistics
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FTransportStatistics GetStatistics() const { return Statistics; }

    /**
     * @brief Reset statistics
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    void ResetStatistics() { Statistics.Reset(); }

    // ==================== Debugging ====================

    /**
     * @brief Get string representation of membrane hierarchy
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FString GetHierarchyString() const;

    /**
     * @brief Get membrane info as string
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    FString GetMembraneInfo(FName MembraneId) const;

    /**
     * @brief Validate membrane system integrity
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    bool ValidateSystem() const;

    // ==================== Events ====================

    /** Called when a rule is applied */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRuleApplied, const FRuleApplicationResult&, Result);
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Membrane|Events")
    FOnRuleApplied OnRuleApplied;

    /** Called when a membrane is created */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMembraneCreated, FName, MembraneId);
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Membrane|Events")
    FOnMembraneCreated OnMembraneCreated;

    /** Called when a membrane is dissolved */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMembraneDissolved, FName, MembraneId);
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Membrane|Events")
    FOnMembraneDissolved OnMembraneDissolved;

    /** Called when system reaches halting state */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSystemHalted);
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Membrane|Events")
    FOnSystemHalted OnSystemHalted;

protected:
    /** All membrane compartments */
    UPROPERTY()
    TMap<FName, FMembraneCompartment> Membranes;

    /** Skin (outermost) membrane ID */
    UPROPERTY()
    FName SkinMembraneId;

    /** Is system initialized */
    UPROPERTY()
    bool bIsInitialized = false;

    /** Transport statistics */
    UPROPERTY()
    FTransportStatistics Statistics;

    /** Random stream for probabilistic rules */
    FRandomStream RandomStream;

    /** Membrane ID counter for unique IDs */
    int32 MembraneIdCounter = 0;

    /** Current system time */
    float CurrentTime = 0.0f;

    // Internal methods

    /** Generate unique membrane ID */
    FName GenerateMembraneId();

    /** Execute rules for a single membrane */
    int32 ExecuteMembraneStep(FName MembraneId);

    /** Execute rules in maximal parallel mode */
    int32 ExecuteMaximalParallel(FName MembraneId);

    /** Execute rules in sequential mode */
    int32 ExecuteSequential(FName MembraneId);

    /** Execute rules in probabilistic mode */
    int32 ExecuteProbabilistic(FName MembraneId);

    /** Find target membrane for transport */
    FName FindTransportTarget(FName SourceId, const FTransportTarget& Target) const;

    /** Check rule applicability conditions */
    bool CheckRuleConditions(const FMembraneCompartment& Membrane, const FTransportRule& Rule) const;

    /** Apply a single rule internal */
    FRuleApplicationResult ApplyRuleInternal(FMembraneCompartment& Membrane, const FTransportRule& Rule);

    /** Get depth of a membrane in hierarchy */
    int32 GetMembraneDepth(FName MembraneId) const;

    /** Log message if logging enabled */
    void LogMessage(const FString& Message) const;
};
