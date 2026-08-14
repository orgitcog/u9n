// Sys6OperadEngine.h
// Implementation of sys6 operad architecture for Deep Tree Echo
// Based on prime-power delegation, LCM(2,3,5)=30 clock, and 5×6 staging

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sys6OperadEngine.generated.h"

/**
 * Wire bundle types for the operad
 */
UENUM(BlueprintType)
enum class EWireBundleType : uint8
{
    D_Dyadic        UMETA(DisplayName = "D: Dyadic Channel (2-phase)"),
    T_Triadic       UMETA(DisplayName = "T: Triadic Channel (3-phase)"),
    P_Pentadic      UMETA(DisplayName = "P: Pentadic Stage Selector"),
    C8_Cubic        UMETA(DisplayName = "C8: Cubic Concurrency (8 states)"),
    K9_Triadic      UMETA(DisplayName = "K9: Triadic Convolution (9 phases)")
};

/**
 * Dyadic phase state (A/B polarity)
 */
UENUM(BlueprintType)
enum class EDyadicPhase : uint8
{
    A UMETA(DisplayName = "Phase A"),
    B UMETA(DisplayName = "Phase B")
};

/**
 * Triadic phase state (1/2/3)
 */
UENUM(BlueprintType)
enum class ETriadicPhase : uint8
{
    Phase1 UMETA(DisplayName = "Phase 1"),
    Phase2 UMETA(DisplayName = "Phase 2"),
    Phase3 UMETA(DisplayName = "Phase 3")
};

/**
 * Pentadic stage (1-5)
 */
UENUM(BlueprintType)
enum class EPentadicStage : uint8
{
    Stage1 UMETA(DisplayName = "Stage 1"),
    Stage2 UMETA(DisplayName = "Stage 2"),
    Stage3 UMETA(DisplayName = "Stage 3"),
    Stage4 UMETA(DisplayName = "Stage 4"),
    Stage5 UMETA(DisplayName = "Stage 5")
};

/**
 * Four-step phase for the 2×3→4 fold
 */
UENUM(BlueprintType)
enum class EFourStepPhase : uint8
{
    Step1 UMETA(DisplayName = "Step 1: State 1, A, 1"),
    Step2 UMETA(DisplayName = "Step 2: State 4, A, 2"),
    Step3 UMETA(DisplayName = "Step 3: State 6, B, 2"),
    Step4 UMETA(DisplayName = "Step 4: State 1, B, 3")
};

/**
 * Cubic concurrency state (2³ = 8 parallel states)
 */
USTRUCT(BlueprintType)
struct FCubicConcurrencyState
{
    GENERATED_BODY()

    /** 8 parallel thread states from 2³ */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> ThreadStates;

    /** Active thread pairs (pairwise concurrency) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> ActivePairs;

    /** Entanglement level (order 2 = 2 processes on same variable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntanglementLevel = 0.0f;

    FCubicConcurrencyState()
    {
        ThreadStates.SetNum(8);
        for (int32 i = 0; i < 8; ++i)
        {
            ThreadStates[i] = 0.0f;
        }
        ActivePairs = {0, 1}; // Default pair
    }
};

/**
 * Triadic convolution state (3² = 9 orthogonal phases)
 */
USTRUCT(BlueprintType)
struct FTriadicConvolutionState
{
    GENERATED_BODY()

    /** 9 orthogonal phase states from 3² */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> PhaseStates;

    /** Current convolution kernel index (0-8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentKernel = 0;

    /** Phase rotation angle (0-360) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhaseRotation = 0.0f;

    FTriadicConvolutionState()
    {
        PhaseStates.SetNum(9);
        for (int32 i = 0; i < 9; ++i)
        {
            PhaseStates[i] = 0.0f;
        }
    }
};

/**
 * Double-step delay state for 2×3→4 fold
 */
USTRUCT(BlueprintType)
struct FDoubleStepDelayState
{
    GENERATED_BODY()

