/**
 * @file ReservoirTopologyManagerTests.cpp
 * @brief Comprehensive unit tests for Reservoir Topology Manager
 *
 * Tests cover:
 * - Topology generation and application
 * - Topology validation (size, self-loops, spectral radius)
 * - Dynamic spectral radius adjustment
 * - Dynamic weight scaling
 * - Named preset registration and activation
 * - Configuration change notifications
 * - Manager state snapshots
 *
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <random>
#include <cmath>

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
using FString = std::string;
using int32 = int;
#define TEXT(x) x

template<typename T>
using TArray = std::vector<T>;

// Mock topology config
struct FReservoirTopologyConfig
{
    int32 NumNodes = 100;
    float Connectivity = 0.1f;
    float SpectralRadius = 0.9f;
    int32 RandomSeed = 0;
};

// Mock topology stats
struct FReservoirTopologyStats
{
    int32 NumNodes = 0;
    int32 NumEdges = 0;
    float ActualConnectivity = 0.0f;
    float ComputedSpectralRadius = 0.0f;
};

// Mock topology data
struct FReservoirTopologyData
{
    FReservoirTopologyConfig Config;
    FReservoirTopologyStats Stats;
    TArray<float> AdjacencyMatrix;
};

// Mock preset
struct FReservoirTopologyPreset
{
    FString Name;
    FString Description;
    FReservoirTopologyConfig Config;
};

// Mock validation result
struct FReservoirTopologyValidation
{
    bool bIsValid = false;
    FString FailureReason;
    int32 NumNodes = 0;
    int32 NumEdges = 0;
    float MeasuredSpectralRadius = 0.0f;
};

// Mock manager state
struct FReservoirTopologyManagerState
{
    bool bHasActiveTopology = false;
    FString ActivePresetName;
    int32 NumNodes = 0;
    int32 NumPresets = 0;
    int32 ConfigurationChangeCount = 0;
};
#endif

// ============================================================================
// Mock Reservoir Topology Manager for Testing
// ============================================================================

class MockReservoirTopologyManager
{
public:
    int32 MinNodes = 10;
    int32 MaxNodes = 10000;
    float MinSpectralRadius = 0.1f;
    float MaxSpectralRadius = 1.5f;

    bool GenerateAndApply(const FReservoirTopologyConfig& Config)
    {
        FReservoirTopologyData Data = GenerateTopology(Config);
        auto Validation = ValidateTopology(Data);
        if (!Validation.bIsValid)
        {
            return false;
        }
        Commit(Data, "");
        return true;
    }

    bool ApplyTopology(const FReservoirTopologyData& Data)
    {
        auto Validation = ValidateTopology(Data);
        if (!Validation.bIsValid)
        {
            return false;
        }
        Commit(Data, "");
        return true;
    }

    void ClearTopology()
    {
        Active = FReservoirTopologyData();
        ActivePreset.clear();
        bHasActive = false;
        ChangeCount++;
    }

    bool HasActiveTopology() const { return bHasActive; }
    FReservoirTopologyData GetActiveTopology() const { return Active; }

    bool AdjustSpectralRadius(float Target)
    {
        if (!bHasActive)
        {
            return false;
        }
        if (Target < MinSpectralRadius || Target > MaxSpectralRadius)
        {
            return false;
        }
        // Scale weights proportionally to reach target spectral radius
        float Current = Active.Stats.ComputedSpectralRadius;
        if (Current > 0.0f)
        {
            float Factor = Target / Current;
            for (auto& W : Active.AdjacencyMatrix)
            {
                if (W != 0.0f) { W *= Factor; }
            }
        }
        Active.Stats.ComputedSpectralRadius = Target;
        Active.Config.SpectralRadius = Target;
        ChangeCount++;
        return true;
    }

    bool ScaleWeights(float Factor)
    {
        if (!bHasActive || Factor <= 0.0f)
        {
            return false;
        }
        for (auto& W : Active.AdjacencyMatrix)
        {
            if (W != 0.0f) { W *= Factor; }
        }
        Active.Stats.ComputedSpectralRadius *= Factor;
        ChangeCount++;
        return true;
    }

    void RegisterPreset(const FReservoirTopologyPreset& Preset)
    {
        if (!Preset.Name.empty())
        {
            Presets[Preset.Name] = Preset;
        }
    }

    bool UnregisterPreset(const FString& Name)
    {
        return Presets.erase(Name) > 0;
    }

    bool ActivatePreset(const FString& Name)
    {
        auto It = Presets.find(Name);
        if (It == Presets.end())
        {
            return false;
        }
        FReservoirTopologyData Data = GenerateTopology(It->second.Config);
        auto Validation = ValidateTopology(Data);
        if (!Validation.bIsValid)
        {
            return false;
        }
        Commit(Data, Name);
        return true;
    }

    TArray<FString> GetPresetNames() const
    {
        TArray<FString> Names;
        for (const auto& Pair : Presets)
        {
            Names.push_back(Pair.first);
        }
        return Names;
    }

    bool GetPreset(const FString& Name, FReservoirTopologyPreset& Out) const
    {
        auto It = Presets.find(Name);
        if (It == Presets.end())
        {
            return false;
        }
        Out = It->second;
        return true;
    }

    FReservoirTopologyValidation ValidateTopology(const FReservoirTopologyData& Data) const
    {
        FReservoirTopologyValidation R;
        R.NumNodes = Data.Stats.NumNodes;
        R.NumEdges = Data.Stats.NumEdges;
        R.MeasuredSpectralRadius = Data.Stats.ComputedSpectralRadius;

        const int32 N = Data.Stats.NumNodes;
        if (N < MinNodes || N > MaxNodes)
        {
            R.FailureReason = "node_count_out_of_range";
            return R;
        }
        if ((int32)Data.AdjacencyMatrix.size() != N * N)
        {
            R.FailureReason = "adjacency_size_mismatch";
            return R;
        }
        for (int32 i = 0; i < N; i++)
        {
            if (Data.AdjacencyMatrix[i * N + i] != 0.0f)
            {
                R.FailureReason = "self_loop_detected";
                return R;
            }
        }
        float SR = Data.Stats.ComputedSpectralRadius;
        if (SR < MinSpectralRadius || SR > MaxSpectralRadius)
        {
            R.FailureReason = "spectral_radius_out_of_range";
            return R;
        }
        R.bIsValid = true;
        return R;
    }

    FReservoirTopologyManagerState GetManagerState() const
    {
        FReservoirTopologyManagerState S;
        S.bHasActiveTopology = bHasActive;
        S.ActivePresetName = ActivePreset;
        S.NumNodes = bHasActive ? Active.Stats.NumNodes : 0;
        S.NumPresets = (int32)Presets.size();
        S.ConfigurationChangeCount = ChangeCount;
        return S;
    }

private:
    FReservoirTopologyData GenerateTopology(const FReservoirTopologyConfig& Config)
    {
        FReservoirTopologyData Data;
        Data.Config = Config;
        Data.AdjacencyMatrix.assign(Config.NumNodes * Config.NumNodes, 0.0f);

        std::mt19937 rng(Config.RandomSeed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::normal_distribution<float> wdist(0.0f, 1.0f);

        int32 edges = 0;
        for (int32 i = 0; i < Config.NumNodes; i++)
        {
            for (int32 j = 0; j < Config.NumNodes; j++)
            {
                if (i != j && dist(rng) < Config.Connectivity)
                {
                    Data.AdjacencyMatrix[i * Config.NumNodes + j] = wdist(rng);
                    edges++;
                }
            }
        }

        Data.Stats.NumNodes = Config.NumNodes;
        Data.Stats.NumEdges = edges;
        Data.Stats.ActualConnectivity = (float)edges / (Config.NumNodes * (Config.NumNodes - 1));
        // Mock: treat configured spectral radius as the measured one
        Data.Stats.ComputedSpectralRadius = Config.SpectralRadius;
        return Data;
    }

    void Commit(const FReservoirTopologyData& Data, const FString& PresetName)
    {
        Active = Data;
        ActivePreset = PresetName;
        bHasActive = true;
        ChangeCount++;
    }

    FReservoirTopologyData Active;
    std::map<FString, FReservoirTopologyPreset> Presets;
    bool bHasActive = false;
    FString ActivePreset;
    int32 ChangeCount = 0;
};

// ============================================================================
// Test Fixture
// ============================================================================

class ReservoirTopologyManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Manager = std::make_unique<MockReservoirTopologyManager>();
    }

    void TearDown() override
    {
        Manager.reset();
    }

    FReservoirTopologyConfig MakeConfig(int32 Nodes, float Conn, float SR, int32 Seed)
    {
        FReservoirTopologyConfig C;
        C.NumNodes = Nodes;
        C.Connectivity = Conn;
        C.SpectralRadius = SR;
        C.RandomSeed = Seed;
        return C;
    }

    std::unique_ptr<MockReservoirTopologyManager> Manager;
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(ReservoirTopologyManagerTest, GenerateAndApply_Succeeds)
{
    EXPECT_FALSE(Manager->HasActiveTopology());
    bool bOk = Manager->GenerateAndApply(MakeConfig(100, 0.1f, 0.9f, 42));
    EXPECT_TRUE(bOk);
    EXPECT_TRUE(Manager->HasActiveTopology());
    EXPECT_EQ(Manager->GetActiveTopology().Stats.NumNodes, 100);
}

TEST_F(ReservoirTopologyManagerTest, GenerateAndApply_RejectsInvalidConfig)
{
    // Below MinNodes
    bool bOk = Manager->GenerateAndApply(MakeConfig(5, 0.1f, 0.9f, 1));
    EXPECT_FALSE(bOk);
    EXPECT_FALSE(Manager->HasActiveTopology());
}

TEST_F(ReservoirTopologyManagerTest, ApplyTopology_ValidData)
{
    FReservoirTopologyData Data;
    Data.Stats.NumNodes = 50;
    Data.Stats.NumEdges = 200;
    Data.Stats.ComputedSpectralRadius = 0.9f;
    Data.AdjacencyMatrix.assign(50 * 50, 0.0f);
    Data.AdjacencyMatrix[1] = 0.5f; // one off-diagonal edge

    EXPECT_TRUE(Manager->ApplyTopology(Data));
    EXPECT_TRUE(Manager->HasActiveTopology());
}

TEST_F(ReservoirTopologyManagerTest, ClearTopology_ResetsState)
{
    Manager->GenerateAndApply(MakeConfig(50, 0.1f, 0.9f, 42));
    ASSERT_TRUE(Manager->HasActiveTopology());

    Manager->ClearTopology();
    EXPECT_FALSE(Manager->HasActiveTopology());
    EXPECT_EQ(Manager->GetManagerState().NumNodes, 0);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(ReservoirTopologyManagerTest, Validate_RejectsSelfLoops)
{
    FReservoirTopologyData Data;
    Data.Stats.NumNodes = 20;
    Data.Stats.ComputedSpectralRadius = 0.9f;
    Data.AdjacencyMatrix.assign(20 * 20, 0.0f);
    Data.AdjacencyMatrix[0] = 1.0f; // self-loop at node 0

    auto V = Manager->ValidateTopology(Data);
    EXPECT_FALSE(V.bIsValid);
    EXPECT_EQ(V.FailureReason, "self_loop_detected");
}

TEST_F(ReservoirTopologyManagerTest, Validate_RejectsSizeMismatch)
{
    FReservoirTopologyData Data;
    Data.Stats.NumNodes = 20;
    Data.Stats.ComputedSpectralRadius = 0.9f;
    Data.AdjacencyMatrix.assign(10, 0.0f); // wrong size

    auto V = Manager->ValidateTopology(Data);
    EXPECT_FALSE(V.bIsValid);
    EXPECT_EQ(V.FailureReason, "adjacency_size_mismatch");
}

TEST_F(ReservoirTopologyManagerTest, Validate_RejectsSpectralRadiusOutOfRange)
{
    FReservoirTopologyData Data;
    Data.Stats.NumNodes = 20;
    Data.Stats.ComputedSpectralRadius = 5.0f; // > MaxSpectralRadius
    Data.AdjacencyMatrix.assign(20 * 20, 0.0f);

    auto V = Manager->ValidateTopology(Data);
    EXPECT_FALSE(V.bIsValid);
    EXPECT_EQ(V.FailureReason, "spectral_radius_out_of_range");
}

TEST_F(ReservoirTopologyManagerTest, Validate_AcceptsValidTopology)
{
    FReservoirTopologyData Data;
    Data.Stats.NumNodes = 30;
    Data.Stats.NumEdges = 90;
    Data.Stats.ComputedSpectralRadius = 0.9f;
    Data.AdjacencyMatrix.assign(30 * 30, 0.0f);
    Data.AdjacencyMatrix[1] = 0.4f;

    auto V = Manager->ValidateTopology(Data);
    EXPECT_TRUE(V.bIsValid);
    EXPECT_EQ(V.NumNodes, 30);
    EXPECT_EQ(V.NumEdges, 90);
    EXPECT_FLOAT_EQ(V.MeasuredSpectralRadius, 0.9f);
}

// ============================================================================
// Dynamic Adjustment Tests
// ============================================================================

TEST_F(ReservoirTopologyManagerTest, AdjustSpectralRadius_RequiresActiveTopology)
{
    EXPECT_FALSE(Manager->AdjustSpectralRadius(0.9f));
}

TEST_F(ReservoirTopologyManagerTest, AdjustSpectralRadius_Succeeds)
{
    Manager->GenerateAndApply(MakeConfig(50, 0.2f, 0.9f, 42));
    ASSERT_TRUE(Manager->HasActiveTopology());

    EXPECT_TRUE(Manager->AdjustSpectralRadius(1.1f));
    EXPECT_FLOAT_EQ(Manager->GetActiveTopology().Stats.ComputedSpectralRadius, 1.1f);
}

TEST_F(ReservoirTopologyManagerTest, AdjustSpectralRadius_RejectsOutOfRange)
{
    Manager->GenerateAndApply(MakeConfig(50, 0.2f, 0.9f, 42));
    EXPECT_FALSE(Manager->AdjustSpectralRadius(3.0f)); // > MaxSpectralRadius
    EXPECT_FLOAT_EQ(Manager->GetActiveTopology().Stats.ComputedSpectralRadius, 0.9f);
}

TEST_F(ReservoirTopologyManagerTest, ScaleWeights_ScalesNonZeroWeights)
{
    Manager->GenerateAndApply(MakeConfig(30, 0.5f, 0.9f, 7));
    ASSERT_TRUE(Manager->HasActiveTopology());

    float Before = Manager->GetActiveTopology().Stats.ComputedSpectralRadius;
    EXPECT_TRUE(Manager->ScaleWeights(2.0f));
    EXPECT_FLOAT_EQ(Manager->GetActiveTopology().Stats.ComputedSpectralRadius, Before * 2.0f);
}

TEST_F(ReservoirTopologyManagerTest, ScaleWeights_RejectsInvalidFactor)
{
    Manager->GenerateAndApply(MakeConfig(30, 0.5f, 0.9f, 7));
    EXPECT_FALSE(Manager->ScaleWeights(0.0f));
    EXPECT_FALSE(Manager->ScaleWeights(-1.0f));
}

// ============================================================================
// Preset Tests
// ============================================================================

TEST_F(ReservoirTopologyManagerTest, Presets_RegisterAndList)
{
    FReservoirTopologyPreset P1; P1.Name = "small_world"; P1.Config = MakeConfig(100, 0.1f, 0.9f, 1);
    FReservoirTopologyPreset P2; P2.Name = "scale_free";  P2.Config = MakeConfig(100, 0.1f, 0.9f, 2);

    Manager->RegisterPreset(P1);
    Manager->RegisterPreset(P2);

    auto Names = Manager->GetPresetNames();
    EXPECT_EQ(Names.size(), 2u);
    EXPECT_EQ(Manager->GetManagerState().NumPresets, 2);
}

TEST_F(ReservoirTopologyManagerTest, Presets_GetByName)
{
    FReservoirTopologyPreset P; P.Name = "modular"; P.Description = "Modular topology";
    P.Config = MakeConfig(120, 0.15f, 0.95f, 3);
    Manager->RegisterPreset(P);

    FReservoirTopologyPreset Out;
    EXPECT_TRUE(Manager->GetPreset("modular", Out));
    EXPECT_EQ(Out.Description, "Modular topology");
    EXPECT_EQ(Out.Config.NumNodes, 120);

    EXPECT_FALSE(Manager->GetPreset("nonexistent", Out));
}

TEST_F(ReservoirTopologyManagerTest, Presets_Activate)
{
    FReservoirTopologyPreset P; P.Name = "default"; P.Config = MakeConfig(80, 0.1f, 0.9f, 5);
    Manager->RegisterPreset(P);

    EXPECT_TRUE(Manager->ActivatePreset("default"));
    EXPECT_TRUE(Manager->HasActiveTopology());
    EXPECT_EQ(Manager->GetManagerState().ActivePresetName, "default");
    EXPECT_EQ(Manager->GetActiveTopology().Stats.NumNodes, 80);
}

TEST_F(ReservoirTopologyManagerTest, Presets_ActivateUnknownFails)
{
    EXPECT_FALSE(Manager->ActivatePreset("does_not_exist"));
    EXPECT_FALSE(Manager->HasActiveTopology());
}

TEST_F(ReservoirTopologyManagerTest, Presets_Unregister)
{
    FReservoirTopologyPreset P; P.Name = "temp"; P.Config = MakeConfig(50, 0.1f, 0.9f, 1);
    Manager->RegisterPreset(P);
    EXPECT_EQ(Manager->GetManagerState().NumPresets, 1);

    EXPECT_TRUE(Manager->UnregisterPreset("temp"));
    EXPECT_EQ(Manager->GetManagerState().NumPresets, 0);
    EXPECT_FALSE(Manager->UnregisterPreset("temp")); // already removed
}

TEST_F(ReservoirTopologyManagerTest, Presets_IgnoresEmptyName)
{
    FReservoirTopologyPreset P; P.Name = ""; P.Config = MakeConfig(50, 0.1f, 0.9f, 1);
    Manager->RegisterPreset(P);
    EXPECT_EQ(Manager->GetManagerState().NumPresets, 0);
}

// ============================================================================
// State & Change Tracking Tests
// ============================================================================

TEST_F(ReservoirTopologyManagerTest, State_TracksChangeCount)
{
    EXPECT_EQ(Manager->GetManagerState().ConfigurationChangeCount, 0);

    Manager->GenerateAndApply(MakeConfig(50, 0.1f, 0.9f, 1)); // +1
    Manager->AdjustSpectralRadius(1.0f);                       // +1
    Manager->ScaleWeights(1.1f);                               // +1

    EXPECT_EQ(Manager->GetManagerState().ConfigurationChangeCount, 3);
}

TEST_F(ReservoirTopologyManagerTest, State_ReflectsActiveTopology)
{
    Manager->GenerateAndApply(MakeConfig(64, 0.2f, 0.9f, 9));
    auto S = Manager->GetManagerState();

    EXPECT_TRUE(S.bHasActiveTopology);
    EXPECT_EQ(S.NumNodes, 64);
    EXPECT_TRUE(S.ActivePresetName.empty()); // ad-hoc, not a preset
}

TEST_F(ReservoirTopologyManagerTest, Reproducibility_SameSeedSameTopology)
{
    Manager->GenerateAndApply(MakeConfig(50, 0.2f, 0.9f, 123));
    auto A = Manager->GetActiveTopology().AdjacencyMatrix;

    Manager->ClearTopology();
    Manager->GenerateAndApply(MakeConfig(50, 0.2f, 0.9f, 123));
    auto B = Manager->GetActiveTopology().AdjacencyMatrix;

    ASSERT_EQ(A.size(), B.size());
    for (size_t i = 0; i < A.size(); i++)
    {
        EXPECT_FLOAT_EQ(A[i], B[i]);
    }
}

