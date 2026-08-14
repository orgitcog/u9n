// CognitiveMemoryManager.h
// Performance optimization and memory management for Deep Tree Echo cognitive systems
// Implements pooled memory allocation, caching, and async processing

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/AsyncWork.h"
#include "CognitiveMemoryManager.generated.h"

/**
 * Memory pool type
 */
UENUM(BlueprintType)
enum class EMemoryPoolType : uint8
{
    StateVector     UMETA(DisplayName = "State Vector Pool"),
    PerceptData     UMETA(DisplayName = "Percept Data Pool"),
    EmotionLayer    UMETA(DisplayName = "Emotion Layer Pool"),
    CouplingState   UMETA(DisplayName = "Coupling State Pool"),
    StreamBuffer    UMETA(DisplayName = "Stream Buffer Pool"),
    BindingData     UMETA(DisplayName = "Binding Data Pool"),
    TransitionData  UMETA(DisplayName = "Transition Data Pool"),
    General         UMETA(DisplayName = "General Purpose Pool")
};

/**
 * Cache entry type
 */
UENUM(BlueprintType)
enum class ECacheEntryType : uint8
{
    ComputedState   UMETA(DisplayName = "Computed State"),
    BlendedEmotion  UMETA(DisplayName = "Blended Emotion"),
    ProcessedInput  UMETA(DisplayName = "Processed Input"),
    PredictionModel UMETA(DisplayName = "Prediction Model"),
    ExpressionRule  UMETA(DisplayName = "Expression Rule"),
    CouplingResult  UMETA(DisplayName = "Coupling Result")
};

/**
 * Memory block header
 */
USTRUCT()
struct FMemoryBlockHeader
{
    GENERATED_BODY()

    /** Block size in bytes */
    UPROPERTY()
    int32 BlockSize = 0;

    /** Pool type */
    UPROPERTY()
    EMemoryPoolType PoolType = EMemoryPoolType::General;

    /** Is block in use */
    UPROPERTY()
    bool bInUse = false;

    /** Allocation timestamp */
    UPROPERTY()
    float AllocationTime = 0.0f;

    /** Last access timestamp */
    UPROPERTY()
    float LastAccessTime = 0.0f;

    /** Reference count */
    UPROPERTY()
    int32 RefCount = 0;
};

/**
 * Memory pool statistics
 */
USTRUCT(BlueprintType)
struct FMemoryPoolStats
{
    GENERATED_BODY()

    /** Pool type */
    UPROPERTY(BlueprintReadOnly)
    EMemoryPoolType PoolType = EMemoryPoolType::General;

    /** Total blocks in pool */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalBlocks = 0;

    /** Blocks currently in use */
    UPROPERTY(BlueprintReadOnly)
    int32 BlocksInUse = 0;

    /** Total memory allocated (bytes) */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalMemory = 0;

    /** Memory in use (bytes) */
    UPROPERTY(BlueprintReadOnly)
    int64 MemoryInUse = 0;

    /** Peak memory usage (bytes) */
    UPROPERTY(BlueprintReadOnly)
    int64 PeakMemoryUsage = 0;

    /** Allocation count */
    UPROPERTY(BlueprintReadOnly)
    int64 AllocationCount = 0;

    /** Deallocation count */
    UPROPERTY(BlueprintReadOnly)
    int64 DeallocationCount = 0;

    /** Cache hit rate */
    UPROPERTY(BlueprintReadOnly)
    float CacheHitRate = 0.0f;
};

/**
 * Cache entry
 */
USTRUCT()
struct FCacheEntry
{
    GENERATED_BODY()

    /** Entry key */
    UPROPERTY()
    FString Key;

    /** Entry type */
    UPROPERTY()
    ECacheEntryType EntryType = ECacheEntryType::ComputedState;

    /** Cached data (serialized) */
    UPROPERTY()
    TArray<uint8> Data;

    /** Creation timestamp */
    UPROPERTY()
    float CreationTime = 0.0f;

    /** Last access timestamp */
    UPROPERTY()
    float LastAccessTime = 0.0f;

    /** Access count */
    UPROPERTY()
    int32 AccessCount = 0;

