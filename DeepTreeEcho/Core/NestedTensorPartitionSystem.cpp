// NestedTensorPartitionSystem.cpp
// Implementation of OEIS A000081 Nested Tensor Partition System

#include "NestedTensorPartitionSystem.h"
#include <sstream>
#include <queue>
#include <set>
#include <cassert>

namespace DeepTreeEcho {

// ============================================================================
// Static Prime Table
// ============================================================================

const std::vector<uint32_t>& FPrimeWeightedSignature::GetPrimes() {
    static const std::vector<uint32_t> Primes = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
        31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
        73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
        127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
        179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
        233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
        283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
        353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
        419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
        467, 479, 487, 491, 499, 503, 509, 521, 523, 541
    };
    return Primes;
}

uint32_t FPrimeWeightedSignature::GetPrime(uint32_t n) {
    const auto& primes = GetPrimes();
    if (n < primes.size()) {
        return primes[n];
    }
    // For larger n, compute on the fly (simple sieve)
    uint32_t candidate = primes.back() + 2;
    uint32_t count = static_cast<uint32_t>(primes.size());
    while (count <= n) {
        bool isPrime = true;
        uint32_t sqrtCandidate = static_cast<uint32_t>(std::sqrt(candidate));
        for (uint32_t p : primes) {
            if (p > sqrtCandidate) break;
            if (candidate % p == 0) {
                isPrime = false;
                break;
            }
        }
        if (isPrime) {
            count++;
            if (count > n) return candidate;
        }
        candidate += 2;
    }
    return candidate;
}

uint32_t FPrimeWeightedSignature::Tau(uint32_t k) {
    // tau(k) = pi(k-1), where pi(n) is the nth prime (0-indexed)
    if (k == 0) return 1;  // Edge case
    return GetPrime(k - 1);
}

void FPrimeWeightedSignature::ComputeFromComponents(const std::vector<uint32_t>& InComponents) {
    Components = InComponents;
    PrimeWeights.clear();
    ProductSignature = 1;

    for (uint32_t comp : Components) {
        uint32_t weight = Tau(comp);
        PrimeWeights.push_back(weight);
        ProductSignature *= weight;
    }
}

bool FPrimeWeightedSignature::IsEquivalent(const FPrimeWeightedSignature& Other) const {
    return ProductSignature == Other.ProductSignature;
}

std::vector<std::pair<uint32_t, uint32_t>> FPrimeWeightedSignature::Factorize() const {
    std::vector<std::pair<uint32_t, uint32_t>> factors;
    uint64_t n = ProductSignature;

    for (uint32_t p : GetPrimes()) {
        if (static_cast<uint64_t>(p) * p > n) break;
        uint32_t count = 0;
        while (n % p == 0) {
            n /= p;
            count++;
        }
        if (count > 0) {
            factors.emplace_back(p, count);
        }
    }
    if (n > 1) {
        factors.emplace_back(static_cast<uint32_t>(n), 1);
    }
    return factors;
}

// ============================================================================
// FPartitionPart Implementation
// ============================================================================

FPartitionPart::FPartitionPart(uint32_t InValue) : Value(InValue) {
    ComputePrimeWeight();
    Factorize();
}

void FPartitionPart::ComputePrimeWeight() {
    PrimeWeight = FPrimeWeightedSignature::Tau(Value);
}

void FPartitionPart::Factorize() {
    Factors.clear();
    if (Value <= 1) {
        IsPrime = (Value == 1);
        return;
    }

    uint32_t n = Value;
    for (uint32_t p : FPrimeWeightedSignature::GetPrimes()) {
        if (p * p > n) break;
        while (n % p == 0) {
            Factors.push_back(p);
            n /= p;
        }
    }
    if (n > 1) {
        Factors.push_back(n);
    }

    IsPrime = (Factors.size() == 1 && Factors[0] == Value);
}

std::vector<uint32_t> FPartitionPart::GetTensorShape() const {
    if (Factors.empty() || IsPrime) {
        return {Value};
    }
    return Factors;
}

std::string FPartitionPart::ToString() const {
    std::ostringstream oss;
    if (Factors.size() <= 1 || IsPrime) {
        oss << "[" << Value << "]";
    } else {
        oss << "[";
        for (size_t i = 0; i < Factors.size(); i++) {
            if (i > 0) oss << ",";
            oss << Factors[i];
        }
        oss << "]";
    }
    return oss.str();
}

