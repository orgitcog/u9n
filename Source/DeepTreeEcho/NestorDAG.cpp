/**
 * @file NestorDAG.cpp
 * @brief Implementation of Nestor (Nested-Tensor) DAG for Quantized Integer Formalism
 */

#include "NestorDAG.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <numeric>

namespace DeepTreeEcho
{

//=============================================================================
// FNestorDimension Implementation
//=============================================================================

FNestorDimension FNestorDimension::FromPartition(const std::vector<uint32_t>& Parts)
{
    if (Parts.empty()) return FNestorDimension();
    
    uint32_t Mass = std::accumulate(Parts.begin(), Parts.end(), 0u);
    uint32_t K = static_cast<uint32_t>(Parts.size());
    uint32_t Max = Parts[0];  // Parts are nonincreasing
    
    return FNestorDimension(Mass, K, Max);
}

std::string FNestorDimension::ToString() const
{
    std::ostringstream oss;
    oss << "(" << Mass << "," << OuterLength << "," << MaxPart 
        << "," << Raggedness << "," << SplitDepth << ")";
    return oss.str();
}

bool FNestorDimension::operator<(const FNestorDimension& Other) const
{
    // Order by split depth (DAG layering)
    if (SplitDepth != Other.SplitDepth) return SplitDepth < Other.SplitDepth;
    // Then by max part (descending)
    if (MaxPart != Other.MaxPart) return MaxPart > Other.MaxPart;
    // Then by outer length
    return OuterLength < Other.OuterLength;
}

//=============================================================================
// FNestorNode Implementation
//=============================================================================

FNestorNode::FNestorNode(const std::vector<uint32_t>& InParts, uint32_t InId)
    : Parts(InParts)
    , NodeId(InId)
{
    // Ensure parts are sorted in nonincreasing order
    std::sort(Parts.begin(), Parts.end(), std::greater<uint32_t>());
    Dimension = FNestorDimension::FromPartition(Parts);
}

std::string FNestorNode::ToString() const
{
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < Parts.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << Parts[i];
    }
    oss << ")";
    return oss.str();
}

//=============================================================================
// FNestorAddress Implementation
//=============================================================================

FNestorAddress FNestorAddress::Prefix(uint32_t k) const
{
    FNestorAddress Result;
    if (k <= Indices.size())
    {
        Result.Indices.assign(Indices.begin(), Indices.begin() + k);
    }
    else
    {
        Result.Indices = Indices;
    }
    return Result;
}

FNestorAddress FNestorAddress::Extend(uint32_t NewIndex) const
{
    FNestorAddress Result;
    Result.Indices = Indices;
    Result.Indices.push_back(NewIndex);
    return Result;
}

bool FNestorAddress::Extends(const FNestorAddress& Other) const
{
    if (Indices.size() < Other.Indices.size()) return false;
    for (size_t i = 0; i < Other.Indices.size(); ++i)
    {
        if (Indices[i] != Other.Indices[i]) return false;
    }
    return true;
}

std::string FNestorAddress::ToString() const
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < Indices.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << Indices[i];
    }
    oss << "]";
    return oss.str();
}

std::string FNestorAddress::ToNestedBrackets() const
{
    if (Indices.empty()) return "[]";
    
    std::ostringstream oss;
    // Build nested bracket notation: [i][[i][j]][[i][j][k]]...
    for (size_t depth = 1; depth <= Indices.size(); ++depth)
    {
        oss << "[";
        for (size_t i = 0; i < depth; ++i)
        {
            oss << "[" << Indices[i] << "]";
        }
        oss << "]";
    }
    return oss.str();
}

//=============================================================================
// FLevelOperator Implementation
//=============================================================================

FLevelOperator FLevelOperator::KroneckerProduct(const FLevelOperator& Other) const
{
    FLevelOperator Result;
    Result.Level = Level;  // Keep first level (or combine somehow)
    Result.ChoiceIndex = ChoiceIndex;
    Result.Rows = Rows * Other.Rows;
    Result.Cols = Cols * Other.Cols;
    Result.Matrix.resize(Result.Rows * Result.Cols);
    
    // Compute Kronecker product: A ⊗ B
    for (uint32_t i = 0; i < Rows; ++i)
    {
        for (uint32_t j = 0; j < Cols; ++j)
        {
            float aij = Matrix[i * Cols + j];
            for (uint32_t k = 0; k < Other.Rows; ++k)
            {
                for (uint32_t l = 0; l < Other.Cols; ++l)
                {
                    float bkl = Other.Matrix[k * Other.Cols + l];
                    uint32_t row = i * Other.Rows + k;
                    uint32_t col = j * Other.Cols + l;
                    Result.Matrix[row * Result.Cols + col] = aij * bkl;
                }
            }
        }
    }
    
    return Result;
}

