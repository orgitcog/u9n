// TaskflowProfiler.h
// Performance profiling and visualization for Taskflow-based cognitive scheduling
// Provides detailed metrics, timeline visualization, and performance analytics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/CircularBuffer.h"

#include "TaskflowProfiler.generated.h"

// Forward declarations
class UTaskflowCognitiveScheduler;

/**
 * Profile event type for timeline tracking
 */
UENUM(BlueprintType)
enum class EProfileEventType : uint8
{
    TaskStart       UMETA(DisplayName = "Task Start"),
    TaskEnd         UMETA(DisplayName = "Task End"),
    SyncBarrier     UMETA(DisplayName = "Sync Barrier"),
    WorkSteal       UMETA(DisplayName = "Work Steal"),
    GraphSubmit     UMETA(DisplayName = "Graph Submit"),
    GraphComplete   UMETA(DisplayName = "Graph Complete"),
    StreamProcess   UMETA(DisplayName = "Stream Process"),
    OperadStep      UMETA(DisplayName = "Operad Step")
};

/**
 * Single profiling event record
 */
USTRUCT(BlueprintType)
struct FProfileEvent
{
    GENERATED_BODY()

    /** Timestamp in microseconds from session start */
    UPROPERTY(BlueprintReadOnly)
    int64 TimestampUs = 0;

    /** Event type */
    UPROPERTY(BlueprintReadOnly)
    EProfileEventType EventType = EProfileEventType::TaskStart;

    /** Worker thread ID */
    UPROPERTY(BlueprintReadOnly)
    int32 WorkerID = -1;

    /** Associated task/graph name */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** Duration in microseconds (for end events) */
    UPROPERTY(BlueprintReadOnly)
    int64 DurationUs = 0;

    /** Additional metadata */
    UPROPERTY(BlueprintReadOnly)
    FString Metadata;
};

/**
 * Timeline segment for visualization
 */
USTRUCT(BlueprintType)
struct FTimelineSegment
{
    GENERATED_BODY()

    /** Worker thread ID */
    UPROPERTY(BlueprintReadOnly)
    int32 WorkerID = 0;

    /** Start time in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 StartTimeUs = 0;

    /** End time in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 EndTimeUs = 0;

    /** Task/operation name */
    UPROPERTY(BlueprintReadOnly)
    FString TaskName;

    /** Task domain for color coding */
    UPROPERTY(BlueprintReadOnly)
    FString Domain;
};

/**
 * Per-worker statistics
 */
USTRUCT(BlueprintType)
struct FWorkerStats
{
    GENERATED_BODY()

    /** Worker thread ID */
    UPROPERTY(BlueprintReadOnly)
    int32 WorkerID = 0;

    /** Total tasks executed */
    UPROPERTY(BlueprintReadOnly)
    int32 TasksExecuted = 0;

    /** Total execution time in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalExecutionTimeUs = 0;

    /** Idle time in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 IdleTimeUs = 0;

    /** Work steals performed */
    UPROPERTY(BlueprintReadOnly)
    int32 WorkSteals = 0;

    /** Utilization percentage */
    UPROPERTY(BlueprintReadOnly)
    float Utilization = 0.0f;

    /** Average task duration in microseconds */
    UPROPERTY(BlueprintReadOnly)
    float AvgTaskDurationUs = 0.0f;
};

/**
 * Cognitive domain statistics
 */
USTRUCT(BlueprintType)
struct FDomainStats
{
    GENERATED_BODY()

    /** Domain name */
    UPROPERTY(BlueprintReadOnly)
    FString DomainName;

    /** Total tasks in this domain */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTasks = 0;

    /** Total execution time in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalTimeUs = 0;

    /** Average task duration */
    UPROPERTY(BlueprintReadOnly)
    float AvgTaskDurationUs = 0.0f;

    /** Peak task duration */
    UPROPERTY(BlueprintReadOnly)
    float PeakTaskDurationUs = 0.0f;

    /** Percentage of total execution time */
    UPROPERTY(BlueprintReadOnly)
    float TimePercentage = 0.0f;
};

/**
 * Critical path information
 */
USTRUCT(BlueprintType)
struct FCriticalPath
{
    GENERATED_BODY()

    /** Tasks on the critical path (in order) */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Tasks;

    /** Total critical path duration in microseconds */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalDurationUs = 0;

    /** Longest single task on path */
    UPROPERTY(BlueprintReadOnly)
    FString LongestTask;

    /** Duration of longest task */
    UPROPERTY(BlueprintReadOnly)
    int64 LongestTaskDurationUs = 0;

    /** Parallelism achieved (total work / critical path) */
    UPROPERTY(BlueprintReadOnly)
    float ParallelismFactor = 1.0f;
};

/**
 * Profile session summary
 */
USTRUCT(BlueprintType)
struct FProfileSession
{
    GENERATED_BODY()

    /** Session identifier */
    UPROPERTY(BlueprintReadOnly)
    FGuid SessionID;

    /** Session start time */
    UPROPERTY(BlueprintReadOnly)
    FDateTime StartTime;

    /** Total duration in seconds */
    UPROPERTY(BlueprintReadOnly)
    float DurationSeconds = 0.0f;

    /** Total events recorded */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalEvents = 0;

    /** Total graphs executed */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalGraphs = 0;

    /** Total tasks executed */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTasks = 0;

    /** Average parallel efficiency */
    UPROPERTY(BlueprintReadOnly)
    float AvgParallelEfficiency = 0.0f;

    /** Per-worker statistics */
    UPROPERTY(BlueprintReadOnly)
    TArray<FWorkerStats> WorkerStats;

    /** Per-domain statistics */
    UPROPERTY(BlueprintReadOnly)
    TArray<FDomainStats> DomainStats;

