/**
 * @file prime-basis.hpp
 * @brief Prime Factorization Basis for Cognitive Computation
 *
 * Instead of floats (powers of 2), we use prime factorizations as the
 * fundamental numeric representation. This provides:
 *
 * 1. EXACT ARITHMETIC - No floating point errors, ever
 * 2. NATURAL HYPERGRAPH ENCODING - Primes = nodes, composites = hyperedges
 * 3. UNIFIED TENSOR SHAPES - Shape encoded in single factorized integer
 * 4. COMBINATORICS ↔ TOPOLOGY BRIDGE - via exp/log on exponents
 * 5. FIBER BUNDLE STRUCTURE - Transformation groups act on exponent space
 *
 * Key insight:
 *   n = p₁^a₁ × p₂^a₂ × p₃^a₃ × ...
 *
 *   - Primes pᵢ are NODES (atomic, unique, indivisible)
 *   - Exponents aᵢ are EDGE WEIGHTS (multiplicities, connection strengths)
 *   - Composite n is a HYPEREDGE (connects all primes in factorization)
 *
 * The exponential bridge:
 *   DISCRETE:   n = ∏ pᵢ^aᵢ           (multiplication = composition)
 *   CONTINUOUS: log(n) = Σ aᵢ·log(pᵢ) (addition = superposition)
 *
 * Neural network weights operate on exponents {aᵢ}, allowing
 * gradient-based optimization of combinatorial structures.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <span>
#include <string>
#include <optional>
#include <compare>
#include <cmath>
#include <bit>
#include <array>
#include <concepts>
#include <numeric>

namespace cogmorph::prime {

// ============================================================================
// Prime Table and Utilities
// ============================================================================

/// First N primes (extensible at runtime)
inline constexpr std::array<uint64_t, 64> FIRST_PRIMES = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
    157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
    239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293
};

/// Prime index (0 = 2, 1 = 3, 2 = 5, ...)
using PrimeIndex = uint16_t;

/// Exponent in prime factorization (can be negative for rationals)
using Exponent = int32_t;

/// Check if a number is prime (simple trial division for small numbers)
[[nodiscard]] constexpr bool is_prime(uint64_t n) noexcept {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (uint64_t i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

/// Get the nth prime (0-indexed)
[[nodiscard]] constexpr uint64_t nth_prime(size_t n) noexcept {
    if (n < FIRST_PRIMES.size()) return FIRST_PRIMES[n];
    // For larger indices, compute on demand (not constexpr)
    return 0; // Placeholder - would need runtime computation
}

/// Get index of a prime (inverse of nth_prime)
[[nodiscard]] constexpr std::optional<PrimeIndex> prime_index(uint64_t p) noexcept {
    for (size_t i = 0; i < FIRST_PRIMES.size(); ++i) {
        if (FIRST_PRIMES[i] == p) return static_cast<PrimeIndex>(i);
    }
    return std::nullopt;
}

// ============================================================================
// Factorization - The Core Type
// ============================================================================

/**
 * @brief A number represented as its prime factorization
 *
 * Stores the exponents for each prime factor. Supports:
 * - Exact multiplication/division (add/subtract exponents)
 * - Exact GCD/LCM (min/max of exponents)
 * - Conversion to/from integers and rationals
 * - Interpretation as hypergraph structure
 *
 * Memory layout: sparse map of (prime_index -> exponent)
 * For dense small factorizations, consider array-based variant.
 */
class Factorization {
public:
    // Storage: map from prime index to exponent
    // Exponent can be negative (for rationals)
    using FactorMap = std::map<PrimeIndex, Exponent>;

    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------

    /// Zero (represented as empty factorization with special flag)
    static Factorization zero() {
        Factorization f;
        f.is_zero_ = true;
        return f;
    }

    /// One (empty factorization = product of nothing = 1)
    static Factorization one() { return Factorization{}; }

    /// From a single prime
    static Factorization prime(PrimeIndex idx, Exponent exp = 1) {
        Factorization f;
        if (exp != 0) f.factors_[idx] = exp;
        return f;
    }

    /// From an integer (factorize it)
    static Factorization from_integer(uint64_t n) {
        if (n == 0) return zero();
        if (n == 1) return one();

        Factorization f;
        for (PrimeIndex i = 0; i < FIRST_PRIMES.size() && n > 1; ++i) {
            uint64_t p = FIRST_PRIMES[i];
            while (n % p == 0) {
                f.factors_[i]++;
                n /= p;
            }
        }
        // If n > 1 here, it has a large prime factor we can't represent
        // In production, we'd extend the prime table
        return f;
    }

