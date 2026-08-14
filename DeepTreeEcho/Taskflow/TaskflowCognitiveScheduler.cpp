// TaskflowCognitiveScheduler.cpp
// Implementation of high-performance parallel task scheduling for Deep Tree Echo

#include "TaskflowCognitiveScheduler.h"
#include "CognitiveCycleManager.h"
#include "Sys6OperadEngine.h"
#include "DeepTreeEchoReservoir.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"

UTaskflowCognitiveScheduler::UTaskflowCognitiveScheduler()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Default configuration
    WorkerThreadCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    if (WorkerThreadCount > 8) WorkerThreadCount = 8;
    if (WorkerThreadCount < 2) WorkerThreadCount = 2;

    bAutoConstructGraphs = true;
    bEnableProfiling = true;
    MaxPendingTasks = 256;

    // Initialize stream configs for 3 cognitive streams
    InitializeStreamConfigs();
}

UTaskflowCognitiveScheduler::~UTaskflowCognitiveScheduler()
{
    Shutdown();
}

void UTaskflowCognitiveScheduler::InitializeStreamConfigs()
{
    StreamConfigs.Empty();
    for (int32 i = 1; i <= 3; ++i)
    {
        FStreamProcessingConfig Config;
        Config.StreamID = i;
        Config.bEnableParallel = true;
        Config.MaxConcurrentTasks = 4;
        Config.BatchSize = 32;
        Config.bEnableWorkStealing = true;
        StreamConfigs.Add(Config);
    }
}

void UTaskflowCognitiveScheduler::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    Initialize();
}

void UTaskflowCognitiveScheduler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}

void UTaskflowCognitiveScheduler::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        OperadEngine = Owner->FindComponentByClass<USys6OperadEngine>();
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

bool UTaskflowCognitiveScheduler::Initialize()
{
    if (bIsInitialized)
    {
        return true;
    }

#ifndef TASKFLOW_FALLBACK_MODE
    // Create Taskflow executor with specified worker count
    Executor = MakeUnique<tf::Executor>(WorkerThreadCount);

    // Pre-build task graphs if auto-construction is enabled
    if (bAutoConstructGraphs)
    {
        CognitiveGraph = MakeUnique<tf::Taskflow>("CognitiveStreams");
        OperadGraph = MakeUnique<tf::Taskflow>("Sys6Operad");
        ReservoirGraph = MakeUnique<tf::Taskflow>("ReservoirHierarchy");
        Cognition4EGraph = MakeUnique<tf::Taskflow>("4ECognition");

        BuildCognitiveStreamGraph();
        BuildOperadGraph();
        BuildReservoirGraph();
        Build4ECognitionGraph();
    }
#endif

    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("TaskflowCognitiveScheduler: Initialized with %d worker threads"), WorkerThreadCount);

    return true;
}

void UTaskflowCognitiveScheduler::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    bIsShuttingDown = true;

#ifndef TASKFLOW_FALLBACK_MODE
    // Wait for all pending work to complete
    if (Executor)
    {
        Executor->wait_for_all();
    }

    // Clear task graphs
    CognitiveGraph.Reset();
    OperadGraph.Reset();
    ReservoirGraph.Reset();
    Cognition4EGraph.Reset();
    Executor.Reset();
#endif

    // Clear task maps
    {
        FScopeLock Lock(&TaskMapLock);
        PendingTasks.Empty();
        CompletedTasks.Empty();
    }

    bIsInitialized = false;
    bIsShuttingDown = false;

    UE_LOG(LogTemp, Log, TEXT("TaskflowCognitiveScheduler: Shutdown complete"));
}

bool UTaskflowCognitiveScheduler::IsInitialized() const
{
    return bIsInitialized;
}

void UTaskflowCognitiveScheduler::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsInitialized || bIsShuttingDown)
    {
        return;
    }

    // Update metrics
    if (bEnableProfiling)
    {
        UpdateMetrics(DeltaTime);
    }

    // Process completed task callbacks
    ProcessCompletedTasks();

    // Apply backpressure if too many pending tasks
    if (PendingTasks.Num() > MaxPendingTasks)
    {
        ApplyBackpressure();
    }
}

// ========================================
// COGNITIVE STREAM SCHEDULING
// ========================================

