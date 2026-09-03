/**
 * NestedTensorPartitionSystem.cpp
 * 
 * Deep Tree Echo - OEIS A000081 Nested Tensor Partition System Implementation
 * 
 * Maps integer partitions to nested tensors for cognitive thread scheduling.
 * 
 * Copyright (c) 2025 Deep Tree Echo Project
 */

#include "NestedTensorPartitionSystem.h"

#include <algorithm>
#include <sstream>
#include <cmath>
#include <numeric>
#include <set>

namespace DeepTreeEcho {

//=============================================================================
// FPartitionPart Implementation
//=============================================================================

std::string FPartitionPart::ToString() const {
    if (IsAtomic()) {
        return std::to_string(Value);
    }
    
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < Factors.size(); ++i) {
        if (i > 0) oss << ",";
        oss << Factors[i];
    }
    oss << "]";
    return oss.str();
}

//=============================================================================
// FNestedPartition Implementation
//=============================================================================

std::string FNestedPartition::GetAdditiveForm() const {
    std::ostringstream oss;
    for (size_t i = 0; i < Parts.size(); ++i) {
        if (i > 0) oss << "+";
        oss << Parts[i].Value;
    }
    return oss.str();
}

std::string FNestedPartition::GetTensorForm() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < Parts.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << Parts[i].ToString();
    }
    oss << "]";
    return oss.str();
}

uint32_t FNestedPartition::GetTotalRank() const {
    uint32_t rank = 0;
    for (const auto& part : Parts) {
        rank += part.GetRank();
    }
    return rank;
}

bool FNestedPartition::Refines(const FNestedPartition& Other) const {
    // A refines B if A can be obtained by splitting parts of B
    if (N != Other.N) return false;
    if (Parts.size() <= Other.Parts.size()) return false;
    
    // Check if our parts can be grouped to form Other's parts
    std::vector<uint32_t> ourValues, otherValues;
    for (const auto& p : Parts) ourValues.push_back(p.Value);
    for (const auto& p : Other.Parts) otherValues.push_back(p.Value);
    
    // Simple check: our multiset should contain Other's multiset
    // after appropriate grouping
    std::sort(ourValues.begin(), ourValues.end(), std::greater<uint32_t>());
    std::sort(otherValues.begin(), otherValues.end(), std::greater<uint32_t>());
    
    // This is a simplified check - full refinement requires more complex logic
    return ourValues.size() > otherValues.size();
}

//=============================================================================
// FNestedTensorPartitionSystem Implementation
//=============================================================================

FNestedTensorPartitionSystem::FNestedTensorPartitionSystem() {
    // Pre-generate partitions for small N
    for (uint32_t n = 1; n <= 7; ++n) {
        GeneratePartitions(n);
    }
}

//=============================================================================
// Partition Generation
//=============================================================================

std::vector<FNestedPartition> FNestedTensorPartitionSystem::GeneratePartitions(uint32_t N) {
    // Check cache
    auto it = PartitionCache.find(N);
    if (it != PartitionCache.end()) {
        return it->second;
    }
    
    std::vector<FNestedPartition> results;
    std::vector<uint32_t> currentParts;
    
    GeneratePartitionsRecursive(N, N, currentParts, results);
    
    // Factor each part and assign flags
    for (auto& partition : results) {
        for (auto& part : partition.Parts) {
            part.Factors = FactorPart(part.Value);
        }
        partition.Signature = ComputeSignature(partition);
    }
    
    AssignPartitionFlags(results);
    
    // Compute weights
    for (auto& partition : results) {
        partition.Weight = ComputePartitionWeight(partition);
    }
    
    // Cache and return
    PartitionCache[N] = results;
    return results;
}

void FNestedTensorPartitionSystem::GeneratePartitionsRecursive(
    uint32_t N,
    uint32_t MaxPart,
    std::vector<uint32_t>& CurrentParts,
    std::vector<FNestedPartition>& Results
) {
    if (N == 0) {
        // Found a valid partition
        FNestedPartition partition;
        partition.N = std::accumulate(CurrentParts.begin(), CurrentParts.end(), 0u);
        
        for (uint32_t value : CurrentParts) {
            FPartitionPart part;
            part.Value = value;
            partition.Parts.push_back(part);
        }
        
        Results.push_back(partition);
        return;
    }
    
    // Try each possible next part
    for (uint32_t part = std::min(N, MaxPart); part >= 1; --part) {
        CurrentParts.push_back(part);
        GeneratePartitionsRecursive(N - part, part, CurrentParts, Results);
        CurrentParts.pop_back();
    }
}

