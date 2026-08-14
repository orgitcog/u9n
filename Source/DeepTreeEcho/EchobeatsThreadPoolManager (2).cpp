/**
 * EchobeatsThreadPoolManager.cpp
 * 
 * Deep Tree Echo - Echobeats Thread Pool Manager Implementation
 * 
 * Implements the PyTorch-aligned thread pool architecture for
 * the 3-stream cognitive model with 12-step interleaving.
 * 
 * Copyright (c) 2025 Deep Tree Echo Project
 */

#include "EchobeatsThreadPoolManager.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#ifdef HAVE_NUMA
#include <numa.h>
#endif
#endif

// NUMA fallback for systems without libnuma
#ifndef HAVE_NUMA
struct bitmask { unsigned long size; unsigned long *maskp; };
namespace {
    inline int numa_available() { return -1; }
    inline int numa_num_configured_nodes() { return 1; }
    inline int numa_node_of_cpu(int /*cpu*/) { return 0; }
    inline void numa_set_preferred(int /*node*/) {}
    inline void numa_run_on_node(int /*node*/) {}
    inline int numa_num_configured_cpus() { return 1; }
    inline bitmask* numa_allocate_cpumask() { return nullptr; }
    inline void numa_node_to_cpus(int /*node*/, bitmask* /*mask*/) {}
    inline int numa_bitmask_isbitset(bitmask* /*mask*/, int /*cpu*/) { return 0; }
    inline void numa_free_cpumask(bitmask* /*mask*/) {}
}
#endif

