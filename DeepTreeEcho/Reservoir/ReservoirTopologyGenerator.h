#pragma once

/**
 * Reservoir Topology Generator
 * 
 * Generates various reservoir network topologies for Echo State Networks:
 * - Random sparse connectivity
 * - Small-world networks (Watts-Strogatz)
 * - Scale-free networks (Barabási-Albert)
 * - Modular architectures
 * - Topology visualization export
 * 
 * Based on:
 * - Watts-Strogatz small-world model
 * - Barabási-Albert preferential attachment
 * - Newman-Girvan modularity algorithms
 * - Graph theory for reservoir computing
 * 
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include <Eigen/Sparse>
#include "ReservoirTopologyGenerator.generated.h"

/**
 * Topology Type - Type of reservoir connectivity pattern
 */
UENUM(BlueprintType)
enum class EReservoirTopologyType : uint8
{
    RandomSparse UMETA(DisplayName = "Random Sparse"),
    SmallWorld UMETA(DisplayName = "Small World"),
    ScaleFree UMETA(DisplayName = "Scale Free"),
    Modular UMETA(DisplayName = "Modular"),
    Ring UMETA(DisplayName = "Ring"),
    FullyConnected UMETA(DisplayName = "Fully Connected")
};

/**
 * Topology Configuration - Parameters for topology generation
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyConfig
{
    GENERATED_BODY()

    /** Topology type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology")
    EReservoirTopologyType TopologyType = EReservoirTopologyType::RandomSparse;

    /** Number of nodes/units in reservoir */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (ClampMin = "10", ClampMax = "10000"))
    int32 NumNodes = 100;

    /** Connectivity probability (0.0 - 1.0) for random sparse */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Connectivity = 0.1f;

    /** Spectral radius for weight scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float SpectralRadius = 0.9f;

    /** Random seed for reproducibility (0 = random) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology")
    int32 RandomSeed = 0;

    // Small-World specific parameters
    /** Mean degree for small-world networks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|SmallWorld", meta = (ClampMin = "2", ClampMax = "100"))
    int32 MeanDegree = 6;

    /** Rewiring probability for small-world */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|SmallWorld", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RewiringProbability = 0.1f;

    // Scale-Free specific parameters
    /** Number of edges to attach for scale-free networks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|ScaleFree", meta = (ClampMin = "1", ClampMax = "20"))
    int32 AttachmentEdges = 2;

    // Modular specific parameters
    /** Number of modules for modular architecture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|Modular", meta = (ClampMin = "2", ClampMax = "20"))
    int32 NumModules = 4;

    /** Inter-module connectivity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|Modular", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InterModuleConnectivity = 0.02f;

    /** Intra-module connectivity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology|Modular", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IntraModuleConnectivity = 0.1f;
};

/**
 * Topology Statistics - Metrics describing generated topology
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyStats
{
    GENERATED_BODY()

    /** Total number of nodes */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 NumNodes = 0;

    /** Total number of edges */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 NumEdges = 0;

    /** Actual connectivity (edges / possible_edges) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float ActualConnectivity = 0.0f;

    /** Average degree */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AverageDegree = 0.0f;

    /** Maximum degree */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MaxDegree = 0;

    /** Minimum degree */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MinDegree = 0;

    /** Computed spectral radius */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float ComputedSpectralRadius = 0.0f;

    /** Clustering coefficient (small-world metric) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float ClusteringCoefficient = 0.0f;

    /** Average path length */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AveragePathLength = 0.0f;

    /** Modularity score */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float Modularity = 0.0f;
};

