/**
 * @file matula-numbers.hpp
 * @brief Matula-Göbel Numbers: Bijection between ℕ and Rooted Trees
 *
 * The Matula-Göbel encoding provides a 1-1 correspondence between
 * positive integers and rooted trees (unlabeled, unordered).
 *
 * This unifies three computational domains:
 *
 * 1. B-SERIES (Butcher, 1963)
 *    - Rooted trees index Runge-Kutta methods
 *    - Elementary weights for numerical integration
 *    - Order conditions as tree equations
 *
 * 2. J-SURFACES (Lie Derivatives)
 *    - Elementary differentials for ODEs
 *    - Continuous flows on manifolds
 *    - Analytical gradient functions
 *
 * 3. P-SYSTEMS (Membrane Computing, Păun 2000)
 *    - Free hyper-multisets
 *    - Categorical/topological evolution
 *    - Rewriting systems on tree structures
 *
 * THE KEY INSIGHT:
 *   All three domains share the same combinatorial structure!
 *   By using Matula numbers as machine code indices, we can:
 *   - Compute discrete integration (B-series)
 *   - Compute continuous derivatives (J-surfaces)
 *   - Compute membrane evolution (P-systems)
 *   ...all with simple prime arithmetic, avoiding combinatorial explosion!
 *
 * ENCODING:
 *   n = 1           →  single node (leaf)
 *   n = prime(k)    →  tree with root + subtree encoded by k
 *   n = p₁×p₂×...   →  tree with root + children for each factor
 *
 * EXAMPLES:
 *   1 = •                           (leaf)
 *   2 = prime(1) = •─•              (chain of 2)
 *   3 = prime(2) = •─•─•            (chain of 3)
 *   4 = 2² = •<•                    (root with 2 identical children)
 *            └•
 *   5 = prime(3) = •─•─•─•          (chain of 4)
 *   6 = 2×3 = •<•                   (root with children: leaf, chain-2)
 *             └•─•
 *   7 = prime(4) = tree(4) as child
 *
 * SEQUENCE: A061773 in OEIS (inverse mapping)
 */

#pragma once

#include "prime-basis.hpp"
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>

namespace cogmorph::matula {

using namespace cogmorph::prime;

// ============================================================================
// Rooted Tree Structure
// ============================================================================

/**
 * @brief A rooted tree node
 *
 * Trees are unordered (children form multiset) and unlabeled.
 * Two trees are equal iff they have the same Matula number.
 */
struct TreeNode {
    std::vector<std::shared_ptr<TreeNode>> children;

    TreeNode() = default;

    /// Create leaf
    static std::shared_ptr<TreeNode> leaf() {
        return std::make_shared<TreeNode>();
    }

    /// Create tree with given children
    static std::shared_ptr<TreeNode> with_children(
            std::vector<std::shared_ptr<TreeNode>> kids) {
        auto node = std::make_shared<TreeNode>();
        node->children = std::move(kids);
        return node;
    }

    /// Number of children
    [[nodiscard]] size_t degree() const { return children.size(); }

    /// Is this a leaf?
    [[nodiscard]] bool is_leaf() const { return children.empty(); }

    /// Count total nodes in tree
    [[nodiscard]] size_t node_count() const {
        size_t count = 1;
        for (const auto& child : children) {
            count += child->node_count();
        }
        return count;
    }

