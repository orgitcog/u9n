// NestedTensorPartitionSystem.h
// OEIS A000081 Nested Tensor Partition System for Deep Tree Echo
// Implements integer partitions as tensor shapes with prime-weighted signatures

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace DeepTreeEcho {

// ============================================================================
// Forward Declarations
// ============================================================================

struct FPartitionPart;
struct FNestedPartition;
struct FPrimeWeightedSignature;
struct FThreadPoolMapping;
struct FCognitiveSchedule;
class FPartitionLattice;
class FNestedTensorPartitionSystem;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Partition flag indicating shape derivation type
 */
enum class EPartitionFlag : uint8_t {
    Original,   // O - New shape signature (cacheable)
    Derived,    // D - Reshape/split of existing tensor
    Both        // O+D - Shape appears new but reducible
};

/**
 * @brief Duality orientation for partition operations
 */
enum class EDualityOrientation : uint8_t {
    Objective,      // Extensional - branching, inference, hypergraph
    Subjective,     // Intensional - nesting, training, nested tensor
    Transjective    // Buffer - codec operations between O and S
};

/**
 * @brief Thread pool allocation strategy
 */
enum class EThreadPoolStrategy : uint8_t {
    InterOp,    // Task fan-out (additive partition)
    IntraOp,    // Parallel split (multiplicative factorization)
    Hybrid      // Combined strategy
};

// ============================================================================
// Core Structures
// ============================================================================

/**
 * @brief A single part of a partition with optional factorization
 *
 * Represents one component in a partition, e.g., the "6" in [6] or
 * the factorized [2][3] form.
 */
struct FPartitionPart {
    uint32_t Value = 0;                     // Raw value of the part
    std::vector<uint32_t> Factors;          // Prime factorization if composite
    bool IsPrime = false;                   // True if value is prime
    uint32_t PrimeWeight = 0;               // tau(k) = pi(k-1)

    // Default constructor
    FPartitionPart() = default;

    // Value constructor
    explicit FPartitionPart(uint32_t InValue);

    // Compute the prime weight using tau function
    void ComputePrimeWeight();

    // Factorize the value
    void Factorize();

    // Get tensor shape representation
    std::vector<uint32_t> GetTensorShape() const;

    // String representation
    std::string ToString() const;
};

/**
 * @brief A complete nested partition with signature and flags
 *
 * Represents a full integer partition as a nested tensor structure,
 * e.g., 12 = 6+4+2 -> [[2,3], [2,2], [2]]
 */
struct FNestedPartition {
    uint32_t N = 0;                                 // The integer being partitioned
    std::vector<FPartitionPart> Parts;              // The parts of the partition
    std::vector<uint32_t> Multiplicities;           // Multiplicity of each part size
    EPartitionFlag Flag = EPartitionFlag::Original; // O/D flag
    uint64_t Signature = 0;                         // Unique prime product signature

    // Default constructor
    FNestedPartition() = default;

    // Construct from parts
    FNestedPartition(uint32_t InN, const std::vector<uint32_t>& InParts);

    // Compute the unique signature (product of prime weights)
    void ComputeSignature();

    // Get as nested tensor shape
    std::vector<std::vector<uint32_t>> GetNestedTensorShape() const;

    // Get thread pool configuration
    FThreadPoolMapping GetThreadPoolMapping(EThreadPoolStrategy Strategy) const;

    // Check if this refines another partition
    bool Refines(const FNestedPartition& Other) const;

    // Get all possible refinements
    std::vector<FNestedPartition> GetRefinements() const;

    // String representation
    std::string ToString() const;

    // Comparison for sorting (by signature)
    bool operator<(const FNestedPartition& Other) const { return Signature < Other.Signature; }
};

/**
 * @brief Prime-weighted signature for cognitive states
 *
 * Uses tau(k) = pi(k-1) mapping to create unique signatures
 * for cognitive state combinations.
 */
struct FPrimeWeightedSignature {
    std::vector<uint32_t> Components;       // Component indices
    std::vector<uint32_t> PrimeWeights;     // Prime weights for each component
    uint64_t ProductSignature = 1;          // Product of all prime weights

    // Static prime table (first 100 primes)
    static const std::vector<uint32_t>& GetPrimes();

    // Get nth prime (0-indexed)
    static uint32_t GetPrime(uint32_t n);

    // Tau function: tau(k) = pi(k-1)
    static uint32_t Tau(uint32_t k);

    // Compute signature from components
    void ComputeFromComponents(const std::vector<uint32_t>& InComponents);

    // Check if two signatures are equivalent (same prime factorization)
    bool IsEquivalent(const FPrimeWeightedSignature& Other) const;

    // Factorize the product signature
    std::vector<std::pair<uint32_t, uint32_t>> Factorize() const;
};

/**
 * @brief Thread pool configuration derived from partition
 */
struct FThreadPoolMapping {
    uint32_t TotalThreads = 0;              // Total thread count
    std::vector<uint32_t> SegmentLengths;   // Per-segment thread counts (inter-op)
    std::vector<std::vector<uint32_t>> Grid; // Grid dimensions for each segment (intra-op)
    EThreadPoolStrategy Strategy = EThreadPoolStrategy::Hybrid;

    // Get flat thread indices
    std::vector<uint32_t> GetFlatIndices() const;

    // Get grid coordinates for a flat index
    std::vector<uint32_t> GetGridCoordinates(uint32_t FlatIndex) const;

    // String representation
    std::string ToString() const;
};

/**
 * @brief Cognitive schedule for 12-step loop with 3 streams
 */
struct FCognitiveSchedule {
    static constexpr uint32_t TotalSteps = 12;
    static constexpr uint32_t NumStreams = 3;
    static constexpr uint32_t NumTriads = 4;

    // Triad step groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    std::array<std::array<uint32_t, 3>, NumTriads> TriadSteps;

    // Workload partition per triad
    std::array<FNestedPartition, NumTriads> TriadPartitions;

    // Phase offsets (in degrees): 0, 30, 60, 90
    std::array<float, NumTriads> PhaseOffsets;

    // Stream assignments
    std::array<std::string, NumStreams> StreamNames;

    // Initialize with default 12-step configuration
    void Initialize();

    // Get partition for a specific step
    FNestedPartition GetStepPartition(uint32_t Step) const;

    // Get stream index for a step
    uint32_t GetStreamIndex(uint32_t Step) const;

    // Get triad index for a step
    uint32_t GetTriadIndex(uint32_t Step) const;
};

// ============================================================================
// Partition Lattice
// ============================================================================

/**
 * @brief Partition lattice (shape-DAG) for scheduling
 *
 * The partition lattice organizes all partitions of N into a directed
 * acyclic graph where edges represent refinement relationships.
 */
class FPartitionLattice {
public:
    FPartitionLattice() = default;
    explicit FPartitionLattice(uint32_t N);

    // Build the lattice for given N
    void Build(uint32_t N);

    // Get all partitions
    const std::vector<FNestedPartition>& GetPartitions() const { return Partitions; }

    // Get the state matrix (diagonal of prime weights)
    std::vector<std::vector<uint64_t>> GetStateMatrix() const;

    // Get the refinement matrix (adjacency)
    std::vector<std::vector<uint32_t>> GetRefinementMatrix() const;

    // Get the tagged refinement matrix (edge weights)
    std::vector<std::vector<uint64_t>> GetTaggedRefinementMatrix() const;

    // Find partition by signature
    const FNestedPartition* FindBySignature(uint64_t Signature) const;

    // Get all partitions at a specific depth (number of parts)
    std::vector<const FNestedPartition*> GetPartitionsAtDepth(uint32_t Depth) const;

    // Get refinement path from coarse to fine
    std::vector<const FNestedPartition*> GetRefinementPath(
        const FNestedPartition& Coarse,
        const FNestedPartition& Fine) const;

    // Topological sort (coarse to fine)
    std::vector<const FNestedPartition*> TopologicalSort() const;

private:
    uint32_t N = 0;
    std::vector<FNestedPartition> Partitions;
    std::unordered_map<uint64_t, size_t> SignatureToIndex;
    std::vector<std::vector<size_t>> RefinementEdges;  // Adjacency list

    // Generate all partitions of N
    void GeneratePartitions(uint32_t n, uint32_t maxPart, std::vector<uint32_t>& current);

    // Build refinement edges
    void BuildRefinementEdges();
};

// ============================================================================
// Nested Tensor Partition System
// ============================================================================

/**
 * @brief Main system for tensor-based cognitive scheduling
 *
 * Implements the OEIS A000081 nested tensor partition system for
 * scheduling the 12-step cognitive loop across 3 concurrent streams.
 */
class FNestedTensorPartitionSystem {
public:
    FNestedTensorPartitionSystem();
    ~FNestedTensorPartitionSystem();

    // Initialize the system
    void Initialize();

    // Reset to initial state
    void Reset();

    // ========================================================================
    // Partition Operations
    // ========================================================================

    // Generate all partitions of N
    std::vector<FNestedPartition> GeneratePartitions(uint32_t N);

    // Get the canonical [6] = [2][3] decomposition
    FNestedPartition DecomposeSix();

    // Decompose any composite number into factor partition
    FNestedPartition DecomposeComposite(uint32_t N);

    // Get partition lattice for N
    const FPartitionLattice& GetLattice(uint32_t N);

    // ========================================================================
    // Prime Weight Operations
    // ========================================================================

    // Compute prime weight for a partition
    uint64_t ComputePrimeWeight(const FNestedPartition& Partition) const;

    // Compute cognitive signature from state vector
    uint64_t ComputeCognitiveSignature(const std::vector<uint32_t>& StateVector) const;

    // Decode signature back to state vector
    std::vector<uint32_t> DecodeSignature(uint64_t Signature) const;

    // ========================================================================
    // Thread Pool Scheduling
    // ========================================================================

    // Map partition to thread pool configuration
    FThreadPoolMapping MapToThreadPool(const FNestedPartition& Partition,
                                        EThreadPoolStrategy Strategy = EThreadPoolStrategy::Hybrid);

    // Generate cognitive schedule for 12-step loop
    FCognitiveSchedule GenerateCognitiveSchedule(uint32_t TotalWorkload = 36);

    // Get optimal partition for given workload and parallelism
    FNestedPartition GetOptimalPartition(uint32_t Workload, uint32_t TargetParallelism);

    // ========================================================================
    // Duality Operations (Objective/Subjective)
    // ========================================================================

    // Get objective (extensional) matrix for N
    std::vector<std::vector<uint64_t>> GetObjectiveMatrix(uint32_t N);

    // Get subjective (intensional) matrix for N
    std::vector<std::vector<uint64_t>> GetSubjectiveMatrix(uint32_t N);

    // Transform between objective and subjective representations
    std::vector<uint64_t> ObjectiveToSubjective(const std::vector<uint64_t>& Objective);
    std::vector<uint64_t> SubjectiveToObjective(const std::vector<uint64_t>& Subjective);

    // ========================================================================
    // OEIS A000081 Alignment
    // ========================================================================

    // Get A000081 term count for nesting level
    uint32_t GetA000081TermCount(uint32_t NestingLevel) const;

    // Get nesting level to term count mapping
    std::vector<uint32_t> GetNestingLevelTermCounts(uint32_t MaxLevel = 7) const;

    // Verify A000081 alignment
    bool VerifyA000081Alignment() const;

    // ========================================================================
    // Callbacks and Observers
    // ========================================================================

    using PartitionCallback = std::function<void(const FNestedPartition&)>;
    using ScheduleCallback = std::function<void(uint32_t Step, const FNestedPartition&)>;

    void SetPartitionCallback(PartitionCallback Callback) { OnPartitionGenerated = Callback; }
    void SetScheduleCallback(ScheduleCallback Callback) { OnScheduleStep = Callback; }

private:
    bool bInitialized = false;

    // Cached lattices for different N values
    std::unordered_map<uint32_t, FPartitionLattice> LatticeCache;

    // OEIS A000081 sequence (precomputed)
    std::vector<uint32_t> A000081Sequence;

    // Callbacks
    PartitionCallback OnPartitionGenerated;
    ScheduleCallback OnScheduleStep;

    // Initialize A000081 sequence
    void InitializeA000081Sequence();

    // Helper: Check if number is prime
    bool IsPrime(uint32_t N) const;

    // Helper: Get prime factorization
    std::vector<uint32_t> GetPrimeFactors(uint32_t N) const;
};

} // namespace DeepTreeEcho
