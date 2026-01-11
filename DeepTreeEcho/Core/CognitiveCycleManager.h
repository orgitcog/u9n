// CognitiveCycleManager.h
// Central orchestration of the 12-step cognitive cycle with 3 concurrent streams
// Implements the echobeats architecture with sys6 triality integration
//
// MERGED VERSION: Contains all functionality from both Core and Cognitive versions
//
// Cognitive Architecture:
// - 3 concurrent consciousness streams (perception, action, simulation)
// - 12-step cycle with streams phased 120° apart (4 steps)
// - 7 expressive mode steps + 5 reflective mode steps
// - Triads occurring every 4 steps: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
//
// OEIS A000081 Nested Shells:
// - 1 nest → 1 term (1 step apart)
// - 2 nests → 2 terms (2 steps apart)
// - 3 nests → 4 terms (3 steps apart)
// - 4 nests → 9 terms (4 steps apart)
//
// Step Composition:
// - 1 pivotal relevance realization step (orienting present commitment)
// - 5 actual affordance interaction steps (conditioning past performance)
// - 1 pivotal relevance realization step (orienting present commitment)
// - 5 virtual salience simulation steps (anticipating future potential)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CognitiveCycleManager.generated.h"

// Forward declarations
class UDeepTreeEchoReservoir;
class UWisdomCultivation;
class UEmbodied4ECognition;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Cognitive mode enumeration
 */
UENUM(BlueprintType)
enum class ECognitiveModeType : uint8
{
    Expressive      UMETA(DisplayName = "Expressive"),
    Reflective      UMETA(DisplayName = "Reflective")
};

/**
 * Legacy cognitive mode (for backward compatibility)
 */
UENUM(BlueprintType)
enum class ECognitiveMode : uint8
{
    /** Expressive mode - outward-directed processing */
    Expressive UMETA(DisplayName = "Expressive"),
    
    /** Reflective mode - inward-directed processing */
    Reflective UMETA(DisplayName = "Reflective")
};

/**
 * Cognitive step type enumeration (detailed)
 */
UENUM(BlueprintType)
enum class ECognitiveStepType : uint8
{
    Perceive        UMETA(DisplayName = "Perceive"),
    Orient          UMETA(DisplayName = "Orient"),
    Reflect         UMETA(DisplayName = "Reflect"),
    Integrate       UMETA(DisplayName = "Integrate"),
    Decide          UMETA(DisplayName = "Decide"),
    Simulate        UMETA(DisplayName = "Simulate"),
    Act             UMETA(DisplayName = "Act"),
    Observe         UMETA(DisplayName = "Observe"),
    Learn           UMETA(DisplayName = "Learn"),
    Consolidate     UMETA(DisplayName = "Consolidate"),
    Anticipate      UMETA(DisplayName = "Anticipate"),
    Transcend       UMETA(DisplayName = "Transcend"),
    // Legacy step types
    RelevanceRealization UMETA(DisplayName = "Relevance Realization"),
    AffordanceInteraction UMETA(DisplayName = "Affordance Interaction"),
    SalienceSimulation UMETA(DisplayName = "Salience Simulation")
};

/**
 * Stream role enumeration
 */
UENUM(BlueprintType)
enum class EStreamRole : uint8
{
    Perceiving      UMETA(DisplayName = "Perceiving"),
    Acting          UMETA(DisplayName = "Acting"),
    Reflecting      UMETA(DisplayName = "Reflecting")
};

/**
 * Consciousness Stream Type (legacy)
 */
UENUM(BlueprintType)
enum class EConsciousnessStream : uint8
{
    /** Stream 1: Perception - sensing and interpreting */
    Perception UMETA(DisplayName = "Perception"),
    
    /** Stream 2: Action - executing and expressing */
    Action UMETA(DisplayName = "Action"),
    
    /** Stream 3: Simulation - predicting and imagining */
    Simulation UMETA(DisplayName = "Simulation")
};

/**
 * Triad Group (steps grouped by 4)
 */
UENUM(BlueprintType)
enum class ETriadGroup : uint8
{
    /** Triad 1: Steps {1, 5, 9} */
    Triad1 UMETA(DisplayName = "Triad 1 (1,5,9)"),
    
    /** Triad 2: Steps {2, 6, 10} */
    Triad2 UMETA(DisplayName = "Triad 2 (2,6,10)"),
    
    /** Triad 3: Steps {3, 7, 11} */
    Triad3 UMETA(DisplayName = "Triad 3 (3,7,11)"),
    
