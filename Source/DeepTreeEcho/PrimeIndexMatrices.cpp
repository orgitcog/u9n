/**
 * @file PrimeIndexMatrices.cpp
 * @brief Implementation of Prime-Index Coordinate Matrices for Deep Tree Echo
 */

#include "PrimeIndexMatrices.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>

namespace DeepTreeEcho {

//=============================================================================
// PrimeOracle Implementation
//=============================================================================

PrimeOracle::PrimeOracle(size_t maxPrimes) {
    generatePrimes(maxPrimes);
}

void PrimeOracle::generatePrimes(size_t count) {
    primes_.clear();
    primes_.reserve(count);
    
    uint64_t candidate = 2;
    while (primes_.size() < count) {
        bool isPrimeCandidate = true;
        uint64_t sqrtCandidate = static_cast<uint64_t>(std::sqrt(candidate));
        
        for (uint64_t p : primes_) {
            if (p > sqrtCandidate) break;
            if (candidate % p == 0) {
                isPrimeCandidate = false;
                break;
            }
        }
        
        if (isPrimeCandidate) {
            primes_.push_back(candidate);
        }
        candidate++;
    }
}

uint64_t PrimeOracle::pi(size_t k) const {
    if (k >= primes_.size()) {
        throw std::out_of_range("Prime index out of range");
    }
    return primes_[k];
}

uint64_t PrimeOracle::tau(size_t k) const {
    if (k == 0) {
        throw std::invalid_argument("Part tag τ(k) requires k >= 1");
    }
    return pi(k - 1);
}

bool PrimeOracle::isPrime(uint64_t n) const {
    if (n < 2) return false;
    for (uint64_t p : primes_) {
        if (p * p > n) break;
        if (n % p == 0) return n == p;
    }
    return true;
}

int PrimeOracle::primeIndex(uint64_t p) const {
    auto it = std::find(primes_.begin(), primes_.end(), p);
    if (it == primes_.end()) return -1;
    return static_cast<int>(std::distance(primes_.begin(), it));
}

std::vector<uint64_t> PrimeOracle::primesUpTo(uint64_t n) const {
    std::vector<uint64_t> result;
    for (uint64_t p : primes_) {
        if (p > n) break;
        result.push_back(p);
    }
    return result;
}

//=============================================================================
// PrimePartition Implementation
//=============================================================================

std::tuple<int, int, int, int, int> PrimePartition::dimSignature() const {
    return std::make_tuple(n, length, maxPart, n - length, length - 1);
}

std::string PrimePartition::toString() const {
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << ",";
        oss << parts[i];
    }
    oss << ")";
    return oss.str();
}

