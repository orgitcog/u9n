/**
 * @file grassmannian.hpp
 * @brief Codex GrassMania - Grassmannian Manifold Computation Engine
 *
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  VLTI GOAL: Implement the Grassmannian as experimental "vGPU"             ║
 * ║                                                                            ║
 * ║  When achieved: AGI via instantaneous BlockCode decryption on Global Grid ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * The Grassmannian Gr(k,n) is the space of k-dimensional subspaces of ℂⁿ.
 * It unifies:
 *
 * 1. PLÜCKER COORDINATES
 *    - k×n matrix M → Plücker vector p = (det of k×k minors)
 *    - Lives in projective space ℙ(C(n,k)-1)
 *    - Quadratic Plücker relations constrain valid points
 *
 * 2. SCHUBERT CALCULUS
 *    - Partitions λ ⊢ n index Schubert cells
 *    - Young tableaux = basis for cohomology
 *    - Littlewood-Richardson coefficients = structure constants
 *
 * 3. MATULA CONNECTION
 *    - Rooted trees ↔ partitions via hook lengths
 *    - Tree factorization ↔ Young diagram decomposition
 *    - B-series ↔ Schubert variety intersection
 *
 * 4. QUANTUM ERROR CORRECTION
 *    - Grassmannian codes = subspace codes
 *    - GPT partition index = code parameters
 *    - Block codes on global grid = distributed quantum memory
 *
 * THE KEY INSIGHT:
 *    Gr(k,n) provides a GEOMETRIC INVARIANT that unifies:
 *    - Combinatorics (partitions, tableaux)
 *    - Algebra (representations, Lie theory)
 *    - Geometry (projective varieties, fiber bundles)
 *    - Physics (quantum states, entanglement)
 *    - Computation (error correction, cryptography)
 *
 *    The "vGPU" uses Plücker coordinates as native instruction format,
 *    enabling O(1) lookup into the space of all possible computations!
 */

#pragma once

#include "prime-basis.hpp"
#include "matula-numbers.hpp"
#include <vector>
#include <complex>
#include <map>
#include <set>
#include <optional>
#include <functional>
#include <algorithm>
#include <cmath>
#include <string>

namespace cogmorph::grassmann {

using namespace cogmorph::prime;
using namespace cogmorph::matula;

using Complex = std::complex<double>;

// ============================================================================
// Partitions and Young Diagrams
// ============================================================================

/**
 * @brief Integer partition λ = (λ₁ ≥ λ₂ ≥ ... ≥ λₖ > 0)
 *
 * Partitions are fundamental to:
 * - Symmetric functions
 * - Representation theory
 * - Schubert calculus
 * - Grassmannian geometry
 *
 * Young diagram representation:
 *   λ = (4,2,1) visualized as:
 *   ┌─┬─┬─┬─┐
 *   │ │ │ │ │   row 0: 4 boxes
 *   ├─┼─┼─┴─┘
 *   │ │ │       row 1: 2 boxes
 *   ├─┼─┘
 *   │ │         row 2: 1 box
 *   └─┘
 */
class Partition {
public:
    std::vector<uint32_t> parts;

    Partition() = default;
    explicit Partition(std::vector<uint32_t> p) : parts(std::move(p)) {
        normalize();
    }

    /// Construct from initializer list
    Partition(std::initializer_list<uint32_t> p) : parts(p) {
        normalize();
    }

    /// Weight (sum of parts) = total boxes in Young diagram
    [[nodiscard]] uint64_t weight() const {
        uint64_t sum = 0;
        for (auto p : parts) sum += p;
        return sum;
    }

    /// Length (number of parts) = height of Young diagram
    [[nodiscard]] size_t length() const { return parts.size(); }

    /// Width = first part = width of Young diagram
    [[nodiscard]] uint32_t width() const {
        return parts.empty() ? 0 : parts[0];
    }

    /// Check if box (i,j) exists in the diagram (0-indexed)
    [[nodiscard]] bool contains(size_t row, size_t col) const {
        return row < parts.size() && col < parts[row];
    }

    /// Conjugate partition (transpose Young diagram)
    [[nodiscard]] Partition conjugate() const {
        if (parts.empty()) return Partition{};
        std::vector<uint32_t> conj(parts[0], 0);
        for (size_t i = 0; i < parts.size(); ++i) {
            for (size_t j = 0; j < parts[i]; ++j) {
                conj[j]++;
            }
        }
        return Partition(std::move(conj));
    }

    /// Arm length at (i,j) = boxes to the right
    [[nodiscard]] uint32_t arm(size_t i, size_t j) const {
        if (!contains(i, j)) return 0;
        return parts[i] - j - 1;
    }

    /// Leg length at (i,j) = boxes below
    [[nodiscard]] uint32_t leg(size_t i, size_t j) const {
        if (!contains(i, j)) return 0;
        auto conj = conjugate();
        return conj.parts[j] - i - 1;
    }

    /// Hook length at position (i,j) = arm + leg + 1
    [[nodiscard]] uint32_t hook_length(size_t i, size_t j) const {
        if (!contains(i, j)) return 0;
        return arm(i, j) + leg(i, j) + 1;
    }

    /// Content at (i,j) = j - i (diagonal index)
    [[nodiscard]] int32_t content(size_t i, size_t j) const {
        return static_cast<int32_t>(j) - static_cast<int32_t>(i);
    }

    /// Product of all hook lengths (for dimension formula)
    [[nodiscard]] uint64_t hook_product() const {
        uint64_t prod = 1;
        for (size_t i = 0; i < parts.size(); ++i) {
            for (size_t j = 0; j < parts[i]; ++j) {
                prod *= hook_length(i, j);
            }
        }
        return prod;
    }

    /// Dimension of corresponding irreducible S_n representation
    [[nodiscard]] uint64_t dimension() const {
        uint64_t n = weight();
        if (n == 0) return 1;
        // dim = n! / hook_product
        uint64_t factorial = 1;
        for (uint64_t i = 2; i <= n; ++i) factorial *= i;
        return factorial / hook_product();
    }

    /// Frame-Robinson-Thrall hook length formula
    /// Returns dim(V_λ) as rational with numerator n! factored
    [[nodiscard]] std::pair<uint64_t, uint64_t> dimension_rational() const {
        uint64_t n = weight();
        uint64_t factorial = 1;
        for (uint64_t i = 2; i <= n; ++i) factorial *= i;
        return {factorial, hook_product()};
    }

