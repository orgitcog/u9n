// CausalChainTracking.h
// Causal relationship tracking between temporal events for reasoning about consequences
// Feature F1.5.2 - Temporal Reasoning System
// Deep Tree Echo Cognitive Architecture

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CausalChainTracking.generated.h"

// Forward declarations
class UEpisodicMemorySystem;
class UHypergraphMemorySystem;

// ========================================
// CAUSAL RELATIONSHIP ENUMERATIONS
// ========================================

/**
 * Type of causal relationship between events
 */
UENUM(BlueprintType)
enum class ECausalRelationType : uint8
{
    /** Direct causation (A directly causes B) */
    DirectCause         UMETA(DisplayName = "Direct Cause"),
    /** Indirect causation (A causes B through intermediary) */
    IndirectCause       UMETA(DisplayName = "Indirect Cause"),
    /** Enabling condition (A makes B possible) */
    Enabler             UMETA(DisplayName = "Enabler"),
    /** Preventing condition (A prevents B) */
    Preventer           UMETA(DisplayName = "Preventer"),
    /** Necessary condition (B requires A) */
    NecessaryCondition  UMETA(DisplayName = "Necessary Condition"),
    /** Sufficient condition (A is enough for B) */
    SufficientCondition UMETA(DisplayName = "Sufficient Condition"),
    /** Probabilistic cause (A increases probability of B) */
    ProbabilisticCause  UMETA(DisplayName = "Probabilistic Cause"),
    /** Counterfactual (if A hadn't happened, B wouldn't have) */
    Counterfactual      UMETA(DisplayName = "Counterfactual"),
    /** Temporal precedence only (A before B, no proven causation) */
    TemporalPrecedence  UMETA(DisplayName = "Temporal Precedence"),
    /** Correlation (A and B co-occur, causation unknown) */
    Correlation         UMETA(DisplayName = "Correlation")
};

/**
 * Confidence level in causal relationship
 */
UENUM(BlueprintType)
enum class ECausalConfidence : uint8
{
    /** Speculative (weak evidence) */
    Speculative     UMETA(DisplayName = "Speculative"),
    /** Possible (some evidence) */
    Possible        UMETA(DisplayName = "Possible"),
    /** Probable (good evidence) */
    Probable        UMETA(DisplayName = "Probable"),
    /** Certain (strong evidence or observed) */
    Certain         UMETA(DisplayName = "Certain"),
    /** Verified (empirically confirmed) */
    Verified        UMETA(DisplayName = "Verified")
};

/**
 * Status of a causal chain
 */
UENUM(BlueprintType)
enum class ECausalChainStatus : uint8
{
    /** Chain is being constructed */
    Building        UMETA(DisplayName = "Building"),
    /** Chain is complete and valid */
    Complete        UMETA(DisplayName = "Complete"),
    /** Chain has been validated */
    Validated       UMETA(DisplayName = "Validated"),
    /** Chain has been invalidated */
    Invalidated     UMETA(DisplayName = "Invalidated"),
    /** Chain is dormant (not currently relevant) */
    Dormant         UMETA(DisplayName = "Dormant")
};

/**
 * Type of event in causal reasoning
 */
UENUM(BlueprintType)
enum class ECausalEventType : uint8
{
    /** Sensory observation */
    Observation     UMETA(DisplayName = "Observation"),
    /** Agent action */
    Action          UMETA(DisplayName = "Action"),
    /** State change in environment */
    StateChange     UMETA(DisplayName = "State Change"),
    /** Internal cognitive event */
    CognitiveEvent  UMETA(DisplayName = "Cognitive Event"),
    /** Hypothetical/predicted event */
    Hypothetical    UMETA(DisplayName = "Hypothetical"),
    /** Counterfactual event (what didn't happen) */
    Counterfactual  UMETA(DisplayName = "Counterfactual")
};

// ========================================
// CAUSAL RELATIONSHIP STRUCTURES
// ========================================

/**
 * Represents a temporal event in the causal reasoning system
 */