bool PrimePartition::refines(const PrimePartition& other) const {
    if (n != other.n) return false;
    if (length <= other.length) return false;
    
    // Check if this partition can be obtained by splitting parts of other
    std::vector<int> remaining = other.parts;
    for (int part : parts) {
        bool found = false;
        for (size_t i = 0; i < remaining.size(); ++i) {
            if (remaining[i] >= part) {
                remaining[i] -= part;
                if (remaining[i] == 0) {
                    remaining.erase(remaining.begin() + i);
                }
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return remaining.empty() || std::all_of(remaining.begin(), remaining.end(), 
                                            [](int x) { return x == 0; });
}

//=============================================================================
// PartitionGenerator Implementation
//=============================================================================

PartitionGenerator::PartitionGenerator(const PrimeOracle& oracle) 
    : oracle_(oracle) {}

std::vector<PrimePartition> PartitionGenerator::generate(int n) const {
    std::vector<PrimePartition> result;
    std::vector<int> current;
    generateRecursive(n, n, current, result);
    return result;
}

std::vector<PrimePartition> PartitionGenerator::generateOrdered(int n) const {
    auto partitions = generate(n);
    std::sort(partitions.begin(), partitions.end(),
              [](const PrimePartition& a, const PrimePartition& b) {
                  return a.weight > b.weight;  // Descending by weight
              });
    return partitions;
}

int PartitionGenerator::count(int n) const {
    return static_cast<int>(generate(n).size());
}

void PartitionGenerator::generateRecursive(int n, int maxPart,
                                           std::vector<int>& current,
                                           std::vector<PrimePartition>& result) const {
    if (n == 0) {
        PrimePartition p;
        p.parts = current;
        p.n = 0;
        for (int part : current) p.n += part;
        p.length = static_cast<int>(current.size());
        p.maxPart = current.empty() ? 0 : current[0];
        p.weight = computeWeight(current);
        result.push_back(p);
        return;
    }
    
    for (int part = std::min(n, maxPart); part >= 1; --part) {
        current.push_back(part);
        generateRecursive(n - part, part, current, result);
        current.pop_back();
    }
}

uint64_t PartitionGenerator::computeWeight(const std::vector<int>& parts) const {
    uint64_t weight = 1;
    for (int part : parts) {
        weight *= oracle_.tau(part);
    }
    return weight;
}

//=============================================================================
// SparseMatrix Implementation
//=============================================================================

template<typename T>
SparseMatrix<T>::SparseMatrix(size_t rows, size_t cols) 
    : rows_(rows), cols_(cols) {}

template<typename T>
void SparseMatrix<T>::set(size_t i, size_t j, T value) {
    if (value != T{}) {
        data_[{i, j}] = value;
    } else {
        data_.erase({i, j});
    }
}

template<typename T>
T SparseMatrix<T>::get(size_t i, size_t j) const {
    auto it = data_.find({i, j});
    return (it != data_.end()) ? it->second : T{};
}

template<typename T>
std::vector<std::vector<T>> SparseMatrix<T>::toDense() const {
    std::vector<std::vector<T>> result(rows_, std::vector<T>(cols_, T{}));
    for (const auto& [pos, val] : data_) {
        result[pos.first][pos.second] = val;
    }
    return result;
}

template<typename T>
SparseMatrix<T> SparseMatrix<T>::operator+(const SparseMatrix<T>& other) const {
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Matrix dimensions must match for addition");
    }
    
    SparseMatrix<T> result(rows_, cols_);
    for (const auto& [pos, val] : data_) {
        result.set(pos.first, pos.second, val);
    }
    for (const auto& [pos, val] : other.data_) {
        T current = result.get(pos.first, pos.second);
        result.set(pos.first, pos.second, current + val);
    }
    return result;
}

template<typename T>
SparseMatrix<T> SparseMatrix<T>::operator*(const SparseMatrix<T>& other) const {
    if (cols_ != other.rows_) {
        throw std::invalid_argument("Matrix dimensions incompatible for multiplication");
    }
    
    SparseMatrix<T> result(rows_, other.cols_);
    for (const auto& [posA, valA] : data_) {
        for (const auto& [posB, valB] : other.data_) {
            if (posA.second == posB.first) {
                T current = result.get(posA.first, posB.second);
                result.set(posA.first, posB.second, current + valA * valB);
            }
        }
    }
    return result;
}

template<typename T>
SparseMatrix<T> SparseMatrix<T>::directSum(const SparseMatrix<T>& a, 
                                           const SparseMatrix<T>& b) {
    SparseMatrix<T> result(a.rows_ + b.rows_, a.cols_ + b.cols_);
    
    for (const auto& [pos, val] : a.data_) {
        result.set(pos.first, pos.second, val);
    }
    for (const auto& [pos, val] : b.data_) {
        result.set(pos.first + a.rows_, pos.second + a.cols_, val);
    }
    return result;
}

template<typename T>
std::string SparseMatrix<T>::toString() const {
    auto dense = toDense();
    std::ostringstream oss;
    
    for (size_t i = 0; i < rows_; ++i) {
        oss << "| ";
        for (size_t j = 0; j < cols_; ++j) {
            oss << std::setw(4) << dense[i][j] << " ";
        }
        oss << "|\n";
    }
    return oss.str();
}

// Explicit template instantiations
template class SparseMatrix<uint64_t>;
template class SparseMatrix<double>;
template class SparseMatrix<int>;

//=============================================================================
// StateMatrix Implementation
//=============================================================================

StateMatrix::StateMatrix(int n, const PrimeOracle& oracle) 
    : n_(n), oracle_(oracle), matrix_(0, 0) {
    
    PartitionGenerator gen(oracle);
    basis_ = gen.generateOrdered(n);
    
    size_t dim = basis_.size();
    matrix_ = SparseMatrix<uint64_t>(dim, dim);
    
    for (size_t i = 0; i < dim; ++i) {
        matrix_.set(i, i, basis_[i].weight);
    }
}

uint64_t StateMatrix::weight(size_t idx) const {
    if (idx >= basis_.size()) {
        throw std::out_of_range("Partition index out of range");
    }
    return basis_[idx].weight;
}

std::string StateMatrix::toString() const {
    std::ostringstream oss;
    oss << "State Matrix S_" << n_ << " (dim=" << dim() << "):\n";
    oss << "Basis: ";
    for (const auto& p : basis_) {
        oss << p.toString() << "→" << p.weight << " ";
    }
    oss << "\n\n" << matrix_.toString();
    return oss.str();
}

//=============================================================================
// RefinementMatrix Implementation
//=============================================================================

RefinementMatrix::RefinementMatrix(int n, const PrimeOracle& oracle, bool tagged)
    : n_(n), tagged_(tagged), oracle_(oracle), matrix_(0, 0) {
    
    PartitionGenerator gen(oracle);
    basis_ = gen.generateOrdered(n);
    
    size_t dim = basis_.size();
    matrix_ = SparseMatrix<uint64_t>(dim, dim);
    
    // Build refinement edges
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < dim; ++j) {
            if (i != j && isSingleSplit(basis_[i], basis_[j])) {
                if (tagged_) {
                    matrix_.set(i, j, computeSplitTag(basis_[i], basis_[j]));
                } else {
                    matrix_.set(i, j, 1);
                }
            }
        }
    }
}