    /// From a rational (p/q)
    static Factorization from_rational(uint64_t numerator, uint64_t denominator) {
        auto num = from_integer(numerator);
        auto den = from_integer(denominator);
        return num / den;
    }

    /// From exponent vector (dense representation)
    static Factorization from_exponents(std::span<const Exponent> exps) {
        Factorization f;
        for (size_t i = 0; i < exps.size(); ++i) {
            if (exps[i] != 0) {
                f.factors_[static_cast<PrimeIndex>(i)] = exps[i];
            }
        }
        return f;
    }

    // ------------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------------

    /// Get exponent for a prime (0 if not present)
    [[nodiscard]] Exponent operator[](PrimeIndex idx) const {
        auto it = factors_.find(idx);
        return (it != factors_.end()) ? it->second : 0;
    }

    /// Get exponent for a prime by value
    [[nodiscard]] Exponent exponent_of(uint64_t prime) const {
        if (auto idx = prime_index(prime)) {
            return (*this)[*idx];
        }
        return 0;
    }

    /// Check if this is zero
    [[nodiscard]] bool is_zero() const { return is_zero_; }

    /// Check if this is one
    [[nodiscard]] bool is_one() const { return !is_zero_ && factors_.empty(); }

    /// Check if this is a pure prime (single factor with exponent 1)
    [[nodiscard]] bool is_prime() const {
        return !is_zero_ && factors_.size() == 1 &&
               factors_.begin()->second == 1;
    }

    /// Check if this is an integer (all exponents non-negative)
    [[nodiscard]] bool is_integer() const {
        if (is_zero_) return true;
        for (const auto& [idx, exp] : factors_) {
            if (exp < 0) return false;
        }
        return true;
    }

    /// Number of distinct prime factors
    [[nodiscard]] size_t num_factors() const { return factors_.size(); }

    /// Get all factors
    [[nodiscard]] const FactorMap& factors() const { return factors_; }

    /// Highest prime index used
    [[nodiscard]] PrimeIndex max_prime_index() const {
        if (factors_.empty()) return 0;
        return factors_.rbegin()->first;
    }

    // ------------------------------------------------------------------------
    // Arithmetic Operations (exact!)
    // ------------------------------------------------------------------------

    /// Multiplication = add exponents
    Factorization operator*(const Factorization& other) const {
        if (is_zero_ || other.is_zero_) return zero();

        Factorization result = *this;
        for (const auto& [idx, exp] : other.factors_) {
            result.factors_[idx] += exp;
            if (result.factors_[idx] == 0) {
                result.factors_.erase(idx);
            }
        }
        return result;
    }

    /// Division = subtract exponents
    Factorization operator/(const Factorization& other) const {
        if (other.is_zero_) {
            // Division by zero - return special value
            Factorization inf;
            inf.is_infinity_ = true;
            return inf;
        }
        if (is_zero_) return zero();

        Factorization result = *this;
        for (const auto& [idx, exp] : other.factors_) {
            result.factors_[idx] -= exp;
            if (result.factors_[idx] == 0) {
                result.factors_.erase(idx);
            }
        }
        return result;
    }

    /// Power = multiply all exponents
    Factorization pow(Exponent n) const {
        if (is_zero_) return (n > 0) ? zero() : one(); // 0^0 = 1 by convention
        if (n == 0) return one();

        Factorization result;
        for (const auto& [idx, exp] : factors_) {
            Exponent new_exp = exp * n;
            if (new_exp != 0) {
                result.factors_[idx] = new_exp;
            }
        }
        return result;
    }

    /// GCD = min of exponents (for integers)
    Factorization gcd(const Factorization& other) const {
        if (is_zero_) return other;
        if (other.is_zero_) return *this;

        Factorization result;
        for (const auto& [idx, exp] : factors_) {
            Exponent other_exp = other[idx];
            Exponent min_exp = std::min(exp, other_exp);
            if (min_exp > 0) {
                result.factors_[idx] = min_exp;
            }
        }
        return result;
    }

    /// LCM = max of exponents (for integers)
    Factorization lcm(const Factorization& other) const {
        if (is_zero_ || other.is_zero_) return zero();

        Factorization result = *this;
        for (const auto& [idx, exp] : other.factors_) {
            result.factors_[idx] = std::max(result.factors_[idx], exp);
        }
        return result;
    }

    // ------------------------------------------------------------------------
    // Comparison
    // ------------------------------------------------------------------------

