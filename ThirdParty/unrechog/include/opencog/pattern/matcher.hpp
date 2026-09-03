#pragma once
/**
 * @file matcher.hpp
 * @brief Coroutine-based pattern matcher
 *
 * Uses C++20 coroutines for lazy evaluation of pattern matches.
 * Only computes as many matches as requested.
 */

#include <opencog/pattern/pattern.hpp>
#include <opencog/pattern/generator.hpp>
#include <opencog/atomspace/atomspace.hpp>

#include <functional>
#include <optional>

namespace opencog {

// ============================================================================
// Match Result
// ============================================================================

/**
 * @brief Result of a pattern match attempt
 */
struct MatchResult {
    BindingSet bindings;           // Variable bindings
    AtomId matched_atom;           // The atom that matched
    float confidence{1.0f};        // Match confidence (for fuzzy matching)

    [[nodiscard]] bool valid() const { return matched_atom.valid(); }
};

// ============================================================================
// Pattern Matcher Configuration
// ============================================================================

struct MatcherConfig {
    bool allow_partial_matches = false;   // Return partial bindings
    bool check_type_hierarchy = true;     // Consider type inheritance
    size_t max_results = SIZE_MAX;        // Limit number of results
    float min_confidence = 0.0f;          // Minimum confidence threshold

    // Callback for match progress (optional)
    std::function<void(size_t matches_found)> progress_callback;
};

// ============================================================================
// Pattern Matcher
// ============================================================================

/**
 * @brief Coroutine-based lazy pattern matcher
 *
 * Matches patterns against atoms in an AtomSpace, yielding results
 * one at a time using coroutines for efficiency.
 */
class PatternMatcher {
public:
    explicit PatternMatcher(const AtomSpace& space, MatcherConfig config = {});

    // ========================================================================
    // Core Matching Operations
    // ========================================================================

    /**
     * @brief Execute a pattern and yield all matching bindings
     *
     * Usage:
     *   for (auto& result : matcher.match(pattern)) {
     *       process(result);
     *   }
     */
    [[nodiscard]] generator<MatchResult> match(const Pattern& pattern);

    /**
     * @brief Execute a pattern on a specific atom
     */
    [[nodiscard]] std::optional<MatchResult> match_atom(
        const PatternTerm& pattern,
        AtomId atom,
        BindingSet bindings = {}
    );

    /**
     * @brief Find first match (or nullopt if none)
     */
    [[nodiscard]] std::optional<MatchResult> find_first(const Pattern& pattern);

    /**
     * @brief Find all matches (eager evaluation)
     */
    [[nodiscard]] std::vector<MatchResult> find_all(
        const Pattern& pattern,
        size_t limit = SIZE_MAX
    );

    /**
     * @brief Count matches without collecting them
     */
    [[nodiscard]] size_t count_matches(const Pattern& pattern);

    /**
     * @brief Check if any match exists
     */
    [[nodiscard]] bool any_match(const Pattern& pattern);

    // ========================================================================
    // Specialized Queries
    // ========================================================================

    /**
     * @brief Find all atoms satisfying a predicate
     *
     * Usage:
     *   for (auto& id : matcher.filter([](auto& as, auto h) {
     *       return as.get_tv(h).strength > 0.5;
     *   })) { ... }
     */
    [[nodiscard]] generator<AtomId> filter(
        std::function<bool(const AtomSpace&, Handle)> predicate
    );

    /**
     * @brief Find atoms by type with additional filter
     */
    [[nodiscard]] generator<AtomId> filter_by_type(
        AtomType type,
        std::function<bool(const AtomSpace&, Handle)> predicate = nullptr
    );

    // ========================================================================
    // Bind Link (GetLink) Query
    // ========================================================================

    /**
     * @brief Execute a BindLink-style query
     *
     * A BindLink has:
     * - Variable declarations
     * - A pattern to match
     * - A rewrite template
     *
     * Returns the instantiated rewrites for each match.
     */
    [[nodiscard]] generator<Handle> bind(
        const std::vector<std::string>& variables,
        const PatternTerm& pattern,
        const PatternTerm& rewrite
    );

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(MatcherConfig config) { config_ = std::move(config); }
    [[nodiscard]] const MatcherConfig& config() const { return config_; }

private:
    const AtomSpace& space_;
    MatcherConfig config_;

    // Internal matching implementations
    [[nodiscard]] generator<MatchResult> match_term(
        const PatternTerm& term,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_grounded(
        const GroundedTerm& term,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_variable(
        const VariableTerm& term,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_typed(
        const TypedTerm& term,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_link_pattern(
        const LinkPattern& pattern,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_and(
        const AndPattern& pattern,
        BindingSet bindings
    );

    [[nodiscard]] generator<MatchResult> match_or(
        const OrPattern& pattern,
        BindingSet bindings
    );

    [[nodiscard]] bool match_not(
        const NotPattern& pattern,
        const BindingSet& bindings
    );

    // Unification
    [[nodiscard]] std::optional<BindingSet> unify_outgoing(
        std::span<const PatternTerm> pattern_outgoing,
        std::span<const AtomId> atom_outgoing,
        BindingSet bindings
    );

    [[nodiscard]] std::optional<BindingSet> unify_term(
        const PatternTerm& term,
        AtomId atom,
        BindingSet bindings
    );

    // Type checking
    [[nodiscard]] bool type_matches(AtomType pattern_type, AtomType atom_type) const;
};

// ============================================================================
// Query DSL
// ============================================================================

/**
 * @brief Fluent query builder
 *
 * Usage:
 *   auto results = Query(space)
 *       .match(INHERITANCE_LINK, {var("X"), ground(animal)})
 *       .where([](auto& as, auto h) { return as.get_tv(h).strength > 0.5; })
 *       .limit(10)
 *       .execute();
 */
class Query {
public:
    explicit Query(const AtomSpace& space) : matcher_(space) {}

    Query& variable(std::string name, std::optional<AtomType> type = std::nullopt) {
        builder_.variable(std::move(name), type);
        return *this;
    }

    Query& match(AtomType type, std::vector<PatternTerm> outgoing) {
        builder_.match(type, std::move(outgoing));
        return *this;
    }

    Query& where(std::function<bool(const AtomSpace&, Handle)> predicate) {
        predicate_ = std::move(predicate);
        return *this;
    }

    Query& limit(size_t n) {
        config_.max_results = n;
        return *this;
    }

    Query& min_confidence(float c) {
        config_.min_confidence = c;
        return *this;
    }

    [[nodiscard]] generator<MatchResult> execute() {
        matcher_.set_config(config_);
        return matcher_.match(builder_.build());
    }

    [[nodiscard]] std::vector<MatchResult> collect() {
        return matcher_.find_all(builder_.build(), config_.max_results);
    }

    [[nodiscard]] std::optional<MatchResult> first() {
        return matcher_.find_first(builder_.build());
    }

    [[nodiscard]] size_t count() {
        return matcher_.count_matches(builder_.build());
    }

    [[nodiscard]] bool exists() {
        return matcher_.any_match(builder_.build());
    }

private:
    PatternMatcher matcher_;
    PatternBuilder builder_;
    MatcherConfig config_;
    std::function<bool(const AtomSpace&, Handle)> predicate_;
};

} // namespace opencog