bool RefinementMatrix::canRefine(size_t i, size_t j) const {
    return matrix_.get(i, j) != 0;
}

uint64_t RefinementMatrix::refinementTag(size_t i, size_t j) const {
    return matrix_.get(i, j);
}

bool RefinementMatrix::isSingleSplit(const PrimePartition& a, 
                                     const PrimePartition& b) const {
    // b must have exactly one more part than a
    if (b.length != a.length + 1) return false;
    
    // Find the part in a that was split
    std::vector<int> aParts = a.parts;
    std::vector<int> bParts = b.parts;
    
    std::sort(aParts.begin(), aParts.end(), std::greater<int>());
    std::sort(bParts.begin(), bParts.end(), std::greater<int>());
    
    // Count differences
    std::vector<int> removed, added;
    size_t ai = 0, bi = 0;
    
    while (ai < aParts.size() && bi < bParts.size()) {
        if (aParts[ai] == bParts[bi]) {
            ai++; bi++;
        } else if (aParts[ai] > bParts[bi]) {
            removed.push_back(aParts[ai]);
            ai++;
        } else {
            added.push_back(bParts[bi]);
            bi++;
        }
    }
    while (ai < aParts.size()) removed.push_back(aParts[ai++]);
    while (bi < bParts.size()) added.push_back(bParts[bi++]);
    
    // Valid single split: one part removed, two parts added that sum to it
    if (removed.size() != 1 || added.size() != 2) return false;
    return removed[0] == added[0] + added[1];
}

uint64_t RefinementMatrix::computeSplitTag(const PrimePartition& a, 
                                           const PrimePartition& b) const {
    // Find the two new parts introduced by the split
    std::vector<int> aParts = a.parts;
    std::vector<int> bParts = b.parts;
    
    std::sort(aParts.begin(), aParts.end(), std::greater<int>());
    std::sort(bParts.begin(), bParts.end(), std::greater<int>());
    
    std::vector<int> added;
    size_t ai = 0, bi = 0;
    
    while (ai < aParts.size() && bi < bParts.size()) {
        if (aParts[ai] == bParts[bi]) {
            ai++; bi++;
        } else if (aParts[ai] > bParts[bi]) {
            ai++;
        } else {
            added.push_back(bParts[bi]);
            bi++;
        }
    }
    while (bi < bParts.size()) added.push_back(bParts[bi++]);
    
    // Compute tag as product of τ(new parts)
    uint64_t tag = 1;
    for (int part : added) {
        tag *= oracle_.tau(part);
    }
    return tag;
}