void UTaskflowCognitiveScheduler::ExecuteCognitiveStreamGraph(int32 CycleStep,
                                                               const TArray<TArray<float>>& StreamInputs)
{
    if (!bIsInitialized || StreamInputs.Num() < 3)
    {
        return;
    }

    double StartTime = FPlatformTime::Seconds();

#ifndef TASKFLOW_FALLBACK_MODE
    // Create a new taskflow for this cycle step
    tf::Taskflow StreamFlow("CycleStep_" + std::to_string(CycleStep));

    // Stream output storage
    TArray<TArray<float>> StreamOutputs;
    StreamOutputs.SetNum(3);

    // Create parallel stream tasks
    std::vector<tf::Task> StreamTasks(3);

    for (int32 i = 0; i < 3; ++i)
    {
        const int32 StreamID = i + 1;
        const TArray<float>& Input = StreamInputs[i];

        StreamTasks[i] = StreamFlow.emplace([this, StreamID, &Input, &StreamOutputs, i]() {
            StreamOutputs[i] = ProcessStreamInput(StreamID, Input);
        }).name("Stream_" + std::to_string(StreamID));
    }

    // Create sync barrier task based on current step
    int32 TriadIndex = (CycleStep - 1) % 4;
    bool bIsTriadicSyncPoint = (CycleStep == 1 || CycleStep == 5 || CycleStep == 9 ||
                                 CycleStep == 2 || CycleStep == 6 || CycleStep == 10 ||
                                 CycleStep == 3 || CycleStep == 7 || CycleStep == 11 ||
                                 CycleStep == 4 || CycleStep == 8 || CycleStep == 12);

    if (bIsTriadicSyncPoint)
    {
        tf::Task SyncTask = StreamFlow.emplace([this, TriadIndex]() {
            SynchronizeAtBarrier(ESyncBarrierType::TriadicSync);
        }).name("TriadicSync");

        // All streams must complete before sync
        for (auto& Task : StreamTasks)
        {
            Task.precede(SyncTask);
        }
    }

    // Execute the graph
    Executor->run(StreamFlow).wait();
#else
    // Fallback: Sequential processing
    for (int32 i = 0; i < 3; ++i)
    {
        ProcessStreamInput(i + 1, StreamInputs[i]);
    }
#endif

    double EndTime = FPlatformTime::Seconds();
    float DurationMs = static_cast<float>((EndTime - StartTime) * 1000.0);

    // Update metrics
    {
        FScopeLock Lock(&MetricsLock);
        CurrentMetrics.TasksCompleted += 3;
        TaskLatencies.Add(DurationMs);
    }

    OnGraphExecutionComplete.Broadcast(3, DurationMs);
}

TArray<float> UTaskflowCognitiveScheduler::ProcessStreamParallel(int32 StreamID, const TArray<float>& Input)
{
    if (!bIsInitialized || Input.Num() == 0)
    {
        return TArray<float>();
    }

    // Find stream config
    FStreamProcessingConfig* Config = nullptr;
    for (auto& SC : StreamConfigs)
    {
        if (SC.StreamID == StreamID)
        {
            Config = &SC;
            break;
        }
    }

    if (!Config || !Config->bEnableParallel)
    {
        return ProcessStreamInput(StreamID, Input);
    }

#ifndef TASKFLOW_FALLBACK_MODE
    TArray<float> Output;
    Output.SetNum(Input.Num());

    // Use parallel_for for batch processing
    tf::Taskflow ParallelFlow("StreamParallel_" + std::to_string(StreamID));

    const int32 BatchSize = Config->BatchSize;
    const int32 NumBatches = (Input.Num() + BatchSize - 1) / BatchSize;

    ParallelFlow.for_each_index(0, NumBatches, 1, [&](int32 BatchIdx) {
        int32 StartIdx = BatchIdx * BatchSize;
        int32 EndIdx = FMath::Min(StartIdx + BatchSize, Input.Num());

        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            // Apply stream-specific processing (example: activation function)
            Output[i] = FMath::Tanh(Input[i]);
        }
    });

    Executor->run(ParallelFlow).wait();
    return Output;
#else
    return ProcessStreamInput(StreamID, Input);
#endif
}