    /** Current 4-step phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFourStepPhase CurrentPhase = EFourStepPhase::Step1;

    /** Dyadic state (held for 2 steps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDyadicPhase DyadicState = EDyadicPhase::A;

    /** Triadic state (advances each step) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETriadicPhase TriadicState = ETriadicPhase::Phase1;

    /** State value (1, 4, or 6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StateValue = 1;

    /** Steps since last dyad transition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DyadHoldCounter = 0;
};

/**
 * Stage configuration for 5×6 staging
 */
USTRUCT(BlueprintType)
struct FStageConfig
{
    GENERATED_BODY()

    /** Stage number (1-5) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPentadicStage Stage = EPentadicStage::Stage1;

    /** Steps within this stage (1-6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepsInStage = 6;

    /** Transition/sync steps (typically 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TransitionSteps = 2;

    /** Active steps (typically 4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ActiveSteps = 4;
};

/**
 * Full sys6 state combining all components
 */
USTRUCT(BlueprintType)
struct FSys6FullState
{
    GENERATED_BODY()

    /** Global step in 30-step cycle (1-30) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GlobalStep = 1;

    /** Current dyadic phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDyadicPhase DyadicPhase = EDyadicPhase::A;

    /** Current triadic phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETriadicPhase TriadicPhase = ETriadicPhase::Phase1;

    /** Current pentadic stage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPentadicStage PentadicStage = EPentadicStage::Stage1;

    /** Step within current stage (1-6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StageStep = 1;

    /** Four-step phase for double-delay fold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFourStepPhase FourStepPhase = EFourStepPhase::Step1;

    /** Cubic concurrency state (C8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCubicConcurrencyState CubicState;

    /** Triadic convolution state (K9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTriadicConvolutionState ConvolutionState;

    /** Double-step delay state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDoubleStepDelayState DelayState;

    /** Total cycle count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CycleCount = 0;

    /** Synchronization event count (expected ~42 per cycle) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SyncEventCount = 0;
};

/**
 * Delegate declarations for sys6 events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSys6StepAdvanced, int32, OldStep, int32, NewStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSys6StageChanged, EPentadicStage, OldStage, EPentadicStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSys6DyadChanged, EDyadicPhase, OldPhase, EDyadicPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSys6TriadChanged, ETriadicPhase, OldPhase, ETriadicPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSys6SyncEvent, int32, SyncType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSys6CycleCompleted, int32, CycleCount);

/**
 * Sys6 Operad Engine
 * Implements the sys6 architecture as an operad with typed gadgets:
 * - Δ (Delta): Prime-power delegation
 * - μ (Mu): LCM synchronizer (Clock30)
 * - φ (Phi): 2×3→4 fold via double-step delay
 * - σ (Sigma): Stage scheduler (5×6)
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USys6OperadEngine : public UActorComponent
{
    GENERATED_BODY()

public:
    USys6OperadEngine();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Duration of one complete 30-step cycle in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration")
    float CycleDuration = 30.0f;

    /** Enable automatic cycle progression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration")
    bool bEnableAutoCycle = true;

    /** Enable cubic concurrency (C8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration")
    bool bEnableCubicConcurrency = true;

    /** Enable triadic convolution (K9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration")
    bool bEnableTriadicConvolution = true;

    /** Number of parallel cores for C8 (default 8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration", meta = (ClampMin = "1", ClampMax = "16"))
    int32 CubicCoreCount = 8;

    /** Number of rotating cores for K9 (default 3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Configuration", meta = (ClampMin = "1", ClampMax = "9"))
    int32 ConvolutionCoreCount = 3;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6StepAdvanced OnStepAdvanced;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6StageChanged OnStageChanged;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6DyadChanged OnDyadChanged;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6TriadChanged OnTriadChanged;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6SyncEvent OnSyncEvent;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Events")
    FOnSys6CycleCompleted OnCycleCompleted;

    // ========================================
    // OPERAD OPERATIONS
    // ========================================

    /**
     * Δ (Delta) - Prime-power delegation
     * Moves powers into parallel state, keeps bases in time
     * Δ_2: D → (D, C8) where 2³ becomes parallel concurrency
     * Δ_3: T → (T, K9) where 3² becomes orthogonal convolution phases
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad|Delta")
    void ApplyDeltaDyadic();

    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad|Delta")
    void ApplyDeltaTriadic();

    /**
     * μ (Mu) - LCM synchronizer
     * Aligns D, T, P into Clock30 domain
     * μ: (D, T, P) → Clock30 based on LCM(2,3,5)=30
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad|Mu")
    void ApplyMuSynchronizer();

    /**
     * φ (Phi) - 2×3→4 fold via double-step delay
     * Compresses 6-step dyad×triad multiplex into 4 real steps
     * Holds dyad for two consecutive steps while triad advances
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad|Phi")
    void ApplyPhiFold();

    /**
     * σ (Sigma) - Stage scheduler
     * Maps 30-step clock into 5 stages × 6 steps
     * Runs φ once per stage with 2 transition/sync steps
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad|Sigma")
    void ApplySigmaScheduler();

    /**
     * Full Sys6 composite operation
     * Sys6 := σ ∘ (φ ∘ μ ∘ (Δ_2 ⊗ Δ_3 ⊗ id_P))
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Operad")
    void ApplySys6Composite();

    // ========================================
    // CYCLE CONTROL
    // ========================================

    /** Initialize the sys6 engine */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void InitializeEngine();