    /// Height of tree (leaf = 0)
    [[nodiscard]] size_t height() const {
        size_t max_child_height = 0;
        for (const auto& child : children) {
            max_child_height = std::max(max_child_height, child->height() + 1);
        }
        return max_child_height;
    }
};

using Tree = std::shared_ptr<TreeNode>;

// ============================================================================
// Matula Number ↔ Tree Bijection
// ============================================================================

/**
 * @brief Convert Matula number to rooted tree
 *
 * Decoding:
 *   1 → leaf
 *   prime(k) → tree with single child = decode(k)
 *   n = ∏pᵢ^aᵢ → tree with children = multiset of decode(index(pᵢ))
 */
[[nodiscard]] inline Tree matula_to_tree(uint64_t n) {
    if (n == 0) return nullptr;
    if (n == 1) return TreeNode::leaf();

    // Factorize n
    auto fact = Factorization::from_integer(n);

    // Each prime factor p at index i contributes decode(i+1) as a child
    // Exponent = multiplicity of that child
    std::vector<Tree> children;

    for (const auto& [prime_idx, exponent] : fact.factors()) {
        // prime_idx is 0-based, but we need 1-based for recursion
        // prime(0) = 2 encodes tree for "1" (leaf)
        // So prime index i encodes tree for (i+1)
        Tree child_tree = matula_to_tree(prime_idx + 1);

        // Add 'exponent' copies of this child
        for (Exponent e = 0; e < exponent; ++e) {
            children.push_back(child_tree);
        }
    }

    return TreeNode::with_children(std::move(children));
}

/**
 * @brief Convert rooted tree to Matula number
 *
 * Encoding:
 *   leaf → 1
 *   tree with children {T₁, T₂, ...} → ∏ prime(encode(Tᵢ) - 1)
 */
[[nodiscard]] inline uint64_t tree_to_matula(const Tree& tree) {
    if (!tree) return 0;
    if (tree->is_leaf()) return 1;

    Factorization result = Factorization::one();

    for (const auto& child : tree->children) {
        uint64_t child_matula = tree_to_matula(child);
        // child_matula maps to prime index (child_matula - 1)
        // So we multiply by the (child_matula - 1)th prime
        if (child_matula > 0) {
            PrimeIndex idx = static_cast<PrimeIndex>(child_matula - 1);
            result = result * Factorization::prime(idx, 1);
        }
    }

    auto n = result.to_integer();
    return n.value_or(0);
}

// ============================================================================
// B-Series: Runge-Kutta Methods via Trees
// ============================================================================

/**
 * @brief Elementary weight for B-series (Butcher theory)
 *
 * The elementary weight Φ(t) for a tree t determines the
 * contribution of that tree to a Runge-Kutta method.
 *
 * For tree t with root and children {t₁, t₂, ...}:
 *   Φ(t) = Σⱼ aᵢⱼ · Φ(t₁) · Φ(t₂) · ...
 *
 * where aᵢⱼ are the RK coefficients.
 */
struct BSeries {
    /// Elementary weight function type
    using WeightFunc = std::function<double(uint64_t matula)>;

    /// Order (number of nodes) of a tree
    [[nodiscard]] static size_t order(uint64_t matula) {
        auto tree = matula_to_tree(matula);
        return tree ? tree->node_count() : 0;
    }

    /// Symmetry factor σ(t) - number of tree automorphisms
    [[nodiscard]] static uint64_t symmetry(uint64_t matula) {
        auto tree = matula_to_tree(matula);
        if (!tree || tree->is_leaf()) return 1;

        // Count multiplicities of identical children
        std::unordered_map<uint64_t, int> child_counts;
        for (const auto& child : tree->children) {
            child_counts[tree_to_matula(child)]++;
        }

        // σ(t) = ∏ nᵢ! · ∏ σ(tᵢ)^nᵢ
        uint64_t sigma = 1;
        for (const auto& [child_matula, count] : child_counts) {
            // Factorial of count
            for (int i = 2; i <= count; ++i) {
                sigma *= i;
            }
            // Times symmetry of child, raised to count
            uint64_t child_sym = symmetry(child_matula);
            for (int i = 0; i < count; ++i) {
                sigma *= child_sym;
            }
        }
        return sigma;
    }

    /// Density γ(t) = n! / α(t) where α(t) = #monotonic labelings
    [[nodiscard]] static double density(uint64_t matula) {
        auto tree = matula_to_tree(matula);
        if (!tree) return 0.0;

        size_t n = tree->node_count();

        // Recursive computation: γ(t) = n · ∏ γ(tᵢ)
        std::function<double(const Tree&)> gamma_rec = [&](const Tree& t) -> double {
            if (t->is_leaf()) return 1.0;
            double result = static_cast<double>(t->node_count());
            for (const auto& child : t->children) {
                result *= gamma_rec(child);
            }
            return result;
        };

        return gamma_rec(tree);
    }