void UTaskflowCognitiveScheduler::ExecuteTriadicSync(int32 TriadIndex)
{
    SynchronizeAtBarrier(ESyncBarrierType::TriadicSync);

    // Notify sync event
    OnSyncBarrierReached.Broadcast(ESyncBarrierType::TriadicSync);

    // Update metrics
    {
        FScopeLock Lock(&MetricsLock);
        CurrentMetrics.SyncBarriersHit++;
    }
}

TArray<float> UTaskflowCognitiveScheduler::ComputeStreamCoherenceParallel()
{
    TArray<float> CoherenceMatrix;
    CoherenceMatrix.SetNum(9); // 3x3 matrix

#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow CoherenceFlow("StreamCoherence");

    // Compute all 9 coherence values in parallel
    std::vector<tf::Task> CoherenceTasks;

    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            const int32 Idx = i * 3 + j;
            CoherenceTasks.push_back(CoherenceFlow.emplace([&, i, j, Idx]() {
                if (i == j)
                {
                    CoherenceMatrix[Idx] = 1.0f; // Self-coherence
                }
                else
                {
                    // Placeholder for actual coherence computation
                    CoherenceMatrix[Idx] = 0.5f + 0.1f * FMath::Abs(i - j);
                }
            }));
        }
    }

    Executor->run(CoherenceFlow).wait();
#else
    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            int32 Idx = i * 3 + j;
            CoherenceMatrix[Idx] = (i == j) ? 1.0f : 0.5f + 0.1f * FMath::Abs(i - j);
        }
    }
#endif

    return CoherenceMatrix;
}

// ========================================
// SYS6 OPERAD SCHEDULING
// ========================================

void UTaskflowCognitiveScheduler::ExecuteOperadGraph(int32 Sys6Step)
{
    if (!bIsInitialized)
    {
        return;
    }

    double StartTime = FPlatformTime::Seconds();

#ifndef TASKFLOW_FALLBACK_MODE
    // Build the operad DAG: Sys6 := σ ∘ (φ ∘ μ ∘ (Δ₂ ⊗ Δ₃ ⊗ id_P))
    tf::Taskflow OperadFlow("Sys6_Step_" + std::to_string(Sys6Step));

    // Stage 1: Parallel Delta operations (Δ₂ ⊗ Δ₃)
    tf::Task DeltaDyadic, DeltaTriadic;

    if (OperadConfig.bParallelDelta)
    {
        DeltaDyadic = OperadFlow.emplace([this]() {
            ExecuteDyadicDelegation();
        }).name("Delta_Dyadic");

        DeltaTriadic = OperadFlow.emplace([this]() {
            ExecuteTriadicDelegation();
        }).name("Delta_Triadic");
    }

    // Stage 2: μ Synchronizer (depends on both deltas)
    tf::Task MuSync = OperadFlow.emplace([this]() {
        ComputeLCMAlignment();
    }).name("Mu_Synchronizer");

    if (OperadConfig.bParallelDelta)
    {
        DeltaDyadic.precede(MuSync);
        DeltaTriadic.precede(MuSync);
    }

    // Stage 3: φ Fold (depends on μ)
    tf::Task PhiFold = OperadFlow.emplace([this]() {
        ComputeDoubleStepDelay();
    }).name("Phi_Fold");
    MuSync.precede(PhiFold);

    // Stage 4: σ Scheduler (depends on φ)
    if (OperadConfig.bParallelStages)
    {
        // Create 5 parallel stage tasks
        std::vector<tf::Task> StageTasks(5);
        for (int32 i = 0; i < 5; ++i)
        {
            StageTasks[i] = OperadFlow.emplace([this, i]() {
                ScheduleStage(i);
            }).name("Sigma_Stage_" + std::to_string(i + 1));
            PhiFold.precede(StageTasks[i]);
        }
    }
    else
    {
        tf::Task SigmaScheduler = OperadFlow.emplace([this]() {
            for (int32 i = 0; i < 5; ++i)
            {
                ScheduleStage(i);
            }
        }).name("Sigma_Scheduler");
        PhiFold.precede(SigmaScheduler);
    }

    // Execute the graph
    Executor->run(OperadFlow).wait();
#else
    // Fallback: Sequential execution
    ExecuteDyadicDelegation();
    ExecuteTriadicDelegation();
    ComputeLCMAlignment();
    ComputeDoubleStepDelay();
    for (int32 i = 0; i < 5; ++i)
    {
        ScheduleStage(i);
    }
#endif

    double EndTime = FPlatformTime::Seconds();
    float DurationMs = static_cast<float>((EndTime - StartTime) * 1000.0);

    // Update metrics
    {
        FScopeLock Lock(&MetricsLock);
        CurrentMetrics.TasksCompleted += (OperadConfig.bParallelDelta ? 2 : 1) +
                                          1 + 1 + (OperadConfig.bParallelStages ? 5 : 1);
    }
}

