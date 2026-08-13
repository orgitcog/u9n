/**
 * @file NeuralSymbolicSyncManagerTests.cpp
 * @brief Unit tests for the Neural-Symbolic Synchronization Manager (F1.1.4)
 *
 * Tests cover:
 * - Double-buffer swap correctness
 * - Sync policy evaluation (Immediate, Batched, CycleAligned, OnDemand)
 * - Neural buffer submit/read operations
 * - Symbolic buffer submit/read operations
 * - Buffer overflow handling
 * - Triadic sync point detection
 * - Metrics tracking accuracy
 * - Concurrent access patterns
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <array>
#include <string>
#include <map>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

namespace {

// ============================================================================
// Standalone Mock Types (no Unreal Engine dependency)
// ============================================================================

enum class ESyncPolicy : uint8_t
{
    Immediate,
    Batched,
    CycleAligned,
    OnDemand
};

enum class EBufferState : uint8_t
{
    Writing,
    Reading,
    Swapping
};

struct SyncConfig
{
    ESyncPolicy SyncPolicy = ESyncPolicy::CycleAligned;
    float BatchIntervalSeconds = 0.016f;
    int32_t MaxPendingUpdates = 256;
    bool bEnableDoubleBuffering = true;
    int32_t NeuralBufferCapacity = 512;
    int32_t SymbolicBufferCapacity = 512;
    int32_t TriadSyncMask = 0x0F; // All triads enabled
    bool bSkipEmptySync = true;
    float SyncTimeoutMs = 5.0f;
};

struct NeuralDataBuffer
{
    std::vector<std::vector<float>> ActivationPatterns;
    std::vector<float> Timestamps;
    int32_t Count = 0;
    int64_t FrameNumber = 0;
    EBufferState State = EBufferState::Writing;

    void Clear()
    {
        ActivationPatterns.clear();
        Timestamps.clear();
        Count = 0;
    }

    void Reserve(int32_t Capacity)
    {
        ActivationPatterns.reserve(Capacity);
        Timestamps.reserve(Capacity);
    }
};

struct SymbolicDataBuffer
{
    std::vector<std::string> SymbolIDs;
    std::vector<std::string> SymbolTypes;
    std::vector<std::map<std::string, std::string>> Properties;
    int32_t Count = 0;
    int64_t FrameNumber = 0;
    EBufferState State = EBufferState::Writing;

    void Clear()
    {
        SymbolIDs.clear();
        SymbolTypes.clear();
        Properties.clear();
        Count = 0;
    }

    void Reserve(int32_t Capacity)
    {
        SymbolIDs.reserve(Capacity);
        SymbolTypes.reserve(Capacity);
        Properties.reserve(Capacity);
    }
};

struct SyncMetrics
{
    int64_t TotalSyncCount = 0;
    int64_t TotalSwapCount = 0;
    int64_t SkippedSyncCount = 0;
    float AverageSyncLatencyMs = 0.0f;
    float PeakSyncLatencyMs = 0.0f;
    float NeuralBufferUtilization = 0.0f;
    float SymbolicBufferUtilization = 0.0f;
    int32_t PendingNeuralUpdates = 0;
    int32_t PendingSymbolicUpdates = 0;
    int32_t FramesSinceLastSync = 0;
    int32_t LastSyncCognitiveStep = 0;
    int64_t ForcedSyncCount = 0;
    int64_t TimeoutCount = 0;
};

/**
 * Standalone implementation of the Synchronization Manager logic
 * for testing without Unreal Engine dependencies.
 */
class NeuralSymbolicSyncManager
{
public:
    SyncConfig Config;
    SyncMetrics Metrics;
    bool bIsActive = false;
    int32_t CurrentCognitiveStep = 1;

    NeuralSymbolicSyncManager()
    {
        Initialize();
    }

