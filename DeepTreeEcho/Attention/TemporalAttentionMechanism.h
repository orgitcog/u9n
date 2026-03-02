// TemporalAttentionMechanism.h
// Temporal Attention Mechanism for focusing on relevant temporal windows and events
// Feature F1.5.4 - B-Series Temporal Integration / Temporal Attention Mechanism

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TemporalAttentionMechanism.generated.h"

// Forward declarations
class UAttentionSystem;
class UECANAttentionAllocation;
class UEpisodicMemorySystem;
class UCognitiveCycleManager;

/**
 * Temporal scale for attention windows
 */
UENUM(BlueprintType)
enum class ETemporalScale : uint8
{
    Immediate       UMETA(DisplayName = "Immediate (< 1s)"),
    ShortTerm       UMETA(DisplayName = "Short-Term (1-10s)"),
    MediumTerm      UMETA(DisplayName = "Medium-Term (10s-1min)"),
    LongTerm        UMETA(DisplayName = "Long-Term (1min-1hr)"),
    Extended        UMETA(DisplayName = "Extended (> 1hr)")
};

/**
 * Event importance category
 */
UENUM(BlueprintType)
enum class ETemporalEventImportance : uint8
{
    Background      UMETA(DisplayName = "Background (Low)"),
    Routine         UMETA(DisplayName = "Routine (Normal)"),
    Notable         UMETA(DisplayName = "Notable (Elevated)"),
    Critical        UMETA(DisplayName = "Critical (High)"),
    Landmark        UMETA(DisplayName = "Landmark (Maximum)")
};

/**
 * Temporal attention mode
 */
UENUM(BlueprintType)
enum class ETemporalAttentionMode : uint8
{
    Present         UMETA(DisplayName = "Present Focus"),
    Retrospective   UMETA(DisplayName = "Retrospective (Past)"),
    Prospective     UMETA(DisplayName = "Prospective (Future)"),
    Comparative     UMETA(DisplayName = "Comparative (Past vs Present)"),
    Anticipatory    UMETA(DisplayName = "Anticipatory (Predicting)")
};

/**
 * Temporal event - a discrete occurrence in time
 */
USTRUCT(BlueprintType)
struct FTemporalEvent
{
    GENERATED_BODY()

    /** Unique event identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventID;

    /** Event name/label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventName;

    /** Timestamp when event occurred */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Event duration (0 for instantaneous) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 0.0f;

    /** Importance level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETemporalEventImportance Importance = ETemporalEventImportance::Routine;

    /** Current attention weight (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionWeight = 0.0f;

    /** Salience score (combined relevance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SalienceScore = 0.0f;

    /** Emotional valence (-1.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Emotional arousal (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal = 0.0f;

    /** Associated tags for categorization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;

    /** Linked event IDs (causal/temporal relationships) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> LinkedEventIDs;

    /** Access count (how often this event has been attended to) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AccessCount = 0;

    /** Last access timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastAccessTime = 0.0f;

    /** Feature vector for similarity matching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> FeatureVector;
};

/**
 * Temporal window - a span of time for attention focus
 */
USTRUCT(BlueprintType)
struct FTemporalWindow
{
    GENERATED_BODY()

    /** Window identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString WindowID;

    /** Window label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Label;

    /** Start time of the window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StartTime = 0.0f;

    /** End time of the window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EndTime = 0.0f;

    /** Temporal scale */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETemporalScale Scale = ETemporalScale::Immediate;

    /** Attention weight for this window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionWeight = 1.0f;

    /** Events within this window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> EventIDs;

    /** Overall salience of window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WindowSalience = 0.0f;

    /** Is this the current focus window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFocusWindow = false;

    /** Get duration */
    float GetDuration() const { return EndTime - StartTime; }
};

/**
 * Temporal attention weights for different time scales
 */
USTRUCT(BlueprintType)
struct FTemporalAttentionWeights
{
    GENERATED_BODY()

