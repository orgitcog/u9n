// CognitiveMemoryManager.cpp
// Implementation of performance optimization and memory management

#include "CognitiveMemoryManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"

// Static instance
static UCognitiveMemoryManager* GCognitiveMemoryManager = nullptr;

UCognitiveMemoryManager::UCognitiveMemoryManager()
{
}

UCognitiveMemoryManager::~UCognitiveMemoryManager()
{
    Shutdown();
}

void UCognitiveMemoryManager::Initialize()
{
    FScopeLock Lock(&CriticalSection);

    if (bIsInitialized)
    {
        return;
    }

    InitializePools();
    bIsInitialized = true;
}

void UCognitiveMemoryManager::Shutdown()
{
    FScopeLock Lock(&CriticalSection);

    if (!bIsInitialized)
    {
        return;
    }

    // Wait for async tasks
    WaitForAllAsyncTasks();

    // Clear all data
    MemoryPools.Empty();
    PoolData.Empty();
    StateVectors.Empty();
    CacheEntries.Empty();
    AsyncTaskResults.Empty();
    PerformanceSamples.Empty();
    SampleStartTimes.Empty();

    bIsInitialized = false;
}

void UCognitiveMemoryManager::Reset()
{
    Shutdown();
    Initialize();
}

void UCognitiveMemoryManager::InitializePools()
{
    // Initialize all pool types with default sizes
    TArray<EMemoryPoolType> PoolTypes = {
        EMemoryPoolType::StateVector,
        EMemoryPoolType::PerceptData,
        EMemoryPoolType::EmotionLayer,
        EMemoryPoolType::CouplingState,
        EMemoryPoolType::StreamBuffer,
        EMemoryPoolType::BindingData,
        EMemoryPoolType::TransitionData,
        EMemoryPoolType::General
    };

    for (EMemoryPoolType PoolType : PoolTypes)
    {
        MemoryPools.Add(PoolType, TArray<FMemoryBlockHeader>());
        PoolData.Add(PoolType, TArray<TArray<uint8>>());
    }

    // Pre-allocate common pools
    PreAllocatePool(EMemoryPoolType::StateVector, 32, DefaultStateVectorSize * sizeof(float));
    PreAllocatePool(EMemoryPoolType::StreamBuffer, 16, 1024);
    PreAllocatePool(EMemoryPoolType::General, 64, 256);
}

int32 UCognitiveMemoryManager::AllocateFromPool(EMemoryPoolType PoolType, int32 Size)
{
    FScopeLock Lock(&CriticalSection);

    if (!bIsInitialized)
    {
        Initialize();
    }

    // Find free block
    int32 BlockID = FindFreeBlock(PoolType, Size);

    if (BlockID < 0)
    {
        // Expand pool
        ExpandPool(PoolType, 8, Size);
        BlockID = FindFreeBlock(PoolType, Size);

        if (BlockID < 0)
        {
            OnPoolExhausted.Broadcast(PoolType, Size);
            return -1;
        }
    }

    // Mark block as in use
    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];
    Headers[BlockID].bInUse = true;
    Headers[BlockID].AllocationTime = FPlatformTime::Seconds();
    Headers[BlockID].LastAccessTime = Headers[BlockID].AllocationTime;
    Headers[BlockID].RefCount = 1;

    return BlockID;
}

void UCognitiveMemoryManager::ReturnToPool(EMemoryPoolType PoolType, int32 BlockID)
{
    FScopeLock Lock(&CriticalSection);

    if (!MemoryPools.Contains(PoolType))
    {
        return;
    }

    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];

    if (BlockID >= 0 && BlockID < Headers.Num())
    {
        Headers[BlockID].RefCount--;

        if (Headers[BlockID].RefCount <= 0)
        {
            Headers[BlockID].bInUse = false;
            Headers[BlockID].RefCount = 0;

            // Clear data
            TArray<TArray<uint8>>& Data = PoolData[PoolType];
            if (BlockID < Data.Num())
            {
                Data[BlockID].Empty();
            }
        }
    }
}