void UTaskflowCognitiveScheduler::ExecuteParallelDelta()
{
#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow DeltaFlow("ParallelDelta");

    tf::Task D2 = DeltaFlow.emplace([this]() {
        ExecuteDyadicDelegation();
    }).name("Delta2_Dyadic");

    tf::Task D3 = DeltaFlow.emplace([this]() {
        ExecuteTriadicDelegation();
    }).name("Delta3_Triadic");

    Executor->run(DeltaFlow).wait();
#else
    ExecuteDyadicDelegation();
    ExecuteTriadicDelegation();
#endif
}

void UTaskflowCognitiveScheduler::ExecuteMuSynchronizer()
{
    ComputeLCMAlignment();
    SynchronizeAtBarrier(ESyncBarrierType::OperadSync);
}

void UTaskflowCognitiveScheduler::ExecutePhiFold()
{
    ComputeDoubleStepDelay();
}

void UTaskflowCognitiveScheduler::ExecuteSigmaScheduler()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (OperadConfig.bParallelStages)
    {
        tf::Taskflow SigmaFlow("SigmaScheduler");

        for (int32 i = 0; i < 5; ++i)
        {
            SigmaFlow.emplace([this, i]() {
                ScheduleStage(i);
            }).name("Stage_" + std::to_string(i + 1));
        }

        Executor->run(SigmaFlow).wait();
    }
    else
    {
        for (int32 i = 0; i < 5; ++i)
        {
            ScheduleStage(i);
        }
    }
#else
    for (int32 i = 0; i < 5; ++i)
    {
        ScheduleStage(i);
    }
#endif
}

TArray<float> UTaskflowCognitiveScheduler::ExecuteCubicConcurrency(const TArray<float>& Input)
{
    TArray<float> Output;
    if (!OperadConfig.bEnableCubicConcurrency || Input.Num() == 0)
    {
        return Input;
    }

    Output.SetNum(Input.Num());
    const int32 ThreadCount = FMath::Min(OperadConfig.CubicThreadCount, 8);

#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow C8Flow("CubicConcurrency");

    // 2³ = 8 parallel thread states
    const int32 ChunkSize = (Input.Num() + ThreadCount - 1) / ThreadCount;

    C8Flow.for_each_index(0, ThreadCount, 1, [&](int32 ThreadIdx) {
        int32 StartIdx = ThreadIdx * ChunkSize;
        int32 EndIdx = FMath::Min(StartIdx + ChunkSize, Input.Num());

        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            // C8 processing: Apply cubic transformation
            float Val = Input[i];
            Output[i] = Val * Val * Val; // Simple cubic for now
        }
    });

    Executor->run(C8Flow).wait();
#else
    for (int32 i = 0; i < Input.Num(); ++i)
    {
        Output[i] = Input[i] * Input[i] * Input[i];
    }
#endif

    return Output;
}

TArray<float> UTaskflowCognitiveScheduler::ExecuteTriadicConvolution(const TArray<float>& Input)
{
    TArray<float> Output;
    if (!OperadConfig.bEnableTriadicConvolution || Input.Num() == 0)
    {
        return Input;
    }

    Output.SetNum(Input.Num());
    const int32 PhaseCount = FMath::Min(OperadConfig.ConvolutionPhaseCount, 9);

#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow K9Flow("TriadicConvolution");

    // 3² = 9 orthogonal phase states
    const int32 ChunkSize = (Input.Num() + PhaseCount - 1) / PhaseCount;

    K9Flow.for_each_index(0, PhaseCount, 1, [&](int32 PhaseIdx) {
        int32 StartIdx = PhaseIdx * ChunkSize;
        int32 EndIdx = FMath::Min(StartIdx + ChunkSize, Input.Num());

        // K9 processing: Apply phase rotation
        float PhaseAngle = (PhaseIdx * 40.0f) * PI / 180.0f; // 40° per phase
        float CosPhase = FMath::Cos(PhaseAngle);
        float SinPhase = FMath::Sin(PhaseAngle);

        for (int32 i = StartIdx; i < EndIdx; ++i)
        {
            Output[i] = Input[i] * CosPhase;
        }
    });

    Executor->run(K9Flow).wait();
#else
    for (int32 i = 0; i < Input.Num(); ++i)
    {
        Output[i] = Input[i] * FMath::Cos((i % 9) * 40.0f * PI / 180.0f);
    }
#endif

    return Output;
}

