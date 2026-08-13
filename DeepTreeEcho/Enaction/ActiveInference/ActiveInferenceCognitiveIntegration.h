// ActiveInferenceCognitiveIntegration.h
// Integration layer connecting Active Inference with Sys6 and Echobeats cognitive cycles
// Implements 4E embodied cognition through active inference and niche construction

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveInferenceCognitiveIntegration.generated.h"

// Forward declarations
class UActiveInferenceEngine;
class UNicheConstructionSystem;
class UEchobeatsStreamEngine;
class USys6LCMClockSynchronizer;
class USys6OperadEngine;
class UTensionalCouplingDynamics;
class UCognitiveMemoryManager;
class UPredictiveAdaptationEngine;

/**
 * 4E Cognition mode
 */
UENUM(BlueprintType)
enum class E4ECognitionMode : uint8
{
    Embodied        UMETA(DisplayName = "Embodied"),
    Embedded        UMETA(DisplayName = "Embedded"),
    Enacted         UMETA(DisplayName = "Enacted"),
    Extended        UMETA(DisplayName = "Extended")
};

/**
 * Cognitive integration phase
 */
UENUM(BlueprintType)
enum class ECognitiveIntegrationPhase : uint8
{
    Perception      UMETA(DisplayName = "Perception"),
    Inference       UMETA(DisplayName = "Inference"),
    Action          UMETA(DisplayName = "Action"),
    NicheConstruct  UMETA(DisplayName = "Niche Construction"),
    Learning        UMETA(DisplayName = "Learning"),
    Integration     UMETA(DisplayName = "Integration")
};

/**
 * Stream synchronization state
 */
USTRUCT(BlueprintType)
struct FStreamSyncState
{
    GENERATED_BODY()

    /** Stream index (0-2) */
    UPROPERTY(BlueprintReadOnly)
    int32 StreamIndex = 0;

    /** Current echobeat step for this stream */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 1;

    /** Current 4E mode */
    UPROPERTY(BlueprintReadOnly)
    E4ECognitionMode Mode4E = E4ECognitionMode::Embodied;

    /** Current integration phase */
    UPROPERTY(BlueprintReadOnly)
    ECognitiveIntegrationPhase Phase = ECognitiveIntegrationPhase::Perception;

    /** Active inference free energy */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergy = 0.0f;

    /** Niche fitness */
    UPROPERTY(BlueprintReadOnly)
    float NicheFitness = 0.5f;

    /** Stream coherence with others */
    UPROPERTY(BlueprintReadOnly)
    float StreamCoherence = 1.0f;
};

/**
 * Integrated cognitive state
 */
USTRUCT(BlueprintType)
struct FIntegratedCognitiveState
{
    GENERATED_BODY()

    /** Current LCM step (0-29) */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Current echobeat step (1-12) */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 1;

    /** Dyad state (A or B) */
    UPROPERTY(BlueprintReadOnly)
    FString DyadState = TEXT("A");

    /** Triad state (1, 2, or 3) */
    UPROPERTY(BlueprintReadOnly)
    int32 TriadState = 1;

    /** Stream states */
    UPROPERTY(BlueprintReadOnly)
    TArray<FStreamSyncState> StreamStates;

    /** Global free energy */
    UPROPERTY(BlueprintReadOnly)
    float GlobalFreeEnergy = 0.0f;

    /** Global niche fitness */
    UPROPERTY(BlueprintReadOnly)
    float GlobalNicheFitness = 0.5f;

    /** Inter-stream coherence */
    UPROPERTY(BlueprintReadOnly)
    float InterStreamCoherence = 1.0f;

    /** Active 4E mode */
    UPROPERTY(BlueprintReadOnly)
    E4ECognitionMode Active4EMode = E4ECognitionMode::Embodied;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * 4E cognition metrics
 */
USTRUCT(BlueprintType)
struct F4ECognitionMetrics
{
    GENERATED_BODY()

    /** Embodiment index (body-world coupling) */
    UPROPERTY(BlueprintReadOnly)
    float EmbodimentIndex = 0.5f;

    /** Embeddedness index (environmental coupling) */
    UPROPERTY(BlueprintReadOnly)
    float EmbeddednessIndex = 0.5f;

    /** Enaction index (sensorimotor contingencies) */
    UPROPERTY(BlueprintReadOnly)
    float EnactionIndex = 0.5f;

    /** Extension index (cognitive scaffolding) */
    UPROPERTY(BlueprintReadOnly)
    float ExtensionIndex = 0.5f;

    /** Overall 4E integration */
    UPROPERTY(BlueprintReadOnly)
    float OverallIntegration = 0.5f;