FMemoryPoolStats UCognitiveMemoryManager::GetPoolStats(EMemoryPoolType PoolType) const
{
    FScopeLock Lock(&CriticalSection);

    FMemoryPoolStats Stats;
    Stats.PoolType = PoolType;

    if (!MemoryPools.Contains(PoolType))
    {
        return Stats;
    }

    const TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];

    Stats.TotalBlocks = Headers.Num();

    for (const FMemoryBlockHeader& Header : Headers)
    {
        Stats.TotalMemory += Header.BlockSize;

        if (Header.bInUse)
        {
            Stats.BlocksInUse++;
            Stats.MemoryInUse += Header.BlockSize;
        }
    }

    Stats.PeakMemoryUsage = FMath::Max(Stats.PeakMemoryUsage, Stats.MemoryInUse);

    // Compute cache hit rate for this pool
    if (CacheHits + CacheMisses > 0)
    {
        Stats.CacheHitRate = (float)CacheHits / (float)(CacheHits + CacheMisses);
    }

    return Stats;
}

TArray<FMemoryPoolStats> UCognitiveMemoryManager::GetAllPoolStats() const
{
    TArray<FMemoryPoolStats> AllStats;

    for (const auto& Pair : MemoryPools)
    {
        AllStats.Add(GetPoolStats(Pair.Key));
    }

    return AllStats;
}

void UCognitiveMemoryManager::PreAllocatePool(EMemoryPoolType PoolType, int32 BlockCount, int32 BlockSize)
{
    FScopeLock Lock(&CriticalSection);

    if (!MemoryPools.Contains(PoolType))
    {
        MemoryPools.Add(PoolType, TArray<FMemoryBlockHeader>());
        PoolData.Add(PoolType, TArray<TArray<uint8>>());
    }

    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];
    TArray<TArray<uint8>>& Data = PoolData[PoolType];

    for (int32 i = 0; i < BlockCount; ++i)
    {
        FMemoryBlockHeader Header;
        Header.BlockSize = BlockSize;
        Header.PoolType = PoolType;
        Header.bInUse = false;
        Header.RefCount = 0;

        Headers.Add(Header);

        TArray<uint8> BlockData;
        BlockData.SetNum(BlockSize);
        Data.Add(BlockData);
    }
}

void UCognitiveMemoryManager::CompactPool(EMemoryPoolType PoolType)
{
    FScopeLock Lock(&CriticalSection);

    if (!MemoryPools.Contains(PoolType))
    {
        return;
    }

    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];
    TArray<TArray<uint8>>& Data = PoolData[PoolType];

    // Remove unused blocks from the end
    while (Headers.Num() > 0 && !Headers.Last().bInUse)
    {
        Headers.Pop();
        Data.Pop();
    }
}

int32 UCognitiveMemoryManager::AllocateStateVector(int32 Size)
{
    FScopeLock Lock(&CriticalSection);

    int32 VectorSize = (Size > 0) ? Size : DefaultStateVectorSize;
    int32 VectorID = NextStateVectorID++;

    TArray<float> Vector;
    Vector.SetNumZeroed(VectorSize);

    StateVectors.Add(VectorID, Vector);

    return VectorID;
}

void UCognitiveMemoryManager::FreeStateVector(int32 VectorID)
{
    FScopeLock Lock(&CriticalSection);
    StateVectors.Remove(VectorID);
}

TArray<float> UCognitiveMemoryManager::GetStateVector(int32 VectorID) const
{
    FScopeLock Lock(&CriticalSection);

    if (StateVectors.Contains(VectorID))
    {
        return StateVectors[VectorID];
    }

    return TArray<float>();
}

void UCognitiveMemoryManager::SetStateVector(int32 VectorID, const TArray<float>& Data)
{
    FScopeLock Lock(&CriticalSection);

    if (StateVectors.Contains(VectorID))
    {
        StateVectors[VectorID] = Data;
    }
}

int32 UCognitiveMemoryManager::CopyStateVector(int32 SourceID)
{
    FScopeLock Lock(&CriticalSection);

    if (!StateVectors.Contains(SourceID))
    {
        return -1;
    }

    int32 NewID = NextStateVectorID++;
    StateVectors.Add(NewID, StateVectors[SourceID]);

    return NewID;
}

void UCognitiveMemoryManager::CacheStore(const FString& Key, ECacheEntryType EntryType, const TArray<uint8>& Data, float TTL, int32 Priority)
{
    FScopeLock Lock(&CriticalSection);

    // Check cache size and evict if necessary
    int64 CurrentSize = GetCacheSize();
    int64 MaxSize = (int64)(MaxCacheSizeMB * 1024 * 1024);

    if (CurrentSize + Data.Num() > MaxSize * CacheEvictionThreshold)
    {
        EvictCacheEntries();
    }

    FCacheEntry Entry;
    Entry.Key = Key;
    Entry.EntryType = EntryType;
    Entry.Data = Data;
    Entry.CreationTime = FPlatformTime::Seconds();
    Entry.LastAccessTime = Entry.CreationTime;
    Entry.AccessCount = 0;
    Entry.TTL = TTL;
    Entry.Priority = Priority;

    CacheEntries.Add(Key, Entry);
}