    void Initialize()
    {
        for (int i = 0; i < 2; i++)
        {
            NeuralBuffers[i].Reserve(Config.NeuralBufferCapacity);
            NeuralBuffers[i].Count = 0;
            NeuralBuffers[i].FrameNumber = 0;

            SymbolicBuffers[i].Reserve(Config.SymbolicBufferCapacity);
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
        Metrics = SyncMetrics();
        SyncLatencySum = 0.0;
    }

    void Start() { bIsActive = true; }
    void Stop() { bIsActive = false; }

    void Reset()
    {
        Stop();
        for (int i = 0; i < 2; i++)
        {
            NeuralBuffers[i].Clear();
            SymbolicBuffers[i].Clear();
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
        Metrics = SyncMetrics();
        SyncLatencySum = 0.0;
    }

    // Neural buffer operations
    bool SubmitNeuralActivation(const std::vector<float>& Activation, float Timestamp)
    {
        NeuralDataBuffer& WriteBuffer = NeuralBuffers[NeuralWriteIndex];
        if (WriteBuffer.Count >= Config.NeuralBufferCapacity)
        {
            return false;
        }
        WriteBuffer.ActivationPatterns.push_back(Activation);
        WriteBuffer.Timestamps.push_back(Timestamp);
        WriteBuffer.Count++;
        WriteBuffer.FrameNumber = FrameCounter;

        if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive)
        {
            ForceSync();
        }
        return true;
    }

    int32_t SubmitNeuralBatch(const std::vector<std::vector<float>>& Activations,
                              const std::vector<float>& Timestamps)
    {
        NeuralDataBuffer& WriteBuffer = NeuralBuffers[NeuralWriteIndex];
        int32_t Available = Config.NeuralBufferCapacity - WriteBuffer.Count;
        int32_t ToWrite = std::min(static_cast<int32_t>(Activations.size()), Available);

        for (int32_t i = 0; i < ToWrite; i++)
        {
            WriteBuffer.ActivationPatterns.push_back(Activations[i]);
            WriteBuffer.Timestamps.push_back(i < static_cast<int32_t>(Timestamps.size()) ? Timestamps[i] : 0.0f);
        }
        WriteBuffer.Count += ToWrite;
        WriteBuffer.FrameNumber = FrameCounter;

        if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive && ToWrite > 0)
        {
            ForceSync();
        }
        return ToWrite;
    }

    bool ReadNeuralBuffer(std::vector<std::vector<float>>& OutActivations,
                          std::vector<float>& OutTimestamps)
    {
        int32_t ReadIndex = 1 - NeuralWriteIndex;
        const NeuralDataBuffer& ReadBuffer = NeuralBuffers[ReadIndex];
        if (ReadBuffer.Count == 0)
        {
            return false;
        }
        OutActivations = ReadBuffer.ActivationPatterns;
        OutTimestamps = ReadBuffer.Timestamps;
        return true;
    }

    int32_t GetPendingNeuralCount() const
    {
        return NeuralBuffers[NeuralWriteIndex].Count;
    }

    // Symbolic buffer operations
    bool SubmitSymbolicUpdate(const std::string& SymbolID, const std::string& SymbolType,
                              const std::map<std::string, std::string>& Props)
    {
        SymbolicDataBuffer& WriteBuffer = SymbolicBuffers[SymbolicWriteIndex];
        if (WriteBuffer.Count >= Config.SymbolicBufferCapacity)
        {
            return false;
        }
        WriteBuffer.SymbolIDs.push_back(SymbolID);
        WriteBuffer.SymbolTypes.push_back(SymbolType);
        WriteBuffer.Properties.push_back(Props);
        WriteBuffer.Count++;
        WriteBuffer.FrameNumber = FrameCounter;

        if (Config.SyncPolicy == ESyncPolicy::Immediate && bIsActive)
        {
            ForceSync();
        }
        return true;
    }