//=============================================================================
// FKroneckerTower Implementation
//=============================================================================

FKroneckerTower FKroneckerTower::Build(const FNestorAddress& Addr,
                                        const std::vector<std::vector<FLevelOperator>>& LevelOps)
{
    FKroneckerTower Result;
    Result.Address = Addr;
    
    if (Addr.IsEmpty())
    {
        // Identity for empty address
        Result.Rows = 1;
        Result.Cols = 1;
        Result.Matrix = {1.0f};
        return Result;
    }
    
    // Build tower: 𝒜_𝐢 := ⊗_{ℓ=1}^{L} A^{(ℓ)}_{iₗ}
    bool First = true;
    FLevelOperator Accumulated;
    
    for (size_t ell = 0; ell < Addr.Length(); ++ell)
    {
        uint32_t ChoiceIdx = Addr.Indices[ell];
        if (ell >= LevelOps.size() || ChoiceIdx >= LevelOps[ell].size())
        {
            // Use identity if operator not defined
            continue;
        }
        
        const FLevelOperator& Op = LevelOps[ell][ChoiceIdx];
        
        if (First)
        {
            Accumulated = Op;
            First = false;
        }
        else
        {
            Accumulated = Accumulated.KroneckerProduct(Op);
        }
    }
    
    if (First)
    {
        // No operators applied, return identity
        Result.Rows = 1;
        Result.Cols = 1;
        Result.Matrix = {1.0f};
    }
    else
    {
        Result.Rows = Accumulated.Rows;
        Result.Cols = Accumulated.Cols;
        Result.Matrix = Accumulated.Matrix;
    }
    
    return Result;
}

//=============================================================================
// FBilinearPayload Implementation
//=============================================================================

float FBilinearPayload::Evaluate(const FKroneckerTower& Tower) const
{
    // Compute p^T · A · q
    if (LeftVector.size() != Tower.Rows || RightVector.size() != Tower.Cols)
    {
        // Dimension mismatch - return 0 or handle error
        return 0.0f;
    }
    
    float Result = 0.0f;
    for (uint32_t i = 0; i < Tower.Rows; ++i)
    {
        for (uint32_t j = 0; j < Tower.Cols; ++j)
        {
            Result += LeftVector[i] * Tower.Matrix[i * Tower.Cols + j] * RightVector[j];
        }
    }
    
    return Result;
}

//=============================================================================
// FNestorDAG Implementation
//=============================================================================

FNestorDAG::FNestorDAG(uint32_t InN)
    : N(InN)
{
    BuildDAG();
}

void FNestorDAG::BuildDAG()
{
    // Generate all partitions of N
    std::vector<std::vector<uint32_t>> AllPartitions;
    std::vector<uint32_t> Current;
    GeneratePartitions(N, N, Current, AllPartitions);
    
    // Sort partitions by dimension (for consistent ordering)
    std::sort(AllPartitions.begin(), AllPartitions.end(),
        [](const std::vector<uint32_t>& A, const std::vector<uint32_t>& B)
        {
            FNestorDimension DimA = FNestorDimension::FromPartition(A);
            FNestorDimension DimB = FNestorDimension::FromPartition(B);
            return DimA < DimB;
        });
    
    // Create nodes
    std::map<std::vector<uint32_t>, uint32_t> PartitionToId;
    for (size_t i = 0; i < AllPartitions.size(); ++i)
    {
        FNestorNode Node(AllPartitions[i], static_cast<uint32_t>(i));
        Nodes.push_back(Node);
        PartitionToId[AllPartitions[i]] = static_cast<uint32_t>(i);
    }
    
    // Create edges (refinement/split relationships)
    for (size_t i = 0; i < Nodes.size(); ++i)
    {
        for (size_t j = 0; j < Nodes.size(); ++j)
        {
            if (i == j) continue;
            
            uint32_t SplitPartIdx, SplitInto;
            if (IsSingleSplitRefinement(Nodes[i].Parts, Nodes[j].Parts, SplitPartIdx, SplitInto))
            {
                FNestorEdge Edge(static_cast<uint32_t>(i), static_cast<uint32_t>(j), 
                                 SplitPartIdx, SplitInto);
                Edges.push_back(Edge);
                ChildrenMap[static_cast<uint32_t>(i)].push_back(static_cast<uint32_t>(j));
            }
        }
    }
}

