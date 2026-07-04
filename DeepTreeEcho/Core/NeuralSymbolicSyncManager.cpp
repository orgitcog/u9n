// NeuralSymbolicSyncManager.cpp
// Neural-Symbolic Synchronization Manager for Deep Tree Echo
// Feature F1.1.4: Coordinates timing and data consistency between neural and symbolic subsystems
// Copyright (c) 2025 Deep Tree Echo Project

#include "Core/NeuralSymbolicSyncManager.h"
#include "Core/NeuroSymbolicBridge.h"
#include "Core/BidirectionalMessageProtocol.h"
#include "Core/CognitiveCycleManager.h"

UNeuralSymbolicSyncManager::UNeuralSymbolicSyncManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UNeuralSymbolicSyncManager::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
    Start();
}

void UNeuralSymbolicSyncManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stop();
    Super::EndPlay(EndPlayReason);
}

void UNeuralSymbolicSyncManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsActive)
    {
        return;
    }

    FrameCounter++;
    Metrics.FramesSinceLastSync = static_cast<int32>(FrameCounter - LastSyncFrame);

    // Accumulate time for batched policy
    BatchTimeAccumulator += DeltaTime;

    // Cache current cognitive step from cycle manager
    if (CycleManager)
    {
        PreviousCognitiveStep = CurrentCognitiveStep;
        // Get step from cycle manager (1-12)
        CurrentCognitiveStep = CycleManager->GetCurrentStep();
    }

    // Update pending counts for metrics
    Metrics.PendingNeuralUpdates = NeuralBuffers[NeuralWriteIndex].Count;
    Metrics.PendingSymbolicUpdates = SymbolicBuffers[SymbolicWriteIndex].Count;

    // Check for buffer overflow
    if (IsBufferPressureHigh())
    {
        HandleBufferOverflow();
        return;
    }

    // Evaluate sync based on policy
    if (ShouldSync(DeltaTime))
    {
        ForceSync();
        BatchTimeAccumulator = 0.0f;
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UNeuralSymbolicSyncManager::Initialize()
{
    DiscoverSubsystems();

    // Initialize neural double buffers
    for (int32 i = 0; i < 2; i++)
    {
        NeuralBuffers[i].ActivationPatterns.Reserve(Config.NeuralBufferCapacity);
        NeuralBuffers[i].Timestamps.Reserve(Config.NeuralBufferCapacity);
        NeuralBuffers[i].Count = 0;
        NeuralBuffers[i].FrameNumber = 0;
    }
    NeuralBuffers[0].State = EBufferState::Writing;
    NeuralBuffers[1].State = EBufferState::Reading;

    // Initialize symbolic double buffers
    for (int32 i = 0; i < 2; i++)
    {
        SymbolicBuffers[i].SymbolIDs.Reserve(Config.SymbolicBufferCapacity);
        SymbolicBuffers[i].SymbolTypes.Reserve(Config.SymbolicBufferCapacity);
        SymbolicBuffers[i].Properties.Reserve(Config.SymbolicBufferCapacity);
        SymbolicBuffers[i].Count = 0;
        SymbolicBuffers[i].FrameNumber = 0;
    }
    SymbolicBuffers[0].State = EBufferState::Writing;
    SymbolicBuffers[1].State = EBufferState::Reading;

    // Reset indices
    NeuralWriteIndex = 0;
    SymbolicWriteIndex = 0;

    // Reset timing
    BatchTimeAccumulator = 0.0f;
    FrameCounter = 0;
    LastSyncFrame = 0;

    ResetMetrics();
}

void UNeuralSymbolicSyncManager::Start()
{
    bIsActive = true;
}

void UNeuralSymbolicSyncManager::Stop()
{
    bIsActive = false;
}

void UNeuralSymbolicSyncManager::Reset()
{
    Stop();

    // Clear all buffers
    for (int32 i = 0; i < 2; i++)
    {
        NeuralBuffers[i].ActivationPatterns.Empty();
        NeuralBuffers[i].Timestamps.Empty();
        NeuralBuffers[i].Count = 0;
        NeuralBuffers[i].FrameNumber = 0;

        SymbolicBuffers[i].SymbolIDs.Empty();
        SymbolicBuffers[i].SymbolTypes.Empty();
        SymbolicBuffers[i].Properties.Empty();
        SymbolicBuffers[i].Count = 0;
        SymbolicBuffers[i].FrameNumber = 0;
    }

    NeuralBuffers[0].State = EBufferState::Writing;
    NeuralBuffers[1].State = EBufferState::Reading;
    SymbolicBuffers[0].State = EBufferState::Writing;
    SymbolicBuffers[1].State = EBufferState::Reading;

    NeuralWriteIndex = 0;
    SymbolicWriteIndex = 0;
    BatchTimeAccumulator = 0.0f;
    FrameCounter = 0;
    LastSyncFrame = 0;

    ResetMetrics();
}

// ============================================================================
// NEURAL BUFFER OPERATIONS
// ============================================================================

bool UNeuralSymbolicSyncManager::SubmitNeuralActivation(const TArray<float>& Activation, float Timestamp)
{
    FNeuralDataBuffer& WriteBuffer = NeuralBuffers[NeuralWriteIndex];

    if (WriteBuffer.Count >= Config.NeuralBufferCapacity)
    {
        return false; // Buffer full
    }

    WriteBuffer.ActivationPatterns.Add(Activation);
    WriteBuffer.Timestamps.Add(Timestamp);
    WriteBuffer.Count++;
    WriteBuffer.FrameNumber = FrameCounter;

    // Immediate policy triggers sync on every write
    if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive)
    {
        ForceSync();
    }

    return true;
}

