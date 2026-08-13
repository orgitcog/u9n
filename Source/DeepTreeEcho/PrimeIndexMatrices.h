/**
 * @file PrimeIndexMatrices.h
 * @brief Prime-Index Coordinate Matrices for Deep Tree Echo
 * 
 * This module implements the prime-index coordinate system for partition matrices,
 * including State matrices (S_n) and Refinement matrices (R_n) with prime tagging.
 * 
 * Key concepts:
 * - Prime ordinal: π(k) = k-th prime (0-indexed)
 * - Part tag: τ(k) = π(k-1), so 1→2, 2→3, 3→5, 4→7, 5→11
 * - Partition weight: W(λ) = ⊗_{t} τ(λ_t) = product of prime tags
 * 
 * The matrices support both Objective (⊗/branch) and Subjective (⋁/nest) orientations,
 * which form complementary computational ontologies related by categorical duality.
 */

#pragma once

#include <vector>
#include <map>
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <cmath>

namespace DeepTreeEcho {

//=============================================================================
// Prime Number Utilities
//=============================================================================

/**
 * @brief Prime number generator and lookup
 */
class PrimeOracle {
public:
    PrimeOracle(size_t maxPrimes = 100);
    
    // π(k) = k-th prime (0-indexed)
    uint64_t pi(size_t k) const;
    
    // τ(k) = π(k-1), part tag function
    uint64_t tau(size_t k) const;
    
    // Check if n is prime
    bool isPrime(uint64_t n) const;
    
    // Get index of prime (inverse of π)
    int primeIndex(uint64_t p) const;
    
    // Get all primes up to n
    std::vector<uint64_t> primesUpTo(uint64_t n) const;
    
private:
    std::vector<uint64_t> primes_;
    void generatePrimes(size_t count);
};

//=============================================================================
// Partition Representation
//=============================================================================

/**
 * @brief Integer partition with prime-index weights
 */
struct PrimePartition {
    std::vector<int> parts;      // Partition parts in descending order
    uint64_t weight;             // W(λ) = product of τ(parts)
    int n;                       // Sum of parts
    int length;                  // Number of parts
    int maxPart;                 // Largest part
    
    // Dimension signature (n, k, max, n-k, k-1)
    std::tuple<int, int, int, int, int> dimSignature() const;
    
    // String representation
    std::string toString() const;
    
    // Check if this partition refines another (is finer)
    bool refines(const PrimePartition& other) const;
    
    // Check if this is a "pure" partition (single part)
    bool isPure() const { return length == 1; }
    
    // Check if this is "atomic" (all 1s)
    bool isAtomic() const { return maxPart == 1; }
};

/**
 * @brief Generate all partitions of n with prime weights
 */
class PartitionGenerator {
public:
    explicit PartitionGenerator(const PrimeOracle& oracle);
    
    // Generate all partitions of n
    std::vector<PrimePartition> generate(int n) const;
    
    // Generate partitions in canonical order (descending by weight)
    std::vector<PrimePartition> generateOrdered(int n) const;
    
    // Count partitions (p(n))
    int count(int n) const;
    
private:
    const PrimeOracle& oracle_;
    
    void generateRecursive(int n, int maxPart, 
                          std::vector<int>& current,
                          std::vector<PrimePartition>& result) const;
    
    uint64_t computeWeight(const std::vector<int>& parts) const;
};

//=============================================================================
// Matrix Types
//=============================================================================

/**
 * @brief Sparse matrix representation for partition matrices
 */
template<typename T>
class SparseMatrix {
public:
    SparseMatrix(size_t rows, size_t cols);
    
    void set(size_t i, size_t j, T value);
    T get(size_t i, size_t j) const;
    
    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    
    // Convert to dense matrix
    std::vector<std::vector<T>> toDense() const;
    
    // Matrix operations
    SparseMatrix<T> operator+(const SparseMatrix<T>& other) const;
    SparseMatrix<T> operator*(const SparseMatrix<T>& other) const;
    
    // Block diagonal sum (direct sum)
    static SparseMatrix<T> directSum(const SparseMatrix<T>& a, 
                                     const SparseMatrix<T>& b);
    
