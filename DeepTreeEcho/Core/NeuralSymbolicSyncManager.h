// NeuralSymbolicSyncManager.h
// Neural-Symbolic Synchronization Manager for Deep Tree Echo
// Feature F1.1.4: Coordinates timing and data consistency between neural and symbolic subsystems
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/CognitiveTypes.h"
#include "NeuralSymbolicSyncManager.generated.h"

// Forward declarations
class UNeuroSymbolicBridge;
class UBidirectionalMessageProtocol;
class UCognitiveCycleManager;

// ============================================================================
// SYNCHRONIZATION POLICY
// ============================================================================

/**
 * Synchronization policy determining when neural/symbolic states are exchanged
 */
UENUM(BlueprintType)
enum class ESyncPolicy : uint8
{
    /** Immediate: Sync on every update (lowest latency, highest CPU) */
    Immediate    UMETA(DisplayName = "Immediate"),

    /** Batched: Accumulate updates and sync at fixed intervals */
    Batched      UMETA(DisplayName = "Batched"),

    /** CycleAligned: Sync only at cognitive cycle triadic points */
    CycleAligned UMETA(DisplayName = "Cycle Aligned"),

    /** OnDemand: Sync only when explicitly requested */
    OnDemand     UMETA(DisplayName = "On Demand")
};

/**
 * Buffer state for double-buffering
 */
UENUM(BlueprintType)
enum class EBufferState : uint8
{
    /** Ready for writing by producer */
    Writing  UMETA(DisplayName = "Writing"),

    /** Ready for reading by consumer */
    Reading  UMETA(DisplayName = "Reading"),

    /** Being swapped between states */
    Swapping UMETA(DisplayName = "Swapping")
};

// ============================================================================
// SYNCHRONIZATION CONFIGURATION
// ============================================================================

/**
 * Configuration for the synchronization manager
 */
USTRUCT(BlueprintType)
struct FNeuralSymbolicSyncConfig
{
    GENERATED_BODY()

    /** Active synchronization policy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    ESyncPolicy SyncPolicy = ESyncPolicy::CycleAligned;

    /** Batch interval in seconds (used with Batched policy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float BatchIntervalSeconds = 0.016f;

    /** Maximum pending updates before forced sync */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "1", ClampMax = "10000"))
    int32 MaxPendingUpdates = 256;

    /** Enable double-buffering for lock-free access */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableDoubleBuffering = true;

    /** Neural buffer capacity (number of activation patterns) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "16", ClampMax = "4096"))
    int32 NeuralBufferCapacity = 512;

    /** Symbolic buffer capacity (number of representations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "16", ClampMax = "4096"))
    int32 SymbolicBufferCapacity = 512;

    /** Which triadic sync points to use (bitmask: bit0=Triad1, bit1=Triad2, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (Bitmask))
    int32 TriadSyncMask = 0x0F;

    /** Allow sync to be skipped if no data changed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bSkipEmptySync = true;

    /** Timeout for sync operations (ms) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.1", ClampMax = "100.0"))
    float SyncTimeoutMs = 5.0f;
};

// ============================================================================
// SYNCHRONIZATION STATE
// ============================================================================

/**
 * Neural data buffer for double-buffering
 */
USTRUCT(BlueprintType)
struct FNeuralDataBuffer
{
    GENERATED_BODY()

    /** Activation patterns stored in this buffer */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    TArray<TArray<float>> ActivationPatterns;

    /** Timestamps for each pattern */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    TArray<float> Timestamps;

    /** Number of valid entries */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    int32 Count = 0;

    /** Frame number when last written */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    int64 FrameNumber = 0;

    /** Buffer state */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    EBufferState State = EBufferState::Writing;
};

/**
 * Symbolic data buffer for double-buffering
 */
USTRUCT(BlueprintType)
struct FSymbolicDataBuffer
{
    GENERATED_BODY()

    /** Symbol IDs stored in this buffer */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    TArray<FString> SymbolIDs;

    /** Symbol types */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    TArray<FString> SymbolTypes;

    /** Property maps per symbol */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    TArray<TMap<FString, FString>> Properties;

    /** Number of valid entries */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    int32 Count = 0;

    /** Frame number when last written */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    int64 FrameNumber = 0;