// ============================================================================
// FNestedPartition Implementation
// ============================================================================

FNestedPartition::FNestedPartition(uint32_t InN, const std::vector<uint32_t>& InParts)
    : N(InN) {
    for (uint32_t part : InParts) {
        Parts.emplace_back(part);
    }

    // Compute multiplicities
    std::unordered_map<uint32_t, uint32_t> counts;
    for (const auto& part : Parts) {
        counts[part.Value]++;
    }
    for (const auto& [val, count] : counts) {
        Multiplicities.push_back(count);
    }
    std::sort(Multiplicities.begin(), Multiplicities.end(), std::greater<uint32_t>());

    ComputeSignature();
}

void FNestedPartition::ComputeSignature() {
    Signature = 1;
    for (const auto& part : Parts) {
        Signature *= part.PrimeWeight;
    }
}

std::vector<std::vector<uint32_t>> FNestedPartition::GetNestedTensorShape() const {
    std::vector<std::vector<uint32_t>> shape;
    for (const auto& part : Parts) {
        shape.push_back(part.GetTensorShape());
    }
    return shape;
}

FThreadPoolMapping FNestedPartition::GetThreadPoolMapping(EThreadPoolStrategy Strategy) const {
    FThreadPoolMapping mapping;
    mapping.TotalThreads = N;
    mapping.Strategy = Strategy;

    switch (Strategy) {
        case EThreadPoolStrategy::InterOp:
            // Each part is a separate segment
            for (const auto& part : Parts) {
                mapping.SegmentLengths.push_back(part.Value);
                mapping.Grid.push_back({part.Value});
            }
            break;

        case EThreadPoolStrategy::IntraOp:
            // Use factorization for grid dimensions
            mapping.SegmentLengths.push_back(N);
            {
                std::vector<uint32_t> grid;
                for (const auto& part : Parts) {
                    auto shape = part.GetTensorShape();
                    grid.insert(grid.end(), shape.begin(), shape.end());
                }
                mapping.Grid.push_back(grid);
            }
            break;

        case EThreadPoolStrategy::Hybrid:
            // Combine: segments for parts, grids for factorization
            for (const auto& part : Parts) {
                mapping.SegmentLengths.push_back(part.Value);
                mapping.Grid.push_back(part.GetTensorShape());
            }
            break;
    }

    return mapping;
}

bool FNestedPartition::Refines(const FNestedPartition& Other) const {
    if (N != Other.N) return false;
    if (Parts.size() <= Other.Parts.size()) return false;

    // A partition refines another if it can be obtained by splitting parts
    // This is a simplified check - full refinement requires more complex logic
    return true;  // Placeholder
}

std::vector<FNestedPartition> FNestedPartition::GetRefinements() const {
    std::vector<FNestedPartition> refinements;

    // For each part > 1, try splitting it
    for (size_t i = 0; i < Parts.size(); i++) {
        if (Parts[i].Value > 1) {
            uint32_t val = Parts[i].Value;
            // Split into k + (val-k) for k from 1 to val/2
            for (uint32_t k = 1; k <= val / 2; k++) {
                std::vector<uint32_t> newParts;
                for (size_t j = 0; j < Parts.size(); j++) {
                    if (j == i) {
                        newParts.push_back(k);
                        newParts.push_back(val - k);
                    } else {
                        newParts.push_back(Parts[j].Value);
                    }
                }
                std::sort(newParts.begin(), newParts.end(), std::greater<uint32_t>());
                refinements.emplace_back(N, newParts);
            }
        }
    }

    // Remove duplicates by signature
    std::set<uint64_t> seen;
    std::vector<FNestedPartition> unique;
    for (auto& ref : refinements) {
        if (seen.find(ref.Signature) == seen.end()) {
            seen.insert(ref.Signature);
            unique.push_back(std::move(ref));
        }
    }

    return unique;
}

std::string FNestedPartition::ToString() const {
    std::ostringstream oss;
    oss << N << " = ";
    for (size_t i = 0; i < Parts.size(); i++) {
        if (i > 0) oss << " + ";
        oss << Parts[i].Value;
    }
    oss << " -> [";
    for (size_t i = 0; i < Parts.size(); i++) {
        if (i > 0) oss << ", ";
        oss << Parts[i].ToString();
    }
    oss << "] (sig=" << Signature << ")";
    return oss.str();
}

// ============================================================================
// FThreadPoolMapping Implementation
// ============================================================================