    /** Time-to-live (seconds, 0 = infinite) */
    UPROPERTY()
    float TTL = 0.0f;

    /** Priority (higher = less likely to evict) */
    UPROPERTY()
    int32 Priority = 0;
};

/**
 * Async task result
 */
USTRUCT(BlueprintType)
struct FAsyncTaskResult
{
    GENERATED_BODY()

    /** Task ID */
    UPROPERTY(BlueprintReadOnly)
    int32 TaskID = 0;

    /** Is task complete */
    UPROPERTY(BlueprintReadOnly)
    bool bIsComplete = false;

    /** Was task successful */
    UPROPERTY(BlueprintReadOnly)
    bool bWasSuccessful = false;

    /** Error message (if failed) */
    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;

    /** Execution time (seconds) */
    UPROPERTY(BlueprintReadOnly)
    float ExecutionTime = 0.0f;

    /** Result data (serialized) */
    UPROPERTY()
    TArray<uint8> ResultData;
};

/**
 * Performance metrics
 */
USTRUCT(BlueprintType)
struct FCognitivePerformanceMetrics
{
    GENERATED_BODY()

    /** Average frame time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float AverageFrameTime = 0.0f;

    /** Peak frame time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float PeakFrameTime = 0.0f;

    /** Cognitive processing time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float CognitiveProcessingTime = 0.0f;

    /** Memory allocation time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float MemoryAllocationTime = 0.0f;

    /** Cache lookup time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float CacheLookupTime = 0.0f;

    /** Async task queue depth */
    UPROPERTY(BlueprintReadOnly)
    int32 AsyncQueueDepth = 0;

    /** Total memory usage (MB) */
    UPROPERTY(BlueprintReadOnly)
    float TotalMemoryMB = 0.0f;

    /** GC pressure indicator (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float GCPressure = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryPoolExhausted, EMemoryPoolType, PoolType, int32, RequestedSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCacheEviction, FString, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsyncTaskComplete, int32, TaskID, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerformanceWarning, FString, WarningMessage);

/**
 * Cognitive Memory Manager
 * 
 * Provides optimized memory management and caching for Deep Tree Echo cognitive systems.
 * 
 * Key features:
 * - Pooled memory allocation for common data structures
 * - LRU cache with configurable eviction policies
 * - Async task management for heavy computations
 * - Performance monitoring and optimization
 * - Thread-safe operations
 */
UCLASS(BlueprintType)
class DEEPTREEECHO_API UCognitiveMemoryManager : public UObject
{
    GENERATED_BODY()

public:
    UCognitiveMemoryManager();
    virtual ~UCognitiveMemoryManager();

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Default state vector size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    int32 DefaultStateVectorSize = 64;

    /** Maximum cache size (MB) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float MaxCacheSizeMB = 64.0f;

    /** Cache eviction threshold (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float CacheEvictionThreshold = 0.9f;

    /** Enable async processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    bool bEnableAsyncProcessing = true;

    /** Maximum async tasks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    int32 MaxAsyncTasks = 8;

    /** Enable performance monitoring */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    bool bEnablePerformanceMonitoring = true;

    /** Performance sample window (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float PerformanceSampleWindow = 1.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnMemoryPoolExhausted OnPoolExhausted;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnCacheEviction OnCacheEviction;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnAsyncTaskComplete OnAsyncTaskComplete;

    UPROPERTY(BlueprintAssignable, Category = "Memory|Events")
    FOnPerformanceWarning OnPerformanceWarning;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the memory manager */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void Initialize();

    /** Shutdown and cleanup */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void Shutdown();

    /** Reset all pools and caches */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void Reset();

    // ========================================
    // PUBLIC API - MEMORY POOLS
    // ========================================

    /** Allocate from pool */
    UFUNCTION(BlueprintCallable, Category = "Memory|Pools")
    int32 AllocateFromPool(EMemoryPoolType PoolType, int32 Size);

    /** Return to pool */
    UFUNCTION(BlueprintCallable, Category = "Memory|Pools")
    void ReturnToPool(EMemoryPoolType PoolType, int32 BlockID);

