/**
 * @file NestorDAG.h
 * @brief Nestor (Nested-Tensor) DAG for Quantized Integer Formalism
 * 
 * This module implements the Nestor DAG framework where integers are represented
 * as hierarchical multi-index objects over partition-trees. A "quantized integer"
 * is not a scalar, but a measure/tensor over refinement paths.
 * 
 * Key concepts:
 * - Partition refinement DAG (split edges)
 * - Per-node dimension signatures: (n, k, max, n-k, k-1)
 * - Address-sum operator (inverted-∨) for power-series expansion
 * - Kronecker tower operators for path composition
 * - Bilinear quantized integer evaluation
 */

#pragma once

#include "CoreMinimal.h"
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <memory>

namespace DeepTreeEcho
{

/**
 * @brief Dimension signature for a partition node
 * 
 * dim(λ) = (n, k, max, n-k, k-1)
 * - n: Mass (invariant, sum of parts)
 * - k: Outer length (number of parts, ragged width)
 * - max: Max part size (inner extent proxy)
 * - n-k: Raggedness (distance from all-ones)
 * - k-1: Split-depth (graph distance from root)
 */
struct FNestorDimension
{
    uint32_t Mass;          // n = Σλᵢ
    uint32_t OuterLength;   // k = number of parts
    uint32_t MaxPart;       // λ₁ = largest part
    uint32_t Raggedness;    // n - k
    uint32_t SplitDepth;    // k - 1
    
    FNestorDimension() : Mass(0), OuterLength(0), MaxPart(0), Raggedness(0), SplitDepth(0) {}
    
    FNestorDimension(uint32_t InMass, uint32_t InK, uint32_t InMax)
        : Mass(InMass)
        , OuterLength(InK)
        , MaxPart(InMax)
        , Raggedness(InMass - InK)
        , SplitDepth(InK - 1)
    {}
    
    // Compute from partition
    static FNestorDimension FromPartition(const std::vector<uint32_t>& Parts);
    
    // String representation: (n, k, max, n-k, k-1)
    std::string ToString() const;
    
    // Comparison for DAG layering
    bool operator<(const FNestorDimension& Other) const;
};

/**
 * @brief A partition node in the Nestor DAG
 */
struct FNestorNode
{
    std::vector<uint32_t> Parts;    // Partition parts (nonincreasing)
    FNestorDimension Dimension;      // Dimension signature
    uint32_t NodeId;                 // Unique node identifier
    
    FNestorNode() : NodeId(0) {}
    FNestorNode(const std::vector<uint32_t>& InParts, uint32_t InId);
    
    // String representation: (λ₁, λ₂, ..., λₖ)
    std::string ToString() const;
    
    // Check if this is the root (single part)
    bool IsRoot() const { return Parts.size() == 1; }
    
    // Check if this is the sink (all ones)
    bool IsSink() const { return Dimension.MaxPart == 1; }
};

/**
 * @brief A refinement edge in the Nestor DAG (split operation)
 */
struct FNestorEdge
{
    uint32_t SourceId;      // Parent node
    uint32_t TargetId;      // Child node
    uint32_t SplitPartIdx;  // Which part was split
    uint32_t SplitInto;     // Number of pieces
    
    FNestorEdge() : SourceId(0), TargetId(0), SplitPartIdx(0), SplitInto(0) {}
    FNestorEdge(uint32_t Src, uint32_t Tgt, uint32_t PartIdx, uint32_t NumPieces)
        : SourceId(Src), TargetId(Tgt), SplitPartIdx(PartIdx), SplitInto(NumPieces) {}
};

/**
 * @brief An address (path) in the Nestor DAG
 * 
 * A path is a sequence of refinement choices: π = (i₁, i₂, ..., iₗ)
 * where each iₗ selects which split/refinement edge was taken at level ℓ.
 */
struct FNestorAddress
{
    std::vector<uint32_t> Indices;  // Path indices
    
    FNestorAddress() {}
    FNestorAddress(std::initializer_list<uint32_t> Init) : Indices(Init) {}
    
    // Length of the address (depth)
    uint32_t Length() const { return static_cast<uint32_t>(Indices.size()); }
    
    // Empty address check
    bool IsEmpty() const { return Indices.empty(); }
    
    // Get prefix of length k
    FNestorAddress Prefix(uint32_t k) const;
    
    // Extend address with new index
    FNestorAddress Extend(uint32_t NewIndex) const;
    
    // Check if this address extends another
    bool Extends(const FNestorAddress& Other) const;
    
    // String representation: [i₁, i₂, ..., iₗ]
    std::string ToString() const;
    