    // String representation
    std::string toString() const;
    
private:
    size_t rows_, cols_;
    std::map<std::pair<size_t, size_t>, T> data_;
};

//=============================================================================
// State Matrix (S_n)
//=============================================================================

/**
 * @brief State (spectrum) matrix over partitions
 * 
 * S_n is diagonal with entries W(λ) for each partition λ of n.
 * Represents the "pure states" of the partition space.
 */
class StateMatrix {
public:
    StateMatrix(int n, const PrimeOracle& oracle);
    
    // Get the diagonal matrix
    SparseMatrix<uint64_t> matrix() const { return matrix_; }
    
    // Get partition basis
    const std::vector<PrimePartition>& basis() const { return basis_; }
    
    // Get weight for partition index
    uint64_t weight(size_t idx) const;
    
    // Get dimension
    size_t dim() const { return basis_.size(); }
    
    // String representation
    std::string toString() const;
    
private:
    int n_;
    std::vector<PrimePartition> basis_;
    SparseMatrix<uint64_t> matrix_;
    const PrimeOracle& oracle_;
};

//=============================================================================
// Refinement Matrix (R_n)
//=============================================================================

/**
 * @brief Refinement (adjacency) matrix for partition DAG
 * 
 * R_n[i][j] = 1 if partition i can refine to partition j via single split.
 * Tagged version has edge labels as the prime tags introduced by the split.
 */
class RefinementMatrix {
public:
    RefinementMatrix(int n, const PrimeOracle& oracle, bool tagged = false);
    
    // Get the adjacency matrix
    SparseMatrix<uint64_t> matrix() const { return matrix_; }
    
    // Get partition basis
    const std::vector<PrimePartition>& basis() const { return basis_; }
    
    // Check if partition i can refine to j
    bool canRefine(size_t i, size_t j) const;
    
    // Get refinement tag (for tagged matrix)
    uint64_t refinementTag(size_t i, size_t j) const;
    
    // Get all refinement paths from coarsest to finest
    std::vector<std::vector<size_t>> allPaths() const;
    
    // String representation
    std::string toString() const;
    
private:
    int n_;
    bool tagged_;
    std::vector<PrimePartition> basis_;
    SparseMatrix<uint64_t> matrix_;
    const PrimeOracle& oracle_;
    
    // Check if partition a can split to partition b in one step
    bool isSingleSplit(const PrimePartition& a, const PrimePartition& b) const;
    
    // Compute the tag for a split
    uint64_t computeSplitTag(const PrimePartition& a, const PrimePartition& b) const;
};

//=============================================================================
// Objective/Subjective Decomposition
//=============================================================================

/**
 * @brief Orientation type for partition matrices
 */
enum class Orientation {
    Objective,   // ⊗ (branch/product) - coproduct-centric
    Subjective,  // ⋁ (nest/chain) - product-centric
    Full         // Combined M_n = O_n ⊕ S_n
};

/**
 * @brief Decomposed partition matrix with objective/subjective split
 * 
 * The full matrix M_n decomposes as:
 *   M_n = O_n ⊕ S_n
 * 
 * Where:
 *   O_n = Objective/product operator (⊗-side)
 *   S_n = Subjective/nest operator (⋁-side)
 */
class DecomposedMatrix {
public:
    DecomposedMatrix(int n, const PrimeOracle& oracle);
    
    // Get objective matrix (O_n)
    SparseMatrix<uint64_t> objective() const { return objective_; }
    
    // Get subjective matrix (S_n)
    SparseMatrix<uint64_t> subjective() const { return subjective_; }
    
    // Get full matrix (M_n = O_n + S_n or O_n ⊕ S_n)
    SparseMatrix<uint64_t> full(bool directSum = false) const;
    
    // Get partition basis
    const std::vector<PrimePartition>& objectiveBasis() const { return objBasis_; }
    const std::vector<PrimePartition>& subjectiveBasis() const { return subjBasis_; }
    
    // String representation
    std::string toString() const;
    
private:
    int n_;
    std::vector<PrimePartition> objBasis_;   // Mixed partitions
    std::vector<PrimePartition> subjBasis_;  // Pure partitions
    SparseMatrix<uint64_t> objective_;
    SparseMatrix<uint64_t> subjective_;
    const PrimeOracle& oracle_;
    