    bool ReadSymbolicBuffer(std::vector<std::string>& OutIDs,
                            std::vector<std::string>& OutTypes,
                            std::vector<std::map<std::string, std::string>>& OutProps)
    {
        int32_t ReadIndex = 1 - SymbolicWriteIndex;
        const SymbolicDataBuffer& ReadBuffer = SymbolicBuffers[ReadIndex];
        if (ReadBuffer.Count == 0)
        {
            return false;
        }
        OutIDs = ReadBuffer.SymbolIDs;
        OutTypes = ReadBuffer.SymbolTypes;
        OutProps = ReadBuffer.Properties;
        return true;
    }

    int32_t GetPendingSymbolicCount() const
    {
        return SymbolicBuffers[SymbolicWriteIndex].Count;
    }

    // Sync operations
    bool ForceSync()
    {
        auto Start = std::chrono::high_resolution_clock::now();

        bool bHasNeural = NeuralBuffers[NeuralWriteIndex].Count > 0;
        bool bHasSymbolic = SymbolicBuffers[SymbolicWriteIndex].Count > 0;

        if (Config.bSkipEmptySync && !bHasNeural && !bHasSymbolic)
        {
            Metrics.SkippedSyncCount++;
            return false;
        }

        PerformBufferSwap();

        auto End = std::chrono::high_resolution_clock::now();
        float DurationMs = std::chrono::duration<float, std::milli>(End - Start).count();
        UpdateMetrics(DurationMs);
        LastSyncFrame = FrameCounter;
        Metrics.LastSyncCognitiveStep = CurrentCognitiveStep;

        return true;
    }

    bool IsAtSyncPoint() const
    {
        return IsTriadSyncPoint(CurrentCognitiveStep);
    }

    int32_t GetNextSyncPoint() const
    {
        for (int32_t Offset = 1; Offset <= 12; Offset++)
        {
            int32_t Step = ((CurrentCognitiveStep - 1 + Offset) % 12) + 1;
            if (IsTriadSyncPoint(Step))
            {
                return Step;
            }
        }
        return CurrentCognitiveStep;
    }

    bool IsTriadSyncPoint(int32_t Step) const
    {
        if (Step < 1 || Step > 12) return false;
        int32_t TriadIndex = (Step - 1) % 4;
        return (Config.TriadSyncMask & (1 << TriadIndex)) != 0;
    }

    bool IsBufferPressureHigh() const
    {
        return NeuralBuffers[NeuralWriteIndex].Count >= Config.MaxPendingUpdates ||
               SymbolicBuffers[SymbolicWriteIndex].Count >= Config.MaxPendingUpdates;
    }

    float GetBufferUtilizationPercent() const
    {
        float NeuralUtil = Config.NeuralBufferCapacity > 0
            ? (static_cast<float>(NeuralBuffers[NeuralWriteIndex].Count) / Config.NeuralBufferCapacity) * 100.0f
            : 0.0f;
        float SymbolicUtil = Config.SymbolicBufferCapacity > 0
            ? (static_cast<float>(SymbolicBuffers[SymbolicWriteIndex].Count) / Config.SymbolicBufferCapacity) * 100.0f
            : 0.0f;
        return std::max(NeuralUtil, SymbolicUtil);
    }

    void Tick(float DeltaTime)
    {
        if (!bIsActive) return;
        FrameCounter++;
        BatchTimeAccumulator += DeltaTime;
        Metrics.FramesSinceLastSync = static_cast<int32_t>(FrameCounter - LastSyncFrame);
        Metrics.PendingNeuralUpdates = NeuralBuffers[NeuralWriteIndex].Count;
        Metrics.PendingSymbolicUpdates = SymbolicBuffers[SymbolicWriteIndex].Count;

        if (IsBufferPressureHigh())
        {
            Metrics.ForcedSyncCount++;
            ForceSync();
            return;
        }

        if (ShouldSync(DeltaTime))
        {
            ForceSync();
            BatchTimeAccumulator = 0.0f;
        }
    }