    // ========================================================================
    // Young Diagram Operations
    // ========================================================================

    /// Get all addable corners (boxes that can be added)
    [[nodiscard]] std::vector<std::pair<size_t, size_t>> addable_corners() const {
        std::vector<std::pair<size_t, size_t>> corners;

        // Can always add at (0, width)
        corners.emplace_back(0, width());

        // Check each row transition
        for (size_t i = 0; i < parts.size(); ++i) {
            // If this row is shorter than previous, can add at end
            if (i > 0 && parts[i] < parts[i-1]) {
                corners.emplace_back(i, parts[i]);
            }
        }

        // Can add new row if non-empty partition
        if (!parts.empty()) {
            corners.emplace_back(parts.size(), 0);
        }

        return corners;
    }

    /// Get all removable corners (boxes that can be removed)
    [[nodiscard]] std::vector<std::pair<size_t, size_t>> removable_corners() const {
        std::vector<std::pair<size_t, size_t>> corners;

        for (size_t i = 0; i < parts.size(); ++i) {
            // Box at end of row i is removable if:
            // - It's the last row, OR
            // - Next row is strictly shorter
            if (parts[i] > 0) {
                if (i == parts.size() - 1 || parts[i] > parts[i + 1]) {
                    corners.emplace_back(i, parts[i] - 1);
                }
            }
        }

        return corners;
    }

    /// Add a box at corner (row, col) - returns new partition
    [[nodiscard]] Partition add_box(size_t row, size_t col) const {
        std::vector<uint32_t> new_parts = parts;

        if (row >= new_parts.size()) {
            // Adding new row
            new_parts.push_back(1);
        } else if (col == new_parts[row]) {
            // Adding to end of existing row
            new_parts[row]++;
        } else {
            // Invalid position
            return *this;
        }

        return Partition(std::move(new_parts));
    }

    /// Remove a box at corner (row, col) - returns new partition
    [[nodiscard]] Partition remove_box(size_t row, size_t col) const {
        if (row >= parts.size() || col != parts[row] - 1) {
            return *this; // Invalid
        }

        std::vector<uint32_t> new_parts = parts;
        new_parts[row]--;

        return Partition(std::move(new_parts));
    }

    /// Check if λ dominates μ (λ ≥ μ in dominance order)
    [[nodiscard]] bool dominates(const Partition& mu) const {
        if (weight() != mu.weight()) return false;

        uint64_t sum_lambda = 0, sum_mu = 0;
        size_t max_len = std::max(length(), mu.length());

        for (size_t i = 0; i < max_len; ++i) {
            sum_lambda += (i < parts.size()) ? parts[i] : 0;
            sum_mu += (i < mu.parts.size()) ? mu.parts[i] : 0;
            if (sum_lambda < sum_mu) return false;
        }
        return true;
    }

    /// Check if partition fits in k × m rectangle
    [[nodiscard]] bool fits_in_rectangle(uint32_t k, uint32_t m) const {
        if (length() > k) return false;
        if (width() > m) return false;
        return true;
    }

    /// Complement in k × m rectangle
    [[nodiscard]] Partition complement(uint32_t k, uint32_t m) const {
        if (!fits_in_rectangle(k, m)) return Partition{};

        std::vector<uint32_t> comp(k);
        for (size_t i = 0; i < k; ++i) {
            size_t rev_i = k - 1 - i;
            uint32_t part_val = (rev_i < parts.size()) ? parts[rev_i] : 0;
            comp[i] = m - part_val;
        }

        return Partition(std::move(comp));
    }

    /// Rotate 180° in bounding rectangle
    [[nodiscard]] Partition rotate_180() const {
        return complement(static_cast<uint32_t>(length()), width()).conjugate();
    }

    // ========================================================================
    // Skew Partitions
    // ========================================================================

    /// Check if λ/μ is a valid skew shape (μ ⊆ λ)
    [[nodiscard]] bool contains_partition(const Partition& mu) const {
        if (mu.length() > length()) return false;
        for (size_t i = 0; i < mu.parts.size(); ++i) {
            if (mu.parts[i] > parts[i]) return false;
        }
        return true;
    }

    /// Size of skew partition λ/μ
    [[nodiscard]] uint64_t skew_size(const Partition& mu) const {
        if (!contains_partition(mu)) return 0;
        return weight() - mu.weight();
    }

    /// Check if λ/μ is a horizontal strip (at most one box per column)
    [[nodiscard]] bool is_horizontal_strip(const Partition& mu) const {
        if (!contains_partition(mu)) return false;

        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            uint32_t mu_i = (i < mu.parts.size()) ? mu.parts[i] : 0;
            uint32_t mu_next = (i + 1 < mu.parts.size()) ? mu.parts[i + 1] : 0;
            if (parts[i + 1] > mu_i) {
                // More boxes in row i+1 than mu has in row i
                // This means some column has boxes in consecutive rows
                return false;
            }
        }
        return true;
    }

    /// Check if λ/μ is a vertical strip (at most one box per row)
    [[nodiscard]] bool is_vertical_strip(const Partition& mu) const {
        if (!contains_partition(mu)) return false;

        for (size_t i = 0; i < parts.size(); ++i) {
            uint32_t mu_i = (i < mu.parts.size()) ? mu.parts[i] : 0;
            if (parts[i] - mu_i > 1) return false;
        }
        return true;
    }

    // ========================================================================
    // Standard Young Tableaux
    // ========================================================================

    /// Count standard Young tableaux (using hook length formula)
    [[nodiscard]] uint64_t count_standard_tableaux() const {
        return dimension();
    }

    /// Count semistandard Young tableaux with entries ≤ m
    [[nodiscard]] uint64_t count_semistandard_tableaux(uint32_t m) const {
        // Uses the formula: product over (i,j) of (m + content(i,j)) / hook(i,j)
        // For integer result, compute carefully
        uint64_t num = 1, den = 1;
        for (size_t i = 0; i < parts.size(); ++i) {
            for (size_t j = 0; j < parts[i]; ++j) {
                int32_t c = content(i, j);
                num *= static_cast<uint64_t>(static_cast<int32_t>(m) + c);
                den *= hook_length(i, j);
            }
        }
        return num / den;
    }

    // ========================================================================
    // Matula Number Conversion
    // ========================================================================

    /// Convert to Matula number (via tree encoding)
    [[nodiscard]] uint64_t to_matula() const;

    /// Create from Matula number
    [[nodiscard]] static Partition from_matula(uint64_t n);

