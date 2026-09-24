// NestedShellStructure.h
// Implementation of OEIS A000081 nested shell structure for cognitive architecture
// Defines the relationship between nesting levels and terms for echobeats streams

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NestedShellStructure.generated.h"

/**
 * Shell nesting level
 */
UENUM(BlueprintType)
enum class EShellNestingLevel : uint8
{
    Level1  UMETA(DisplayName = "Level 1 - 1 Term (Root)"),
    Level2  UMETA(DisplayName = "Level 2 - 2 Terms"),
    Level3  UMETA(DisplayName = "Level 3 - 4 Terms"),
    Level4  UMETA(DisplayName = "Level 4 - 9 Terms")
};

/**
 * Shell term state
 */
USTRUCT(BlueprintType)
struct FShellTermState
{
    GENERATED_BODY()

    /** Term ID within shell */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TermID = 0;

    /** Nesting level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShellNestingLevel NestingLevel = EShellNestingLevel::Level1;

    /** Parent term ID (-1 for root) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ParentTermID = -1;

    /** Child term IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> ChildTermIDs;

    /** Term activation (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activation = 0.0f;

    /** Term state vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> StateVector;

    /** Steps apart from parent (1, 2, 3, or 4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepsFromParent = 0;
};

/**
 * Shell propagation direction
 */
UENUM(BlueprintType)
enum class EShellPropagationDirection : uint8
{
    OuterToInner    UMETA(DisplayName = "Outer to Inner (Parent to Child)"),
    InnerToOuter    UMETA(DisplayName = "Inner to Outer (Child to Parent)"),
    Bidirectional   UMETA(DisplayName = "Bidirectional")
};

/**
 * Full nested shell state
 */
USTRUCT(BlueprintType)
struct FNestedShellState
{
    GENERATED_BODY()

    /** All shell terms (16 total: 1+2+4+9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FShellTermState> Terms;

    /** Current propagation direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShellPropagationDirection PropagationDirection = EShellPropagationDirection::Bidirectional;

    /** Overall shell coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShellCoherence = 1.0f;

    /** Current echobeat step mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentEchobeatStep = 1;

    /** Active nesting level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShellNestingLevel ActiveLevel = EShellNestingLevel::Level1;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShellTermActivated, int32, TermID, float, Activation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShellLevelChanged, EShellNestingLevel, OldLevel, EShellNestingLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShellPropagationComplete, EShellPropagationDirection, Direction, float, Coherence);

/**
 * Nested Shell Structure Component
 * 
 * Implements the OEIS A000081 nested shell structure for cognitive architecture.
 * 
 * OEIS A000081 sequence (number of rooted trees with n nodes):
 * - Level 1: 1 term (root)
 * - Level 2: 2 terms
 * - Level 3: 4 terms
 * - Level 4: 9 terms
 * - Total: 16 terms
 * 
 * Step relationships:
 * - 1 nest: 1 step apart
 * - 2 nests: 2 steps apart
 * - 3 nests: 3 steps apart
 * - 4 nests: 4 steps apart
 * 
 * Maps to 12-step cognitive loop with 5/7 twin prime relationship.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNestedShellStructure : public UActorComponent
{
    GENERATED_BODY()

public:
    UNestedShellStructure();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** State vector size per term */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NestedShell|Config")
    int32 StateVectorSize = 32;

    /** Propagation decay factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NestedShell|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PropagationDecay = 0.9f;

    /** Enable automatic propagation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NestedShell|Config")
    bool bEnableAutoPropagation = true;

    /** Propagation rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NestedShell|Config")
    float PropagationRate = 10.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "NestedShell|Events")
    FOnShellTermActivated OnTermActivated;

    UPROPERTY(BlueprintAssignable, Category = "NestedShell|Events")
    FOnShellLevelChanged OnLevelChanged;

    UPROPERTY(BlueprintAssignable, Category = "NestedShell|Events")
    FOnShellPropagationComplete OnPropagationComplete;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the nested shell structure */
    UFUNCTION(BlueprintCallable, Category = "NestedShell")
    void InitializeShellStructure();

    /** Build the OEIS A000081 tree structure */
    UFUNCTION(BlueprintCallable, Category = "NestedShell")
    void BuildTreeStructure();

    // ========================================
    // PUBLIC API - TERM OPERATIONS
    // ========================================

    /** Activate a term */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Terms")
    void ActivateTerm(int32 TermID, float Activation);

    /** Set term state vector */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Terms")
    void SetTermState(int32 TermID, const TArray<float>& StateVector);

    /** Get term state */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Terms")
    FShellTermState GetTermState(int32 TermID) const;

    /** Get terms at nesting level */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Terms")
    TArray<FShellTermState> GetTermsAtLevel(EShellNestingLevel Level) const;

    /** Get child terms */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Terms")
    TArray<FShellTermState> GetChildTerms(int32 ParentTermID) const;

    // ========================================
    // PUBLIC API - PROPAGATION
    // ========================================

    /** Propagate activation outer to inner */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Propagation")
    void PropagateOuterToInner();

    /** Propagate activation inner to outer */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Propagation")
    void PropagateInnerToOuter();

    /** Propagate bidirectionally */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Propagation")
    void PropagateBidirectional();

    /** Set propagation direction */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Propagation")
    void SetPropagationDirection(EShellPropagationDirection Direction);

    // ========================================
    // PUBLIC API - ECHOBEAT MAPPING
    // ========================================

    /** Map echobeat step to shell terms */
    UFUNCTION(BlueprintCallable, Category = "NestedShell|Mapping")
    void MapEchobeatStep(int32 Step);

    /** Get active terms for echobeat step */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Mapping")
    TArray<int32> GetActiveTermsForStep(int32 Step) const;

    /** Get nesting level for step */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Mapping")
    EShellNestingLevel GetNestingLevelForStep(int32 Step) const;

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get full shell state */
    UFUNCTION(BlueprintPure, Category = "NestedShell|State")
    FNestedShellState GetShellState() const;

    /** Get shell coherence */
    UFUNCTION(BlueprintPure, Category = "NestedShell|State")
    float GetShellCoherence() const;

    /** Get term count at level */
    UFUNCTION(BlueprintPure, Category = "NestedShell|State")
    int32 GetTermCountAtLevel(EShellNestingLevel Level) const;

    /** Get total term count */
    UFUNCTION(BlueprintPure, Category = "NestedShell|State")
    int32 GetTotalTermCount() const;

    // ========================================
    // STATIC UTILITIES
    // ========================================

    /** Get OEIS A000081 value for level */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Utilities")
    static int32 GetA000081Value(int32 Level);

    /** Get steps apart for nesting level */
    UFUNCTION(BlueprintPure, Category = "NestedShell|Utilities")
    static int32 GetStepsApart(EShellNestingLevel Level);

protected:
    /** Current shell state */
    FNestedShellState State;

    /** Propagation timer */
    float PropagationTimer = 0.0f;

    // Internal methods
    void ComputeShellCoherence();
    void PropagateToChildren(int32 ParentTermID);
    void PropagateToParent(int32 ChildTermID);
    TArray<float> AverageChildStates(int32 ParentTermID);
};