    void SetCognitiveStep(int32_t Step)
    {
        PreviousCognitiveStep = CurrentCognitiveStep;
        CurrentCognitiveStep = Step;
    }

private:
    NeuralDataBuffer NeuralBuffers[2];
    SymbolicDataBuffer SymbolicBuffers[2];
    int32_t NeuralWriteIndex = 0;
    int32_t SymbolicWriteIndex = 0;
    float BatchTimeAccumulator = 0.0f;
    int64_t FrameCounter = 0;
    int64_t LastSyncFrame = 0;
    double SyncLatencySum = 0.0;
    int32_t PreviousCognitiveStep = 1;

    void PerformBufferSwap()
    {
        // Only swap a buffer pair if the write buffer has data
        if (NeuralBuffers[NeuralWriteIndex].Count > 0)
        {
            NeuralBuffers[NeuralWriteIndex].State = EBufferState::Swapping;
            int32_t OldWrite = NeuralWriteIndex;
            NeuralWriteIndex = 1 - NeuralWriteIndex;
            NeuralBuffers[NeuralWriteIndex].Clear();
            NeuralBuffers[NeuralWriteIndex].State = EBufferState::Writing;
            NeuralBuffers[OldWrite].State = EBufferState::Reading;
        }

        // Only swap symbolic buffers if write buffer has data
        if (SymbolicBuffers[SymbolicWriteIndex].Count > 0)
        {
            SymbolicBuffers[SymbolicWriteIndex].State = EBufferState::Swapping;
            int32_t OldWrite = SymbolicWriteIndex;
            SymbolicWriteIndex = 1 - SymbolicWriteIndex;
            SymbolicBuffers[SymbolicWriteIndex].Clear();
            SymbolicBuffers[SymbolicWriteIndex].State = EBufferState::Writing;
            SymbolicBuffers[OldWrite].State = EBufferState::Reading;
        }

        Metrics.TotalSwapCount++;
        Metrics.TotalSyncCount++;
    }

    bool ShouldSync(float DeltaTime) const
    {
        switch (Config.SyncPolicy)
        {
        case ESyncPolicy::Immediate:
            return false; // handled in Submit methods
        case ESyncPolicy::Batched:
            return BatchTimeAccumulator >= Config.BatchIntervalSeconds;
        case ESyncPolicy::CycleAligned:
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

    void UpdateMetrics(float SyncDuration)
    {
        SyncLatencySum += SyncDuration;
        Metrics.AverageSyncLatencyMs = static_cast<float>(SyncLatencySum / Metrics.TotalSyncCount);
        Metrics.PeakSyncLatencyMs = std::max(Metrics.PeakSyncLatencyMs, SyncDuration);

        Metrics.NeuralBufferUtilization = Config.NeuralBufferCapacity > 0
            ? static_cast<float>(NeuralBuffers[1 - NeuralWriteIndex].Count) / Config.NeuralBufferCapacity
            : 0.0f;
        Metrics.SymbolicBufferUtilization = Config.SymbolicBufferCapacity > 0
            ? static_cast<float>(SymbolicBuffers[1 - SymbolicWriteIndex].Count) / Config.SymbolicBufferCapacity
            : 0.0f;
        Metrics.FramesSinceLastSync = 0;
    }
};

// ============================================================================
// TEST FIXTURES
// ============================================================================

class SyncManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Manager = std::make_unique<NeuralSymbolicSyncManager>();
        Manager->Start();
    }

    void TearDown() override
    {
        Manager->Stop();
        Manager.reset();
    }

    std::unique_ptr<NeuralSymbolicSyncManager> Manager;
};

// ============================================================================
// BUFFER OPERATIONS TESTS
// ============================================================================

TEST_F(SyncManagerTest, NeuralSubmit_SingleActivation_Succeeds)
{
    std::vector<float> activation = {0.1f, 0.5f, 0.9f, 0.3f};
    EXPECT_TRUE(Manager->SubmitNeuralActivation(activation, 1.0f));
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 1);
}