    /** Markov blanket integrity */
    UPROPERTY(BlueprintReadOnly)
    float MarkovBlanketIntegrity = 1.0f;

    /** Niche construction activity */
    UPROPERTY(BlueprintReadOnly)
    float NicheConstructionActivity = 0.0f;
};

/**
 * Cognitive cycle event
 */
USTRUCT(BlueprintType)
struct FCognitiveCycleEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadOnly)
    int32 EventID = 0;

    /** LCM step */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Echobeat step */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 1;

    /** Phase */
    UPROPERTY(BlueprintReadOnly)
    ECognitiveIntegrationPhase Phase = ECognitiveIntegrationPhase::Perception;

    /** 4E mode */
    UPROPERTY(BlueprintReadOnly)
    E4ECognitionMode Mode4E = E4ECognitionMode::Embodied;

    /** Free energy before */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergyBefore = 0.0f;

    /** Free energy after */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergyAfter = 0.0f;

    /** Action taken */
    UPROPERTY(BlueprintReadOnly)
    int32 ActionTaken = -1;

    /** Niche modification */
    UPROPERTY(BlueprintReadOnly)
    bool bNicheModified = false;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveStateChanged, const FIntegratedCognitiveState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EModeChanged, E4ECognitionMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntegrationPhaseChanged, ECognitiveIntegrationPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveCycleEvent, const FCognitiveCycleEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EMetricsUpdated, const F4ECognitionMetrics&, Metrics);

