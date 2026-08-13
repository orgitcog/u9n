//=============================================================================
// PerformanceBenchmark.h
// 
// Performance benchmarking framework for Deep Tree Echo cognitive components.
// Measures latency, throughput, and memory usage across all major subsystems.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace DeepTreeEcho {
namespace Benchmarks {

/**
 * Benchmark metric types
 */
enum class BenchmarkMetric
{
    Latency,        // Operation time in microseconds
    Throughput,     // Operations per second
    MemoryUsage,    // Memory consumption in bytes
    MemoryPeak,     // Peak memory usage in bytes
    CPUUsage,       // CPU utilization percentage
    Accuracy        // Result accuracy (0-1)
};

/**
 * Benchmark component categories
 */
enum class ComponentCategory
{
    Reservoir,          // Echo State Network operations
    CognitiveLoop,      // 12-step cognitive cycle
    Memory,             // Episodic, semantic, hypergraph memory
    Embodied,           // 4E embodied cognition
    Temporal,           // Temporal event graph, causal chains
    ActiveInference,    // Predictive processing
    Integration,        // Cross-component integration
    All                 // All components
};

/**
 * Single benchmark sample
 */
struct BenchmarkSample
{
    double Timestamp = 0.0;     // Sample timestamp (seconds)
    double Value = 0.0;         // Measured value
    std::string Unit;           // Unit of measurement
    BenchmarkMetric Metric = BenchmarkMetric::Latency;
};

/**
 * Benchmark result for a single test
 */
struct BenchmarkResult
{
    std::string TestName;               // Test identifier
    std::string ComponentName;          // Component being tested
    ComponentCategory Category = ComponentCategory::All;
    BenchmarkMetric Metric = BenchmarkMetric::Latency;
    
    // Statistical summary
    double MinValue = 0.0;
    double MaxValue = 0.0;
    double MeanValue = 0.0;
    double MedianValue = 0.0;
    double StdDeviation = 0.0;
    double Percentile95 = 0.0;
    double Percentile99 = 0.0;
    
    // Metadata
    int32_t SampleCount = 0;
    int32_t IterationCount = 0;
    double TotalDuration = 0.0;         // Total benchmark duration
    std::string Unit;                   // Unit of measurement
    
    // Samples (optional, for detailed analysis)
    std::vector<BenchmarkSample> Samples;
    
    // Target/threshold (if defined)
    double TargetValue = 0.0;
    bool PassedTarget = false;
    
    /**
     * Check if result meets target threshold
     */
    bool MeetsTarget() const
    {
        if (TargetValue <= 0.0) return true;
        
        switch (Metric)
        {
            case BenchmarkMetric::Latency:
            case BenchmarkMetric::MemoryUsage:
            case BenchmarkMetric::MemoryPeak:
                return MeanValue <= TargetValue;
            case BenchmarkMetric::Throughput:
            case BenchmarkMetric::Accuracy:
                return MeanValue >= TargetValue;
            default:
                return true;
        }
    }
};

/**
 * Configuration for benchmark execution
 */
struct BenchmarkConfig
{
    // Execution settings
    int32_t WarmupIterations = 10;      // Iterations to discard
    int32_t MeasureIterations = 100;    // Iterations to measure
    int32_t RepeatCount = 3;            // Number of times to repeat benchmark
    double MaxDuration = 60.0;          // Maximum duration in seconds
    
    // Memory tracking
    bool TrackMemory = true;            // Track memory usage
    bool TrackPeakMemory = true;        // Track peak memory
    
    // Output settings
    bool CollectSamples = false;        // Store individual samples
    bool VerboseOutput = false;         // Detailed logging
    
    // Target thresholds (optional)
    std::map<std::string, double> TargetThresholds;
};

/**
 * Suite of benchmark results
 */
struct BenchmarkSuite
{
    std::string SuiteName;
    std::string Description;
    double StartTime = 0.0;
    double EndTime = 0.0;
    double TotalDuration = 0.0;
    
    std::vector<BenchmarkResult> Results;
    
    // Summary statistics
    int32_t TotalTests = 0;
    int32_t PassedTests = 0;
    int32_t FailedTests = 0;
    
    /**
     * Get pass rate as percentage
     */
    double GetPassRate() const
    {
        if (TotalTests == 0) return 0.0;
        return (static_cast<double>(PassedTests) / TotalTests) * 100.0;
    }
    
    /**
     * Export results to JSON string
     */
    std::string ExportToJson() const;
    
    /**
     * Export results to CSV string
     */
    std::string ExportToCsv() const;
    
    /**
     * Export results to markdown string
     */
    std::string ExportToMarkdown() const;
};

/**
 * Memory snapshot for tracking allocations
 */
struct MemorySnapshot
{
    size_t UsedBytes = 0;
    size_t PeakBytes = 0;
    size_t AllocationCount = 0;
    double Timestamp = 0.0;
};

/**
 * High-precision timer for benchmarking
 */
class BenchmarkTimer
{
public:
    void Start();
    void Stop();
    void Reset();
    