USTRUCT(BlueprintType)
struct FCausalEvent
{
    GENERATED_BODY()

    /** Unique event identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 EventID = 0;

    /** Event type */
    UPROPERTY(BlueprintReadWrite)
    ECausalEventType EventType = ECausalEventType::Observation;

    /** Human-readable event description */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Event occurrence timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Duration of the event (0 for instantaneous) */
    UPROPERTY(BlueprintReadWrite)
    float Duration = 0.0f;

    /** Spatial location associated with event */
    UPROPERTY(BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    /** Vector representation of event content */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ContentVector;

    /** Associated memory trace ID (from EpisodicMemorySystem) */
    UPROPERTY(BlueprintReadWrite)
    int64 MemoryTraceID = 0;

    /** Associated hypergraph node ID (from HypergraphMemorySystem) */
    UPROPERTY(BlueprintReadWrite)
    int64 HypergraphNodeID = 0;

    /** Confidence in event occurrence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float OccurrenceConfidence = 1.0f;

    /** Salience/importance of event (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Salience = 0.5f;

    /** Tags for categorization */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Tags;

    /** Associated actor (if any) */
    UPROPERTY(BlueprintReadWrite)
    FString ActorID;

    /** Properties key-value storage */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> Properties;
};

/**
 * Represents a causal link between two events
 */
USTRUCT(BlueprintType)
struct FCausalLink
{
    GENERATED_BODY()

    /** Unique link identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 LinkID = 0;

    /** Cause event ID */
    UPROPERTY(BlueprintReadWrite)
    int64 CauseEventID = 0;

    /** Effect event ID */
    UPROPERTY(BlueprintReadWrite)
    int64 EffectEventID = 0;

    /** Type of causal relationship */
    UPROPERTY(BlueprintReadWrite)
    ECausalRelationType RelationType = ECausalRelationType::DirectCause;

    /** Causal strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Confidence in the causal link */
    UPROPERTY(BlueprintReadWrite)
    ECausalConfidence Confidence = ECausalConfidence::Possible;

    /** Numeric confidence value (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ConfidenceValue = 0.5f;

    /** Temporal delay between cause and effect */
    UPROPERTY(BlueprintReadWrite)
    float TemporalDelay = 0.0f;

    /** Number of observations supporting this link */
    UPROPERTY(BlueprintReadWrite)
    int32 ObservationCount = 1;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    float CreationTime = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;

    /** Explanation/justification for the link */
    UPROPERTY(BlueprintReadWrite)
    FString Explanation;

    /** Mechanism description (how cause leads to effect) */
    UPROPERTY(BlueprintReadWrite)
    FString Mechanism;

    /** Is this link currently active */
    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = true;
};

/**
 * Represents a chain of causal events
 */
USTRUCT(BlueprintType)
struct FCausalChain
{
    GENERATED_BODY()

    /** Unique chain identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 ChainID = 0;

    /** Chain label/name */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Ordered event IDs in the chain */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> EventIDs;

    /** Causal links connecting events (parallel to gaps between events) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int64> LinkIDs;

    /** Chain status */
    UPROPERTY(BlueprintReadWrite)
    ECausalChainStatus Status = ECausalChainStatus::Building;

    /** Overall chain strength (product or min of link strengths) */
    UPROPERTY(BlueprintReadWrite)
    float OverallStrength = 0.0f;

    /** Overall chain confidence */
    UPROPERTY(BlueprintReadWrite)
    float OverallConfidence = 0.0f;

    /** Root cause event ID */
    UPROPERTY(BlueprintReadWrite)
    int64 RootCauseID = 0;

    /** Final effect event ID */
    UPROPERTY(BlueprintReadWrite)
    int64 FinalEffectID = 0;

    /** Total temporal span of the chain */
    UPROPERTY(BlueprintReadWrite)
    float TemporalSpan = 0.0f;

    /** Chain depth (number of links) */
    UPROPERTY(BlueprintReadWrite)
    int32 Depth = 0;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    float CreationTime = 0.0f;

