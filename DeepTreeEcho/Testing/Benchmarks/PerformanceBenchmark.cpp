//=============================================================================
// PerformanceBenchmark.cpp
// 
// Implementation of performance benchmarking framework for Deep Tree Echo.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "PerformanceBenchmark.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace DeepTreeEcho {
namespace Benchmarks {

// ============================================================================
// BenchmarkTimer Implementation
// ============================================================================

void BenchmarkTimer::Start()
{
    StartTime = std::chrono::high_resolution_clock::now();
    bIsRunning = true;
}

void BenchmarkTimer::Stop()
{
    EndTime = std::chrono::high_resolution_clock::now();
    bIsRunning = false;
}

void BenchmarkTimer::Reset()
{
    StartTime = std::chrono::high_resolution_clock::time_point();
    EndTime = std::chrono::high_resolution_clock::time_point();
    bIsRunning = false;
}

double BenchmarkTimer::GetElapsedMicroseconds() const
{
    auto end = bIsRunning ? std::chrono::high_resolution_clock::now() : EndTime;
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - StartTime);
    return static_cast<double>(duration.count()) / 1000.0;
}

double BenchmarkTimer::GetElapsedMilliseconds() const
{
    return GetElapsedMicroseconds() / 1000.0;
}

double BenchmarkTimer::GetElapsedSeconds() const
{
    return GetElapsedMicroseconds() / 1000000.0;
}

// ============================================================================
// MemoryTracker Implementation
// ============================================================================

void MemoryTracker::Start()
{
    StartSnapshot = GetCurrentUsage();
    bIsTracking = true;
}

void MemoryTracker::Stop()
{
    EndSnapshot = GetCurrentUsage();
    bIsTracking = false;
}

void MemoryTracker::Reset()
{
    StartSnapshot = MemorySnapshot();
    EndSnapshot = MemorySnapshot();
    bIsTracking = false;
}

MemorySnapshot MemoryTracker::GetSnapshot() const
{
    return EndSnapshot;
}

MemorySnapshot MemoryTracker::GetCurrentUsage() const
{
    MemorySnapshot snapshot;
    snapshot.Timestamp = PerformanceBenchmark::GetTimestamp();
    
#if defined(_WIN32) || defined(_WIN64)
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    {
        snapshot.UsedBytes = pmc.WorkingSetSize;
        snapshot.PeakBytes = pmc.PeakWorkingSetSize;
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
        // maxrss is in kilobytes on Linux, bytes on some BSDs
        snapshot.UsedBytes = static_cast<size_t>(usage.ru_maxrss) * 1024;
        snapshot.PeakBytes = snapshot.UsedBytes;
    }
#endif
    
    return snapshot;
}

size_t MemoryTracker::GetUsedBytes() const
{
    return EndSnapshot.UsedBytes;
}

size_t MemoryTracker::GetPeakBytes() const
{
    return EndSnapshot.PeakBytes;
}

size_t MemoryTracker::GetAllocatedDelta() const
{
    if (EndSnapshot.UsedBytes >= StartSnapshot.UsedBytes)
    {
        return EndSnapshot.UsedBytes - StartSnapshot.UsedBytes;
    }
    return 0;
}

// ============================================================================
// BenchmarkSuite Implementation
// ============================================================================

std::string BenchmarkSuite::ExportToJson() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    
    oss << "{\n";
    oss << "  \"suite_name\": \"" << SuiteName << "\",\n";
    oss << "  \"description\": \"" << Description << "\",\n";
    oss << "  \"total_duration\": " << TotalDuration << ",\n";
    oss << "  \"total_tests\": " << TotalTests << ",\n";
    oss << "  \"passed_tests\": " << PassedTests << ",\n";
    oss << "  \"failed_tests\": " << FailedTests << ",\n";
    oss << "  \"pass_rate\": " << GetPassRate() << ",\n";
    oss << "  \"results\": [\n";
    
    for (size_t i = 0; i < Results.size(); ++i)
    {
        const auto& r = Results[i];
        oss << "    {\n";
        oss << "      \"test_name\": \"" << r.TestName << "\",\n";
        oss << "      \"component_name\": \"" << r.ComponentName << "\",\n";
        oss << "      \"metric\": " << static_cast<int>(r.Metric) << ",\n";
        oss << "      \"min\": " << r.MinValue << ",\n";
        oss << "      \"max\": " << r.MaxValue << ",\n";
        oss << "      \"mean\": " << r.MeanValue << ",\n";
        oss << "      \"median\": " << r.MedianValue << ",\n";
        oss << "      \"std_dev\": " << r.StdDeviation << ",\n";
        oss << "      \"p95\": " << r.Percentile95 << ",\n";
        oss << "      \"p99\": " << r.Percentile99 << ",\n";
        oss << "      \"sample_count\": " << r.SampleCount << ",\n";
        oss << "      \"unit\": \"" << r.Unit << "\",\n";
        oss << "      \"target\": " << r.TargetValue << ",\n";
        oss << "      \"passed\": " << (r.PassedTarget ? "true" : "false") << "\n";
        oss << "    }" << (i < Results.size() - 1 ? "," : "") << "\n";
    }
    
    oss << "  ]\n";
    oss << "}\n";
    
    return oss.str();
}