    /// Generate all trees up to given order
    [[nodiscard]] static std::vector<uint64_t> trees_up_to_order(size_t max_order) {
        std::vector<uint64_t> result;

        // Enumerate Matula numbers and filter by order
        // This is brute-force; could be made more efficient
        for (uint64_t n = 1; n <= 100; ++n) {  // Limit for safety
            if (order(n) <= max_order) {
                result.push_back(n);
            }
        }
        return result;
    }
};

// ============================================================================
// J-Surfaces: Elementary Differentials for ODEs
// ============================================================================

/**
 * @brief Elementary differential for J-surfaces (Lie theory)
 *
 * For an ODE y' = f(y), each rooted tree t defines an
 * elementary differential F(t)(y):
 *
 *   F(•) = f                           (single node)
 *   F(t) = f^(k)(F(t₁), ..., F(tₖ))    (t has children t₁,...,tₖ)
 *
 * where f^(k) is the k-th derivative of f.
 */
struct JSurface {
    /// Differential order (same as tree node count)
    [[nodiscard]] static size_t diff_order(uint64_t matula) {
        return BSeries::order(matula);
    }

    /// Structure of the elementary differential
    [[nodiscard]] static std::string diff_structure(uint64_t matula) {
        auto tree = matula_to_tree(matula);
        if (!tree) return "";
        if (tree->is_leaf()) return "f";

        std::ostringstream oss;
        oss << "f";
        if (tree->degree() > 1) {
            oss << "^(" << tree->degree() << ")";
        } else {
            oss << "'";
        }
        oss << "(";
        bool first = true;
        for (const auto& child : tree->children) {
            if (!first) oss << ", ";
            first = false;
            oss << diff_structure(tree_to_matula(child));
        }
        oss << ")";
        return oss.str();
    }

    /// Coefficient in Taylor series (related to density)
    [[nodiscard]] static double taylor_coeff(uint64_t matula) {
        double gamma = BSeries::density(matula);
        uint64_t sigma = BSeries::symmetry(matula);
        return 1.0 / (gamma * sigma);
    }
};

// ============================================================================
// P-Systems: Membrane Computing via Tree Rewriting
// ============================================================================

/**
 * @brief Membrane structure as Matula-encoded tree
 *
 * In membrane computing (P-systems):
 *   - Each membrane is a node
 *   - Nested membranes are children
 *   - Rules rewrite membrane configurations
 *
 * Matula numbers let us:
 *   - Represent membrane structures as integers
 *   - Compose membranes via multiplication
 *   - Match/rewrite via factorization
 */
struct PSystem {
    /// A membrane configuration (tree encoded as Matula number)
    using Membrane = uint64_t;

    /// A rewriting rule: if membrane matches 'pattern', replace with 'replacement'
    struct Rule {
        Membrane pattern;
        Membrane replacement;
        double rate;  // Stochastic rate (optional)

        Rule(Membrane p, Membrane r, double rt = 1.0)
            : pattern(p), replacement(r), rate(rt) {}
    };

    /// Check if pattern divides (is contained in) membrane
    [[nodiscard]] static bool matches(Membrane membrane, Membrane pattern) {
        if (pattern == 0) return false;
        if (pattern == 1) return true;  // Empty pattern always matches

        auto mem_fact = Factorization::from_integer(membrane);
        auto pat_fact = Factorization::from_integer(pattern);

        // Pattern matches if all its prime factors are in membrane
        // with at least the same exponents
        for (const auto& [idx, exp] : pat_fact.factors()) {
            if (mem_fact[idx] < exp) return false;
        }
        return true;
    }