    /** Buffer state */
    UPROPERTY(BlueprintReadOnly, Category = "Buffer")
    EBufferState State = EBufferState::Writing;
};

/**
 * Synchronization metrics
 */
USTRUCT(BlueprintType)
struct FSyncMetrics
{
    GENERATED_BODY()

    /** Total sync operations performed */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalSyncCount = 0;

    /** Total buffer swaps performed */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TotalSwapCount = 0;

    /** Sync operations skipped (empty data) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 SkippedSyncCount = 0;

    /** Average sync latency in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageSyncLatencyMs = 0.0f;

    /** Peak sync latency in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PeakSyncLatencyMs = 0.0f;

    /** Current neural buffer utilization (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float NeuralBufferUtilization = 0.0f;

    /** Current symbolic buffer utilization (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float SymbolicBufferUtilization = 0.0f;

    /** Number of pending neural updates */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 PendingNeuralUpdates = 0;

    /** Number of pending symbolic updates */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 PendingSymbolicUpdates = 0;

    /** Frames since last sync */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 FramesSinceLastSync = 0;

    /** Current cognitive step at last sync */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 LastSyncCognitiveStep = 0;

    /** Forced syncs due to buffer overflow */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 ForcedSyncCount = 0;

    /** Sync timeouts */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int64 TimeoutCount = 0;
};

// ============================================================================
// SYNCHRONIZATION MANAGER COMPONENT
// ============================================================================

/**
 * Neural-Symbolic Synchronization Manager
 *
 * Coordinates the timing and data consistency between neural and symbolic
 * subsystems within the Deep Tree Echo cognitive architecture. Provides:
 *
 * - Double-buffered data exchange: Lock-free concurrent access between
 *   neural producers and symbolic consumers (and vice versa)
 * - Cycle-aligned synchronization: Respects the 12-step cognitive cycle
 *   by performing sync at configurable triadic points {1,5,9}, {2,6,10}, etc.
 * - Configurable policies: Immediate, batched, cycle-aligned, or on-demand
 * - Buffer overflow protection: Forces sync when buffers approach capacity
 * - Metrics tracking: Latency, throughput, utilization, and skip statistics
 *
 * The manager ensures that neural activations and symbolic state changes
 * are exchanged consistently without data races or torn reads, while
 * maintaining the real-time performance requirements (<1ms sync latency).
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNeuralSymbolicSyncManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeuralSymbolicSyncManager();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Synchronization configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SyncManager|Config")
    FNeuralSymbolicSyncConfig Config;

    // ========================================
    // SUBSYSTEM REFERENCES
    // ========================================

    /** Reference to the neuro-symbolic bridge */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|Subsystems")
    UNeuroSymbolicBridge* Bridge;

    /** Reference to the message protocol */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|Subsystems")
    UBidirectionalMessageProtocol* MessageProtocol;

    /** Reference to the cognitive cycle manager */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|Subsystems")
    UCognitiveCycleManager* CycleManager;

    // ========================================
    // STATE (Read-Only)
    // ========================================

    /** Current sync metrics */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|State")
    FSyncMetrics Metrics;

    /** Whether the manager is currently active */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|State")
    bool bIsActive = false;

    /** Current cognitive step (cached from CycleManager, 1-12) */
    UPROPERTY(BlueprintReadOnly, Category = "SyncManager|State")
    int32 CurrentCognitiveStep = 1;

    // ========================================
    // PUBLIC API - LIFECYCLE
    // ========================================

    /** Initialize the synchronization manager and discover subsystems */
    UFUNCTION(BlueprintCallable, Category = "SyncManager")
    void Initialize();

    /** Start synchronization processing */
    UFUNCTION(BlueprintCallable, Category = "SyncManager")
    void Start();

    /** Stop synchronization processing */
    UFUNCTION(BlueprintCallable, Category = "SyncManager")
    void Stop();

    /** Reset all buffers and metrics */
    UFUNCTION(BlueprintCallable, Category = "SyncManager")
    void Reset();

    // ========================================
    // PUBLIC API - NEURAL BUFFER OPERATIONS
    // ========================================

    /** Submit neural activation data to the write buffer */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Neural")
    bool SubmitNeuralActivation(const TArray<float>& Activation, float Timestamp);

    /** Submit a batch of neural activations */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Neural")
    int32 SubmitNeuralBatch(const TArray<TArray<float>>& Activations, const TArray<float>& Timestamps);

    /** Read the latest committed neural data (from read buffer) */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Neural")
    bool ReadNeuralBuffer(TArray<TArray<float>>& OutActivations, TArray<float>& OutTimestamps);

    /** Get the number of pending neural updates in the write buffer */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Neural")
    int32 GetPendingNeuralCount() const;

    // ========================================
    // PUBLIC API - SYMBOLIC BUFFER OPERATIONS
    // ========================================

    /** Submit a symbolic state update to the write buffer */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Symbolic")
    bool SubmitSymbolicUpdate(const FString& SymbolID, const FString& SymbolType, const TMap<FString, FString>& Properties);

    /** Submit a batch of symbolic updates */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Symbolic")
    int32 SubmitSymbolicBatch(const TArray<FString>& SymbolIDs, const TArray<FString>& SymbolTypes);

    /** Read the latest committed symbolic data (from read buffer) */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Symbolic")
    bool ReadSymbolicBuffer(TArray<FString>& OutSymbolIDs, TArray<FString>& OutSymbolTypes, TArray<TMap<FString, FString>>& OutProperties);

    /** Get the number of pending symbolic updates in the write buffer */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Symbolic")
    int32 GetPendingSymbolicCount() const;

    // ========================================
    // PUBLIC API - SYNCHRONIZATION CONTROL
    // ========================================

    /** Force an immediate synchronization (buffer swap) */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Sync")
    bool ForceSync();

    /** Check if a sync is pending based on current policy */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Sync")
    bool IsSyncPending() const;

    /** Check if the current cognitive step is a sync point */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Sync")
    bool IsAtSyncPoint() const;

    /** Get the next sync point (cognitive step number, 1-12) */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Sync")
    int32 GetNextSyncPoint() const;

    /** Change the sync policy at runtime */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Sync")
    void SetSyncPolicy(ESyncPolicy NewPolicy);

    // ========================================
    // PUBLIC API - METRICS & DIAGNOSTICS
    // ========================================

    /** Get current synchronization metrics */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Metrics")
    FSyncMetrics GetMetrics() const;

    /** Reset metrics counters */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Metrics")
    void ResetMetrics();

    /** Get buffer utilization as percentage (0-100) */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Metrics")
    float GetBufferUtilizationPercent() const;

    /** Check if buffers are approaching capacity */
    UFUNCTION(BlueprintCallable, Category = "SyncManager|Metrics")
    bool IsBufferPressureHigh() const;