std::string BenchmarkSuite::ExportToCsv() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    
    // Header
    oss << "TestName,Component,Metric,Min,Max,Mean,Median,StdDev,P95,P99,Samples,Unit,Target,Passed\n";
    
    // Data rows
    for (const auto& r : Results)
    {
        oss << r.TestName << ","
            << r.ComponentName << ","
            << static_cast<int>(r.Metric) << ","
            << r.MinValue << ","
            << r.MaxValue << ","
            << r.MeanValue << ","
            << r.MedianValue << ","
            << r.StdDeviation << ","
            << r.Percentile95 << ","
            << r.Percentile99 << ","
            << r.SampleCount << ","
            << r.Unit << ","
            << r.TargetValue << ","
            << (r.PassedTarget ? "true" : "false") << "\n";
    }
    
    return oss.str();
}

std::string BenchmarkSuite::ExportToMarkdown() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "# " << SuiteName << "\n\n";
    oss << Description << "\n\n";
    
    oss << "## Summary\n\n";
    oss << "| Metric | Value |\n";
    oss << "|--------|-------|\n";
    oss << "| Total Duration | " << TotalDuration << "s |\n";
    oss << "| Total Tests | " << TotalTests << " |\n";
    oss << "| Passed | " << PassedTests << " |\n";
    oss << "| Failed | " << FailedTests << " |\n";
    oss << "| Pass Rate | " << GetPassRate() << "% |\n\n";
    
    oss << "## Detailed Results\n\n";
    oss << "| Test | Component | Mean | P95 | P99 | Target | Status |\n";
    oss << "|------|-----------|------|-----|-----|--------|--------|\n";
    
    for (const auto& r : Results)
    {
        oss << "| " << r.TestName
            << " | " << r.ComponentName
            << " | " << r.MeanValue << " " << r.Unit
            << " | " << r.Percentile95 << " " << r.Unit
            << " | " << r.Percentile99 << " " << r.Unit
            << " | " << (r.TargetValue > 0 ? std::to_string(r.TargetValue) : "N/A") << " " << r.Unit
            << " | " << (r.PassedTarget ? "✅ Pass" : "❌ Fail") << " |\n";
    }
    
    return oss.str();
}

// ============================================================================
// PerformanceBenchmark Implementation
// ============================================================================

PerformanceBenchmark::PerformanceBenchmark()
{
}

PerformanceBenchmark::~PerformanceBenchmark()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

void PerformanceBenchmark::Initialize(const BenchmarkConfig& Config)
{
    CurrentConfig = Config;
    Timer.Reset();
    MemTracker.Reset();
    bIsInitialized = true;
}

void PerformanceBenchmark::Shutdown()
{
    Timer.Reset();
    MemTracker.Reset();
    bIsInitialized = false;
}

void PerformanceBenchmark::SetConfig(const BenchmarkConfig& Config)
{
    CurrentConfig = Config;
}

void PerformanceBenchmark::SetTargetThreshold(const std::string& BenchmarkName, double Target)
{
    CurrentConfig.TargetThresholds[BenchmarkName] = Target;
}

double PerformanceBenchmark::GetTimestamp()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}

MemorySnapshot PerformanceBenchmark::GetCurrentMemory()
{
    MemoryTracker tracker;
    return tracker.GetCurrentUsage();
}

void PerformanceBenchmark::ComputeStatistics(
    const std::vector<double>& Samples,
    double& OutMin,
    double& OutMax,
    double& OutMean,
    double& OutMedian,
    double& OutStdDev,
    double& OutP95,
    double& OutP99)
{
    if (Samples.empty())
    {
        OutMin = OutMax = OutMean = OutMedian = OutStdDev = OutP95 = OutP99 = 0.0;
        return;
    }
    
    // Create sorted copy for percentile calculations
    std::vector<double> sorted = Samples;
    std::sort(sorted.begin(), sorted.end());
    
    // Min/Max
    OutMin = sorted.front();
    OutMax = sorted.back();
    
    // Mean
    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    OutMean = sum / static_cast<double>(sorted.size());
    
    // Median
    size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0)
    {
        OutMedian = (sorted[mid - 1] + sorted[mid]) / 2.0;
    }
    else
    {
        OutMedian = sorted[mid];
    }
    
    // Standard deviation
    double sqSum = 0.0;
    for (double val : sorted)
    {
        sqSum += (val - OutMean) * (val - OutMean);
    }
    OutStdDev = std::sqrt(sqSum / static_cast<double>(sorted.size()));
    
    // Percentiles
    size_t p95Idx = static_cast<size_t>(0.95 * sorted.size());
    size_t p99Idx = static_cast<size_t>(0.99 * sorted.size());
    
    OutP95 = sorted[std::min(p95Idx, sorted.size() - 1)];
    OutP99 = sorted[std::min(p99Idx, sorted.size() - 1)];
}