std::shared_ptr<FPartitionNode> FNestedTensorPartitionSystem::GeneratePartitionLattice(uint32_t N) {
    // Check cache
    auto it = LatticeCache.find(N);
    if (it != LatticeCache.end()) {
        return it->second;
    }
    
    // Generate all partitions
    auto partitions = GeneratePartitions(N);
    
    // Create root node (the trivial partition [N])
    auto root = std::make_shared<FPartitionNode>();
    root->NodeId = NextNodeId++;
    root->Depth = 0;
    
    // Find the trivial partition
    for (const auto& p : partitions) {
        if (p.Parts.size() == 1 && p.Parts[0].Value == N) {
            root->Partition = p;
            break;
        }
    }
    
    // Build the lattice
    BuildLatticeEdges(root);
    
    // Cache and return
    LatticeCache[N] = root;
    return root;
}

std::vector<uint32_t> FNestedTensorPartitionSystem::FactorPart(uint32_t Value) {
    if (Value <= 1) return {};
    
    std::vector<uint32_t> factors;
    uint32_t n = Value;
    
    // Find prime factors
    for (uint32_t p = 2; p * p <= n; ++p) {
        while (n % p == 0) {
            factors.push_back(p);
            n /= p;
        }
    }
    
    if (n > 1) {
        factors.push_back(n);
    }
    
    // If only one factor (prime), return empty
    if (factors.size() <= 1) {
        return {};
    }
    
    // Group factors into two groups for [a][b] representation
    // For 6 = 2*3, return {2, 3}
    // For 12 = 2*2*3, return {4, 3} or {2, 6}
    if (factors.size() == 2) {
        return factors;
    }
    
    // For more factors, group into two
    uint32_t product1 = 1, product2 = 1;
    for (size_t i = 0; i < factors.size(); ++i) {
        if (i < factors.size() / 2) {
            product1 *= factors[i];
        } else {
            product2 *= factors[i];
        }
    }
    
    return {product1, product2};
}

std::vector<uint32_t> FNestedTensorPartitionSystem::ComputeSignature(const FNestedPartition& Partition) {
    // Count multiplicities of each part value
    std::unordered_map<uint32_t, uint32_t> counts;
    for (const auto& part : Partition.Parts) {
        counts[part.Value]++;
    }
    
    // Convert to sorted vector
    std::vector<uint32_t> signature;
    for (const auto& [value, count] : counts) {
        signature.push_back(count);
    }
    
    std::sort(signature.begin(), signature.end(), std::greater<uint32_t>());
    return signature;
}

void FNestedTensorPartitionSystem::AssignPartitionFlags(std::vector<FNestedPartition>& Partitions) {
    std::set<std::string> seenSignatures;
    
    for (auto& partition : Partitions) {
        // Convert signature to string for comparison
        std::ostringstream oss;
        oss << "{";
        for (size_t i = 0; i < partition.Signature.size(); ++i) {
            if (i > 0) oss << ",";
            oss << partition.Signature[i];
        }
        oss << "}";
        std::string sigStr = oss.str();
        
        // Check if this is a new signature
        bool isNew = seenSignatures.find(sigStr) == seenSignatures.end();
        seenSignatures.insert(sigStr);
        
        // Check if any part is composite (can be derived by factorization)
        bool isDerived = false;
        for (const auto& part : partition.Parts) {
            if (!part.IsAtomic()) {
                isDerived = true;
                break;
            }
        }
        
        // Assign flag
        if (isNew && isDerived) {
            partition.Flag = EPartitionFlag::OriginalDerived;
        } else if (isNew) {
            partition.Flag = EPartitionFlag::Original;
        } else if (isDerived) {
            partition.Flag = EPartitionFlag::Derived;
        } else {
            partition.Flag = EPartitionFlag::None;
        }
    }
}