    /// Apply a rule: divide out pattern, multiply in replacement
    [[nodiscard]] static Membrane apply_rule(Membrane membrane, const Rule& rule) {
        if (!matches(membrane, rule.pattern)) return membrane;

        auto mem_fact = Factorization::from_integer(membrane);
        auto pat_fact = Factorization::from_integer(rule.pattern);
        auto rep_fact = Factorization::from_integer(rule.replacement);

        // Remove pattern, add replacement
        auto result = (mem_fact / pat_fact) * rep_fact;
        return result.to_integer().value_or(0);
    }

    /// Compose two membranes (put side by side as siblings)
    [[nodiscard]] static Membrane compose(Membrane a, Membrane b) {
        auto fa = Factorization::from_integer(a);
        auto fb = Factorization::from_integer(b);
        return (fa * fb).to_integer().value_or(0);
    }

    /// Nest membrane b inside membrane a
    [[nodiscard]] static Membrane nest(Membrane outer, Membrane inner) {
        // Nesting means inner becomes a child of outer's root
        // In Matula encoding: multiply outer by prime(inner-1)
        if (inner == 0) return outer;
        PrimeIndex idx = static_cast<PrimeIndex>(inner - 1);
        auto fo = Factorization::from_integer(outer);
        auto result = fo * Factorization::prime(idx, 1);
        return result.to_integer().value_or(0);
    }

    /// Extract multiset of top-level children
    [[nodiscard]] static std::vector<Membrane> children(Membrane membrane) {
        std::vector<Membrane> result;
        auto fact = Factorization::from_integer(membrane);

        for (const auto& [idx, exp] : fact.factors()) {
            // Child is encoded by prime index + 1
            Membrane child = idx + 1;
            for (Exponent e = 0; e < exp; ++e) {
                result.push_back(child);
            }
        }
        return result;
    }
};

// ============================================================================
// Unified Machine Code: Prime Power Series Instructions
// ============================================================================

/**
 * @brief Machine instruction encoded as Matula number
 *
 * Each Matula number defines an operation that can be interpreted in
 * all three domains simultaneously:
 *
 *   Instruction n:
 *     - B-series: Compute elementary weight for tree(n)
 *     - J-surface: Evaluate elementary differential F(tree(n))
 *     - P-system: Apply membrane rule encoded by tree(n)
 *
 * Arithmetic on instructions:
 *   - Multiply (n × m): Compose operations (parallel children)
 *   - Divide (n / m): Factor out sub-operation
 *   - Prime (pₖ): Lift to next depth level
 *
 * This avoids combinatorial explosion because:
 *   - Tree structure is implicit in factorization
 *   - No explicit tree construction needed for most operations
 *   - Matching/composition are just GCD/LCM
 */
struct MatulaInstruction {
    uint64_t code;
    Factorization factorization;

    MatulaInstruction(uint64_t n)
        : code(n), factorization(Factorization::from_integer(n)) {}

    /// Decode to tree (only when needed)
    [[nodiscard]] Tree to_tree() const { return matula_to_tree(code); }

    /// Order/complexity
    [[nodiscard]] size_t order() const { return BSeries::order(code); }

    /// Compose instructions (parallel execution)
    MatulaInstruction operator*(const MatulaInstruction& other) const {
        auto result = factorization * other.factorization;
        return MatulaInstruction(result.to_integer().value_or(0));
    }

    /// Sequence instructions (nesting)
    MatulaInstruction sequence(const MatulaInstruction& next) const {
        // Nest 'next' inside 'this'
        return MatulaInstruction(PSystem::nest(code, next.code));
    }

    /// Check if this contains sub-instruction
    [[nodiscard]] bool contains(const MatulaInstruction& sub) const {
        return PSystem::matches(code, sub.code);
    }

    /// Factor out sub-instruction
    MatulaInstruction operator/(const MatulaInstruction& sub) const {
        if (!contains(sub)) return *this;
        auto result = factorization / sub.factorization;
        return MatulaInstruction(result.to_integer().value_or(1));
    }

