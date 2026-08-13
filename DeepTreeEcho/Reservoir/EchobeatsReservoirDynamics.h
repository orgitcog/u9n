#pragma once

/**
 * Echobeats Reservoir Dynamics
 * 
 * Integrates the 12-step echobeats cognitive loop with reservoir computing
 * dynamics, implementing 3 concurrent streams with triadic synchronization
 * and OEIS A000081 nested shell structure.
 * 
 * Architecture:
 * - 12-step cognitive loop (7 expressive + 5 reflective)
 * - 3 concurrent streams phased 4 steps apart (120 degrees)
 * - Triadic step groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 * - OEIS A000081 nested shells: 1->1, 2->2, 3->4, 4->9 terms
 * - 5/7 twin prime structure (mean=6, 3x2 triad-of-dyads)
 * 
 * Step Types:
 * - Pivotal: Relevance realization (steps 1, 7)
 * - Affordance: Actual interaction (steps 2-6)
 * - Salience: Virtual simulation (steps 8-12)
 * 
 * @see DeepCognitiveBridge for base cognitive bridge
 * @see TetradicReservoirIntegration for System 5 architecture
 * @see Embodied4ECognition for 4E cognition integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "DeepCognitiveBridge.h"
#include "TetradicReservoirIntegration.h"
#include "Embodied4ECognition.h"
#include "EchobeatsReservoirDynamics.generated.h"

/**
 * Echobeat Step Type
 */
UENUM(BlueprintType)
enum class EEchobeatStepType : uint8
{
    /** Pivotal relevance realization */
    Pivotal UMETA(DisplayName = "Pivotal"),
    
    /** Affordance interaction */
    Affordance UMETA(DisplayName = "Affordance"),
    
    /** Salience simulation */
    Salience UMETA(DisplayName = "Salience")
};

/**
 * Echobeat Mode
 */
UENUM(BlueprintType)
enum class EEchobeatMode : uint8
{
    /** Expressive mode (7 steps) */
    Expressive UMETA(DisplayName = "Expressive"),
    
    /** Reflective mode (5 steps) */
    Reflective UMETA(DisplayName = "Reflective")
};

/**
 * Nested Shell Term
 * Represents a term in the OEIS A000081 nested shell structure
 */
USTRUCT(BlueprintType)
struct FNestedShellTerm
{
    GENERATED_BODY()

    /** Term ID */
    UPROPERTY(BlueprintReadWrite)
    int32 TermID = 0;

    /** Shell level (1-4) */
    UPROPERTY(BlueprintReadWrite)
    int32 ShellLevel = 1;

    /** Reservoir state for this term */
    UPROPERTY(BlueprintReadWrite)
    FReservoirState ReservoirState;

    /** Parent term ID (-1 if root) */
    UPROPERTY(BlueprintReadWrite)
    int32 ParentTermID = -1;

    /** Child term IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ChildTermIDs;

    /** Activation level */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 0.0f;
};

/**
 * Echobeat Step Configuration
 * Defines the configuration for each step in the 12-step cycle
 */
USTRUCT(BlueprintType)
struct FEchobeatStepConfig
{
    GENERATED_BODY()

    /** Step number (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 StepNumber = 1;

    /** Step type */
    UPROPERTY(BlueprintReadWrite)
    EEchobeatStepType StepType = EEchobeatStepType::Pivotal;

    /** Step mode */
    UPROPERTY(BlueprintReadWrite)
    EEchobeatMode Mode = EEchobeatMode::Expressive;

    /** Primary stream at this step */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveStreamType PrimaryStream = ECognitiveStreamType::Pivotal;

    /** Triadic group (0-3) */
    UPROPERTY(BlueprintReadWrite)
    int32 TriadicGroup = 0;

    /** Nested shell level active at this step */
    UPROPERTY(BlueprintReadWrite)
    int32 ActiveShellLevel = 1;

    /** Reservoir coupling strength for this step */
    UPROPERTY(BlueprintReadWrite)
    float ReservoirCoupling = 0.3f;
};

/**
 * Stream Synchronization State
 * Tracks synchronization between the 3 concurrent streams
 */
USTRUCT(BlueprintType)
struct FStreamSynchronizationState
{
    GENERATED_BODY()

    /** Current synchronization phase */
    UPROPERTY(BlueprintReadWrite)
    int32 SyncPhase = 0;

    /** Inter-stream coherence matrix (3x3) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CoherenceMatrix;

    /** Phase alignment scores */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> PhaseAlignmentScores;

    /** Last sync timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastSyncTime = 0.0f;

    /** Sync quality (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float SyncQuality = 0.5f;
};

/**
 * Echobeats Reservoir Dynamics Component
 * 
 * Integrates echobeats cognitive loop with reservoir computing.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEchobeatsReservoirDynamics : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchobeatsReservoirDynamics();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable echobeats dynamics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bEnableEchobeatsDynamics = true;

    /** 12-step cycle duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float CycleDuration = 12.0f;

    /** Reservoir units per stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config", meta = (ClampMin = "10", ClampMax = "500"))
    int32 UnitsPerStream = 100;

    /** Inter-stream coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InterStreamCoupling = 0.3f;

    /** Enable nested shell processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bEnableNestedShells = true;

    /** Enable triadic synchronization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bEnableTriadicSync = true;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to cognitive bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    UDeepCognitiveBridge* CognitiveBridge;

    /** Reference to tetradic integration */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    UTetradicReservoirIntegration* TetradicIntegration;

    /** Reference to 4E cognition */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    UEmbodied4ECognition* Embodied4E;

    /** Reference to base reservoir */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Components")
    UDeepTreeEchoReservoir* BaseReservoir;