std::vector<uint32_t> FThreadPoolMapping::GetFlatIndices() const {
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < TotalThreads; i++) {
        indices.push_back(i);
    }
    return indices;
}

std::vector<uint32_t> FThreadPoolMapping::GetGridCoordinates(uint32_t FlatIndex) const {
    std::vector<uint32_t> coords;
    if (Grid.empty()) return coords;

    // Find which segment
    uint32_t offset = 0;
    size_t segmentIdx = 0;
    for (size_t i = 0; i < SegmentLengths.size(); i++) {
        if (FlatIndex < offset + SegmentLengths[i]) {
            segmentIdx = i;
            break;
        }
        offset += SegmentLengths[i];
    }

    uint32_t localIdx = FlatIndex - offset;
    const auto& grid = Grid[segmentIdx];

    // Convert to grid coordinates
    for (int i = static_cast<int>(grid.size()) - 1; i >= 0; i--) {
        coords.insert(coords.begin(), localIdx % grid[i]);
        localIdx /= grid[i];
    }

    return coords;
}

std::string FThreadPoolMapping::ToString() const {
    std::ostringstream oss;
    oss << "ThreadPool[" << TotalThreads << "]: ";
    for (size_t i = 0; i < SegmentLengths.size(); i++) {
        if (i > 0) oss << " + ";
        oss << SegmentLengths[i];
        if (!Grid.empty() && i < Grid.size() && Grid[i].size() > 1) {
            oss << "(";
            for (size_t j = 0; j < Grid[i].size(); j++) {
                if (j > 0) oss << "x";
                oss << Grid[i][j];
            }
            oss << ")";
        }
    }
    return oss.str();
}

// ============================================================================
// FCognitiveSchedule Implementation
// ============================================================================

void FCognitiveSchedule::Initialize() {
    // Stream names
    StreamNames = {"Cerebral", "Somatic", "Autonomic"};

    // Triad step groups with 120-degree phase separation
    TriadSteps[0] = {1, 5, 9};    // Phase 0
    TriadSteps[1] = {2, 6, 10};   // Phase 30
    TriadSteps[2] = {3, 7, 11};   // Phase 60
    TriadSteps[3] = {4, 8, 12};   // Phase 90

    // Phase offsets
    PhaseOffsets = {0.0f, 30.0f, 60.0f, 90.0f};

    // Default partition: 9 units per triad = [3][3]
    for (uint32_t i = 0; i < NumTriads; i++) {
        TriadPartitions[i] = FNestedPartition(9, {3, 3, 3});
    }
}

FNestedPartition FCognitiveSchedule::GetStepPartition(uint32_t Step) const {
    uint32_t triadIdx = GetTriadIndex(Step);
    return TriadPartitions[triadIdx];
}

uint32_t FCognitiveSchedule::GetStreamIndex(uint32_t Step) const {
    // Steps 1,2,3,4 -> stream 0,0,0,0 (first occurrence in each triad)
    // Steps 5,6,7,8 -> stream 1,1,1,1 (second occurrence)
    // Steps 9,10,11,12 -> stream 2,2,2,2 (third occurrence)
    return ((Step - 1) / NumTriads) % NumStreams;
}

uint32_t FCognitiveSchedule::GetTriadIndex(uint32_t Step) const {
    return (Step - 1) % NumTriads;
}

// ============================================================================
// FPartitionLattice Implementation
// ============================================================================

FPartitionLattice::FPartitionLattice(uint32_t InN) {
    Build(InN);
}

void FPartitionLattice::Build(uint32_t InN) {
    N = InN;
    Partitions.clear();
    SignatureToIndex.clear();
    RefinementEdges.clear();

    std::vector<uint32_t> current;
    GeneratePartitions(N, N, current);

    // Sort by signature
    std::sort(Partitions.begin(), Partitions.end());

    // Build index map
    for (size_t i = 0; i < Partitions.size(); i++) {
        SignatureToIndex[Partitions[i].Signature] = i;
    }

    BuildRefinementEdges();
}

void FPartitionLattice::GeneratePartitions(uint32_t n, uint32_t maxPart,
                                            std::vector<uint32_t>& current) {
    if (n == 0) {
        Partitions.emplace_back(N, current);
        return;
    }

    for (uint32_t part = std::min(n, maxPart); part >= 1; part--) {
        current.push_back(part);
        GeneratePartitions(n - part, part, current);
        current.pop_back();
    }
}