protected:
    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Perform the actual buffer swap operation */
    void PerformBufferSwap();

    /** Evaluate whether sync should occur based on policy */
    bool ShouldSync(float DeltaTime) const;

    /** Check if current step matches the triad sync mask */
    bool IsTriadSyncPoint(int32 Step) const;

    /** Update internal metrics after a sync */
    void UpdateMetrics(float SyncDuration);

    /** Handle buffer overflow condition */
    void HandleBufferOverflow();

    /** Discover and cache subsystem references */
    void DiscoverSubsystems();

private:
    // ========================================
    // DOUBLE BUFFERS
    // ========================================

    /** Neural double-buffer: index 0 = write, index 1 = read */
    FNeuralDataBuffer NeuralBuffers[2];

    /** Symbolic double-buffer: index 0 = write, index 1 = read */
    FSymbolicDataBuffer SymbolicBuffers[2];

    /** Which buffer index is currently the write buffer (0 or 1) */
    int32 NeuralWriteIndex = 0;

    /** Which buffer index is currently the write buffer (0 or 1) */
    int32 SymbolicWriteIndex = 0;

    // ========================================
    // TIMING STATE
    // ========================================

    /** Time accumulator for batched sync policy */
    float BatchTimeAccumulator = 0.0f;

    /** Frame counter */
    int64 FrameCounter = 0;

    /** Last sync frame number */
    int64 LastSyncFrame = 0;

    /** Running sum of sync latencies for average calculation */
    double SyncLatencySum = 0.0;

    /** Previous cognitive step (to detect transitions) */
    int32 PreviousCognitiveStep = 1;
};