bool UCognitiveMemoryManager::CacheRetrieve(const FString& Key, TArray<uint8>& OutData) const
{
    FScopeLock Lock(&CriticalSection);

    if (CacheEntries.Contains(Key))
    {
        FCacheEntry& Entry = const_cast<FCacheEntry&>(CacheEntries[Key]);

        // Check TTL
        if (Entry.TTL > 0.0f)
        {
            float Age = FPlatformTime::Seconds() - Entry.CreationTime;
            if (Age > Entry.TTL)
            {
                const_cast<UCognitiveMemoryManager*>(this)->CacheMisses++;
                return false;
            }
        }

        Entry.LastAccessTime = FPlatformTime::Seconds();
        Entry.AccessCount++;
        OutData = Entry.Data;

        const_cast<UCognitiveMemoryManager*>(this)->CacheHits++;
        return true;
    }

    const_cast<UCognitiveMemoryManager*>(this)->CacheMisses++;
    return false;
}

bool UCognitiveMemoryManager::CacheContains(const FString& Key) const
{
    FScopeLock Lock(&CriticalSection);
    return CacheEntries.Contains(Key);
}

void UCognitiveMemoryManager::CacheRemove(const FString& Key)
{
    FScopeLock Lock(&CriticalSection);

    if (CacheEntries.Contains(Key))
    {
        OnCacheEviction.Broadcast(Key);
        CacheEntries.Remove(Key);
    }
}

void UCognitiveMemoryManager::CacheClearByType(ECacheEntryType EntryType)
{
    FScopeLock Lock(&CriticalSection);

    TArray<FString> KeysToRemove;

    for (const auto& Pair : CacheEntries)
    {
        if (Pair.Value.EntryType == EntryType)
        {
            KeysToRemove.Add(Pair.Key);
        }
    }

    for (const FString& Key : KeysToRemove)
    {
        OnCacheEviction.Broadcast(Key);
        CacheEntries.Remove(Key);
    }
}

void UCognitiveMemoryManager::CacheClearAll()
{
    FScopeLock Lock(&CriticalSection);

    for (const auto& Pair : CacheEntries)
    {
        OnCacheEviction.Broadcast(Pair.Key);
    }

    CacheEntries.Empty();
    CacheHits = 0;
    CacheMisses = 0;
}

int64 UCognitiveMemoryManager::GetCacheSize() const
{
    FScopeLock Lock(&CriticalSection);

    int64 TotalSize = 0;

    for (const auto& Pair : CacheEntries)
    {
        TotalSize += Pair.Value.Data.Num();
    }

    return TotalSize;
}

float UCognitiveMemoryManager::GetCacheHitRate() const
{
    FScopeLock Lock(&CriticalSection);

    if (CacheHits + CacheMisses == 0)
    {
        return 0.0f;
    }

    return (float)CacheHits / (float)(CacheHits + CacheMisses);
}

void UCognitiveMemoryManager::EvictCacheEntries()
{
    // LRU eviction
    TArray<TPair<FString, float>> EntriesByAccess;

    for (const auto& Pair : CacheEntries)
    {
        // Lower score = more likely to evict
        float Score = Pair.Value.LastAccessTime + Pair.Value.Priority * 100.0f;
        EntriesByAccess.Add(TPair<FString, float>(Pair.Key, Score));
    }

    // Sort by score (ascending)
    EntriesByAccess.Sort([](const TPair<FString, float>& A, const TPair<FString, float>& B) {
        return A.Value < B.Value;
    });

    // Evict bottom 25%
    int32 EvictCount = EntriesByAccess.Num() / 4;
    EvictCount = FMath::Max(EvictCount, 1);

    for (int32 i = 0; i < EvictCount && i < EntriesByAccess.Num(); ++i)
    {
        CacheRemove(EntriesByAccess[i].Key);
    }
}