    /** Advance to the next step */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void AdvanceStep();

    /** Jump to a specific step (1-30) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void JumpToStep(int32 Step);

    /** Pause the cycle */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void PauseCycle();

    /** Resume the cycle */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void ResumeCycle();

    /** Reset to step 1 */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Control")
    void ResetCycle();

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get the full sys6 state */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    FSys6FullState GetFullState() const;

    /** Get current global step (1-30) */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    int32 GetCurrentStep() const;

    /** Get current dyadic phase (mod 2) */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    EDyadicPhase GetDyadicPhase() const;

    /** Get current triadic phase (mod 3) */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    ETriadicPhase GetTriadicPhase() const;

    /** Get current pentadic stage (1-5) */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    EPentadicStage GetPentadicStage() const;

    /** Get current four-step phase */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    EFourStepPhase GetFourStepPhase() const;

    /** Get cubic concurrency state */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    FCubicConcurrencyState GetCubicState() const;

    /** Get triadic convolution state */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    FTriadicConvolutionState GetConvolutionState() const;

    /** Check if at a sync boundary */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    bool IsAtSyncBoundary() const;

    /** Get sync boundary type (0=none, 1=dyad, 2=triad, 3=pentad, 4=multiple) */
    UFUNCTION(BlueprintPure, Category = "Sys6 State")
    int32 GetSyncBoundaryType() const;

    // ========================================
    // CONCURRENCY OPERATIONS
    // ========================================

    /** Process input through cubic concurrency (8-way parallel) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Concurrency")
    TArray<float> ProcessCubicConcurrency(const TArray<float>& Input);

    /** Process input through triadic convolution (9-phase) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Convolution")
    TArray<float> ProcessTriadicConvolution(const TArray<float>& Input);

    /** Set thread pair for entanglement (order 2 concurrency) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Concurrency")
    void SetEntangledPair(int32 ThreadA, int32 ThreadB);

    /** Rotate convolution kernel */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Convolution")
    void RotateConvolutionKernel();

protected:
    /** Current sys6 state */
    FSys6FullState State;

    /** Step timer */
    float StepTimer = 0.0f;

    /** Is cycle paused */
    bool bIsPaused = false;

    /** Stage configurations */
    TArray<FStageConfig> StageConfigs;

    // Internal methods
    void UpdateDyadicPhase();
    void UpdateTriadicPhase();
    void UpdatePentadicStage();
    void UpdateFourStepPhase();
    void UpdateCubicConcurrency();
    void UpdateTriadicConvolution();
    void CheckAndFireSyncEvents();
    void InitializeStageConfigs();
    
    /** Compute the double-step delay pattern state */
    void ComputeDoubleStepDelayState();
    
    /** Get the state value for current 4-step phase (1, 4, or 6) */
    int32 GetStateValueForPhase(EFourStepPhase Phase) const;
};
