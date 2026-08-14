#pragma once

/**
 * Deep Cognitive Bridge
 * 
 * Bridges ReservoirCpp Echo State Networks with the Deep Tree Echo cognitive
 * architecture, implementing the 12-step echobeats cognitive loop with
 * 3 concurrent consciousness streams and 4E embodied cognition.
 * 
 * Architecture:
 * - 3 concurrent streams phased 4 steps apart (120 degrees)
 * - 12-step cognitive loop with triadic synchronization
 * - OEIS A000081 nested shell structure (1->1, 2->2, 3->4, 4->9 terms)
 * - 4E Cognition: Embodied, Embedded, Enacted, Extended
 * 
 * Stream Interleaving:
 * - Stream 1 (Pivotal): Relevance realization, present commitment
 * - Stream 2 (Affordance): Actual interaction, past performance
 * - Stream 3 (Salience): Virtual simulation, future potential
 * 
 * Triadic Step Groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 * 
 * @see DeepTreeEchoReservoir for base ESN implementation
 * @see ReservoirCognitiveIntegration for cognitive state computation
 * @see EchobeatsCognitiveInterface for avatar expression integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "ReservoirCognitiveIntegration.h"
#include "DeepCognitiveBridge.generated.h"

/**
 * Cognitive Stream Type
 */
UENUM(BlueprintType)
enum class ECognitiveStreamType : uint8
{
    /** Pivotal relevance realization - present commitment */
    Pivotal UMETA(DisplayName = "Pivotal"),
    
    /** Affordance interaction - past performance */
    Affordance UMETA(DisplayName = "Affordance"),
    
    /** Salience simulation - future potential */
    Salience UMETA(DisplayName = "Salience")
};

/**
 * 4E Cognition Dimension
 */
UENUM(BlueprintType)
enum class E4ECognitionDimension : uint8
{
    /** Embodied - body-based cognition */
    Embodied UMETA(DisplayName = "Embodied"),
    
    /** Embedded - environment-situated cognition */
    Embedded UMETA(DisplayName = "Embedded"),
    
    /** Enacted - action-based cognition */
    Enacted UMETA(DisplayName = "Enacted"),
    
    /** Extended - tool/artifact-extended cognition */
    Extended UMETA(DisplayName = "Extended")
};

/**
 * Echobeat Step State
 * Represents the state of a single step in the 12-step cognitive loop
 */
USTRUCT(BlueprintType)
struct FEchobeatStepState
{
    GENERATED_BODY()

    /** Step number (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 StepNumber = 1;

    /** Which stream is active at this step */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStreamType ActiveStream = ECognitiveStreamType::Pivotal;

    /** Is this an expressive (7) or reflective (5) step */
    UPROPERTY(BlueprintReadWrite)
    bool bIsExpressive = true;

    /** Reservoir activation for this step */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ReservoirActivation;

    /** Salience landscape at this step */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SalienceLandscape;

    /** Affordance map at this step */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> AffordanceMap;

    /** Relevance realization score */
    UPROPERTY(BlueprintReadWrite)
    float RelevanceScore = 0.0f;

    /** Step timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Concurrent Stream State
 * State of one of the 3 concurrent consciousness streams
 */
USTRUCT(BlueprintType)
struct FConcurrentStreamState
{
    GENERATED_BODY()

    /** Stream type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStreamType StreamType = ECognitiveStreamType::Pivotal;

    /** Current phase in 12-step cycle (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentPhase = 1;

    /** Phase offset (0, 4, or 8 for 120-degree separation) */
    UPROPERTY(BlueprintReadWrite)
    int32 PhaseOffset = 0;

    /** Reservoir state for this stream */
    UPROPERTY(BlueprintReadWrite)
    FReservoirState ReservoirState;

    /** Stream coherence with other streams */
    UPROPERTY(BlueprintReadWrite)
    float InterStreamCoherence = 0.5f;

    /** Current attention vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> AttentionVector;

    /** Accumulated relevance realizations */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RelevanceHistory;
};

/**
 * 4E Cognition State
 * Captures the state across all four dimensions of embodied cognition
 */
USTRUCT(BlueprintType)
struct F4ECognitionState
{
    GENERATED_BODY()