int32 UCognitiveMemoryManager::QueueAsyncTask(const FString& TaskName, const TArray<uint8>& InputData)
{
    FScopeLock Lock(&CriticalSection);

    if (!bEnableAsyncProcessing)
    {
        return -1;
    }

    if (ActiveAsyncTasks.GetValue() >= MaxAsyncTasks)
    {
        return -1;
    }

    int32 TaskID = NextAsyncTaskID++;

    FAsyncTaskResult Result;
    Result.TaskID = TaskID;
    Result.bIsComplete = false;
    Result.bWasSuccessful = false;

    AsyncTaskResults.Add(TaskID, Result);
    ActiveAsyncTasks.Increment();

    // Queue the async work
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, TaskID, TaskName, InputData]()
    {
        double StartTime = FPlatformTime::Seconds();

        // Simulate processing (in real implementation, this would do actual work)
        FPlatformProcess::Sleep(0.01f);

        double EndTime = FPlatformTime::Seconds();

        // Update result
        {
            FScopeLock InnerLock(&CriticalSection);

            if (AsyncTaskResults.Contains(TaskID))
            {
                AsyncTaskResults[TaskID].bIsComplete = true;
                AsyncTaskResults[TaskID].bWasSuccessful = true;
                AsyncTaskResults[TaskID].ExecutionTime = EndTime - StartTime;
                AsyncTaskResults[TaskID].ResultData = InputData; // Echo back for now
            }
        }

        ActiveAsyncTasks.Decrement();

        // Broadcast completion on game thread
        AsyncTask(ENamedThreads::GameThread, [this, TaskID]()
        {
            OnAsyncTaskComplete.Broadcast(TaskID, true);
        });
    });

    return TaskID;
}

bool UCognitiveMemoryManager::IsAsyncTaskComplete(int32 TaskID) const
{
    FScopeLock Lock(&CriticalSection);

    if (AsyncTaskResults.Contains(TaskID))
    {
        return AsyncTaskResults[TaskID].bIsComplete;
    }

    return true; // Non-existent tasks are "complete"
}

FAsyncTaskResult UCognitiveMemoryManager::GetAsyncTaskResult(int32 TaskID) const
{
    FScopeLock Lock(&CriticalSection);

    if (AsyncTaskResults.Contains(TaskID))
    {
        return AsyncTaskResults[TaskID];
    }

    FAsyncTaskResult EmptyResult;
    EmptyResult.TaskID = TaskID;
    EmptyResult.bIsComplete = true;
    EmptyResult.bWasSuccessful = false;
    EmptyResult.ErrorMessage = TEXT("Task not found");

    return EmptyResult;
}

void UCognitiveMemoryManager::CancelAsyncTask(int32 TaskID)
{
    FScopeLock Lock(&CriticalSection);

    if (AsyncTaskResults.Contains(TaskID))
    {
        AsyncTaskResults[TaskID].bIsComplete = true;
        AsyncTaskResults[TaskID].bWasSuccessful = false;
        AsyncTaskResults[TaskID].ErrorMessage = TEXT("Cancelled");
    }
}

int32 UCognitiveMemoryManager::GetAsyncQueueDepth() const
{
    return ActiveAsyncTasks.GetValue();
}

void UCognitiveMemoryManager::WaitForAllAsyncTasks()
{
    while (ActiveAsyncTasks.GetValue() > 0)
    {
        FPlatformProcess::Sleep(0.01f);
    }
}

FCognitivePerformanceMetrics UCognitiveMemoryManager::GetPerformanceMetrics() const
{
    FScopeLock Lock(&CriticalSection);
    return CurrentMetrics;
}

void UCognitiveMemoryManager::BeginPerformanceSample(const FString& SampleName)
{
    if (!bEnablePerformanceMonitoring)
    {
        return;
    }

    FScopeLock Lock(&CriticalSection);
    SampleStartTimes.Add(SampleName, FPlatformTime::Seconds());
}

float UCognitiveMemoryManager::EndPerformanceSample(const FString& SampleName)
{
    if (!bEnablePerformanceMonitoring)
    {
        return 0.0f;
    }

    FScopeLock Lock(&CriticalSection);

    if (!SampleStartTimes.Contains(SampleName))
    {
        return 0.0f;
    }

    double StartTime = SampleStartTimes[SampleName];
    double EndTime = FPlatformTime::Seconds();
    float ElapsedMs = (EndTime - StartTime) * 1000.0f;

    SampleStartTimes.Remove(SampleName);

    // Store sample
    if (!PerformanceSamples.Contains(SampleName))
    {
        PerformanceSamples.Add(SampleName, TArray<float>());
    }

    TArray<float>& Samples = PerformanceSamples[SampleName];
    Samples.Add(ElapsedMs);

    // Keep only recent samples
    int32 MaxSamples = FMath::Max(1, (int32)(PerformanceSampleWindow * 60.0f)); // Assume 60 FPS
    while (Samples.Num() > MaxSamples)
    {
        Samples.RemoveAt(0);
    }

    return ElapsedMs;
}