void FPartitionLattice::BuildRefinementEdges() {
    RefinementEdges.resize(Partitions.size());

    for (size_t i = 0; i < Partitions.size(); i++) {
        auto refinements = Partitions[i].GetRefinements();
        for (const auto& ref : refinements) {
            auto it = SignatureToIndex.find(ref.Signature);
            if (it != SignatureToIndex.end()) {
                RefinementEdges[i].push_back(it->second);
            }
        }
    }
}

std::vector<std::vector<uint64_t>> FPartitionLattice::GetStateMatrix() const {
    size_t n = Partitions.size();
    std::vector<std::vector<uint64_t>> matrix(n, std::vector<uint64_t>(n, 0));
    for (size_t i = 0; i < n; i++) {
        matrix[i][i] = Partitions[i].Signature;
    }
    return matrix;
}

std::vector<std::vector<uint32_t>> FPartitionLattice::GetRefinementMatrix() const {
    size_t n = Partitions.size();
    std::vector<std::vector<uint32_t>> matrix(n, std::vector<uint32_t>(n, 0));
    for (size_t i = 0; i < n; i++) {
        for (size_t j : RefinementEdges[i]) {
            matrix[i][j] = 1;
        }
    }
    return matrix;
}

std::vector<std::vector<uint64_t>> FPartitionLattice::GetTaggedRefinementMatrix() const {
    size_t n = Partitions.size();
    std::vector<std::vector<uint64_t>> matrix(n, std::vector<uint64_t>(n, 0));
    for (size_t i = 0; i < n; i++) {
        for (size_t j : RefinementEdges[i]) {
            // Edge weight is the signature of the target
            matrix[i][j] = Partitions[j].Signature;
        }
    }
    return matrix;
}

const FNestedPartition* FPartitionLattice::FindBySignature(uint64_t Signature) const {
    auto it = SignatureToIndex.find(Signature);
    if (it != SignatureToIndex.end()) {
        return &Partitions[it->second];
    }
    return nullptr;
}

std::vector<const FNestedPartition*> FPartitionLattice::GetPartitionsAtDepth(uint32_t Depth) const {
    std::vector<const FNestedPartition*> result;
    for (const auto& p : Partitions) {
        if (p.Parts.size() == Depth) {
            result.push_back(&p);
        }
    }
    return result;
}