    /** Critical path analysis */
    UPROPERTY(BlueprintReadOnly)
    FCriticalPath CriticalPath;

    FProfileSession()
    {
        SessionID = FGuid::NewGuid();
        StartTime = FDateTime::Now();
    }
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProfileEventRecorded, FProfileEvent, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionStarted, FGuid, SessionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionEnded, FProfileSession, Session);

/**
 * TaskflowProfiler
 *
 * Comprehensive profiling and performance analysis for Taskflow-based
 * cognitive task scheduling. Provides:
 * - Real-time event tracking
 * - Per-worker utilization analysis
 * - Domain-level performance breakdown
 * - Critical path analysis
 * - Timeline visualization data
 * - Export to various formats (JSON, Chrome Trace, TFProf)
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTaskflowProfiler : public UActorComponent
{
    GENERATED_BODY()

public:
    UTaskflowProfiler();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable profiling (can impact performance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration")
    bool bEnableProfiling = true;

    /** Maximum events to store in memory */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration")
    int32 MaxStoredEvents = 100000;

    /** Enable real-time analysis (higher overhead) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration")
    bool bEnableRealTimeAnalysis = false;

    /** Event sampling rate (1.0 = all events, 0.1 = 10%) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration",
              meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float SamplingRate = 1.0f;

    /** Auto-export sessions when they end */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration")
    bool bAutoExportSessions = false;

    /** Export directory for auto-export */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiler Configuration")
    FString ExportDirectory;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Profiler Events")
    FOnProfileEventRecorded OnEventRecorded;

    UPROPERTY(BlueprintAssignable, Category = "Profiler Events")
    FOnSessionStarted OnSessionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Profiler Events")
    FOnSessionEnded OnSessionEnded;

    // ========================================
    // SESSION CONTROL
    // ========================================

    /** Start a new profiling session */
    UFUNCTION(BlueprintCallable, Category = "Profiler Control")
    FGuid StartSession(const FString& SessionName = TEXT(""));

    /** End the current session and generate summary */
    UFUNCTION(BlueprintCallable, Category = "Profiler Control")
    FProfileSession EndSession();

    /** Check if a session is active */
    UFUNCTION(BlueprintPure, Category = "Profiler Control")
    bool IsSessionActive() const;

    /** Get the current session ID */
    UFUNCTION(BlueprintPure, Category = "Profiler Control")
    FGuid GetCurrentSessionID() const;

    /** Clear all recorded events */
    UFUNCTION(BlueprintCallable, Category = "Profiler Control")
    void ClearEvents();

    // ========================================
    // EVENT RECORDING
    // ========================================

    /** Record a task start event */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordTaskStart(const FString& TaskName, int32 WorkerID = -1, const FString& Domain = TEXT(""));

    /** Record a task end event */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordTaskEnd(const FString& TaskName, int32 WorkerID = -1, int64 DurationUs = 0);

    /** Record a sync barrier event */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordSyncBarrier(const FString& BarrierName);

    /** Record a work steal event */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordWorkSteal(int32 FromWorker, int32 ToWorker);

    /** Record a graph submission */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordGraphSubmit(const FString& GraphName, int32 TaskCount);

    /** Record a graph completion */
    UFUNCTION(BlueprintCallable, Category = "Profiler Recording")
    void RecordGraphComplete(const FString& GraphName, int64 DurationUs);

    // ========================================
    // ANALYSIS
    // ========================================

    /** Get worker statistics */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    TArray<FWorkerStats> GetWorkerStats() const;

    /** Get domain statistics */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    TArray<FDomainStats> GetDomainStats() const;

    /** Get critical path information */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    FCriticalPath GetCriticalPath() const;

    /** Get timeline segments for visualization */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    TArray<FTimelineSegment> GetTimelineSegments(int32 MaxSegments = 1000) const;

    /** Get recent events */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    TArray<FProfileEvent> GetRecentEvents(int32 Count = 100) const;

    /** Calculate parallel efficiency */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    float CalculateParallelEfficiency() const;

    /** Get total recorded events count */
    UFUNCTION(BlueprintPure, Category = "Profiler Analysis")
    int32 GetEventCount() const;

    // ========================================
    // EXPORT
    // ========================================

    /** Export to JSON format */
    UFUNCTION(BlueprintCallable, Category = "Profiler Export")
    bool ExportToJSON(const FString& FilePath);

    /** Export to Chrome Trace format (for chrome://tracing) */
    UFUNCTION(BlueprintCallable, Category = "Profiler Export")
    bool ExportToChromeTrace(const FString& FilePath);

    /** Export summary report as text */
    UFUNCTION(BlueprintCallable, Category = "Profiler Export")
    bool ExportSummaryReport(const FString& FilePath);

    /** Generate HTML visualization */
    UFUNCTION(BlueprintCallable, Category = "Profiler Export")
    bool ExportHTMLVisualization(const FString& FilePath);

protected:
    // Reference to scheduler
    UPROPERTY()
    UTaskflowCognitiveScheduler* Scheduler;

    // Session state
    FProfileSession CurrentSession;
    FThreadSafeBool bSessionActive;
    double SessionStartTimeSeconds;

    // Event storage
    TArray<FProfileEvent> Events;
    FCriticalSection EventLock;

    // Real-time stats
    TMap<int32, FWorkerStats> WorkerStatsMap;
    TMap<FString, FDomainStats> DomainStatsMap;
    FCriticalSection StatsLock;

    // Internal methods
    void FindSchedulerReference();
    int64 GetTimestampUs() const;
    void UpdateRealTimeStats(const FProfileEvent& Event);
    void ComputeSessionSummary();
    FString GenerateChromeTraceJSON() const;
    FString GenerateHTMLContent() const;
};
