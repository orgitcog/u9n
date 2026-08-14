// EchobeatsStreamEngine.h
// Implementation of 3-phase concurrent cognitive stream architecture
// Based on Kawaii Hexapod System 4 tripod gait pattern

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Core/Sys6OperadEngine.h"
#include "../../Core/Sys6CognitiveBridge.h"
#include "EchobeatsStreamEngine.generated.h"

/**
 * Cognitive stream type (3 concurrent streams)
 */
UENUM(BlueprintType)
enum class ECognitiveStreamType : uint8
{
    Pivotal     UMETA(DisplayName = "Pivotal - Relevance Realization"),
    Affordance  UMETA(DisplayName = "Affordance - Environmental Interaction"),
    Salience    UMETA(DisplayName = "Salience - Virtual Simulation")
};

/**
 * Cognitive mode (Expressive vs Reflective)
 */
UENUM(BlueprintType)
enum class EStreamCognitiveMode : uint8
{
    Expressive  UMETA(DisplayName = "Expressive - Reactive/Action-oriented"),
    Reflective  UMETA(DisplayName = "Reflective - Anticipatory/Simulation-oriented")
};

/**
 * System 4 term type for cognitive processing
 */
UENUM(BlueprintType)
enum class ESystem4Term : uint8
{
    T1_Perception       UMETA(DisplayName = "T1: Perception (Need vs Capacity)"),
    T2_IdeaFormation    UMETA(DisplayName = "T2: Idea Formation"),
    T4_SensoryInput     UMETA(DisplayName = "T4: Sensory Input"),
    T5_ActionSequence   UMETA(DisplayName = "T5: Action Sequence"),
    T7_MemoryEncoding   UMETA(DisplayName = "T7: Memory Encoding"),
    T8_BalancedResponse UMETA(DisplayName = "T8: Balanced Response")
};

/**
 * Echobeat step type
 */
UENUM(BlueprintType)
enum class EEchobeatStepType : uint8
{
    Pivotal     UMETA(DisplayName = "Pivotal - Relevance Realization"),
    Affordance  UMETA(DisplayName = "Affordance - Interaction"),
    Salience    UMETA(DisplayName = "Salience - Simulation")
};

/**
 * Step configuration for 12-step cognitive loop
 */
USTRUCT(BlueprintType)
struct FEchobeatStepConfig
{
    GENERATED_BODY()

    /** Step number (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepNumber = 1;

    /** Step type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEchobeatStepType StepType = EEchobeatStepType::Pivotal;

    /** Cognitive mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EStreamCognitiveMode Mode = EStreamCognitiveMode::Expressive;

    /** Primary stream for this step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveStreamType PrimaryStream = ECognitiveStreamType::Pivotal;

    /** System 4 term */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESystem4Term Term = ESystem4Term::T1_Perception;

    /** Triadic group (0-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TriadicGroup = 0;

    /** Phase ID (0-2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhaseID = 0;
};

/**
 * Cognitive stream state
 */
USTRUCT(BlueprintType)
struct FCognitiveStreamState
{
    GENERATED_BODY()

    /** Stream type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveStreamType StreamType = ECognitiveStreamType::Pivotal;

    /** Current step in stream's 4-step cycle (0-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamStep = 0;

    /** Phase offset (0, 4, or 8 steps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhaseOffset = 0;

    /** Stream activation level (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 0.0f;

    /** Stream coherence with other streams */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StreamCoherence = 1.0f;

    /** Current processing state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> ProcessingState;

    /** Reservoir state for this stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> ReservoirState;
};

/**
 * Tensional coupling between streams
 */
USTRUCT(BlueprintType)
struct FTensionalCoupling
{
    GENERATED_BODY()

    /** Coupling type name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CouplingName;

    /** First term in coupling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESystem4Term TermA = ESystem4Term::T4_SensoryInput;

    /** First term mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EStreamCognitiveMode ModeA = EStreamCognitiveMode::Expressive;

    /** Second term in coupling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESystem4Term TermB = ESystem4Term::T7_MemoryEncoding;

    /** Second term mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EStreamCognitiveMode ModeB = EStreamCognitiveMode::Reflective;

    /** Coupling strength (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    /** Is coupling currently active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = false;
};

/**
 * Full echobeats state
 */
USTRUCT(BlueprintType)
struct FEchobeatsFullState
{
    GENERATED_BODY()

    /** Current step (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStep = 1;

    /** Current step configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEchobeatStepConfig CurrentConfig;

    /** Stream states */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FCognitiveStreamState> StreamStates;