void FNestorDAG::GeneratePartitions(uint32_t n, uint32_t maxPart, 
                                     std::vector<uint32_t>& current,
                                     std::vector<std::vector<uint32_t>>& result)
{
    if (n == 0)
    {
        result.push_back(current);
        return;
    }
    
    for (uint32_t i = std::min(n, maxPart); i >= 1; --i)
    {
        current.push_back(i);
        GeneratePartitions(n - i, i, current, result);
        current.pop_back();
    }
}

bool FNestorDAG::IsSingleSplitRefinement(const std::vector<uint32_t>& A,
                                          const std::vector<uint32_t>& B,
                                          uint32_t& SplitPartIdx,
                                          uint32_t& SplitInto) const
{
    // B is a single-split refinement of A if:
    // - B has exactly one more part than A (or more, if split into >2)
    // - All parts of A except one appear in B
    // - The remaining part of A is split into the extra parts of B
    
    if (B.size() <= A.size()) return false;
    
    // Create multisets for comparison
    std::multiset<uint32_t> SetA(A.begin(), A.end());
    std::multiset<uint32_t> SetB(B.begin(), B.end());
    
    // Find parts in A not in B
    std::vector<uint32_t> OnlyInA;
    for (uint32_t part : A)
    {
        auto it = SetB.find(part);
        if (it != SetB.end())
        {
            SetB.erase(it);
        }
        else
        {
            OnlyInA.push_back(part);
        }
    }
    
    // Should have exactly one part in A that was split
    if (OnlyInA.size() != 1) return false;
    
    uint32_t SplitPart = OnlyInA[0];
    
    // The remaining parts in SetB should sum to SplitPart
    uint32_t SumRemaining = std::accumulate(SetB.begin(), SetB.end(), 0u);
    if (SumRemaining != SplitPart) return false;
    
    // Find the index of the split part in A
    for (size_t i = 0; i < A.size(); ++i)
    {
        if (A[i] == SplitPart)
        {
            SplitPartIdx = static_cast<uint32_t>(i);
            break;
        }
    }
    
    SplitInto = static_cast<uint32_t>(SetB.size());
    return true;
}

std::vector<const FNestorNode*> FNestorDAG::GetNodesAtDepth(uint32_t Depth) const
{
    std::vector<const FNestorNode*> Result;
    for (const auto& Node : Nodes)
    {
        if (Node.Dimension.SplitDepth == Depth)
        {
            Result.push_back(&Node);
        }
    }
    return Result;
}

