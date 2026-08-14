// TaskflowProfiler.cpp
// Implementation of performance profiling for Taskflow-based cognitive scheduling

#include "TaskflowProfiler.h"
#include "TaskflowCognitiveScheduler.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UTaskflowProfiler::UTaskflowProfiler()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    bEnableProfiling = true;
    MaxStoredEvents = 100000;
    bEnableRealTimeAnalysis = false;
    SamplingRate = 1.0f;
    bAutoExportSessions = false;
    ExportDirectory = FPaths::ProjectSavedDir() / TEXT("Profiling");
}

void UTaskflowProfiler::BeginPlay()
{
    Super::BeginPlay();
    FindSchedulerReference();
}

void UTaskflowProfiler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bSessionActive)
    {
        EndSession();
    }
    Super::EndPlay(EndPlayReason);
}

void UTaskflowProfiler::FindSchedulerReference()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Scheduler = Owner->FindComponentByClass<UTaskflowCognitiveScheduler>();
    }
}

void UTaskflowProfiler::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    // Update session duration
    CurrentSession.DurationSeconds = static_cast<float>(
        FPlatformTime::Seconds() - SessionStartTimeSeconds
    );
}

// ========================================
// SESSION CONTROL
// ========================================

FGuid UTaskflowProfiler::StartSession(const FString& SessionName)
{
    if (bSessionActive)
    {
        EndSession();
    }

    CurrentSession = FProfileSession();
    CurrentSession.SessionID = FGuid::NewGuid();
    CurrentSession.StartTime = FDateTime::Now();

    SessionStartTimeSeconds = FPlatformTime::Seconds();
    bSessionActive = true;

    // Clear previous data
    {
        FScopeLock Lock(&EventLock);
        Events.Empty();
    }

    {
        FScopeLock Lock(&StatsLock);
        WorkerStatsMap.Empty();
        DomainStatsMap.Empty();
    }

    UE_LOG(LogTemp, Log, TEXT("TaskflowProfiler: Session started - %s"), *CurrentSession.SessionID.ToString());

    OnSessionStarted.Broadcast(CurrentSession.SessionID);

    return CurrentSession.SessionID;
}

FProfileSession UTaskflowProfiler::EndSession()
{
    if (!bSessionActive)
    {
        return FProfileSession();
    }

    bSessionActive = false;

    // Compute final statistics
    ComputeSessionSummary();

    UE_LOG(LogTemp, Log, TEXT("TaskflowProfiler: Session ended - Duration: %.2f seconds, Events: %d"),
           CurrentSession.DurationSeconds, CurrentSession.TotalEvents);

    // Auto-export if enabled
    if (bAutoExportSessions && !ExportDirectory.IsEmpty())
    {
        FString FileName = FString::Printf(TEXT("profile_%s.json"),
                                           *CurrentSession.SessionID.ToString());
        FString FilePath = ExportDirectory / FileName;
        ExportToJSON(FilePath);
    }

    OnSessionEnded.Broadcast(CurrentSession);

    return CurrentSession;
}

bool UTaskflowProfiler::IsSessionActive() const
{
    return bSessionActive;
}

FGuid UTaskflowProfiler::GetCurrentSessionID() const
{
    return CurrentSession.SessionID;
}

void UTaskflowProfiler::ClearEvents()
{
    FScopeLock Lock(&EventLock);
    Events.Empty();
}

// ========================================
// EVENT RECORDING
// ========================================

int64 UTaskflowProfiler::GetTimestampUs() const
{
    return static_cast<int64>((FPlatformTime::Seconds() - SessionStartTimeSeconds) * 1000000.0);
}