    double GetElapsedMicroseconds() const;
    double GetElapsedMilliseconds() const;
    double GetElapsedSeconds() const;
    
    bool IsRunning() const { return bIsRunning; }
    
private:
    std::chrono::high_resolution_clock::time_point StartTime;
    std::chrono::high_resolution_clock::time_point EndTime;
    bool bIsRunning = false;
};

/**
 * Memory tracker for benchmark measurements
 */
class MemoryTracker
{
public:
    void Start();
    void Stop();
    void Reset();
    
    MemorySnapshot GetSnapshot() const;
    MemorySnapshot GetCurrentUsage() const;
    
    size_t GetUsedBytes() const;
    size_t GetPeakBytes() const;
    size_t GetAllocatedDelta() const;
    
private:
    MemorySnapshot StartSnapshot;
    MemorySnapshot EndSnapshot;
    bool bIsTracking = false;
};

/**
 * Performance Benchmark Framework
 * 
 * Main class for running performance benchmarks across all Deep Tree Echo
 * cognitive components. Measures latency, throughput, and memory usage.
 * 
 * Usage:
 *   PerformanceBenchmark benchmark;
 *   benchmark.Initialize();
 *   
 *   // Run specific benchmarks
 *   benchmark.RunReservoirBenchmarks();
 *   benchmark.RunCognitiveLoopBenchmarks();
 *   
 *   // Or run all benchmarks
 *   auto suite = benchmark.RunAllBenchmarks();
 *   
 *   // Export results
 *   std::string report = suite.ExportToMarkdown();
 */
class PerformanceBenchmark
{
public:
    PerformanceBenchmark();
    ~PerformanceBenchmark();
    
    /**
     * Initialize benchmark framework
     */
    void Initialize(const BenchmarkConfig& Config = BenchmarkConfig());
    
    /**
     * Shutdown and cleanup
     */
    void Shutdown();
    
    // ========================================
    // BENCHMARK EXECUTION
    // ========================================
    
    /**
     * Run all benchmarks across all components
     */
    BenchmarkSuite RunAllBenchmarks();
    
    /**
     * Run benchmarks for a specific category
     */
    BenchmarkSuite RunBenchmarks(ComponentCategory Category);
    
    /**
     * Run reservoir/ESN benchmarks
     */
    std::vector<BenchmarkResult> RunReservoirBenchmarks();
    
    /**
     * Run cognitive loop benchmarks
     */
    std::vector<BenchmarkResult> RunCognitiveLoopBenchmarks();
    
    /**
     * Run memory system benchmarks
     */
    std::vector<BenchmarkResult> RunMemoryBenchmarks();
    
    /**
     * Run embodied cognition benchmarks
     */
    std::vector<BenchmarkResult> RunEmbodiedBenchmarks();
    
    /**
     * Run temporal reasoning benchmarks
     */
    std::vector<BenchmarkResult> RunTemporalBenchmarks();
    
    /**
     * Run active inference benchmarks
     */
    std::vector<BenchmarkResult> RunActiveInferenceBenchmarks();
    
    /**
     * Run integration benchmarks
     */
    std::vector<BenchmarkResult> RunIntegrationBenchmarks();
    
    // ========================================
    // CUSTOM BENCHMARK SUPPORT
    // ========================================
    
    /**
     * Run a custom benchmark function
     * 
     * @param Name Benchmark name
     * @param Fn Function to benchmark
     * @param Category Component category
     * @param Metric Metric to measure
     * @return Benchmark result
     */
    BenchmarkResult RunCustomBenchmark(
        const std::string& Name,
        std::function<void()> Fn,
        ComponentCategory Category = ComponentCategory::All,
        BenchmarkMetric Metric = BenchmarkMetric::Latency
    );
    
    /**
     * Run a latency benchmark
     * 
     * @param Name Benchmark name
     * @param Fn Function to benchmark
     * @param Category Component category
     * @return Latency result in microseconds
     */
    BenchmarkResult BenchmarkLatency(
        const std::string& Name,
        std::function<void()> Fn,
        ComponentCategory Category = ComponentCategory::All
    );
    
    /**
     * Run a throughput benchmark
     * 
     * @param Name Benchmark name
     * @param Fn Function to benchmark (should process one unit)
     * @param Category Component category
     * @return Throughput result in operations per second
     */
    BenchmarkResult BenchmarkThroughput(
        const std::string& Name,
        std::function<void()> Fn,
        ComponentCategory Category = ComponentCategory::All
    );
    
    /**
     * Run a memory benchmark
     * 
     * @param Name Benchmark name
     * @param Fn Function to benchmark
     * @param Category Component category
     * @return Memory usage result in bytes
     */
    BenchmarkResult BenchmarkMemory(
        const std::string& Name,
        std::function<void()> Fn,
        ComponentCategory Category = ComponentCategory::All
    );
    