std::vector<std::vector<size_t>> RefinementMatrix::allPaths() const {
    std::vector<std::vector<size_t>> paths;
    
    // Find source (coarsest partition - single part)
    size_t source = 0;
    for (size_t i = 0; i < basis_.size(); ++i) {
        if (basis_[i].isPure()) {
            source = i;
            break;
        }
    }
    
    // Find sink (finest partition - all 1s)
    size_t sink = 0;
    for (size_t i = 0; i < basis_.size(); ++i) {
        if (basis_[i].isAtomic()) {
            sink = i;
            break;
        }
    }
    
    // DFS to find all paths
    std::function<void(size_t, std::vector<size_t>&)> dfs = 
        [&](size_t node, std::vector<size_t>& path) {
            path.push_back(node);
            if (node == sink) {
                paths.push_back(path);
            } else {
                for (size_t next = 0; next < basis_.size(); ++next) {
                    if (canRefine(node, next)) {
                        dfs(next, path);
                    }
                }
            }
            path.pop_back();
        };
    
    std::vector<size_t> path;
    dfs(source, path);
    return paths;
}

std::string RefinementMatrix::toString() const {
    std::ostringstream oss;
    oss << "Refinement Matrix R_" << n_;
    if (tagged_) oss << "^(tagged)";
    oss << " (dim=" << basis_.size() << "):\n";
    oss << "Basis: ";
    for (const auto& p : basis_) {
        oss << p.toString() << " ";
    }
    oss << "\n\n" << matrix_.toString();
    return oss.str();
}

//=============================================================================
// DecomposedMatrix Implementation
//=============================================================================

DecomposedMatrix::DecomposedMatrix(int n, const PrimeOracle& oracle)
    : n_(n), oracle_(oracle), objective_(0, 0), subjective_(0, 0) {
    decompose();
}

void DecomposedMatrix::decompose() {
    PartitionGenerator gen(oracle_);
    auto allPartitions = gen.generateOrdered(n_);
    
    // Separate into objective (mixed) and subjective (pure)
    for (const auto& p : allPartitions) {
        if (p.isPure()) {
            subjBasis_.push_back(p);
        } else {
            objBasis_.push_back(p);
        }
    }
    
    // Build objective matrix
    objective_ = SparseMatrix<uint64_t>(objBasis_.size(), objBasis_.size());
    for (size_t i = 0; i < objBasis_.size(); ++i) {
        objective_.set(i, i, objBasis_[i].weight);
    }
    
    // Build subjective matrix
    // For n >= 3, there can be multiple subjective channels
    // Base pure: p_n
    // Nested concurrent: additional channels based on nesting structure
    
    // For simplicity, we use the pure partition weight
    // Extended subjective channels would require additional analysis
    subjective_ = SparseMatrix<uint64_t>(subjBasis_.size(), subjBasis_.size());
    for (size_t i = 0; i < subjBasis_.size(); ++i) {
        subjective_.set(i, i, subjBasis_[i].weight);
    }
}

SparseMatrix<uint64_t> DecomposedMatrix::full(bool directSum) const {
    if (directSum) {
        return SparseMatrix<uint64_t>::directSum(objective_, subjective_);
    } else {
        // Merge into single matrix over all partitions
        size_t totalDim = objBasis_.size() + subjBasis_.size();
        SparseMatrix<uint64_t> result(totalDim, totalDim);
        
        for (size_t i = 0; i < objBasis_.size(); ++i) {
            result.set(i, i, objBasis_[i].weight);
        }
        for (size_t i = 0; i < subjBasis_.size(); ++i) {
            result.set(objBasis_.size() + i, objBasis_.size() + i, subjBasis_[i].weight);
        }
        return result;
    }
}