void UTaskflowProfiler::RecordTaskStart(const FString& TaskName, int32 WorkerID, const FString& Domain)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    // Apply sampling
    if (SamplingRate < 1.0f && FMath::FRand() > SamplingRate)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::TaskStart;
    Event.WorkerID = WorkerID;
    Event.Name = TaskName;
    Event.Metadata = Domain;

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    if (bEnableRealTimeAnalysis)
    {
        UpdateRealTimeStats(Event);
    }

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::RecordTaskEnd(const FString& TaskName, int32 WorkerID, int64 DurationUs)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    if (SamplingRate < 1.0f && FMath::FRand() > SamplingRate)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::TaskEnd;
    Event.WorkerID = WorkerID;
    Event.Name = TaskName;
    Event.DurationUs = DurationUs;

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    if (bEnableRealTimeAnalysis)
    {
        UpdateRealTimeStats(Event);
    }

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::RecordSyncBarrier(const FString& BarrierName)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::SyncBarrier;
    Event.Name = BarrierName;

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::RecordWorkSteal(int32 FromWorker, int32 ToWorker)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::WorkSteal;
    Event.WorkerID = ToWorker;
    Event.Metadata = FString::Printf(TEXT("From:%d"), FromWorker);

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    if (bEnableRealTimeAnalysis)
    {
        FScopeLock Lock(&StatsLock);
        if (FWorkerStats* Stats = WorkerStatsMap.Find(ToWorker))
        {
            Stats->WorkSteals++;
        }
    }

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::RecordGraphSubmit(const FString& GraphName, int32 TaskCount)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::GraphSubmit;
    Event.Name = GraphName;
    Event.Metadata = FString::Printf(TEXT("Tasks:%d"), TaskCount);

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    CurrentSession.TotalGraphs++;

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::RecordGraphComplete(const FString& GraphName, int64 DurationUs)
{
    if (!bEnableProfiling || !bSessionActive)
    {
        return;
    }

    FProfileEvent Event;
    Event.TimestampUs = GetTimestampUs();
    Event.EventType = EProfileEventType::GraphComplete;
    Event.Name = GraphName;
    Event.DurationUs = DurationUs;

    {
        FScopeLock Lock(&EventLock);
        if (Events.Num() < MaxStoredEvents)
        {
            Events.Add(Event);
        }
    }

    OnEventRecorded.Broadcast(Event);
}

void UTaskflowProfiler::UpdateRealTimeStats(const FProfileEvent& Event)
{
    FScopeLock Lock(&StatsLock);

    if (Event.WorkerID >= 0)
    {
        FWorkerStats& Stats = WorkerStatsMap.FindOrAdd(Event.WorkerID);
        Stats.WorkerID = Event.WorkerID;

        if (Event.EventType == EProfileEventType::TaskEnd)
        {
            Stats.TasksExecuted++;
            Stats.TotalExecutionTimeUs += Event.DurationUs;
            Stats.AvgTaskDurationUs = static_cast<float>(Stats.TotalExecutionTimeUs) / Stats.TasksExecuted;
        }
    }

    // Update domain stats
    if (!Event.Metadata.IsEmpty() && Event.EventType == EProfileEventType::TaskEnd)
    {
        FDomainStats& DomainStats = DomainStatsMap.FindOrAdd(Event.Metadata);
        DomainStats.DomainName = Event.Metadata;
        DomainStats.TotalTasks++;
        DomainStats.TotalTimeUs += Event.DurationUs;
        DomainStats.AvgTaskDurationUs = static_cast<float>(DomainStats.TotalTimeUs) / DomainStats.TotalTasks;
        DomainStats.PeakTaskDurationUs = FMath::Max(DomainStats.PeakTaskDurationUs,
                                                     static_cast<float>(Event.DurationUs));
    }
}

// ========================================
// ANALYSIS
// ========================================

TArray<FWorkerStats> UTaskflowProfiler::GetWorkerStats() const
{
    FScopeLock Lock(&StatsLock);

    TArray<FWorkerStats> Result;
    WorkerStatsMap.GenerateValueArray(Result);
    return Result;
}

TArray<FDomainStats> UTaskflowProfiler::GetDomainStats() const
{
    FScopeLock Lock(&StatsLock);

    TArray<FDomainStats> Result;
    DomainStatsMap.GenerateValueArray(Result);
    return Result;
}