std::vector<double> PerformanceBenchmark::RunIterations(
    std::function<void()> Fn,
    int32_t WarmupCount,
    int32_t MeasureCount)
{
    // Warmup iterations (results discarded)
    for (int32_t i = 0; i < WarmupCount; ++i)
    {
        Fn();
    }
    
    // Measurement iterations
    std::vector<double> samples;
    samples.reserve(MeasureCount);
    
    for (int32_t i = 0; i < MeasureCount; ++i)
    {
        Timer.Reset();
        Timer.Start();
        Fn();
        Timer.Stop();
        
        samples.push_back(Timer.GetElapsedMicroseconds());
    }
    
    return samples;
}

BenchmarkResult PerformanceBenchmark::CreateResult(
    const std::string& Name,
    const std::string& Component,
    ComponentCategory Category,
    BenchmarkMetric Metric,
    const std::vector<double>& Samples,
    const std::string& Unit)
{
    BenchmarkResult result;
    result.TestName = Name;
    result.ComponentName = Component;
    result.Category = Category;
    result.Metric = Metric;
    result.SampleCount = static_cast<int32_t>(Samples.size());
    result.IterationCount = CurrentConfig.MeasureIterations;
    result.Unit = Unit;
    
    // Compute statistics
    ComputeStatistics(Samples,
        result.MinValue, result.MaxValue, result.MeanValue,
        result.MedianValue, result.StdDeviation,
        result.Percentile95, result.Percentile99);
    
    // Store samples if configured
    if (CurrentConfig.CollectSamples)
    {
        double timestamp = GetTimestamp();
        for (size_t i = 0; i < Samples.size(); ++i)
        {
            BenchmarkSample sample;
            sample.Timestamp = timestamp + static_cast<double>(i) * 0.001;
            sample.Value = Samples[i];
            sample.Unit = Unit;
            sample.Metric = Metric;
            result.Samples.push_back(sample);
        }
    }
    
    // Check target threshold
    auto it = CurrentConfig.TargetThresholds.find(Name);
    if (it != CurrentConfig.TargetThresholds.end())
    {
        result.TargetValue = it->second;
    }
    result.PassedTarget = result.MeetsTarget();
    
    return result;
}

BenchmarkResult PerformanceBenchmark::RunCustomBenchmark(
    const std::string& Name,
    std::function<void()> Fn,
    ComponentCategory Category,
    BenchmarkMetric Metric)
{
    std::vector<double> samples = RunIterations(Fn,
        CurrentConfig.WarmupIterations,
        CurrentConfig.MeasureIterations);
    
    std::string unit;
    switch (Metric)
    {
        case BenchmarkMetric::Latency:
            unit = "μs";
            break;
        case BenchmarkMetric::Throughput:
            unit = "ops/s";
            break;
        case BenchmarkMetric::MemoryUsage:
        case BenchmarkMetric::MemoryPeak:
            unit = "bytes";
            break;
        case BenchmarkMetric::CPUUsage:
            unit = "%";
            break;
        case BenchmarkMetric::Accuracy:
            unit = "";
            break;
    }
    
    return CreateResult(Name, "Custom", Category, Metric, samples, unit);
}

BenchmarkResult PerformanceBenchmark::BenchmarkLatency(
    const std::string& Name,
    std::function<void()> Fn,
    ComponentCategory Category)
{
    return RunCustomBenchmark(Name, Fn, Category, BenchmarkMetric::Latency);
}

BenchmarkResult PerformanceBenchmark::BenchmarkThroughput(
    const std::string& Name,
    std::function<void()> Fn,
    ComponentCategory Category)
{
    // For throughput, measure how many operations per second
    double durationSeconds = 1.0; // Measure for 1 second
    int32_t operationCount = 0;
    
    BenchmarkTimer throughputTimer;
    throughputTimer.Start();
    
    while (throughputTimer.GetElapsedSeconds() < durationSeconds)
    {
        Fn();
        operationCount++;
    }
    
    throughputTimer.Stop();
    
    double elapsed = throughputTimer.GetElapsedSeconds();
    double opsPerSecond = static_cast<double>(operationCount) / elapsed;
    
    std::vector<double> samples;
    samples.push_back(opsPerSecond);
    
    return CreateResult(Name, "Custom", Category, BenchmarkMetric::Throughput, samples, "ops/s");
}

BenchmarkResult PerformanceBenchmark::BenchmarkMemory(
    const std::string& Name,
    std::function<void()> Fn,
    ComponentCategory Category)
{
    std::vector<double> samples;
    samples.reserve(CurrentConfig.MeasureIterations);
    
    for (int32_t i = 0; i < CurrentConfig.MeasureIterations; ++i)
    {
        MemTracker.Start();
        Fn();
        MemTracker.Stop();
        
        samples.push_back(static_cast<double>(MemTracker.GetAllocatedDelta()));
        MemTracker.Reset();
    }
    
    return CreateResult(Name, "Custom", Category, BenchmarkMetric::MemoryUsage, samples, "bytes");
}

// ============================================================================
// Component-Specific Benchmark Implementations
// ============================================================================