    /** Has this chain been used for prediction */
    UPROPERTY(BlueprintReadWrite)
    bool bUsedForPrediction = false;

    /** Number of times chain pattern has been observed */
    UPROPERTY(BlueprintReadWrite)
    int32 PatternCount = 1;
};

/**
 * Result of causal inference query
 */
USTRUCT(BlueprintType)
struct FCausalInferenceResult
{
    GENERATED_BODY()

    /** Query was successful */
    UPROPERTY(BlueprintReadWrite)
    bool bSuccess = false;

    /** Inferred causal chains */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCausalChain> InferredChains;

    /** Discovered causal links */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCausalLink> DiscoveredLinks;

    /** Confidence in inference (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float InferenceConfidence = 0.0f;

    /** Explanation of inference process */
    UPROPERTY(BlueprintReadWrite)
    FString Explanation;

    /** Alternative explanations considered */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> AlternativeExplanations;

    /** Events examined during inference */
    UPROPERTY(BlueprintReadWrite)
    int32 EventsExamined = 0;

    /** Execution time in milliseconds */
    UPROPERTY(BlueprintReadWrite)
    float ExecutionTimeMs = 0.0f;
};

/**
 * Prediction based on causal knowledge
 */
USTRUCT(BlueprintType)
struct FCausalPrediction
{
    GENERATED_BODY()

    /** Unique prediction identifier */
    UPROPERTY(BlueprintReadWrite)
    int64 PredictionID = 0;

    /** Predicted event */
    UPROPERTY(BlueprintReadWrite)
    FCausalEvent PredictedEvent;

    /** Causal chain supporting the prediction */
    UPROPERTY(BlueprintReadWrite)
    int64 SupportingChainID = 0;

    /** Prediction confidence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 0.0f;

    /** Expected time of predicted event */
    UPROPERTY(BlueprintReadWrite)
    float PredictedTime = 0.0f;

    /** Time window for prediction (uncertainty) */
    UPROPERTY(BlueprintReadWrite)
    float TimeWindow = 0.0f;

    /** Was the prediction verified */
    UPROPERTY(BlueprintReadWrite)
    bool bVerified = false;

    /** Was the prediction correct */
    UPROPERTY(BlueprintReadWrite)
    bool bCorrect = false;

    /** Actual outcome event ID (after verification) */
    UPROPERTY(BlueprintReadWrite)
    int64 ActualOutcomeID = 0;
};

/**
 * Configuration for causal chain tracking
 */
USTRUCT(BlueprintType)
struct FCausalTrackingConfig
{
    GENERATED_BODY()

    /** Maximum temporal window for causal inference (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "3600.0"))
    float MaxCausalWindow = 60.0f;

    /** Minimum strength threshold for causal links */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinLinkStrength = 0.1f;

    /** Minimum confidence for accepting causal links */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinConfidence = 0.3f;

    /** Maximum chain depth to explore */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxChainDepth = 10;

    /** Maximum events to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100", ClampMax = "100000"))
    int32 MaxEvents = 10000;

    /** Maximum causal links to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100", ClampMax = "100000"))
    int32 MaxLinks = 50000;

    /** Maximum chains to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10", ClampMax = "10000"))
    int32 MaxChains = 1000;

    /** Enable automatic causal inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAutoInference = true;

    /** Interval for automatic inference (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float AutoInferenceInterval = 5.0f;

    /** Enable prediction generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePredictions = true;

    /** Decay rate for unused causal links */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float LinkDecayRate = 0.01f;

    /** Strengthening factor for confirmed links */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "2.0"))
    float ConfirmationFactor = 1.2f;

    /** Weakening factor for disconfirmed links */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5", ClampMax = "1.0"))
    float DisconfirmationFactor = 0.8f;

    /** Use content similarity for causal inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseContentSimilarity = true;

    /** Content similarity threshold for inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ContentSimilarityThreshold = 0.5f;
};

/**
 * Statistics for causal tracking system
 */