FCriticalPath UTaskflowProfiler::GetCriticalPath() const
{
    return CurrentSession.CriticalPath;
}

TArray<FTimelineSegment> UTaskflowProfiler::GetTimelineSegments(int32 MaxSegments) const
{
    TArray<FTimelineSegment> Segments;
    FScopeLock Lock(&EventLock);

    TMap<FString, int64> TaskStartTimes;

    for (const FProfileEvent& Event : Events)
    {
        if (Segments.Num() >= MaxSegments)
        {
            break;
        }

        if (Event.EventType == EProfileEventType::TaskStart)
        {
            TaskStartTimes.Add(Event.Name, Event.TimestampUs);
        }
        else if (Event.EventType == EProfileEventType::TaskEnd)
        {
            if (int64* StartTime = TaskStartTimes.Find(Event.Name))
            {
                FTimelineSegment Segment;
                Segment.WorkerID = Event.WorkerID;
                Segment.StartTimeUs = *StartTime;
                Segment.EndTimeUs = Event.TimestampUs;
                Segment.TaskName = Event.Name;
                Segment.Domain = Event.Metadata;
                Segments.Add(Segment);

                TaskStartTimes.Remove(Event.Name);
            }
        }
    }

    return Segments;
}

TArray<FProfileEvent> UTaskflowProfiler::GetRecentEvents(int32 Count) const
{
    TArray<FProfileEvent> Result;
    FScopeLock Lock(&EventLock);

    int32 StartIndex = FMath::Max(0, Events.Num() - Count);
    for (int32 i = StartIndex; i < Events.Num(); ++i)
    {
        Result.Add(Events[i]);
    }

    return Result;
}

float UTaskflowProfiler::CalculateParallelEfficiency() const
{
    FScopeLock Lock(&StatsLock);

    if (WorkerStatsMap.Num() == 0)
    {
        return 1.0f;
    }

    int64 TotalWork = 0;
    int64 MaxWorkerTime = 0;

    for (const auto& Pair : WorkerStatsMap)
    {
        TotalWork += Pair.Value.TotalExecutionTimeUs;
        MaxWorkerTime = FMath::Max(MaxWorkerTime, Pair.Value.TotalExecutionTimeUs);
    }

    if (MaxWorkerTime == 0)
    {
        return 1.0f;
    }

    // Efficiency = (Total Work / (Workers * Max Worker Time))
    float IdealParallelTime = static_cast<float>(TotalWork) / WorkerStatsMap.Num();
    return IdealParallelTime / MaxWorkerTime;
}

int32 UTaskflowProfiler::GetEventCount() const
{
    FScopeLock Lock(&EventLock);
    return Events.Num();
}

void UTaskflowProfiler::ComputeSessionSummary()
{
    FScopeLock EventLockGuard(&EventLock);
    FScopeLock StatsLockGuard(&StatsLock);

    CurrentSession.TotalEvents = Events.Num();
    CurrentSession.TotalTasks = 0;

    // Count completed tasks
    for (const FProfileEvent& Event : Events)
    {
        if (Event.EventType == EProfileEventType::TaskEnd)
        {
            CurrentSession.TotalTasks++;
        }
    }

    // Collect worker stats
    CurrentSession.WorkerStats.Empty();
    for (const auto& Pair : WorkerStatsMap)
    {
        CurrentSession.WorkerStats.Add(Pair.Value);
    }

    // Collect domain stats
    CurrentSession.DomainStats.Empty();
    int64 TotalDomainTime = 0;
    for (auto& Pair : DomainStatsMap)
    {
        TotalDomainTime += Pair.Value.TotalTimeUs;
    }

    for (auto& Pair : DomainStatsMap)
    {
        if (TotalDomainTime > 0)
        {
            Pair.Value.TimePercentage = 100.0f * Pair.Value.TotalTimeUs / TotalDomainTime;
        }
        CurrentSession.DomainStats.Add(Pair.Value);
    }

    // Calculate parallel efficiency
    CurrentSession.AvgParallelEfficiency = CalculateParallelEfficiency();

    // Compute critical path (simplified)
    CurrentSession.CriticalPath.ParallelismFactor = CurrentSession.AvgParallelEfficiency * WorkerStatsMap.Num();
}

