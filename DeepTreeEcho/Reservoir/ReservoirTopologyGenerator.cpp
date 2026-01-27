/**
 * @file ReservoirTopologyGenerator.cpp
 * @brief Implementation of Reservoir Topology Generator
 * 
 * Generates various network topologies for reservoir computing including:
 * - Random sparse connectivity
 * - Small-world networks (Watts-Strogatz algorithm)
 * - Scale-free networks (Barabási-Albert algorithm)
 * - Modular architectures with community structures
 * 
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include "ReservoirTopologyGenerator.h"
#include <random>
#include <algorithm>
#include <queue>
#include <set>
#include <cmath>
#include <chrono>
#include <Eigen/Eigenvalues>

UReservoirTopologyGenerator::UReservoirTopologyGenerator()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UReservoirTopologyGenerator::BeginPlay()
{
    Super::BeginPlay();
}

// ========================================
// PUBLIC API - TOPOLOGY GENERATION
// ========================================

FReservoirTopologyData UReservoirTopologyGenerator::GenerateTopology(const FReservoirTopologyConfig& Config)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    FReservoirTopologyData Result;
    Result.Config = Config;

    // Initialize RNG
    InitializeRNG(Config.RandomSeed);

    // Generate topology based on type
    Eigen::SparseMatrix<float> Matrix;
    TArray<int32> Modules;

    switch (Config.TopologyType)
    {
    case EReservoirTopologyType::RandomSparse:
        Matrix = GenerateRandomSparseSparse(Config.NumNodes, Config.Connectivity, Config.RandomSeed);
        break;

    case EReservoirTopologyType::SmallWorld:
        Matrix = GenerateRingLattice(Config.NumNodes, Config.MeanDegree);
        RewireEdges(Matrix, Config.RewiringProbability, Config.RandomSeed);
        break;

    case EReservoirTopologyType::ScaleFree:
        Matrix = GeneratePreferentialAttachment(Config.NumNodes, Config.AttachmentEdges, Config.RandomSeed);
        break;

    case EReservoirTopologyType::Modular:
        Matrix = GenerateModularStructure(Config.NumNodes, Config.NumModules, 
                                         Config.IntraModuleConnectivity, 
                                         Config.InterModuleConnectivity, 
                                         Modules, Config.RandomSeed);
        Result.NodeModules = Modules;
        break;

    case EReservoirTopologyType::Ring:
        Matrix = GenerateRingLattice(Config.NumNodes, 2); // Simple ring
        break;

    case EReservoirTopologyType::FullyConnected:
        {
            Matrix.resize(Config.NumNodes, Config.NumNodes);
            for (int32 i = 0; i < Config.NumNodes; i++)
            {
                for (int32 j = 0; j < Config.NumNodes; j++)
                {
                    if (i != j)
                    {
                        Matrix.insert(i, j) = RandomWeight();
                    }
                }
            }
        }
        break;
    }

    // Scale to target spectral radius
    if (Config.SpectralRadius > 0.0f)
    {
        float CurrentSR = ComputeSpectralRadiusInternal(Matrix);
        if (CurrentSR > 0.0f)
        {
            ScaleMatrix(Matrix, CurrentSR, Config.SpectralRadius);
        }
    }

    // Convert to array format
    Result.AdjacencyMatrix = EigenSparseToArray(Matrix);

    // Compute statistics
    Result.Stats = ComputeTopologyStats(Result.AdjacencyMatrix, Config.NumNodes);

    auto EndTime = std::chrono::high_resolution_clock::now();
    Result.GenerationTime = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();

    LastTopology = Result;

    return Result;
}

FReservoirTopologyData UReservoirTopologyGenerator::GenerateRandomSparse(
    int32 NumNodes, float Connectivity, float SpectralRadius, int32 Seed)
{
    FReservoirTopologyConfig Config;
    Config.TopologyType = EReservoirTopologyType::RandomSparse;
    Config.NumNodes = NumNodes;
    Config.Connectivity = Connectivity;
    Config.SpectralRadius = SpectralRadius;
    Config.RandomSeed = Seed;
    return GenerateTopology(Config);
}

FReservoirTopologyData UReservoirTopologyGenerator::GenerateSmallWorld(
    int32 NumNodes, int32 MeanDegree, float RewiringProb, float SpectralRadius, int32 Seed)
{
    FReservoirTopologyConfig Config;
    Config.TopologyType = EReservoirTopologyType::SmallWorld;
    Config.NumNodes = NumNodes;
    Config.MeanDegree = MeanDegree;
    Config.RewiringProbability = RewiringProb;
    Config.SpectralRadius = SpectralRadius;
    Config.RandomSeed = Seed;
    return GenerateTopology(Config);
}

FReservoirTopologyData UReservoirTopologyGenerator::GenerateScaleFree(
    int32 NumNodes, int32 AttachmentEdges, float SpectralRadius, int32 Seed)
{
    FReservoirTopologyConfig Config;
    Config.TopologyType = EReservoirTopologyType::ScaleFree;
    Config.NumNodes = NumNodes;
    Config.AttachmentEdges = AttachmentEdges;
    Config.SpectralRadius = SpectralRadius;
    Config.RandomSeed = Seed;
    return GenerateTopology(Config);
}

FReservoirTopologyData UReservoirTopologyGenerator::GenerateModular(
    int32 NumNodes, int32 NumModules, float IntraConn, float InterConn, float SpectralRadius, int32 Seed)
{
    FReservoirTopologyConfig Config;
    Config.TopologyType = EReservoirTopologyType::Modular;
    Config.NumNodes = NumNodes;
    Config.NumModules = NumModules;
    Config.IntraModuleConnectivity = IntraConn;
    Config.InterModuleConnectivity = InterConn;
    Config.SpectralRadius = SpectralRadius;
    Config.RandomSeed = Seed;
    return GenerateTopology(Config);
}

// ========================================
// PUBLIC API - TOPOLOGY ANALYSIS
// ========================================

FReservoirTopologyStats UReservoirTopologyGenerator::ComputeTopologyStats(
    const TArray<float>& AdjacencyMatrix, int32 NumNodes)
{
    FReservoirTopologyStats Stats;
    Stats.NumNodes = NumNodes;

    if (AdjacencyMatrix.Num() != NumNodes * NumNodes)
    {
        return Stats;
    }

    // Convert to Eigen for analysis
    Eigen::SparseMatrix<float> Matrix = ArrayToEigenSparse(AdjacencyMatrix, NumNodes);

    // Count edges and compute degree distribution
    TArray<int32> InDegrees, OutDegrees;
    ComputeDegreeDistribution(Matrix, InDegrees, OutDegrees);

    Stats.NumEdges = 0;
    for (int32 i = 0; i < NumNodes; i++)
    {
        Stats.NumEdges += OutDegrees[i];
    }

    // Connectivity
    int32 MaxPossibleEdges = NumNodes * (NumNodes - 1);
    Stats.ActualConnectivity = MaxPossibleEdges > 0 ? (float)Stats.NumEdges / MaxPossibleEdges : 0.0f;

    // Degree statistics
    if (NumNodes > 0)
    {
        int32 TotalDegree = 0;
        Stats.MinDegree = OutDegrees[0];
        Stats.MaxDegree = OutDegrees[0];

        for (int32 Degree : OutDegrees)
        {
            TotalDegree += Degree;
            Stats.MinDegree = FMath::Min(Stats.MinDegree, Degree);
            Stats.MaxDegree = FMath::Max(Stats.MaxDegree, Degree);
        }

        Stats.AverageDegree = (float)TotalDegree / NumNodes;
    }

    // Spectral radius
    Stats.ComputedSpectralRadius = ComputeSpectralRadiusInternal(Matrix);

    // Clustering coefficient
    Stats.ClusteringCoefficient = ComputeClusteringCoefficient(Matrix);

    // Average path length
    Stats.AveragePathLength = ComputeAveragePathLength(Matrix);

    return Stats;
}

float UReservoirTopologyGenerator::ComputeSpectralRadius(const TArray<float>& AdjacencyMatrix, int32 NumNodes)
{
    Eigen::SparseMatrix<float> Matrix = ArrayToEigenSparse(AdjacencyMatrix, NumNodes);
    return ComputeSpectralRadiusInternal(Matrix);
}

TArray<float> UReservoirTopologyGenerator::ScaleToSpectralRadius(
    const TArray<float>& AdjacencyMatrix, int32 NumNodes, float TargetSpectralRadius)
{
    Eigen::SparseMatrix<float> Matrix = ArrayToEigenSparse(AdjacencyMatrix, NumNodes);
    float CurrentSR = ComputeSpectralRadiusInternal(Matrix);
    
    if (CurrentSR > 0.0f)
    {
        ScaleMatrix(Matrix, CurrentSR, TargetSpectralRadius);
    }

    return EigenSparseToArray(Matrix);
}

// ========================================
// PUBLIC API - VISUALIZATION EXPORT
// ========================================

FString UReservoirTopologyGenerator::ExportToJSON(const FReservoirTopologyData& TopologyData)
{
    FString JSON = TEXT("{\n");
    
    // Metadata
    JSON += TEXT("  \"topology_type\": \"");
    switch (TopologyData.Config.TopologyType)
    {
    case EReservoirTopologyType::RandomSparse: JSON += TEXT("random_sparse"); break;
    case EReservoirTopologyType::SmallWorld: JSON += TEXT("small_world"); break;
    case EReservoirTopologyType::ScaleFree: JSON += TEXT("scale_free"); break;
    case EReservoirTopologyType::Modular: JSON += TEXT("modular"); break;
    case EReservoirTopologyType::Ring: JSON += TEXT("ring"); break;
    case EReservoirTopologyType::FullyConnected: JSON += TEXT("fully_connected"); break;
    }
    JSON += TEXT("\",\n");

    // Statistics
    JSON += FString::Printf(TEXT("  \"num_nodes\": %d,\n"), TopologyData.Stats.NumNodes);
    JSON += FString::Printf(TEXT("  \"num_edges\": %d,\n"), TopologyData.Stats.NumEdges);
    JSON += FString::Printf(TEXT("  \"connectivity\": %.6f,\n"), TopologyData.Stats.ActualConnectivity);
    JSON += FString::Printf(TEXT("  \"spectral_radius\": %.6f,\n"), TopologyData.Stats.ComputedSpectralRadius);
    JSON += FString::Printf(TEXT("  \"clustering_coefficient\": %.6f,\n"), TopologyData.Stats.ClusteringCoefficient);
    JSON += FString::Printf(TEXT("  \"generation_time_ms\": %.2f,\n"), TopologyData.GenerationTime);

    // Nodes
    JSON += TEXT("  \"nodes\": [\n");
    for (int32 i = 0; i < TopologyData.Stats.NumNodes; i++)
    {
        JSON += TEXT("    {");
        JSON += FString::Printf(TEXT("\"id\": %d"), i);
        if (TopologyData.NodeModules.Num() > i)
        {
            JSON += FString::Printf(TEXT(", \"module\": %d"), TopologyData.NodeModules[i]);
        }
        JSON += TEXT("}");
        if (i < TopologyData.Stats.NumNodes - 1) JSON += TEXT(",");
        JSON += TEXT("\n");
    }
    JSON += TEXT("  ],\n");

    // Edges
    JSON += TEXT("  \"edges\": [\n");
    bool FirstEdge = true;
    for (int32 i = 0; i < TopologyData.Stats.NumNodes; i++)
    {
        for (int32 j = 0; j < TopologyData.Stats.NumNodes; j++)
        {
            int32 Index = i * TopologyData.Stats.NumNodes + j;
            if (Index < TopologyData.AdjacencyMatrix.Num() && TopologyData.AdjacencyMatrix[Index] != 0.0f)
            {
                if (!FirstEdge) JSON += TEXT(",\n");
                JSON += TEXT("    {");
                JSON += FString::Printf(TEXT("\"source\": %d, \"target\": %d, \"weight\": %.6f"), 
                                       i, j, TopologyData.AdjacencyMatrix[Index]);
                JSON += TEXT("}");
                FirstEdge = false;
            }
        }
    }
    JSON += TEXT("\n  ]\n");
    JSON += TEXT("}\n");

    return JSON;
}

FString UReservoirTopologyGenerator::ExportToGraphML(const FReservoirTopologyData& TopologyData)
{
    FString GraphML = TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    GraphML += TEXT("<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n");
    GraphML += TEXT("  <key id=\"weight\" for=\"edge\" attr.name=\"weight\" attr.type=\"double\"/>\n");
    GraphML += TEXT("  <key id=\"module\" for=\"node\" attr.name=\"module\" attr.type=\"int\"/>\n");
    GraphML += TEXT("  <graph id=\"reservoir\" edgedefault=\"directed\">\n");

    // Nodes
    for (int32 i = 0; i < TopologyData.Stats.NumNodes; i++)
    {
        GraphML += FString::Printf(TEXT("    <node id=\"n%d\">\n"), i);
        if (TopologyData.NodeModules.Num() > i)
        {
            GraphML += FString::Printf(TEXT("      <data key=\"module\">%d</data>\n"), TopologyData.NodeModules[i]);
        }
        GraphML += TEXT("    </node>\n");
    }

    // Edges
    int32 EdgeID = 0;
    for (int32 i = 0; i < TopologyData.Stats.NumNodes; i++)
    {
        for (int32 j = 0; j < TopologyData.Stats.NumNodes; j++)
        {
            int32 Index = i * TopologyData.Stats.NumNodes + j;
            if (Index < TopologyData.AdjacencyMatrix.Num() && TopologyData.AdjacencyMatrix[Index] != 0.0f)
            {
                GraphML += FString::Printf(TEXT("    <edge id=\"e%d\" source=\"n%d\" target=\"n%d\">\n"), EdgeID++, i, j);
                GraphML += FString::Printf(TEXT("      <data key=\"weight\">%.6f</data>\n"), TopologyData.AdjacencyMatrix[Index]);
                GraphML += TEXT("    </edge>\n");
            }
        }
    }

    GraphML += TEXT("  </graph>\n");
    GraphML += TEXT("</graphml>\n");

    return GraphML;
}

bool UReservoirTopologyGenerator::SaveTopologyToFile(
    const FReservoirTopologyData& TopologyData, const FString& FilePath, bool bUseJSON)
{
    FString Content = bUseJSON ? ExportToJSON(TopologyData) : ExportToGraphML(TopologyData);
    return FFileHelper::SaveStringToFile(Content, *FilePath);
}

// ========================================
// PUBLIC API - CONVERSION
// ========================================

TArray<float> UReservoirTopologyGenerator::EigenSparseToArray(const Eigen::SparseMatrix<float>& SparseMatrix)
{
    TArray<float> Result;
    int32 Rows = SparseMatrix.rows();
    int32 Cols = SparseMatrix.cols();
    Result.SetNumZeroed(Rows * Cols);

    for (int32 k = 0; k < SparseMatrix.outerSize(); ++k)
    {
        for (Eigen::SparseMatrix<float>::InnerIterator it(SparseMatrix, k); it; ++it)
        {
            int32 Row = it.row();
            int32 Col = it.col();
            Result[Row * Cols + Col] = it.value();
        }
    }

    return Result;
}

Eigen::SparseMatrix<float> UReservoirTopologyGenerator::ArrayToEigenSparse(
    const TArray<float>& Array, int32 NumNodes)
{
    Eigen::SparseMatrix<float> Matrix(NumNodes, NumNodes);
    
    // Use triplet list for efficient sparse matrix construction
    std::vector<Eigen::Triplet<float>> Triplets;
    Triplets.reserve(Array.Num() / 10); // Estimate: ~10% non-zero
    
    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 0; j < NumNodes; j++)
        {
            int32 Index = i * NumNodes + j;
            if (Index < Array.Num() && Array[Index] != 0.0f)
            {
                Triplets.push_back(Eigen::Triplet<float>(i, j, Array[Index]));
            }
        }
    }

    Matrix.setFromTriplets(Triplets.begin(), Triplets.end());
    return Matrix;
}

// ========================================
// INTERNAL GENERATION METHODS
// ========================================

Eigen::SparseMatrix<float> UReservoirTopologyGenerator::GenerateRandomSparseSparse(
    int32 NumNodes, float Connectivity, int32 Seed)
{
    Eigen::SparseMatrix<float> Matrix(NumNodes, NumNodes);
    
    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 0; j < NumNodes; j++)
        {
            if (i != j && RandomFloat() < Connectivity)
            {
                Matrix.insert(i, j) = RandomWeight();
            }
        }
    }

    return Matrix;
}

Eigen::SparseMatrix<float> UReservoirTopologyGenerator::GenerateRingLattice(
    int32 NumNodes, int32 MeanDegree)
{
    Eigen::SparseMatrix<float> Matrix(NumNodes, NumNodes);
    int32 HalfK = MeanDegree / 2;

    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 1; j <= HalfK; j++)
        {
            int32 Target = (i + j) % NumNodes;
            Matrix.insert(i, Target) = RandomWeight();
            
            // Bidirectional connection
            int32 TargetRev = (i - j + NumNodes) % NumNodes;
            Matrix.insert(i, TargetRev) = RandomWeight();
        }
    }

    return Matrix;
}

void UReservoirTopologyGenerator::RewireEdges(
    Eigen::SparseMatrix<float>& Matrix, float RewiringProb, int32 Seed)
{
    int32 NumNodes = Matrix.rows();
    std::vector<std::tuple<int32, int32, float>> Edges;

    // Collect all edges
    for (int32 k = 0; k < Matrix.outerSize(); ++k)
    {
        for (Eigen::SparseMatrix<float>::InnerIterator it(Matrix, k); it; ++it)
        {
            Edges.push_back(std::make_tuple(it.row(), it.col(), it.value()));
        }
    }

    // Rewire edges
    for (auto& Edge : Edges)
    {
        if (RandomFloat() < RewiringProb)
        {
            int32 Source = std::get<0>(Edge);
            int32 NewTarget = RandomInt(NumNodes);
            
            // Avoid self-loops
            while (NewTarget == Source)
            {
                NewTarget = RandomInt(NumNodes);
            }

            std::get<1>(Edge) = NewTarget;
        }
    }

    // Rebuild matrix
    Matrix.setZero();
    for (const auto& Edge : Edges)
    {
        Matrix.insert(std::get<0>(Edge), std::get<1>(Edge)) = std::get<2>(Edge);
    }
}

Eigen::SparseMatrix<float> UReservoirTopologyGenerator::GeneratePreferentialAttachment(
    int32 NumNodes, int32 AttachmentEdges, int32 Seed)
{
    Eigen::SparseMatrix<float> Matrix(NumNodes, NumNodes);
    TArray<int32> Degrees;
    Degrees.SetNumZeroed(NumNodes);

    // Start with a small fully connected graph
    int32 InitialNodes = FMath::Min(AttachmentEdges + 1, NumNodes);
    for (int32 i = 0; i < InitialNodes; i++)
    {
        for (int32 j = 0; j < InitialNodes; j++)
        {
            if (i != j)
            {
                Matrix.insert(i, j) = RandomWeight();
                Degrees[j]++;
            }
        }
    }

    // Add remaining nodes with preferential attachment
    for (int32 i = InitialNodes; i < NumNodes; i++)
    {
        // Compute total degree for probability calculation
        int32 TotalDegree = 0;
        for (int32 k = 0; k < i; k++)
        {
            TotalDegree += Degrees[k] + 1; // +1 to avoid zero probabilities
        }

        // Attach to m nodes using preferential attachment
        TArray<int32> Targets;
        for (int32 m = 0; m < AttachmentEdges && Targets.Num() < i; m++)
        {
            // Select target based on degree probability
            float RandValue = RandomFloat() * TotalDegree;
            float CumulativeProb = 0.0f;
            int32 SelectedTarget = 0;

            for (int32 k = 0; k < i; k++)
            {
                if (Targets.Contains(k)) continue; // Skip already selected targets

                CumulativeProb += (Degrees[k] + 1);
                if (RandValue < CumulativeProb)
                {
                    SelectedTarget = k;
                    break;
                }
            }

            Targets.Add(SelectedTarget);
            Matrix.insert(i, SelectedTarget) = RandomWeight();
            Degrees[SelectedTarget]++;
        }
    }

    return Matrix;
}

Eigen::SparseMatrix<float> UReservoirTopologyGenerator::GenerateModularStructure(
    int32 NumNodes, int32 NumModules, float IntraConn, float InterConn, 
    TArray<int32>& NodeModules, int32 Seed)
{
    Eigen::SparseMatrix<float> Matrix(NumNodes, NumNodes);
    NodeModules.SetNumZeroed(NumNodes);

    // Assign nodes to modules
    int32 NodesPerModule = NumNodes / NumModules;
    int32 Remainder = NumNodes % NumModules;

    int32 NodeIndex = 0;
    for (int32 m = 0; m < NumModules; m++)
    {
        int32 ModuleSize = NodesPerModule + (m < Remainder ? 1 : 0);
        for (int32 i = 0; i < ModuleSize; i++)
        {
            NodeModules[NodeIndex++] = m;
        }
    }

    // Generate connections
    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 0; j < NumNodes; j++)
        {
            if (i == j) continue;

            bool SameModule = (NodeModules[i] == NodeModules[j]);
            float ConnProb = SameModule ? IntraConn : InterConn;

            if (RandomFloat() < ConnProb)
            {
                Matrix.insert(i, j) = RandomWeight();
            }
        }
    }

    return Matrix;
}

// ========================================
// INTERNAL ANALYSIS METHODS
// ========================================

void UReservoirTopologyGenerator::ComputeDegreeDistribution(
    const Eigen::SparseMatrix<float>& Matrix, TArray<int32>& InDegrees, TArray<int32>& OutDegrees)
{
    int32 NumNodes = Matrix.rows();
    InDegrees.SetNumZeroed(NumNodes);
    OutDegrees.SetNumZeroed(NumNodes);

    for (int32 k = 0; k < Matrix.outerSize(); ++k)
    {
        for (Eigen::SparseMatrix<float>::InnerIterator it(Matrix, k); it; ++it)
        {
            OutDegrees[it.row()]++;
            InDegrees[it.col()]++;
        }
    }
}

float UReservoirTopologyGenerator::ComputeClusteringCoefficient(const Eigen::SparseMatrix<float>& Matrix)
{
    int32 NumNodes = Matrix.rows();
    float TotalCoefficient = 0.0f;
    int32 ValidNodes = 0;

    for (int32 i = 0; i < NumNodes; i++)
    {
        // Get neighbors
        TArray<int32> Neighbors;
        for (int32 j = 0; j < NumNodes; j++)
        {
            if (Matrix.coeff(i, j) != 0.0f)
            {
                Neighbors.Add(j);
            }
        }

        int32 k = Neighbors.Num();
        if (k < 2) continue;

        // Count connections between neighbors
        int32 Connections = 0;
        for (int32 n1 = 0; n1 < k; n1++)
        {
            for (int32 n2 = n1 + 1; n2 < k; n2++)
            {
                if (Matrix.coeff(Neighbors[n1], Neighbors[n2]) != 0.0f)
                {
                    Connections++;
                }
            }
        }

        // Clustering coefficient for this node
        float MaxConnections = k * (k - 1) / 2.0f;
        if (MaxConnections > 0)
        {
            TotalCoefficient += Connections / MaxConnections;
            ValidNodes++;
        }
    }

    return ValidNodes > 0 ? TotalCoefficient / ValidNodes : 0.0f;
}

float UReservoirTopologyGenerator::ComputeAveragePathLength(const Eigen::SparseMatrix<float>& Matrix)
{
    int32 NumNodes = Matrix.rows();
    float TotalPathLength = 0.0f;
    int32 PathCount = 0;

    // Note: This uses BFS from each node, resulting in O(N × (N + E)) complexity
    // For large networks (>1000 nodes), this can be slow
    // Consider using sampling or approximation methods for very large networks

    // BFS from each node
    for (int32 Start = 0; Start < NumNodes; Start++)
    {
        TArray<int32> Distances;
        Distances.Init(-1, NumNodes);
        Distances[Start] = 0;

        std::queue<int32> Queue;
        Queue.push(Start);

        while (!Queue.empty())
        {
            int32 Current = Queue.front();
            Queue.pop();

            for (int32 j = 0; j < NumNodes; j++)
            {
                if (Matrix.coeff(Current, j) != 0.0f && Distances[j] == -1)
                {
                    Distances[j] = Distances[Current] + 1;
                    Queue.push(j);
                    TotalPathLength += Distances[j];
                    PathCount++;
                }
            }
        }
    }

    // Note: This only considers connected components
    // Paths to unreachable nodes are not included in the average
    return PathCount > 0 ? TotalPathLength / PathCount : 0.0f;
}

float UReservoirTopologyGenerator::ComputeModularity(
    const Eigen::SparseMatrix<float>& Matrix, const TArray<int32>& NodeModules)
{
    if (NodeModules.Num() == 0) return 0.0f;

    int32 NumNodes = Matrix.rows();
    int32 TotalEdges = Matrix.nonZeros();
    if (TotalEdges == 0) return 0.0f;

    // Precompute degrees for all nodes to avoid O(N³) complexity
    TArray<int32> OutDegrees;
    OutDegrees.SetNumZeroed(NumNodes);
    
    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 0; j < NumNodes; j++)
        {
            if (Matrix.coeff(i, j) != 0.0f)
            {
                OutDegrees[i]++;
            }
        }
    }

    float Modularity = 0.0f;

    for (int32 i = 0; i < NumNodes; i++)
    {
        for (int32 j = 0; j < NumNodes; j++)
        {
            if (NodeModules[i] == NodeModules[j])
            {
                float Aij = Matrix.coeff(i, j);
                
                // Use precomputed degrees
                int32 ki = OutDegrees[i];
                int32 kj = OutDegrees[j];
                
                float Expected = (float)(ki * kj) / (2.0f * TotalEdges);
                Modularity += (Aij - Expected);
            }
        }
    }

    return Modularity / (2.0f * TotalEdges);
}

float UReservoirTopologyGenerator::ComputeSpectralRadiusInternal(const Eigen::SparseMatrix<float>& Matrix)
{
    if (Matrix.rows() == 0) return 0.0f;

    // Use power iteration for sparse matrices
    int32 N = Matrix.rows();
    Eigen::VectorXf x = Eigen::VectorXf::Random(N);
    x.normalize();

    float Lambda = 0.0f;
    int32 MaxIterations = 100;
    float Tolerance = 1e-6f;
    bool Converged = false;

    for (int32 iter = 0; iter < MaxIterations; iter++)
    {
        Eigen::VectorXf y = Matrix * x;
        float NewLambda = y.norm();
        
        if (FMath::Abs(NewLambda - Lambda) < Tolerance)
        {
            Lambda = NewLambda;
            Converged = true;
            break;
        }

        Lambda = NewLambda;
        x = y / Lambda;
    }

    // Warn if not converged
    if (!Converged)
    {
        UE_LOG(LogTemp, Warning, TEXT("Spectral radius computation did not converge after %d iterations (current estimate: %f)"),
            MaxIterations, Lambda);
    }

    return Lambda;
}

void UReservoirTopologyGenerator::ScaleMatrix(
    Eigen::SparseMatrix<float>& Matrix, float CurrentSR, float TargetSR)
{
    if (CurrentSR > 0.0f)
    {
        float ScaleFactor = TargetSR / CurrentSR;
        Matrix *= ScaleFactor;
    }
}

// ========================================
// INTERNAL UTILITY METHODS
// ========================================

void UReservoirTopologyGenerator::InitializeRNG(int32 Seed)
{
    if (Seed == 0)
    {
        std::random_device rd;
        RNG.seed(rd());
    }
    else
    {
        RNG.seed(Seed);
    }
}

float UReservoirTopologyGenerator::RandomFloat()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(RNG);
}

int32 UReservoirTopologyGenerator::RandomInt(int32 Max)
{
    std::uniform_int_distribution<int32> dist(0, Max - 1);
    return dist(RNG);
}

float UReservoirTopologyGenerator::RandomWeight()
{
    std::normal_distribution<float> dist(0.0f, 1.0f);
    return dist(RNG);
}

bool UReservoirTopologyGenerator::ValidateMatrix(const Eigen::SparseMatrix<float>& Matrix, int32 ExpectedSize)
{
    return Matrix.rows() == ExpectedSize && Matrix.cols() == ExpectedSize;
}
