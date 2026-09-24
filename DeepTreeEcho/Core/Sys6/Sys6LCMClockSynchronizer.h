// Sys6LCMClockSynchronizer.h
// 30-Step LCM Clock Synchronizer for Deep Tree Echo
// Implements the sys6 triality architecture with double-step delay pattern

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sys6LCMClockSynchronizer.generated.h"

// Forward declarations
class UEchobeatsStreamEngine;
class USys6OperadEngine;
class UTensionalCouplingDynamics;
class UPredictiveAdaptationEngine;

/**
 * Sys6 phase type (2-cycle)
 */
UENUM(BlueprintType)
enum class ESys6DyadPhase : uint8
{
    PhaseA  UMETA(DisplayName = "Phase A"),
    PhaseB  UMETA(DisplayName = "Phase B")
};

/**
 * Sys6 triad state (3-cycle)
 */
UENUM(BlueprintType)
enum class ESys6TriadState : uint8
{
    State1  UMETA(DisplayName = "State 1"),
    State2  UMETA(DisplayName = "State 2"),
    State3  UMETA(DisplayName = "State 3")
};

/**
 * Sys6 pentad stage (5-cycle)
 */
UENUM(BlueprintType)
enum class ESys6PentadStage : uint8
{
    Stage1  UMETA(DisplayName = "Stage 1"),
    Stage2  UMETA(DisplayName = "Stage 2"),
    Stage3  UMETA(DisplayName = "Stage 3"),
    Stage4  UMETA(DisplayName = "Stage 4"),
    Stage5  UMETA(DisplayName = "Stage 5")
};

/**
 * Entanglement order
 */
UENUM(BlueprintType)
enum class EEntanglementOrder : uint8
{
    Order1_Normal       UMETA(DisplayName = "Order 1 (Normal Concurrency)"),
    Order2_Entangled    UMETA(DisplayName = "Order 2 (Entangled Qubits)")
};

/**
 * Double-step delay pattern state
 */
USTRUCT(BlueprintType)
struct FDoubleStepDelayState
{
    GENERATED_BODY()

    /** Pattern step (1-4) */
    UPROPERTY(BlueprintReadOnly)
    int32 PatternStep = 1;

    /** Internal state (1, 4, 6, 1 cycle) */
    UPROPERTY(BlueprintReadOnly)
    int32 InternalState = 1;

    /** Dyad phase (A or B) */
    UPROPERTY(BlueprintReadOnly)
    ESys6DyadPhase DyadPhase = ESys6DyadPhase::PhaseA;

    /** Triad state (1, 2, or 3) */
    UPROPERTY(BlueprintReadOnly)
    ESys6TriadState TriadState = ESys6TriadState::State1;
};

/**
 * LCM clock state
 */
USTRUCT(BlueprintType)
struct FLCMClockState
{
    GENERATED_BODY()

    /** Current LCM step (0-29) */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Dyad cycle position (0-14, 15 complete cycles in 30 steps) */
    UPROPERTY(BlueprintReadOnly)
    int32 DyadCycle = 0;

    /** Triad cycle position (0-9, 10 complete cycles in 30 steps) */
    UPROPERTY(BlueprintReadOnly)
    int32 TriadCycle = 0;

    /** Pentad cycle position (0-5, 6 complete cycles in 30 steps) */
    UPROPERTY(BlueprintReadOnly)
    int32 PentadCycle = 0;

    /** Current dyad phase */
    UPROPERTY(BlueprintReadOnly)
    ESys6DyadPhase DyadPhase = ESys6DyadPhase::PhaseA;

    /** Current triad state */
    UPROPERTY(BlueprintReadOnly)
    ESys6TriadState TriadState = ESys6TriadState::State1;

    /** Current pentad stage */
    UPROPERTY(BlueprintReadOnly)
    ESys6PentadStage PentadStage = ESys6PentadStage::Stage1;

    /** Double-step delay state */
    UPROPERTY(BlueprintReadOnly)
    FDoubleStepDelayState DelayState;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Entanglement state
 */
USTRUCT(BlueprintType)
struct FEntanglementState
{
    GENERATED_BODY()

    /** Entanglement order */
    UPROPERTY(BlueprintReadOnly)
    EEntanglementOrder Order = EEntanglementOrder::Order1_Normal;

    /** Entangled thread pairs */
    UPROPERTY(BlueprintReadOnly)
    TArray<FIntPoint> EntangledPairs;

    /** Shared memory addresses */
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> SharedAddresses;

    /** Coherence level (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float CoherenceLevel = 1.0f;
};

/**
 * Thread synchronization state
 */
USTRUCT(BlueprintType)
struct FThreadSyncState
{
    GENERATED_BODY()

    /** Thread ID */
    UPROPERTY(BlueprintReadOnly)
    int32 ThreadID = 0;

    /** Current LCM step for this thread */
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentStep = 0;

