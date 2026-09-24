// TaskflowCognitiveScheduler.h
// High-performance parallel task scheduling for Deep Tree Echo cognitive architecture
// Integrates Taskflow for DAG-based parallel execution of cognitive streams

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/AsyncWork.h"

// Taskflow header-only library
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include "taskflow/taskflow.hpp"
#include "taskflow/algorithm/for_each.hpp"
#include "taskflow/algorithm/reduce.hpp"
#include "taskflow/algorithm/transform.hpp"
#else
// Fallback for C++17 compatibility - use basic task structures
#define TASKFLOW_FALLBACK_MODE 1
#endif

#include "TaskflowCognitiveScheduler.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class USys6OperadEngine;
class UDeepTreeEchoReservoir;

/**
 * Task execution priority levels
 */
UENUM(BlueprintType)
enum class ETaskPriority : uint8
{
    Critical    UMETA(DisplayName = "Critical (Real-time)"),
    High        UMETA(DisplayName = "High (Perception)"),
    Normal      UMETA(DisplayName = "Normal (Processing)"),
    Low         UMETA(DisplayName = "Low (Background)"),
    Deferred    UMETA(DisplayName = "Deferred (Non-urgent)")
};

/**
 * Task domain classification
 */
UENUM(BlueprintType)
enum class ETaskDomain : uint8
{
    CognitiveStream     UMETA(DisplayName = "Cognitive Stream Processing"),
    ReservoirCompute    UMETA(DisplayName = "Reservoir Computing"),
    SensoryIntegration  UMETA(DisplayName = "Sensory Integration"),
    OperadExecution     UMETA(DisplayName = "Sys6 Operad Execution"),
    MemoryOperation     UMETA(DisplayName = "Memory Operation"),
    InferenceUpdate     UMETA(DisplayName = "Active Inference Update"),
    EmbodiedCognition   UMETA(DisplayName = "4E Cognition Update")
};

/**
 * Task execution status
 */
UENUM(BlueprintType)
enum class ETaskStatus : uint8
{
    Pending     UMETA(DisplayName = "Pending"),
    Scheduled   UMETA(DisplayName = "Scheduled"),
    Running     UMETA(DisplayName = "Running"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed"),
    Cancelled   UMETA(DisplayName = "Cancelled")
};

/**
 * Synchronization barrier type for stream coordination
 */
UENUM(BlueprintType)
enum class ESyncBarrierType : uint8
{
    None            UMETA(DisplayName = "No Barrier"),
    StreamSync      UMETA(DisplayName = "Stream Synchronization"),
    TriadicSync     UMETA(DisplayName = "Triadic Synchronization"),
    StageSync       UMETA(DisplayName = "Stage Boundary Sync"),
    CycleSync       UMETA(DisplayName = "Full Cycle Sync"),
    OperadSync      UMETA(DisplayName = "Operad Operation Sync")
};

/**
 * Cognitive task descriptor for scheduling
 */
USTRUCT(BlueprintType)
struct FCognitiveTaskDescriptor
{
    GENERATED_BODY()

    /** Unique task identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid TaskID;

    /** Human-readable task name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TaskName;

    /** Task execution domain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETaskDomain Domain = ETaskDomain::CognitiveStream;

    /** Task priority */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETaskPriority Priority = ETaskPriority::Normal;

    /** Current status */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETaskStatus Status = ETaskStatus::Pending;

    /** Associated stream ID (1-3 for cognitive streams) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamID = 0;

    /** Cognitive cycle step (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CycleStep = 1;

    /** Sys6 step (1-30) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Sys6Step = 1;

    /** Task dependencies (GUIDs of prerequisite tasks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> Dependencies;

    /** Required synchronization barrier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESyncBarrierType SyncBarrier = ESyncBarrierType::None;

    /** Estimated execution time in milliseconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedDurationMs = 1.0f;

    /** Actual execution time in milliseconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActualDurationMs = 0.0f;

    FCognitiveTaskDescriptor()
    {
        TaskID = FGuid::NewGuid();
    }
};

/**
 * Task graph metrics for performance monitoring
 */
USTRUCT(BlueprintType)
struct FTaskGraphMetrics
{
    GENERATED_BODY()

    /** Total tasks submitted this frame */
    UPROPERTY(BlueprintReadOnly)
    int32 TasksSubmitted = 0;

    /** Tasks completed this frame */
    UPROPERTY(BlueprintReadOnly)
    int32 TasksCompleted = 0;

    /** Tasks currently executing */
    UPROPERTY(BlueprintReadOnly)
    int32 TasksRunning = 0;

    /** Tasks waiting in queue */
    UPROPERTY(BlueprintReadOnly)
    int32 TasksPending = 0;

    /** Average task latency in ms */
    UPROPERTY(BlueprintReadOnly)
    float AverageLatencyMs = 0.0f;

    /** Peak task latency in ms */
    UPROPERTY(BlueprintReadOnly)
    float PeakLatencyMs = 0.0f;