std::string DecomposedMatrix::toString() const {
    std::ostringstream oss;
    oss << "Decomposed Matrix M_" << n_ << " = O_" << n_ << " ⊕ S_" << n_ << "\n\n";
    
    oss << "Objective O_" << n_ << " (dim=" << objBasis_.size() << "):\n";
    oss << "Basis: ";
    for (const auto& p : objBasis_) {
        oss << p.toString() << "→" << p.weight << " ";
    }
    oss << "\n" << objective_.toString() << "\n";
    
    oss << "Subjective S_" << n_ << " (dim=" << subjBasis_.size() << "):\n";
    oss << "Basis: ";
    for (const auto& p : subjBasis_) {
        oss << p.toString() << "→" << p.weight << " ";
    }
    oss << "\n" << subjective_.toString() << "\n";
    
    oss << "Full M_" << n_ << " (direct sum):\n";
    oss << full(true).toString();
    
    return oss.str();
}

//=============================================================================
// NestorMatrixSystem Implementation
//=============================================================================

NestorMatrixSystem::NestorMatrixSystem(int n) : n_(n) {
    oracle_ = std::make_unique<PrimeOracle>(100);
    state_ = std::make_unique<StateMatrix>(n, *oracle_);
    refinement_ = std::make_unique<RefinementMatrix>(n, *oracle_, false);
    taggedRefinement_ = std::make_unique<RefinementMatrix>(n, *oracle_, true);
    decomposed_ = std::make_unique<DecomposedMatrix>(n, *oracle_);
}

std::string NestorMatrixSystem::report() const {
    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n";
    oss << "║          NESTOR MATRIX SYSTEM REPORT FOR n = " << std::setw(2) << n_ << "              ║\n";
    oss << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    oss << "Partition count p(" << n_ << ") = " << partitionCount() << "\n\n";
    
    oss << "═══════════════════════════════════════════════════════════════\n";
    oss << state_->toString() << "\n";
    
    oss << "═══════════════════════════════════════════════════════════════\n";
    oss << refinement_->toString() << "\n";
    
    oss << "═══════════════════════════════════════════════════════════════\n";
    oss << taggedRefinement_->toString() << "\n";
    
    oss << "═══════════════════════════════════════════════════════════════\n";
    oss << decomposed_->toString() << "\n";
    
    return oss.str();
}

std::string NestorMatrixSystem::toLatex() const {
    std::ostringstream oss;
    
    // State matrix
    oss << "S_{" << n_ << "} = \\begin{bmatrix}\n";
    auto stateMatrix = state_->matrix().toDense();
    for (size_t i = 0; i < stateMatrix.size(); ++i) {
        for (size_t j = 0; j < stateMatrix[i].size(); ++j) {
            if (j > 0) oss << " & ";
            oss << stateMatrix[i][j];
        }
        oss << " \\\\\n";
    }
    oss << "\\end{bmatrix}\n\n";
    
    // Refinement matrix
    oss << "R_{" << n_ << "} = \\begin{bmatrix}\n";
    auto refMatrix = refinement_->matrix().toDense();
    for (size_t i = 0; i < refMatrix.size(); ++i) {
        for (size_t j = 0; j < refMatrix[i].size(); ++j) {
            if (j > 0) oss << " & ";
            oss << refMatrix[i][j];
        }
        oss << " \\\\\n";
    }
    oss << "\\end{bmatrix}\n";
    
    return oss.str();
}

std::string NestorMatrixSystem::toMermaid() const {
    std::ostringstream oss;
    oss << "```mermaid\n";
    oss << "graph TD\n";
    
    const auto& basis = state_->basis();
    
    // Add nodes
    for (size_t i = 0; i < basis.size(); ++i) {
        oss << "    P" << i << "[\"" << basis[i].toString() 
            << "<br/>W=" << basis[i].weight << "\"]\n";
    }
    
    // Add edges from refinement matrix
    for (size_t i = 0; i < basis.size(); ++i) {
        for (size_t j = 0; j < basis.size(); ++j) {
            if (refinement_->canRefine(i, j)) {
                uint64_t tag = taggedRefinement_->refinementTag(i, j);
                oss << "    P" << i << " -->|\"" << tag << "\"| P" << j << "\n";
            }
        }
    }
    
    oss << "```\n";
    return oss.str();
}