    /** Weight for immediate past (< 1s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ImmediateWeight = 0.4f;

    /** Weight for short-term past (1-10s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShortTermWeight = 0.3f;

    /** Weight for medium-term past (10s-1min) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MediumTermWeight = 0.15f;

    /** Weight for long-term past (1min-1hr) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LongTermWeight = 0.1f;

    /** Weight for extended past (> 1hr) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExtendedWeight = 0.05f;

    /** Get weight for scale */
    float GetWeight(ETemporalScale Scale) const
    {
        switch (Scale)
        {
            case ETemporalScale::Immediate: return ImmediateWeight;
            case ETemporalScale::ShortTerm: return ShortTermWeight;
            case ETemporalScale::MediumTerm: return MediumTermWeight;
            case ETemporalScale::LongTerm: return LongTermWeight;
            case ETemporalScale::Extended: return ExtendedWeight;
            default: return 0.0f;
        }
    }
};

/**
 * Temporal attention configuration
 */
USTRUCT(BlueprintType)
struct FTemporalAttentionConfig
{
    GENERATED_BODY()

    // === Time Scale Boundaries (in seconds) ===

    /** Immediate boundary (0 to this value is immediate) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ImmediateBoundary = 1.0f;

    /** Short-term boundary */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShortTermBoundary = 10.0f;

    /** Medium-term boundary */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MediumTermBoundary = 60.0f;

    /** Long-term boundary */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LongTermBoundary = 3600.0f;

    // === Decay Parameters ===

    /** Temporal decay rate (attention decreases over time) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TemporalDecayRate = 0.1f;

    /** Base attention threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttentionThreshold = 0.1f;

    /** Salience boost for high-importance events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "5.0"))
    float ImportanceBoost = 2.0f;

    // === Capacity Limits ===

    /** Maximum events to track */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100", ClampMax = "10000"))
    int32 MaxEvents = 1000;

    /** Maximum windows to maintain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10", ClampMax = "100"))
    int32 MaxWindows = 50;

    /** Maximum events in focus at once */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxFocusEvents = 10;

    // === Attention Dynamics ===

    /** Enable automatic attention updating */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAutoUpdate = true;

    /** Emotional salience weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EmotionalWeight = 0.3f;

    /** Recency weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RecencyWeight = 0.4f;

    /** Importance weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ImportanceWeight = 0.3f;
};

/**
 * Temporal attention state
 */
USTRUCT(BlueprintType)
struct FTemporalAttentionState
{
    GENERATED_BODY()

    /** Current attention mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETemporalAttentionMode Mode = ETemporalAttentionMode::Present;

    /** Current focus window ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentWindowID;

    /** Currently focused event IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> FocusedEventIDs;

    /** Current temporal attention weights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTemporalAttentionWeights Weights;

    /** Total events being tracked */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalEvents = 0;

    /** Total attention capacity used */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionCapacityUsed = 0.0f;

    /** Last update timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastUpdateTime = 0.0f;
};

/**
 * Temporal attention query result
 */
USTRUCT(BlueprintType)
struct FTemporalQueryResult
{
    GENERATED_BODY()

    /** Matching events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTemporalEvent> Events;

    /** Query time range start */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float QueryStartTime = 0.0f;

    /** Query time range end */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float QueryEndTime = 0.0f;