    /** Phase offset (0, 10, or 20 for 3 threads) */
    UPROPERTY(BlueprintReadOnly)
    int32 PhaseOffset = 0;

    /** Is synchronized */
    UPROPERTY(BlueprintReadOnly)
    bool bIsSynchronized = true;

    /** Drift from master clock */
    UPROPERTY(BlueprintReadOnly)
    float Drift = 0.0f;
};

/**
 * Cubic concurrency state
 */
USTRUCT(BlueprintType)
struct FCubicConcurrencyState
{
    GENERATED_BODY()

    /** Active thread count */
    UPROPERTY(BlueprintReadOnly)
    int32 ActiveThreads = 3;

    /** Thread states */
    UPROPERTY(BlueprintReadOnly)
    TArray<FThreadSyncState> ThreadStates;

    /** Pairwise convolution results */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ConvolutionResults;

    /** Triadic convolution coherence */
    UPROPERTY(BlueprintReadOnly)
    float TriadicCoherence = 1.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCMStepAdvanced, const FLCMClockState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDyadPhaseChanged, ESys6DyadPhase, OldPhase, ESys6DyadPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTriadStateChanged, ESys6TriadState, OldState, ESys6TriadState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPentadStageChanged, ESys6PentadStage, OldStage, ESys6PentadStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCMCycleComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntanglementChanged, const FEntanglementState&, State);

/**
 * Sys6 LCM Clock Synchronizer
 * 
 * Implements the 30-step LCM(2,3,5) clock for the sys6 triality architecture.
 * Manages the double-step delay pattern, cubic concurrency, and entanglement.
 * 
 * Key features:
 * - 30-step irreducible cycle derived from LCM(2,3,5)
 * - Double-step delay pattern: (A,1) -> (A,2) -> (B,2) -> (B,3)
 * - Cubic concurrency with pairwise thread convolutions
 * - Entanglement support (order 2 qubits)
 * - Synchronization with echobeats 12-step cycle
 * - Prime power delegation to nested structures
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USys6LCMClockSynchronizer : public UActorComponent
{
    GENERATED_BODY()

public:
    USys6LCMClockSynchronizer();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Clock tick rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Timing")
    float ClockTickRate = 30.0f;

    /** Enable automatic advancement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Timing")
    bool bAutoAdvance = true;

    /** Enable entanglement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Entanglement")
    bool bEnableEntanglement = true;

    /** Default entanglement order */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Entanglement")
    EEntanglementOrder DefaultEntanglementOrder = EEntanglementOrder::Order2_Entangled;

    /** Number of concurrent threads */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Concurrency")
    int32 ConcurrentThreadCount = 3;

    /** Enable cubic concurrency */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Concurrency")
    bool bEnableCubicConcurrency = true;

    /** Synchronization tolerance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Sync")
    float SyncTolerance = 0.01f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLCMStepAdvanced OnLCMStepAdvanced;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDyadPhaseChanged OnDyadPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTriadStateChanged OnTriadStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPentadStageChanged OnPentadStageChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLCMCycleComplete OnLCMCycleComplete;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEntanglementChanged OnEntanglementChanged;

    // ========================================
    // PUBLIC API - CLOCK CONTROL
    // ========================================

    /** Advance clock by one step */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void AdvanceStep();

    /** Advance clock by N steps */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void AdvanceSteps(int32 Count);

    /** Set clock to specific step */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void SetStep(int32 Step);

    /** Reset clock to step 0 */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void ResetClock();

    /** Pause clock */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void PauseClock();

    /** Resume clock */
    UFUNCTION(BlueprintCallable, Category = "Clock")
    void ResumeClock();

    /** Is clock paused */
    UFUNCTION(BlueprintPure, Category = "Clock")
    bool IsClockPaused() const;

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get current LCM clock state */
    UFUNCTION(BlueprintPure, Category = "State")
    FLCMClockState GetClockState() const;

    /** Get current LCM step */
    UFUNCTION(BlueprintPure, Category = "State")
    int32 GetCurrentStep() const;

    /** Get current dyad phase */
    UFUNCTION(BlueprintPure, Category = "State")
    ESys6DyadPhase GetDyadPhase() const;

    /** Get current triad state */
    UFUNCTION(BlueprintPure, Category = "State")
    ESys6TriadState GetTriadState() const;

    /** Get current pentad stage */
    UFUNCTION(BlueprintPure, Category = "State")
    ESys6PentadStage GetPentadStage() const;

    /** Get double-step delay state */
    UFUNCTION(BlueprintPure, Category = "State")
    FDoubleStepDelayState GetDelayState() const;

    // ========================================
    // PUBLIC API - DOUBLE-STEP DELAY PATTERN
    // ========================================

    /** Get delay pattern for step */
    UFUNCTION(BlueprintPure, Category = "DelayPattern")
    FDoubleStepDelayState GetDelayPatternForStep(int32 Step) const;