// ========================================
// 4E COGNITION PARALLEL PROCESSING
// ========================================

void UTaskflowCognitiveScheduler::Execute4ECognitionParallel()
{
    if (!bIsInitialized)
    {
        return;
    }

#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow Flow4E("4ECognition");

    // Create 4 parallel tasks for each E dimension
    tf::Task Embodied = Flow4E.emplace([this]() {
        // Embodied: proprioceptive/interoceptive updates
        UE_LOG(LogTemp, Verbose, TEXT("4E: Processing Embodied cognition"));
    }).name("Embodied");

    tf::Task Embedded = Flow4E.emplace([this]() {
        // Embedded: affordance detection and niche coupling
        UE_LOG(LogTemp, Verbose, TEXT("4E: Processing Embedded cognition"));
    }).name("Embedded");

    tf::Task Enacted = Flow4E.emplace([this]() {
        // Enacted: sensorimotor contingencies and active inference
        UE_LOG(LogTemp, Verbose, TEXT("4E: Processing Enacted cognition"));
    }).name("Enacted");

    tf::Task Extended = Flow4E.emplace([this]() {
        // Extended: external memory and tool use
        UE_LOG(LogTemp, Verbose, TEXT("4E: Processing Extended cognition"));
    }).name("Extended");

    // Execute all 4 in parallel
    Executor->run(Flow4E).wait();
#else
    UE_LOG(LogTemp, Verbose, TEXT("4E: Sequential processing (fallback mode)"));
#endif

    // Update metrics
    {
        FScopeLock Lock(&MetricsLock);
        CurrentMetrics.TasksCompleted += 4;
    }
}

// ========================================
// RESERVOIR PARALLEL PROCESSING
// ========================================

void UTaskflowCognitiveScheduler::ExecuteReservoirHierarchyParallel(int32 StreamID, const TArray<float>& Input)
{
    if (!bIsInitialized || !ReservoirComponent)
    {
        return;
    }

#ifndef TASKFLOW_FALLBACK_MODE
    tf::Taskflow ReservoirFlow("Reservoir_Stream_" + std::to_string(StreamID));

    // Create hierarchical tasks: Base -> Mid -> Top
    TArray<float> BaseOutput, MidOutput, TopOutput;

    tf::Task BaseLevel = ReservoirFlow.emplace([&]() {
        // Base level: Input processing
        BaseOutput = Input; // Placeholder
        UE_LOG(LogTemp, Verbose, TEXT("Reservoir: Base level processing for Stream %d"), StreamID);
    }).name("Reservoir_Base");

    tf::Task MidLevel = ReservoirFlow.emplace([&]() {
        // Mid level: Echo propagation
        MidOutput = BaseOutput;
        UE_LOG(LogTemp, Verbose, TEXT("Reservoir: Mid level processing for Stream %d"), StreamID);
    }).name("Reservoir_Mid");

    tf::Task TopLevel = ReservoirFlow.emplace([&]() {
        // Top level: Pattern consolidation
        TopOutput = MidOutput;
        UE_LOG(LogTemp, Verbose, TEXT("Reservoir: Top level processing for Stream %d"), StreamID);
    }).name("Reservoir_Top");

    // Set dependencies: Base -> Mid -> Top
    BaseLevel.precede(MidLevel);
    MidLevel.precede(TopLevel);

    Executor->run(ReservoirFlow).wait();
#else
    UE_LOG(LogTemp, Verbose, TEXT("Reservoir: Sequential processing for Stream %d (fallback mode)"), StreamID);
#endif
}

// ========================================
// TASK MANAGEMENT
// ========================================