USTRUCT(BlueprintType)
struct FCausalTrackingStats
{
    GENERATED_BODY()

    /** Total events tracked */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalEvents = 0;

    /** Total causal links */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalLinks = 0;

    /** Total causal chains */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalChains = 0;

    /** Active predictions */
    UPROPERTY(BlueprintReadOnly)
    int32 ActivePredictions = 0;

    /** Verified predictions */
    UPROPERTY(BlueprintReadOnly)
    int32 VerifiedPredictions = 0;

    /** Correct predictions */
    UPROPERTY(BlueprintReadOnly)
    int32 CorrectPredictions = 0;

    /** Prediction accuracy (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float PredictionAccuracy = 0.0f;

    /** Average chain depth */
    UPROPERTY(BlueprintReadOnly)
    float AverageChainDepth = 0.0f;

    /** Average link strength */
    UPROPERTY(BlueprintReadOnly)
    float AverageLinkStrength = 0.0f;

    /** Total inference operations */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalInferences = 0;

    /** Average inference time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float AverageInferenceTimeMs = 0.0f;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCausalEventRecorded, int64, EventID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCausalLinkCreated, int64, LinkID, ECausalRelationType, RelationType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCausalChainDiscovered, int64, ChainID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionMade, int64, PredictionID, float, Confidence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionVerified, int64, PredictionID, bool, bCorrect);

/**
 * UCausalChainTracking
 * 
 * Component for tracking causal relationships between temporal events
 * and reasoning about consequences. Provides:
 * 
 * - Event recording and management
 * - Causal link inference between events
 * - Causal chain construction and analysis
 * - Consequence prediction based on learned causality
 * - Integration with episodic and hypergraph memory systems
 * 
 * Implements counterfactual reasoning, probabilistic causation,
 * and temporal causal reasoning for the Deep Tree Echo cognitive architecture.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UCausalChainTracking : public UActorComponent
{
    GENERATED_BODY()

public:
    UCausalChainTracking();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Causal tracking configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CausalTracking|Config")
    FCausalTrackingConfig Config;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when a new causal event is recorded */
    UPROPERTY(BlueprintAssignable, Category = "CausalTracking|Events")
    FOnCausalEventRecorded OnCausalEventRecorded;

    /** Called when a new causal link is created */
    UPROPERTY(BlueprintAssignable, Category = "CausalTracking|Events")
    FOnCausalLinkCreated OnCausalLinkCreated;

    /** Called when a new causal chain is discovered */
    UPROPERTY(BlueprintAssignable, Category = "CausalTracking|Events")
    FOnCausalChainDiscovered OnCausalChainDiscovered;

    /** Called when a prediction is made */
    UPROPERTY(BlueprintAssignable, Category = "CausalTracking|Events")
    FOnPredictionMade OnPredictionMade;

    /** Called when a prediction is verified */
    UPROPERTY(BlueprintAssignable, Category = "CausalTracking|Events")
    FOnPredictionVerified OnPredictionVerified;

    // ========================================
    // EVENT MANAGEMENT
    // ========================================

    /** Record a new causal event */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    int64 RecordEvent(const FString& Description, ECausalEventType EventType,
                      FVector Location = FVector::ZeroVector, float Salience = 0.5f);

    /** Record event with full specification */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    int64 RecordEventFull(const FCausalEvent& Event);

    /** Get event by ID */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    FCausalEvent GetEvent(int64 EventID) const;

    /** Check if event exists */
    UFUNCTION(BlueprintPure, Category = "CausalTracking|Events")
    bool EventExists(int64 EventID) const;

    /** Get events in time range */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    TArray<FCausalEvent> GetEventsInTimeRange(float StartTime, float EndTime) const;

    /** Get events by type */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    TArray<FCausalEvent> GetEventsByType(ECausalEventType EventType) const;

    /** Get recent events */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    TArray<FCausalEvent> GetRecentEvents(int32 Count = 10) const;

    /** Update event salience */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Events")
    void UpdateEventSalience(int64 EventID, float NewSalience);