std::string NestorMatrixSystem::toJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"n\": " << n_ << ",\n";
    oss << "  \"partitionCount\": " << partitionCount() << ",\n";
    
    // Partitions
    oss << "  \"partitions\": [\n";
    const auto& basis = state_->basis();
    for (size_t i = 0; i < basis.size(); ++i) {
        oss << "    {\"parts\": [";
        for (size_t j = 0; j < basis[i].parts.size(); ++j) {
            if (j > 0) oss << ", ";
            oss << basis[i].parts[j];
        }
        oss << "], \"weight\": " << basis[i].weight << "}";
        if (i < basis.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    
    // State matrix diagonal
    oss << "  \"stateWeights\": [";
    for (size_t i = 0; i < basis.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << basis[i].weight;
    }
    oss << "],\n";
    
    // Refinement edges
    oss << "  \"refinements\": [\n";
    bool first = true;
    for (size_t i = 0; i < basis.size(); ++i) {
        for (size_t j = 0; j < basis.size(); ++j) {
            if (refinement_->canRefine(i, j)) {
                if (!first) oss << ",\n";
                first = false;
                oss << "    {\"from\": " << i << ", \"to\": " << j 
                    << ", \"tag\": " << taggedRefinement_->refinementTag(i, j) << "}";
            }
        }
    }
    oss << "\n  ]\n";
    oss << "}\n";
    
    return oss.str();
}

//=============================================================================
// CognitiveMatrixMapper Implementation
//=============================================================================

CognitiveMatrixMapper::CognitiveMatrixMapper(const NestorMatrixSystem& system)
    : system_(system) {}

int CognitiveMatrixMapper::partitionToStream(const PrimePartition& p) const {
    // Map partition length to stream (mod 3)
    // Length 1 → Stream 0 (Cerebral)
    // Length 2 → Stream 1 (Somatic)
    // Length 3+ → Stream 2 (Autonomic)
    return (p.length - 1) % 3;
}

int CognitiveMatrixMapper::partitionToStep(const PrimePartition& p) const {
    // Map partition to 12-step loop
    // Based on weight modulo 12
    return static_cast<int>(p.weight % 12);
}

CognitiveMatrixMapper::ThreadConfig 
CognitiveMatrixMapper::partitionToThreadConfig(const PrimePartition& p) const {
    ThreadConfig config;
    
    // Inter-op threads = number of parts
    config.interOpThreads = p.length;
    
    // Intra-op threads = max part value
    config.intraOpThreads = p.maxPart;
    
    // Task shape = partition parts
    config.taskShape = p.parts;
    
    return config;
}

CognitiveMatrixMapper::CognitiveSchedule 
CognitiveMatrixMapper::generateSchedule(const std::vector<size_t>& path) const {
    CognitiveSchedule schedule;
    
    const auto& basis = system_.stateMatrix().basis();
    
    for (size_t idx : path) {
        const auto& p = basis[idx];
        schedule.steps.push_back(partitionToStep(p));
        schedule.streams.push_back(partitionToStream(p));
        schedule.configs.push_back(partitionToThreadConfig(p));
    }
    
    return schedule;
}

//=============================================================================
// TransjectiveMembrane Implementation
//=============================================================================

TransjectiveMembrane::TransjectiveMembrane(const CrossingPolicy& policy)
    : policy_(policy) {}

bool TransjectiveMembrane::canCross(MembraneLayer from, MembraneLayer to,
                                    const std::string& dataType) const {
    // Check direction permissions
    if (from == MembraneLayer::Outer && to == MembraneLayer::Inner) {
        if (!policy_.allowObjectiveToSubjective) return false;
    }
    if (from == MembraneLayer::Inner && to == MembraneLayer::Outer) {
        if (!policy_.allowSubjectiveToObjective) return false;
    }
    
    // Check energy budget
    if (policy_.energyBudget <= 0) return false;
    
    // Check capabilities (simplified)
    // In a full implementation, this would check dataType against required capabilities
    
    return true;
}

} // namespace DeepTreeEcho