FGuid UTaskflowCognitiveScheduler::SubmitTask(const FCognitiveTaskDescriptor& Descriptor)
{
    FCognitiveTaskDescriptor Task = Descriptor;
    Task.Status = ETaskStatus::Scheduled;

    {
        FScopeLock Lock(&TaskMapLock);
        PendingTasks.Add(Task.TaskID, Task);
    }

    {
        FScopeLock Lock(&MetricsLock);
        CurrentMetrics.TasksSubmitted++;
        CurrentMetrics.TasksPending++;
    }

    return Task.TaskID;
}

bool UTaskflowCognitiveScheduler::CancelTask(const FGuid& TaskID)
{
    FScopeLock Lock(&TaskMapLock);

    if (FCognitiveTaskDescriptor* Task = PendingTasks.Find(TaskID))
    {
        if (Task->Status == ETaskStatus::Pending || Task->Status == ETaskStatus::Scheduled)
        {
            Task->Status = ETaskStatus::Cancelled;
            PendingTasks.Remove(TaskID);

            {
                FScopeLock MetricsLockGuard(&MetricsLock);
                CurrentMetrics.TasksPending--;
            }

            return true;
        }
    }

    return false;
}

ETaskStatus UTaskflowCognitiveScheduler::GetTaskStatus(const FGuid& TaskID) const
{
    FScopeLock Lock(&TaskMapLock);

    if (const FCognitiveTaskDescriptor* Task = PendingTasks.Find(TaskID))
    {
        return Task->Status;
    }

    if (const FCognitiveTaskDescriptor* Task = CompletedTasks.Find(TaskID))
    {
        return Task->Status;
    }

    return ETaskStatus::Failed;
}

bool UTaskflowCognitiveScheduler::WaitForTask(const FGuid& TaskID, float TimeoutMs)
{
    double StartTime = FPlatformTime::Seconds();
    double TimeoutSec = TimeoutMs / 1000.0;

    while ((FPlatformTime::Seconds() - StartTime) < TimeoutSec)
    {
        ETaskStatus Status = GetTaskStatus(TaskID);
        if (Status == ETaskStatus::Completed || Status == ETaskStatus::Failed ||
            Status == ETaskStatus::Cancelled)
        {
            return Status == ETaskStatus::Completed;
        }

        FPlatformProcess::Sleep(0.001f); // 1ms sleep
    }

    return false;
}

void UTaskflowCognitiveScheduler::WaitForAll()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (Executor)
    {
        Executor->wait_for_all();
    }
#endif
}

// ========================================
// METRICS AND PROFILING
// ========================================

FTaskGraphMetrics UTaskflowCognitiveScheduler::GetMetrics() const
{
    FScopeLock Lock(&MetricsLock);
    return CurrentMetrics;
}

void UTaskflowCognitiveScheduler::ResetMetrics()
{
    FScopeLock Lock(&MetricsLock);
    CurrentMetrics = FTaskGraphMetrics();
    TaskLatencies.Empty();
}

bool UTaskflowCognitiveScheduler::ExportProfilingData(const FString& FilePath)
{
    if (!bEnableProfiling)
    {
        return false;
    }

    FString ProfilingData;
    ProfilingData += TEXT("TaskflowCognitiveScheduler Profiling Report\n");
    ProfilingData += TEXT("==========================================\n\n");

    {
        FScopeLock Lock(&MetricsLock);
        ProfilingData += FString::Printf(TEXT("Tasks Submitted: %d\n"), CurrentMetrics.TasksSubmitted);
        ProfilingData += FString::Printf(TEXT("Tasks Completed: %d\n"), CurrentMetrics.TasksCompleted);
        ProfilingData += FString::Printf(TEXT("Tasks Running: %d\n"), CurrentMetrics.TasksRunning);
        ProfilingData += FString::Printf(TEXT("Tasks Pending: %d\n"), CurrentMetrics.TasksPending);
        ProfilingData += FString::Printf(TEXT("Average Latency: %.3f ms\n"), CurrentMetrics.AverageLatencyMs);
        ProfilingData += FString::Printf(TEXT("Peak Latency: %.3f ms\n"), CurrentMetrics.PeakLatencyMs);
        ProfilingData += FString::Printf(TEXT("Worker Utilization: %.2f%%\n"), CurrentMetrics.WorkerUtilization * 100.0f);
        ProfilingData += FString::Printf(TEXT("Active Workers: %d\n"), CurrentMetrics.ActiveWorkers);
        ProfilingData += FString::Printf(TEXT("Sync Barriers: %d\n"), CurrentMetrics.SyncBarriersHit);
        ProfilingData += FString::Printf(TEXT("Parallel Speedup: %.2fx\n"), CurrentMetrics.ParallelSpeedup);
    }

    return FFileHelper::SaveStringToFile(ProfilingData, *FilePath);
}