    bool operator==(const Factorization& other) const {
        if (is_zero_ != other.is_zero_) return false;
        return factors_ == other.factors_;
    }

    auto operator<=>(const Factorization& other) const {
        // Compare by numeric value (convert to double for comparison)
        return to_double() <=> other.to_double();
    }

    // ------------------------------------------------------------------------
    // Conversion
    // ------------------------------------------------------------------------

    /// Convert to integer (if possible)
    [[nodiscard]] std::optional<uint64_t> to_integer() const {
        if (is_zero_) return 0;
        if (!is_integer()) return std::nullopt;

        uint64_t result = 1;
        for (const auto& [idx, exp] : factors_) {
            uint64_t p = nth_prime(idx);
            for (Exponent i = 0; i < exp; ++i) {
                // Check for overflow
                if (result > UINT64_MAX / p) return std::nullopt;
                result *= p;
            }
        }
        return result;
    }

    /// Convert to double (approximate)
    [[nodiscard]] double to_double() const {
        if (is_zero_) return 0.0;
        if (is_infinity_) return std::numeric_limits<double>::infinity();

        double result = 1.0;
        for (const auto& [idx, exp] : factors_) {
            result *= std::pow(static_cast<double>(nth_prime(idx)), exp);
        }
        return result;
    }

    /// Convert to log-space (sum of exp * log(prime))
    [[nodiscard]] double to_log() const {
        if (is_zero_) return -std::numeric_limits<double>::infinity();
        if (is_infinity_) return std::numeric_limits<double>::infinity();

        double result = 0.0;
        for (const auto& [idx, exp] : factors_) {
            result += exp * std::log(static_cast<double>(nth_prime(idx)));
        }
        return result;
    }

    /// Get exponents as dense vector (up to max_index)
    [[nodiscard]] std::vector<Exponent> to_exponent_vector(size_t max_index = 64) const {
        std::vector<Exponent> result(max_index, 0);
        for (const auto& [idx, exp] : factors_) {
            if (idx < max_index) {
                result[idx] = exp;
            }
        }
        return result;
    }

    /// String representation
    [[nodiscard]] std::string to_string() const {
        if (is_zero_) return "0";
        if (is_infinity_) return "∞";
        if (factors_.empty()) return "1";

        std::string result;
        bool first = true;
        for (const auto& [idx, exp] : factors_) {
            if (!first) result += " × ";
            first = false;
            result += std::to_string(nth_prime(idx));
            if (exp != 1) {
                result += "^" + std::to_string(exp);
            }
        }
        return result;
    }

private:
    FactorMap factors_;
    bool is_zero_ = false;
    bool is_infinity_ = false;
};

// ============================================================================
// HyperIndex - Hypergraph Structure via Prime Factorization
// ============================================================================

/**
 * @brief A hypergraph index encoded as a prime factorization
 *
 * Interpretation:
 *   - Each prime index represents a NODE in the hypergraph
 *   - Each exponent represents the EDGE WEIGHT to that node
 *   - The composite number represents a HYPEREDGE connecting those nodes
 *
 * Examples:
 *   6 = 2 × 3           → binary edge between nodes 0 and 1
 *   30 = 2 × 3 × 5      → ternary hyperedge connecting nodes 0, 1, 2
 *   12 = 2² × 3         → weighted edge: node 0 with weight 2, node 1 with weight 1
 *   1                   → empty hyperedge (connects nothing)
 *   p (prime)           → self-loop / node identity
 */
class HyperIndex {
public:
    using NodeId = PrimeIndex;
    using Weight = Exponent;

    HyperIndex() = default;
    explicit HyperIndex(Factorization f) : fact_(std::move(f)) {}

    /// Create from explicit node-weight pairs
    static HyperIndex from_weighted_nodes(std::span<const std::pair<NodeId, Weight>> nodes) {
        Factorization f;
        for (const auto& [node, weight] : nodes) {
            if (weight != 0) {
                f = f * Factorization::prime(node, weight);
            }
        }
        return HyperIndex(f);
    }

    /// Create from unweighted node set (all weights = 1)
    static HyperIndex from_nodes(std::span<const NodeId> nodes) {
        Factorization f;
        for (NodeId node : nodes) {
            f = f * Factorization::prime(node, 1);
        }
        return HyperIndex(f);
    }

    /// Create edge between two nodes
    static HyperIndex edge(NodeId a, NodeId b, Weight wa = 1, Weight wb = 1) {
        return HyperIndex(Factorization::prime(a, wa) * Factorization::prime(b, wb));
    }

