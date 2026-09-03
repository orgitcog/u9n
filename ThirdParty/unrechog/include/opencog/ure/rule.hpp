#pragma once
/**
 * @file rule.hpp
 * @brief Unified Rule Engine rule definitions
 *
 * The URE is a general-purpose rule engine that can run:
 * - PLN inference rules
 * - Rewrite rules
 * - Custom transformation rules
 *
 * Rules are themselves atoms (DefineLinks) in the AtomSpace.
 */

#include <opencog/core/types.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/pattern/pattern.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace opencog::ure {

// ============================================================================
// Rule Definition
// ============================================================================

/**
 * @brief A URE rule
 *
 * A rule consists of:
 * - Name
 * - Variable declarations
 * - Premise pattern (what the rule matches)
 * - Conclusion template (what the rule produces)
 * - Truth value formula (how to compute output TV)
 */
struct Rule {
    std::string name;
    std::vector<std::string> variables;
    Handle premise;           // BindLink-style pattern
    Handle conclusion;        // Rewrite template
    Handle tv_formula;        // Optional: formula for computing TV

    // Metadata
    TruthValue tv{1.0f, 1.0f};  // Rule's own truth value (strength = applicability)
    float complexity{1.0f};     // Computational cost estimate
    float priority{1.0f};       // User-assigned priority

    [[nodiscard]] bool valid() const {
        return !name.empty() && premise.valid() && conclusion.valid();
    }
};

// ============================================================================
// Rule Base
// ============================================================================

/**
 * @brief Collection of rules for the URE
 *
 * Rules can be loaded from:
 * - The AtomSpace (as DefineLinks)
 * - External files
 * - Programmatically added
 */
class RuleBase {
public:
    explicit RuleBase(AtomSpace& space);

    // ========================================================================
    // Rule Management
    // ========================================================================

    /**
     * @brief Add a rule to the base
     */
    void add_rule(Rule rule);

    /**
     * @brief Add a rule from an atom (DefineLink)
     */
    void add_rule_from_atom(Handle rule_atom);

    /**
     * @brief Load all rules from the AtomSpace
     *
     * Scans for DefineLinks that define rules.
     */
    void load_from_atomspace();

    /**
     * @brief Remove a rule by name
     */
    bool remove_rule(const std::string& name);

    /**
     * @brief Clear all rules
     */
    void clear();

    // ========================================================================
    // Rule Lookup
    // ========================================================================

    [[nodiscard]] std::optional<Rule> get_rule(const std::string& name) const;
    [[nodiscard]] std::vector<Rule> get_all_rules() const;
    [[nodiscard]] size_t size() const { return rules_.size(); }

    /**
     * @brief Get rules applicable to a given atom type
     */
    [[nodiscard]] std::vector<const Rule*> get_rules_for_type(AtomType type) const;

    /**
     * @brief Get rules sorted by priority
     */
    [[nodiscard]] std::vector<const Rule*> get_rules_by_priority() const;

    // ========================================================================
    // Rule Analysis
    // ========================================================================

    /**
     * @brief Get variable declarations from a rule
     */
    [[nodiscard]] std::vector<std::string> get_variables(const Rule& rule) const;

    /**
     * @brief Check if a rule could potentially apply to an atom
     */
    [[nodiscard]] bool could_apply(const Rule& rule, Handle target) const;

private:
    AtomSpace& space_;
    std::vector<Rule> rules_;

    // Index for fast lookup
    std::unordered_map<std::string, size_t> name_index_;
    std::unordered_multimap<AtomType, size_t> type_index_;

    void rebuild_indices();
};

// ============================================================================
// Rule Application Result
// ============================================================================

struct RuleApplicationResult {
    const Rule* rule;
    BindingSet bindings;
    Handle result;
    TruthValue computed_tv;

    [[nodiscard]] bool valid() const { return rule && result.valid(); }
};

// ============================================================================
// Rule Applicator
// ============================================================================

/**
 * @brief Applies rules to atoms
 */
class RuleApplicator {
public:
    RuleApplicator(AtomSpace& space, const RuleBase& rules);

    /**
     * @brief Apply a specific rule with given bindings
     */
    [[nodiscard]] std::optional<RuleApplicationResult> apply(
        const Rule& rule,
        const BindingSet& bindings
    );

    /**
     * @brief Find all applicable rules for a target
     */
    [[nodiscard]] std::vector<RuleApplicationResult> find_applicable(
        Handle target,
        size_t max_results = SIZE_MAX
    );

    /**
     * @brief Apply all applicable rules to a target
     */
    [[nodiscard]] std::vector<RuleApplicationResult> apply_all(
        Handle target,
        size_t max_results = SIZE_MAX
    );

private:
    AtomSpace& space_;
    const RuleBase& rules_;
    PatternMatcher matcher_;

    [[nodiscard]] Handle instantiate_template(
        Handle templ,
        const BindingSet& bindings
    );

    [[nodiscard]] TruthValue compute_tv(
        const Rule& rule,
        const BindingSet& bindings
    );
};

} // namespace opencog::ure
