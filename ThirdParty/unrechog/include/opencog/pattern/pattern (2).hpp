#pragma once
/**
 * @file pattern.hpp
 * @brief Pattern definitions for the pattern matcher
 *
 * Patterns are templates that can be matched against atoms in the AtomSpace.
 * They support:
 * - Variable binding
 * - Type constraints
 * - Nested patterns
 * - Logical connectives (And, Or, Not)
 */

#include <opencog/core/types.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace opencog {

// Forward declarations
class AtomSpace;

// ============================================================================
// Variable Bindings
// ============================================================================

/**
 * @brief A variable that can be bound during pattern matching
 */
struct Variable {
    std::string name;
    std::optional<AtomType> type_constraint;  // Optional type restriction

    explicit Variable(std::string n, std::optional<AtomType> tc = std::nullopt)
        : name(std::move(n)), type_constraint(tc) {}
};

/**
 * @brief Map of variable names to their bound values
 */
using BindingMap = std::unordered_map<std::string, AtomId>;

/**
 * @brief A complete set of bindings for a pattern match
 */
struct BindingSet {
    BindingMap bindings;

    [[nodiscard]] bool contains(const std::string& var) const {
        return bindings.contains(var);
    }

    [[nodiscard]] AtomId get(const std::string& var) const {
        auto it = bindings.find(var);
        return it != bindings.end() ? it->second : ATOM_NULL;
    }

    void bind(const std::string& var, AtomId atom) {
        bindings[var] = atom;
    }

    [[nodiscard]] bool empty() const { return bindings.empty(); }
    [[nodiscard]] size_t size() const { return bindings.size(); }
};

// ============================================================================
// Pattern Term Types
// ============================================================================

/**
 * @brief A concrete atom reference in a pattern
 */
struct GroundedTerm {
    AtomId atom;

    explicit GroundedTerm(AtomId a) : atom(a) {}
};

/**
 * @brief A variable term in a pattern
 */
struct VariableTerm {
    std::string name;
    std::optional<AtomType> type_constraint;

    explicit VariableTerm(std::string n, std::optional<AtomType> tc = std::nullopt)
        : name(std::move(n)), type_constraint(tc) {}
};

/**
 * @brief A type-only constraint (matches any atom of this type)
 */
struct TypedTerm {
    AtomType type;

    explicit TypedTerm(AtomType t) : type(t) {}
};

/**
 * @brief Glob pattern - matches zero or more atoms
 */
struct GlobTerm {
    std::string name;
    size_t min_count = 0;
    size_t max_count = SIZE_MAX;

    explicit GlobTerm(std::string n, size_t min = 0, size_t max = SIZE_MAX)
        : name(std::move(n)), min_count(min), max_count(max) {}
};

// Forward declaration for recursive structure
struct LinkPattern;

/**
 * @brief A term in a pattern can be any of these types
 */
using PatternTerm = std::variant<
    GroundedTerm,
    VariableTerm,
    TypedTerm,
    GlobTerm,
    std::shared_ptr<LinkPattern>
>;

// ============================================================================
// Link Pattern
// ============================================================================

/**
 * @brief A pattern for matching links
 */
struct LinkPattern {
    AtomType type;                     // Link type to match
    std::vector<PatternTerm> outgoing; // Pattern for outgoing set
    bool ordered = true;               // If false, outgoing order doesn't matter

    LinkPattern(AtomType t, std::vector<PatternTerm> out, bool ord = true)
        : type(t), outgoing(std::move(out)), ordered(ord) {}
};

// ============================================================================
// Logical Connectives
// ============================================================================

struct AndPattern;
struct OrPattern;
struct NotPattern;

using LogicalPattern = std::variant<
    std::shared_ptr<AndPattern>,
    std::shared_ptr<OrPattern>,
    std::shared_ptr<NotPattern>
>;

/**
 * @brief Conjunction - all sub-patterns must match
 */
struct AndPattern {
    std::vector<PatternTerm> terms;

    explicit AndPattern(std::vector<PatternTerm> t) : terms(std::move(t)) {}
};

/**
 * @brief Disjunction - at least one sub-pattern must match
 */
struct OrPattern {
    std::vector<PatternTerm> terms;

    explicit OrPattern(std::vector<PatternTerm> t) : terms(std::move(t)) {}
};