    // Nested bracket notation: [i][[i][j]]...
    std::string ToNestedBrackets() const;
    
    bool operator<(const FNestorAddress& Other) const { return Indices < Other.Indices; }
    bool operator==(const FNestorAddress& Other) const { return Indices == Other.Indices; }
};

/**
 * @brief Per-level operator for Kronecker tower construction
 * 
 * At depth ℓ, choice iₗ selects operator A^{(ℓ)}_{iₗ}
 */
struct FLevelOperator
{
    uint32_t Level;         // ℓ
    uint32_t ChoiceIndex;   // iₗ
    std::vector<float> Matrix;  // Flattened operator matrix
    uint32_t Rows;
    uint32_t Cols;
    
    FLevelOperator() : Level(0), ChoiceIndex(0), Rows(0), Cols(0) {}
    
    // Kronecker product with another operator
    FLevelOperator KroneckerProduct(const FLevelOperator& Other) const;
};

/**
 * @brief Kronecker tower operator for an address
 * 
 * 𝒜_𝐢 := ⊗_{ℓ=1}^{L} A^{(ℓ)}_{iₗ}
 */
struct FKroneckerTower
{
    FNestorAddress Address;
    std::vector<float> Matrix;  // Composed Kronecker product
    uint32_t Rows;
    uint32_t Cols;
    
    FKroneckerTower() : Rows(1), Cols(1), Matrix({1.0f}) {}  // Identity for empty address
    
    // Build tower from level operators
    static FKroneckerTower Build(const FNestorAddress& Addr, 
                                  const std::vector<std::vector<FLevelOperator>>& LevelOps);
};

/**
 * @brief Bilinear payload for quantized integer evaluation
 * 
 * Q_n[𝐢] := p_{n,𝐢} · 𝒜_𝐢 · q_{n,𝐢}
 */
struct FBilinearPayload
{
    std::vector<float> LeftVector;   // p_{n,𝐢}
    std::vector<float> RightVector;  // q_{n,𝐢}
    
    FBilinearPayload() {}
    FBilinearPayload(const std::vector<float>& Left, const std::vector<float>& Right)
        : LeftVector(Left), RightVector(Right) {}
    
    // Evaluate bilinear form: p^T · A · q
    float Evaluate(const FKroneckerTower& Tower) const;
};

/**
 * @brief The complete Nestor DAG for a given integer n
 */
class FNestorDAG
{
public:
    FNestorDAG(uint32_t N);
    
    // Get the integer this DAG represents
    uint32_t GetN() const { return N; }
    
    // Get all nodes (partitions)
    const std::vector<FNestorNode>& GetNodes() const { return Nodes; }
    
    // Get all edges (refinements)
    const std::vector<FNestorEdge>& GetEdges() const { return Edges; }
    
    // Get root node (single partition (n))
    const FNestorNode& GetRoot() const { return Nodes[0]; }
    
    // Get sink node (all-ones partition)
    const FNestorNode& GetSink() const { return Nodes.back(); }
    
    // Get nodes at a specific depth
    std::vector<const FNestorNode*> GetNodesAtDepth(uint32_t Depth) const;
    
    // Get children of a node
    std::vector<uint32_t> GetChildren(uint32_t NodeId) const;
    
    // Get valid children indices for an address (Ch(π))
    std::vector<uint32_t> GetChildIndices(const FNestorAddress& Prefix) const;
    
    // Get all valid addresses (paths) in the DAG
    std::vector<FNestorAddress> GetAllAddresses() const;
    
    // Get addresses extending a prefix (Ext(π))
    std::vector<FNestorAddress> GetExtensions(const FNestorAddress& Prefix) const;
    
    // Generate Mermaid diagram
    std::string ToMermaid() const;
    
private:
    uint32_t N;
    std::vector<FNestorNode> Nodes;
    std::vector<FNestorEdge> Edges;
    std::map<uint32_t, std::vector<uint32_t>> ChildrenMap;
    
    // Build the DAG by generating all partitions and refinement edges
    void BuildDAG();
    
    // Generate all partitions of N
    void GeneratePartitions(uint32_t n, uint32_t maxPart, std::vector<uint32_t>& current,
                            std::vector<std::vector<uint32_t>>& result);
    