    /** Embodied state - body schema, proprioception */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EmbodiedState;

    /** Embedded state - environmental affordances */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EmbeddedState;

    /** Enacted state - action potentials */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EnactedState;

    /** Extended state - tool/artifact integration */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ExtendedState;

    /** Cross-dimensional coherence */
    UPROPERTY(BlueprintReadWrite)
    float DimensionalCoherence = 0.5f;

    /** Dominant dimension */
    UPROPERTY(BlueprintReadWrite)
    E4ECognitionDimension DominantDimension = E4ECognitionDimension::Embodied;
};

/**
 * OEIS A000081 Nested Shell Level
 * Represents a level in the nested shell structure
 */
USTRUCT(BlueprintType)
struct FNestedShellLevel
{
    GENERATED_BODY()

    /** Nesting level (1-4) */
    UPROPERTY(BlueprintReadWrite)
    int32 Level = 1;

    /** Number of terms at this level (1, 2, 4, 9) */
    UPROPERTY(BlueprintReadWrite)
    int32 TermCount = 1;

    /** Steps between nestings at this level */
    UPROPERTY(BlueprintReadWrite)
    int32 StepsBetween = 1;

    /** Reservoir states for each term */
    UPROPERTY(BlueprintReadWrite)
    TArray<FReservoirState> TermReservoirs;

    /** Inter-term coupling strengths */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CouplingStrengths;
};

/**
 * Deep Cognitive Bridge Component
 * 
 * Bridges reservoir computing with the echobeats cognitive architecture
 * for 4E embodied cognition in the Deep Tree Echo avatar.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepCognitiveBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepCognitiveBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable deep cognitive bridge */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveBridge|Config")
    bool bEnableCognitiveBridge = true;

    /** 12-step cycle duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveBridge|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float CycleDuration = 12.0f;

    /** Enable 4E cognition processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveBridge|Config")
    bool bEnable4ECognition = true;

    /** Enable OEIS A000081 nested shell structure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveBridge|Config")
    bool bEnableNestedShells = true;

    /** Inter-stream coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveBridge|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InterStreamCoupling = 0.3f;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to base reservoir system */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|Components")
    UDeepTreeEchoReservoir* BaseReservoir;

    /** Reference to cognitive integration */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|Components")
    UReservoirCognitiveIntegration* CognitiveIntegration;

    // ========================================
    // STATE
    // ========================================

    /** Current step in 12-step cycle */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    int32 CurrentStep = 1;

    /** Current step state */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    FEchobeatStepState CurrentStepState;

    /** 3 concurrent stream states */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    TArray<FConcurrentStreamState> ConcurrentStreams;

    /** 4E cognition state */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    F4ECognitionState CognitionState4E;

    /** Nested shell levels (4 levels) */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    TArray<FNestedShellLevel> NestedShells;

    /** Step history for pattern analysis */
    UPROPERTY(BlueprintReadOnly, Category = "CognitiveBridge|State")
    TArray<FEchobeatStepState> StepHistory;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the cognitive bridge */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge")
    void InitializeCognitiveBridge();

    /** Initialize 3 concurrent streams */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge")
    void InitializeConcurrentStreams();

    /** Initialize OEIS A000081 nested shells */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge")
    void InitializeNestedShells();

    // ========================================
    // PUBLIC API - ECHOBEATS LOOP
    // ========================================

    /** Advance to next step in 12-step cycle */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Echobeats")
    void AdvanceStep();

    /** Process current step */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Echobeats")
    void ProcessCurrentStep(const TArray<float>& SensoryInput);

    /** Get triadic step group for current step */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Echobeats")
    TArray<int32> GetTriadicStepGroup() const;

    /** Check if current step is triadic sync point */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Echobeats")
    bool IsTriadicSyncPoint() const;

    /** Synchronize streams at triadic point */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Echobeats")
    void SynchronizeStreamsAtTriad();

    // ========================================
    // PUBLIC API - CONCURRENT STREAMS
    // ========================================