TArray<float> UTaskflowCognitiveScheduler::GetWorkerUtilization() const
{
    TArray<float> Utilization;
    Utilization.SetNum(WorkerThreadCount);

#ifndef TASKFLOW_FALLBACK_MODE
    if (Executor)
    {
        // Note: Taskflow doesn't expose per-worker utilization directly
        // This is a placeholder that would need custom instrumentation
        for (int32 i = 0; i < WorkerThreadCount; ++i)
        {
            Utilization[i] = CurrentMetrics.WorkerUtilization;
        }
    }
#else
    for (int32 i = 0; i < WorkerThreadCount; ++i)
    {
        Utilization[i] = 0.0f;
    }
#endif

    return Utilization;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UTaskflowCognitiveScheduler::BuildCognitiveStreamGraph()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (!CognitiveGraph)
    {
        return;
    }

    CognitiveGraph->clear();

    // Pre-build template for cognitive stream processing
    // Actual execution uses dynamic graphs with runtime data
    UE_LOG(LogTemp, Verbose, TEXT("Built cognitive stream task graph template"));
#endif
}

void UTaskflowCognitiveScheduler::BuildOperadGraph()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (!OperadGraph)
    {
        return;
    }

    OperadGraph->clear();

    // Pre-build template for Sys6 operad execution
    UE_LOG(LogTemp, Verbose, TEXT("Built Sys6 operad task graph template"));
#endif
}

void UTaskflowCognitiveScheduler::BuildReservoirGraph()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (!ReservoirGraph)
    {
        return;
    }

    ReservoirGraph->clear();

    // Pre-build template for reservoir hierarchy
    UE_LOG(LogTemp, Verbose, TEXT("Built reservoir hierarchy task graph template"));
#endif
}

void UTaskflowCognitiveScheduler::Build4ECognitionGraph()
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (!Cognition4EGraph)
    {
        return;
    }

    Cognition4EGraph->clear();

    // Pre-build template for 4E cognition updates
    UE_LOG(LogTemp, Verbose, TEXT("Built 4E cognition task graph template"));
#endif
}

void UTaskflowCognitiveScheduler::UpdateMetrics(float DeltaTime)
{
    FScopeLock Lock(&MetricsLock);

    // Calculate average latency
    if (TaskLatencies.Num() > 0)
    {
        float Sum = 0.0f;
        float Peak = 0.0f;
        for (float Latency : TaskLatencies)
        {
            Sum += Latency;
            Peak = FMath::Max(Peak, Latency);
        }
        CurrentMetrics.AverageLatencyMs = Sum / TaskLatencies.Num();
        CurrentMetrics.PeakLatencyMs = Peak;

        // Keep only recent latencies
        if (TaskLatencies.Num() > 1000)
        {
            TaskLatencies.RemoveAt(0, TaskLatencies.Num() - 1000);
        }
    }

    // Update worker stats
    CurrentMetrics.ActiveWorkers = WorkerThreadCount;

#ifndef TASKFLOW_FALLBACK_MODE
    if (Executor)
    {
        // Estimate utilization based on pending work
        CurrentMetrics.WorkerUtilization = FMath::Clamp(
            static_cast<float>(CurrentMetrics.TasksRunning) / WorkerThreadCount,
            0.0f, 1.0f
        );
    }
#endif

    // Calculate parallel speedup estimate
    if (CurrentMetrics.TasksCompleted > 0 && CurrentMetrics.AverageLatencyMs > 0.0f)
    {
        // Simple estimate: speedup = workers * utilization
        CurrentMetrics.ParallelSpeedup = 1.0f +
            (WorkerThreadCount - 1) * CurrentMetrics.WorkerUtilization * 0.7f; // 70% efficiency factor
    }

    // Broadcast metrics update
    OnMetricsUpdated.Broadcast(CurrentMetrics);
}