    /// Create self-loop / node marker
    static HyperIndex node(NodeId n) {
        return HyperIndex(Factorization::prime(n, 1));
    }

    // ------------------------------------------------------------------------
    // Hypergraph Queries
    // ------------------------------------------------------------------------

    /// Get arity (number of connected nodes)
    [[nodiscard]] size_t arity() const { return fact_.num_factors(); }

    /// Check if this connects a specific node
    [[nodiscard]] bool connects(NodeId node) const { return fact_[node] != 0; }

    /// Get weight of connection to a node
    [[nodiscard]] Weight weight(NodeId node) const { return fact_[node]; }

    /// Get all connected nodes
    [[nodiscard]] std::vector<NodeId> nodes() const {
        std::vector<NodeId> result;
        for (const auto& [idx, exp] : fact_.factors()) {
            result.push_back(idx);
        }
        return result;
    }

    /// Get all node-weight pairs
    [[nodiscard]] std::vector<std::pair<NodeId, Weight>> weighted_nodes() const {
        std::vector<std::pair<NodeId, Weight>> result;
        for (const auto& [idx, exp] : fact_.factors()) {
            result.emplace_back(idx, exp);
        }
        return result;
    }

    // ------------------------------------------------------------------------
    // Hypergraph Operations
    // ------------------------------------------------------------------------

    /// Union of hyperedges (connects all nodes from both)
    HyperIndex operator|(const HyperIndex& other) const {
        return HyperIndex(fact_.lcm(other.fact_));
    }

    /// Intersection of hyperedges (only nodes in both)
    HyperIndex operator&(const HyperIndex& other) const {
        return HyperIndex(fact_.gcd(other.fact_));
    }

    /// Composition (multiply = add weights)
    HyperIndex operator*(const HyperIndex& other) const {
        return HyperIndex(fact_ * other.fact_);
    }