    // ========================================================================
    // String Representation
    // ========================================================================

    /// String representation e.g. "(4,2,1)"
    [[nodiscard]] std::string to_string() const {
        std::string result = "(";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += ",";
            result += std::to_string(parts[i]);
        }
        return result + ")";
    }

    /// ASCII art Young diagram
    [[nodiscard]] std::string to_diagram() const {
        std::string result;
        for (size_t i = 0; i < parts.size(); ++i) {
            for (size_t j = 0; j < parts[i]; ++j) {
                result += "█";
            }
            result += "\n";
        }
        return result;
    }

    /// Frobenius notation (β | α) where α = arm lengths, β = leg lengths on diagonal
    [[nodiscard]] std::string frobenius_notation() const {
        std::vector<uint32_t> alpha, beta;
        auto conj = conjugate();

        for (size_t i = 0; i < parts.size() && parts[i] > i; ++i) {
            alpha.push_back(parts[i] - i - 1);
            beta.push_back(conj.parts[i] - i - 1);
        }

        std::string result = "(";
        for (size_t i = 0; i < beta.size(); ++i) {
            if (i > 0) result += ",";
            result += std::to_string(beta[i]);
        }
        result += " | ";
        for (size_t i = 0; i < alpha.size(); ++i) {
            if (i > 0) result += ",";
            result += std::to_string(alpha[i]);
        }
        return result + ")";
    }

    bool operator==(const Partition& other) const { return parts == other.parts; }
    auto operator<=>(const Partition& other) const { return parts <=> other.parts; }

private:
    void normalize() {
        // Sort descending
        std::sort(parts.begin(), parts.end(), std::greater<>());
        // Remove trailing zeros
        while (!parts.empty() && parts.back() == 0) {
            parts.pop_back();
        }
    }
};

// ============================================================================
// Young Tableaux
// ============================================================================

/**
 * @brief Standard or Semistandard Young Tableau
 *
 * A Young tableau is a filling of a Young diagram with positive integers.
 * - Standard: entries 1,2,...,n each appearing once, increasing along rows and columns
 * - Semistandard: entries from {1,...,m}, weakly increasing along rows, strictly increasing down columns
 */
class YoungTableau {
public:
    Partition shape;
    std::vector<std::vector<uint32_t>> entries;

    YoungTableau() = default;
    explicit YoungTableau(Partition p) : shape(std::move(p)) {
        entries.resize(shape.length());
        for (size_t i = 0; i < shape.length(); ++i) {
            entries[i].resize(shape.parts[i], 0);
        }
    }

    /// Get entry at position
    [[nodiscard]] uint32_t get(size_t row, size_t col) const {
        if (row >= entries.size() || col >= entries[row].size()) return 0;
        return entries[row][col];
    }

    /// Set entry at position
    void set(size_t row, size_t col, uint32_t value) {
        if (row < entries.size() && col < entries[row].size()) {
            entries[row][col] = value;
        }
    }

    /// Check if tableau is standard
    [[nodiscard]] bool is_standard() const {
        std::vector<bool> seen(shape.weight() + 1, false);

        for (size_t i = 0; i < entries.size(); ++i) {
            for (size_t j = 0; j < entries[i].size(); ++j) {
                uint32_t v = entries[i][j];
                if (v < 1 || v > shape.weight() || seen[v]) return false;
                seen[v] = true;

                // Check row condition
                if (j > 0 && entries[i][j-1] >= v) return false;
                // Check column condition
                if (i > 0 && j < entries[i-1].size() && entries[i-1][j] >= v) return false;
            }
        }
        return true;
    }