uint32_t FNestedTensorPartitionSystem::ComputePartitionWeight(const FNestedPartition& Partition) {
    // Weight based on A000081 counting
    // For now, use a simplified formula
    uint32_t weight = 1;
    
    // Weight increases with number of parts
    weight *= static_cast<uint32_t>(Partition.Parts.size());
    
    // Weight increases with factorization depth
    for (const auto& part : Partition.Parts) {
        if (!part.IsAtomic()) {
            weight *= static_cast<uint32_t>(part.Factors.size());
        }
    }
    
    return weight;
}

void FNestedTensorPartitionSystem::BuildLatticeEdges(std::shared_ptr<FPartitionNode> Root) {
    // This would build the full refinement lattice
    // For now, we just mark the root
    // Full implementation would recursively create child nodes for each refinement
}

//=============================================================================
// Thread Pool Mapping
//=============================================================================

FThreadPoolMapping FNestedTensorPartitionSystem::MapToThreadPool(
    const FNestedPartition& Partition,
    uint32_t NestingLevel
) {
    FThreadPoolMapping mapping;
    
    // Inter-op tasks = number of parts
    mapping.InterOpTasks = Partition.GetPartCount();
    
    // Intra-op grid from largest composite part
    for (const auto& part : Partition.Parts) {
        if (!part.IsAtomic() && part.Factors.size() >= 2) {
            mapping.IntraOpGrid[0] = part.Factors[0];
            mapping.IntraOpGrid[1] = part.Factors[1];
            break;
        }
    }
    
    // If no composite parts, use 1x1 grid
    if (mapping.IntraOpGrid[0] == 0) {
        mapping.IntraOpGrid[0] = 1;
        mapping.IntraOpGrid[1] = 1;
    }
    
    // Total threads
    mapping.TotalThreads = mapping.InterOpTasks * mapping.IntraOpGrid[0] * mapping.IntraOpGrid[1];
    
    // Nesting level
    mapping.NestingLevel = NestingLevel;
    mapping.StepDistance = GetStepDistance(NestingLevel);
    
    // Stream assignment based on partition structure
    mapping.StreamAssignment = Partition.GetPartCount() % NUM_STREAMS;
    
    return mapping;
}

FCognitiveSchedule FNestedTensorPartitionSystem::GenerateCognitiveSchedule(
    uint32_t TotalWorkload,
    uint32_t NestingDepth
) {
    FCognitiveSchedule schedule;
    schedule.TotalWorkload = TotalWorkload;
    schedule.NestingDepth = NestingDepth;
    schedule.TermCount = GetTermCount(NestingDepth);
    
    // Distribute workload across 4 triads
    uint32_t workPerTriad = TotalWorkload / 4;
    uint32_t remainder = TotalWorkload % 4;
    
    // Triad step assignments
    const std::array<std::array<uint32_t, 3>, 4> triadSteps = {{
        {1, 5, 9},
        {2, 6, 10},
        {3, 7, 11},
        {4, 8, 12}
    }};
    
    for (uint32_t i = 0; i < 4; ++i) {
        auto& triad = schedule.Triads[i];
        triad.TriadIndex = i;
        triad.Steps = triadSteps[i];
        
        // Assign workload
        uint32_t triadWork = workPerTriad + (i < remainder ? 1 : 0);
        
        // Generate partition for this workload
        if (triadWork > 0) {
            auto partitions = GeneratePartitions(triadWork);
            if (!partitions.empty()) {
                // Use the partition with most parts for maximum parallelism
                triad.Workload = *std::max_element(partitions.begin(), partitions.end(),
                    [](const FNestedPartition& a, const FNestedPartition& b) {
                        return a.GetPartCount() < b.GetPartCount();
                    });
            }
        }
        
        // Map to thread pool
        triad.ThreadMapping = MapToThreadPool(triad.Workload, NestingDepth);
    }
    
    return schedule;
}

FNestedPartition FNestedTensorPartitionSystem::GetOptimalPartition(uint32_t ThreadCount) {
    // Find partition that best matches thread count
    auto partitions = GeneratePartitions(ThreadCount);
    
    if (partitions.empty()) {
        FNestedPartition empty;
        empty.N = ThreadCount;
        return empty;
    }
    
    // Prefer partitions with [2][3] style factorizations
    for (const auto& p : partitions) {
        for (const auto& part : p.Parts) {
            if (!part.IsAtomic()) {
                return p;
            }
        }
    }
    
    // Otherwise return the one with most parts
    return *std::max_element(partitions.begin(), partitions.end(),
        [](const FNestedPartition& a, const FNestedPartition& b) {
            return a.GetPartCount() < b.GetPartCount();
        });
}