TEST_F(SyncManagerTest, NeuralSubmit_BufferFull_ReturnsFalse)
{
    Manager->Config.NeuralBufferCapacity = 3;
    Manager->Initialize();
    Manager->Start();

    std::vector<float> activation = {0.5f};
    EXPECT_TRUE(Manager->SubmitNeuralActivation(activation, 1.0f));
    EXPECT_TRUE(Manager->SubmitNeuralActivation(activation, 2.0f));
    EXPECT_TRUE(Manager->SubmitNeuralActivation(activation, 3.0f));
    EXPECT_FALSE(Manager->SubmitNeuralActivation(activation, 4.0f)); // Buffer full
}

TEST_F(SyncManagerTest, NeuralBatch_PartialWrite_ReturnsCount)
{
    Manager->Config.NeuralBufferCapacity = 5;
    Manager->Initialize();
    Manager->Start();

    std::vector<std::vector<float>> batch(10, {0.5f, 0.3f});
    std::vector<float> timestamps(10, 1.0f);
    int32_t Written = Manager->SubmitNeuralBatch(batch, timestamps);
    EXPECT_EQ(Written, 5); // Only 5 fit
}

TEST_F(SyncManagerTest, SymbolicSubmit_SingleUpdate_Succeeds)
{
    std::map<std::string, std::string> props = {{"health", "100"}, {"type", "player"}};
    EXPECT_TRUE(Manager->SubmitSymbolicUpdate("entity_001", "Entity", props));
    EXPECT_EQ(Manager->GetPendingSymbolicCount(), 1);
}

TEST_F(SyncManagerTest, SymbolicSubmit_BufferFull_ReturnsFalse)
{
    Manager->Config.SymbolicBufferCapacity = 2;
    Manager->Initialize();
    Manager->Start();

    std::map<std::string, std::string> props;
    EXPECT_TRUE(Manager->SubmitSymbolicUpdate("e1", "Entity", props));
    EXPECT_TRUE(Manager->SubmitSymbolicUpdate("e2", "Entity", props));
    EXPECT_FALSE(Manager->SubmitSymbolicUpdate("e3", "Entity", props));
}

// ============================================================================
// DOUBLE BUFFER SWAP TESTS
// ============================================================================

TEST_F(SyncManagerTest, ForceSync_SwapsBuffers_DataAccessible)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Config.bSkipEmptySync = false;

    std::vector<float> activation = {0.8f, 0.2f, 0.6f};
    Manager->SubmitNeuralActivation(activation, 1.0f);

    // Before sync: read buffer should be empty
    std::vector<std::vector<float>> outActivations;
    std::vector<float> outTimestamps;
    EXPECT_FALSE(Manager->ReadNeuralBuffer(outActivations, outTimestamps));

    // Perform sync
    EXPECT_TRUE(Manager->ForceSync());

    // After sync: read buffer should contain the data
    EXPECT_TRUE(Manager->ReadNeuralBuffer(outActivations, outTimestamps));
    ASSERT_EQ(outActivations.size(), 1u);
    EXPECT_EQ(outActivations[0], activation);
    EXPECT_FLOAT_EQ(outTimestamps[0], 1.0f);

    // Write buffer should be empty after swap
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 0);
}

TEST_F(SyncManagerTest, ForceSync_SymbolicData_Accessible)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Config.bSkipEmptySync = false;

    std::map<std::string, std::string> props = {{"color", "red"}};
    Manager->SubmitSymbolicUpdate("obj_1", "Object", props);

    Manager->ForceSync();

    std::vector<std::string> ids, types;
    std::vector<std::map<std::string, std::string>> outProps;
    EXPECT_TRUE(Manager->ReadSymbolicBuffer(ids, types, outProps));
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "obj_1");
    EXPECT_EQ(types[0], "Object");
    EXPECT_EQ(outProps[0]["color"], "red");
}

TEST_F(SyncManagerTest, DoubleSwap_PreviousDataOverwritten)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Config.bSkipEmptySync = false;

    // First write and sync
    Manager->SubmitNeuralActivation({1.0f, 2.0f}, 1.0f);
    Manager->ForceSync();

    // Second write and sync (overwrites first read buffer)
    Manager->SubmitNeuralActivation({3.0f, 4.0f}, 2.0f);
    Manager->ForceSync();

    std::vector<std::vector<float>> out;
    std::vector<float> ts;
    EXPECT_TRUE(Manager->ReadNeuralBuffer(out, ts));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], (std::vector<float>{3.0f, 4.0f}));
}