    /** Active tensional couplings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTensionalCoupling> ActiveCouplings;

    /** Overall coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallCoherence = 1.0f;

    /** Relevance realization level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevanceLevel = 0.5f;

    /** Cycle count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CycleCount = 0;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEchobeatStepChanged, int32, OldStep, int32, NewStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStreamActivated, ECognitiveStreamType, StreamType, float, ActivationLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCouplingActivated, FString, CouplingName, float, Strength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelevanceRealized, int32, Step, float, RelevanceLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEchobeatsCycleCompleted, int32, CycleCount);

/**
 * Echobeats Stream Engine
 * 
 * Implements the 3-phase concurrent cognitive stream architecture based on
 * Kawaii Hexapod System 4 tripod gait pattern.
 * 
 * Key features:
 * - 3 concurrent streams (Pivotal, Affordance, Salience) phased 4 steps apart
 * - 12-step cognitive loop with 7 expressive + 5 reflective steps
 * - Tensional couplings (T4E↔T7R, T1R↔T2E, T8E)
 * - Triadic synchronization at steps 4, 8, 12
 * - Integration with sys6 operad architecture
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEchobeatsStreamEngine : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchobeatsStreamEngine();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Duration of one complete 12-step cycle in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    float CycleDuration = 12.0f;

    /** Enable automatic cycle progression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bEnableAutoCycle = true;

    /** Enable tensional coupling detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bEnableTensionalCoupling = true;

    /** Relevance threshold for triggering events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RelevanceThreshold = 0.7f;

    /** Reservoir units per stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config", meta = (ClampMin = "10", ClampMax = "500"))
    int32 ReservoirUnitsPerStream = 100;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnEchobeatStepChanged OnStepChanged;

    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnStreamActivated OnStreamActivated;

    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnCouplingActivated OnCouplingActivated;

    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnRelevanceRealized OnRelevanceRealized;

    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnEchobeatsCycleCompleted OnCycleCompleted;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to sys6 operad engine */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    USys6OperadEngine* Sys6Engine;

    /** Reference to cognitive bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    USys6CognitiveBridge* CognitiveBridge;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the echobeats engine */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeEngine();

    /** Initialize step configurations */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeStepConfigs();

    /** Initialize stream states */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeStreamStates();

    /** Initialize tensional couplings */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeTensionalCouplings();

    // ========================================
    // PUBLIC API - CYCLE CONTROL
    // ========================================

    /** Advance to the next step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void AdvanceStep();

    /** Jump to a specific step (1-12) */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void JumpToStep(int32 Step);

    /** Pause the cycle */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void PauseCycle();

    /** Resume the cycle */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void ResumeCycle();

    /** Reset to step 1 */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void ResetCycle();

    // ========================================
    // PUBLIC API - STREAM PROCESSING
    // ========================================

    /** Process sensory input through streams */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Processing")
    void ProcessSensoryInput(const TArray<float>& SensoryInput);

    /** Get stream output */
    UFUNCTION(BlueprintPure, Category = "Echobeats|Processing")
    TArray<float> GetStreamOutput(ECognitiveStreamType StreamType) const;

    /** Compute relevance realization */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Processing")
    float ComputeRelevanceRealization(const TArray<float>& Input);

    /** Process tensional coupling */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Processing")
    void ProcessTensionalCoupling(const FTensionalCoupling& Coupling);

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get full echobeats state */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    FEchobeatsFullState GetFullState() const;

    /** Get current step (1-12) */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    int32 GetCurrentStep() const;

    /** Get current step configuration */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    FEchobeatStepConfig GetCurrentStepConfig() const;

    /** Get stream state */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    FCognitiveStreamState GetStreamState(ECognitiveStreamType StreamType) const;

    /** Get active couplings */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    TArray<FTensionalCoupling> GetActiveCouplings() const;

    /** Get triadic group for step */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    int32 GetTriadicGroup(int32 Step) const;

    /** Check if at triadic sync point */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    bool IsAtTriadicSyncPoint() const;

    /** Get overall coherence */
    UFUNCTION(BlueprintPure, Category = "Echobeats|State")
    float GetOverallCoherence() const;

protected:
    /** Current echobeats state */
    FEchobeatsFullState State;

    /** Step configurations */
    TArray<FEchobeatStepConfig> StepConfigs;

    /** Tensional coupling definitions */
    TArray<FTensionalCoupling> TensionalCouplings;

    /** Step timer */
    float StepTimer = 0.0f;

    /** Is cycle paused */
    bool bIsPaused = false;

    // Internal methods
    void FindComponentReferences();
    void UpdateStreamStates();
    void DetectActiveCouplings();
    void ComputeOverallCoherence();
    void ProcessPivotalStream(const TArray<float>& Input);
    void ProcessAffordanceStream(const TArray<float>& Input);
    void ProcessSalienceStream(const TArray<float>& Input);
    void SyncWithSys6();

    /** Handle sys6 step advanced */
    UFUNCTION()
    void HandleSys6StepAdvanced(int32 OldStep, int32 NewStep);

    /** Handle bridge sync */
    UFUNCTION()
    void HandleBridgeSync(int32 Sys6Step, int32 CognitiveStep);
};