    /** Triad 4: Steps {4, 8, 12} */
    Triad4 UMETA(DisplayName = "Triad 4 (4,8,12)")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Configuration for a single cognitive step
 */
USTRUCT(BlueprintType)
struct FCognitiveStepConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveStepType StepType = ECognitiveStepType::Perceive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveModeType Mode = ECognitiveModeType::Expressive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PrimaryStreamID = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPivotalStep = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NestingLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TriadicGroup = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;
};

/**
 * State of a consciousness stream
 */
USTRUCT(BlueprintType)
struct FStreamState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EStreamRole Role = EStreamRole::Perceiving;

    /** Stream type (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConsciousnessStream StreamType = EConsciousnessStream::Perception;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentPhase = 1;

    /** Current step within 12-step cycle (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStep = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhaseOffset = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Stream coherence with other streams (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InterStreamCoherence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> ReservoirState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentFocus;

    /** Current cognitive mode */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveMode Mode = ECognitiveMode::Expressive;

    /** Current step type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStepType StepType = ECognitiveStepType::RelevanceRealization;

    /** Salience value for current processing */
    UPROPERTY(BlueprintReadWrite)
    float SalienceValue = 0.5f;

    /** Affordance value for current processing */
    UPROPERTY(BlueprintReadWrite)
    float AffordanceValue = 0.5f;
};

/**
 * Triadic synchronization state
 */
USTRUCT(BlueprintType)
struct FTriadicSyncState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TriadIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> TriadSteps; // e.g., {1, 5, 9}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SyncQuality = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhaseAlignment = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> CoherenceMatrix; // 3x3 matrix flattened
};

/**
 * Nested Shell State - OEIS A000081 structure
 */
USTRUCT(BlueprintType)
struct FNestedShellState
{
    GENERATED_BODY()

    /** Nesting level (1-4) */
    UPROPERTY(BlueprintReadWrite)
    int32 NestingLevel = 1;

    /** Number of terms at this level */
    UPROPERTY(BlueprintReadWrite)
    int32 TermCount = 1;

    /** Steps apart for this nesting */
    UPROPERTY(BlueprintReadWrite)
    int32 StepsApart = 1;

    /** Current term values */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TermValues;

    /** Shell coherence */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;
};

/**
 * Triad State - State of a triad group
 */
USTRUCT(BlueprintType)
struct FTriadState
{
    GENERATED_BODY()

    /** Triad group */
    UPROPERTY(BlueprintReadWrite)
    ETriadGroup Group = ETriadGroup::Triad1;

    /** Steps in this triad */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> Steps;

    /** Triad activation */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Triad coherence */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;
};

/**
 * Sys6 triality state for 30-step cycle
 */
USTRUCT(BlueprintType)
struct FSys6TrialityState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStep = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentPhase = 1; // 1-3

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStage = 1; // 1-5

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DyadState = 0; // 0=A, 1=B

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TriadState = 1; // 1-3

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> ActiveThreads;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntanglementLevel = 0.0f;
};

/**
 * Overall cognitive cycle state
 */