BenchmarkResult PerformanceBenchmark::BenchmarkReservoirInitialization()
{
    // Simulate reservoir initialization
    auto initFn = []() {
        // Simulate creating a 1000-node reservoir
        std::vector<double> weights(1000 * 1000, 0.0);
        for (size_t i = 0; i < weights.size(); ++i)
        {
            weights[i] = static_cast<double>(i % 100) / 100.0;
        }
    };
    
    auto result = BenchmarkLatency("reservoir_init", initFn, ComponentCategory::Reservoir);
    result.ComponentName = "Reservoir";
    result.TargetValue = 10000.0; // 10ms target
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkReservoirForward()
{
    // Simulate reservoir forward pass
    std::vector<double> state(1000, 0.5);
    std::vector<double> input(100, 1.0);
    
    auto forwardFn = [&state, &input]() {
        // Simple matrix-vector multiplication simulation
        for (size_t i = 0; i < state.size(); ++i)
        {
            double sum = 0.0;
            for (size_t j = 0; j < input.size(); ++j)
            {
                sum += input[j] * (static_cast<double>((i + j) % 100) / 100.0);
            }
            state[i] = std::tanh(sum + state[i] * 0.9);
        }
    };
    
    auto result = BenchmarkLatency("reservoir_forward", forwardFn, ComponentCategory::Reservoir);
    result.ComponentName = "Reservoir";
    result.TargetValue = Targets::ReservoirForwardLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkReservoirStateUpdate()
{
    std::vector<double> state(1000, 0.5);
    
    auto updateFn = [&state]() {
        // State update with leak rate
        const double leakRate = 0.3;
        for (size_t i = 0; i < state.size(); ++i)
        {
            state[i] = (1.0 - leakRate) * state[i] + leakRate * std::tanh(state[i] * 0.9);
        }
    };
    
    auto result = BenchmarkLatency("reservoir_state_update", updateFn, ComponentCategory::Reservoir);
    result.ComponentName = "Reservoir";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkReservoirMemory()
{
    auto allocFn = []() {
        // Simulate reservoir memory allocation
        std::vector<double> weights(1000 * 1000);
        std::vector<double> state(1000);
        std::vector<double> input(100);
        // Force allocation by touching memory
        weights[0] = 1.0;
        state[0] = 1.0;
        input[0] = 1.0;
    };
    
    auto result = BenchmarkMemory("reservoir_memory", allocFn, ComponentCategory::Reservoir);
    result.ComponentName = "Reservoir";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkCognitiveStepExecution()
{
    // Simulate one step of the 12-step cognitive cycle
    auto stepFn = []() {
        // Simulate processing for one cognitive step
        std::vector<double> perception(256, 0.5);
        std::vector<double> action(64, 0.0);
        std::vector<double> reflection(128, 0.0);
        
        // Processing simulation
        for (size_t i = 0; i < action.size(); ++i)
        {
            double sum = 0.0;
            for (size_t j = 0; j < 4; ++j)
            {
                sum += perception[i * 4 + j];
            }
            action[i] = std::tanh(sum);
        }
        
        for (size_t i = 0; i < reflection.size(); ++i)
        {
            reflection[i] = action[i % 64] * 0.5;
        }
    };
    
    auto result = BenchmarkLatency("cognitive_step", stepFn, ComponentCategory::CognitiveLoop);
    result.ComponentName = "CognitiveLoop";
    result.TargetValue = Targets::CognitiveStepLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkStreamSynchronization()
{
    // Simulate 3-stream synchronization (120° phase offset)
    auto syncFn = []() {
        std::vector<double> stream1(64, 0.5);
        std::vector<double> stream2(64, 0.5);
        std::vector<double> stream3(64, 0.5);
        std::vector<double> integrated(64, 0.0);
        
        // Synchronization at triadic point
        for (size_t i = 0; i < 64; ++i)
        {
            integrated[i] = (stream1[i] + stream2[i] + stream3[i]) / 3.0;
            // Update streams
            stream1[i] = integrated[i] * 0.9 + stream1[i] * 0.1;
            stream2[i] = integrated[i] * 0.9 + stream2[i] * 0.1;
            stream3[i] = integrated[i] * 0.9 + stream3[i] * 0.1;
        }
    };
    
    auto result = BenchmarkLatency("stream_sync", syncFn, ComponentCategory::CognitiveLoop);
    result.ComponentName = "CognitiveLoop";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkTriadicIntegration()
{
    // Simulate triadic integration at sync points {1,5,9}, {2,6,10}, etc.
    auto triadicFn = []() {
        std::vector<std::vector<double>> steps(12, std::vector<double>(32, 0.5));
        
        // Integrate triadic groups
        std::vector<std::vector<int>> triads = {{0,4,8}, {1,5,9}, {2,6,10}, {3,7,11}};
        
        for (const auto& triad : triads)
        {
            for (size_t i = 0; i < 32; ++i)
            {
                double sum = steps[triad[0]][i] + steps[triad[1]][i] + steps[triad[2]][i];
                double integrated = sum / 3.0;
                steps[triad[0]][i] = integrated;
                steps[triad[1]][i] = integrated;
                steps[triad[2]][i] = integrated;
            }
        }
    };
    
    auto result = BenchmarkLatency("triadic_integration", triadicFn, ComponentCategory::CognitiveLoop);
    result.ComponentName = "CognitiveLoop";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkEpisodicMemoryStore()
{
    // Simulate storing episodic memory trace
    auto storeFn = []() {
        // Create memory trace
        std::vector<double> content(256, 0.5);
        std::vector<double> context(64, 0.3);
        double salience = 0.7;
        
        // Simulate encoding
        std::vector<double> encoded(content.size());
        for (size_t i = 0; i < content.size(); ++i)
        {
            encoded[i] = content[i] * salience + context[i % 64] * (1.0 - salience);
        }
    };
    
    auto result = BenchmarkLatency("episodic_store", storeFn, ComponentCategory::Memory);
    result.ComponentName = "Memory";
    result.TargetValue = Targets::MemoryStoreLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkEpisodicMemoryRetrieve()
{
    // Simulate episodic memory retrieval
    std::vector<std::vector<double>> memories(100, std::vector<double>(256, 0.5));
    std::vector<double> query(256, 0.6);
    
    auto retrieveFn = [&memories, &query]() {
        double bestScore = -1.0;
        size_t bestIdx = 0;
        
        for (size_t m = 0; m < memories.size(); ++m)
        {
            double score = 0.0;
            for (size_t i = 0; i < query.size(); ++i)
            {
                score += query[i] * memories[m][i];
            }
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = m;
            }
        }
        (void)bestIdx; // Suppress unused warning
    };
    
    auto result = BenchmarkLatency("episodic_retrieve", retrieveFn, ComponentCategory::Memory);
    result.ComponentName = "Memory";
    result.TargetValue = Targets::MemoryRetrieveLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkHypergraphQuery()
{
    // Simulate hypergraph pattern query
    auto queryFn = []() {
        // Simulate hypergraph traversal
        std::map<int, std::vector<int>> edges;
        for (int i = 0; i < 100; ++i)
        {
            edges[i] = {(i + 1) % 100, (i + 2) % 100, (i + 3) % 100};
        }
        
        // Simple BFS-like traversal
        std::vector<bool> visited(100, false);
        std::vector<int> frontier = {0};
        int depth = 0;
        
        while (!frontier.empty() && depth < 5)
        {
            std::vector<int> nextFrontier;
            for (int node : frontier)
            {
                if (!visited[node])
                {
                    visited[node] = true;
                    for (int neighbor : edges[node])
                    {
                        if (!visited[neighbor])
                        {
                            nextFrontier.push_back(neighbor);
                        }
                    }
                }
            }
            frontier = nextFrontier;
            depth++;
        }
    };
    
    auto result = BenchmarkLatency("hypergraph_query", queryFn, ComponentCategory::Memory);
    result.ComponentName = "Memory";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkMemoryConsolidation()
{
    // Simulate memory consolidation process
    auto consolidateFn = []() {
        std::vector<std::vector<double>> shortTerm(50, std::vector<double>(128, 0.5));
        std::vector<std::vector<double>> longTerm(200, std::vector<double>(128, 0.3));
        
        // Consolidation: merge short-term into long-term
        for (size_t i = 0; i < shortTerm.size(); ++i)
        {
            size_t targetIdx = i % longTerm.size();
            for (size_t j = 0; j < 128; ++j)
            {
                longTerm[targetIdx][j] = 0.9 * longTerm[targetIdx][j] + 0.1 * shortTerm[i][j];
            }
        }
    };
    
    auto result = BenchmarkLatency("memory_consolidation", consolidateFn, ComponentCategory::Memory);
    result.ComponentName = "Memory";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkEventRecording()
{
    // Simulate temporal event recording
    auto recordFn = []() {
        struct Event {
            double timestamp;
            std::string description;
            std::vector<double> content;
            double salience;
        };
        
        Event event;
        event.timestamp = 1234567.89;
        event.description = "Test event for benchmarking";
        event.content.resize(64, 0.5);
        event.salience = 0.7;
        (void)event; // Suppress unused variable warning
    };
    
    auto result = BenchmarkLatency("event_recording", recordFn, ComponentCategory::Temporal);
    result.ComponentName = "Temporal";
    result.TargetValue = Targets::EventRecordLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkCausalInference()
{
    // Simulate causal chain inference
    auto inferenceFn = []() {
        // Simulate event sequence
        std::vector<std::pair<double, std::vector<double>>> events;
        for (int i = 0; i < 100; ++i)
        {
            events.push_back({static_cast<double>(i), std::vector<double>(32, static_cast<double>(i % 10) / 10.0)});
        }
        
        // Infer causal links based on temporal proximity and content similarity
        std::vector<std::tuple<int, int, double>> links;
        for (size_t i = 0; i < events.size(); ++i)
        {
            for (size_t j = i + 1; j < std::min(i + 10, events.size()); ++j)
            {
                double similarity = 0.0;
                for (size_t k = 0; k < 32; ++k)
                {
                    similarity += events[i].second[k] * events[j].second[k];
                }
                if (similarity > 0.5)
                {
                    links.push_back({static_cast<int>(i), static_cast<int>(j), similarity});
                }
            }
        }
    };
    
    auto result = BenchmarkLatency("causal_inference", inferenceFn, ComponentCategory::Temporal);
    result.ComponentName = "Temporal";
    result.TargetValue = Targets::CausalInferenceLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkTemporalQuery()
{
    // Simulate temporal range query
    auto queryFn = []() {
        std::vector<std::pair<double, int>> events;
        for (int i = 0; i < 10000; ++i)
        {
            events.push_back({static_cast<double>(i), i});
        }
        
        // Query events in range [1000, 2000]
        std::vector<int> results;
        for (const auto& event : events)
        {
            if (event.first >= 1000.0 && event.first <= 2000.0)
            {
                results.push_back(event.second);
            }
        }
    };
    
    auto result = BenchmarkLatency("temporal_query", queryFn, ComponentCategory::Temporal);
    result.ComponentName = "Temporal";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkSensoryIntegration()
{
    // Simulate multi-modal sensory integration
    auto integrateFn = []() {
        std::vector<double> visual(256, 0.5);
        std::vector<double> auditory(128, 0.4);
        std::vector<double> proprioceptive(64, 0.6);
        std::vector<double> integrated(256);
        
        // Multi-modal fusion
        for (size_t i = 0; i < 256; ++i)
        {
            double v = visual[i];
            double a = auditory[i % 128];
            double p = proprioceptive[i % 64];
            integrated[i] = 0.5 * v + 0.3 * a + 0.2 * p;
        }
    };
    
    auto result = BenchmarkLatency("sensory_integration", integrateFn, ComponentCategory::Embodied);
    result.ComponentName = "Embodied";
    result.TargetValue = Targets::SensoryIntegrationLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkMotorPlanning()
{
    // Simulate motor planning
    auto planFn = []() {
        std::vector<double> currentState(32);
        std::vector<double> goalState(32);
        std::vector<std::vector<double>> trajectory;
        
        // Simple linear interpolation trajectory
        int steps = 10;
        for (int t = 0; t <= steps; ++t)
        {
            std::vector<double> state(32);
            double alpha = static_cast<double>(t) / steps;
            for (size_t i = 0; i < 32; ++i)
            {
                state[i] = (1.0 - alpha) * currentState[i] + alpha * goalState[i];
            }
            trajectory.push_back(state);
        }
    };
    
    auto result = BenchmarkLatency("motor_planning", planFn, ComponentCategory::Embodied);
    result.ComponentName = "Embodied";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkAffordanceDetection()
{
    // Simulate affordance detection
    auto detectFn = []() {
        std::vector<std::vector<double>> objects(20, std::vector<double>(64, 0.5));
        std::vector<std::vector<double>> affordanceTemplates(10, std::vector<double>(64, 0.6));
        
        // Match objects to affordances
        for (size_t o = 0; o < objects.size(); ++o)
        {
            double bestMatch = 0.0;
            int bestAffordance = -1;
            
            for (size_t a = 0; a < affordanceTemplates.size(); ++a)
            {
                double match = 0.0;
                for (size_t i = 0; i < 64; ++i)
                {
                    match += objects[o][i] * affordanceTemplates[a][i];
                }
                if (match > bestMatch)
                {
                    bestMatch = match;
                    bestAffordance = static_cast<int>(a);
                }
            }
            (void)bestAffordance;
        }
    };
    
    auto result = BenchmarkLatency("affordance_detection", detectFn, ComponentCategory::Embodied);
    result.ComponentName = "Embodied";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkBeliefUpdate()
{
    // Simulate Bayesian belief update
    auto updateFn = []() {
        std::vector<double> prior(100, 0.01);
        std::vector<double> likelihood(100, 0.5);
        std::vector<double> posterior(100);
        
        // Bayesian update
        double evidence = 0.0;
        for (size_t i = 0; i < 100; ++i)
        {
            evidence += prior[i] * likelihood[i];
        }
        
        for (size_t i = 0; i < 100; ++i)
        {
            posterior[i] = (prior[i] * likelihood[i]) / evidence;
        }
    };
    
    auto result = BenchmarkLatency("belief_update", updateFn, ComponentCategory::ActiveInference);
    result.ComponentName = "ActiveInference";
    result.TargetValue = Targets::BeliefUpdateLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkPolicySelection()
{
    // Simulate policy selection via expected free energy
    auto selectFn = []() {
        std::vector<std::vector<double>> policies(8, std::vector<double>(4, 0.0));
        std::vector<double> expectedFreeEnergy(8);
        
        // Compute expected free energy for each policy
        for (size_t p = 0; p < policies.size(); ++p)
        {
            double efe = 0.0;
            for (size_t t = 0; t < 4; ++t)
            {
                // Simulate EFE computation
                efe += policies[p][t] * std::log(policies[p][t] + 0.001);
            }
            expectedFreeEnergy[p] = -efe;
        }
        
        // Select policy with minimum EFE
        auto minIt = std::min_element(expectedFreeEnergy.begin(), expectedFreeEnergy.end());
        (void)minIt;
    };
    
    auto result = BenchmarkLatency("policy_selection", selectFn, ComponentCategory::ActiveInference);
    result.ComponentName = "ActiveInference";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkFreeEnergyComputation()
{
    // Simulate variational free energy computation
    auto computeFn = []() {
        std::vector<double> q(64, 1.0 / 64.0);  // Approximate posterior
        std::vector<double> p(64, 1.0 / 64.0);  // Prior
        std::vector<double> observation(64, 0.5);
        
        // KL divergence + likelihood
        double kl = 0.0;
        for (size_t i = 0; i < 64; ++i)
        {
            if (q[i] > 0 && p[i] > 0)
            {
                kl += q[i] * std::log(q[i] / p[i]);
            }
        }
        
        double likelihood = 0.0;
        for (size_t i = 0; i < 64; ++i)
        {
            likelihood += q[i] * observation[i];
        }
        
        double freeEnergy = kl - likelihood;
        (void)freeEnergy;
    };
    
    auto result = BenchmarkLatency("free_energy_computation", computeFn, ComponentCategory::ActiveInference);
    result.ComponentName = "ActiveInference";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkFullCognitiveLoop()
{
    // Simulate full 12-step cognitive loop
    auto loopFn = []() {
        std::vector<double> perception(256, 0.5);
        std::vector<double> state(512, 0.0);
        std::vector<double> action(64, 0.0);
        
        // 12 steps with 3 concurrent streams
        for (int step = 0; step < 12; ++step)
        {
            // Process based on stream (perceiving, acting, reflecting)
            int stream = step % 3;
            
            if (stream == 0) // Perceiving
            {
                for (size_t i = 0; i < 256; ++i)
                {
                    state[i] = std::tanh(perception[i] + state[i] * 0.9);
                }
            }
            else if (stream == 1) // Acting
            {
                for (size_t i = 0; i < 64; ++i)
                {
                    action[i] = std::tanh(state[i * 4] + state[i * 4 + 1]);
                }
            }
            else // Reflecting
            {
                for (size_t i = 256; i < 512; ++i)
                {
                    state[i] = state[i] * 0.8 + action[i % 64] * 0.2;
                }
            }
        }
    };
    
    auto result = BenchmarkLatency("full_cognitive_loop", loopFn, ComponentCategory::Integration);
    result.ComponentName = "Integration";
    result.TargetValue = Targets::FullCycleLatency;
    result.PassedTarget = result.MeetsTarget();
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkCrossComponentLatency()
{
    // Measure latency of data passing between components
    auto crossFn = []() {
        // Simulate data flow: Perception -> Reservoir -> Memory -> Action
        std::vector<double> perception(128, 0.5);
        std::vector<double> reservoirState(256, 0.0);
        std::vector<double> memoryTrace(128, 0.0);
        std::vector<double> action(32, 0.0);
        
        // Perception to Reservoir
        for (size_t i = 0; i < 256; ++i)
        {
            reservoirState[i] = std::tanh(perception[i % 128] * 0.5);
        }
        
        // Reservoir to Memory
        for (size_t i = 0; i < 128; ++i)
        {
            memoryTrace[i] = reservoirState[i * 2] + reservoirState[i * 2 + 1];
        }
        
        // Memory to Action
        for (size_t i = 0; i < 32; ++i)
        {
            action[i] = memoryTrace[i * 4] + memoryTrace[i * 4 + 1];
        }
    };
    
    auto result = BenchmarkLatency("cross_component_latency", crossFn, ComponentCategory::Integration);
    result.ComponentName = "Integration";
    return result;
}

BenchmarkResult PerformanceBenchmark::BenchmarkEndToEndThroughput()
{
    // Measure end-to-end throughput
    auto e2eFn = []() {
        // Single end-to-end processing unit
        std::vector<double> input(128, 0.5);
        std::vector<double> output(64, 0.0);
        
        for (size_t i = 0; i < 64; ++i)
        {
            output[i] = std::tanh(input[i * 2] + input[i * 2 + 1]);
        }
    };
    
    return BenchmarkThroughput("e2e_throughput", e2eFn, ComponentCategory::Integration);
}

// ============================================================================
// Benchmark Suite Runners
// ============================================================================

std::vector<BenchmarkResult> PerformanceBenchmark::RunReservoirBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkReservoirInitialization());
    results.push_back(BenchmarkReservoirForward());
    results.push_back(BenchmarkReservoirStateUpdate());
    results.push_back(BenchmarkReservoirMemory());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunCognitiveLoopBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkCognitiveStepExecution());
    results.push_back(BenchmarkStreamSynchronization());
    results.push_back(BenchmarkTriadicIntegration());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunMemoryBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkEpisodicMemoryStore());
    results.push_back(BenchmarkEpisodicMemoryRetrieve());
    results.push_back(BenchmarkHypergraphQuery());
    results.push_back(BenchmarkMemoryConsolidation());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunEmbodiedBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkSensoryIntegration());
    results.push_back(BenchmarkMotorPlanning());
    results.push_back(BenchmarkAffordanceDetection());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunTemporalBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkEventRecording());
    results.push_back(BenchmarkCausalInference());
    results.push_back(BenchmarkTemporalQuery());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunActiveInferenceBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkBeliefUpdate());
    results.push_back(BenchmarkPolicySelection());
    results.push_back(BenchmarkFreeEnergyComputation());
    
    return results;
}

std::vector<BenchmarkResult> PerformanceBenchmark::RunIntegrationBenchmarks()
{
    std::vector<BenchmarkResult> results;
    
    results.push_back(BenchmarkFullCognitiveLoop());
    results.push_back(BenchmarkCrossComponentLatency());
    results.push_back(BenchmarkEndToEndThroughput());
    
    return results;
}

BenchmarkSuite PerformanceBenchmark::RunBenchmarks(ComponentCategory Category)
{
    BenchmarkSuite suite;
    suite.StartTime = GetTimestamp();
    
    switch (Category)
    {
        case ComponentCategory::Reservoir:
            suite.SuiteName = "Reservoir Benchmarks";
            suite.Description = "Echo State Network performance benchmarks";
            suite.Results = RunReservoirBenchmarks();
            break;
        case ComponentCategory::CognitiveLoop:
            suite.SuiteName = "Cognitive Loop Benchmarks";
            suite.Description = "12-step cognitive cycle performance benchmarks";
            suite.Results = RunCognitiveLoopBenchmarks();
            break;
        case ComponentCategory::Memory:
            suite.SuiteName = "Memory Benchmarks";
            suite.Description = "Episodic and hypergraph memory performance benchmarks";
            suite.Results = RunMemoryBenchmarks();
            break;
        case ComponentCategory::Embodied:
            suite.SuiteName = "Embodied Cognition Benchmarks";
            suite.Description = "4E embodied cognition performance benchmarks";
            suite.Results = RunEmbodiedBenchmarks();
            break;
        case ComponentCategory::Temporal:
            suite.SuiteName = "Temporal Reasoning Benchmarks";
            suite.Description = "Temporal event and causal chain performance benchmarks";
            suite.Results = RunTemporalBenchmarks();
            break;
        case ComponentCategory::ActiveInference:
            suite.SuiteName = "Active Inference Benchmarks";
            suite.Description = "Predictive processing performance benchmarks";
            suite.Results = RunActiveInferenceBenchmarks();
            break;
        case ComponentCategory::Integration:
            suite.SuiteName = "Integration Benchmarks";
            suite.Description = "Cross-component integration performance benchmarks";
            suite.Results = RunIntegrationBenchmarks();
            break;
        case ComponentCategory::All:
        default:
            return RunAllBenchmarks();
    }
    
    suite.EndTime = GetTimestamp();
    suite.TotalDuration = suite.EndTime - suite.StartTime;
    suite.TotalTests = static_cast<int32_t>(suite.Results.size());
    
    for (const auto& result : suite.Results)
    {
        if (result.PassedTarget)
            suite.PassedTests++;
        else
            suite.FailedTests++;
    }
    
    return suite;
}

BenchmarkSuite PerformanceBenchmark::RunAllBenchmarks()
{
    BenchmarkSuite suite;
    suite.SuiteName = "Deep Tree Echo Performance Benchmark Suite";
    suite.Description = "Comprehensive performance benchmarks for all cognitive components";
    suite.StartTime = GetTimestamp();
    
    // Collect all benchmarks
    auto reservoirResults = RunReservoirBenchmarks();
    auto cognitiveResults = RunCognitiveLoopBenchmarks();
    auto memoryResults = RunMemoryBenchmarks();
    auto embodiedResults = RunEmbodiedBenchmarks();
    auto temporalResults = RunTemporalBenchmarks();
    auto inferenceResults = RunActiveInferenceBenchmarks();
    auto integrationResults = RunIntegrationBenchmarks();
    
    // Merge all results
    suite.Results.insert(suite.Results.end(), reservoirResults.begin(), reservoirResults.end());
    suite.Results.insert(suite.Results.end(), cognitiveResults.begin(), cognitiveResults.end());
    suite.Results.insert(suite.Results.end(), memoryResults.begin(), memoryResults.end());
    suite.Results.insert(suite.Results.end(), embodiedResults.begin(), embodiedResults.end());
    suite.Results.insert(suite.Results.end(), temporalResults.begin(), temporalResults.end());
    suite.Results.insert(suite.Results.end(), inferenceResults.begin(), inferenceResults.end());
    suite.Results.insert(suite.Results.end(), integrationResults.begin(), integrationResults.end());
    
    suite.EndTime = GetTimestamp();
    suite.TotalDuration = suite.EndTime - suite.StartTime;
    suite.TotalTests = static_cast<int32_t>(suite.Results.size());
    
    for (const auto& result : suite.Results)
    {
        if (result.PassedTarget)
            suite.PassedTests++;
        else
            suite.FailedTests++;
    }
    
    return suite;
}

} // namespace Benchmarks
} // namespace DeepTreeEcho