/**
 * @brief Negation - sub-pattern must NOT match
 */
struct NotPattern {
    PatternTerm term;

    explicit NotPattern(PatternTerm t) : term(std::move(t)) {}
};

// ============================================================================
// Full Pattern Specification
// ============================================================================

/**
 * @brief Complete pattern specification for matching
 */
struct Pattern {
    std::vector<std::string> variables;     // Declared variables
    PatternTerm body;                       // The pattern to match
    std::optional<PatternTerm> clause;      // Optional additional constraint

    // Builder methods
    Pattern& with_variable(std::string name, std::optional<AtomType> type = std::nullopt) {
        variables.push_back(name);
        return *this;
    }

    Pattern& with_body(PatternTerm b) {
        body = std::move(b);
        return *this;
    }

    Pattern& with_clause(PatternTerm c) {
        clause = std::move(c);
        return *this;
    }
};

// ============================================================================
// Pattern Builder DSL
// ============================================================================

/**
 * @brief Fluent builder for patterns
 *
 * Usage:
 *   auto pattern = PatternBuilder()
 *       .variable("X", CONCEPT_NODE)
 *       .variable("Y", CONCEPT_NODE)
 *       .match(INHERITANCE_LINK, {var("X"), var("Y")})
 *       .build();
 */
class PatternBuilder {
public:
    PatternBuilder& variable(std::string name, std::optional<AtomType> type = std::nullopt) {
        pattern_.variables.push_back(name);
        var_types_[name] = type;
        return *this;
    }

    PatternBuilder& match(AtomType type, std::vector<PatternTerm> outgoing) {
        pattern_.body = std::make_shared<LinkPattern>(type, std::move(outgoing));
        return *this;
    }

    PatternBuilder& where(PatternTerm clause) {
        pattern_.clause = std::move(clause);
        return *this;
    }

    [[nodiscard]] Pattern build() const { return pattern_; }

    // Helper to create a variable term
    [[nodiscard]] VariableTerm var(const std::string& name) const {
        auto it = var_types_.find(name);
        return VariableTerm{name, it != var_types_.end() ? it->second : std::nullopt};
    }

    // Helper to create a grounded term
    [[nodiscard]] static GroundedTerm ground(AtomId id) {
        return GroundedTerm{id};
    }

    // Helper to create a typed term
    [[nodiscard]] static TypedTerm typed(AtomType type) {
        return TypedTerm{type};
    }

    // Helper to create a glob term
    [[nodiscard]] static GlobTerm glob(std::string name, size_t min = 0, size_t max = SIZE_MAX) {
        return GlobTerm{std::move(name), min, max};
    }

private:
    Pattern pattern_;
    std::unordered_map<std::string, std::optional<AtomType>> var_types_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Create a variable term
 */
[[nodiscard]] inline VariableTerm var(std::string name, std::optional<AtomType> type = std::nullopt) {
    return VariableTerm{std::move(name), type};
}

/**
 * @brief Create a grounded term
 */
[[nodiscard]] inline GroundedTerm ground(AtomId id) {
    return GroundedTerm{id};
}

/**
 * @brief Create a typed term
 */
[[nodiscard]] inline TypedTerm typed(AtomType type) {
    return TypedTerm{type};
}

/**
 * @brief Create a link pattern
 */
[[nodiscard]] inline std::shared_ptr<LinkPattern> link(
    AtomType type,
    std::vector<PatternTerm> outgoing,
    bool ordered = true
) {
    return std::make_shared<LinkPattern>(type, std::move(outgoing), ordered);
}

/**
 * @brief Create an And pattern
 */
[[nodiscard]] inline std::shared_ptr<AndPattern> and_pattern(std::vector<PatternTerm> terms) {
    return std::make_shared<AndPattern>(std::move(terms));
}

/**
 * @brief Create an Or pattern
 */
[[nodiscard]] inline std::shared_ptr<OrPattern> or_pattern(std::vector<PatternTerm> terms) {
    return std::make_shared<OrPattern>(std::move(terms));
}

/**
 * @brief Create a Not pattern
 */
[[nodiscard]] inline std::shared_ptr<NotPattern> not_pattern(PatternTerm term) {
    return std::make_shared<NotPattern>(std::move(term));
}

} // namespace opencog