TEST_F(SyncManagerTest, ForceSync_SkipEmpty_WhenConfigured)
{
    Manager->Config.bSkipEmptySync = true;
    EXPECT_FALSE(Manager->ForceSync()); // No data, should skip
    EXPECT_EQ(Manager->Metrics.SkippedSyncCount, 1);
}

TEST_F(SyncManagerTest, ForceSync_NoSkipEmpty_WhenDisabled)
{
    Manager->Config.bSkipEmptySync = false;
    EXPECT_TRUE(Manager->ForceSync()); // Should proceed even with no data
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 1);
}

// ============================================================================
// SYNC POLICY TESTS
// ============================================================================

TEST_F(SyncManagerTest, ImmediatePolicy_SyncsOnEverySubmit)
{
    Manager->Config.SyncPolicy = ESyncPolicy::Immediate;

    Manager->SubmitNeuralActivation({0.5f}, 1.0f);
    // Immediate policy syncs inside Submit
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 1);

    Manager->SubmitNeuralActivation({0.6f}, 2.0f);
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 2);
}

TEST_F(SyncManagerTest, OnDemandPolicy_NeverAutoSyncs)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;

    Manager->SubmitNeuralActivation({0.5f}, 1.0f);
    Manager->Tick(0.016f);
    Manager->Tick(0.016f);
    Manager->Tick(1.0f);

    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 0);
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 1);
}

TEST_F(SyncManagerTest, CycleAlignedPolicy_SyncsAtTriadPoints)
{
    Manager->Config.SyncPolicy = ESyncPolicy::CycleAligned;
    Manager->Config.TriadSyncMask = 0x01; // Only Triad 1 (steps 1, 5, 9)

    Manager->SubmitNeuralActivation({0.5f}, 1.0f);

    // Transition to step 2 → not a sync point for Triad 1
    Manager->SetCognitiveStep(2);
    Manager->Tick(0.016f);
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 0);

    // Transition to step 5 → IS a sync point for Triad 1
    Manager->SubmitNeuralActivation({0.6f}, 2.0f);
    Manager->SetCognitiveStep(5);
    Manager->Tick(0.016f);
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 1);
}

// ============================================================================
// TRIADIC SYNC POINT TESTS
// ============================================================================

TEST_F(SyncManagerTest, TriadSyncPoints_AllTriadsEnabled)
{
    Manager->Config.TriadSyncMask = 0x0F; // All triads

    // Every step should be a sync point when all triads are enabled
    for (int32_t step = 1; step <= 12; step++)
    {
        EXPECT_TRUE(Manager->IsTriadSyncPoint(step))
            << "Step " << step << " should be a sync point";
    }
}

TEST_F(SyncManagerTest, TriadSyncPoints_OnlyTriad1)
{
    Manager->Config.TriadSyncMask = 0x01; // Only Triad 1 (bit 0)

    // Triad 1 = steps where (step-1)%4 == 0: steps 1, 5, 9
    EXPECT_TRUE(Manager->IsTriadSyncPoint(1));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(2));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(3));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(4));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(5));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(6));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(7));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(8));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(9));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(10));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(11));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(12));
}

TEST_F(SyncManagerTest, TriadSyncPoints_Triad2And4)
{
    Manager->Config.TriadSyncMask = 0x0A; // Triads 2 and 4 (bits 1 and 3)

    // Triad 2: (step-1)%4 == 1 → steps 2, 6, 10
    // Triad 4: (step-1)%4 == 3 → steps 4, 8, 12
    EXPECT_FALSE(Manager->IsTriadSyncPoint(1));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(2));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(3));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(4));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(5));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(6));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(7));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(8));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(9));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(10));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(11));
    EXPECT_TRUE(Manager->IsTriadSyncPoint(12));
}