USTRUCT(BlueprintType)
struct FCognitiveCycleState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStep = 1;

    /** Global step counter (1-12, wraps) - alias for CurrentStep */
    UPROPERTY(BlueprintReadWrite)
    int32 GlobalStep = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveModeType CurrentMode = ECognitiveModeType::Expressive;

    /** Current step type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStepType CurrentStepType = ECognitiveStepType::RelevanceRealization;

    /** Current triad group */
    UPROPERTY(BlueprintReadWrite)
    ETriadGroup CurrentTriad = ETriadGroup::Triad1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CycleCount = 0;

    /** Total cycles completed */
    UPROPERTY(BlueprintReadWrite)
    int32 CyclesCompleted = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CycleProgress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FStreamState> Streams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTriadicSyncState TriadicSync;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSys6TrialityState Sys6State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallCoherence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevanceRealizationLevel = 0.0f;

    /** Expressive steps remaining in current cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 ExpressiveStepsRemaining = 7;

    /** Reflective steps remaining in current cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 ReflectiveStepsRemaining = 5;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCycleStepChanged, int32, OldStep, int32, NewStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModeChanged, ECognitiveModeType, OldMode, ECognitiveModeType, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPivotalStepReached, int32, Step);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTriadicSync, int32, TriadIndex, float, SyncQuality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCycleCompleted, int32, CycleCount);

/**
 * Central manager for the 12-step cognitive cycle
 * Orchestrates 3 concurrent consciousness streams with triadic synchronization
 * 
 * MERGED VERSION: Contains all functionality from both Core and Cognitive implementations
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UCognitiveCycleManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UCognitiveCycleManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Duration of one complete 12-step cycle in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle Configuration")
    float CycleDuration = 12.0f;

    /** Step duration in seconds (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveCycle|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float StepDuration = 0.1f;

    /** Enable automatic cycle progression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle Configuration")
    bool bEnableAutoCycle = true;

    /** Enable triadic synchronization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle Configuration")
    bool bEnableTriadicSync = true;

    /** Enable sys6 triality integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle Configuration")
    bool bEnableSys6Triality = true;

    /** Enable inter-stream awareness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveCycle|Config")
    bool bEnableInterStreamAwareness = true;

    /** Strength of inter-stream coupling at sync points */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle Configuration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StreamCouplingStrength = 0.3f;

    // ========================================
    // STATE (Public for legacy compatibility)
    // ========================================

    /** Stream states (3 concurrent streams) - legacy access */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FStreamState> StreamStates;

    /** Nested shell states (4 levels) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FNestedShellState> NestedShells;

    /** Triad states (4 triads) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FTriadState> TriadStates;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCycleStepChanged OnStepChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnModeChanged OnModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPivotalStepReached OnPivotalStep;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTriadicSync OnTriadicSync;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCycleCompleted OnCycleCompleted;

    // ========================================
    // CYCLE CONTROL
    // ========================================

    /** Initialize the cognitive cycle */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void InitializeCycle();

    /** Advance to the next step */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void AdvanceStep();

    /** Advance cycle by multiple steps */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Control")
    void AdvanceSteps(int32 Steps);

    /** Process current step for all streams */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Control")
    void ProcessCurrentStep();

    /** Jump to a specific step */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void JumpToStep(int32 Step);

    /** Pause the cycle */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void PauseCycle();

    /** Resume the cycle */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void ResumeCycle();

    /** Reset the cycle to step 1 */
    UFUNCTION(BlueprintCallable, Category = "Cycle Control")
    void ResetCycle();

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get the current step number (1-12) */
    UFUNCTION(BlueprintPure, Category = "State")
    int32 GetCurrentStep() const;

    /** Get the current cognitive mode */
    UFUNCTION(BlueprintPure, Category = "State")
    ECognitiveModeType GetCurrentMode() const;

    /** Get the configuration for the current step */
    UFUNCTION(BlueprintPure, Category = "State")
    FCognitiveStepConfig GetCurrentStepConfig() const;

    /** Get the configuration for a specific step */
    UFUNCTION(BlueprintPure, Category = "State")
    FCognitiveStepConfig GetStepConfig(int32 Step) const;

    /** Get the full cycle state */
    UFUNCTION(BlueprintPure, Category = "State")
    FCognitiveCycleState GetCycleState() const;

    /** Check if current step is a pivotal step */
    UFUNCTION(BlueprintPure, Category = "State")
    bool IsCurrentStepPivotal() const;

    /** Check if we're at a triadic sync point */
    UFUNCTION(BlueprintPure, Category = "State")
    bool IsTriadicSyncPoint() const;

    /** Get the current triadic group (0-3) */
    UFUNCTION(BlueprintPure, Category = "State")
    int32 GetCurrentTriadicGroup() const;

    // ========================================
    // STREAM MANAGEMENT
    // ========================================

    /** Get the state of a specific stream by ID */
    UFUNCTION(BlueprintPure, Category = "Streams")
    FStreamState GetStreamState(int32 StreamID) const;

    /** Get stream state by type (legacy) */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    FStreamState GetStreamStateByType(EConsciousnessStream StreamType) const;

    /** Get current step for stream */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    int32 GetStreamStep(EConsciousnessStream StreamType) const;

    /** Get stream activation level */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    float GetStreamActivation(EConsciousnessStream StreamType) const;

    /** Set stream salience value */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    void SetStreamSalience(EConsciousnessStream StreamType, float Salience);

    /** Set stream affordance value */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    void SetStreamAffordance(EConsciousnessStream StreamType, float Affordance);

    /** Get the active stream for the current step */
    UFUNCTION(BlueprintPure, Category = "Streams")
    int32 GetActiveStreamID() const;

    /** Update a stream's focus */
    UFUNCTION(BlueprintCallable, Category = "Streams")
    void SetStreamFocus(int32 StreamID, const FString& Focus);

    /** Get inter-stream coherence */
    UFUNCTION(BlueprintPure, Category = "Streams")
    float GetInterStreamCoherence() const;

    /** Force synchronization of all streams */
    UFUNCTION(BlueprintCallable, Category = "Streams")
    void ForceSynchronize();

    // ========================================
    // TRIAD ACCESS
    // ========================================

    /** Get current triad group */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Triad")
    ETriadGroup GetCurrentTriad() const;

    /** Get triad state */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Triad")
    FTriadState GetTriadState(ETriadGroup Group) const;

    /** Check if step is in triad */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Triad")
    bool IsStepInTriad(int32 Step, ETriadGroup Group) const;

    // ========================================
    // NESTED SHELLS (OEIS A000081)
    // ========================================

    /** Get nested shell state by level */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Shells")
    FNestedShellState GetNestedShellState(int32 Level) const;

    /** Get total terms across all shells */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Shells")
    int32 GetTotalNestedTerms() const;

    // ========================================
    // INTER-STREAM AWARENESS
    // ========================================

    /** Get what stream 1 perceives of stream 2's action */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Awareness")
    float GetPerceptionOfAction() const;

    /** Get what stream 3 simulates of stream 2's action */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Awareness")
    float GetSimulationOfAction() const;

    /** Calculate inter-stream coherence */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Awareness")
    float CalculateInterStreamCoherence() const;

    // ========================================
    // STEP TYPE QUERIES
    // ========================================

    /** Get step type for given step number */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|StepType")
    ECognitiveStepType GetStepType(int32 Step) const;

    /** Get cognitive mode for given step number */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|StepType")
    ECognitiveMode GetStepMode(int32 Step) const;

    /** Check if step is relevance realization step */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|StepType")
    bool IsRelevanceRealizationStep(int32 Step) const;

    // ========================================
    // SYS6 TRIALITY
    // ========================================

    /** Get the current sys6 state */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    FSys6TrialityState GetSys6State() const;

    /** Get the current dyad state (A or B) */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    FString GetCurrentDyadState() const;

    /** Get the current triad state (1, 2, or 3) */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    int32 GetCurrentTriadState() const;

    /** Get the entanglement level */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    float GetEntanglementLevel() const;

    // ========================================
    // INTEGRATION
    // ========================================

    /** Process input through the current step */
    UFUNCTION(BlueprintCallable, Category = "Processing")
    TArray<float> ProcessInput(const TArray<float>& Input);

    /** Get the relevance realization level */
    UFUNCTION(BlueprintPure, Category = "Processing")
    float GetRelevanceRealizationLevel() const;