    /** Worker thread utilization (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float WorkerUtilization = 0.0f;

    /** Number of active worker threads */
    UPROPERTY(BlueprintReadOnly)
    int32 ActiveWorkers = 0;

    /** Number of sync barriers hit this frame */
    UPROPERTY(BlueprintReadOnly)
    int32 SyncBarriersHit = 0;

    /** Parallel speedup factor */
    UPROPERTY(BlueprintReadOnly)
    float ParallelSpeedup = 1.0f;
};

/**
 * Stream processing configuration
 */
USTRUCT(BlueprintType)
struct FStreamProcessingConfig
{
    GENERATED_BODY()

    /** Stream identifier (1-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamID = 1;

    /** Enable parallel processing for this stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableParallel = true;

    /** Maximum concurrent tasks per stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxConcurrentTasks = 4;

    /** Task batch size for parallel_for operations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BatchSize = 32;

    /** Enable work-stealing between streams */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableWorkStealing = true;
};

/**
 * Operad execution graph configuration
 */
USTRUCT(BlueprintType)
struct FOperadGraphConfig
{
    GENERATED_BODY()

    /** Enable parallel delta operations (Δ₂ ⊗ Δ₃) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bParallelDelta = true;

    /** Enable parallel stage execution (σ stages) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bParallelStages = true;

    /** Enable cubic concurrency (C8 parallelism) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableCubicConcurrency = true;

    /** Number of threads for C8 (max 8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "8"))
    int32 CubicThreadCount = 8;

    /** Enable triadic convolution parallelism (K9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableTriadicConvolution = true;

    /** Number of phases for K9 (max 9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "9"))
    int32 ConvolutionPhaseCount = 9;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaskCompleted, FGuid, TaskID, float, DurationMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSyncBarrierReached, ESyncBarrierType, BarrierType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGraphExecutionComplete, int32, TaskCount, float, TotalDurationMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskGraphMetricsUpdated, FTaskGraphMetrics, Metrics);

/**
 * TaskflowCognitiveScheduler
 *
 * High-performance DAG-based parallel task scheduler for cognitive processing.
 * Integrates Taskflow library for efficient work-stealing and task graph execution.
 *
 * Key features:
 * - Parallel processing of 3 cognitive streams
 * - Sys6 operad DAG execution with proper synchronization
 * - Triadic sync barriers at {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 * - Work-stealing across stream boundaries
 * - Profiling integration for performance monitoring
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTaskflowCognitiveScheduler : public UActorComponent
{
    GENERATED_BODY()

public:
    UTaskflowCognitiveScheduler();
    virtual ~UTaskflowCognitiveScheduler();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Number of worker threads for task execution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    int32 WorkerThreadCount = 4;

    /** Enable automatic task graph construction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    bool bAutoConstructGraphs = true;

    /** Enable profiling and metrics collection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    bool bEnableProfiling = true;

    /** Maximum pending tasks before backpressure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    int32 MaxPendingTasks = 256;

    /** Stream processing configurations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    TArray<FStreamProcessingConfig> StreamConfigs;

    /** Operad graph configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taskflow Configuration")
    FOperadGraphConfig OperadConfig;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Taskflow Events")
    FOnTaskCompleted OnTaskCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Taskflow Events")
    FOnSyncBarrierReached OnSyncBarrierReached;

    UPROPERTY(BlueprintAssignable, Category = "Taskflow Events")
    FOnGraphExecutionComplete OnGraphExecutionComplete;

    UPROPERTY(BlueprintAssignable, Category = "Taskflow Events")
    FOnTaskGraphMetricsUpdated OnMetricsUpdated;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize the Taskflow executor and thread pool */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Control")
    bool Initialize();

    /** Shutdown and cleanup resources */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Control")
    void Shutdown();

    /** Check if scheduler is initialized and ready */
    UFUNCTION(BlueprintPure, Category = "Taskflow Control")
    bool IsInitialized() const;

    // ========================================
    // COGNITIVE STREAM SCHEDULING
    // ========================================