    /** Total matching count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalMatches = 0;

    /** Average attention weight of results */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageAttentionWeight = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemporalEventRegistered, const FTemporalEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTemporalFocusChanged, const FString&, OldEventID, const FString&, NewEventID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemporalWindowCreated, const FTemporalWindow&, Window);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTemporalModeChanged, ETemporalAttentionMode, OldMode, ETemporalAttentionMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemporalEventExpired, const FString&, EventID);

/**
 * Temporal Attention Mechanism Component
 * 
 * Implements attention mechanisms for focusing on relevant temporal windows and events.
 * Part of the B-Series Temporal Integration phase (Phase 1.5).
 * 
 * Key capabilities:
 * - Track and prioritize temporal events
 * - Manage attention across multiple time scales
 * - Focus on relevant temporal windows
 * - Integrate with spatial attention and memory systems
 * - Support retrospective and prospective attention modes
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UTemporalAttentionMechanism : public UActorComponent
{
    GENERATED_BODY()

public:
    UTemporalAttentionMechanism();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalAttention|Configuration")
    FTemporalAttentionConfig Config;

    /** Current temporal attention weights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalAttention|Configuration")
    FTemporalAttentionWeights TemporalWeights;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "TemporalAttention|Events")
    FOnTemporalEventRegistered OnTemporalEventRegistered;

    UPROPERTY(BlueprintAssignable, Category = "TemporalAttention|Events")
    FOnTemporalFocusChanged OnTemporalFocusChanged;

    UPROPERTY(BlueprintAssignable, Category = "TemporalAttention|Events")
    FOnTemporalWindowCreated OnTemporalWindowCreated;

    UPROPERTY(BlueprintAssignable, Category = "TemporalAttention|Events")
    FOnTemporalModeChanged OnTemporalModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "TemporalAttention|Events")
    FOnTemporalEventExpired OnTemporalEventExpired;

    // ========================================
    // EVENT MANAGEMENT
    // ========================================

    /** Register a new temporal event */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    FTemporalEvent RegisterEvent(const FString& EventName, 
                                  ETemporalEventImportance Importance,
                                  float Duration = 0.0f,
                                  const TArray<FString>& Tags = TArray<FString>());

    /** Register event with emotional context */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    FTemporalEvent RegisterEmotionalEvent(const FString& EventName,
                                           ETemporalEventImportance Importance,
                                           float EmotionalValence,
                                           float EmotionalArousal,
                                           const TArray<FString>& Tags = TArray<FString>());

    /** Update event importance */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    void UpdateEventImportance(const FString& EventID, ETemporalEventImportance NewImportance);

    /** Add feature vector to event */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    void SetEventFeatures(const FString& EventID, const TArray<float>& Features);

    /** Link two events (causal/temporal relationship) */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    void LinkEvents(const FString& EventID1, const FString& EventID2);

    /** Get event by ID */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Events")
    FTemporalEvent GetEvent(const FString& EventID) const;

    /** Check if event exists */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Events")
    bool EventExists(const FString& EventID) const;

    /** Remove event */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Events")
    void RemoveEvent(const FString& EventID);

    // ========================================
    // TEMPORAL WINDOW MANAGEMENT
    // ========================================

    /** Create a temporal window */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Windows")
    FTemporalWindow CreateWindow(const FString& Label, float StartTime, float EndTime);

    /** Get window by ID */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Windows")
    FTemporalWindow GetWindow(const FString& WindowID) const;

    /** Get events in time range */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Windows")
    TArray<FTemporalEvent> GetEventsInRange(float StartTime, float EndTime) const;

    /** Get events by temporal scale */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Windows")
    TArray<FTemporalEvent> GetEventsByScale(ETemporalScale Scale) const;

    /** Get current focus window */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Windows")
    FTemporalWindow GetCurrentFocusWindow() const;

    /** Set focus window */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Windows")
    void SetFocusWindow(const FString& WindowID);

    // ========================================
    // ATTENTION CONTROL
    // ========================================

    /** Focus attention on specific event */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    bool FocusOnEvent(const FString& EventID);

    /** Focus attention on time range */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    bool FocusOnTimeRange(float StartTime, float EndTime);

    /** Set temporal attention mode */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    void SetAttentionMode(ETemporalAttentionMode NewMode);

    /** Get current attention mode */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Control")
    ETemporalAttentionMode GetAttentionMode() const;

    /** Adjust temporal scale weights */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    void SetTemporalWeights(const FTemporalAttentionWeights& NewWeights);