/**
 * Active Inference Cognitive Integration
 * 
 * Integrates the Active Inference Engine and Niche Construction System
 * with the Sys6 LCM clock and Echobeats cognitive cycle.
 * 
 * Implements 4E embodied cognition:
 * - Embodied: Cognition shaped by body morphology and sensorimotor capabilities
 * - Embedded: Cognition exploits environmental structure
 * - Enacted: Cognition emerges from sensorimotor interaction
 * - Extended: Cognition extends into environment through tools and scaffolds
 * 
 * Key features:
 * - Synchronizes active inference with 30-step LCM clock
 * - Coordinates 3 concurrent cognitive streams
 * - Maps 4E modes to echobeat phases
 * - Integrates niche construction with cognitive cycle
 * - Maintains coherence across streams and components
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UActiveInferenceCognitiveIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UActiveInferenceCognitiveIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable 4E cognition integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|4E")
    bool bEnable4ECognition = true;

    /** 4E mode transition smoothness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|4E")
    float ModeTransitionSmoothing = 0.1f;

    /** Inter-stream coherence threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Streams")
    float CoherenceThreshold = 0.5f;

    /** Free energy integration weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Integration")
    float FreeEnergyWeight = 1.0f;

    /** Niche fitness integration weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Integration")
    float NicheFitnessWeight = 0.5f;

    /** Enable automatic niche construction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Niche")
    bool bAutoNicheConstruction = true;

    /** Niche construction threshold (free energy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Niche")
    float NicheConstructionThreshold = 0.5f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveStateChanged OnCognitiveStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOn4EModeChanged On4EModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnIntegrationPhaseChanged OnIntegrationPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveCycleEvent OnCognitiveCycleEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOn4EMetricsUpdated On4EMetricsUpdated;

    // ========================================
    // PUBLIC API - COGNITIVE STATE
    // ========================================

    /** Get integrated cognitive state */
    UFUNCTION(BlueprintPure, Category = "State")
    FIntegratedCognitiveState GetIntegratedState() const;

    /** Get stream sync state */
    UFUNCTION(BlueprintPure, Category = "State")
    FStreamSyncState GetStreamState(int32 StreamIndex) const;

    /** Get current 4E mode */
    UFUNCTION(BlueprintPure, Category = "State")
    E4ECognitionMode GetCurrent4EMode() const;

    /** Get current integration phase */
    UFUNCTION(BlueprintPure, Category = "State")
    ECognitiveIntegrationPhase GetCurrentPhase() const;

    /** Get global free energy */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetGlobalFreeEnergy() const;

    /** Get global niche fitness */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetGlobalNicheFitness() const;

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Get 4E cognition metrics */
    UFUNCTION(BlueprintPure, Category = "4E")
    F4ECognitionMetrics Get4EMetrics() const;

    /** Set 4E mode manually */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Set4EMode(E4ECognitionMode Mode);

    /** Get 4E mode for echobeat step */
    UFUNCTION(BlueprintPure, Category = "4E")
    E4ECognitionMode Get4EModeForStep(int32 EchobeatStep) const;

    /** Compute embodiment index */
    UFUNCTION(BlueprintPure, Category = "4E")
    float ComputeEmbodimentIndex() const;

    /** Compute embeddedness index */
    UFUNCTION(BlueprintPure, Category = "4E")
    float ComputeEmbeddednessIndex() const;

    /** Compute enaction index */
    UFUNCTION(BlueprintPure, Category = "4E")
    float ComputeEnactionIndex() const;

    /** Compute extension index */
    UFUNCTION(BlueprintPure, Category = "4E")
    float ComputeExtensionIndex() const;

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void ProcessCognitiveStep(int32 LCMStep);

    /** Run perception phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void RunPerceptionPhase(const TArray<float>& Observations);

    /** Run inference phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void RunInferencePhase();

    /** Run action phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    int32 RunActionPhase();

    /** Run niche construction phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void RunNicheConstructionPhase();

    /** Run learning phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void RunLearningPhase();

    /** Run integration phase */
    UFUNCTION(BlueprintCallable, Category = "Cycle")
    void RunIntegrationPhase();

    // ========================================
    // PUBLIC API - STREAM COORDINATION
    // ========================================

    /** Synchronize streams */
    UFUNCTION(BlueprintCallable, Category = "Streams")
    void SynchronizeStreams();

    /** Get inter-stream coherence */
    UFUNCTION(BlueprintPure, Category = "Streams")
    float GetInterStreamCoherence() const;

    /** Compute stream coherence */
    UFUNCTION(BlueprintCallable, Category = "Streams")
    float ComputeStreamCoherence(int32 Stream1, int32 Stream2) const;

    /** Balance stream tensions */
    UFUNCTION(BlueprintCallable, Category = "Streams")
    void BalanceStreamTensions();

    // ========================================
    // PUBLIC API - ACTIVE INFERENCE INTEGRATION
    // ========================================

    /** Trigger active inference cycle */
    UFUNCTION(BlueprintCallable, Category = "ActiveInference")
    void TriggerActiveInferenceCycle();

    /** Get expected free energy for action */
    UFUNCTION(BlueprintPure, Category = "ActiveInference")
    float GetExpectedFreeEnergy(int32 ActionIndex) const;

    /** Update beliefs from observations */
    UFUNCTION(BlueprintCallable, Category = "ActiveInference")
    void UpdateBeliefsFromObservations(const TArray<float>& Observations);

    /** Select action via active inference */
    UFUNCTION(BlueprintCallable, Category = "ActiveInference")
    int32 SelectActionViaActiveInference();

    // ========================================
    // PUBLIC API - NICHE INTEGRATION
    // ========================================

    /** Trigger niche construction */
    UFUNCTION(BlueprintCallable, Category = "Niche")
    void TriggerNicheConstruction();

    /** Evaluate niche construction opportunity */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float EvaluateNicheConstructionOpportunity() const;

    /** Get niche-cognition coupling */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float GetNicheCognitionCoupling() const;

    /** Update niche from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Niche")
    void UpdateNicheFromCognitiveState();

protected:
    // Component references
    UPROPERTY()
    UActiveInferenceEngine* ActiveInferenceEngine;

    UPROPERTY()
    UNicheConstructionSystem* NicheSystem;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USys6LCMClockSynchronizer* LCMClock;

    UPROPERTY()
    USys6OperadEngine* OperadEngine;

    UPROPERTY()
    UTensionalCouplingDynamics* TensionalCoupling;

    UPROPERTY()
    UCognitiveMemoryManager* MemoryManager;

    UPROPERTY()
    UPredictiveAdaptationEngine* PredictiveEngine;

    // Internal state
    FIntegratedCognitiveState CurrentState;
    F4ECognitionMetrics Current4EMetrics;
    TArray<FCognitiveCycleEvent> CycleHistory;

    E4ECognitionMode Current4EMode = E4ECognitionMode::Embodied;
    ECognitiveIntegrationPhase CurrentPhase = ECognitiveIntegrationPhase::Perception;

    int32 NextEventID = 1;
    float LastCycleTime = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void InitializeState();
    void UpdateStreamStates();
    void Update4EMetrics();
    void TransitionPhase(ECognitiveIntegrationPhase NewPhase);
    void Transition4EMode(E4ECognitionMode NewMode);
    ECognitiveIntegrationPhase GetPhaseForStep(int32 EchobeatStep) const;
    void RecordCycleEvent(ECognitiveIntegrationPhase Phase, float FEBefore, float FEAfter, int32 Action, bool bNicheModified);
    float ComputeGlobalFreeEnergy() const;
    float ComputeGlobalNicheFitness() const;
};
