#pragma once

/**
 * @file MembraneHierarchyManager.h
 * @brief Feature F1.3.1: Membrane Hierarchy Manager
 * 
 * Implements nested P-System membrane structure management with:
 * - Parent-child relationship mapping between membranes
 * - Scope isolation for contained objects
 * - Automatic membrane creation for specified actor classes
 * - Membrane hierarchy visualization support
 * 
 * Based on P-System (Membrane Computing) theory:
 * - Păun, G. (2000): "Computing with Membranes"
 * - Păun, G. (2002): "Membrane Computing: An Introduction"
 * 
 * Integration with Deep Tree Echo:
 * - Provides nested execution contexts (OEIS A000081)
 * - Supports hierarchical cognitive processing
 * - Enables scope-isolated symbol evolution
 * - Compatible with 12-step cognitive cycle
 * 
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MembraneHierarchyManager.generated.h"

// Forward declarations
class AActor;

/**
 * @brief Membrane Type - Classification of membrane function
 */
UENUM(BlueprintType)
enum class EMembraneType : uint8
{
    /** Root/Skin membrane - outermost boundary */
    Root        UMETA(DisplayName = "Root Membrane"),
    
    /** Elementary membrane - contains no sub-membranes */
    Elementary  UMETA(DisplayName = "Elementary Membrane"),
    
    /** Composite membrane - contains sub-membranes */
    Composite   UMETA(DisplayName = "Composite Membrane"),
    
    /** Catalytic membrane - facilitates reactions without consumption */
    Catalytic   UMETA(DisplayName = "Catalytic Membrane")
};

/**
 * @brief Membrane State - Current operational state
 */
UENUM(BlueprintType)
enum class EMembraneState : uint8
{
    /** Active - processing symbols and rules */
    Active      UMETA(DisplayName = "Active"),
    
    /** Dormant - temporarily suspended */
    Dormant     UMETA(DisplayName = "Dormant"),
    
    /** Dissolving - in process of dissolution */
    Dissolving  UMETA(DisplayName = "Dissolving"),
    
    /** Dividing - in process of division */
    Dividing    UMETA(DisplayName = "Dividing")
};

/**
 * @brief Permeability Type - How objects pass through membrane
 */
UENUM(BlueprintType)
enum class EPermeabilityType : uint8
{
    /** Permeable - allows objects to pass freely */
    Permeable       UMETA(DisplayName = "Permeable"),
    
    /** Selective - allows only certain object types */
    Selective       UMETA(DisplayName = "Selective"),
    
    /** Semipermeable - allows in one direction only */
    Semipermeable   UMETA(DisplayName = "Semipermeable"),
    
    /** Impermeable - blocks all object transfer */
    Impermeable     UMETA(DisplayName = "Impermeable")
};

/**
 * @brief Contained Object - Symbol or object within a membrane
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneObject
{
    GENERATED_BODY()

    /** Unique object ID */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    FString ObjectID;

    /** Object symbol/type name */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    FString Symbol;

    /** Multiplicity (count of identical objects) */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    int32 Multiplicity = 1;

    /** Is this object a catalyst */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    bool bIsCatalyst = false;

    /** Associated data payload */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    TMap<FString, FString> Properties;

    /** Numeric value (for computational objects) */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    float NumericValue = 0.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Object")
    float CreationTime = 0.0f;
};