    // ========================================
    // CAUSAL LINK MANAGEMENT
    // ========================================

    /** Create a causal link between two events */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    int64 CreateCausalLink(int64 CauseEventID, int64 EffectEventID,
                           ECausalRelationType RelationType = ECausalRelationType::DirectCause,
                           float Strength = 0.5f, const FString& Explanation = TEXT(""));

    /** Get causal link by ID */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    FCausalLink GetCausalLink(int64 LinkID) const;

    /** Check if link exists */
    UFUNCTION(BlueprintPure, Category = "CausalTracking|Links")
    bool LinkExists(int64 LinkID) const;

    /** Get all links from a cause event */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    TArray<FCausalLink> GetEffectsOf(int64 CauseEventID) const;

    /** Get all links to an effect event */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    TArray<FCausalLink> GetCausesOf(int64 EffectEventID) const;

    /** Get link between two specific events */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    FCausalLink GetLinkBetween(int64 CauseEventID, int64 EffectEventID) const;

    /** Strengthen a causal link (due to repeated observation) */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    void StrengthenLink(int64 LinkID, float Factor = 1.1f);

    /** Weaken a causal link (due to disconfirmation) */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    void WeakenLink(int64 LinkID, float Factor = 0.9f);

    /** Remove a causal link */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Links")
    bool RemoveLink(int64 LinkID);

    // ========================================
    // CAUSAL CHAIN MANAGEMENT
    // ========================================

    /** Build a causal chain from root cause to final effect */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    int64 BuildChain(int64 RootCauseID, int64 FinalEffectID, const FString& Label = TEXT(""));

    /** Get causal chain by ID */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    FCausalChain GetChain(int64 ChainID) const;

    /** Get all chains involving an event */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    TArray<FCausalChain> GetChainsInvolvingEvent(int64 EventID) const;

    /** Get chains by status */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    TArray<FCausalChain> GetChainsByStatus(ECausalChainStatus Status) const;

    /** Validate a causal chain */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    bool ValidateChain(int64 ChainID);

    /** Invalidate a causal chain */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    void InvalidateChain(int64 ChainID);

    /** Get root cause of an effect (trace back through chains) */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    FCausalEvent GetRootCause(int64 EffectEventID, int32 MaxDepth = 10) const;

    /** Get all consequences of a cause (trace forward through chains) */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Chains")
    TArray<FCausalEvent> GetAllConsequences(int64 CauseEventID, int32 MaxDepth = 10) const;

    // ========================================
    // CAUSAL INFERENCE
    // ========================================

    /** Infer causal relationships from recent events */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Inference")
    FCausalInferenceResult InferCausality(float TimeWindow = 10.0f);

    /** Infer causal relationship between two specific events */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Inference")
    FCausalInferenceResult InferRelationship(int64 EventA, int64 EventB);

    /** Find all potential causes for an effect */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Inference")
    TArray<FCausalLink> FindPotentialCauses(int64 EffectEventID, float TimeWindow = 10.0f);

    /** Find all potential effects of a cause */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Inference")
    TArray<FCausalLink> FindPotentialEffects(int64 CauseEventID, float TimeWindow = 10.0f);

    /** Perform counterfactual analysis (what if X hadn't happened) */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Inference")
    TArray<FCausalEvent> CounterfactualAnalysis(int64 EventID);

    // ========================================
    // PREDICTION
    // ========================================

    /** Predict consequences based on current situation */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Prediction")
    TArray<FCausalPrediction> PredictConsequences(int64 CauseEventID);

    /** Predict what caused current situation */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Prediction")
    TArray<FCausalPrediction> PredictCauses(int64 EffectEventID);

    /** Verify a prediction against actual outcome */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Prediction")
    void VerifyPrediction(int64 PredictionID, int64 ActualOutcomeID);

    /** Get active predictions */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Prediction")
    TArray<FCausalPrediction> GetActivePredictions() const;