    /**
     * Build and execute the cognitive cycle task graph
     * Creates parallel tasks for 3 streams with triadic sync barriers
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Cognitive")
    void ExecuteCognitiveStreamGraph(int32 CycleStep, const TArray<TArray<float>>& StreamInputs);

    /**
     * Process a single cognitive stream in parallel
     * @param StreamID Stream identifier (1-3)
     * @param Input Input data for processing
     * @return Processed output
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Cognitive")
    TArray<float> ProcessStreamParallel(int32 StreamID, const TArray<float>& Input);

    /**
     * Execute triadic synchronization barrier
     * Waits for all three streams to reach sync point
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Cognitive")
    void ExecuteTriadicSync(int32 TriadIndex);

    /**
     * Schedule stream coherence computation
     * Parallel computation of 3x3 coherence matrix
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Cognitive")
    TArray<float> ComputeStreamCoherenceParallel();

    // ========================================
    // SYS6 OPERAD SCHEDULING
    // ========================================

    /**
     * Build and execute the Sys6 operad task DAG
     * Sys6 := σ ∘ (φ ∘ μ ∘ (Δ₂ ⊗ Δ₃ ⊗ id_P))
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    void ExecuteOperadGraph(int32 Sys6Step);

    /**
     * Execute parallel delta operations (Δ₂ ⊗ Δ₃)
     * Runs dyadic and triadic delegation concurrently
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    void ExecuteParallelDelta();

    /**
     * Execute the LCM synchronizer (μ)
     * Synchronization barrier for D, T, P alignment to Clock30
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    void ExecuteMuSynchronizer();

    /**
     * Execute the phi fold (φ: 2×3→4)
     * Double-step delay fold computation
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    void ExecutePhiFold();

    /**
     * Execute stage scheduler (σ: 5×6)
     * Parallel execution of 5 stages with 6 steps each
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    void ExecuteSigmaScheduler();

    /**
     * Execute cubic concurrency (C8: 2³ parallel threads)
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    TArray<float> ExecuteCubicConcurrency(const TArray<float>& Input);

    /**
     * Execute triadic convolution (K9: 3² orthogonal phases)
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Operad")
    TArray<float> ExecuteTriadicConvolution(const TArray<float>& Input);

    // ========================================
    // 4E COGNITION PARALLEL PROCESSING
    // ========================================

    /**
     * Execute parallel 4E cognition updates
     * Runs Embodied, Embedded, Enacted, Extended updates concurrently
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow 4E")
    void Execute4ECognitionParallel();

    // ========================================
    // RESERVOIR PARALLEL PROCESSING
    // ========================================

    /**
     * Execute parallel reservoir hierarchy updates
     * Processes 3 hierarchical levels concurrently per stream
     */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Reservoir")
    void ExecuteReservoirHierarchyParallel(int32 StreamID, const TArray<float>& Input);

    // ========================================
    // TASK MANAGEMENT
    // ========================================

    /** Submit a task for execution */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Tasks")
    FGuid SubmitTask(const FCognitiveTaskDescriptor& Descriptor);

    /** Cancel a pending task */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Tasks")
    bool CancelTask(const FGuid& TaskID);

    /** Get task status */
    UFUNCTION(BlueprintPure, Category = "Taskflow Tasks")
    ETaskStatus GetTaskStatus(const FGuid& TaskID) const;

    /** Wait for task completion (blocking) */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Tasks")
    bool WaitForTask(const FGuid& TaskID, float TimeoutMs = 1000.0f);

    /** Wait for all pending tasks to complete */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Tasks")
    void WaitForAll();

    // ========================================
    // METRICS AND PROFILING
    // ========================================

    /** Get current task graph metrics */
    UFUNCTION(BlueprintPure, Category = "Taskflow Metrics")
    FTaskGraphMetrics GetMetrics() const;

    /** Reset metrics counters */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Metrics")
    void ResetMetrics();

    /** Export profiling data to file */
    UFUNCTION(BlueprintCallable, Category = "Taskflow Metrics")
    bool ExportProfilingData(const FString& FilePath);

    /** Get worker thread utilization per thread */
    UFUNCTION(BlueprintPure, Category = "Taskflow Metrics")
    TArray<float> GetWorkerUtilization() const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    USys6OperadEngine* OperadEngine;

    UPROPERTY()
    UDeepTreeEchoReservoir* ReservoirComponent;

    // Taskflow executor and taskflows
#ifndef TASKFLOW_FALLBACK_MODE
    TUniquePtr<tf::Executor> Executor;
    TUniquePtr<tf::Taskflow> CognitiveGraph;
    TUniquePtr<tf::Taskflow> OperadGraph;
    TUniquePtr<tf::Taskflow> ReservoirGraph;
    TUniquePtr<tf::Taskflow> Cognition4EGraph;
#endif

    // Task tracking
    TMap<FGuid, FCognitiveTaskDescriptor> PendingTasks;
    TMap<FGuid, FCognitiveTaskDescriptor> CompletedTasks;
    FCriticalSection TaskMapLock;

    // Metrics
    FTaskGraphMetrics CurrentMetrics;
    TArray<float> TaskLatencies;
    FCriticalSection MetricsLock;

    // State
    FThreadSafeBool bIsInitialized;
    FThreadSafeBool bIsShuttingDown;

    // Internal methods
    void FindComponentReferences();
    void InitializeStreamConfigs();
    void BuildCognitiveStreamGraph();
    void BuildOperadGraph();
    void BuildReservoirGraph();
    void Build4ECognitionGraph();

    void UpdateMetrics(float DeltaTime);
    void ProcessCompletedTasks();
    void ApplyBackpressure();

    // Stream processing helpers
    TArray<float> ProcessStreamInput(int32 StreamID, const TArray<float>& Input);
    void SynchronizeAtBarrier(ESyncBarrierType BarrierType);

    // Operad execution helpers
    void ExecuteDyadicDelegation();
    void ExecuteTriadicDelegation();
    void ComputeLCMAlignment();
    void ComputeDoubleStepDelay();
    void ScheduleStage(int32 StageIndex);
};