/**
 * Reservoir Topology Data - Adjacency matrix and metadata
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyData
{
    GENERATED_BODY()

    /** Topology configuration used */
    UPROPERTY(BlueprintReadOnly, Category = "Topology")
    FReservoirTopologyConfig Config;

    /** Topology statistics */
    UPROPERTY(BlueprintReadOnly, Category = "Topology")
    FReservoirTopologyStats Stats;

    /** Flattened adjacency matrix (row-major order) */
    UPROPERTY(BlueprintReadOnly, Category = "Topology")
    TArray<float> AdjacencyMatrix;

    /** Node module assignments (for modular topology) */
    UPROPERTY(BlueprintReadOnly, Category = "Topology")
    TArray<int32> NodeModules;

    /** Generation timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Topology")
    float GenerationTime = 0.0f;
};

/**
 * Reservoir Topology Generator Component
 * Generates various network topologies for reservoir computing
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UReservoirTopologyGenerator : public UActorComponent
{
    GENERATED_BODY()

public:
    UReservoirTopologyGenerator();

    // ========================================
    // PUBLIC API - TOPOLOGY GENERATION
    // ========================================

    /** Generate reservoir topology with specified configuration */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyData GenerateTopology(const FReservoirTopologyConfig& Config);

    /** Generate random sparse topology */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyData GenerateRandomSparse(int32 NumNodes, float Connectivity, float SpectralRadius, int32 Seed = 0);

    /** Generate small-world topology (Watts-Strogatz) */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyData GenerateSmallWorld(int32 NumNodes, int32 MeanDegree, float RewiringProb, float SpectralRadius, int32 Seed = 0);

    /** Generate scale-free topology (Barabási-Albert) */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyData GenerateScaleFree(int32 NumNodes, int32 AttachmentEdges, float SpectralRadius, int32 Seed = 0);

    /** Generate modular topology */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyData GenerateModular(int32 NumNodes, int32 NumModules, float IntraConn, float InterConn, float SpectralRadius, int32 Seed = 0);

    // ========================================
    // PUBLIC API - TOPOLOGY ANALYSIS
    // ========================================

    /** Compute topology statistics */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FReservoirTopologyStats ComputeTopologyStats(const TArray<float>& AdjacencyMatrix, int32 NumNodes);

    /** Compute spectral radius of adjacency matrix */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    float ComputeSpectralRadius(const TArray<float>& AdjacencyMatrix, int32 NumNodes);

    /** Scale adjacency matrix to target spectral radius */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    TArray<float> ScaleToSpectralRadius(const TArray<float>& AdjacencyMatrix, int32 NumNodes, float TargetSpectralRadius);

    // ========================================
    // PUBLIC API - VISUALIZATION EXPORT
    // ========================================

    /** Export topology to JSON format */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FString ExportToJSON(const FReservoirTopologyData& TopologyData);

    /** Export topology to GraphML format */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    FString ExportToGraphML(const FReservoirTopologyData& TopologyData);

    /** Save topology to file */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    bool SaveTopologyToFile(const FReservoirTopologyData& TopologyData, const FString& FilePath, bool bUseJSON = true);

    // ========================================
    // PUBLIC API - CONVERSION
    // ========================================

    /** Convert Eigen sparse matrix to UE array */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    static TArray<float> EigenSparseToArray(const Eigen::SparseMatrix<float>& SparseMatrix);

    /** Convert UE array to Eigen sparse matrix */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Topology")
    static Eigen::SparseMatrix<float> ArrayToEigenSparse(const TArray<float>& Array, int32 NumNodes);

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL GENERATION METHODS
    // ========================================

    /** Generate random sparse matrix */
    Eigen::SparseMatrix<float> GenerateRandomSparseSparse(int32 NumNodes, float Connectivity, int32 Seed);

    /** Generate ring lattice for small-world */
    Eigen::SparseMatrix<float> GenerateRingLattice(int32 NumNodes, int32 MeanDegree);

    /** Rewire edges for small-world property */
    void RewireEdges(Eigen::SparseMatrix<float>& Matrix, float RewiringProb, int32 Seed);

    /** Generate scale-free network using preferential attachment */
    Eigen::SparseMatrix<float> GeneratePreferentialAttachment(int32 NumNodes, int32 AttachmentEdges, int32 Seed);

    /** Generate modular network structure */
    Eigen::SparseMatrix<float> GenerateModularStructure(int32 NumNodes, int32 NumModules, float IntraConn, float InterConn, TArray<int32>& NodeModules, int32 Seed);

    // ========================================
    // INTERNAL ANALYSIS METHODS
    // ========================================

    /** Compute degree distribution */
    void ComputeDegreeDistribution(const Eigen::SparseMatrix<float>& Matrix, TArray<int32>& InDegrees, TArray<int32>& OutDegrees);

    /** Compute clustering coefficient */
    float ComputeClusteringCoefficient(const Eigen::SparseMatrix<float>& Matrix);

    /** Compute average path length (BFS-based) */
    float ComputeAveragePathLength(const Eigen::SparseMatrix<float>& Matrix);

    /** Compute modularity score */
    float ComputeModularity(const Eigen::SparseMatrix<float>& Matrix, const TArray<int32>& NodeModules);

    /** Compute spectral radius using power iteration */
    float ComputeSpectralRadiusInternal(const Eigen::SparseMatrix<float>& Matrix);

    /** Scale matrix to target spectral radius */
    void ScaleMatrix(Eigen::SparseMatrix<float>& Matrix, float CurrentSR, float TargetSR);

    // ========================================
    // INTERNAL UTILITY METHODS
    // ========================================

    /** Initialize random number generator */
    void InitializeRNG(int32 Seed);

    /** Generate random float in range [0, 1) */
    float RandomFloat();

    /** Generate random integer in range [0, max) */
    int32 RandomInt(int32 Max);

    /** Generate random weight from normal distribution */
    float RandomWeight();

    /** Ensure matrix is valid (non-empty, proper dimensions) */
    bool ValidateMatrix(const Eigen::SparseMatrix<float>& Matrix, int32 ExpectedSize);

    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Random number generator state */
    std::mt19937 RNG;

    /** Last generated topology data */
    UPROPERTY()
    FReservoirTopologyData LastTopology;
};