/**
 * @brief Membrane Permeability Rules
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FPermeabilityRules
{
    GENERATED_BODY()

    /** Base permeability type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    EPermeabilityType PermeabilityType = EPermeabilityType::Selective;

    /** Allowed symbols (for Selective permeability) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    TArray<FString> AllowedSymbols;

    /** Blocked symbols */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    TArray<FString> BlockedSymbols;

    /** Allow inward passage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    bool bAllowInward = true;

    /** Allow outward passage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    bool bAllowOutward = true;

    /** Energy cost for passage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Permeability")
    float PassageEnergyCost = 0.0f;
};

/**
 * @brief Membrane Configuration
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneConfig
{
    GENERATED_BODY()

    /** Membrane type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    EMembraneType MembraneType = EMembraneType::Elementary;

    /** Maximum nesting depth (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config", meta = (ClampMin = "0", ClampMax = "100"))
    int32 MaxNestingDepth = 10;

    /** Maximum child membranes (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config", meta = (ClampMin = "0"))
    int32 MaxChildMembranes = 100;

    /** Maximum objects per membrane (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config", meta = (ClampMin = "0"))
    int32 MaxObjectsPerMembrane = 10000;

    /** Permeability rules */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    FPermeabilityRules PermeabilityRules;

    /** Enable dissolution on empty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    bool bDissolveWhenEmpty = false;

    /** Priority level (higher = processed first) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    int32 Priority = 0;

    /** Associated actor classes for automatic membrane creation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    TArray<TSubclassOf<AActor>> AssociatedActorClasses;
};

/**
 * @brief Membrane State Data
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneStateData
{
    GENERATED_BODY()

    /** Unique membrane ID */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    FString MembraneID;

    /** Human-readable label */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|State")
    FString Label;

    /** Current operational state */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    EMembraneState State = EMembraneState::Active;

    /** Membrane type */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    EMembraneType Type = EMembraneType::Elementary;

    /** Parent membrane ID (empty for root) */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    FString ParentID;

    /** Child membrane IDs */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    TArray<FString> ChildIDs;

    /** Nesting depth (0 = root) */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    int32 NestingDepth = 0;

    /** Contained objects */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    TArray<FMembraneObject> Objects;

    /** Associated actor (if applicable) */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    TWeakObjectPtr<AActor> AssociatedActor;

    /** Energy level (for metabolic processes) */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|State")
    float EnergyLevel = 100.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    float CreationTime = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    float LastUpdateTime = 0.0f;

    /** Total object count (including multiplicities) */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|State")
    int32 TotalObjectCount = 0;
};

/**
 * @brief Hierarchy Statistics
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneHierarchyStats
{
    GENERATED_BODY()

    /** Total membrane count */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    int32 TotalMembraneCount = 0;

    /** Maximum nesting depth achieved */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    int32 MaxNestingDepthAchieved = 0;

    /** Total object count across all membranes */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    int32 TotalObjectCount = 0;

    /** Active membrane count */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    int32 ActiveMembraneCount = 0;

    /** Average objects per membrane */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    float AverageObjectsPerMembrane = 0.0f;

    /** Average children per composite membrane */
    UPROPERTY(BlueprintReadOnly, Category = "Membrane|Stats")
    float AverageChildrenPerMembrane = 0.0f;
};

/**
 * @brief Communication Message between membranes
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FMembraneMessage
{
    GENERATED_BODY()

    /** Source membrane ID */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    FString SourceID;

    /** Target membrane ID (empty for parent) */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    FString TargetID;

    /** Objects being transferred */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    TArray<FMembraneObject> Objects;

    /** Direction: true = outward (to parent), false = inward (to child) */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    bool bOutward = true;

    /** Message priority */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    int32 Priority = 0;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite, Category = "Membrane|Message")
    float Timestamp = 0.0f;
};

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMembraneCreated, const FString&, MembraneID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMembraneDestroyed, const FString&, MembraneID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectTransferred, const FString&, SourceID, const FString&, TargetID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMembraneStateChanged, const FString&, MembraneID, EMembraneState, NewState);