    /** Boost attention for specific tags */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    void BoostTagAttention(const TArray<FString>& Tags, float BoostAmount);

    /** Release current focus */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Control")
    void ReleaseFocus();

    // ========================================
    // SALIENCE COMPUTATION
    // ========================================

    /** Compute salience for an event */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|Salience")
    float ComputeEventSalience(const FString& EventID) const;

    /** Get most salient events */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Salience")
    TArray<FTemporalEvent> GetMostSalientEvents(int32 Count = 10) const;

    /** Get most salient events in time range */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Salience")
    TArray<FTemporalEvent> GetMostSalientInRange(float StartTime, float EndTime, int32 Count = 10) const;

    /** Get events by importance */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Salience")
    TArray<FTemporalEvent> GetEventsByImportance(ETemporalEventImportance MinImportance) const;

    // ========================================
    // TEMPORAL QUERIES
    // ========================================

    /** Query events by tags */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Queries")
    FTemporalQueryResult QueryByTags(const TArray<FString>& Tags, int32 MaxResults = 10) const;

    /** Query events by emotional context */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Queries")
    FTemporalQueryResult QueryByEmotion(float TargetValence, float ValenceTolerance = 0.3f, int32 MaxResults = 10) const;

    /** Query recent events */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Queries")
    FTemporalQueryResult QueryRecent(float SecondsAgo, int32 MaxResults = 10) const;

    /** Query linked events (causal chain) */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Queries")
    TArray<FTemporalEvent> GetLinkedEvents(const FString& EventID, int32 MaxDepth = 3) const;

    /** Query similar events by feature vector */
    UFUNCTION(BlueprintCallable, Category = "TemporalAttention|Queries")
    TArray<FTemporalEvent> QuerySimilarEvents(const TArray<float>& Features, float Threshold = 0.5f, int32 MaxResults = 10) const;

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get current temporal attention state */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    FTemporalAttentionState GetAttentionState() const;

    /** Get all tracked events */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    TArray<FTemporalEvent> GetAllEvents() const;

    /** Get all windows */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    TArray<FTemporalWindow> GetAllWindows() const;

    /** Get focused events */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    TArray<FTemporalEvent> GetFocusedEvents() const;

    /** Get event count */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    int32 GetEventCount() const;

    /** Get temporal scale for time offset */
    UFUNCTION(BlueprintPure, Category = "TemporalAttention|State")
    ETemporalScale GetScaleForTime(float TimeOffset) const;

protected:
    // Component references
    UPROPERTY()
    UAttentionSystem* AttentionSystem;

    UPROPERTY()
    UECANAttentionAllocation* ECANSystem;

    UPROPERTY()
    UEpisodicMemorySystem* MemorySystem;

    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    // Internal state
    TMap<FString, FTemporalEvent> Events;
    TMap<FString, FTemporalWindow> Windows;
    TArray<FString> FocusedEventIDs;
    FString CurrentFocusWindowID;
    ETemporalAttentionMode CurrentMode = ETemporalAttentionMode::Present;

    int32 EventIDCounter = 0;
    int32 WindowIDCounter = 0;
    float CurrentTime = 0.0f;

    // Tag-based attention boosts
    TMap<FString, float> TagBoosts;

    // Internal methods
    void FindComponentReferences();
    void InitializeTemporalAttention();

    void UpdateAttentionWeights(float DeltaTime);
    void UpdateEventSalience(float DeltaTime);
    void ProcessEventExpiry();
    void UpdateFocusedEvents();

    float ComputeRecencySalience(const FTemporalEvent& Event) const;
    float ComputeImportanceSalience(const FTemporalEvent& Event) const;
    float ComputeEmotionalSalience(const FTemporalEvent& Event) const;
    float ComputeTagBoost(const FTemporalEvent& Event) const;
    float ComputeTemporalDecay(float Age) const;

    FString GenerateEventID();
    FString GenerateWindowID();
    void UpdateWindowEvents(const FString& WindowID);
};