int32 UNeuralSymbolicSyncManager::SubmitNeuralBatch(const TArray<TArray<float>>& Activations, const TArray<float>& Timestamps)
{
    FNeuralDataBuffer& WriteBuffer = NeuralBuffers[NeuralWriteIndex];
    const int32 Available = Config.NeuralBufferCapacity - WriteBuffer.Count;
    const int32 ToWrite = FMath::Min(Activations.Num(), Available);

    for (int32 i = 0; i < ToWrite; i++)
    {
        WriteBuffer.ActivationPatterns.Add(Activations[i]);
        WriteBuffer.Timestamps.Add(i < Timestamps.Num() ? Timestamps[i] : 0.0f);
    }
    WriteBuffer.Count += ToWrite;
    WriteBuffer.FrameNumber = FrameCounter;

    // Immediate policy triggers sync on every write
    if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive && ToWrite > 0)
    {
        ForceSync();
    }

    return ToWrite;
}

bool UNeuralSymbolicSyncManager::ReadNeuralBuffer(TArray<TArray<float>>& OutActivations, TArray<float>& OutTimestamps)
{
    const int32 ReadIndex = 1 - NeuralWriteIndex; // Read from the other buffer
    const FNeuralDataBuffer& ReadBuffer = NeuralBuffers[ReadIndex];

    if (ReadBuffer.Count == 0)
    {
        return false;
    }

    OutActivations = ReadBuffer.ActivationPatterns;
    OutTimestamps = ReadBuffer.Timestamps;
    return true;
}

int32 UNeuralSymbolicSyncManager::GetPendingNeuralCount() const
{
    return NeuralBuffers[NeuralWriteIndex].Count;
}

// ============================================================================
// SYMBOLIC BUFFER OPERATIONS
// ============================================================================

bool UNeuralSymbolicSyncManager::SubmitSymbolicUpdate(const FString& SymbolID, const FString& SymbolType, const TMap<FString, FString>& Properties)
{
    FSymbolicDataBuffer& WriteBuffer = SymbolicBuffers[SymbolicWriteIndex];

    if (WriteBuffer.Count >= Config.SymbolicBufferCapacity)
    {
        return false; // Buffer full
    }

    WriteBuffer.SymbolIDs.Add(SymbolID);
    WriteBuffer.SymbolTypes.Add(SymbolType);
    WriteBuffer.Properties.Add(Properties);
    WriteBuffer.Count++;
    WriteBuffer.FrameNumber = FrameCounter;

    if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive)
    {
        ForceSync();
    }

    return true;
}

int32 UNeuralSymbolicSyncManager::SubmitSymbolicBatch(const TArray<FString>& SymbolIDs, const TArray<FString>& SymbolTypes)
{
    FSymbolicDataBuffer& WriteBuffer = SymbolicBuffers[SymbolicWriteIndex];
    const int32 Available = Config.SymbolicBufferCapacity - WriteBuffer.Count;
    const int32 ToWrite = FMath::Min(SymbolIDs.Num(), Available);

    for (int32 i = 0; i < ToWrite; i++)
    {
        WriteBuffer.SymbolIDs.Add(SymbolIDs[i]);
        WriteBuffer.SymbolTypes.Add(i < SymbolTypes.Num() ? SymbolTypes[i] : TEXT("Unknown"));
        WriteBuffer.Properties.Add(TMap<FString, FString>());
    }
    WriteBuffer.Count += ToWrite;
    WriteBuffer.FrameNumber = FrameCounter;

    // Immediate policy triggers sync on every write
    if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive && ToWrite > 0)
    {
        ForceSync();
    }

    return ToWrite;
}