    // ========================================
    // STATE
    // ========================================

    /** Current step (1-12) */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|State")
    int32 CurrentStep = 1;

    /** Step configurations */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|State")
    TArray<FEchobeatStepConfig> StepConfigs;

    /** Stream reservoirs (3 streams) */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|State")
    TArray<FReservoirState> StreamReservoirs;

    /** Nested shell terms */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|State")
    TArray<FNestedShellTerm> NestedShellTerms;

    /** Stream synchronization state */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|State")
    FStreamSynchronizationState SyncState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize echobeats dynamics */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeEchobeatsDynamics();

    /** Initialize step configurations */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeStepConfigurations();

    /** Initialize stream reservoirs */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeStreamReservoirs();

    /** Initialize nested shells (OEIS A000081) */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void InitializeNestedShells();

    // ========================================
    // PUBLIC API - STEP PROCESSING
    // ========================================

    /** Advance to next step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    void AdvanceStep();

    /** Process current step with input */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    void ProcessStep(const TArray<float>& SensoryInput);

    /** Get current step configuration */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    FEchobeatStepConfig GetCurrentStepConfig() const;

    /** Get step type for step number */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    EEchobeatStepType GetStepType(int32 Step) const;

    /** Get step mode for step number */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    EEchobeatMode GetStepMode(int32 Step) const;

    /** Check if step is pivotal */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Steps")
    bool IsPivotalStep(int32 Step) const;

    // ========================================
    // PUBLIC API - STREAM PROCESSING
    // ========================================

    /** Update stream reservoir */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Streams")
    void UpdateStreamReservoir(ECognitiveStreamType Stream, const TArray<float>& Input);

    /** Get stream reservoir state */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Streams")
    FReservoirState GetStreamReservoirState(ECognitiveStreamType Stream) const;

    /** Get stream phase for current step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Streams")
    int32 GetStreamPhase(ECognitiveStreamType Stream) const;

    /** Couple streams at triadic sync point */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Streams")
    void CoupleStreamsAtTriad();

    /** Compute inter-stream coherence */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Streams")
    float ComputeInterStreamCoherence() const;

    // ========================================
    // PUBLIC API - NESTED SHELLS
    // ========================================

    /** Update nested shell term */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|NestedShells")
    void UpdateNestedShellTerm(int32 TermID, const TArray<float>& Input);

    /** Get nested shell term */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|NestedShells")
    FNestedShellTerm GetNestedShellTerm(int32 TermID) const;

    /** Get terms at shell level */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|NestedShells")
    TArray<FNestedShellTerm> GetTermsAtLevel(int32 Level) const;

    /** Propagate through nested shells */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|NestedShells")
    void PropagateNestedShells();

    /** Get term count for level (OEIS A000081) */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|NestedShells")
    int32 GetTermCountForLevel(int32 Level) const;

    // ========================================
    // PUBLIC API - TRIADIC SYNCHRONIZATION
    // ========================================

    /** Get triadic group for step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Triadic")
    int32 GetTriadicGroup(int32 Step) const;

    /** Get steps in triadic group */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Triadic")
    TArray<int32> GetStepsInTriadicGroup(int32 Group) const;

    /** Check if at triadic sync point */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Triadic")
    bool IsTriadicSyncPoint() const;

    /** Synchronize at triadic point */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Triadic")
    void SynchronizeAtTriad();

    /** Get triadic synchronization quality */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Triadic")
    float GetTriadicSyncQuality() const;

    // ========================================
    // PUBLIC API - RELEVANCE REALIZATION
    // ========================================

    /** Compute relevance realization */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Relevance")
    float ComputeRelevanceRealization(const TArray<float>& Context) const;

    /** Check for relevance event */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Relevance")
    bool CheckRelevanceEvent() const;

    /** Get relevance history */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Relevance")
    TArray<float> GetRelevanceHistory() const;

    // ========================================
    // PUBLIC API - INTEGRATION
    // ========================================

    /** Integrate with tetradic system */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    void IntegrateWithTetradic();

    /** Integrate with 4E cognition */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    void IntegrateWith4E();

    /** Get integrated cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    TArray<float> GetIntegratedCognitiveState() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when step advances */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEchobeatStepAdvanced, int32, NewStep, FEchobeatStepConfig, Config);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnEchobeatStepAdvanced OnStepAdvanced;

    /** Called at triadic sync point */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTriadicSyncPoint, int32, TriadicGroup, float, SyncQuality);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnTriadicSyncPoint OnTriadicSyncPoint;

    /** Called on relevance realization */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelevanceRealized, int32, Step, float, RelevanceScore);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnRelevanceRealized OnRelevanceRealized;

    /** Called when mode changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModeChanged, EEchobeatMode, OldMode, EEchobeatMode, NewMode);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnModeChanged OnModeChanged;

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

    /** Previous mode for change detection */
    EEchobeatMode PreviousMode = EEchobeatMode::Expressive;

    /** Relevance history */
    TArray<float> RelevanceHistory;

    /** Relevance threshold */
    float RelevanceThreshold = 0.7f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Build nested shell tree structure */
    void BuildNestedShellTree();

    /** Process pivotal step */
    void ProcessPivotalStep(const TArray<float>& Input);

    /** Process affordance step */
    void ProcessAffordanceStep(const TArray<float>& Input);

    /** Process salience step */
    void ProcessSalienceStep(const TArray<float>& Input);

    /** Update stream synchronization */
    void UpdateStreamSynchronization();

    /** Propagate shell level */
    void PropagateShellLevel(int32 Level);

    /** Check mode change */
    void CheckModeChange();

    /** Store relevance in history */
    void StoreRelevanceInHistory(float Relevance);
};