// ========================================
// EXPORT
// ========================================

bool UTaskflowProfiler::ExportToJSON(const FString& FilePath)
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    // Session info
    RootObject->SetStringField(TEXT("session_id"), CurrentSession.SessionID.ToString());
    RootObject->SetStringField(TEXT("start_time"), CurrentSession.StartTime.ToString());
    RootObject->SetNumberField(TEXT("duration_seconds"), CurrentSession.DurationSeconds);
    RootObject->SetNumberField(TEXT("total_events"), CurrentSession.TotalEvents);
    RootObject->SetNumberField(TEXT("total_tasks"), CurrentSession.TotalTasks);
    RootObject->SetNumberField(TEXT("parallel_efficiency"), CurrentSession.AvgParallelEfficiency);

    // Events array
    TArray<TSharedPtr<FJsonValue>> EventsArray;
    {
        FScopeLock Lock(&EventLock);
        for (const FProfileEvent& Event : Events)
        {
            TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
            EventObj->SetNumberField(TEXT("timestamp_us"), Event.TimestampUs);
            EventObj->SetStringField(TEXT("type"), UEnum::GetValueAsString(Event.EventType));
            EventObj->SetNumberField(TEXT("worker_id"), Event.WorkerID);
            EventObj->SetStringField(TEXT("name"), Event.Name);
            EventObj->SetNumberField(TEXT("duration_us"), Event.DurationUs);
            EventObj->SetStringField(TEXT("metadata"), Event.Metadata);
            EventsArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
        }
    }
    RootObject->SetArrayField(TEXT("events"), EventsArray);

    // Serialize
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool UTaskflowProfiler::ExportToChromeTrace(const FString& FilePath)
{
    FString TraceJSON = GenerateChromeTraceJSON();
    return FFileHelper::SaveStringToFile(TraceJSON, *FilePath);
}

FString UTaskflowProfiler::GenerateChromeTraceJSON() const
{
    TArray<TSharedPtr<FJsonValue>> TraceEvents;
    FScopeLock Lock(&EventLock);

    TMap<FString, int64> TaskStartTimes;

    for (const FProfileEvent& Event : Events)
    {
        TSharedPtr<FJsonObject> TraceEvent = MakeShareable(new FJsonObject);

        if (Event.EventType == EProfileEventType::TaskStart)
        {
            TaskStartTimes.Add(Event.Name, Event.TimestampUs);

            TraceEvent->SetStringField(TEXT("name"), Event.Name);
            TraceEvent->SetStringField(TEXT("cat"), Event.Metadata.IsEmpty() ? TEXT("task") : Event.Metadata);
            TraceEvent->SetStringField(TEXT("ph"), TEXT("B")); // Begin
            TraceEvent->SetNumberField(TEXT("ts"), Event.TimestampUs);
            TraceEvent->SetNumberField(TEXT("pid"), 1);
            TraceEvent->SetNumberField(TEXT("tid"), Event.WorkerID >= 0 ? Event.WorkerID : 0);
        }
        else if (Event.EventType == EProfileEventType::TaskEnd)
        {
            TraceEvent->SetStringField(TEXT("name"), Event.Name);
            TraceEvent->SetStringField(TEXT("cat"), Event.Metadata.IsEmpty() ? TEXT("task") : Event.Metadata);
            TraceEvent->SetStringField(TEXT("ph"), TEXT("E")); // End
            TraceEvent->SetNumberField(TEXT("ts"), Event.TimestampUs);
            TraceEvent->SetNumberField(TEXT("pid"), 1);
            TraceEvent->SetNumberField(TEXT("tid"), Event.WorkerID >= 0 ? Event.WorkerID : 0);
        }
        else if (Event.EventType == EProfileEventType::SyncBarrier)
        {
            TraceEvent->SetStringField(TEXT("name"), Event.Name);
            TraceEvent->SetStringField(TEXT("cat"), TEXT("sync"));
            TraceEvent->SetStringField(TEXT("ph"), TEXT("i")); // Instant
            TraceEvent->SetNumberField(TEXT("ts"), Event.TimestampUs);
            TraceEvent->SetNumberField(TEXT("pid"), 1);
            TraceEvent->SetNumberField(TEXT("tid"), 0);
            TraceEvent->SetStringField(TEXT("s"), TEXT("g")); // Global scope
        }

        if (TraceEvent->Values.Num() > 0)
        {
            TraceEvents.Add(MakeShareable(new FJsonValueObject(TraceEvent)));
        }
    }

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    RootObject->SetArrayField(TEXT("traceEvents"), TraceEvents);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    return OutputString;
}