    /** Validate delay pattern */
    UFUNCTION(BlueprintPure, Category = "DelayPattern")
    bool ValidateDelayPattern(int32 Step, ESys6DyadPhase ExpectedDyad, ESys6TriadState ExpectedTriad) const;

    /** Get all delay pattern states */
    UFUNCTION(BlueprintPure, Category = "DelayPattern")
    TArray<FDoubleStepDelayState> GetFullDelayPattern() const;

    // ========================================
    // PUBLIC API - ENTANGLEMENT
    // ========================================

    /** Get entanglement state */
    UFUNCTION(BlueprintPure, Category = "Entanglement")
    FEntanglementState GetEntanglementState() const;

    /** Set entanglement order */
    UFUNCTION(BlueprintCallable, Category = "Entanglement")
    void SetEntanglementOrder(EEntanglementOrder Order);

    /** Create entangled pair */
    UFUNCTION(BlueprintCallable, Category = "Entanglement")
    void CreateEntangledPair(int32 Thread1, int32 Thread2, int32 SharedAddress);

    /** Break entanglement */
    UFUNCTION(BlueprintCallable, Category = "Entanglement")
    void BreakEntanglement(int32 Thread1, int32 Thread2);

    /** Is pair entangled */
    UFUNCTION(BlueprintPure, Category = "Entanglement")
    bool IsPairEntangled(int32 Thread1, int32 Thread2) const;

    // ========================================
    // PUBLIC API - CUBIC CONCURRENCY
    // ========================================

    /** Get cubic concurrency state */
    UFUNCTION(BlueprintPure, Category = "Concurrency")
    FCubicConcurrencyState GetConcurrencyState() const;

    /** Get thread sync state */
    UFUNCTION(BlueprintPure, Category = "Concurrency")
    FThreadSyncState GetThreadState(int32 ThreadID) const;

    /** Synchronize thread */
    UFUNCTION(BlueprintCallable, Category = "Concurrency")
    void SynchronizeThread(int32 ThreadID);

    /** Synchronize all threads */
    UFUNCTION(BlueprintCallable, Category = "Concurrency")
    void SynchronizeAllThreads();

    /** Compute pairwise convolution */
    UFUNCTION(BlueprintCallable, Category = "Concurrency")
    float ComputePairwiseConvolution(int32 Thread1, int32 Thread2);

    /** Compute triadic convolution */
    UFUNCTION(BlueprintCallable, Category = "Concurrency")
    float ComputeTriadicConvolution();

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Map LCM step to echobeat step */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    int32 MapLCMToEchobeat(int32 LCMStep) const;

    /** Map echobeat step to LCM step */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    int32 MapEchobeatToLCM(int32 EchobeatStep) const;

    /** Get echobeat stream for LCM step */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    int32 GetStreamForLCMStep(int32 LCMStep) const;

    /** Synchronize with echobeats engine */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void SynchronizeWithEchobeats();

    // ========================================
    // PUBLIC API - PRIME POWER DELEGATION
    // ========================================

    /** Get prime factor decomposition for step */
    UFUNCTION(BlueprintPure, Category = "PrimePower")
    void GetPrimeFactors(int32 Step, int32& OutPower2, int32& OutPower3, int32& OutPower5) const;

    /** Delegate to nested concurrency (power of 2) */
    UFUNCTION(BlueprintCallable, Category = "PrimePower")
    void DelegateToNestedConcurrency(int32 Power2Level);

    /** Delegate to nested convolution (power of 3) */
    UFUNCTION(BlueprintCallable, Category = "PrimePower")
    void DelegateToNestedConvolution(int32 Power3Level);

    /** Delegate to nested transformation (power of 5) */
    UFUNCTION(BlueprintCallable, Category = "PrimePower")
    void DelegateToNestedTransformation(int32 Power5Level);

protected:
    // Component references
    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USys6OperadEngine* OperadEngine;

    UPROPERTY()
    UTensionalCouplingDynamics* CouplingDynamics;

    UPROPERTY()
    UPredictiveAdaptationEngine* AdaptationEngine;

    // Internal state
    FLCMClockState ClockState;
    FEntanglementState EntanglementState;
    FCubicConcurrencyState ConcurrencyState;
    bool bIsPaused = false;
    float AccumulatedTime = 0.0f;
    float StepDuration = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void InitializeThreadStates();
    void UpdateClockState();
    void UpdateDelayPattern();
    void UpdateEntanglement();
    void UpdateConcurrency();
    void BroadcastStateChanges(const FLCMClockState& OldState);
    void ComputePhaseFromStep(int32 Step);
    ESys6DyadPhase ComputeDyadPhase(int32 Step) const;
    ESys6TriadState ComputeTriadState(int32 Step) const;
    ESys6PentadStage ComputePentadStage(int32 Step) const;
    FDoubleStepDelayState ComputeDelayState(int32 Step) const;
};