float UCognitiveMemoryManager::GetSampleAverageTime(const FString& SampleName) const
{
    FScopeLock Lock(&CriticalSection);

    if (!PerformanceSamples.Contains(SampleName))
    {
        return 0.0f;
    }

    const TArray<float>& Samples = PerformanceSamples[SampleName];

    if (Samples.Num() == 0)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (float Sample : Samples)
    {
        Sum += Sample;
    }

    return Sum / Samples.Num();
}

void UCognitiveMemoryManager::ResetPerformanceMetrics()
{
    FScopeLock Lock(&CriticalSection);

    PerformanceSamples.Empty();
    SampleStartTimes.Empty();
    CurrentMetrics = FCognitivePerformanceMetrics();
}

void UCognitiveMemoryManager::RunGarbageCollection()
{
    FScopeLock Lock(&CriticalSection);

    // Compact all pools
    for (const auto& Pair : MemoryPools)
    {
        CompactPool(Pair.Key);
    }

    // Clear expired cache entries
    TArray<FString> ExpiredKeys;
    float CurrentTime = FPlatformTime::Seconds();

    for (const auto& Pair : CacheEntries)
    {
        if (Pair.Value.TTL > 0.0f)
        {
            float Age = CurrentTime - Pair.Value.CreationTime;
            if (Age > Pair.Value.TTL)
            {
                ExpiredKeys.Add(Pair.Key);
            }
        }
    }

    for (const FString& Key : ExpiredKeys)
    {
        CacheRemove(Key);
    }

    // Clean up completed async tasks
    TArray<int32> CompletedTasks;
    for (const auto& Pair : AsyncTaskResults)
    {
        if (Pair.Value.bIsComplete)
        {
            CompletedTasks.Add(Pair.Key);
        }
    }

    for (int32 TaskID : CompletedTasks)
    {
        AsyncTaskResults.Remove(TaskID);
    }
}

void UCognitiveMemoryManager::OptimizeMemoryLayout()
{
    FScopeLock Lock(&CriticalSection);

    // Compact all pools
    for (const auto& Pair : MemoryPools)
    {
        CompactPool(Pair.Key);
    }

    // Shrink state vectors to fit
    for (auto& Pair : StateVectors)
    {
        Pair.Value.Shrink();
    }
}

void UCognitiveMemoryManager::TrimUnusedMemory()
{
    FScopeLock Lock(&CriticalSection);

    // Remove unused pool blocks
    for (auto& Pair : MemoryPools)
    {
        TArray<FMemoryBlockHeader>& Headers = Pair.Value;
        TArray<TArray<uint8>>& Data = PoolData[Pair.Key];

        for (int32 i = Headers.Num() - 1; i >= 0; --i)
        {
            if (!Headers[i].bInUse)
            {
                float Age = FPlatformTime::Seconds() - Headers[i].LastAccessTime;
                if (Age > 60.0f) // Unused for 60 seconds
                {
                    Headers.RemoveAt(i);
                    if (i < Data.Num())
                    {
                        Data.RemoveAt(i);
                    }
                }
            }
        }
    }
}