namespace DeepTreeEcho {

//=============================================================================
// Static Member Definitions
//=============================================================================

constexpr std::array<ECognitiveStream, FEchobeatsThreadPoolManager::STEPS_PER_CYCLE> 
    FEchobeatsThreadPoolManager::StepStreamMap;

constexpr std::array<ECognitivePhase, FEchobeatsThreadPoolManager::STEPS_PER_CYCLE> 
    FEchobeatsThreadPoolManager::StepPhaseMap;

constexpr std::array<EProcessingMode, FEchobeatsThreadPoolManager::STEPS_PER_CYCLE> 
    FEchobeatsThreadPoolManager::StepModeMap;

//=============================================================================
// Tetrahedral Thread Bundle Implementation
//=============================================================================

/**
 * Tetrahedral face definitions
 * 
 * The tetrahedron has 4 vertices (threads) and 4 triangular faces:
 *   Face 0: Threads {0, 1, 2} - Perception-Action-Simulation
 *   Face 1: Threads {0, 1, 3} - Perception-Action-Integration
 *   Face 2: Threads {0, 2, 3} - Perception-Simulation-Integration
 *   Face 3: Threads {1, 2, 3} - Action-Simulation-Integration
 */
static constexpr std::array<std::array<int32_t, 3>, 4> TetrahedralFaces = {{
    {0, 1, 2},  // Face 0: Active processing triad
    {0, 1, 3},  // Face 1: Motor loop triad
    {0, 2, 3},  // Face 2: Prediction loop triad
    {1, 2, 3}   // Face 3: Learning loop triad
}};

/**
 * Dyadic edge definitions
 * 
 * The tetrahedron has 6 edges connecting pairs of vertices:
 *   Edge 0: {0, 1} - Perception-Action (sensory-motor)
 *   Edge 1: {0, 2} - Perception-Simulation (predictive coding)
 *   Edge 2: {0, 3} - Perception-Integration (state update)
 *   Edge 3: {1, 2} - Action-Simulation (forward model)
 *   Edge 4: {1, 3} - Action-Integration (behavior learning)
 *   Edge 5: {2, 3} - Simulation-Integration (model update)
 */
static constexpr std::array<std::array<int32_t, 2>, 6> TetrahedralEdges = {{
    {0, 1},  // Edge 0: Sensory-motor
    {0, 2},  // Edge 1: Predictive coding
    {0, 3},  // Edge 2: State update
    {1, 2},  // Edge 3: Forward model
    {1, 3},  // Edge 4: Behavior learning
    {2, 3}   // Edge 5: Model update
}};

std::array<int32_t, 3> FEchobeatsThreadPoolManager::GetTriadicFace(int32_t FaceIndex) {
    if (FaceIndex < 0 || FaceIndex >= 4) {
        return {0, 0, 0};
    }
    return TetrahedralFaces[FaceIndex];
}

int32_t FEchobeatsThreadPoolManager::GetDyadicEdge(int32_t Thread1, int32_t Thread2) {
    // Ensure Thread1 < Thread2 for consistent lookup
    if (Thread1 > Thread2) {
        std::swap(Thread1, Thread2);
    }
    
    for (int32_t i = 0; i < 6; ++i) {
        if (TetrahedralEdges[i][0] == Thread1 && TetrahedralEdges[i][1] == Thread2) {
            return i;
        }
    }
    
    return -1;  // Invalid edge
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

FEchobeatsThreadPoolManager::FEchobeatsThreadPoolManager()
    : FEchobeatsThreadPoolManager(FThreadPoolConfig{})
{
}

FEchobeatsThreadPoolManager::FEchobeatsThreadPoolManager(const FThreadPoolConfig& InConfig)
    : Config(InConfig)
{
    InitializeWorkers();
    
    if (Config.EnableNUMAAffinity) {
        ConfigureNUMAAffinity();
    }
    
    ConfigurePyTorchThreads();
    
    CycleStartTime = std::chrono::steady_clock::now();
}

FEchobeatsThreadPoolManager::~FEchobeatsThreadPoolManager() {
    ShutdownWorkers();
}

//=============================================================================
// Worker Thread Management
//=============================================================================

void FEchobeatsThreadPoolManager::InitializeWorkers() {
    for (int32_t streamIdx = 0; streamIdx < NUM_STREAMS; ++streamIdx) {
        ECognitiveStream stream = static_cast<ECognitiveStream>(streamIdx);
        
        for (int32_t threadIdx = 0; threadIdx < Config.ThreadsPerStream; ++threadIdx) {
            WorkerThreads[streamIdx].emplace_back(
                &FEchobeatsThreadPoolManager::WorkerLoop, 
                this, 
                stream, 
                threadIdx
            );
        }
    }
}

void FEchobeatsThreadPoolManager::ShutdownWorkers() {
    Running.store(false);
    
    // Wake up all waiting workers
    for (int32_t i = 0; i < NUM_STREAMS; ++i) {
        QueueConditions[i].notify_all();
    }
    
    // Join all worker threads
    for (int32_t i = 0; i < NUM_STREAMS; ++i) {
        for (auto& thread : WorkerThreads[i]) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        WorkerThreads[i].clear();
    }
}

void FEchobeatsThreadPoolManager::WorkerLoop(ECognitiveStream Stream, int32_t ThreadIndex) {
    const int32_t streamIdx = static_cast<int32_t>(Stream);
    
    while (Running.load()) {
        FCognitiveTask task;
        
        // Wait for a task
        {
            std::unique_lock<std::mutex> lock(QueueMutexes[streamIdx]);
            
            QueueConditions[streamIdx].wait(lock, [this, streamIdx]() {
                return !Running.load() || !StreamQueues[streamIdx].empty();
            });
            
            if (!Running.load() && StreamQueues[streamIdx].empty()) {
                break;
            }
            
            if (StreamQueues[streamIdx].empty()) {
                continue;
            }
            
            // Get highest priority task
            task = StreamQueues[streamIdx].top();
            StreamQueues[streamIdx].pop();
            
            // Update queue metrics
            Metrics.StreamMetrics[streamIdx].CurrentQueueDepth.fetch_sub(1);
        }
        
        // Check dependencies
        if (!AreDependenciesSatisfied(task)) {
            // Re-queue the task with slightly lower priority
            task.Priority -= 0.01f;
            SubmitTask(std::move(task));
            continue;
        }
        
        // Process the task
        FTaskResult result = ProcessTask(task);
        
        // Update metrics
        auto& streamMetrics = Metrics.StreamMetrics[streamIdx];
        streamMetrics.TasksCompleted.fetch_add(1);
        streamMetrics.TotalExecutionTime.fetch_add(result.ExecutionTime.count());
        
        uint64_t execTime = result.ExecutionTime.count();
        uint64_t currentMax = streamMetrics.MaxExecutionTime.load();
        while (execTime > currentMax && 
               !streamMetrics.MaxExecutionTime.compare_exchange_weak(currentMax, execTime)) {
        }
        
        uint64_t currentMin = streamMetrics.MinExecutionTime.load();
        while (execTime < currentMin && 
               !streamMetrics.MinExecutionTime.compare_exchange_weak(currentMin, execTime)) {
        }
        
        if (!result.Success) {
            streamMetrics.TasksFailed.fetch_add(1);
        }
        
        // Fulfill promise if tracked
        {
            std::lock_guard<std::mutex> lock(TaskPromisesMutex);
            auto it = TaskPromises.find(task.TaskId);
            if (it != TaskPromises.end()) {
                it->second.set_value(result);
                TaskPromises.erase(it);
            }
        }
    }
}

FTaskResult FEchobeatsThreadPoolManager::ProcessTask(FCognitiveTask& Task) {
    FTaskResult result;
    result.TaskId = Task.TaskId;
    result.Stream = Task.Stream;
    result.Step = Task.Step;
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        if (Task.Work) {
            Task.Work();
        }
        result.Success = true;
    } catch (const std::exception& e) {
        result.Success = false;
        std::cerr << "Task " << Task.TaskId << " failed: " << e.what() << std::endl;
    } catch (...) {
        result.Success = false;
        std::cerr << "Task " << Task.TaskId << " failed with unknown exception" << std::endl;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.ExecutionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    return result;
}

bool FEchobeatsThreadPoolManager::AreDependenciesSatisfied(const FCognitiveTask& Task) {
    if (Task.Dependencies.empty()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(TaskPromisesMutex);
    
    for (uint64_t depId : Task.Dependencies) {
        // If the promise still exists, the task hasn't completed
        if (TaskPromises.find(depId) != TaskPromises.end()) {
            return false;
        }
    }
    
    return true;
}

//=============================================================================
// Task Submission
//=============================================================================

uint64_t FEchobeatsThreadPoolManager::SubmitTask(FCognitiveTask Task) {
    // Assign task ID if not set
    if (Task.TaskId == 0) {
        Task.TaskId = NextTaskId.fetch_add(1);
    }
    
    const int32_t streamIdx = static_cast<int32_t>(Task.Stream);
    
    // Create promise for tracking if needed
    {
        std::lock_guard<std::mutex> lock(TaskPromisesMutex);
        TaskPromises[Task.TaskId] = std::promise<FTaskResult>();
    }
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(QueueMutexes[streamIdx]);
        StreamQueues[streamIdx].push(std::move(Task));
        
        // Update metrics
        auto& streamMetrics = Metrics.StreamMetrics[streamIdx];
        streamMetrics.TasksSubmitted.fetch_add(1);
        uint32_t depth = streamMetrics.CurrentQueueDepth.fetch_add(1) + 1;
        
        uint32_t maxDepth = streamMetrics.MaxQueueDepth.load();
        while (depth > maxDepth && 
               !streamMetrics.MaxQueueDepth.compare_exchange_weak(maxDepth, depth)) {
        }
    }
    
    // Notify a worker
    QueueConditions[streamIdx].notify_one();
    
    return Task.TaskId;
}

std::vector<uint64_t> FEchobeatsThreadPoolManager::SubmitTasks(std::vector<FCognitiveTask> Tasks) {
    std::vector<uint64_t> taskIds;
    taskIds.reserve(Tasks.size());
    
    for (auto& task : Tasks) {
        taskIds.push_back(SubmitTask(std::move(task)));
    }
    
    return taskIds;
}

uint64_t FEchobeatsThreadPoolManager::SubmitForStep(
    std::function<void()> Work, 
    int32_t Step, 
    float Priority
) {
    FCognitiveTask task;
    task.Work = std::move(Work);
    task.Step = Step;
    task.Stream = GetStreamForStep(Step);
    task.Phase = GetPhaseForStep(Step);
    task.Priority = Priority;
    
    return SubmitTask(std::move(task));
}

bool FEchobeatsThreadPoolManager::WaitForTask(
    uint64_t TaskId, 
    std::chrono::milliseconds Timeout
) {
    std::future<FTaskResult> future;
    
    {
        std::lock_guard<std::mutex> lock(TaskPromisesMutex);
        auto it = TaskPromises.find(TaskId);
        if (it == TaskPromises.end()) {
            // Task already completed or doesn't exist
            return true;
        }
        future = it->second.get_future();
    }
    
    auto status = future.wait_for(Timeout);
    return status == std::future_status::ready;
}

bool FEchobeatsThreadPoolManager::WaitForStream(
    ECognitiveStream Stream, 
    std::chrono::milliseconds Timeout
) {
    const int32_t streamIdx = static_cast<int32_t>(Stream);
    auto deadline = std::chrono::steady_clock::now() + Timeout;
    
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(QueueMutexes[streamIdx]);
            if (StreamQueues[streamIdx].empty()) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return false;
}

//=============================================================================
// Cognitive Loop Control
//=============================================================================

void FEchobeatsThreadPoolManager::AdvanceStep() {
    int32_t currentStep = CurrentStep.load();
    int32_t nextStep = (currentStep % STEPS_PER_CYCLE) + 1;
    
    // Check if we're at an integration point (steps 4, 8, 12)
    if (currentStep % 4 == 0) {
        SynchronizeStreams();
    }
    
    CurrentStep.store(nextStep);
    
    // If we completed a cycle, update metrics
    if (nextStep == 1) {
        CurrentCycle.fetch_add(1);
        
        auto now = std::chrono::steady_clock::now();
        auto cycleTime = std::chrono::duration_cast<std::chrono::microseconds>(
            now - CycleStartTime
        );
        
        Metrics.CyclesCompleted.fetch_add(1);
        Metrics.TotalCycleTime.fetch_add(cycleTime.count());
        
        CycleStartTime = now;
    }
}

uint64_t FEchobeatsThreadPoolManager::RunCycle() {
    auto startTime = std::chrono::steady_clock::now();
    
    // Ensure we start at step 1
    CurrentStep.store(1);
    CycleStartTime = startTime;
    
    // Run all 12 steps
    for (int32_t step = 1; step <= STEPS_PER_CYCLE; ++step) {
        // Wait for current stream to complete its tasks
        ECognitiveStream stream = GetStreamForStep(step);
        WaitForStream(stream, std::chrono::milliseconds(
            static_cast<int64_t>(Config.TargetCycleTimeMs / STEPS_PER_CYCLE * 2)
        ));
        
        AdvanceStep();
    }
    
    auto endTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
}

//=============================================================================
// Synchronization
//=============================================================================

void FEchobeatsThreadPoolManager::SynchronizeStreams() {
    auto startTime = std::chrono::steady_clock::now();
    
    // Wait for all streams to complete their current tasks
    for (int32_t i = 0; i < NUM_STREAMS; ++i) {
        WaitForStream(static_cast<ECognitiveStream>(i), std::chrono::milliseconds(100));
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto syncTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    Metrics.SyncEvents.fetch_add(1);
    Metrics.SyncWaitTime.fetch_add(syncTime.count());
}

uint64_t FEchobeatsThreadPoolManager::CreateBarrier() {
    uint64_t barrierId = NextBarrierId.fetch_add(1);
    
    std::lock_guard<std::mutex> lock(BarriersMutex);
    Barriers[barrierId] = std::make_shared<std::barrier<>>(NUM_STREAMS);
    
    return barrierId;
}

void FEchobeatsThreadPoolManager::WaitAtBarrier(uint64_t BarrierId) {
    std::shared_ptr<std::barrier<>> barrier;
    
    {
        std::lock_guard<std::mutex> lock(BarriersMutex);
        auto it = Barriers.find(BarrierId);
        if (it == Barriers.end()) {
            return;  // Barrier doesn't exist
        }
        barrier = it->second;
    }
    
    barrier->arrive_and_wait();
}

//=============================================================================
// Configuration
//=============================================================================

void FEchobeatsThreadPoolManager::ConfigureNUMAAffinity() {
#ifdef __linux__
    // Check if NUMA is available
    if (numa_available() < 0) {
        std::cerr << "NUMA not available on this system" << std::endl;
        return;
    }
    
    int numNodes = numa_num_configured_nodes();
    if (numNodes < 1) {
        return;
    }
    
    // Map cognitive streams to NUMA nodes
    // Stream 0 (Cerebral) -> Node 0
    // Stream 1 (Somatic) -> Node 1 (or 0 if only 1 node)
    // Stream 2 (Autonomic) -> Node 2 (or 0 if only 1-2 nodes)
    
    for (int32_t streamIdx = 0; streamIdx < NUM_STREAMS; ++streamIdx) {
        int nodeIdx = streamIdx % numNodes;
        
        for (auto& thread : WorkerThreads[streamIdx]) {
            if (thread.joinable()) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                
                // Get CPUs on this NUMA node
                struct bitmask* nodeCpus = numa_allocate_cpumask();
                numa_node_to_cpus(nodeIdx, nodeCpus);
                
                // Set affinity to all CPUs on this node
                for (int cpu = 0; cpu < numa_num_configured_cpus(); ++cpu) {
                    if (numa_bitmask_isbitset(nodeCpus, cpu)) {
                        CPU_SET(cpu, &cpuset);
                    }
                }
                
                pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
                
                numa_free_cpumask(nodeCpus);
            }
        }
    }
#endif
}

void FEchobeatsThreadPoolManager::ConfigurePyTorchThreads() {
    // This would typically call into PyTorch's C++ API
    // For now, we just document the recommended configuration
    
    // torch::set_num_interop_threads(Config.InterOpThreads);
    // torch::set_num_threads(Config.IntraOpThreads);
    
    // The actual implementation would depend on how PyTorch is linked
    std::cout << "Recommended PyTorch thread configuration:" << std::endl;
    std::cout << "  Inter-op threads: " << Config.InterOpThreads << std::endl;
    std::cout << "  Intra-op threads: " << Config.IntraOpThreads << std::endl;
    std::cout << "  DataLoader workers: " << Config.DataLoaderWorkers << std::endl;
    std::cout << "  CUDA streams: " << Config.CUDAStreams << std::endl;
}

//=============================================================================
// Metrics
//=============================================================================

void FEchobeatsThreadPoolManager::ResetMetrics() {
    for (int32_t i = 0; i < NUM_STREAMS; ++i) {
        auto& m = Metrics.StreamMetrics[i];
        m.TasksSubmitted.store(0);
        m.TasksCompleted.store(0);
        m.TasksFailed.store(0);
        m.TotalExecutionTime.store(0);
        m.MaxExecutionTime.store(0);
        m.MinExecutionTime.store(UINT64_MAX);
        m.CurrentQueueDepth.store(0);
        m.MaxQueueDepth.store(0);
        m.QueueWaitTime.store(0);
        m.ContentionEvents.store(0);
    }
    
    Metrics.CyclesCompleted.store(0);
    Metrics.TotalCycleTime.store(0);
    Metrics.SyncEvents.store(0);
    Metrics.SyncWaitTime.store(0);
}

} // namespace DeepTreeEcho