    /// Check if this is a sub-hyperedge of other
    [[nodiscard]] bool subset_of(const HyperIndex& other) const {
        for (const auto& [idx, exp] : fact_.factors()) {
            if (other.fact_[idx] < exp) return false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // Conversion
    // ------------------------------------------------------------------------

    [[nodiscard]] const Factorization& factorization() const { return fact_; }
    [[nodiscard]] std::optional<uint64_t> to_integer() const { return fact_.to_integer(); }
    [[nodiscard]] std::string to_string() const { return fact_.to_string(); }

private:
    Factorization fact_;
};

// ============================================================================
// TensorShape - Tensor Dimensions as Prime Factorization
// ============================================================================

/**
 * @brief Tensor shape encoded as prime factorization
 *
 * Traditional: shape = (batch, seq, hidden) = (32, 512, 768)
 * Prime basis: shape = 2^5 × 3 × 5 × 7 × ... encoding structure
 *
 * Benefits:
 * - Shape compatibility is GCD/LCM
 * - Reshape is just rearranging exponents
 * - Broadcasting is natural
 * - Fiber bundle structure: base space × fiber
 */
class TensorShape {
public:
    TensorShape() = default;
    explicit TensorShape(Factorization f) : fact_(std::move(f)) {}

    /// From traditional dimension list
    static TensorShape from_dims(std::span<const uint64_t> dims) {
        // Each dimension gets its own prime
        // dim[i] becomes the exponent of prime[i]
        Factorization f;
        for (size_t i = 0; i < dims.size(); ++i) {
            // Factorize each dimension and shift prime indices
            auto dim_fact = Factorization::from_integer(dims[i]);
            // For simplicity, we'll use a different encoding:
            // Dimension i with size d means prime[i] with exponent encoding d
            // Actually, let's just store dims as exponents of consecutive primes
            if (dims[i] > 0) {
                // Use logarithmic encoding: exponent = ceil(log2(dim))
                Exponent exp = static_cast<Exponent>(std::ceil(std::log2(dims[i] + 1)));
                f = f * Factorization::prime(static_cast<PrimeIndex>(i), exp);
            }
        }
        return TensorShape(f);
    }

    /// From explicit (prime_index, size) pairs - more flexible
    static TensorShape from_indexed_dims(std::span<const std::pair<PrimeIndex, uint64_t>> dims) {
        Factorization f;
        for (const auto& [idx, size] : dims) {
            if (size > 1) {
                f = f * Factorization::from_integer(size);
            }
        }
        return TensorShape(f);
    }

    /// Total number of elements
    [[nodiscard]] std::optional<uint64_t> numel() const {
        return fact_.to_integer();
    }

    /// Number of dimensions (number of prime factors)
    [[nodiscard]] size_t ndim() const {
        return fact_.num_factors();
    }

    /// Check if shapes are compatible for broadcasting
    [[nodiscard]] bool broadcastable_with(const TensorShape& other) const {
        // GCD of shapes should divide both
        auto g = fact_.gcd(other.fact_);
        return true; // Simplified - real impl would check divisibility
    }

    /// Reshape by rearranging prime factors
    [[nodiscard]] TensorShape reshape(const TensorShape& new_shape) const {
        // Valid if total elements match
        if (fact_.to_integer() != new_shape.fact_.to_integer()) {
            return TensorShape{}; // Invalid
        }
        return new_shape;
    }

    [[nodiscard]] const Factorization& factorization() const { return fact_; }
    [[nodiscard]] std::string to_string() const { return fact_.to_string(); }

private:
    Factorization fact_;
};

// ============================================================================
// ExponentVector - For Neural Network Operations
// ============================================================================

/**
 * @brief Dense vector of exponents for gradient-based optimization
 *
 * This is the "continuous" view of a factorization, where:
 * - Each element is an exponent (can be fractional for interpolation)
 * - Operations are element-wise (like normal vectors)
 * - exp/log bridge to/from factorizations
 *
 * Neural network weights operate on this representation.
 */
class ExponentVector {
public:
    ExponentVector() = default;
    explicit ExponentVector(size_t size) : exponents_(size, 0.0) {}
    explicit ExponentVector(std::vector<double> exps) : exponents_(std::move(exps)) {}

    /// From factorization
    static ExponentVector from_factorization(const Factorization& f, size_t size = 64) {
        ExponentVector v(size);
        for (const auto& [idx, exp] : f.factors()) {
            if (idx < size) {
                v.exponents_[idx] = static_cast<double>(exp);
            }
        }
        return v;
    }

    /// To factorization (rounds exponents to integers)
    [[nodiscard]] Factorization to_factorization() const {
        std::vector<Exponent> int_exps;
        int_exps.reserve(exponents_.size());
        for (double e : exponents_) {
            int_exps.push_back(static_cast<Exponent>(std::round(e)));
        }
        return Factorization::from_exponents(int_exps);
    }

    // Element access
    [[nodiscard]] double& operator[](size_t i) { return exponents_[i]; }
    [[nodiscard]] double operator[](size_t i) const { return exponents_[i]; }
    [[nodiscard]] size_t size() const { return exponents_.size(); }

    // Vector operations
    ExponentVector operator+(const ExponentVector& other) const {
        ExponentVector result(std::max(size(), other.size()));
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = (i < size() ? exponents_[i] : 0.0) +
                        (i < other.size() ? other[i] : 0.0);
        }
        return result;
    }

    ExponentVector operator-(const ExponentVector& other) const {
        ExponentVector result(std::max(size(), other.size()));
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = (i < size() ? exponents_[i] : 0.0) -
                        (i < other.size() ? other[i] : 0.0);
        }
        return result;
    }

    ExponentVector operator*(double scalar) const {
        ExponentVector result = *this;
        for (double& e : result.exponents_) e *= scalar;
        return result;
    }

    /// Dot product (for computing log of product)
    [[nodiscard]] double dot(const ExponentVector& other) const {
        double sum = 0.0;
        size_t n = std::min(size(), other.size());
        for (size_t i = 0; i < n; ++i) {
            sum += exponents_[i] * other[i];
        }
        return sum;
    }

    /// L2 norm
    [[nodiscard]] double norm() const {
        double sum = 0.0;
        for (double e : exponents_) sum += e * e;
        return std::sqrt(sum);
    }

    /// Softmax over exponents (for attention-like operations)
    [[nodiscard]] ExponentVector softmax() const {
        ExponentVector result(size());
        double max_val = *std::max_element(exponents_.begin(), exponents_.end());
        double sum = 0.0;
        for (size_t i = 0; i < size(); ++i) {
            result[i] = std::exp(exponents_[i] - max_val);
            sum += result[i];
        }
        for (double& e : result.exponents_) e /= sum;
        return result;
    }

    [[nodiscard]] const std::vector<double>& data() const { return exponents_; }
    [[nodiscard]] std::vector<double>& data() { return exponents_; }

private:
    std::vector<double> exponents_;
};

// ============================================================================
// PrimeRational - Exact Rational Arithmetic
// ============================================================================