    /** Get pool statistics */
    UFUNCTION(BlueprintPure, Category = "Memory|Pools")
    FMemoryPoolStats GetPoolStats(EMemoryPoolType PoolType) const;

    /** Get all pool statistics */
    UFUNCTION(BlueprintPure, Category = "Memory|Pools")
    TArray<FMemoryPoolStats> GetAllPoolStats() const;

    /** Pre-allocate pool blocks */
    UFUNCTION(BlueprintCallable, Category = "Memory|Pools")
    void PreAllocatePool(EMemoryPoolType PoolType, int32 BlockCount, int32 BlockSize);

    /** Compact pool (defragment) */
    UFUNCTION(BlueprintCallable, Category = "Memory|Pools")
    void CompactPool(EMemoryPoolType PoolType);

    // ========================================
    // PUBLIC API - STATE VECTOR OPERATIONS
    // ========================================

    /** Allocate state vector */
    UFUNCTION(BlueprintCallable, Category = "Memory|StateVectors")
    int32 AllocateStateVector(int32 Size = 0);

    /** Free state vector */
    UFUNCTION(BlueprintCallable, Category = "Memory|StateVectors")
    void FreeStateVector(int32 VectorID);

    /** Get state vector data */
    UFUNCTION(BlueprintPure, Category = "Memory|StateVectors")
    TArray<float> GetStateVector(int32 VectorID) const;

    /** Set state vector data */
    UFUNCTION(BlueprintCallable, Category = "Memory|StateVectors")
    void SetStateVector(int32 VectorID, const TArray<float>& Data);

    /** Copy state vector */
    UFUNCTION(BlueprintCallable, Category = "Memory|StateVectors")
    int32 CopyStateVector(int32 SourceID);

    // ========================================
    // PUBLIC API - CACHING
    // ========================================

    /** Store in cache */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cache")
    void CacheStore(const FString& Key, ECacheEntryType EntryType, const TArray<uint8>& Data, float TTL = 0.0f, int32 Priority = 0);

    /** Retrieve from cache */
    UFUNCTION(BlueprintPure, Category = "Memory|Cache")
    bool CacheRetrieve(const FString& Key, TArray<uint8>& OutData) const;

    /** Check if key exists in cache */
    UFUNCTION(BlueprintPure, Category = "Memory|Cache")
    bool CacheContains(const FString& Key) const;

    /** Remove from cache */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cache")
    void CacheRemove(const FString& Key);

    /** Clear cache by type */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cache")
    void CacheClearByType(ECacheEntryType EntryType);

    /** Clear entire cache */
    UFUNCTION(BlueprintCallable, Category = "Memory|Cache")
    void CacheClearAll();

    /** Get cache size (bytes) */
    UFUNCTION(BlueprintPure, Category = "Memory|Cache")
    int64 GetCacheSize() const;

    /** Get cache hit rate */
    UFUNCTION(BlueprintPure, Category = "Memory|Cache")
    float GetCacheHitRate() const;

    // ========================================
    // PUBLIC API - ASYNC PROCESSING
    // ========================================

    /** Queue async task */
    UFUNCTION(BlueprintCallable, Category = "Memory|Async")
    int32 QueueAsyncTask(const FString& TaskName, const TArray<uint8>& InputData);

    /** Check if async task is complete */
    UFUNCTION(BlueprintPure, Category = "Memory|Async")
    bool IsAsyncTaskComplete(int32 TaskID) const;

    /** Get async task result */
    UFUNCTION(BlueprintPure, Category = "Memory|Async")
    FAsyncTaskResult GetAsyncTaskResult(int32 TaskID) const;

    /** Cancel async task */
    UFUNCTION(BlueprintCallable, Category = "Memory|Async")
    void CancelAsyncTask(int32 TaskID);

    /** Get async queue depth */
    UFUNCTION(BlueprintPure, Category = "Memory|Async")
    int32 GetAsyncQueueDepth() const;

    /** Wait for all async tasks */
    UFUNCTION(BlueprintCallable, Category = "Memory|Async")
    void WaitForAllAsyncTasks();

    // ========================================
    // PUBLIC API - PERFORMANCE MONITORING
    // ========================================