/**
 * @brief Membrane Hierarchy Manager Component
 * 
 * Manages nested P-System membrane structures with parent-child relationships
 * and scope isolation. Implements Feature F1.3.1 requirements:
 * - Automatic membrane creation for specified actor classes
 * - Parent-child relationship mapping
 * - Membrane hierarchy visualization
 * - Scope isolation for contained objects
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UMembraneHierarchyManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UMembraneHierarchyManager();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Global membrane configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    FMembraneConfig GlobalConfig;

    /** Enable membrane processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    bool bEnableProcessing = true;

    /** Enable automatic membrane creation for configured actors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Config")
    bool bAutoCreateMembranes = true;

    /** Enable debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Membrane|Debug")
    bool bEnableDebugVisualization = false;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when a membrane is created */
    UPROPERTY(BlueprintAssignable, Category = "Membrane|Events")
    FOnMembraneCreated OnMembraneCreated;

    /** Called when a membrane is destroyed */
    UPROPERTY(BlueprintAssignable, Category = "Membrane|Events")
    FOnMembraneDestroyed OnMembraneDestroyed;

    /** Called when objects are transferred between membranes */
    UPROPERTY(BlueprintAssignable, Category = "Membrane|Events")
    FOnObjectTransferred OnObjectTransferred;

    /** Called when membrane state changes */
    UPROPERTY(BlueprintAssignable, Category = "Membrane|Events")
    FOnMembraneStateChanged OnMembraneStateChanged;

    // ========================================
    // PUBLIC API - MEMBRANE LIFECYCLE
    // ========================================

    /**
     * @brief Create a new root membrane
     * @param Label Human-readable label
     * @param Config Optional custom configuration
     * @return Membrane ID
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Lifecycle")
    FString CreateRootMembrane(const FString& Label, const FMembraneConfig& Config);

    /**
     * @brief Create a child membrane within a parent
     * @param ParentID Parent membrane ID
     * @param Label Human-readable label
     * @param Config Optional custom configuration
     * @return Membrane ID (empty if failed)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Lifecycle")
    FString CreateChildMembrane(const FString& ParentID, const FString& Label, const FMembraneConfig& Config);

    /**
     * @brief Create membrane automatically for an actor
     * @param Actor Actor to associate with membrane
     * @param ParentID Parent membrane ID (empty for root)
     * @return Membrane ID
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Lifecycle")
    FString CreateMembraneForActor(AActor* Actor, const FString& ParentID);

    /**
     * @brief Dissolve a membrane, moving contents to parent
     * @param MembraneID Membrane to dissolve
     * @return Success
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Lifecycle")
    bool DissolveMembrane(const FString& MembraneID);

    /**
     * @brief Divide a membrane into two
     * @param MembraneID Membrane to divide
     * @param SplitRatio Ratio of objects for first membrane (0.0-1.0)
     * @return Array of two new membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Lifecycle")
    TArray<FString> DivideMembrane(const FString& MembraneID, float SplitRatio = 0.5f);

    // ========================================
    // PUBLIC API - HIERARCHY MANAGEMENT
    // ========================================

    /**
     * @brief Get membrane state data
     * @param MembraneID Membrane ID
     * @return Membrane state data
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    FMembraneStateData GetMembraneState(const FString& MembraneID) const;

    /**
     * @brief Get parent membrane ID
     * @param MembraneID Child membrane ID
     * @return Parent membrane ID (empty if root or not found)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    FString GetParentMembrane(const FString& MembraneID) const;

    /**
     * @brief Get all child membrane IDs
     * @param MembraneID Parent membrane ID
     * @return Array of child membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetChildMembranes(const FString& MembraneID) const;

    /**
     * @brief Get all descendant membrane IDs (recursive)
     * @param MembraneID Ancestor membrane ID
     * @return Array of all descendant membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetAllDescendants(const FString& MembraneID) const;

    /**
     * @brief Get all ancestor membrane IDs (path to root)
     * @param MembraneID Descendant membrane ID
     * @return Array of ancestor membrane IDs (from parent to root)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetAncestorPath(const FString& MembraneID) const;

    /**
     * @brief Get all sibling membrane IDs
     * @param MembraneID Membrane ID
     * @return Array of sibling membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetSiblingMembranes(const FString& MembraneID) const;

    /**
     * @brief Get root membrane IDs
     * @return Array of root membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetRootMembranes() const;

    /**
     * @brief Get all membrane IDs at a specific depth
     * @param Depth Nesting depth (0 = root)
     * @return Array of membrane IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    TArray<FString> GetMembranesAtDepth(int32 Depth) const;

    /**
     * @brief Check if membrane exists
     * @param MembraneID Membrane ID
     * @return True if membrane exists
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    bool MembraneExists(const FString& MembraneID) const;

    /**
     * @brief Get nesting depth of a membrane
     * @param MembraneID Membrane ID
     * @return Nesting depth (0 = root, -1 if not found)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    int32 GetNestingDepth(const FString& MembraneID) const;

    /**
     * @brief Check if one membrane is ancestor of another
     * @param AncestorID Potential ancestor ID
     * @param DescendantID Potential descendant ID
     * @return True if ancestor relationship exists
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Hierarchy")
    bool IsAncestorOf(const FString& AncestorID, const FString& DescendantID) const;

    // ========================================
    // PUBLIC API - OBJECT MANAGEMENT
    // ========================================

    /**
     * @brief Add object to a membrane
     * @param MembraneID Target membrane ID
     * @param Object Object to add
     * @return Success
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    bool AddObject(const FString& MembraneID, const FMembraneObject& Object);

    /**
     * @brief Remove object from a membrane
     * @param MembraneID Source membrane ID
     * @param ObjectID Object ID to remove
     * @return Success
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    bool RemoveObject(const FString& MembraneID, const FString& ObjectID);

    /**
     * @brief Transfer object between membranes
     * @param SourceID Source membrane ID
     * @param TargetID Target membrane ID
     * @param ObjectID Object ID to transfer
     * @return Success
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    bool TransferObject(const FString& SourceID, const FString& TargetID, const FString& ObjectID);

    /**
     * @brief Get all objects in a membrane
     * @param MembraneID Membrane ID
     * @return Array of objects
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    TArray<FMembraneObject> GetObjects(const FString& MembraneID) const;

    /**
     * @brief Get objects by symbol type
     * @param MembraneID Membrane ID
     * @param Symbol Symbol to match
     * @return Array of matching objects
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    TArray<FMembraneObject> GetObjectsBySymbol(const FString& MembraneID, const FString& Symbol) const;

    /**
     * @brief Find object in membrane scope (including children)
     * @param MembraneID Starting membrane ID
     * @param ObjectID Object ID to find
     * @param bSearchDescendants Search in child membranes
     * @return Containing membrane ID (empty if not found)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Objects")
    FString FindObjectScope(const FString& MembraneID, const FString& ObjectID, bool bSearchDescendants = true) const;

    // ========================================
    // PUBLIC API - PERMEABILITY
    // ========================================

    /**
     * @brief Check if object can pass through membrane boundary
     * @param MembraneID Membrane ID
     * @param Object Object to check
     * @param bOutward True for outward passage, false for inward
     * @return True if passage is allowed
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Permeability")
    bool CanPassThrough(const FString& MembraneID, const FMembraneObject& Object, bool bOutward) const;

    /**
     * @brief Set permeability rules for a membrane
     * @param MembraneID Membrane ID
     * @param Rules New permeability rules
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Permeability")
    void SetPermeabilityRules(const FString& MembraneID, const FPermeabilityRules& Rules);

    // ========================================
    // PUBLIC API - STATE MANAGEMENT
    // ========================================

    /**
     * @brief Set membrane operational state
     * @param MembraneID Membrane ID
     * @param NewState New state
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|State")
    void SetMembraneState(const FString& MembraneID, EMembraneState NewState);

    /**
     * @brief Update membrane energy level
     * @param MembraneID Membrane ID
     * @param DeltaEnergy Energy change (positive = gain)
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|State")
    void UpdateMembraneEnergy(const FString& MembraneID, float DeltaEnergy);

    // ========================================
    // PUBLIC API - STATISTICS
    // ========================================

    /**
     * @brief Get hierarchy statistics
     * @return Statistics structure
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Stats")
    FMembraneHierarchyStats GetHierarchyStats() const;

    /**
     * @brief Get membrane count
     * @return Total membrane count
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Stats")
    int32 GetMembraneCount() const;

    /**
     * @brief Get hierarchy as string (for debugging)
     * @return Formatted hierarchy string
     */
    UFUNCTION(BlueprintCallable, Category = "Membrane|Debug")
    FString GetHierarchyString() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** All membranes by ID */
    TMap<FString, FMembraneStateData> Membranes;

    /** Membrane ID counter */
    int32 MembraneIDCounter = 0;

    /** Object ID counter */
    int32 ObjectIDCounter = 0;

    /** Pending messages queue */
    TArray<FMembraneMessage> MessageQueue;

    /** Current simulation time */
    float CurrentTime = 0.0f;

    // ========================================
    // INTERNAL METHODS - MEMBRANE OPERATIONS
    // ========================================

    /**
     * @brief Generate unique membrane ID
     */
    FString GenerateMembraneID();

    /**
     * @brief Generate unique object ID
     */
    FString GenerateObjectID();

    /**
     * @brief Update membrane statistics
     */
    void UpdateMembraneStats(FMembraneStateData& Membrane);

    /**
     * @brief Process pending messages
     */
    void ProcessMessageQueue();

    /**
     * @brief Recursively collect descendants
     */
    void CollectDescendants(const FString& MembraneID, TArray<FString>& OutDescendants) const;

    /**
     * @brief Build hierarchy string recursively
     */
    void BuildHierarchyString(const FString& MembraneID, int32 Indent, FString& OutString) const;

    /**
     * @brief Validate membrane configuration
     */
    bool ValidateConfig(const FMembraneConfig& Config) const;

    /**
     * @brief Check if adding child is allowed
     */
    bool CanAddChild(const FString& ParentID) const;

    /**
     * @brief Handle membrane dissolution cleanup
     */
    void CleanupDissolvedMembrane(const FString& MembraneID);

    /**
     * @brief Update hierarchy after structural changes
     */
    void UpdateHierarchyAfterChange();
};
