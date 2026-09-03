/**
 * NestedTensorPartitionSystem.h
 * 
 * Deep Tree Echo - OEIS A000081 Nested Tensor Partition System
 * 
 * Maps integer partitions to nested tensors for cognitive thread scheduling.
 * Based on the OEIS A000081 sequence (number of rooted trees with n nodes):
 *   N=1: 1 term
 *   N=2: 2 terms
 *   N=3: 4 terms
 *   N=4: 9 terms
 * 
 * Key insight: Integer partitions are tensor shapes, not numbers.
 * The partition lattice forms a shape-DAG for thread pool scheduling.
 * 
 * Structural decomposition:
 *   [6] = [2][3] → 2×3 product space (not flat index)
 *   6/36 = 1/6 → normalized sampling ratio
 * 
 * Copyright (c) 2025 Deep Tree Echo Project
 */

#pragma once

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <optional>

namespace DeepTreeEcho {

//=============================================================================
// OEIS A000081 Constants
//=============================================================================

/**
 * OEIS A000081: Number of rooted trees with n unlabeled nodes
 * a(n) = 1, 1, 2, 4, 9, 20, 48, 115, 286, 719, ...
 * 
 * For cognitive architecture:
 *   N=1: 1 term (singular channel, undifferentiated)
 *   N=2: 2 terms (opponent processing, universal-particular)
 *   N=3: 4 terms (2 orthogonal dyadic pairs)
 *   N=4: 9 terms (3 concurrent streams × 3 phases)
 *   N=5: 20 terms (full cognitive expansion)
 */
static constexpr std::array<uint32_t, 10> A000081 = {
    1, 1, 2, 4, 9, 20, 48, 115, 286, 719
};

/**
 * Nesting level to step distance mapping
 *   1 nest: 1 step apart
 *   2 nests: 2 steps apart
 *   3 nests: 3 steps apart
 *   4 nests: 4 steps apart
 */
static constexpr std::array<uint32_t, 5> NestStepDistance = {
    0, 1, 2, 3, 4
};

//=============================================================================
// Partition Flags
//=============================================================================

/**
 * Partition origin/derivation flags
 * O = Original (new irreducible nesting)
 * D = Derived (refinement of prior partition)
 */
enum class EPartitionFlag : uint8_t {
    None = 0,
    Original = 1,      // O: New shape signature
    Derived = 2,       // D: Reshape/split of existing tensor
    OriginalDerived = 3 // O+D: Shape appears new but reducible
};

//=============================================================================
// Nested Tensor Types
//=============================================================================

/**
 * A single part in a partition (may be composite)
 * Example: [6] = [2][3] means Part{6, {2, 3}}
 */
struct FPartitionPart {
    // The integer value of this part
    uint32_t Value = 0;
    
    // Factorization (empty if atomic/prime)
    std::vector<uint32_t> Factors;
    
    // Is this part atomic (not further decomposable)?
    bool IsAtomic() const { return Factors.empty(); }
    
    // Get the tensor rank (1 if atomic, factors.size() otherwise)
    uint32_t GetRank() const { return IsAtomic() ? 1 : static_cast<uint32_t>(Factors.size()); }
    
    // String representation
    std::string ToString() const;
};

/**
 * An integer partition as a nested tensor
 * Example: 7 = 3+2+1+1 → [[1,2], [2], [1], [1]]
 */
struct FNestedPartition {
    // The integer being partitioned
    uint32_t N = 0;
    
    // The parts (in decreasing order)
    std::vector<FPartitionPart> Parts;
    
    // Partition signature {multiplicities}
    std::vector<uint32_t> Signature;
    
    // Origin/derivation flag
    EPartitionFlag Flag = EPartitionFlag::None;
    
    // Weight/multiplicity (from A000081 counting)
    uint32_t Weight = 1;
    
    // Additive representation (e.g., "3+2+1+1")
    std::string GetAdditiveForm() const;
    
    // Nested tensor representation (e.g., "[[1,2], [2], [1], [1]]")
    std::string GetTensorForm() const;
    
    // Get total tensor rank
    uint32_t GetTotalRank() const;
    
    // Get number of parts
    uint32_t GetPartCount() const { return static_cast<uint32_t>(Parts.size()); }
    
    // Check if this partition refines another
    bool Refines(const FNestedPartition& Other) const;
};

/**
 * A node in the partition lattice (shape DAG)
 */
struct FPartitionNode {
    // The partition at this node
    FNestedPartition Partition;
    
    // Parent nodes (coarser partitions)
    std::vector<std::weak_ptr<FPartitionNode>> Parents;
    
    // Child nodes (finer partitions via split/reshape)
    std::vector<std::shared_ptr<FPartitionNode>> Children;
    
    // Depth in the DAG (0 = root = [N])
    uint32_t Depth = 0;
    