    void decompose();
};

//=============================================================================
// Nestor Matrix System
//=============================================================================

/**
 * @brief Complete Nestor matrix system for a given n
 * 
 * Combines:
 * - State matrix S_n (diagonal spectrum)
 * - Refinement matrix R_n (DAG adjacency)
 * - Tagged refinement R_n^(tagged) (with split tags)
 * - Objective/Subjective decomposition
 */
class NestorMatrixSystem {
public:
    NestorMatrixSystem(int n);
    
    // Access components
    const StateMatrix& stateMatrix() const { return *state_; }
    const RefinementMatrix& refinementMatrix() const { return *refinement_; }
    const RefinementMatrix& taggedRefinementMatrix() const { return *taggedRefinement_; }
    const DecomposedMatrix& decomposedMatrix() const { return *decomposed_; }
    
    // Get n
    int n() const { return n_; }
    
    // Get partition count p(n)
    size_t partitionCount() const { return state_->dim(); }
    
    // Generate full report
    std::string report() const;
    
    // Export to various formats
    std::string toLatex() const;
    std::string toMermaid() const;
    std::string toJson() const;
    
private:
    int n_;
    std::unique_ptr<PrimeOracle> oracle_;
    std::unique_ptr<StateMatrix> state_;
    std::unique_ptr<RefinementMatrix> refinement_;
    std::unique_ptr<RefinementMatrix> taggedRefinement_;
    std::unique_ptr<DecomposedMatrix> decomposed_;
};

//=============================================================================
// Cognitive Integration
//=============================================================================

/**
 * @brief Maps partition matrices to cognitive loop scheduling
 */
class CognitiveMatrixMapper {
public:
    CognitiveMatrixMapper(const NestorMatrixSystem& system);
    
    // Map partition to cognitive stream
    int partitionToStream(const PrimePartition& p) const;
    
    // Map partition to loop step
    int partitionToStep(const PrimePartition& p) const;
    
    // Get thread pool configuration from partition
    struct ThreadConfig {
        int interOpThreads;
        int intraOpThreads;
        std::vector<int> taskShape;
    };
    ThreadConfig partitionToThreadConfig(const PrimePartition& p) const;
    
    // Generate cognitive schedule from refinement path
    struct CognitiveSchedule {
        std::vector<int> steps;
        std::vector<int> streams;
        std::vector<ThreadConfig> configs;
    };
    CognitiveSchedule generateSchedule(const std::vector<size_t>& path) const;
    
private:
    const NestorMatrixSystem& system_;
};

//=============================================================================
// Transjective Membrane Interface
//=============================================================================

/**
 * @brief Membrane layer type
 */
enum class MembraneLayer {
    Inner,        // Subjective core
    Intermembrane, // Transjective buffer
    Outer         // Objective gateway
};

/**
 * @brief Crossing policy for membrane transitions
 */
struct CrossingPolicy {
    bool allowObjectiveToSubjective;
    bool allowSubjectiveToObjective;
    float trustThreshold;
    float energyBudget;
    std::vector<std::string> requiredCapabilities;
};

/**
 * @brief Transjective membrane manager
 * 
 * Implements the double-membrane architecture:
 * - Inner membrane: Subjective core (product-closed)
 * - Intermembrane space: Transjective buffer (codecs, policies)
 * - Outer membrane: Objective gateway (coproduct-free)
 */
class TransjectiveMembrane {
public:
    TransjectiveMembrane(const CrossingPolicy& policy);
    
    // Check if crossing is allowed
    bool canCross(MembraneLayer from, MembraneLayer to, 
                  const std::string& dataType) const;
    
    // Transform data for crossing
    template<typename T>
    T transform(const T& data, MembraneLayer from, MembraneLayer to) const;
    
    // Get current energy budget
    float energyBudget() const { return policy_.energyBudget; }
    
    // Update policy
    void updatePolicy(const CrossingPolicy& policy) { policy_ = policy; }
    
private:
    CrossingPolicy policy_;
};

} // namespace DeepTreeEcho