bool UTaskflowProfiler::ExportSummaryReport(const FString& FilePath)
{
    FString Report;

    Report += TEXT("================================================================================\n");
    Report += TEXT("                    TASKFLOW COGNITIVE SCHEDULER PROFILING REPORT              \n");
    Report += TEXT("================================================================================\n\n");

    Report += FString::Printf(TEXT("Session ID: %s\n"), *CurrentSession.SessionID.ToString());
    Report += FString::Printf(TEXT("Start Time: %s\n"), *CurrentSession.StartTime.ToString());
    Report += FString::Printf(TEXT("Duration: %.3f seconds\n\n"), CurrentSession.DurationSeconds);

    Report += TEXT("--- SUMMARY ---\n");
    Report += FString::Printf(TEXT("Total Events: %d\n"), CurrentSession.TotalEvents);
    Report += FString::Printf(TEXT("Total Tasks: %d\n"), CurrentSession.TotalTasks);
    Report += FString::Printf(TEXT("Total Graphs: %d\n"), CurrentSession.TotalGraphs);
    Report += FString::Printf(TEXT("Parallel Efficiency: %.1f%%\n\n"), CurrentSession.AvgParallelEfficiency * 100.0f);

    Report += TEXT("--- WORKER STATISTICS ---\n");
    for (const FWorkerStats& Stats : CurrentSession.WorkerStats)
    {
        Report += FString::Printf(TEXT("Worker %d:\n"), Stats.WorkerID);
        Report += FString::Printf(TEXT("  Tasks Executed: %d\n"), Stats.TasksExecuted);
        Report += FString::Printf(TEXT("  Total Time: %.3f ms\n"), Stats.TotalExecutionTimeUs / 1000.0f);
        Report += FString::Printf(TEXT("  Avg Task Duration: %.3f us\n"), Stats.AvgTaskDurationUs);
        Report += FString::Printf(TEXT("  Work Steals: %d\n"), Stats.WorkSteals);
    }

    Report += TEXT("\n--- DOMAIN STATISTICS ---\n");
    for (const FDomainStats& Stats : CurrentSession.DomainStats)
    {
        Report += FString::Printf(TEXT("%s:\n"), *Stats.DomainName);
        Report += FString::Printf(TEXT("  Tasks: %d\n"), Stats.TotalTasks);
        Report += FString::Printf(TEXT("  Total Time: %.3f ms (%.1f%%)\n"),
                                  Stats.TotalTimeUs / 1000.0f, Stats.TimePercentage);
        Report += FString::Printf(TEXT("  Avg Duration: %.3f us\n"), Stats.AvgTaskDurationUs);
        Report += FString::Printf(TEXT("  Peak Duration: %.3f us\n"), Stats.PeakTaskDurationUs);
    }

    Report += TEXT("\n================================================================================\n");

    return FFileHelper::SaveStringToFile(Report, *FilePath);
}

bool UTaskflowProfiler::ExportHTMLVisualization(const FString& FilePath)
{
    FString HTML = GenerateHTMLContent();
    return FFileHelper::SaveStringToFile(HTML, *FilePath);
}