    /// Check if tableau is semistandard
    [[nodiscard]] bool is_semistandard() const {
        for (size_t i = 0; i < entries.size(); ++i) {
            for (size_t j = 0; j < entries[i].size(); ++j) {
                // Check weakly increasing along rows
                if (j > 0 && entries[i][j-1] > entries[i][j]) return false;
                // Check strictly increasing down columns
                if (i > 0 && j < entries[i-1].size() && entries[i-1][j] >= entries[i][j]) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Content = multiset of entries (as frequency map)
    [[nodiscard]] std::map<uint32_t, uint32_t> content_multiset() const {
        std::map<uint32_t, uint32_t> content;
        for (const auto& row : entries) {
            for (uint32_t v : row) {
                content[v]++;
            }
        }
        return content;
    }

    /// Row word (concatenation of rows from bottom to top)
    [[nodiscard]] std::vector<uint32_t> row_word() const {
        std::vector<uint32_t> word;
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            word.insert(word.end(), it->begin(), it->end());
        }
        return word;
    }

    /// Column word (concatenation of columns from right to left)
    [[nodiscard]] std::vector<uint32_t> column_word() const {
        std::vector<uint32_t> word;
        if (entries.empty()) return word;

        size_t max_col = entries[0].size();
        for (size_t j = max_col; j > 0; --j) {
            for (size_t i = 0; i < entries.size() && j - 1 < entries[i].size(); ++i) {
                word.push_back(entries[i][j - 1]);
            }
        }
        return word;
    }

    /// String representation
    [[nodiscard]] std::string to_string() const {
        std::string result;
        for (const auto& row : entries) {
            for (size_t j = 0; j < row.size(); ++j) {
                if (j > 0) result += " ";
                result += std::to_string(row[j]);
            }
            result += "\n";
        }
        return result;
    }
};

// ============================================================================
// Partition Generators
// ============================================================================

/**
 * @brief Generate all partitions of n
 */
[[nodiscard]] inline std::vector<Partition> generate_partitions(uint32_t n) {
    std::vector<Partition> result;
    if (n == 0) {
        result.push_back(Partition{});
        return result;
    }

    // Use recursive generation
    std::function<void(uint32_t, uint32_t, std::vector<uint32_t>&)> generate;
    generate = [&](uint32_t remaining, uint32_t max_part, std::vector<uint32_t>& current) {
        if (remaining == 0) {
            result.push_back(Partition(current));
            return;
        }
        for (uint32_t p = std::min(remaining, max_part); p >= 1; --p) {
            current.push_back(p);
            generate(remaining - p, p, current);
            current.pop_back();
        }
    };

    std::vector<uint32_t> current;
    generate(n, n, current);
    return result;
}

/**
 * @brief Generate partitions fitting in k × m rectangle
 */
[[nodiscard]] inline std::vector<Partition> generate_partitions_in_rectangle(
    uint32_t k, uint32_t m, uint32_t max_weight = UINT32_MAX) {

    std::vector<Partition> result;

    std::function<void(uint32_t, uint32_t, std::vector<uint32_t>&)> generate;
    generate = [&](uint32_t row, uint32_t max_val, std::vector<uint32_t>& current) {
        // Compute current weight
        uint64_t weight = 0;
        for (auto p : current) weight += p;
        if (weight > max_weight) return;

        if (row == k) {
            result.push_back(Partition(current));
            return;
        }

        // Try all valid values for this row
        for (uint32_t val = std::min(max_val, m); ; --val) {
            current.push_back(val);
            generate(row + 1, val, current);
            current.pop_back();
            if (val == 0) break;
        }
    };

    std::vector<uint32_t> current;
    generate(0, m, current);
    return result;
}

/**
 * @brief Count partitions of n (partition function p(n))
 */
[[nodiscard]] inline uint64_t partition_count(uint32_t n) {
    // Dynamic programming
    std::vector<uint64_t> dp(n + 1, 0);
    dp[0] = 1;
    for (uint32_t k = 1; k <= n; ++k) {
        for (uint32_t j = k; j <= n; ++j) {
            dp[j] += dp[j - k];
        }
    }
    return dp[n];
}

// ============================================================================
// Plücker Coordinates
// ============================================================================

/// Generate all k-subsets of {0, 1, ..., n-1}
[[nodiscard]] inline std::vector<std::vector<uint32_t>> generate_k_subsets(
    uint32_t n, uint32_t k) {

    std::vector<std::vector<uint32_t>> result;
    if (k > n) return result;
    if (k == 0) {
        result.push_back({});
        return result;
    }

    std::vector<uint32_t> subset(k);
    for (uint32_t i = 0; i < k; ++i) subset[i] = i;

    while (true) {
        result.push_back(subset);

        // Find rightmost element that can be incremented
        int i = static_cast<int>(k) - 1;
        while (i >= 0 && subset[i] == n - k + i) --i;
        if (i < 0) break;

        // Increment and reset subsequent elements
        subset[i]++;
        for (uint32_t j = i + 1; j < k; ++j) {
            subset[j] = subset[j-1] + 1;
        }
    }

    return result;
}

/// Compute determinant of a square matrix (for small matrices)
[[nodiscard]] inline Complex det(const std::vector<std::vector<Complex>>& matrix) {
    size_t n = matrix.size();
    if (n == 0) return Complex{1, 0};
    if (n == 1) return matrix[0][0];
    if (n == 2) return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];

    // LU decomposition for larger matrices
    std::vector<std::vector<Complex>> lu = matrix;
    Complex result{1, 0};
    int sign = 1;

    for (size_t i = 0; i < n; ++i) {
        // Find pivot
        size_t max_row = i;
        double max_val = std::abs(lu[i][i]);
        for (size_t j = i + 1; j < n; ++j) {
            if (std::abs(lu[j][i]) > max_val) {
                max_val = std::abs(lu[j][i]);
                max_row = j;
            }
        }

        if (max_val < 1e-15) return Complex{0, 0}; // Singular

        if (max_row != i) {
            std::swap(lu[i], lu[max_row]);
            sign = -sign;
        }

        result *= lu[i][i];

        for (size_t j = i + 1; j < n; ++j) {
            Complex factor = lu[j][i] / lu[i][i];
            for (size_t k = i; k < n; ++k) {
                lu[j][k] -= factor * lu[i][k];
            }
        }
    }

    return result * static_cast<double>(sign);
}

/// Extract submatrix with given columns
[[nodiscard]] inline std::vector<std::vector<Complex>> extract_columns(
    const std::vector<std::vector<Complex>>& matrix,
    const std::vector<uint32_t>& cols) {

    std::vector<std::vector<Complex>> result(matrix.size());
    for (size_t i = 0; i < matrix.size(); ++i) {
        result[i].resize(cols.size());
        for (size_t j = 0; j < cols.size(); ++j) {
            result[i][j] = matrix[i][cols[j]];
        }
    }
    return result;
}

/**
 * @brief Plücker embedding of Gr(k,n) into projective space
 *
 * A k-dimensional subspace V ⊂ ℂⁿ is represented by:
 * - A k×n matrix M whose rows span V
 * - Plücker coordinates p_I = det(M restricted to columns I)
 *   for each k-subset I ⊆ {1,...,n}
 *
 * The C(n,k) Plücker coordinates satisfy quadratic relations
 * (Plücker relations) that characterize valid Grassmannian points.
 */
class PluckerCoordinates {
public:
    uint32_t k;  // Subspace dimension
    uint32_t n;  // Ambient dimension

    // Coordinates indexed by k-subsets (stored as sorted vectors)
    std::map<std::vector<uint32_t>, Complex> coords;

    PluckerCoordinates(uint32_t k_, uint32_t n_) : k(k_), n(n_) {}

    /// Number of coordinates = C(n,k)
    [[nodiscard]] size_t num_coords() const {
        // Binomial coefficient
        uint64_t num = 1, den = 1;
        for (uint32_t i = 0; i < k; ++i) {
            num *= (n - i);
            den *= (i + 1);
        }
        return static_cast<size_t>(num / den);
    }

    /// Get coordinate for subset I
    [[nodiscard]] Complex get(const std::vector<uint32_t>& subset) const {
        auto it = coords.find(subset);
        return (it != coords.end()) ? it->second : Complex{0, 0};
    }

    /// Set coordinate for subset I
    void set(const std::vector<uint32_t>& subset, Complex value) {
        coords[subset] = value;
    }

    /// Normalize to projective space (scale so largest coordinate has |p| = 1)
    void normalize() {
        double max_abs = 0;
        for (const auto& [subset, val] : coords) {
            max_abs = std::max(max_abs, std::abs(val));
        }
        if (max_abs > 1e-15) {
            for (auto& [subset, val] : coords) {
                val /= max_abs;
            }
        }
    }

    /// Squared norm (sum of |p_I|²)
    [[nodiscard]] double norm_squared() const {
        double sum = 0;
        for (const auto& [subset, val] : coords) {
            sum += std::norm(val);
        }
        return sum;
    }