bool UNeuralSymbolicSyncManager::ReadSymbolicBuffer(TArray<FString>& OutSymbolIDs, TArray<FString>& OutSymbolTypes, TArray<TMap<FString, FString>>& OutProperties)
{
    const int32 ReadIndex = 1 - SymbolicWriteIndex;
    const FSymbolicDataBuffer& ReadBuffer = SymbolicBuffers[ReadIndex];

    if (ReadBuffer.Count == 0)
    {
        return false;
    }

    OutSymbolIDs = ReadBuffer.SymbolIDs;
    OutSymbolTypes = ReadBuffer.SymbolTypes;
    OutProperties = ReadBuffer.Properties;
    return true;
}

int32 UNeuralSymbolicSyncManager::GetPendingSymbolicCount() const
{
    return SymbolicBuffers[SymbolicWriteIndex].Count;
}

// ============================================================================
// SYNCHRONIZATION CONTROL
// ============================================================================

bool UNeuralSymbolicSyncManager::ForceSync()
{
    const double StartTime = FPlatformTime::Seconds();

    // Check if there's anything to sync
    const bool bHasNeuralData = NeuralBuffers[NeuralWriteIndex].Count > 0;
    const bool bHasSymbolicData = SymbolicBuffers[SymbolicWriteIndex].Count > 0;

    if (Config.bSkipEmptySync && !bHasNeuralData && !bHasSymbolicData)
    {
        Metrics.SkippedSyncCount++;
        return false;
    }

    PerformBufferSwap();

    const double EndTime = FPlatformTime::Seconds();
    const float DurationMs = static_cast<float>((EndTime - StartTime) * 1000.0);

    UpdateMetrics(DurationMs);
    LastSyncFrame = FrameCounter;
    Metrics.LastSyncCognitiveStep = CurrentCognitiveStep;

    return true;
}

bool UNeuralSymbolicSyncManager::IsSyncPending() const
{
    if (!bIsActive)
    {
        return false;
    }

    switch (Config.SyncPolicy)
    {
    case ESyncPolicy::Immediate:
        return NeuralBuffers[NeuralWriteIndex].Count > 0 || SymbolicBuffers[SymbolicWriteIndex].Count > 0;

    case ESyncPolicy::Batched:
        return BatchTimeAccumulator >= Config.BatchIntervalSeconds;

    case ESyncPolicy::CycleAligned:
        // Consistent with ShouldSync: only pending when transitioning into a sync point
        return (CurrentCognitiveStep != PreviousCognitiveStep) && IsAtSyncPoint();

    case ESyncPolicy::OnDemand:
        return false;

    default:
        return false;
    }
}

bool UNeuralSymbolicSyncManager::IsAtSyncPoint() const
{
    return IsTriadSyncPoint(CurrentCognitiveStep);
}

int32 UNeuralSymbolicSyncManager::GetNextSyncPoint() const
{
    // Triadic sync points: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Find the next step that matches the triad mask
    for (int32 Offset = 1; Offset <= 12; Offset++)
    {
        int32 Step = ((CurrentCognitiveStep - 1 + Offset) % 12) + 1;
        if (IsTriadSyncPoint(Step))
        {
            return Step;
        }
    }
    return CurrentCognitiveStep; // Fallback
}

void UNeuralSymbolicSyncManager::SetSyncPolicy(ESyncPolicy NewPolicy)
{
    Config.SyncPolicy = NewPolicy;
    BatchTimeAccumulator = 0.0f;
}

// ============================================================================
// METRICS & DIAGNOSTICS
// ============================================================================

FSyncMetrics UNeuralSymbolicSyncManager::GetMetrics() const
{
    return Metrics;
}

void UNeuralSymbolicSyncManager::ResetMetrics()
{
    Metrics = FSyncMetrics();
    SyncLatencySum = 0.0;
}

float UNeuralSymbolicSyncManager::GetBufferUtilizationPercent() const
{
    const float NeuralUtil = Config.NeuralBufferCapacity > 0
        ? (static_cast<float>(NeuralBuffers[NeuralWriteIndex].Count) / Config.NeuralBufferCapacity) * 100.0f
        : 0.0f;
    const float SymbolicUtil = Config.SymbolicBufferCapacity > 0
        ? (static_cast<float>(SymbolicBuffers[SymbolicWriteIndex].Count) / Config.SymbolicBufferCapacity) * 100.0f
        : 0.0f;

    return FMath::Max(NeuralUtil, SymbolicUtil);
}