    // Unique identifier
    uint64_t NodeId = 0;
};

//=============================================================================
// Thread Pool Mapping
//=============================================================================

/**
 * Maps partition structure to thread pool configuration
 */
struct FThreadPoolMapping {
    // Inter-op fan-out (additive partition)
    // Example: 6 = 3+2+1 → 3 inter-op tasks
    uint32_t InterOpTasks = 0;
    
    // Intra-op parallel split (factorization)
    // Example: [6] = [2][3] → 2×3 intra-op grid
    std::array<uint32_t, 2> IntraOpGrid = {1, 1};
    
    // Total thread count
    uint32_t TotalThreads = 0;
    
    // Nesting level (1-4)
    uint32_t NestingLevel = 1;
    
    // Step distance for synchronization
    uint32_t StepDistance = 1;
    
    // Cognitive stream assignment
    uint32_t StreamAssignment = 0;
};

//=============================================================================
// Cognitive Scheduling Types
//=============================================================================

/**
 * Cognitive triad assignment based on partition structure
 * Maps the 12-step loop triads to partition-based scheduling
 */
struct FTriadSchedule {
    // Triad index (0-3)
    uint32_t TriadIndex = 0;
    
    // Steps in this triad
    std::array<uint32_t, 3> Steps = {0, 0, 0};
    
    // Partition for this triad's workload
    FNestedPartition Workload;
    
    // Thread pool mapping
    FThreadPoolMapping ThreadMapping;
};

/**
 * Full 12-step cognitive schedule based on partition algebra
 */
struct FCognitiveSchedule {
    // The four triads
    std::array<FTriadSchedule, 4> Triads;
    
    // Total workload (sum of all partitions)
    uint32_t TotalWorkload = 0;
    
    // Nesting depth used
    uint32_t NestingDepth = 0;
    
    // A000081 term count at this depth
    uint32_t TermCount = 0;
};

//=============================================================================
// NestedTensorPartitionSystem
//=============================================================================

/**
 * Main system for partition-based cognitive scheduling
 * 
 * Core concepts:
 * 1. Integer partitions = tensor shapes
 * 2. Partition lattice = shape DAG
 * 3. Refinement = reshape/split operations
 * 4. O/D flags = cacheable vs non-cacheable shapes
 * 5. 6 = [2][3] → product space, not flat index
 */
class FNestedTensorPartitionSystem {
public:
    //=========================================================================
    // Constants
    //=========================================================================
    
    static constexpr uint32_t MAX_N = 12;  // Maximum partition size
    static constexpr uint32_t MAX_NESTING = 4;  // Maximum nesting depth
    static constexpr uint32_t NUM_STREAMS = 3;  // Cognitive streams
    static constexpr uint32_t STEPS_PER_CYCLE = 12;  // Cognitive loop steps
    
    //=========================================================================
    // Lifecycle
    //=========================================================================
    
    FNestedTensorPartitionSystem();
    ~FNestedTensorPartitionSystem() = default;
    
    //=========================================================================
    // Partition Generation
    //=========================================================================
    
    /**
     * Generate all partitions of N
     * @param N The integer to partition
     * @return Vector of nested partitions
     */
    std::vector<FNestedPartition> GeneratePartitions(uint32_t N);
    
    /**
     * Generate the partition lattice (shape DAG) for N
     * @param N The integer to partition
     * @return Root node of the lattice
     */
    std::shared_ptr<FPartitionNode> GeneratePartitionLattice(uint32_t N);
    
    /**
     * Factor a part into its prime/structural decomposition
     * @param Value The value to factor
     * @return Factorization (empty if prime)
     */
    std::vector<uint32_t> FactorPart(uint32_t Value);
    
    /**
     * Compute the signature {multiplicities} for a partition
     * @param Partition The partition
     * @return Signature vector
     */
    std::vector<uint32_t> ComputeSignature(const FNestedPartition& Partition);
    
    //=========================================================================
    // Thread Pool Mapping
    //=========================================================================
    
    /**
     * Map a partition to thread pool configuration
     * @param Partition The partition
     * @param NestingLevel Target nesting level (1-4)
     * @return Thread pool mapping
     */
    FThreadPoolMapping MapToThreadPool(const FNestedPartition& Partition, uint32_t NestingLevel);
    
    /**
     * Generate cognitive schedule for a given workload
     * @param TotalWorkload Total work units
     * @param NestingDepth Desired nesting depth (1-4)
     * @return Cognitive schedule
     */
    FCognitiveSchedule GenerateCognitiveSchedule(uint32_t TotalWorkload, uint32_t NestingDepth);
    
    /**
     * Get optimal partition for a given thread count
     * @param ThreadCount Available threads
     * @return Optimal partition
     */
    FNestedPartition GetOptimalPartition(uint32_t ThreadCount);
    
    //=========================================================================
    // A000081 Utilities
    //=========================================================================
    