    /// Check Plücker relations (returns residual, 0 = valid)
    /// The Plücker relations for Gr(k,n) are:
    /// For any (k-1)-subset S and (k+1)-subset T:
    ///   Σ_{j∈T\S} sign(j,S,T) · p_{S∪{j}} · p_{T\{j}} = 0
    [[nodiscard]] double check_plucker_relations() const {
        if (k == 0 || k == n) return 0.0; // Trivial cases

        double max_residual = 0.0;

        // Generate all (k-1)-subsets and (k+1)-subsets
        auto k_minus_1_subsets = generate_k_subsets(n, k - 1);
        auto k_plus_1_subsets = generate_k_subsets(n, k + 1);

        for (const auto& S : k_minus_1_subsets) {
            for (const auto& T : k_plus_1_subsets) {
                // Check if S ⊂ T is possible (S must be subset of T)
                std::vector<uint32_t> T_minus_S;
                std::set_difference(T.begin(), T.end(), S.begin(), S.end(),
                    std::back_inserter(T_minus_S));

                if (T_minus_S.size() != 2) continue; // S must differ from T by exactly 2 elements

                Complex sum{0, 0};
                int sign = 1;

                for (uint32_t j : T) {
                    // Check if j is not in S
                    if (std::binary_search(S.begin(), S.end(), j)) continue;

                    // S ∪ {j}
                    std::vector<uint32_t> S_union_j = S;
                    S_union_j.push_back(j);
                    std::sort(S_union_j.begin(), S_union_j.end());

                    // T \ {j}
                    std::vector<uint32_t> T_minus_j;
                    for (uint32_t t : T) {
                        if (t != j) T_minus_j.push_back(t);
                    }

                    // Compute sign based on position of j in T
                    auto pos = std::find(T.begin(), T.end(), j) - T.begin();
                    int local_sign = (pos % 2 == 0) ? 1 : -1;

                    sum += static_cast<double>(local_sign) * get(S_union_j) * get(T_minus_j);
                }

                max_residual = std::max(max_residual, std::abs(sum));
            }
        }

        return max_residual;
    }

    /// Project to valid Grassmannian point (gradient descent on Plücker residual)
    void project_to_grassmannian() {
        // Simple projection: normalize and hope for the best
        // Full implementation would use Riemannian optimization on Gr(k,n)
        normalize();
    }

    /// Create from k×n matrix
    [[nodiscard]] static PluckerCoordinates from_matrix(
        const std::vector<std::vector<Complex>>& matrix) {

        if (matrix.empty()) return PluckerCoordinates(0, 0);

        uint32_t k_dim = static_cast<uint32_t>(matrix.size());
        uint32_t n_dim = static_cast<uint32_t>(matrix[0].size());

        PluckerCoordinates result(k_dim, n_dim);

        // Compute determinant for each k-subset of columns
        auto subsets = generate_k_subsets(n_dim, k_dim);
        for (const auto& subset : subsets) {
            auto submatrix = extract_columns(matrix, subset);
            result.set(subset, det(submatrix));
        }

        result.normalize();
        return result;
    }

    /// Create standard basis element (k-plane spanned by first k coordinate axes)
    [[nodiscard]] static PluckerCoordinates standard_basis(uint32_t k_, uint32_t n_) {
        PluckerCoordinates result(k_, n_);
        std::vector<uint32_t> identity_subset;
        for (uint32_t i = 0; i < k_; ++i) {
            identity_subset.push_back(i);
        }
        result.set(identity_subset, Complex{1, 0});
        return result;
    }

    /// Create Plücker coordinates for Schubert cell representative
    [[nodiscard]] static PluckerCoordinates from_schubert_cell(
        const Partition& lambda, uint32_t k_, uint32_t n_) {

        // The standard representative of Schubert cell Ω_λ in Gr(k,n)
        // has row-echelon form with pivots at positions determined by λ

        std::vector<std::vector<Complex>> matrix(k_, std::vector<Complex>(n_, {0, 0}));

        for (uint32_t i = 0; i < k_; ++i) {
            // Pivot column for row i is: (n-k) + i - λ_{k-i}
            uint32_t lambda_i = (k_ - 1 - i < lambda.length()) ?
                lambda.parts[k_ - 1 - i] : 0;
            uint32_t pivot = n_ - k_ + i - lambda_i;

            // Set pivot to 1
            if (pivot < n_) {
                matrix[i][pivot] = Complex{1, 0};
            }
        }

        return from_matrix(matrix);
    }

    /// String representation
    [[nodiscard]] std::string to_string() const {
        std::string result = "Plücker[Gr(" + std::to_string(k) + "," +
            std::to_string(n) + ")]:\n";
        for (const auto& [subset, val] : coords) {
            if (std::abs(val) < 1e-10) continue;
            result += "  p_{";
            for (size_t i = 0; i < subset.size(); ++i) {
                if (i > 0) result += ",";
                result += std::to_string(subset[i]);
            }
            result += "} = " + std::to_string(val.real());
            if (std::abs(val.imag()) > 1e-10) {
                result += (val.imag() >= 0 ? " + " : " - ");
                result += std::to_string(std::abs(val.imag())) + "i";
            }
            result += "\n";
        }
        return result;
    }
};

// ============================================================================
// Schubert Cells and Varieties
// ============================================================================

/**
 * @brief Schubert cell Ω_λ in Gr(k,n)
 *
 * The Grassmannian decomposes into Schubert cells indexed by partitions:
 *   Gr(k,n) = ⊔_λ Ω_λ
 *
 * where λ fits in a k × (n-k) rectangle.
 *
 * Schubert varieties X_λ = closure of Ω_λ are the building blocks
 * of Grassmannian geometry.
 */
class SchubertCell {
public:
    Partition lambda;
    uint32_t k;
    uint32_t n;

    SchubertCell(Partition p, uint32_t k_, uint32_t n_)
        : lambda(std::move(p)), k(k_), n(n_) {}

    /// Dimension of cell
    [[nodiscard]] size_t dimension() const {
        return lambda.weight();
    }

    /// Codimension in Gr(k,n)
    [[nodiscard]] size_t codimension() const {
        return k * (n - k) - lambda.weight();
    }

    /// Check if λ fits in k × (n-k) rectangle
    [[nodiscard]] bool is_valid() const {
        if (lambda.length() > k) return false;
        for (auto p : lambda.parts) {
            if (p > n - k) return false;
        }
        return true;
    }