    /** Get prediction by ID */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Prediction")
    FCausalPrediction GetPrediction(int64 PredictionID) const;

    // ========================================
    // INTEGRATION
    // ========================================

    /** Set episodic memory system reference */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Integration")
    void SetEpisodicMemory(UEpisodicMemorySystem* EpisodicMemory);

    /** Set hypergraph memory system reference */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Integration")
    void SetHypergraphMemory(UHypergraphMemorySystem* HypergraphMemory);

    /** Sync event with episodic memory trace */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Integration")
    void SyncWithEpisodicMemory(int64 EventID, int64 MemoryTraceID);

    /** Sync event with hypergraph node */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Integration")
    void SyncWithHypergraph(int64 EventID, int64 NodeID);

    /** Auto-discover causal relationships from memory */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Integration")
    void DiscoverFromMemory();

    // ========================================
    // STATISTICS AND DIAGNOSTICS
    // ========================================

    /** Get causal tracking statistics */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Stats")
    FCausalTrackingStats GetStatistics() const;

    /** Get system info string */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Stats")
    FString GetSystemInfo() const;

    /** Reset all causal tracking data */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Stats")
    void Reset();

    /** Prune old/weak causal knowledge */
    UFUNCTION(BlueprintCallable, Category = "CausalTracking|Stats")
    void PruneWeakLinks(float MinStrength = 0.1f);

protected:
    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Process automatic causal inference */
    void ProcessAutoInference();

    /** Decay unused causal links */
    void DecayLinks(float DeltaTime);

    /** Compute content similarity between events */
    float ComputeEventSimilarity(const FCausalEvent& A, const FCausalEvent& B) const;

    /** Compute temporal plausibility of causal link */
    float ComputeTemporalPlausibility(const FCausalEvent& Cause, const FCausalEvent& Effect) const;

    /** Find path between two events */
    TArray<int64> FindCausalPath(int64 StartEventID, int64 EndEventID, int32 MaxDepth) const;

    /** Update chain statistics */
    void UpdateChainStatistics(int64 ChainID);

    /** Get current simulation time */
    float GetCurrentTime() const;

    /** Generate new event ID */
    int64 GenerateEventID();

    /** Generate new link ID */
    int64 GenerateLinkID();

    /** Generate new chain ID */
    int64 GenerateChainID();

    /** Generate new prediction ID */
    int64 GeneratePredictionID();

private:
    // ========================================
    // INTERNAL DATA
    // ========================================

    /** All causal events */
    UPROPERTY()
    TMap<int64, FCausalEvent> Events;

    /** All causal links */
    UPROPERTY()
    TMap<int64, FCausalLink> Links;

    /** All causal chains */
    UPROPERTY()
    TMap<int64, FCausalChain> Chains;

    /** All predictions */
    UPROPERTY()
    TMap<int64, FCausalPrediction> Predictions;

    /** Cause event ID -> Link IDs (outgoing) */
    TMap<int64, TArray<int64>> CauseToLinks;

    /** Effect event ID -> Link IDs (incoming) */
    TMap<int64, TArray<int64>> EffectToLinks;

    /** Event ID -> Chain IDs */
    TMap<int64, TArray<int64>> EventToChains;

    /** Next event ID */
    int64 NextEventID = 1;

    /** Next link ID */
    int64 NextLinkID = 1;

    /** Next chain ID */
    int64 NextChainID = 1;

    /** Next prediction ID */
    int64 NextPredictionID = 1;

    /** Accumulated time */
    float AccumulatedTime = 0.0f;

    /** Last auto-inference time */
    float LastInferenceTime = 0.0f;

    /** Reference to episodic memory */
    UPROPERTY()
    UEpisodicMemorySystem* EpisodicMemoryRef = nullptr;

    /** Reference to hypergraph memory */
    UPROPERTY()
    UHypergraphMemorySystem* HypergraphMemoryRef = nullptr;

    /** Statistics tracking */
    int32 TotalInferenceOperations = 0;
    double TotalInferenceTimeMs = 0.0;
};