    /**
     * Get A000081(n) - number of rooted trees with n nodes
     * @param N The index
     * @return A000081(N)
     */
    static uint32_t GetA000081(uint32_t N);
    
    /**
     * Get term count for nesting level
     * @param NestingLevel The nesting level (1-4)
     * @return Number of terms
     */
    static uint32_t GetTermCount(uint32_t NestingLevel);
    
    /**
     * Get step distance for nesting level
     * @param NestingLevel The nesting level (1-4)
     * @return Step distance
     */
    static uint32_t GetStepDistance(uint32_t NestingLevel);
    
    //=========================================================================
    // 6 = [2][3] Decomposition
    //=========================================================================
    
    /**
     * Decompose 6 into [2][3] product space
     * This is the canonical example of composite state decomposition
     * @return Partition representing [2][3]
     */
    FNestedPartition DecomposeSix();
    
    /**
     * Normalize a ratio (e.g., 6/36 = 1/6)
     * @param Numerator The numerator
     * @param Denominator The denominator
     * @return Normalized (reduced) ratio
     */
    std::pair<uint32_t, uint32_t> NormalizeRatio(uint32_t Numerator, uint32_t Denominator);
    
    /**
     * Check if a value is a product space (composite)
     * @param Value The value to check
     * @return True if composite (non-atomic)
     */
    bool IsProductSpace(uint32_t Value);
    
    //=========================================================================
    // Tensor Operations
    //=========================================================================
    
    /**
     * Split a partition (refinement operation)
     * @param Partition The partition to split
     * @param PartIndex Index of part to split
     * @param SplitFactors How to split the part
     * @return Refined partition
     */
    FNestedPartition SplitPartition(
        const FNestedPartition& Partition,
        uint32_t PartIndex,
        const std::vector<uint32_t>& SplitFactors
    );
    
    /**
     * Reshape a partition (change factorization)
     * @param Partition The partition to reshape
     * @param PartIndex Index of part to reshape
     * @param NewFactors New factorization
     * @return Reshaped partition
     */
    FNestedPartition ReshapePartition(
        const FNestedPartition& Partition,
        uint32_t PartIndex,
        const std::vector<uint32_t>& NewFactors
    );
    
    /**
     * Merge adjacent parts (coarsening operation)
     * @param Partition The partition to coarsen
     * @param StartIndex Start of parts to merge
     * @param Count Number of parts to merge
     * @return Coarsened partition
     */
    FNestedPartition MergePartition(
        const FNestedPartition& Partition,
        uint32_t StartIndex,
        uint32_t Count
    );
    
    //=========================================================================
    // Visualization
    //=========================================================================
    
    /**
     * Generate Mermaid diagram for partition lattice
     * @param N The integer to partition
     * @return Mermaid flowchart string
     */
    std::string GenerateMermaidDiagram(uint32_t N);
    
    /**
     * Generate partition table (like the user's table)
     * @param MaxN Maximum N to include
     * @return Formatted table string
     */
    std::string GeneratePartitionTable(uint32_t MaxN);
    
private:
    //=========================================================================
    // Internal State
    //=========================================================================
    
    // Cache of generated partitions
    std::unordered_map<uint32_t, std::vector<FNestedPartition>> PartitionCache;
    
    // Cache of partition lattices
    std::unordered_map<uint32_t, std::shared_ptr<FPartitionNode>> LatticeCache;
    
    // Next node ID
    uint64_t NextNodeId = 1;
    
    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    /**
     * Recursive partition generation
     */
    void GeneratePartitionsRecursive(
        uint32_t N,
        uint32_t MaxPart,
        std::vector<uint32_t>& CurrentParts,
        std::vector<FNestedPartition>& Results
    );
    
    /**
     * Assign O/D flags to partitions
     */
    void AssignPartitionFlags(std::vector<FNestedPartition>& Partitions);
    
    /**
     * Compute partition weight from A000081
     */
    uint32_t ComputePartitionWeight(const FNestedPartition& Partition);
    
    /**
     * Build lattice edges (refinement relationships)
     */
    void BuildLatticeEdges(std::shared_ptr<FPartitionNode> Root);
    
    /**
     * GCD for ratio normalization
     */
    static uint32_t GCD(uint32_t A, uint32_t B);
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * Convert partition flag to string
 */
inline const char* ToString(EPartitionFlag Flag) {
    switch (Flag) {
        case EPartitionFlag::None: return "";
        case EPartitionFlag::Original: return "O";
        case EPartitionFlag::Derived: return "D";
        case EPartitionFlag::OriginalDerived: return "O D";
        default: return "?";
    }
}

/**
 * Parse a partition from additive form (e.g., "3+2+1+1")
 */
FNestedPartition ParsePartition(const std::string& AdditiveForm);

/**
 * Check if two partitions are equivalent
 */
bool PartitionsEquivalent(const FNestedPartition& A, const FNestedPartition& B);

} // namespace DeepTreeEcho