void UTaskflowCognitiveScheduler::ProcessCompletedTasks()
{
    TArray<FGuid> CompletedIDs;

    {
        FScopeLock Lock(&TaskMapLock);

        for (auto& Pair : PendingTasks)
        {
            if (Pair.Value.Status == ETaskStatus::Completed)
            {
                CompletedIDs.Add(Pair.Key);
                CompletedTasks.Add(Pair.Key, Pair.Value);
            }
        }

        for (const FGuid& ID : CompletedIDs)
        {
            PendingTasks.Remove(ID);
        }
    }

    // Fire completion events
    for (const FGuid& ID : CompletedIDs)
    {
        if (const FCognitiveTaskDescriptor* Task = CompletedTasks.Find(ID))
        {
            OnTaskCompleted.Broadcast(ID, Task->ActualDurationMs);
        }
    }
}

void UTaskflowCognitiveScheduler::ApplyBackpressure()
{
    UE_LOG(LogTemp, Warning, TEXT("TaskflowCognitiveScheduler: Applying backpressure (%d pending tasks)"),
           PendingTasks.Num());

    // Wait for some tasks to complete before accepting more
#ifndef TASKFLOW_FALLBACK_MODE
    if (Executor)
    {
        Executor->wait_for_all();
    }
#endif
}

TArray<float> UTaskflowCognitiveScheduler::ProcessStreamInput(int32 StreamID, const TArray<float>& Input)
{
    TArray<float> Output;
    Output.SetNum(Input.Num());

    // Apply stream-specific processing
    for (int32 i = 0; i < Input.Num(); ++i)
    {
        // Default processing: apply tanh activation
        Output[i] = FMath::Tanh(Input[i]);
    }

    return Output;
}

void UTaskflowCognitiveScheduler::SynchronizeAtBarrier(ESyncBarrierType BarrierType)
{
#ifndef TASKFLOW_FALLBACK_MODE
    if (Executor)
    {
        Executor->wait_for_all();
    }
#endif

    // Log sync event
    UE_LOG(LogTemp, Verbose, TEXT("TaskflowCognitiveScheduler: Sync barrier reached - %s"),
           *UEnum::GetValueAsString(BarrierType));
}

void UTaskflowCognitiveScheduler::ExecuteDyadicDelegation()
{
    // Δ₂: D → (D, C8)
    // Move dyadic powers into parallel concurrency (2³ = 8 states)

    if (OperadEngine)
    {
        OperadEngine->ApplyDeltaDyadic();
    }

    UE_LOG(LogTemp, Verbose, TEXT("Operad: Dyadic delegation (Δ₂) executed"));
}

void UTaskflowCognitiveScheduler::ExecuteTriadicDelegation()
{
    // Δ₃: T → (T, K9)
    // Move triadic powers into orthogonal convolution phases (3² = 9 states)

    if (OperadEngine)
    {
        OperadEngine->ApplyDeltaTriadic();
    }

    UE_LOG(LogTemp, Verbose, TEXT("Operad: Triadic delegation (Δ₃) executed"));
}

void UTaskflowCognitiveScheduler::ComputeLCMAlignment()
{
    // μ: (D, T, P) → Clock30
    // Synchronize all channels to LCM(2,3,5) = 30 clock domain

    if (OperadEngine)
    {
        OperadEngine->ApplyMuSynchronizer();
    }

    UE_LOG(LogTemp, Verbose, TEXT("Operad: μ synchronizer (LCM alignment) executed"));
}

void UTaskflowCognitiveScheduler::ComputeDoubleStepDelay()
{
    // φ: 2×3→4
    // Compress 6-step dyad×triad multiplex into 4 real steps

    if (OperadEngine)
    {
        OperadEngine->ApplyPhiFold();
    }

    UE_LOG(LogTemp, Verbose, TEXT("Operad: φ fold (double-step delay) executed"));
}

void UTaskflowCognitiveScheduler::ScheduleStage(int32 StageIndex)
{
    // σ: 5×6 staging
    // Execute one of 5 stages with 6 steps each

    UE_LOG(LogTemp, Verbose, TEXT("Operad: Stage %d scheduled"), StageIndex + 1);
}