//=============================================================================
// A000081 Utilities
//=============================================================================

uint32_t FNestedTensorPartitionSystem::GetA000081(uint32_t N) {
    if (N < A000081.size()) {
        return A000081[N];
    }
    // For larger N, would need to compute dynamically
    return 0;
}

uint32_t FNestedTensorPartitionSystem::GetTermCount(uint32_t NestingLevel) {
    switch (NestingLevel) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 9;
        default: return GetA000081(NestingLevel);
    }
}

uint32_t FNestedTensorPartitionSystem::GetStepDistance(uint32_t NestingLevel) {
    if (NestingLevel < NestStepDistance.size()) {
        return NestStepDistance[NestingLevel];
    }
    return NestingLevel;
}

//=============================================================================
// 6 = [2][3] Decomposition
//=============================================================================

FNestedPartition FNestedTensorPartitionSystem::DecomposeSix() {
    FNestedPartition partition;
    partition.N = 6;
    
    FPartitionPart part;
    part.Value = 6;
    part.Factors = {2, 3};
    
    partition.Parts.push_back(part);
    partition.Signature = {1};
    partition.Flag = EPartitionFlag::Original;
    partition.Weight = 2;  // Two orthogonal axes
    
    return partition;
}

std::pair<uint32_t, uint32_t> FNestedTensorPartitionSystem::NormalizeRatio(
    uint32_t Numerator,
    uint32_t Denominator
) {
    uint32_t gcd = GCD(Numerator, Denominator);
    return {Numerator / gcd, Denominator / gcd};
}

bool FNestedTensorPartitionSystem::IsProductSpace(uint32_t Value) {
    if (Value <= 1) return false;
    
    // Check if composite (has non-trivial factors)
    for (uint32_t p = 2; p * p <= Value; ++p) {
        if (Value % p == 0) {
            return true;
        }
    }
    
    return false;
}

//=============================================================================
// Tensor Operations
//=============================================================================

FNestedPartition FNestedTensorPartitionSystem::SplitPartition(
    const FNestedPartition& Partition,
    uint32_t PartIndex,
    const std::vector<uint32_t>& SplitFactors
) {
    if (PartIndex >= Partition.Parts.size()) {
        return Partition;
    }
    
    FNestedPartition result;
    result.N = Partition.N;
    
    for (size_t i = 0; i < Partition.Parts.size(); ++i) {
        if (i == PartIndex) {
            // Split this part
            for (uint32_t factor : SplitFactors) {
                FPartitionPart newPart;
                newPart.Value = factor;
                newPart.Factors = FactorPart(factor);
                result.Parts.push_back(newPart);
            }
        } else {
            result.Parts.push_back(Partition.Parts[i]);
        }
    }
    
    // Sort parts in decreasing order
    std::sort(result.Parts.begin(), result.Parts.end(),
        [](const FPartitionPart& a, const FPartitionPart& b) {
            return a.Value > b.Value;
        });
    
    result.Signature = ComputeSignature(result);
    result.Flag = EPartitionFlag::Derived;
    result.Weight = ComputePartitionWeight(result);
    
    return result;
}

FNestedPartition FNestedTensorPartitionSystem::ReshapePartition(
    const FNestedPartition& Partition,
    uint32_t PartIndex,
    const std::vector<uint32_t>& NewFactors
) {
    if (PartIndex >= Partition.Parts.size()) {
        return Partition;
    }
    
    FNestedPartition result = Partition;
    
    // Verify factors multiply to the same value
    uint32_t product = 1;
    for (uint32_t f : NewFactors) {
        product *= f;
    }
    
    if (product != result.Parts[PartIndex].Value) {
        return Partition;  // Invalid reshape
    }
    
    result.Parts[PartIndex].Factors = NewFactors;
    result.Flag = EPartitionFlag::Derived;
    
    return result;
}