FString UTaskflowProfiler::GenerateHTMLContent() const
{
    FString HTML;

    HTML += TEXT("<!DOCTYPE html>\n<html>\n<head>\n");
    HTML += TEXT("<title>Taskflow Cognitive Scheduler - Profile Visualization</title>\n");
    HTML += TEXT("<style>\n");
    HTML += TEXT("body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #1a1a2e; color: #eee; }\n");
    HTML += TEXT("h1 { color: #00d4ff; }\n");
    HTML += TEXT("h2 { color: #ff6b6b; border-bottom: 1px solid #444; padding-bottom: 5px; }\n");
    HTML += TEXT(".stats-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin: 20px 0; }\n");
    HTML += TEXT(".stat-card { background: #16213e; padding: 20px; border-radius: 10px; text-align: center; }\n");
    HTML += TEXT(".stat-value { font-size: 2em; color: #00d4ff; }\n");
    HTML += TEXT(".stat-label { color: #888; }\n");
    HTML += TEXT(".timeline { background: #16213e; padding: 20px; border-radius: 10px; margin: 20px 0; }\n");
    HTML += TEXT("table { width: 100%; border-collapse: collapse; }\n");
    HTML += TEXT("th, td { padding: 10px; text-align: left; border-bottom: 1px solid #333; }\n");
    HTML += TEXT("th { background: #0f3460; color: #00d4ff; }\n");
    HTML += TEXT("</style>\n");
    HTML += TEXT("</head>\n<body>\n");

    HTML += TEXT("<h1>Taskflow Cognitive Scheduler Profile</h1>\n");

    // Summary cards
    HTML += TEXT("<div class='stats-grid'>\n");
    HTML += FString::Printf(TEXT("<div class='stat-card'><div class='stat-value'>%d</div><div class='stat-label'>Total Tasks</div></div>\n"),
                            CurrentSession.TotalTasks);
    HTML += FString::Printf(TEXT("<div class='stat-card'><div class='stat-value'>%.1f%%</div><div class='stat-label'>Parallel Efficiency</div></div>\n"),
                            CurrentSession.AvgParallelEfficiency * 100.0f);
    HTML += FString::Printf(TEXT("<div class='stat-card'><div class='stat-value'>%.2fs</div><div class='stat-label'>Duration</div></div>\n"),
                            CurrentSession.DurationSeconds);
    HTML += FString::Printf(TEXT("<div class='stat-card'><div class='stat-value'>%d</div><div class='stat-label'>Workers</div></div>\n"),
                            CurrentSession.WorkerStats.Num());
    HTML += TEXT("</div>\n");

    // Worker stats table
    HTML += TEXT("<h2>Worker Statistics</h2>\n");
    HTML += TEXT("<table>\n<tr><th>Worker</th><th>Tasks</th><th>Total Time</th><th>Avg Duration</th><th>Steals</th></tr>\n");
    for (const FWorkerStats& Stats : CurrentSession.WorkerStats)
    {
        HTML += FString::Printf(TEXT("<tr><td>%d</td><td>%d</td><td>%.2f ms</td><td>%.1f us</td><td>%d</td></tr>\n"),
                                Stats.WorkerID, Stats.TasksExecuted,
                                Stats.TotalExecutionTimeUs / 1000.0f, Stats.AvgTaskDurationUs, Stats.WorkSteals);
    }
    HTML += TEXT("</table>\n");

    // Domain stats table
    HTML += TEXT("<h2>Domain Statistics</h2>\n");
    HTML += TEXT("<table>\n<tr><th>Domain</th><th>Tasks</th><th>Time %</th><th>Peak Duration</th></tr>\n");
    for (const FDomainStats& Stats : CurrentSession.DomainStats)
    {
        HTML += FString::Printf(TEXT("<tr><td>%s</td><td>%d</td><td>%.1f%%</td><td>%.1f us</td></tr>\n"),
                                *Stats.DomainName, Stats.TotalTasks, Stats.TimePercentage, Stats.PeakTaskDurationUs);
    }
    HTML += TEXT("</table>\n");

    HTML += TEXT("</body>\n</html>\n");

    return HTML;
}