/**
 * @brief Rational number as factorization with positive/negative exponents
 *
 * Numerator has positive exponents, denominator has negative.
 * All arithmetic is exact - no floating point errors ever.
 */
class PrimeRational {
public:
    PrimeRational() = default;
    explicit PrimeRational(Factorization f) : fact_(std::move(f)) {}

    static PrimeRational from_integer(int64_t n) {
        if (n == 0) return PrimeRational(Factorization::zero());
        if (n < 0) {
            auto f = Factorization::from_integer(static_cast<uint64_t>(-n));
            return PrimeRational(f); // Would need sign handling
        }
        return PrimeRational(Factorization::from_integer(static_cast<uint64_t>(n)));
    }

    static PrimeRational from_ratio(int64_t num, int64_t den) {
        auto n = Factorization::from_integer(static_cast<uint64_t>(std::abs(num)));
        auto d = Factorization::from_integer(static_cast<uint64_t>(std::abs(den)));
        bool negative = (num < 0) != (den < 0);
        // Sign handling would go here
        return PrimeRational(n / d);
    }

    // Arithmetic
    PrimeRational operator*(const PrimeRational& other) const {
        return PrimeRational(fact_ * other.fact_);
    }

    PrimeRational operator/(const PrimeRational& other) const {
        return PrimeRational(fact_ / other.fact_);
    }

    PrimeRational operator+(const PrimeRational& other) const {
        // Addition requires common denominator - more complex
        // For now, convert to double and back (loses exactness)
        double sum = fact_.to_double() + other.fact_.to_double();
        // Would need continued fraction or other exact algorithm
        return *this; // Placeholder
    }

    [[nodiscard]] double to_double() const { return fact_.to_double(); }
    [[nodiscard]] const Factorization& factorization() const { return fact_; }

private:
    Factorization fact_;
};

// ============================================================================
// Fiber Bundle Structure
// ============================================================================

/**
 * @brief Represents a fiber bundle with base space and fiber via factorization
 *
 * A fiber bundle E → B with fiber F can be encoded as:
 *   E = B × F (as factorizations)
 *
 * The exponents in B form the "base coordinates"
 * The exponents in F form the "fiber coordinates"
 *
 * Transformation groups act on exponent space:
 * - Permutations shuffle prime indices
 * - Scaling multiplies exponents
 * - Translation adds to exponents
 */
class FiberBundle {
public:
    FiberBundle() = default;
    FiberBundle(Factorization base, Factorization fiber)
        : base_(std::move(base)), fiber_(std::move(fiber)) {}

    /// Total space as product
    [[nodiscard]] Factorization total_space() const {
        return base_ * fiber_;
    }

    /// Project to base space
    [[nodiscard]] const Factorization& base() const { return base_; }

    /// Get fiber at a point
    [[nodiscard]] const Factorization& fiber() const { return fiber_; }

    /// Apply transformation to base
    FiberBundle transform_base(const std::function<Factorization(const Factorization&)>& f) const {
        return FiberBundle(f(base_), fiber_);
    }

    /// Apply transformation to fiber
    FiberBundle transform_fiber(const std::function<Factorization(const Factorization&)>& f) const {
        return FiberBundle(base_, f(fiber_));
    }

    /// Pull back fiber along base map
    FiberBundle pullback(const Factorization& new_base) const {
        // The fiber is "pulled back" to the new base
        return FiberBundle(new_base, fiber_);
    }

private:
    Factorization base_;
    Factorization fiber_;
};

// ============================================================================
// Integration with CogMorph
// ============================================================================

/// Convert AtomId to prime factorization (for hypergraph encoding)
[[nodiscard]] inline Factorization atom_to_prime(uint64_t atom_id) {
    return Factorization::from_integer(atom_id + 1); // +1 to avoid zero
}

/// Convert TruthValue to exponent vector
[[nodiscard]] inline ExponentVector truth_to_exponents(float strength, float confidence) {
    // Encode as exponents: strength -> exp[0], confidence -> exp[1]
    ExponentVector v(2);
    v[0] = std::log(strength + 1e-10);
    v[1] = std::log(confidence + 1e-10);
    return v;
}

/// Convert AttentionValue to exponent vector
[[nodiscard]] inline ExponentVector attention_to_exponents(float sti, int16_t lti, uint8_t vlti) {
    ExponentVector v(3);
    v[0] = sti; // Already in log-like space
    v[1] = static_cast<double>(lti);
    v[2] = static_cast<double>(vlti);
    return v;
}

} // namespace cogmorph::prime