TArray<FString> UCognitiveMemoryManager::GetOptimizationSuggestions() const
{
    FScopeLock Lock(&CriticalSection);

    TArray<FString> Suggestions;

    // Check cache hit rate
    float HitRate = GetCacheHitRate();
    if (HitRate < 0.5f && CacheHits + CacheMisses > 100)
    {
        Suggestions.Add(FString::Printf(TEXT("Low cache hit rate (%.1f%%). Consider increasing cache size or adjusting TTL values."), HitRate * 100.0f));
    }

    // Check pool utilization
    for (const auto& Pair : MemoryPools)
    {
        FMemoryPoolStats Stats = GetPoolStats(Pair.Key);
        if (Stats.TotalBlocks > 0)
        {
            float Utilization = (float)Stats.BlocksInUse / (float)Stats.TotalBlocks;
            if (Utilization < 0.25f && Stats.TotalBlocks > 16)
            {
                Suggestions.Add(FString::Printf(TEXT("Pool %d has low utilization (%.1f%%). Consider trimming unused memory."), (int32)Pair.Key, Utilization * 100.0f));
            }
            else if (Utilization > 0.9f)
            {
                Suggestions.Add(FString::Printf(TEXT("Pool %d is nearly full (%.1f%%). Consider pre-allocating more blocks."), (int32)Pair.Key, Utilization * 100.0f));
            }
        }
    }

    // Check async queue
    if (GetAsyncQueueDepth() > MaxAsyncTasks * 0.8f)
    {
        Suggestions.Add(TEXT("Async task queue is nearly full. Consider increasing MaxAsyncTasks or reducing task complexity."));
    }

    // Check performance samples
    float CognitiveTime = GetSampleAverageTime(TEXT("CognitiveProcessing"));
    if (CognitiveTime > 5.0f)
    {
        Suggestions.Add(FString::Printf(TEXT("Cognitive processing taking %.2f ms on average. Consider optimizing or using async processing."), CognitiveTime));
    }

    return Suggestions;
}

UCognitiveMemoryManager* UCognitiveMemoryManager::GetInstance(UObject* WorldContextObject)
{
    if (!GCognitiveMemoryManager)
    {
        GCognitiveMemoryManager = NewObject<UCognitiveMemoryManager>();
        GCognitiveMemoryManager->AddToRoot(); // Prevent garbage collection
        GCognitiveMemoryManager->Initialize();
    }

    return GCognitiveMemoryManager;
}

int32 UCognitiveMemoryManager::FindFreeBlock(EMemoryPoolType PoolType, int32 Size)
{
    if (!MemoryPools.Contains(PoolType))
    {
        return -1;
    }

    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];

    for (int32 i = 0; i < Headers.Num(); ++i)
    {
        if (!Headers[i].bInUse && Headers[i].BlockSize >= Size)
        {
            return i;
        }
    }

    return -1;
}

void UCognitiveMemoryManager::ExpandPool(EMemoryPoolType PoolType, int32 AdditionalBlocks, int32 BlockSize)
{
    if (!MemoryPools.Contains(PoolType))
    {
        MemoryPools.Add(PoolType, TArray<FMemoryBlockHeader>());
        PoolData.Add(PoolType, TArray<TArray<uint8>>());
    }

    TArray<FMemoryBlockHeader>& Headers = MemoryPools[PoolType];
    TArray<TArray<uint8>>& Data = PoolData[PoolType];

    for (int32 i = 0; i < AdditionalBlocks; ++i)
    {
        FMemoryBlockHeader Header;
        Header.BlockSize = BlockSize;
        Header.PoolType = PoolType;
        Header.bInUse = false;
        Header.RefCount = 0;

        Headers.Add(Header);

        TArray<uint8> BlockData;
        BlockData.SetNum(BlockSize);
        Data.Add(BlockData);
    }
}

void UCognitiveMemoryManager::UpdatePerformanceMetrics()
{
    FScopeLock Lock(&CriticalSection);

    CurrentMetrics.AverageFrameTime = GetSampleAverageTime(TEXT("FrameTime"));
    CurrentMetrics.CognitiveProcessingTime = GetSampleAverageTime(TEXT("CognitiveProcessing"));
    CurrentMetrics.MemoryAllocationTime = GetSampleAverageTime(TEXT("MemoryAllocation"));
    CurrentMetrics.CacheLookupTime = GetSampleAverageTime(TEXT("CacheLookup"));
    CurrentMetrics.AsyncQueueDepth = GetAsyncQueueDepth();

    // Calculate total memory
    int64 TotalMemory = 0;
    for (const auto& Pair : MemoryPools)
    {
        FMemoryPoolStats Stats = GetPoolStats(Pair.Key);
        TotalMemory += Stats.TotalMemory;
    }
    TotalMemory += GetCacheSize();

    CurrentMetrics.TotalMemoryMB = TotalMemory / (1024.0f * 1024.0f);

    // Estimate GC pressure
    float CacheUtilization = GetCacheSize() / (MaxCacheSizeMB * 1024.0f * 1024.0f);
    CurrentMetrics.GCPressure = FMath::Clamp(CacheUtilization, 0.0f, 1.0f);
}