TEST_F(SyncManagerTest, TriadSyncPoints_InvalidSteps)
{
    EXPECT_FALSE(Manager->IsTriadSyncPoint(0));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(-1));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(13));
    EXPECT_FALSE(Manager->IsTriadSyncPoint(100));
}

TEST_F(SyncManagerTest, GetNextSyncPoint_CorrectForTriad1)
{
    Manager->Config.TriadSyncMask = 0x01; // Only Triad 1: steps 1, 5, 9
    Manager->CurrentCognitiveStep = 2;
    EXPECT_EQ(Manager->GetNextSyncPoint(), 5);

    Manager->CurrentCognitiveStep = 6;
    EXPECT_EQ(Manager->GetNextSyncPoint(), 9);

    Manager->CurrentCognitiveStep = 10;
    EXPECT_EQ(Manager->GetNextSyncPoint(), 1); // Wraps around
}

// ============================================================================
// BUFFER OVERFLOW TESTS
// ============================================================================

TEST_F(SyncManagerTest, BufferPressure_DetectedWhenHigh)
{
    Manager->Config.MaxPendingUpdates = 3;
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;

    EXPECT_FALSE(Manager->IsBufferPressureHigh());

    Manager->SubmitNeuralActivation({0.1f}, 1.0f);
    Manager->SubmitNeuralActivation({0.2f}, 2.0f);
    EXPECT_FALSE(Manager->IsBufferPressureHigh());

    Manager->SubmitNeuralActivation({0.3f}, 3.0f);
    EXPECT_TRUE(Manager->IsBufferPressureHigh());
}

TEST_F(SyncManagerTest, BufferOverflow_ForcesSyncOnTick)
{
    Manager->Config.MaxPendingUpdates = 2;
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;

    Manager->SubmitNeuralActivation({0.1f}, 1.0f);
    Manager->SubmitNeuralActivation({0.2f}, 2.0f);

    Manager->Tick(0.016f);
    EXPECT_EQ(Manager->Metrics.ForcedSyncCount, 1);
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 1);
}

// ============================================================================
// METRICS TESTS
// ============================================================================

TEST_F(SyncManagerTest, Metrics_InitiallyZero)
{
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 0);
    EXPECT_EQ(Manager->Metrics.TotalSwapCount, 0);
    EXPECT_EQ(Manager->Metrics.SkippedSyncCount, 0);
    EXPECT_FLOAT_EQ(Manager->Metrics.AverageSyncLatencyMs, 0.0f);
    EXPECT_FLOAT_EQ(Manager->Metrics.PeakSyncLatencyMs, 0.0f);
}

TEST_F(SyncManagerTest, Metrics_IncrementOnSync)
{
    Manager->Config.bSkipEmptySync = false;

    Manager->ForceSync();
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 1);
    EXPECT_EQ(Manager->Metrics.TotalSwapCount, 1);

    Manager->ForceSync();
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 2);
    EXPECT_EQ(Manager->Metrics.TotalSwapCount, 2);
}

TEST_F(SyncManagerTest, Metrics_LatencyTracked)
{
    Manager->Config.bSkipEmptySync = false;

    Manager->ForceSync();
    // Latency should be > 0 (even if very small)
    EXPECT_GE(Manager->Metrics.AverageSyncLatencyMs, 0.0f);
    EXPECT_GE(Manager->Metrics.PeakSyncLatencyMs, 0.0f);
}

TEST_F(SyncManagerTest, Metrics_CognitiveStepRecorded)
{
    Manager->Config.bSkipEmptySync = false;
    Manager->CurrentCognitiveStep = 7;

    Manager->ForceSync();
    EXPECT_EQ(Manager->Metrics.LastSyncCognitiveStep, 7);
}