    /** Get performance metrics */
    UFUNCTION(BlueprintPure, Category = "Memory|Performance")
    FCognitivePerformanceMetrics GetPerformanceMetrics() const;

    /** Begin performance sample */
    UFUNCTION(BlueprintCallable, Category = "Memory|Performance")
    void BeginPerformanceSample(const FString& SampleName);

    /** End performance sample */
    UFUNCTION(BlueprintCallable, Category = "Memory|Performance")
    float EndPerformanceSample(const FString& SampleName);

    /** Get sample average time (ms) */
    UFUNCTION(BlueprintPure, Category = "Memory|Performance")
    float GetSampleAverageTime(const FString& SampleName) const;

    /** Reset performance metrics */
    UFUNCTION(BlueprintCallable, Category = "Memory|Performance")
    void ResetPerformanceMetrics();

    // ========================================
    // PUBLIC API - OPTIMIZATION
    // ========================================

    /** Run garbage collection */
    UFUNCTION(BlueprintCallable, Category = "Memory|Optimization")
    void RunGarbageCollection();

    /** Optimize memory layout */
    UFUNCTION(BlueprintCallable, Category = "Memory|Optimization")
    void OptimizeMemoryLayout();

    /** Trim unused memory */
    UFUNCTION(BlueprintCallable, Category = "Memory|Optimization")
    void TrimUnusedMemory();

    /** Get optimization suggestions */
    UFUNCTION(BlueprintPure, Category = "Memory|Optimization")
    TArray<FString> GetOptimizationSuggestions() const;

    // ========================================
    // SINGLETON ACCESS
    // ========================================

    /** Get the global memory manager instance */
    UFUNCTION(BlueprintPure, Category = "Memory", meta = (WorldContext = "WorldContextObject"))
    static UCognitiveMemoryManager* GetInstance(UObject* WorldContextObject);

protected:
    /** Is initialized */
    bool bIsInitialized = false;

    /** Memory pools */
    TMap<EMemoryPoolType, TArray<FMemoryBlockHeader>> MemoryPools;

    /** Pool data storage */
    TMap<EMemoryPoolType, TArray<TArray<uint8>>> PoolData;

    /** State vector storage */
    TMap<int32, TArray<float>> StateVectors;

    /** Next state vector ID */
    int32 NextStateVectorID = 1;

    /** Cache entries */
    TMap<FString, FCacheEntry> CacheEntries;

    /** Cache statistics */
    int64 CacheHits = 0;
    int64 CacheMisses = 0;

    /** Async task results */
    TMap<int32, FAsyncTaskResult> AsyncTaskResults;

    /** Next async task ID */
    int32 NextAsyncTaskID = 1;

    /** Active async task count */
    FThreadSafeCounter ActiveAsyncTasks;

    /** Performance samples */
    TMap<FString, TArray<float>> PerformanceSamples;

    /** Sample start times */
    TMap<FString, double> SampleStartTimes;

    /** Performance metrics */
    FCognitivePerformanceMetrics CurrentMetrics;

    /** Critical section for thread safety */
    mutable FCriticalSection CriticalSection;

    // Internal methods
    void InitializePools();
    void EvictCacheEntries();
    void UpdatePerformanceMetrics();
    int32 FindFreeBlock(EMemoryPoolType PoolType, int32 Size);
    void ExpandPool(EMemoryPoolType PoolType, int32 AdditionalBlocks, int32 BlockSize);
};

/**
 * Scoped performance sample helper
 */
class FScopedPerformanceSample
{
public:
    FScopedPerformanceSample(UCognitiveMemoryManager* Manager, const FString& SampleName)
        : MemoryManager(Manager), Name(SampleName)
    {
        if (MemoryManager)
        {
            MemoryManager->BeginPerformanceSample(Name);
        }
    }

    ~FScopedPerformanceSample()
    {
        if (MemoryManager)
        {
            MemoryManager->EndPerformanceSample(Name);
        }
    }

private:
    UCognitiveMemoryManager* MemoryManager;
    FString Name;
};

#define SCOPED_PERF_SAMPLE(Manager, Name) FScopedPerformanceSample _PerfSample_##__LINE__(Manager, Name)