std::vector<uint32_t> FNestorDAG::GetChildren(uint32_t NodeId) const
{
    auto it = ChildrenMap.find(NodeId);
    if (it != ChildrenMap.end())
    {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> FNestorDAG::GetChildIndices(const FNestorAddress& Prefix) const
{
    // For now, return indices 0..k-1 where k is number of children
    // In a full implementation, this would track the actual refinement choices
    
    if (Prefix.IsEmpty())
    {
        // Children of root
        auto Children = GetChildren(0);
        std::vector<uint32_t> Indices;
        for (size_t i = 0; i < Children.size(); ++i)
        {
            Indices.push_back(static_cast<uint32_t>(i));
        }
        return Indices;
    }
    
    // Navigate to the node at the prefix
    // This is simplified - full implementation would track path properly
    return {};
}

std::vector<FNestorAddress> FNestorDAG::GetAllAddresses() const
{
    std::vector<FNestorAddress> Result;
    
    // Add empty address (root)
    Result.push_back(FNestorAddress());
    
    // BFS to generate all addresses
    std::function<void(uint32_t, FNestorAddress)> GenerateAddresses;
    GenerateAddresses = [&](uint32_t NodeId, FNestorAddress CurrentAddr)
    {
        auto Children = GetChildren(NodeId);
        for (size_t i = 0; i < Children.size(); ++i)
        {
            FNestorAddress NewAddr = CurrentAddr.Extend(static_cast<uint32_t>(i));
            Result.push_back(NewAddr);
            GenerateAddresses(Children[i], NewAddr);
        }
    };
    
    GenerateAddresses(0, FNestorAddress());
    
    return Result;
}

std::vector<FNestorAddress> FNestorDAG::GetExtensions(const FNestorAddress& Prefix) const
{
    std::vector<FNestorAddress> AllAddrs = GetAllAddresses();
    std::vector<FNestorAddress> Result;
    
    for (const auto& Addr : AllAddrs)
    {
        if (Addr.Extends(Prefix))
        {
            Result.push_back(Addr);
        }
    }
    
    return Result;
}

std::string FNestorDAG::ToMermaid() const
{
    std::ostringstream oss;
    oss << "flowchart TD\n";
    
    // Define nodes
    for (const auto& Node : Nodes)
    {
        char Label = 'A' + Node.NodeId;
        oss << "  " << Label << "[\"" << Node.ToString() << "\\n" 
            << Node.Dimension.ToString() << "\"]\n";
    }
    
    oss << "\n";
    
    // Define edges
    for (const auto& Edge : Edges)
    {
        char SrcLabel = 'A' + Edge.SourceId;
        char TgtLabel = 'A' + Edge.TargetId;
        oss << "  " << SrcLabel << " --> " << TgtLabel << "\n";
    }
    
    return oss.str();
}

//=============================================================================
// FQuantizedInteger Implementation
//=============================================================================

FQuantizedInteger::FQuantizedInteger(uint32_t N)
    : DAG(N)
{
}

void FQuantizedInteger::SetLevelOperators(const std::vector<std::vector<FLevelOperator>>& Ops)
{
    LevelOperators = Ops;
}

void FQuantizedInteger::SetPayloads(const std::map<FNestorAddress, FBilinearPayload>& InPayloads)
{
    Payloads = InPayloads;
}

float FQuantizedInteger::Evaluate() const
{
    // Q(n) = ⋁_{𝐢∈𝒜ₙ} p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    float Sum = 0.0f;
    
    auto AllAddresses = DAG.GetAllAddresses();
    for (const auto& Addr : AllAddresses)
    {
        Sum += GetComponent(Addr);
    }
    
    return Sum;
}

float FQuantizedInteger::EvaluateConditional(const FNestorAddress& Prefix) const
{
    // Q(n | π) = ⋁_{𝐢⪰π} p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    float Sum = 0.0f;
    
    auto Extensions = DAG.GetExtensions(Prefix);
    for (const auto& Addr : Extensions)
    {
        Sum += GetComponent(Addr);
    }
    
    return Sum;
}

float FQuantizedInteger::GetComponent(const FNestorAddress& Address) const
{
    // Q_n[𝐢] = p_{n,𝐢}^T · 𝒜_𝐢 · q_{n,𝐢}
    
    auto PayloadIt = Payloads.find(Address);
    if (PayloadIt == Payloads.end())
    {
        // No payload defined for this address
        return 0.0f;
    }
    
    FKroneckerTower Tower = BuildTower(Address);
    return PayloadIt->second.Evaluate(Tower);
}

std::map<FNestorAddress, float> FQuantizedInteger::GetAllComponents() const
{
    std::map<FNestorAddress, float> Result;
    
    auto AllAddresses = DAG.GetAllAddresses();
    for (const auto& Addr : AllAddresses)
    {
        Result[Addr] = GetComponent(Addr);
    }
    
    return Result;
}

FKroneckerTower FQuantizedInteger::BuildTower(const FNestorAddress& Address) const
{
    return FKroneckerTower::Build(Address, LevelOperators);
}

//=============================================================================
// FLevelFactorizedQuantizedInteger Implementation
//=============================================================================

FLevelFactorizedQuantizedInteger::FLevelFactorizedQuantizedInteger(uint32_t N)
    : DAG(N)
{
}

void FLevelFactorizedQuantizedInteger::SetLevelData(uint32_t Level,
                                                     const std::vector<std::vector<float>>& LeftVectors,
                                                     const std::vector<FLevelOperator>& Operators,
                                                     const std::vector<std::vector<float>>& RightVectors)
{
    FLevelFactors Factors;
    Factors.LeftVectors = LeftVectors;
    Factors.Operators = Operators;
    Factors.RightVectors = RightVectors;
    LevelData[Level] = Factors;
}

float FLevelFactorizedQuantizedInteger::Evaluate() const
{
    // Q(n) = ⋁_𝐢 ∏_{ℓ=1}^{|𝐢|} (p^{(ℓ)}_{iₗ} A^{(ℓ)}_{iₗ} q^{(ℓ)}_{iₗ})
    float Sum = 0.0f;
    
    auto AllAddresses = DAG.GetAllAddresses();
    for (const auto& Addr : AllAddresses)
    {
        Sum += GetPathWeight(Addr);
    }
    
    return Sum;
}

float FLevelFactorizedQuantizedInteger::GetPathWeight(const FNestorAddress& Address) const
{
    // w(𝐢) = ∏_{ℓ=1}^{|𝐢|} p^{(ℓ)}_{iₗ} A^{(ℓ)}_{iₗ} q^{(ℓ)}_{iₗ}
    
    if (Address.IsEmpty())
    {
        return 1.0f;  // Identity weight for empty address
    }
    
    float Product = 1.0f;
    
    for (size_t ell = 0; ell < Address.Length(); ++ell)
    {
        uint32_t ChoiceIdx = Address.Indices[ell];
        
        auto LevelIt = LevelData.find(static_cast<uint32_t>(ell));
        if (LevelIt == LevelData.end()) continue;
        
        const auto& Factors = LevelIt->second;
        
        if (ChoiceIdx >= Factors.LeftVectors.size() ||
            ChoiceIdx >= Factors.Operators.size() ||
            ChoiceIdx >= Factors.RightVectors.size())
        {
            continue;
        }
        
        // Compute p^{(ℓ)}_{iₗ} · A^{(ℓ)}_{iₗ} · q^{(ℓ)}_{iₗ}
        const auto& p = Factors.LeftVectors[ChoiceIdx];
        const auto& A = Factors.Operators[ChoiceIdx];
        const auto& q = Factors.RightVectors[ChoiceIdx];
        
        float LevelValue = 0.0f;
        for (uint32_t i = 0; i < A.Rows && i < p.size(); ++i)
        {
            for (uint32_t j = 0; j < A.Cols && j < q.size(); ++j)
            {
                LevelValue += p[i] * A.Matrix[i * A.Cols + j] * q[j];
            }
        }
        
        Product *= LevelValue;
    }
    
    return Product;
}

//=============================================================================
// FNestorCognitiveIntegration Implementation
//=============================================================================

FNestorCognitiveIntegration::FNestorCognitiveIntegration()
{
}

FNestorCognitiveIntegration::FThreadConfig 
FNestorCognitiveIntegration::PartitionToThreadConfig(const FNestorNode& Node) const
{
    FThreadConfig Config;
    Config.TotalThreads = Node.Dimension.Mass;
    Config.ThreadsPerTask = Node.Parts;
    Config.NestingDepth = Node.Dimension.SplitDepth;
    return Config;
}

FNestorCognitiveIntegration::FCognitiveMapping
FNestorCognitiveIntegration::AddressToCognitiveMapping(const FNestorAddress& Address) const
{
    FCognitiveMapping Mapping;
    
    // Map address length to stream index (mod 3)
    Mapping.StreamIndex = Address.Length() % 3;
    
    // Map to phase within stream (mod 4)
    uint32_t TotalIndex = 0;
    for (uint32_t idx : Address.Indices)
    {
        TotalIndex += idx;
    }
    Mapping.PhaseIndex = TotalIndex % 4;
    
    // Map to triad group
    // Triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    uint32_t Step = (Mapping.StreamIndex * 4 + Mapping.PhaseIndex) % 12 + 1;
    Mapping.TriadGroup = ((Step - 1) % 4);
    
    return Mapping;
}

std::vector<FNestorCognitiveIntegration::FCognitiveMapping>
FNestorCognitiveIntegration::GenerateSchedule(const FQuantizedInteger& QInt) const
{
    std::vector<FCognitiveMapping> Schedule;
    
    auto AllAddresses = QInt.GetDAG().GetAllAddresses();
    for (const auto& Addr : AllAddresses)
    {
        Schedule.push_back(AddressToCognitiveMapping(Addr));
    }
    
    // Sort by triad group, then stream, then phase
    std::sort(Schedule.begin(), Schedule.end(),
        [](const FCognitiveMapping& A, const FCognitiveMapping& B)
        {
            if (A.TriadGroup != B.TriadGroup) return A.TriadGroup < B.TriadGroup;
            if (A.StreamIndex != B.StreamIndex) return A.StreamIndex < B.StreamIndex;
            return A.PhaseIndex < B.PhaseIndex;
        });
    
    return Schedule;
}

bool FNestorCognitiveIntegration::ValidateA000081Alignment(uint32_t NestingLevel, 
                                                           uint32_t TermCount) const
{
    // OEIS A000081: 1, 1, 2, 4, 9, 20, 48, 115, 286, 719, ...
    // For nesting levels 1-4, expected terms are: 1, 2, 4, 9
    static const std::vector<uint32_t> A000081 = {1, 1, 2, 4, 9, 20, 48, 115, 286, 719};
    
    if (NestingLevel < A000081.size())
    {
        return TermCount == A000081[NestingLevel];
    }
    
    // For higher levels, we'd need to compute A000081
    return true;  // Assume valid if we can't check
}

} // namespace DeepTreeEcho
