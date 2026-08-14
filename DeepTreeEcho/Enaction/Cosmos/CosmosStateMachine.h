#pragma once

/**
 * Cosmos State Machine - System 1-5 State Transition Architecture
 * 
 * Implements the staged developmental model from the Deep-Tree-Echo framework:
 * - System 1: Undifferentiated unity (1 Universal Set, 1-step cycle)
 * - System 2: First differentiation (2 Sets, 2-step cycle)
 * - System 3: Orthogonal dyadic pairs (4 Sets, 4-step cycle)
 * - System 4: 3 concurrent consciousness threads (5 Sets, 12-step cycle)
 * - System 5: Nested concurrency with tetradic tensor bundles (7 Sets, 60-step cycle)
 * 
 * Mathematical Foundation:
 * - OEIS A000081 sequence for hierarchical nesting (1, 2, 4, 9 terms)
 * - Integer partitions of n for System n structure
 * - Projective geometry analogues for nested concurrency
 * 
 * @see System5CognitiveIntegration for tetradic architecture
 * @see CognitiveCycleManager for 12-step cycle implementation
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CosmosStateMachine.generated.h"

/**
 * System Level Enumeration
 */
UENUM(BlueprintType)
enum class ECosmosSystemLevel : uint8
{
    /** System 1: Undifferentiated unity */
    System1 UMETA(DisplayName = "System 1 - Unity"),
    
    /** System 2: First differentiation */
    System2 UMETA(DisplayName = "System 2 - Differentiation"),
    
    /** System 3: Quaternary structure */
    System3 UMETA(DisplayName = "System 3 - Quaternary"),
    
    /** System 4: Concurrent threads */
    System4 UMETA(DisplayName = "System 4 - Concurrent"),
    
    /** System 5: Nested concurrency */
    System5 UMETA(DisplayName = "System 5 - Nested")
};

/**
 * State Polarity
 */
UENUM(BlueprintType)
enum class EStatePolarity : uint8
{
    /** Expansion polarity */
    Expansion UMETA(DisplayName = "Expansion (E)"),
    
    /** Reduction polarity */
    Reduction UMETA(DisplayName = "Reduction (R)"),
    
    /** Neutral polarity */
    Neutral UMETA(DisplayName = "Neutral (-)"),
    
    /** Primary universal */
    Primary UMETA(DisplayName = "Primary (P)"),
    
    /** Secondary universal */
    Secondary UMETA(DisplayName = "Secondary (S)"),
    
    /** Tertiary universal */
    Tertiary UMETA(DisplayName = "Tertiary (T)")
};

/**
 * Cosmos State - Represents a single state in the state machine
 */
USTRUCT(BlueprintType)
struct FCosmosState
{
    GENERATED_BODY()

    /** State value (1-9 for enneagram-based states) */
    UPROPERTY(BlueprintReadWrite)
    int32 Value = 1;

    /** State polarity */
    UPROPERTY(BlueprintReadWrite)
    EStatePolarity Polarity = EStatePolarity::Expansion;

    /** State label (e.g., "9E", "3R", "U-P") */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Get formatted state string */
    FString ToString() const
    {
        return Label.IsEmpty() ? FString::Printf(TEXT("%d%s"), Value, 
            Polarity == EStatePolarity::Expansion ? TEXT("E") : 
            Polarity == EStatePolarity::Reduction ? TEXT("R") : TEXT("-")) : Label;
    }
};

/**
 * Universal Set - Represents a universal (synchronous) state set
 */
USTRUCT(BlueprintType)
struct FUniversalSet
{
    GENERATED_BODY()

    /** Set identifier (U1, U2, U3) */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Current state */
    UPROPERTY(BlueprintReadWrite)
    FCosmosState CurrentState;

    /** State history */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCosmosState> StateHistory;

    /** Cycle length */
    UPROPERTY(BlueprintReadWrite)
    int32 CycleLength = 1;

    /** Phase offset within cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 PhaseOffset = 0;
};

/**
 * Particular Set - Represents a particular (concurrent) state set
 */
USTRUCT(BlueprintType)
struct FParticularSet
{
    GENERATED_BODY()

    /** Set identifier (P1, P2, P3, P4) */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Current state */
    UPROPERTY(BlueprintReadWrite)
    FCosmosState CurrentState;

    /** State history */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCosmosState> StateHistory;

    /** Cycle length */
    UPROPERTY(BlueprintReadWrite)
    int32 CycleLength = 1;

    /** Phase offset within staggered cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 PhaseOffset = 0;

    /** Stagger interval */
    UPROPERTY(BlueprintReadWrite)
    int32 StaggerInterval = 5;
};

/**
 * Tensor Bundle - System 5 tetradic structure
 */
USTRUCT(BlueprintType)
struct FTensorBundle
{
    GENERATED_BODY()

    /** Bundle identifier (T1, T2, T3, T4) */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Three concurrent threads (fibers) within the bundle */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCosmosState> Fibers;

    /** Bundle coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Phase offset in 5-step staggered cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 PhaseOffset = 0;
};

/**
 * System Configuration
 */
USTRUCT(BlueprintType)
struct FCosmosSystemConfig
{
    GENERATED_BODY()