FNestedPartition FNestedTensorPartitionSystem::MergePartition(
    const FNestedPartition& Partition,
    uint32_t StartIndex,
    uint32_t Count
) {
    if (StartIndex + Count > Partition.Parts.size()) {
        return Partition;
    }
    
    FNestedPartition result;
    result.N = Partition.N;
    
    // Sum the parts to merge
    uint32_t mergedValue = 0;
    for (uint32_t i = StartIndex; i < StartIndex + Count; ++i) {
        mergedValue += Partition.Parts[i].Value;
    }
    
    // Build new parts list
    for (size_t i = 0; i < Partition.Parts.size(); ++i) {
        if (i == StartIndex) {
            FPartitionPart mergedPart;
            mergedPart.Value = mergedValue;
            mergedPart.Factors = FactorPart(mergedValue);
            result.Parts.push_back(mergedPart);
        } else if (i < StartIndex || i >= StartIndex + Count) {
            result.Parts.push_back(Partition.Parts[i]);
        }
    }
    
    // Sort parts in decreasing order
    std::sort(result.Parts.begin(), result.Parts.end(),
        [](const FPartitionPart& a, const FPartitionPart& b) {
            return a.Value > b.Value;
        });
    
    result.Signature = ComputeSignature(result);
    result.Flag = EPartitionFlag::None;
    result.Weight = ComputePartitionWeight(result);
    
    return result;
}

//=============================================================================
// Visualization
//=============================================================================

std::string FNestedTensorPartitionSystem::GenerateMermaidDiagram(uint32_t N) {
    auto partitions = GeneratePartitions(N);
    
    std::ostringstream oss;
    oss << "flowchart TD\n";
    oss << "  N[\"Scalar " << N << "\"]\n\n";
    
    oss << "  subgraph Partitions[\"Partitions of " << N << "\"]\n";
    
    for (size_t i = 0; i < partitions.size(); ++i) {
        const auto& p = partitions[i];
        oss << "    P" << i << "[\"" << p.GetAdditiveForm() << "\\n";
        oss << p.GetTensorForm() << "\\n";
        oss << "Flag: " << ToString(p.Flag) << "\"]\n";
    }
    
    oss << "  end\n\n";
    
    // Connect N to first partition
    oss << "  N --> P0\n";
    
    // Connect partitions by refinement
    for (size_t i = 0; i < partitions.size(); ++i) {
        for (size_t j = i + 1; j < partitions.size(); ++j) {
            if (partitions[j].Refines(partitions[i])) {
                oss << "  P" << i << " --> P" << j << "\n";
            }
        }
    }
    
    return oss.str();
}

std::string FNestedTensorPartitionSystem::GeneratePartitionTable(uint32_t MaxN) {
    std::ostringstream oss;
    
    for (uint32_t n = 1; n <= MaxN; ++n) {
        auto partitions = GeneratePartitions(n);
        
        oss << "#N : " << n << "\n";
        oss << std::string(60, '-') << "\n";
        
        for (const auto& p : partitions) {
            oss << p.GetAdditiveForm() << "\t";
            oss << p.GetTensorForm() << "\t";
            oss << "{";
            for (size_t i = 0; i < p.Signature.size(); ++i) {
                if (i > 0) oss << ",";
                oss << p.Signature[i];
            }
            oss << "}\t";
            oss << p.Weight << "\t";
            oss << ToString(p.Flag) << "\n";
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

//=============================================================================
// Static Utilities
//=============================================================================

uint32_t FNestedTensorPartitionSystem::GCD(uint32_t A, uint32_t B) {
    while (B != 0) {
        uint32_t temp = B;
        B = A % B;
        A = temp;
    }
    return A;
}

//=============================================================================
// Free Functions
//=============================================================================

FNestedPartition ParsePartition(const std::string& AdditiveForm) {
    FNestedPartition partition;
    
    std::istringstream iss(AdditiveForm);
    std::string token;
    
    while (std::getline(iss, token, '+')) {
        uint32_t value = std::stoul(token);
        partition.N += value;
        
        FPartitionPart part;
        part.Value = value;
        partition.Parts.push_back(part);
    }
    
    return partition;
}

bool PartitionsEquivalent(const FNestedPartition& A, const FNestedPartition& B) {
    if (A.N != B.N) return false;
    if (A.Parts.size() != B.Parts.size()) return false;
    
    for (size_t i = 0; i < A.Parts.size(); ++i) {
        if (A.Parts[i].Value != B.Parts[i].Value) return false;
    }
    
    return true;
}

} // namespace DeepTreeEcho