std::vector<const FNestedPartition*> FPartitionLattice::GetRefinementPath(
    const FNestedPartition& Coarse,
    const FNestedPartition& Fine) const {

    std::vector<const FNestedPartition*> path;

    auto coarseIt = SignatureToIndex.find(Coarse.Signature);
    auto fineIt = SignatureToIndex.find(Fine.Signature);
    if (coarseIt == SignatureToIndex.end() || fineIt == SignatureToIndex.end()) {
        return path;
    }

    // BFS to find path
    std::queue<size_t> queue;
    std::unordered_map<size_t, size_t> parent;
    queue.push(coarseIt->second);
    parent[coarseIt->second] = coarseIt->second;

    while (!queue.empty()) {
        size_t current = queue.front();
        queue.pop();

        if (current == fineIt->second) {
            // Reconstruct path
            size_t node = current;
            while (node != coarseIt->second) {
                path.push_back(&Partitions[node]);
                node = parent[node];
            }
            path.push_back(&Partitions[coarseIt->second]);
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (size_t next : RefinementEdges[current]) {
            if (parent.find(next) == parent.end()) {
                parent[next] = current;
                queue.push(next);
            }
        }
    }

    return path;
}

std::vector<const FNestedPartition*> FPartitionLattice::TopologicalSort() const {
    std::vector<const FNestedPartition*> sorted;
    std::vector<uint32_t> inDegree(Partitions.size(), 0);

    // Count in-degrees
    for (size_t i = 0; i < RefinementEdges.size(); i++) {
        for (size_t j : RefinementEdges[i]) {
            inDegree[j]++;
        }
    }

    // Start with nodes that have no incoming edges
    std::queue<size_t> queue;
    for (size_t i = 0; i < inDegree.size(); i++) {
        if (inDegree[i] == 0) {
            queue.push(i);
        }
    }

    while (!queue.empty()) {
        size_t current = queue.front();
        queue.pop();
        sorted.push_back(&Partitions[current]);

        for (size_t next : RefinementEdges[current]) {
            inDegree[next]--;
            if (inDegree[next] == 0) {
                queue.push(next);
            }
        }
    }

    return sorted;
}

// ============================================================================
// FNestedTensorPartitionSystem Implementation
// ============================================================================

FNestedTensorPartitionSystem::FNestedTensorPartitionSystem() {
    InitializeA000081Sequence();
}

FNestedTensorPartitionSystem::~FNestedTensorPartitionSystem() = default;

void FNestedTensorPartitionSystem::Initialize() {
    if (bInitialized) return;

    // Pre-cache common lattices
    for (uint32_t n = 1; n <= 12; n++) {
        LatticeCache[n] = FPartitionLattice(n);
    }

    bInitialized = true;
}

void FNestedTensorPartitionSystem::Reset() {
    LatticeCache.clear();
    bInitialized = false;
}

void FNestedTensorPartitionSystem::InitializeA000081Sequence() {
    // OEIS A000081: Number of unlabeled rooted trees with n nodes
    A000081Sequence = {
        0,   // A(0) = 0 (placeholder)
        1,   // A(1) = 1
        1,   // A(2) = 1
        2,   // A(3) = 2
        4,   // A(4) = 4
        9,   // A(5) = 9
        20,  // A(6) = 20
        48,  // A(7) = 48
        115, // A(8) = 115
        286, // A(9) = 286
        719, // A(10) = 719
        1842, // A(11) = 1842
        4766  // A(12) = 4766
    };
}

std::vector<FNestedPartition> FNestedTensorPartitionSystem::GeneratePartitions(uint32_t N) {
    const auto& lattice = GetLattice(N);
    const auto& partitions = lattice.GetPartitions();

    std::vector<FNestedPartition> result;
    for (const auto& p : partitions) {
        result.push_back(p);
        if (OnPartitionGenerated) {
            OnPartitionGenerated(p);
        }
    }
    return result;
}

FNestedPartition FNestedTensorPartitionSystem::DecomposeSix() {
    // [6] = [2][3] - the canonical example
    return FNestedPartition(6, {6});  // Creates [6] which internally factorizes to [2,3]
}

FNestedPartition FNestedTensorPartitionSystem::DecomposeComposite(uint32_t N) {
    return FNestedPartition(N, {N});
}

const FPartitionLattice& FNestedTensorPartitionSystem::GetLattice(uint32_t N) {
    auto it = LatticeCache.find(N);
    if (it == LatticeCache.end()) {
        LatticeCache[N] = FPartitionLattice(N);
    }
    return LatticeCache[N];
}

uint64_t FNestedTensorPartitionSystem::ComputePrimeWeight(const FNestedPartition& Partition) const {
    return Partition.Signature;
}

uint64_t FNestedTensorPartitionSystem::ComputeCognitiveSignature(
    const std::vector<uint32_t>& StateVector) const {
    FPrimeWeightedSignature sig;
    sig.ComputeFromComponents(StateVector);
    return sig.ProductSignature;
}

std::vector<uint32_t> FNestedTensorPartitionSystem::DecodeSignature(uint64_t Signature) const {
    FPrimeWeightedSignature sig;
    sig.ProductSignature = Signature;
    auto factors = sig.Factorize();

    std::vector<uint32_t> components;
    const auto& primes = FPrimeWeightedSignature::GetPrimes();

    for (const auto& [prime, count] : factors) {
        // Find which k gives tau(k) = prime
        for (uint32_t k = 1; k <= 100; k++) {
            if (FPrimeWeightedSignature::Tau(k) == prime) {
                for (uint32_t c = 0; c < count; c++) {
                    components.push_back(k);
                }
                break;
            }
        }
    }

    return components;
}

FThreadPoolMapping FNestedTensorPartitionSystem::MapToThreadPool(
    const FNestedPartition& Partition,
    EThreadPoolStrategy Strategy) {
    return Partition.GetThreadPoolMapping(Strategy);
}

FCognitiveSchedule FNestedTensorPartitionSystem::GenerateCognitiveSchedule(uint32_t TotalWorkload) {
    FCognitiveSchedule schedule;
    schedule.Initialize();

    // Adjust partitions based on total workload
    uint32_t workloadPerTriad = TotalWorkload / FCognitiveSchedule::NumTriads;
    for (uint32_t i = 0; i < FCognitiveSchedule::NumTriads; i++) {
        // Find best partition for this workload
        schedule.TriadPartitions[i] = GetOptimalPartition(workloadPerTriad, 3);
    }

    return schedule;
}

FNestedPartition FNestedTensorPartitionSystem::GetOptimalPartition(
    uint32_t Workload, uint32_t TargetParallelism) {
    const auto& lattice = GetLattice(Workload);
    const auto& partitions = lattice.GetPartitions();

    // Find partition with number of parts closest to target parallelism
    const FNestedPartition* best = nullptr;
    int32_t bestDiff = INT32_MAX;

    for (const auto& p : partitions) {
        int32_t diff = std::abs(static_cast<int32_t>(p.Parts.size()) -
                               static_cast<int32_t>(TargetParallelism));
        if (diff < bestDiff) {
            bestDiff = diff;
            best = &p;
        }
    }

    return best ? *best : FNestedPartition(Workload, {Workload});
}

std::vector<std::vector<uint64_t>> FNestedTensorPartitionSystem::GetObjectiveMatrix(uint32_t N) {
    const auto& lattice = GetLattice(N);
    const auto& partitions = lattice.GetPartitions();

    size_t n = partitions.size();
    std::vector<std::vector<uint64_t>> matrix(n, std::vector<uint64_t>(n, 0));

    // Objective = explicit multisets (partitions with multiple parts)
    for (size_t i = 0; i < n; i++) {
        if (partitions[i].Parts.size() > 1) {
            matrix[i][i] = partitions[i].Signature;
        }
    }

    return matrix;
}

std::vector<std::vector<uint64_t>> FNestedTensorPartitionSystem::GetSubjectiveMatrix(uint32_t N) {
    const auto& lattice = GetLattice(N);
    const auto& partitions = lattice.GetPartitions();

    size_t n = partitions.size();
    std::vector<std::vector<uint64_t>> matrix(n, std::vector<uint64_t>(n, 0));

    // Subjective = pure parts (single part partitions)
    for (size_t i = 0; i < n; i++) {
        if (partitions[i].Parts.size() == 1) {
            matrix[i][i] = partitions[i].Signature;
        }
    }

    return matrix;
}

std::vector<uint64_t> FNestedTensorPartitionSystem::ObjectiveToSubjective(
    const std::vector<uint64_t>& Objective) {
    // Codec: Tensorize - convert explicit multisets to nested tensors
    std::vector<uint64_t> subjective = Objective;
    // Transformation logic here
    return subjective;
}

std::vector<uint64_t> FNestedTensorPartitionSystem::SubjectiveToObjective(
    const std::vector<uint64_t>& Subjective) {
    // Codec: Graphize - convert nested tensors to explicit multisets
    std::vector<uint64_t> objective = Subjective;
    // Transformation logic here
    return objective;
}

uint32_t FNestedTensorPartitionSystem::GetA000081TermCount(uint32_t NestingLevel) const {
    if (NestingLevel < A000081Sequence.size()) {
        return A000081Sequence[NestingLevel];
    }
    return 0;
}

std::vector<uint32_t> FNestedTensorPartitionSystem::GetNestingLevelTermCounts(uint32_t MaxLevel) const {
    std::vector<uint32_t> counts;
    for (uint32_t i = 1; i <= MaxLevel && i < A000081Sequence.size(); i++) {
        counts.push_back(A000081Sequence[i]);
    }
    return counts;
}

bool FNestedTensorPartitionSystem::VerifyA000081Alignment() const {
    // Verify: nesting level N → A000081(N) terms
    // N=1: 1 term, N=2: 2 terms, N=3: 4 terms, N=4: 9 terms
    std::vector<uint32_t> expected = {1, 2, 4, 9};

    for (size_t i = 0; i < expected.size(); i++) {
        uint32_t termCount = 0;
        if (i + 1 < A000081Sequence.size()) {
            termCount = A000081Sequence[i + 1];
        }
        if (termCount != expected[i]) {
            return false;  // Alignment mismatch
        }
    }
    return true;
}

bool FNestedTensorPartitionSystem::IsPrime(uint32_t N) const {
    if (N < 2) return false;
    if (N == 2) return true;
    if (N % 2 == 0) return false;
    for (uint32_t i = 3; i * i <= N; i += 2) {
        if (N % i == 0) return false;
    }
    return true;
}

std::vector<uint32_t> FNestedTensorPartitionSystem::GetPrimeFactors(uint32_t N) const {
    std::vector<uint32_t> factors;
    for (uint32_t p = 2; p * p <= N; p++) {
        while (N % p == 0) {
            factors.push_back(p);
            N /= p;
        }
    }
    if (N > 1) {
        factors.push_back(N);
    }
    return factors;
}

} // namespace DeepTreeEcho