protected:
    // Component references
    UPROPERTY()
    UDeepTreeEchoReservoir* ReservoirComponent;

    UPROPERTY()
    UWisdomCultivation* WisdomComponent;

    UPROPERTY()
    UEmbodied4ECognition* Embodied4EComponent;

    // Internal state
    FCognitiveCycleState CycleState;
    TArray<FCognitiveStepConfig> StepConfigs;

    float StepTimer = 0.0f;
    float Sys6Timer = 0.0f;
    float AccumulatedTime = 0.0f;
    bool bIsPaused = false;

    // Internal methods - Core version
    void InitializeStepConfigs();
    void InitializeStreams();
    void InitializeSys6State();

    void UpdateCycleTimer(float DeltaTime);
    void UpdateStreams(float DeltaTime);
    void UpdateSys6State(float DeltaTime);

    void ProcessStepTransition(int32 OldStep, int32 NewStep);
    void ProcessTriadicSync();
    void ProcessModeTransition(ECognitiveModeType OldMode, ECognitiveModeType NewMode);

    void SynchronizeStreamsAtTriad();
    void UpdateStreamCoherence();
    void PropagateActivation();

    void UpdateSys6DoubleStepPattern();
    void UpdateThreadMultiplexing();

    float ComputeStreamCoherence(int32 StreamA, int32 StreamB) const;
    float ComputeOverallCoherence() const;
    float ComputeRelevanceRealization() const;

    void FindComponentReferences();

    // Internal methods - Legacy version
    void InitializeNestedShells();
    void InitializeTriads();

    void UpdateStream(FStreamState& Stream);
    void UpdateNestedShells();
    void UpdateTriadActivations();

    ECognitiveStepType CalculateStepType(int32 Step) const;
    ECognitiveMode CalculateStepMode(int32 Step) const;
    ETriadGroup CalculateTriadGroup(int32 Step) const;

    void ApplyInterStreamFeedback();
};
