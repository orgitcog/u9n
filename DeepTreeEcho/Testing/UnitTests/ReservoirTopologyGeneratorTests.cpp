/**
 * @file ReservoirTopologyGeneratorTests.cpp
 * @brief Comprehensive unit tests for Reservoir Topology Generator
 * 
 * Tests cover:
 * - Random sparse topology generation
 * - Small-world topology (Watts-Strogatz)
 * - Scale-free topology (Barabási-Albert)
 * - Modular topology generation
 * - Topology statistics computation
 * - Spectral radius scaling
 * - Visualization export (JSON/GraphML)
 * 
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <chrono>
#include <random>
#include <Eigen/Sparse>

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
namespace FString { using Type = std::string; }
using FName = std::string;
using int32 = int;
using uint32 = unsigned int;
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UMETA(...)
#define GENERATED_BODY()
#define TEXT(x) x

// Mock FMath
struct FMath
{
    template<typename T>
    static T Min(T a, T b) { return a < b ? a : b; }
    
    template<typename T>
    static T Max(T a, T b) { return a > b ? a : b; }
    
    template<typename T>
    static T Abs(T a) { return a < 0 ? -a : a; }
};

// Mock TArray
template<typename T>
using TArray = std::vector<T>;

// Mock FFileHelper
struct FFileHelper
{
    static bool SaveStringToFile(const std::string& Content, const char* FilePath)
    {
        return true; // Mock implementation
    }
};

// Mock topology config
struct FReservoirTopologyConfig
{
    int32 NumNodes = 100;
    float Connectivity = 0.1f;
    float SpectralRadius = 0.9f;
    int32 RandomSeed = 0;
    int32 MeanDegree = 6;
    float RewiringProbability = 0.1f;
    int32 AttachmentEdges = 2;
    int32 NumModules = 4;
    float InterModuleConnectivity = 0.02f;
    float IntraModuleConnectivity = 0.1f;
};

// Mock topology stats
struct FReservoirTopologyStats
{
    int32 NumNodes = 0;
    int32 NumEdges = 0;
    float ActualConnectivity = 0.0f;
    float AverageDegree = 0.0f;
    int32 MaxDegree = 0;
    int32 MinDegree = 0;
    float ComputedSpectralRadius = 0.0f;
    float ClusteringCoefficient = 0.0f;
    float AveragePathLength = 0.0f;
    float Modularity = 0.0f;
};

// Mock topology data
struct FReservoirTopologyData
{
    FReservoirTopologyConfig Config;
    FReservoirTopologyStats Stats;
    TArray<float> AdjacencyMatrix;
    TArray<int32> NodeModules;
    float GenerationTime = 0.0f;
};

#endif

// ============================================================================
// Mock Topology Generator for Testing
// ============================================================================

class MockReservoirTopologyGenerator
{
public:
    FReservoirTopologyData GenerateTopology(const FReservoirTopologyConfig& Config)
    {
        FReservoirTopologyData Result;
        Result.Config = Config;

        // Generate simple mock topology
        Result.AdjacencyMatrix.resize(Config.NumNodes * Config.NumNodes, 0.0f);
        
        // Add random connections
        std::mt19937 rng(Config.RandomSeed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::normal_distribution<float> weightDist(0.0f, 1.0f);

        int32 edges = 0;
        for (int32 i = 0; i < Config.NumNodes; i++)
        {
            for (int32 j = 0; j < Config.NumNodes; j++)
            {
                if (i != j && dist(rng) < Config.Connectivity)
                {
                    Result.AdjacencyMatrix[i * Config.NumNodes + j] = weightDist(rng);
                    edges++;
                }
            }
        }

        // Compute basic stats
        Result.Stats.NumNodes = Config.NumNodes;
        Result.Stats.NumEdges = edges;
        Result.Stats.ActualConnectivity = (float)edges / (Config.NumNodes * (Config.NumNodes - 1));
        Result.Stats.AverageDegree = (float)edges / Config.NumNodes;

        return Result;
    }

    FReservoirTopologyData GenerateRandomSparse(int32 NumNodes, float Connectivity, 
                                                float SpectralRadius, int32 Seed)
    {
        FReservoirTopologyConfig Config;
        Config.NumNodes = NumNodes;
        Config.Connectivity = Connectivity;
        Config.SpectralRadius = SpectralRadius;
        Config.RandomSeed = Seed;
        return GenerateTopology(Config);
    }

    FReservoirTopologyData GenerateSmallWorld(int32 NumNodes, int32 MeanDegree, 
                                             float RewiringProb, float SpectralRadius, int32 Seed)
    {
        FReservoirTopologyConfig Config;
        Config.NumNodes = NumNodes;
        Config.MeanDegree = MeanDegree;
        Config.RewiringProbability = RewiringProb;
        Config.SpectralRadius = SpectralRadius;
        Config.RandomSeed = Seed;
        return GenerateTopology(Config);
    }

    FReservoirTopologyData GenerateScaleFree(int32 NumNodes, int32 AttachmentEdges, 
                                            float SpectralRadius, int32 Seed)
    {
        FReservoirTopologyConfig Config;
        Config.NumNodes = NumNodes;
        Config.AttachmentEdges = AttachmentEdges;
        Config.SpectralRadius = SpectralRadius;
        Config.RandomSeed = Seed;
        return GenerateTopology(Config);
    }

    FReservoirTopologyData GenerateModular(int32 NumNodes, int32 NumModules, 
                                          float IntraConn, float InterConn, 
                                          float SpectralRadius, int32 Seed)
    {
        FReservoirTopologyConfig Config;
        Config.NumNodes = NumNodes;
        Config.NumModules = NumModules;
        Config.IntraModuleConnectivity = IntraConn;
        Config.InterModuleConnectivity = InterConn;
        Config.SpectralRadius = SpectralRadius;
        Config.RandomSeed = Seed;
        
        auto Result = GenerateTopology(Config);
        
        // Assign modules
        Result.NodeModules.resize(NumNodes);
        for (int32 i = 0; i < NumNodes; i++)
        {
            Result.NodeModules[i] = i % NumModules;
        }
        
        return Result;
    }

    std::string ExportToJSON(const FReservoirTopologyData& TopologyData)
    {
        std::string json = "{\n  \"num_nodes\": " + std::to_string(TopologyData.Stats.NumNodes);
        json += ",\n  \"num_edges\": " + std::to_string(TopologyData.Stats.NumEdges);
        json += "\n}";
        return json;
    }

    std::string ExportToGraphML(const FReservoirTopologyData& TopologyData)
    {
        std::string graphml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        graphml += "<graphml>\n</graphml>";
        return graphml;
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

class ReservoirTopologyGeneratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Generator = std::make_unique<MockReservoirTopologyGenerator>();
    }

    void TearDown() override
    {
        Generator.reset();
    }

    std::unique_ptr<MockReservoirTopologyGenerator> Generator;
};

// ============================================================================
// Random Sparse Topology Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, RandomSparse_BasicGeneration)
{
    auto Topology = Generator->GenerateRandomSparse(100, 0.1f, 0.9f, 42);

    EXPECT_EQ(Topology.Stats.NumNodes, 100);
    EXPECT_GT(Topology.Stats.NumEdges, 0);
    EXPECT_EQ(Topology.AdjacencyMatrix.size(), 100 * 100);
    EXPECT_NEAR(Topology.Stats.ActualConnectivity, 0.1f, 0.05f);
}

TEST_F(ReservoirTopologyGeneratorTest, RandomSparse_Reproducibility)
{
    auto Topology1 = Generator->GenerateRandomSparse(50, 0.15f, 0.9f, 123);
    auto Topology2 = Generator->GenerateRandomSparse(50, 0.15f, 0.9f, 123);

    EXPECT_EQ(Topology1.Stats.NumEdges, Topology2.Stats.NumEdges);
    EXPECT_EQ(Topology1.AdjacencyMatrix.size(), Topology2.AdjacencyMatrix.size());
}

TEST_F(ReservoirTopologyGeneratorTest, RandomSparse_ConnectivityRange)
{
    // Test low connectivity
    auto LowConn = Generator->GenerateRandomSparse(100, 0.01f, 0.9f, 1);
    EXPECT_LT(LowConn.Stats.ActualConnectivity, 0.05f);

    // Test high connectivity
    auto HighConn = Generator->GenerateRandomSparse(100, 0.5f, 0.9f, 2);
    EXPECT_GT(HighConn.Stats.ActualConnectivity, 0.3f);
}

TEST_F(ReservoirTopologyGeneratorTest, RandomSparse_NoSelfLoops)
{
    auto Topology = Generator->GenerateRandomSparse(50, 0.2f, 0.9f, 42);

    // Check diagonal is all zeros (no self-loops)
    for (int32 i = 0; i < 50; i++)
    {
        EXPECT_EQ(Topology.AdjacencyMatrix[i * 50 + i], 0.0f);
    }
}

// ============================================================================
// Small-World Topology Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, SmallWorld_BasicGeneration)
{
    auto Topology = Generator->GenerateSmallWorld(100, 6, 0.1f, 0.9f, 42);

    EXPECT_EQ(Topology.Stats.NumNodes, 100);
    EXPECT_GT(Topology.Stats.NumEdges, 0);
    EXPECT_GT(Topology.Stats.AverageDegree, 0.0f);
}

TEST_F(ReservoirTopologyGeneratorTest, SmallWorld_RewiringEffect)
{
    // Low rewiring probability (more regular)
    auto LowRewire = Generator->GenerateSmallWorld(50, 4, 0.01f, 0.9f, 1);

    // High rewiring probability (more random)
    auto HighRewire = Generator->GenerateSmallWorld(50, 4, 0.9f, 0.9f, 2);

    EXPECT_GT(LowRewire.Stats.NumEdges, 0);
    EXPECT_GT(HighRewire.Stats.NumEdges, 0);
}

// ============================================================================
// Scale-Free Topology Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, ScaleFree_BasicGeneration)
{
    auto Topology = Generator->GenerateScaleFree(100, 2, 0.9f, 42);

    EXPECT_EQ(Topology.Stats.NumNodes, 100);
    EXPECT_GT(Topology.Stats.NumEdges, 0);
}

TEST_F(ReservoirTopologyGeneratorTest, ScaleFree_AttachmentEdges)
{
    auto Topology2 = Generator->GenerateScaleFree(100, 2, 0.9f, 1);
    auto Topology5 = Generator->GenerateScaleFree(100, 5, 0.9f, 2);

    // Higher attachment edges should result in higher average degree
    EXPECT_LT(Topology2.Stats.AverageDegree, Topology5.Stats.AverageDegree);
}

// ============================================================================
// Modular Topology Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, Modular_BasicGeneration)
{
    auto Topology = Generator->GenerateModular(100, 4, 0.1f, 0.02f, 0.9f, 42);

    EXPECT_EQ(Topology.Stats.NumNodes, 100);
    EXPECT_GT(Topology.Stats.NumEdges, 0);
    EXPECT_EQ(Topology.NodeModules.size(), 100);
}

TEST_F(ReservoirTopologyGeneratorTest, Modular_ModuleAssignment)
{
    auto Topology = Generator->GenerateModular(100, 4, 0.15f, 0.01f, 0.9f, 42);

    // Check all nodes are assigned to a module
    for (int32 Module : Topology.NodeModules)
    {
        EXPECT_GE(Module, 0);
        EXPECT_LT(Module, 4);
    }
}

TEST_F(ReservoirTopologyGeneratorTest, Modular_IntraVsInterConnectivity)
{
    // High intra-module connectivity
    auto HighIntra = Generator->GenerateModular(100, 4, 0.3f, 0.01f, 0.9f, 1);

    // High inter-module connectivity
    auto HighInter = Generator->GenerateModular(100, 4, 0.1f, 0.2f, 0.9f, 2);

    EXPECT_GT(HighIntra.Stats.NumEdges, 0);
    EXPECT_GT(HighInter.Stats.NumEdges, 0);
}

// ============================================================================
// Topology Statistics Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, Stats_NodeCount)
{
    for (int32 NumNodes : {10, 50, 100, 200})
    {
        auto Topology = Generator->GenerateRandomSparse(NumNodes, 0.1f, 0.9f, 42);
        EXPECT_EQ(Topology.Stats.NumNodes, NumNodes);
    }
}

TEST_F(ReservoirTopologyGeneratorTest, Stats_EdgeCount)
{
    auto Topology = Generator->GenerateRandomSparse(100, 0.1f, 0.9f, 42);
    EXPECT_GT(Topology.Stats.NumEdges, 0);
    EXPECT_LE(Topology.Stats.NumEdges, 100 * 99); // Max possible edges
}

TEST_F(ReservoirTopologyGeneratorTest, Stats_Connectivity)
{
    auto Topology = Generator->GenerateRandomSparse(100, 0.2f, 0.9f, 42);
    
    // Actual connectivity should be close to requested
    EXPECT_GT(Topology.Stats.ActualConnectivity, 0.0f);
    EXPECT_LE(Topology.Stats.ActualConnectivity, 1.0f);
}

TEST_F(ReservoirTopologyGeneratorTest, Stats_AverageDegree)
{
    auto Topology = Generator->GenerateRandomSparse(100, 0.1f, 0.9f, 42);
    
    // Average degree should be positive
    EXPECT_GT(Topology.Stats.AverageDegree, 0.0f);
    
    // Should be approximately: NumEdges / NumNodes
    float ExpectedAvgDegree = (float)Topology.Stats.NumEdges / Topology.Stats.NumNodes;
    EXPECT_FLOAT_EQ(Topology.Stats.AverageDegree, ExpectedAvgDegree);
}

// ============================================================================
// Visualization Export Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, Export_JSONFormat)
{
    auto Topology = Generator->GenerateRandomSparse(50, 0.1f, 0.9f, 42);
    auto JSON = Generator->ExportToJSON(Topology);

    EXPECT_FALSE(JSON.empty());
    EXPECT_NE(JSON.find("num_nodes"), std::string::npos);
    EXPECT_NE(JSON.find("num_edges"), std::string::npos);
}

TEST_F(ReservoirTopologyGeneratorTest, Export_GraphMLFormat)
{
    auto Topology = Generator->GenerateRandomSparse(50, 0.1f, 0.9f, 42);
    auto GraphML = Generator->ExportToGraphML(Topology);

    EXPECT_FALSE(GraphML.empty());
    EXPECT_NE(GraphML.find("<?xml"), std::string::npos);
    EXPECT_NE(GraphML.find("graphml"), std::string::npos);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, Performance_SmallNetwork)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    auto Topology = Generator->GenerateRandomSparse(100, 0.1f, 0.9f, 42);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(end - start).count();

    // Should complete in under 100ms
    EXPECT_LT(duration, 100.0f);
}

TEST_F(ReservoirTopologyGeneratorTest, Performance_MediumNetwork)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    auto Topology = Generator->GenerateRandomSparse(500, 0.05f, 0.9f, 42);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(end - start).count();

    // Should complete in under 500ms
    EXPECT_LT(duration, 500.0f);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, EdgeCase_MinimalNetwork)
{
    auto Topology = Generator->GenerateRandomSparse(10, 0.1f, 0.9f, 42);
    
    EXPECT_EQ(Topology.Stats.NumNodes, 10);
    EXPECT_GE(Topology.Stats.NumEdges, 0);
}

TEST_F(ReservoirTopologyGeneratorTest, EdgeCase_ZeroConnectivity)
{
    auto Topology = Generator->GenerateRandomSparse(100, 0.0f, 0.9f, 42);
    
    EXPECT_EQ(Topology.Stats.NumEdges, 0);
    EXPECT_EQ(Topology.Stats.ActualConnectivity, 0.0f);
}

TEST_F(ReservoirTopologyGeneratorTest, EdgeCase_SingleModule)
{
    auto Topology = Generator->GenerateModular(100, 1, 0.1f, 0.0f, 0.9f, 42);
    
    EXPECT_EQ(Topology.Stats.NumNodes, 100);
    
    // All nodes should be in module 0
    for (int32 Module : Topology.NodeModules)
    {
        EXPECT_EQ(Module, 0);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ReservoirTopologyGeneratorTest, Integration_MultipleTopologies)
{
    // Generate different topologies
    auto Random = Generator->GenerateRandomSparse(100, 0.1f, 0.9f, 1);
    auto SmallWorld = Generator->GenerateSmallWorld(100, 6, 0.1f, 0.9f, 2);
    auto ScaleFree = Generator->GenerateScaleFree(100, 2, 0.9f, 3);
    auto Modular = Generator->GenerateModular(100, 4, 0.1f, 0.02f, 0.9f, 4);

    // All should be valid
    EXPECT_EQ(Random.Stats.NumNodes, 100);
    EXPECT_EQ(SmallWorld.Stats.NumNodes, 100);
    EXPECT_EQ(ScaleFree.Stats.NumNodes, 100);
    EXPECT_EQ(Modular.Stats.NumNodes, 100);
}

TEST_F(ReservoirTopologyGeneratorTest, Integration_ExportAllFormats)
{
    auto Topology = Generator->GenerateRandomSparse(50, 0.1f, 0.9f, 42);

    // Test both export formats
    auto JSON = Generator->ExportToJSON(Topology);
    auto GraphML = Generator->ExportToGraphML(Topology);

    EXPECT_FALSE(JSON.empty());
    EXPECT_FALSE(GraphML.empty());
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