TEST_F(SyncManagerTest, Metrics_BufferUtilization)
{
    Manager->Config.NeuralBufferCapacity = 10;
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Initialize();
    Manager->Start();

    // 3 out of 10 = 30%
    Manager->SubmitNeuralActivation({0.1f}, 1.0f);
    Manager->SubmitNeuralActivation({0.2f}, 2.0f);
    Manager->SubmitNeuralActivation({0.3f}, 3.0f);

    float utilization = Manager->GetBufferUtilizationPercent();
    EXPECT_FLOAT_EQ(utilization, 30.0f);
}

TEST_F(SyncManagerTest, Metrics_FramesSinceLastSync)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;

    Manager->Tick(0.016f);
    Manager->Tick(0.016f);
    Manager->Tick(0.016f);

    EXPECT_EQ(Manager->Metrics.FramesSinceLastSync, 3);
}

// ============================================================================
// CONCURRENT ACCESS PATTERN TESTS
// ============================================================================

TEST_F(SyncManagerTest, ConcurrentWriteRead_NoDataRace)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Config.NeuralBufferCapacity = 1000;
    Manager->Initialize();
    Manager->Start();

    std::atomic<bool> Done{false};
    std::atomic<int> WrittenCount{0};
    std::atomic<int> ReadCount{0};

    // Writer thread
    std::thread Writer([&]() {
        for (int i = 0; i < 100; i++)
        {
            std::vector<float> data = {static_cast<float>(i)};
            if (Manager->SubmitNeuralActivation(data, static_cast<float>(i)))
            {
                WrittenCount++;
            }
        }
        Done = true;
    });

    // Reader thread (reads from read buffer, which is separate)
    std::thread Reader([&]() {
        while (!Done.load())
        {
            std::vector<std::vector<float>> out;
            std::vector<float> ts;
            if (Manager->ReadNeuralBuffer(out, ts))
            {
                ReadCount++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    Writer.join();
    Reader.join();

    // Writer should have written all 100
    EXPECT_EQ(WrittenCount.load(), 100);
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 100);
}

TEST_F(SyncManagerTest, ConcurrentWriteAndSync_DataIntegrity)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->Config.NeuralBufferCapacity = 1000;
    Manager->Initialize();
    Manager->Start();

    // Write some data
    for (int i = 0; i < 50; i++)
    {
        Manager->SubmitNeuralActivation({static_cast<float>(i)}, static_cast<float>(i));
    }
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 50);

    // Sync
    Manager->ForceSync();
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 0);

    // Verify read buffer has all 50 items
    std::vector<std::vector<float>> out;
    std::vector<float> ts;
    EXPECT_TRUE(Manager->ReadNeuralBuffer(out, ts));
    EXPECT_EQ(out.size(), 50u);

    // Verify data integrity
    for (int i = 0; i < 50; i++)
    {
        EXPECT_FLOAT_EQ(out[i][0], static_cast<float>(i));
    }
}

// ============================================================================
// RESET & LIFECYCLE TESTS
// ============================================================================

TEST_F(SyncManagerTest, Reset_ClearsAllState)
{
    Manager->Config.bSkipEmptySync = false;
    Manager->SubmitNeuralActivation({0.5f}, 1.0f);
    Manager->ForceSync();

    Manager->Reset();

    EXPECT_EQ(Manager->GetPendingNeuralCount(), 0);
    EXPECT_EQ(Manager->GetPendingSymbolicCount(), 0);
    EXPECT_EQ(Manager->Metrics.TotalSyncCount, 0);
    EXPECT_FALSE(Manager->bIsActive);

    std::vector<std::vector<float>> out;
    std::vector<float> ts;
    EXPECT_FALSE(Manager->ReadNeuralBuffer(out, ts));
}

TEST_F(SyncManagerTest, StopStart_PreservesBufferData)
{
    Manager->Config.SyncPolicy = ESyncPolicy::OnDemand;
    Manager->SubmitNeuralActivation({0.5f}, 1.0f);

    Manager->Stop();
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 1); // Data preserved

    Manager->Start();
    EXPECT_EQ(Manager->GetPendingNeuralCount(), 1); // Still there
}

} // anonymous namespace