    /** Update specific stream */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Streams")
    void UpdateStream(ECognitiveStreamType StreamType, const TArray<float>& Input);

    /** Get stream state */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Streams")
    FConcurrentStreamState GetStreamState(ECognitiveStreamType StreamType) const;

    /** Compute inter-stream coherence */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Streams")
    float ComputeInterStreamCoherence() const;

    /** Get stream phase for given step */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Streams")
    int32 GetStreamPhaseForStep(ECognitiveStreamType StreamType, int32 Step) const;

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Update 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|4E")
    void Update4ECognitionState(const TArray<float>& SensoryInput);

    /** Get state for specific dimension */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|4E")
    TArray<float> Get4EDimensionState(E4ECognitionDimension Dimension) const;

    /** Compute dimensional coherence */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|4E")
    float ComputeDimensionalCoherence() const;

    /** Get dominant 4E dimension */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|4E")
    E4ECognitionDimension GetDominant4EDimension() const;

    // ========================================
    // PUBLIC API - NESTED SHELLS
    // ========================================

    /** Update nested shell at level */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|NestedShells")
    void UpdateNestedShell(int32 Level, const TArray<float>& Input);

    /** Get nested shell state */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|NestedShells")
    FNestedShellLevel GetNestedShellState(int32 Level) const;

    /** Propagate through nested shells */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|NestedShells")
    void PropagateNestedShells();

    /** Get term count for level (OEIS A000081) */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|NestedShells")
    int32 GetTermCountForLevel(int32 Level) const;

    // ========================================
    // PUBLIC API - RELEVANCE REALIZATION
    // ========================================

    /** Compute relevance realization */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Relevance")
    float ComputeRelevanceRealization(const TArray<float>& Context);

    /** Check for relevance realization event */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Relevance")
    bool CheckRelevanceRealizationEvent() const;

    /** Get relevance history */
    UFUNCTION(BlueprintCallable, Category = "CognitiveBridge|Relevance")
    TArray<float> GetRelevanceHistory() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when step advances */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStepAdvanced, int32, NewStep);
    UPROPERTY(BlueprintAssignable, Category = "CognitiveBridge|Events")
    FOnStepAdvanced OnStepAdvanced;

    /** Called at triadic sync point */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTriadicSync, TArray<int32>, TriadSteps);
    UPROPERTY(BlueprintAssignable, Category = "CognitiveBridge|Events")
    FOnTriadicSync OnTriadicSync;

    /** Called on relevance realization event */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelevanceRealization, float, RelevanceScore);
    UPROPERTY(BlueprintAssignable, Category = "CognitiveBridge|Events")
    FOnRelevanceRealization OnRelevanceRealization;

    /** Called when 4E dimension changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EDimensionChange, E4ECognitionDimension, NewDimension);
    UPROPERTY(BlueprintAssignable, Category = "CognitiveBridge|Events")
    FOn4EDimensionChange On4EDimensionChange;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Cycle timer */
    float CycleTimer = 0.0f;

    /** Step duration */
    float StepDuration = 1.0f;

    /** Previous dominant dimension for change detection */
    E4ECognitionDimension PreviousDominantDimension = E4ECognitionDimension::Embodied;

    /** Relevance threshold for event triggering */
    float RelevanceThreshold = 0.7f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Compute step type (expressive vs reflective) */
    bool ComputeStepType(int32 Step) const;

    /** Get active stream for step */
    ECognitiveStreamType GetActiveStreamForStep(int32 Step) const;

    /** Update step state */
    void UpdateStepState(const TArray<float>& SensoryInput);

    /** Couple streams through reservoir */
    void CoupleStreamsThroughReservoir();

    /** Map reservoir to 4E dimensions */
    void MapReservoirTo4EDimensions();

    /** Compute shell coupling */
    void ComputeShellCoupling(int32 SourceLevel, int32 TargetLevel);

    /** Store step in history */
    void StoreStepInHistory();

    /** Compute stream attention */
    TArray<float> ComputeStreamAttention(ECognitiveStreamType StreamType);
};