bool UNeuralSymbolicSyncManager::IsBufferPressureHigh() const
{
    const int32 NeuralCount = NeuralBuffers[NeuralWriteIndex].Count;
    const int32 SymbolicCount = SymbolicBuffers[SymbolicWriteIndex].Count;

    return NeuralCount >= Config.MaxPendingUpdates || SymbolicCount >= Config.MaxPendingUpdates;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UNeuralSymbolicSyncManager::PerformBufferSwap()
{
    // Only swap a buffer pair if the write buffer has data.
    // Swapping an empty write buffer would clear the read buffer (destroying unconsumed data).

    // Swap neural buffers (only if write buffer has data)
    if (NeuralBuffers[NeuralWriteIndex].Count > 0)
    {
        NeuralBuffers[NeuralWriteIndex].State = EBufferState::Swapping;
        const int32 OldWrite = NeuralWriteIndex;
        NeuralWriteIndex = 1 - NeuralWriteIndex;

        // Clear the new write buffer for fresh data
        NeuralBuffers[NeuralWriteIndex].ActivationPatterns.Empty();
        NeuralBuffers[NeuralWriteIndex].Timestamps.Empty();
        NeuralBuffers[NeuralWriteIndex].Count = 0;
        NeuralBuffers[NeuralWriteIndex].State = EBufferState::Writing;

        // Old write buffer becomes read buffer
        NeuralBuffers[OldWrite].State = EBufferState::Reading;
    }

    // Swap symbolic buffers (only if write buffer has data)
    if (SymbolicBuffers[SymbolicWriteIndex].Count > 0)
    {
        SymbolicBuffers[SymbolicWriteIndex].State = EBufferState::Swapping;
        const int32 OldWrite = SymbolicWriteIndex;
        SymbolicWriteIndex = 1 - SymbolicWriteIndex;

        // Clear the new write buffer
        SymbolicBuffers[SymbolicWriteIndex].SymbolIDs.Empty();
        SymbolicBuffers[SymbolicWriteIndex].SymbolTypes.Empty();
        SymbolicBuffers[SymbolicWriteIndex].Properties.Empty();
        SymbolicBuffers[SymbolicWriteIndex].Count = 0;
        SymbolicBuffers[SymbolicWriteIndex].State = EBufferState::Writing;

        // Old write buffer becomes read buffer
        SymbolicBuffers[OldWrite].State = EBufferState::Reading;
    }

    Metrics.TotalSwapCount++;
    Metrics.TotalSyncCount++;
}

bool UNeuralSymbolicSyncManager::ShouldSync(float DeltaTime) const
{
    switch (Config.SyncPolicy)
    {
    case ESyncPolicy::Immediate:
        // Immediate syncs happen in Submit* methods
        return false;

    case ESyncPolicy::Batched:
        return BatchTimeAccumulator >= Config.BatchIntervalSeconds;

    case ESyncPolicy::CycleAligned:
        // Sync when entering a triad sync point (step transition)
        if (CurrentCognitiveStep != PreviousCognitiveStep)
        {
            return IsTriadSyncPoint(CurrentCognitiveStep);
        }
        return false;

    case ESyncPolicy::OnDemand:
        return false;

    default:
        return false;
    }
}

bool UNeuralSymbolicSyncManager::IsTriadSyncPoint(int32 Step) const
{
    // Steps 1-12 map to triads:
    // Triad 1: Steps {1, 5, 9}  → bit 0
    // Triad 2: Steps {2, 6, 10} → bit 1
    // Triad 3: Steps {3, 7, 11} → bit 2
    // Triad 4: Steps {4, 8, 12} → bit 3
    if (Step < 1 || Step > 12)
    {
        return false;
    }

    // Determine which triad this step belongs to (0-based)
    const int32 TriadIndex = (Step - 1) % 4;

    // Check if this triad is enabled in the mask
    return (Config.TriadSyncMask & (1 << TriadIndex)) != 0;
}

void UNeuralSymbolicSyncManager::UpdateMetrics(float SyncDuration)
{
    SyncLatencySum += SyncDuration;
    Metrics.AverageSyncLatencyMs = static_cast<float>(SyncLatencySum / Metrics.TotalSyncCount);
    Metrics.PeakSyncLatencyMs = FMath::Max(Metrics.PeakSyncLatencyMs, SyncDuration);

    // Update buffer utilization
    Metrics.NeuralBufferUtilization = Config.NeuralBufferCapacity > 0
        ? static_cast<float>(NeuralBuffers[1 - NeuralWriteIndex].Count) / Config.NeuralBufferCapacity
        : 0.0f;
    Metrics.SymbolicBufferUtilization = Config.SymbolicBufferCapacity > 0
        ? static_cast<float>(SymbolicBuffers[1 - SymbolicWriteIndex].Count) / Config.SymbolicBufferCapacity
        : 0.0f;

    Metrics.FramesSinceLastSync = 0;
}

void UNeuralSymbolicSyncManager::HandleBufferOverflow()
{
    Metrics.ForcedSyncCount++;
    ForceSync();
}

void UNeuralSymbolicSyncManager::DiscoverSubsystems()
{
    if (!GetOwner())
    {
        return;
    }

    Bridge = GetOwner()->FindComponentByClass<UNeuroSymbolicBridge>();
    MessageProtocol = GetOwner()->FindComponentByClass<UBidirectionalMessageProtocol>();
    CycleManager = GetOwner()->FindComponentByClass<UCognitiveCycleManager>();
}