    /// Execute as B-series term (returns coefficient)
    [[nodiscard]] double exec_bseries() const {
        return 1.0 / BSeries::density(code);
    }

    /// Execute as J-surface (returns differential expression)
    [[nodiscard]] std::string exec_jsurface() const {
        return JSurface::diff_structure(code);
    }

    /// Execute as P-system step (given current membrane)
    [[nodiscard]] uint64_t exec_psystem(uint64_t membrane,
                                         const PSystem::Rule& rule) const {
        return PSystem::apply_rule(membrane, rule);
    }
};

// ============================================================================
// Instruction Set: Common Matula Operations
// ============================================================================

namespace opcodes {
    // Basic trees
    constexpr uint64_t NOP   = 1;   // • (leaf, identity)
    constexpr uint64_t CHAIN1 = 2;  // •─• (prime(1))
    constexpr uint64_t CHAIN2 = 3;  // •─•─• (prime(2))
    constexpr uint64_t FORK2  = 4;  // •<• (2²)
    constexpr uint64_t CHAIN3 = 5;  // •─•─•─• (prime(3))
    constexpr uint64_t MIXED1 = 6;  // •<•  (2×3)
                                     //  └•─•
    constexpr uint64_t CHAIN4 = 7;  // prime(4)

    // Derived operations
    constexpr uint64_t EULER  = 2;   // Forward Euler (simplest RK)
    constexpr uint64_t MIDPOINT = 3; // Midpoint method
    constexpr uint64_t RK4_PART = 6; // Part of RK4
}

// ============================================================================
// Tree Visualization
// ============================================================================

/// Convert tree to ASCII art
[[nodiscard]] inline std::string tree_to_ascii(const Tree& tree, int depth = 0) {
    if (!tree) return "";

    std::ostringstream oss;
    std::string indent(depth * 2, ' ');

    oss << indent << "●";
    if (!tree->is_leaf()) {
        oss << "┐";
    }
    oss << "\n";

    for (size_t i = 0; i < tree->children.size(); ++i) {
        bool last = (i == tree->children.size() - 1);
        std::string prefix = indent + (last ? " └" : " ├");
        oss << prefix << "─";
        // Recursively print child (simplified)
        if (tree->children[i]->is_leaf()) {
            oss << "●\n";
        } else {
            oss << "●┐\n";
            oss << tree_to_ascii(tree->children[i], depth + 2);
        }
    }

    return oss.str();
}

/// Convert tree to Newick format (standard for phylogenetics)
[[nodiscard]] inline std::string tree_to_newick(const Tree& tree) {
    if (!tree) return "";
    if (tree->is_leaf()) return "()";

    std::ostringstream oss;
    oss << "(";
    bool first = true;
    for (const auto& child : tree->children) {
        if (!first) oss << ",";
        first = false;
        oss << tree_to_newick(child);
    }
    oss << ")";
    return oss.str();
}

// ============================================================================
// Integration with CogMorph
// ============================================================================

/// Encode AtomSpace link structure as Matula number
[[nodiscard]] inline uint64_t link_to_matula(const std::vector<uint64_t>& outgoing) {
    if (outgoing.empty()) return 1;  // Leaf node

    Factorization result = Factorization::one();
    for (uint64_t target : outgoing) {
        // Each target atom ID maps to a prime index
        // (Simplification: use atom ID directly, mod 64 for safety)
        PrimeIndex idx = static_cast<PrimeIndex>(target % 64);
        result = result * Factorization::prime(idx, 1);
    }
    return result.to_integer().value_or(0);
}

/// Decode Matula number to link structure
[[nodiscard]] inline std::vector<uint64_t> matula_to_link(uint64_t matula) {
    std::vector<uint64_t> outgoing;
    auto fact = Factorization::from_integer(matula);

    for (const auto& [idx, exp] : fact.factors()) {
        for (Exponent e = 0; e < exp; ++e) {
            outgoing.push_back(idx);  // Prime index as target
        }
    }
    return outgoing;
}

} // namespace cogmorph::matula