    // Check if partition B is a single-split refinement of partition A
    bool IsSingleSplitRefinement(const std::vector<uint32_t>& A, 
                                  const std::vector<uint32_t>& B,
                                  uint32_t& SplitPartIdx, uint32_t& SplitInto) const;
};

/**
 * @brief Quantized Integer evaluator using the Nestor DAG formalism
 * 
 * Q(n) := ⋁_{𝐢∈𝒜ₙ} p_{n,𝐢} · 𝒜_𝐢 · q_{n,𝐢}
 * 
 * The address-sum operator ⋁ sums over all valid refinement paths.
 */
class FQuantizedInteger
{
public:
    FQuantizedInteger(uint32_t N);
    
    // Set level operators for Kronecker tower construction
    void SetLevelOperators(const std::vector<std::vector<FLevelOperator>>& Ops);
    
    // Set bilinear payloads for each address
    void SetPayloads(const std::map<FNestorAddress, FBilinearPayload>& Payloads);
    
    // Evaluate the full quantized integer (scalar version)
    // Q(n) = ⋁_{𝐢∈𝒜ₙ} p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    float Evaluate() const;
    
    // Evaluate conditional quantized integer given a prefix
    // Q(n | π) = ⋁_{𝐢⪰π} p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    float EvaluateConditional(const FNestorAddress& Prefix) const;
    
    // Get component field value for a specific address
    // Q_n[𝐢] = p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    float GetComponent(const FNestorAddress& Address) const;
    
    // Get all component values as {Q_n[], Q_n[i], Q_n[i,j], ...}
    std::map<FNestorAddress, float> GetAllComponents() const;
    
    // Get the underlying DAG
    const FNestorDAG& GetDAG() const { return DAG; }
    
private:
    FNestorDAG DAG;
    std::vector<std::vector<FLevelOperator>> LevelOperators;
    std::map<FNestorAddress, FBilinearPayload> Payloads;
    
    // Build Kronecker tower for an address
    FKroneckerTower BuildTower(const FNestorAddress& Address) const;
};

/**
 * @brief Level-factorized quantized integer (ultra-dense form)
 * 
 * Q(n) = ⋁_𝐢 ∏_{ℓ=1}^{|𝐢|} (p^{(ℓ)}_{iₗ} A^{(ℓ)}_{iₗ} q^{(ℓ)}_{iₗ})
 * 
 * This is the densest "power-series-ish" form where everything factorizes by level.
 */
class FLevelFactorizedQuantizedInteger
{
public:
    FLevelFactorizedQuantizedInteger(uint32_t N);
    
    // Set per-level vectors and operators
    void SetLevelData(uint32_t Level, 
                      const std::vector<std::vector<float>>& LeftVectors,
                      const std::vector<FLevelOperator>& Operators,
                      const std::vector<std::vector<float>>& RightVectors);
    
    // Evaluate using level-factorized form
    float Evaluate() const;
    
    // Get path weight: w(𝐢) = ∏_{ℓ=1}^{|𝐢|} p^{(ℓ)}_{iₗ} A^{(ℓ)}_{iₗ} q^{(ℓ)}_{iₗ}
    float GetPathWeight(const FNestorAddress& Address) const;
    
private:
    FNestorDAG DAG;
    
    // Per-level data: Level -> (ChoiceIndex -> (p, A, q))
    struct FLevelFactors
    {
        std::vector<std::vector<float>> LeftVectors;
        std::vector<FLevelOperator> Operators;
        std::vector<std::vector<float>> RightVectors;
    };
    std::map<uint32_t, FLevelFactors> LevelData;
};

/**
 * @brief Integration with Deep Tree Echo cognitive architecture
 */
class FNestorCognitiveIntegration
{
public:
    FNestorCognitiveIntegration();
    
    // Map partition to thread pool configuration
    struct FThreadConfig
    {
        uint32_t TotalThreads;
        std::vector<uint32_t> ThreadsPerTask;  // Partition parts
        uint32_t NestingDepth;
    };
    
    FThreadConfig PartitionToThreadConfig(const FNestorNode& Node) const;
    
    // Map to 12-step cognitive loop
    struct FCognitiveMapping
    {
        uint32_t StreamIndex;       // 0, 1, 2 (Cerebral, Somatic, Autonomic)
        uint32_t PhaseIndex;        // 0-3 (within stream)
        uint32_t TriadGroup;        // {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    };
    
    FCognitiveMapping AddressToCognitiveMapping(const FNestorAddress& Address) const;
    
    // Generate optimal schedule from quantized integer
    std::vector<FCognitiveMapping> GenerateSchedule(const FQuantizedInteger& QInt) const;
    
    // OEIS A000081 alignment check
    bool ValidateA000081Alignment(uint32_t NestingLevel, uint32_t TermCount) const;
};

} // namespace DeepTreeEcho
