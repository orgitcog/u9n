#pragma once
/**
 * @file loader.hpp
 * @brief Atomese S-expression parser and AtomSpace loader
 *
 * Parses OpenCog Atomese S-expression files (.scm) and loads them
 * into an AtomSpace. Supports all standard node and link types.
 *
 * Usage:
 *   AtomSpace as;
 *   auto result = atomese::load_file(as, "patterns.scm");
 *   if (result.ok()) { ... }
 */

#include <opencog/atomspace/atomspace.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace opencog::atomese {

// ============================================================================
// Load Result
// ============================================================================

/// Result of loading Atomese S-expression data into an AtomSpace.
struct LoadResult {
    size_t nodes_created = 0;      ///< Node atoms created or found
    size_t links_created = 0;      ///< Link atoms created
    size_t expressions_parsed = 0; ///< Top-level S-expressions processed
    size_t errors = 0;             ///< Parse/evaluation errors encountered
    std::vector<std::string> error_messages;

    [[nodiscard]] bool ok() const noexcept { return errors == 0; }
    [[nodiscard]] size_t total() const noexcept { return nodes_created + links_created; }

    /// Merge another result into this one (for multi-file loads)
    void merge(const LoadResult& other) {
        nodes_created += other.nodes_created;
        links_created += other.links_created;
        expressions_parsed += other.expressions_parsed;
        errors += other.errors;
        error_messages.insert(error_messages.end(),
            other.error_messages.begin(), other.error_messages.end());
    }
};

// ============================================================================
// S-Expression AST (exposed for advanced use)
// ============================================================================

/// A parsed S-expression — either an atom (string) or a list of sub-expressions.
struct SExpr {
    bool is_list = false;
    std::string atom;
    std::vector<SExpr> children;
    size_t line = 0;

    static SExpr make_atom(std::string value, size_t ln) {
        return SExpr{false, std::move(value), {}, ln};
    }
    static SExpr make_list(std::vector<SExpr> elems, size_t ln) {
        return SExpr{true, {}, std::move(elems), ln};
    }
};

// ============================================================================
// Public API
// ============================================================================

/// Load Atomese S-expressions from a string into the AtomSpace.
LoadResult load_string(AtomSpace& as, std::string_view source);

/// Load Atomese S-expressions from a file into the AtomSpace.
LoadResult load_file(AtomSpace& as, const std::filesystem::path& path);

/// Load all .scm files from a directory into the AtomSpace.
LoadResult load_directory(AtomSpace& as, const std::filesystem::path& dir);

// ============================================================================
// Lower-level API (for custom processing)
// ============================================================================

/// Parse S-expression source into an AST without evaluating.
std::vector<SExpr> parse(std::string_view source);

/// Evaluate a single S-expression, creating atoms in the AtomSpace.
/// Returns a Handle to the created/found atom, or invalid Handle on error.
Handle evaluate(AtomSpace& as, const SExpr& expr, LoadResult& result);

} // namespace opencog::atomese