    /** System level */
    UPROPERTY(BlueprintReadWrite)
    ECosmosSystemLevel Level = ECosmosSystemLevel::System1;

    /** Number of universal sets */
    UPROPERTY(BlueprintReadWrite)
    int32 UniversalSetCount = 1;

    /** Number of particular sets */
    UPROPERTY(BlueprintReadWrite)
    int32 ParticularSetCount = 0;

    /** Cycle length */
    UPROPERTY(BlueprintReadWrite)
    int32 CycleLength = 1;

    /** Enable nested concurrency (System 5) */
    UPROPERTY(BlueprintReadWrite)
    bool bEnableNestedConcurrency = false;

    /** Number of tensor bundles (System 5) */
    UPROPERTY(BlueprintReadWrite)
    int32 TensorBundleCount = 0;
};

/**
 * Cosmos State Machine Component
 * 
 * Implements the System 1-5 state transition architecture for
 * the Deep-Tree-Echo cognitive framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UCosmosStateMachine : public UActorComponent
{
    GENERATED_BODY()

public:
    UCosmosStateMachine();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Current system level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmos|Config")
    ECosmosSystemLevel SystemLevel = ECosmosSystemLevel::System1;

    /** Enable automatic state transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmos|Config")
    bool bEnableAutoTransition = true;

    /** Time step duration in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmos|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float TimeStepDuration = 0.1f;

    // ========================================
    // STATE
    // ========================================

    /** Current time step */
    UPROPERTY(BlueprintReadOnly, Category = "Cosmos|State")
    int32 CurrentTimeStep = 0;

    /** Universal sets */
    UPROPERTY(BlueprintReadOnly, Category = "Cosmos|State")
    TArray<FUniversalSet> UniversalSets;

    /** Particular sets */
    UPROPERTY(BlueprintReadOnly, Category = "Cosmos|State")
    TArray<FParticularSet> ParticularSets;

    /** Tensor bundles (System 5) */
    UPROPERTY(BlueprintReadOnly, Category = "Cosmos|State")
    TArray<FTensorBundle> TensorBundles;

    /** Synchronous sets (System 5 hierarchical) */
    UPROPERTY(BlueprintReadOnly, Category = "Cosmos|State")
    TArray<FCosmosState> SynchronousSets;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize state machine for specified system level */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem(ECosmosSystemLevel Level);

    /** Initialize System 1 (Unity) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem1();

    /** Initialize System 2 (Differentiation) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem2();

    /** Initialize System 3 (Quaternary) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem3();

    /** Initialize System 4 (Concurrent) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem4();

    /** Initialize System 5 (Nested Concurrency) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos")
    void InitializeSystem5();

    // ========================================
    // PUBLIC API - STATE TRANSITIONS
    // ========================================

    /** Advance state machine by one time step */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Transition")
    void Step();

    /** Advance state machine by multiple time steps */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Transition")
    void StepMultiple(int32 Steps);

    /** Reset state machine to initial state */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Transition")
    void Reset();

    /** Transition to next system level */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Transition")
    void EvolveToNextLevel();

    // ========================================
    // PUBLIC API - STATE ACCESS
    // ========================================

    /** Get current system configuration */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|State")
    FCosmosSystemConfig GetSystemConfig() const;

    /** Get state of universal set by index */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|State")
    FCosmosState GetUniversalState(int32 Index) const;

    /** Get state of particular set by index */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|State")
    FCosmosState GetParticularState(int32 Index) const;

    /** Get cycle position (0 to CycleLength-1) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|State")
    int32 GetCyclePosition() const;

    /** Get meta-cycle position (for System 5, 0-59) */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|State")
    int32 GetMetaCyclePosition() const;

    // ========================================
    // PUBLIC API - CONVOLUTION (System 5)
    // ========================================

    /** Compute convolution for nested concurrency */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Convolution")
    int32 ComputeConvolution(int32 SetIndex) const;

    /** Get active universal set index */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Convolution")
    int32 GetActiveUniversalIndex() const;

    /** Check if particular set is active this step */
    UFUNCTION(BlueprintCallable, Category = "Cosmos|Convolution")
    bool IsParticularSetActive(int32 SetIndex) const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Accumulated time for auto-transition */
    float AccumulatedTime = 0.0f;

    /** System 4 state sequences (deterministic) */
    TArray<TArray<FCosmosState>> System4Sequences;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize System 4 deterministic sequences */
    void InitializeSystem4Sequences();

    /** Transition universal sets */
    void TransitionUniversalSets();

    /** Transition particular sets */
    void TransitionParticularSets();

    /** Transition tensor bundles (System 5) */
    void TransitionTensorBundles();

    /** Transition synchronous sets (System 5 hierarchical) */
    void TransitionSynchronousSets();

    /** Apply convolution to particular set transition */
    FCosmosState ApplyConvolution(int32 SetIndex, const FCosmosState& CurrentState);

    /** Create state from value and polarity */
    FCosmosState CreateState(int32 Value, EStatePolarity Polarity);

    /** Create universal state label */
    FCosmosState CreateUniversalState(EStatePolarity Polarity);
};
