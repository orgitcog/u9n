#pragma once

/**
 * Cognitive Cycle Manager - 12-Step Cognitive Cycle with 3 Concurrent Streams
 * 
 * Implements the echobeats cognitive architecture:
 * - 3 concurrent consciousness streams (perception, action, simulation)
 * - 12-step cycle with streams phased 120° apart (4 steps)
 * - 7 expressive mode steps + 5 reflective mode steps
 * - Triads occurring every 4 steps: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 * 
 * OEIS A000081 Nested Shells:
 * - 1 nest → 1 term (1 step apart)
 * - 2 nests → 2 terms (2 steps apart)
 * - 3 nests → 4 terms (3 steps apart)
 * - 4 nests → 9 terms (4 steps apart)
 * 
 * Step Composition:
 * - 1 pivotal relevance realization step (orienting present commitment)
 * - 5 actual affordance interaction steps (conditioning past performance)
 * - 1 pivotal relevance realization step (orienting present commitment)
 * - 5 virtual salience simulation steps (anticipating future potential)
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CognitiveCycleManager.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Consciousness Stream Type
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
 * Cognitive Mode
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
 * Step Type within Cognitive Cycle
 */
UENUM(BlueprintType)
enum class ECognitiveStepType : uint8
{
    /** Pivotal relevance realization - orienting present commitment */
    RelevanceRealization UMETA(DisplayName = "Relevance Realization"),
    
    /** Actual affordance interaction - conditioning past performance */
    AffordanceInteraction UMETA(DisplayName = "Affordance Interaction"),
    
    /** Virtual salience simulation - anticipating future potential */
    SalienceSimulation UMETA(DisplayName = "Salience Simulation")
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
 * Stream State - State of a single consciousness stream
 */
USTRUCT(BlueprintType)
struct FStreamState
{
    GENERATED_BODY()

    /** Stream type */
    UPROPERTY(BlueprintReadWrite)
    EConsciousnessStream StreamType = EConsciousnessStream::Perception;

    /** Current step within 12-step cycle (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentStep = 1;

    /** Phase offset (0, 4, or 8 for 120° separation) */
    UPROPERTY(BlueprintReadWrite)
    int32 PhaseOffset = 0;

    /** Current cognitive mode */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveMode Mode = ECognitiveMode::Expressive;

    /** Current step type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStepType StepType = ECognitiveStepType::RelevanceRealization;

    /** Stream activation level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 1.0f;

    /** Stream coherence with other streams (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float InterStreamCoherence = 1.0f;

    /** Salience value for current processing */
    UPROPERTY(BlueprintReadWrite)
    float SalienceValue = 0.5f;

    /** Affordance value for current processing */
    UPROPERTY(BlueprintReadWrite)
    float AffordanceValue = 0.5f;
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
 * Cognitive Cycle State
 */
USTRUCT(BlueprintType)
struct FCognitiveCycleState
{
    GENERATED_BODY()

    /** Global step counter (1-12, wraps) */
    UPROPERTY(BlueprintReadWrite)
    int32 GlobalStep = 1;

    /** Total cycles completed */
    UPROPERTY(BlueprintReadWrite)
    int32 CyclesCompleted = 0;

    /** Current cognitive mode */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveMode CurrentMode = ECognitiveMode::Expressive;

    /** Current step type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStepType CurrentStepType = ECognitiveStepType::RelevanceRealization;

    /** Current triad group */
    UPROPERTY(BlueprintReadWrite)
    ETriadGroup CurrentTriad = ETriadGroup::Triad1;

    /** Expressive steps remaining in current cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 ExpressiveStepsRemaining = 7;

    /** Reflective steps remaining in current cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 ReflectiveStepsRemaining = 5;
};

/**
 * Cognitive Cycle Manager Component
 * 
 * Manages the 12-step cognitive cycle with 3 concurrent consciousness streams
 * for the Deep-Tree-Echo framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UCognitiveCycleManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UCognitiveCycleManager();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic cycle advancement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveCycle|Config")
    bool bEnableAutoCycle = true;

    /** Step duration in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveCycle|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float StepDuration = 0.1f;

    /** Enable inter-stream awareness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveCycle|Config")
    bool bEnableInterStreamAwareness = true;

    // ========================================
    // STATE
    // ========================================

    /** Current cycle state */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    FCognitiveCycleState CycleState;

    /** Stream states (3 concurrent streams) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FStreamState> StreamStates;

    /** Nested shell states (4 levels) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FNestedShellState> NestedShells;

    /** Triad states (4 triads) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveCycle|State")
    TArray<FTriadState> TriadStates;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the cognitive cycle manager */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle")
    void InitializeCycle();

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle")
    void ResetCycle();

    // ========================================
    // PUBLIC API - CYCLE CONTROL
    // ========================================

    /** Advance cycle by one step */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Control")
    void AdvanceStep();

    /** Advance cycle by multiple steps */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Control")
    void AdvanceSteps(int32 Steps);

    /** Process current step for all streams */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Control")
    void ProcessCurrentStep();

    // ========================================
    // PUBLIC API - STREAM ACCESS
    // ========================================

    /** Get stream state by type */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Stream")
    FStreamState GetStreamState(EConsciousnessStream StreamType) const;

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

    // ========================================
    // PUBLIC API - TRIAD ACCESS
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
    // PUBLIC API - NESTED SHELLS
    // ========================================

    /** Get nested shell state by level */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Shells")
    FNestedShellState GetNestedShellState(int32 Level) const;

    /** Get total terms across all shells */
    UFUNCTION(BlueprintCallable, Category = "CognitiveCycle|Shells")
    int32 GetTotalNestedTerms() const;

    // ========================================
    // PUBLIC API - INTER-STREAM AWARENESS
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
    // PUBLIC API - STEP TYPE QUERIES
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

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for auto-cycle */
    float AccumulatedTime = 0.0f;

    /** Initialize stream states */
    void InitializeStreams();

    /** Initialize nested shells (OEIS A000081) */
    void InitializeNestedShells();

    /** Initialize triad states */
    void InitializeTriads();

    /** Update stream for current step */
    void UpdateStream(FStreamState& Stream);

    /** Update nested shells for current step */
    void UpdateNestedShells();

    /** Update triad activations */
    void UpdateTriadActivations();

    /** Calculate step type for step number */
    ECognitiveStepType CalculateStepType(int32 Step) const;

    /** Calculate cognitive mode for step number */
    ECognitiveMode CalculateStepMode(int32 Step) const;

    /** Calculate triad group for step number */
    ETriadGroup CalculateTriadGroup(int32 Step) const;

    /** Apply inter-stream feedback */
    void ApplyInterStreamFeedback();
};