    // ========================================
    // CONFIGURATION
    // ========================================
    
    /**
     * Get current configuration
     */
    const BenchmarkConfig& GetConfig() const { return CurrentConfig; }
    
    /**
     * Update configuration
     */
    void SetConfig(const BenchmarkConfig& Config);
    
    /**
     * Set target threshold for a benchmark
     */
    void SetTargetThreshold(const std::string& BenchmarkName, double Target);
    
    // ========================================
    // UTILITIES
    // ========================================
    
    /**
     * Get current timestamp
     */
    static double GetTimestamp();
    
    /**
     * Get current memory usage
     */
    static MemorySnapshot GetCurrentMemory();
    
    /**
     * Compute statistics from samples
     */
    static void ComputeStatistics(
        const std::vector<double>& Samples,
        double& OutMin,
        double& OutMax,
        double& OutMean,
        double& OutMedian,
        double& OutStdDev,
        double& OutP95,
        double& OutP99
    );
    
private:
    BenchmarkConfig CurrentConfig;
    bool bIsInitialized = false;
    
    BenchmarkTimer Timer;
    MemoryTracker MemTracker;
    
    // Internal benchmark helpers
    std::vector<double> RunIterations(std::function<void()> Fn, int32_t WarmupCount, int32_t MeasureCount);
    BenchmarkResult CreateResult(const std::string& Name, const std::string& Component,
                                 ComponentCategory Category, BenchmarkMetric Metric,
                                 const std::vector<double>& Samples, const std::string& Unit);
    
    // Reservoir benchmark implementations
    BenchmarkResult BenchmarkReservoirInitialization();
    BenchmarkResult BenchmarkReservoirForward();
    BenchmarkResult BenchmarkReservoirStateUpdate();
    BenchmarkResult BenchmarkReservoirMemory();
    
    // Cognitive loop benchmark implementations
    BenchmarkResult BenchmarkCognitiveStepExecution();
    BenchmarkResult BenchmarkStreamSynchronization();
    BenchmarkResult BenchmarkTriadicIntegration();
    
    // Memory benchmark implementations
    BenchmarkResult BenchmarkEpisodicMemoryStore();
    BenchmarkResult BenchmarkEpisodicMemoryRetrieve();
    BenchmarkResult BenchmarkHypergraphQuery();
    BenchmarkResult BenchmarkMemoryConsolidation();
    
    // Temporal benchmark implementations
    BenchmarkResult BenchmarkEventRecording();
    BenchmarkResult BenchmarkCausalInference();
    BenchmarkResult BenchmarkTemporalQuery();
    
    // Embodied benchmark implementations
    BenchmarkResult BenchmarkSensoryIntegration();
    BenchmarkResult BenchmarkMotorPlanning();
    BenchmarkResult BenchmarkAffordanceDetection();
    
    // Active inference benchmark implementations
    BenchmarkResult BenchmarkBeliefUpdate();
    BenchmarkResult BenchmarkPolicySelection();
    BenchmarkResult BenchmarkFreeEnergyComputation();
    
    // Integration benchmark implementations
    BenchmarkResult BenchmarkFullCognitiveLoop();
    BenchmarkResult BenchmarkCrossComponentLatency();
    BenchmarkResult BenchmarkEndToEndThroughput();
};

// ============================================================================
// Target Performance Thresholds (default values)
// ============================================================================

namespace Targets {
    // Latency targets (microseconds)
    constexpr double ReservoirForwardLatency = 1000.0;      // 1ms
    constexpr double CognitiveStepLatency = 500.0;          // 0.5ms
    constexpr double MemoryStoreLatency = 100.0;            // 0.1ms
    constexpr double MemoryRetrieveLatency = 200.0;         // 0.2ms
    constexpr double EventRecordLatency = 100.0;            // 0.1ms
    constexpr double CausalInferenceLatency = 10000.0;      // 10ms
    constexpr double SensoryIntegrationLatency = 500.0;     // 0.5ms
    constexpr double BeliefUpdateLatency = 1000.0;          // 1ms
    constexpr double FullCycleLatency = 16667.0;            // 16.67ms (60fps budget)
    
    // Throughput targets (ops/sec)
    constexpr double ReservoirThroughput = 1000.0;          // 1000 forward passes/sec
    constexpr double MemoryStoreThroughput = 10000.0;       // 10000 stores/sec
    constexpr double EventRecordThroughput = 10000.0;       // 10000 events/sec
    
    // Memory targets (bytes)
    constexpr size_t ReservoirMemoryPerNode = 256;          // 256 bytes/node
    constexpr size_t EventMemoryPerEvent = 128;             // 128 bytes/event
    constexpr size_t MemoryTraceSize = 512;                 // 512 bytes/trace
}

} // namespace Benchmarks
} // namespace DeepTreeEcho
