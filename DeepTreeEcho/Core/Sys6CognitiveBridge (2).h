// Sys6CognitiveBridge.h
// Bridge between sys6 operad engine and 12-step cognitive cycle
// Implements the mapping from 30-step LCM clock to 12-step echobeats

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sys6OperadEngine.h"
#include "CognitiveCycleManager.h"
#include "Sys6CognitiveBridge.generated.h"

/**
 * Mapping mode between sys6 and cognitive cycle
 */
UENUM(BlueprintType)
enum class ESys6MappingMode : uint8
{
    Direct          UMETA(DisplayName = "Direct: 30→12 modular mapping"),
    Interleaved     UMETA(DisplayName = "Interleaved: 5 stages × 12 steps"),
    Hierarchical    UMETA(DisplayName = "Hierarchical: Nested shell mapping")
};

/**
 * Nested shell configuration following OEIS A000081
 */
USTRUCT(BlueprintType)
struct FNestedShellConfig
{
    GENERATED_BODY()

    /** Nesting level (1-4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NestingLevel = 1;

    /** Number of terms at this level (1, 2, 4, 9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TermCount = 1;

    /** Steps apart (1, 2, 3, 4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepsApart = 1;

    /** Active stream IDs at this level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> ActiveStreams;
};

/**
 * Thread permutation state for multiplexing
 */
USTRUCT(BlueprintType)
struct FThreadPermutationState
{
    GENERATED_BODY()

    /** Current permutation index (0-5 for 6 permutations of 4 threads) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PermutationIndex = 0;

    /** Current thread pair P(i,j) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> CurrentPair;

    /** Complementary triad MP1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> TriadMP1;

    /** Complementary triad MP2 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> TriadMP2;

    FThreadPermutationState()
    {
        CurrentPair = {1, 2};
        TriadMP1 = {1, 2, 3};
        TriadMP2 = {1, 3, 4};
    }
};

/**
 * Bridge state combining sys6 and cognitive cycle
 */
USTRUCT(BlueprintType)
struct FSys6BridgeState
{
    GENERATED_BODY()

    /** Current sys6 step (1-30) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Sys6Step = 1;

    /** Current cognitive step (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CognitiveStep = 1;

    /** Current mapping mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESys6MappingMode MappingMode = ESys6MappingMode::Hierarchical;

    /** Current nested shell level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentShellLevel = 1;

    /** Thread permutation state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FThreadPermutationState ThreadState;

    /** Coherence between sys6 and cognitive cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BridgeCoherence = 1.0f;

    /** Phase alignment quality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhaseAlignment = 1.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBridgeSyncEvent, int32, Sys6Step, int32, CognitiveStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShellTransition, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadPermutation, int32, PermutationIndex);

/**
 * Sys6 Cognitive Bridge
 * Integrates the 30-step sys6 operad with the 12-step cognitive cycle
 * Implements nested shell mapping following OEIS A000081
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USys6CognitiveBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    USys6CognitiveBridge();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Mapping mode between sys6 and cognitive cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge Configuration")
    ESys6MappingMode MappingMode = ESys6MappingMode::Hierarchical;

    /** Enable automatic synchronization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge Configuration")
    bool bEnableAutoSync = true;

    /** Synchronization strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge Configuration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SyncStrength = 0.5f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Bridge Events")
    FOnBridgeSyncEvent OnBridgeSync;

    UPROPERTY(BlueprintAssignable, Category = "Bridge Events")
    FOnShellTransition OnShellTransition;

    UPROPERTY(BlueprintAssignable, Category = "Bridge Events")
    FOnThreadPermutation OnThreadPermutation;

    // ========================================
    // BRIDGE OPERATIONS
    // ========================================

    /** Initialize the bridge */
    UFUNCTION(BlueprintCallable, Category = "Bridge Control")
    void InitializeBridge();

    /** Synchronize sys6 and cognitive cycle */
    UFUNCTION(BlueprintCallable, Category = "Bridge Control")
    void Synchronize();

    /** Map sys6 step to cognitive step */
    UFUNCTION(BlueprintPure, Category = "Bridge Mapping")
    int32 MapSys6ToCognitive(int32 Sys6Step) const;

    /** Map cognitive step to sys6 step */
    UFUNCTION(BlueprintPure, Category = "Bridge Mapping")
    int32 MapCognitiveToSys6(int32 CognitiveStep) const;

    /** Get nested shell for current step */
    UFUNCTION(BlueprintPure, Category = "Bridge Mapping")
    FNestedShellConfig GetCurrentShell() const;

    /** Advance thread permutation */
    UFUNCTION(BlueprintCallable, Category = "Bridge Control")
    void AdvanceThreadPermutation();

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get bridge state */
    UFUNCTION(BlueprintPure, Category = "Bridge State")
    FSys6BridgeState GetBridgeState() const;

    /** Get current nested shell level */
    UFUNCTION(BlueprintPure, Category = "Bridge State")
    int32 GetCurrentShellLevel() const;

    /** Get bridge coherence */
    UFUNCTION(BlueprintPure, Category = "Bridge State")
    float GetBridgeCoherence() const;

    /** Check if at shell boundary */
    UFUNCTION(BlueprintPure, Category = "Bridge State")
    bool IsAtShellBoundary() const;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to sys6 operad engine */
    UPROPERTY(BlueprintReadOnly, Category = "Components")
    USys6OperadEngine* Sys6Engine;

    /** Reference to cognitive cycle manager */
    UPROPERTY(BlueprintReadOnly, Category = "Components")
    UCognitiveCycleManager* CognitiveManager;

protected:
    /** Bridge state */
    FSys6BridgeState State;

    /** Nested shell configurations */
    TArray<FNestedShellConfig> ShellConfigs;

    /** Thread permutation sequence P(i,j) */
    TArray<TArray<int32>> ThreadPairSequence;

    /** Complementary triad sequences */
    TArray<TArray<int32>> TriadMP1Sequence;
    TArray<TArray<int32>> TriadMP2Sequence;

    // Internal methods
    void FindComponentReferences();
    void InitializeShellConfigs();
    void InitializeThreadPermutations();
    void UpdateBridgeState();
    void ComputeBridgeCoherence();
    void ComputePhaseAlignment();
    int32 ComputeShellLevel(int32 Step) const;
    void HandleSys6StepAdvanced(int32 OldStep, int32 NewStep);
    void HandleCognitiveStepChanged(int32 OldStep, int32 NewStep);

    /** Direct mapping: 30 → 12 via modular arithmetic */
    int32 DirectMapping(int32 Sys6Step) const;

    /** Interleaved mapping: 5 stages × 12 steps */
    int32 InterleavedMapping(int32 Sys6Step) const;

    /** Hierarchical mapping: Nested shells */
    int32 HierarchicalMapping(int32 Sys6Step) const;
};