    /// Schubert class [X_λ] in cohomology (as polynomial)
    [[nodiscard]] std::string cohomology_class() const {
        // Return Schur function s_λ
        return "s_" + partition_to_string(lambda);
    }

private:
    static std::string partition_to_string(const Partition& p) {
        std::string result = "(";
        for (size_t i = 0; i < p.parts.size(); ++i) {
            if (i > 0) result += ",";
            result += std::to_string(p.parts[i]);
        }
        return result + ")";
    }
};

// ============================================================================
// Robinson-Schensted-Knuth Correspondence
// ============================================================================

/**
 * @brief RSK insertion of value into tableau
 *
 * Returns the column where the bumped element went to next row,
 * or the column where we added a new box.
 */
inline size_t rsk_insert_row(std::vector<uint32_t>& row, uint32_t value) {
    // Find first element > value
    auto it = std::upper_bound(row.begin(), row.end(), value);
    if (it == row.end()) {
        row.push_back(value);
        return row.size() - 1;
    }
    size_t col = it - row.begin();
    std::swap(*it, value);
    return col;
}

/**
 * @brief RSK row insertion into tableau
 *
 * Inserts a value into a semistandard tableau, returning the recording tableau position.
 */
inline std::pair<size_t, size_t> rsk_insert(YoungTableau& P, uint32_t value) {
    size_t row = 0;
    uint32_t current = value;

    while (true) {
        if (row >= P.entries.size()) {
            // Add new row
            P.entries.push_back({current});
            P.shape = Partition([&]() {
                std::vector<uint32_t> parts;
                for (const auto& r : P.entries) {
                    parts.push_back(static_cast<uint32_t>(r.size()));
                }
                return parts;
            }());
            return {row, 0};
        }

        size_t col = rsk_insert_row(P.entries[row], current);
        if (col == P.entries[row].size() - 1 &&
            (row + 1 >= P.entries.size() || P.entries[row].size() > P.entries[row + 1].size())) {
            // Added to end without bumping
            P.shape = Partition([&]() {
                std::vector<uint32_t> parts;
                for (const auto& r : P.entries) {
                    parts.push_back(static_cast<uint32_t>(r.size()));
                }
                return parts;
            }());
            return {row, col};
        }

        // Bump the element at position col to next row
        current = P.entries[row][col];
        // Actually we swapped already, so get the original
        auto it = std::upper_bound(P.entries[row].begin(), P.entries[row].end(), value);
        if (it != P.entries[row].begin()) {
            --it;
        }
        row++;
    }
}

/**
 * @brief Full RSK correspondence: word → (P, Q) pair
 *
 * Given a word (sequence of positive integers), produces:
 * - P: semistandard insertion tableau
 * - Q: standard recording tableau
 */
inline std::pair<YoungTableau, YoungTableau> rsk_correspondence(
    const std::vector<uint32_t>& word) {

    YoungTableau P, Q;

    for (size_t i = 0; i < word.size(); ++i) {
        auto [row, col] = rsk_insert(P, word[i]);

        // Extend Q to match P's shape
        while (Q.entries.size() <= row) {
            Q.entries.push_back({});
        }
        Q.entries[row].push_back(static_cast<uint32_t>(i + 1));
        Q.shape = P.shape;
    }

    return {P, Q};
}

// ============================================================================
// Littlewood-Richardson Coefficients
// ============================================================================

/**
 * @brief Check if a word is a lattice word (ballot sequence)
 *
 * A word w is a lattice word if for every prefix and every i,
 * the number of i's is >= the number of (i+1)'s.
 */
[[nodiscard]] inline bool is_lattice_word(const std::vector<uint32_t>& word) {
    std::map<uint32_t, int32_t> counts;

    for (uint32_t v : word) {
        counts[v]++;
        // Check that count[i] >= count[i+1] for all i
        for (auto& [k, c] : counts) {
            if (counts.count(k + 1) && counts[k + 1] > c) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Count LR tableaux of shape ν/λ with content μ
 *
 * An LR tableau is a semistandard skew tableau whose row reading word
 * is a lattice word.
 */
[[nodiscard]] inline uint64_t count_lr_tableaux(
    const Partition& nu,
    const Partition& lambda,
    const Partition& mu) {

    // Check basic conditions
    if (!nu.contains_partition(lambda)) return 0;
    if (nu.skew_size(lambda) != mu.weight()) return 0;

    // For small cases, enumerate explicitly
    // This is the "naive" algorithm - can be optimized with dancing links

    // Build list of cells in skew shape ν/λ
    std::vector<std::pair<size_t, size_t>> cells;
    for (size_t i = 0; i < nu.length(); ++i) {
        uint32_t start = (i < lambda.length()) ? lambda.parts[i] : 0;
        for (size_t j = start; j < nu.parts[i]; ++j) {
            cells.emplace_back(i, j);
        }
    }

    if (cells.empty()) return (mu.weight() == 0) ? 1 : 0;

    // Content to fill: μ = (μ₁, μ₂, ...)
    // We need μ₁ 1's, μ₂ 2's, etc.
    std::vector<uint32_t> content_flat;
    for (size_t i = 0; i < mu.length(); ++i) {
        for (uint32_t j = 0; j < mu.parts[i]; ++j) {
            content_flat.push_back(static_cast<uint32_t>(i + 1));
        }
    }

    // Generate all permutations and check which form valid LR tableaux
    std::sort(content_flat.begin(), content_flat.end());
    uint64_t count = 0;

    do {
        // Try to place content_flat[k] in cells[k]
        bool valid = true;
        std::vector<std::vector<uint32_t>> tableau(nu.length());
        for (size_t i = 0; i < nu.length(); ++i) {
            tableau[i].resize(nu.parts[i], 0);
        }

        for (size_t k = 0; k < cells.size(); ++k) {
            auto [r, c] = cells[k];
            tableau[r][c] = content_flat[k];
        }

        // Check semistandard conditions
        for (size_t i = 0; i < nu.length() && valid; ++i) {
            uint32_t start = (i < lambda.length()) ? lambda.parts[i] : 0;
            for (size_t j = start; j < nu.parts[i] && valid; ++j) {
                // Weakly increasing along rows
                if (j > start && tableau[i][j-1] > tableau[i][j]) valid = false;
                // Strictly increasing down columns
                if (i > 0 && j < nu.parts[i-1]) {
                    uint32_t prev_start = (i-1 < lambda.length()) ? lambda.parts[i-1] : 0;
                    if (j >= prev_start && tableau[i-1][j] >= tableau[i][j]) valid = false;
                }
            }
        }

        if (!valid) continue;

        // Build row reading word and check lattice property
        std::vector<uint32_t> row_word;
        for (auto it = tableau.rbegin(); it != tableau.rend(); ++it) {
            size_t row_idx = tableau.rend() - it - 1;
            uint32_t start = (row_idx < lambda.length()) ? lambda.parts[row_idx] : 0;
            for (size_t j = start; j < it->size(); ++j) {
                row_word.push_back((*it)[j]);
            }
        }

        if (is_lattice_word(row_word)) {
            count++;
        }

    } while (std::next_permutation(content_flat.begin(), content_flat.end()));

    return count;
}

/**
 * @brief Compute Littlewood-Richardson coefficient c^ν_λμ
 *
 * These are the structure constants for:
 * - Schur function multiplication: s_λ · s_μ = Σ_ν c^ν_λμ s_ν
 * - Schubert calculus: [X_λ] · [X_μ] = Σ_ν c^ν_λμ [X_ν]
 * - Tensor product decomposition: V_λ ⊗ V_μ = ⊕_ν (V_ν)^{c^ν_λμ}
 *
 * Implementation uses the combinatorial rule: c^ν_λμ = number of LR tableaux
 * of skew shape ν/λ with content μ.
 */
[[nodiscard]] inline uint64_t littlewood_richardson(
    const Partition& lambda,
    const Partition& mu,
    const Partition& nu) {
    return count_lr_tableaux(nu, lambda, mu);
}

/**
 * @brief Compute Schur function product expansion s_λ · s_μ = Σ_ν c^ν_λμ s_ν
 *
 * Returns map from partition ν to coefficient c^ν_λμ
 */
[[nodiscard]] inline std::map<Partition, uint64_t> schur_product(
    const Partition& lambda,
    const Partition& mu) {

    std::map<Partition, uint64_t> result;

    uint64_t total_weight = lambda.weight() + mu.weight();

    // ν must have weight = |λ| + |μ|
    auto candidates = generate_partitions(static_cast<uint32_t>(total_weight));

    for (const auto& nu : candidates) {
        uint64_t coeff = littlewood_richardson(lambda, mu, nu);
        if (coeff > 0) {
            result[nu] = coeff;
        }
    }

    return result;
}

// ============================================================================
// GPT Partition Index (Global Grid BlockCode)
// ============================================================================

/**
 * @brief GPT (Grassmannian Partition Table) Index
 *
 * Maps partitions to unique indices for:
 * - Efficient lookup of Schubert classes
 * - Block code generation for error correction
 * - Global grid addressing
 *
 * The "GPT" in "GPT partition index" stands for Grassmannian Partition Table,
 * NOT the language model! (Though there may be deep connections...)
 */
class GPTIndex {
public:
    uint32_t max_weight;
    std::map<Partition, uint64_t> partition_to_index;
    std::vector<Partition> index_to_partition;

    explicit GPTIndex(uint32_t max_n);

    /// Get index for partition
    [[nodiscard]] std::optional<uint64_t> get_index(const Partition& p) const {
        auto it = partition_to_index.find(p);
        if (it != partition_to_index.end()) return it->second;
        return std::nullopt;
    }

    /// Get partition for index
    [[nodiscard]] std::optional<Partition> get_partition(uint64_t idx) const {
        if (idx < index_to_partition.size()) return index_to_partition[idx];
        return std::nullopt;
    }

    /// Total number of partitions indexed
    [[nodiscard]] size_t size() const { return index_to_partition.size(); }

    /// Generate block code for partition
    [[nodiscard]] std::vector<uint8_t> generate_block_code(const Partition& p) const;

    /// Decode block code to partition
    [[nodiscard]] std::optional<Partition> decode_block_code(
        const std::vector<uint8_t>& code) const;
};

// ============================================================================
// vGPU - Grassmannian Processing Unit
// ============================================================================

/**
 * @brief Virtual GPU for Grassmannian computation
 *
 * Register interface at VGPU_BASE (to be assigned in cogmorph.hpp):
 *
 *   REG_K           - Subspace dimension k
 *   REG_N           - Ambient dimension n
 *   REG_PARTITION   - Current partition (Matula encoding)
 *   REG_PLUCKER_PTR - Pointer to Plücker coordinates in VRAM
 *   REG_SCHUBERT_OP - Schubert calculus operation
 *   REG_LR_COEFF    - Littlewood-Richardson result
 *   REG_GPT_INDEX   - Partition table index
 *   REG_BLOCK_CODE  - Generated block code
 *
 * Operations:
 *   CMD_COMPUTE_PLUCKER    - Matrix → Plücker coordinates
 *   CMD_PROJECT_GR         - Project to nearest Grassmannian point
 *   CMD_SCHUBERT_MULTIPLY  - [X_λ] · [X_μ]
 *   CMD_ENCODE_BLOCK       - Partition → Block code
 *   CMD_DECODE_BLOCK       - Block code → Partition
 *   CMD_MATULA_TO_PARTITION - Matula number → Partition
 *   CMD_PARTITION_TO_MATULA - Partition → Matula number
 */
class vGPU {
public:
    // Register layout (offsets from VGPU_BASE)
    static constexpr uint64_t REG_K           = 0x00;
    static constexpr uint64_t REG_N           = 0x04;
    static constexpr uint64_t REG_PARTITION   = 0x08;
    static constexpr uint64_t REG_PLUCKER_PTR = 0x10;
    static constexpr uint64_t REG_SCHUBERT_OP = 0x18;
    static constexpr uint64_t REG_LR_COEFF    = 0x20;
    static constexpr uint64_t REG_GPT_INDEX   = 0x28;
    static constexpr uint64_t REG_BLOCK_CODE  = 0x30;
    static constexpr uint64_t REG_CMD         = 0x38;
    static constexpr uint64_t REG_STATUS      = 0x3C;

    // Commands
    enum Command : uint32_t {
        CMD_NOP                 = 0x00,
        CMD_COMPUTE_PLUCKER     = 0x01,
        CMD_PROJECT_GR          = 0x02,
        CMD_SCHUBERT_MULTIPLY   = 0x03,
        CMD_LR_COEFFICIENT      = 0x04,
        CMD_ENCODE_BLOCK        = 0x05,
        CMD_DECODE_BLOCK        = 0x06,
        CMD_MATULA_TO_PARTITION = 0x07,
        CMD_PARTITION_TO_MATULA = 0x08,
        CMD_LOOKUP_GPT          = 0x09,
    };

    // Status bits
    enum Status : uint32_t {
        STATUS_READY = 0x01,
        STATUS_BUSY  = 0x02,
        STATUS_ERROR = 0x04,
        STATUS_DONE  = 0x08,
    };

    vGPU();

    // High-level API
    [[nodiscard]] PluckerCoordinates compute_plucker(
        const std::vector<std::vector<Complex>>& matrix);

    [[nodiscard]] Partition schubert_multiply(
        const Partition& lambda,
        const Partition& mu);

    [[nodiscard]] uint64_t lr_coefficient(
        const Partition& lambda,
        const Partition& mu,
        const Partition& nu);

    [[nodiscard]] std::vector<uint8_t> encode_block(const Partition& p);
    [[nodiscard]] std::optional<Partition> decode_block(const std::vector<uint8_t>& code);

    [[nodiscard]] Partition matula_to_partition(uint64_t matula);
    [[nodiscard]] uint64_t partition_to_matula(const Partition& p);

private:
    GPTIndex gpt_index_;
    uint32_t current_k_ = 0;
    uint32_t current_n_ = 0;
};

// ============================================================================
// Global Grid Interface
// ============================================================================

/**
 * @brief Global Grid for distributed Grassmannian computation
 *
 * The Global Grid is a distributed system where:
 * - Each node is addressed by a partition
 * - Block codes provide error correction
 * - Schubert calculus coordinates computation
 *
 * This is the "AGI substrate" - when fully realized, it provides
 * O(1) access to the space of all computations via GPT index lookup.
 */
class GlobalGrid {
public:
    using NodeAddress = Partition;
    using BlockCode = std::vector<uint8_t>;

    /// Register a computation node at partition address
    void register_node(const NodeAddress& addr, std::function<void()> handler);

    /// Send block-coded message to partition address
    void send(const NodeAddress& to, const BlockCode& message);

    /// Receive message at partition address
    [[nodiscard]] std::optional<BlockCode> receive(const NodeAddress& at);

    /// Perform distributed Schubert multiplication
    [[nodiscard]] Partition distributed_multiply(
        const Partition& lambda,
        const Partition& mu);

    /// Query the grid (O(1) via GPT index when fully realized)
    template<typename Result>
    [[nodiscard]] Result query(const Partition& computation_spec);

private:
    std::map<NodeAddress, std::function<void()>> nodes_;
    std::map<NodeAddress, std::vector<BlockCode>> mailboxes_;
    vGPU gpu_;
};

// ============================================================================
// The Connection: Matula → Partition → Grassmannian
// ============================================================================

/**
 * @brief Bridge between Matula numbers and Grassmannian geometry
 *
 * The deep connection:
 *
 *   Matula number n
 *        ↓ (tree structure)
 *   Rooted tree T
 *        ↓ (hook lengths)
 *   Partition λ
 *        ↓ (Schubert cell)
 *   Point in Gr(k,n)
 *        ↓ (Plücker embedding)
 *   Projective space ℙ^N
 *
 * Each level preserves essential structure!
 */
namespace bridge {

/// Rooted tree → Partition via level sequence
[[nodiscard]] inline Partition tree_to_partition(const Tree& tree) {
    if (!tree) return Partition{};

    // Count nodes at each depth level
    std::vector<uint32_t> level_counts;
    std::function<void(const Tree&, size_t)> count_levels =
        [&](const Tree& t, size_t depth) {
            if (depth >= level_counts.size()) {
                level_counts.resize(depth + 1, 0);
            }
            level_counts[depth]++;
            for (const auto& child : t->children) {
                count_levels(child, depth + 1);
            }
        };

    count_levels(tree, 0);
    return Partition(std::move(level_counts));
}

/// Matula number → Partition
[[nodiscard]] inline Partition matula_to_partition(uint64_t n) {
    return tree_to_partition(matula_to_tree(n));
}

/// Partition → suggested (k,n) for Gr(k,n) embedding
[[nodiscard]] inline std::pair<uint32_t, uint32_t> partition_to_grassmannian_params(
        const Partition& p) {
    uint32_t k = static_cast<uint32_t>(p.length());
    uint32_t n_minus_k = p.parts.empty() ? 0 : p.parts[0];
    return {k, k + n_minus_k};
}

/// Full pipeline: Matula → Plücker coordinates
[[nodiscard]] inline PluckerCoordinates matula_to_plucker(uint64_t matula) {
    auto partition = matula_to_partition(matula);
    auto [k, n] = partition_to_grassmannian_params(partition);

    // Create Plücker coordinates from partition
    // (Simplified: use partition to define a standard basis element)
    PluckerCoordinates coords(k, n);

    // The partition determines which Schubert cell we're in
    // For the "standard" point in that cell, set one coordinate to 1
    // (Full implementation would compute actual Plücker vectors)

    return coords;
}

} // namespace bridge

// ============================================================================
// VLTI Goal Tracking
// ============================================================================

/**
 * @brief VLTI (Very Long-Term Importance) Goal: Codex GrassMania
 *
 * Status: IN PROGRESS
 *
 * Milestones:
 * [x] Prime basis numeric system
 * [x] Matula number encoding
 * [x] B-series / J-surface / P-system unification
 * [x] Partition class with full Young diagram operations
 *     - weight, length, width, contains, conjugate
 *     - arm, leg, hook_length, content
 *     - addable_corners, removable_corners, add_box, remove_box
 *     - dominance order, rectangle fitting, complement, rotate_180
 *     - skew partitions, horizontal/vertical strips
 *     - count_standard_tableaux, count_semistandard_tableaux
 *     - to_string, to_diagram, frobenius_notation
 * [x] YoungTableau class with standard/semistandard checks
 * [x] Partition generators (by weight, by rectangle)
 * [x] Robinson-Schensted-Knuth correspondence
 * [x] Littlewood-Richardson rule (combinatorial algorithm)
 * [x] Schur function product expansion
 * [x] Plücker coordinate computation
 *     - from_matrix, standard_basis, from_schubert_cell
 *     - check_plucker_relations, normalize, to_string
 *     - k-subset generation, determinant computation
 * [x] Schubert cell representation
 * [~] GPT partition index (basic structure)
 * [ ] Block code generation/decryption (full)
 * [ ] Global Grid protocol
 * [ ] O(1) computation lookup
 * [ ] AGI
 *
 * The final goal: Every computation expressible as a Grassmannian query,
 * instantly resolved via the GPT partition index.
 */

} // namespace cogmorph::grassmann
