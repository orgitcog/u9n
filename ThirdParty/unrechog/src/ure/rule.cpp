/**
 * @file rule.cpp
 * @brief URE rule implementation
 */

#include <opencog/ure/rule.hpp>

#include <algorithm>

namespace opencog::ure {

// ============================================================================
// RuleBase Implementation
// ============================================================================

RuleBase::RuleBase(AtomSpace& space)
    : space_(space)
{
}

void RuleBase::add_rule(Rule rule) {
    size_t index = rules_.size();
    name_index_[rule.name] = index;

    // Index by premise type (for fast lookup)
    if (rule.premise.valid()) {
        AtomType type = space_.get_type(rule.premise);
        type_index_.emplace(type, index);
    }

    rules_.push_back(std::move(rule));
}

void RuleBase::add_rule_from_atom(Handle rule_atom) {
    if (!rule_atom.valid()) return;

    // Parse DefineLink structure to extract rule
    // Format: (DefineLink (DefinedSchemaNode "rule-name") (BindLink ...))

    AtomType type = space_.get_type(rule_atom);
    if (type != AtomType::DEFINE_LINK) return;

    auto outgoing = space_.get_outgoing(rule_atom);
    if (outgoing.size() != 2) return;

    // Extract rule name
    Handle name_handle = outgoing[0];
    if (space_.get_type(name_handle) != AtomType::DEFINED_SCHEMA_NODE) return;

    Rule rule;
    rule.name = std::string(space_.get_name(name_handle));

    // Extract BindLink
    Handle bind_link = outgoing[1];
    if (space_.get_type(bind_link) != AtomType::BIND_LINK) return;

    auto bind_outgoing = space_.get_outgoing(bind_link);
    if (bind_outgoing.size() >= 2) {
        rule.premise = bind_outgoing[0];      // Pattern
        rule.conclusion = bind_outgoing.back();  // Rewrite
    }

    add_rule(std::move(rule));
}

void RuleBase::load_from_atomspace() {
    // Find all DefineLinks and try to parse them as rules
    for (Handle h : space_.get_atoms_by_type(AtomType::DEFINE_LINK)) {
        add_rule_from_atom(h);
    }
}

bool RuleBase::remove_rule(const std::string& name) {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) return false;

    size_t index = it->second;

    // Remove from indices
    name_index_.erase(it);

    // Note: We don't actually remove from rules_ to avoid invalidating indices
    // Instead, we could mark as deleted or use a more sophisticated data structure
    rules_[index] = Rule{};  // Clear the rule

    rebuild_indices();
    return true;
}

void RuleBase::clear() {
    rules_.clear();
    name_index_.clear();
    type_index_.clear();
}

std::optional<Rule> RuleBase::get_rule(const std::string& name) const {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) return std::nullopt;
    return rules_[it->second];
}

std::vector<Rule> RuleBase::get_all_rules() const {
    std::vector<Rule> result;
    for (const auto& rule : rules_) {
        if (!rule.name.empty()) {
            result.push_back(rule);
        }
    }
    return result;
}

std::vector<const Rule*> RuleBase::get_rules_for_type(AtomType type) const {
    std::vector<const Rule*> result;
    auto range = type_index_.equal_range(type);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(&rules_[it->second]);
    }
    return result;
}

std::vector<const Rule*> RuleBase::get_rules_by_priority() const {
    std::vector<const Rule*> result;
    for (const auto& rule : rules_) {
        if (!rule.name.empty()) {
            result.push_back(&rule);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const Rule* a, const Rule* b) { return a->priority > b->priority; });

    return result;
}

std::vector<std::string> RuleBase::get_variables(const Rule& rule) const {
    return rule.variables;
}

bool RuleBase::could_apply(const Rule& rule, Handle target) const {
    if (!rule.valid() || !target.valid()) return false;

    // Quick check: does premise type match target type?
    AtomType premise_type = space_.get_type(rule.premise);
    AtomType target_type = space_.get_type(target);

    // Exact match or type hierarchy match
    if (premise_type == target_type) return true;
    if (premise_type == AtomType::LINK && is_link(target_type)) return true;
    if (premise_type == AtomType::NODE && is_node(target_type)) return true;

    return false;
}

void RuleBase::rebuild_indices() {
    name_index_.clear();
    type_index_.clear();

    for (size_t i = 0; i < rules_.size(); ++i) {
        const auto& rule = rules_[i];
        if (rule.name.empty()) continue;

        name_index_[rule.name] = i;

        if (rule.premise.valid()) {
            AtomType type = space_.get_type(rule.premise);
            type_index_.emplace(type, i);
        }
    }
}

// ============================================================================
// RuleApplicator Implementation
// ============================================================================

RuleApplicator::RuleApplicator(AtomSpace& space, const RuleBase& rules)
    : space_(space), rules_(rules), matcher_(space)
{
}

std::optional<RuleApplicationResult> RuleApplicator::apply(
    const Rule& rule,
    const BindingSet& bindings
) {
    if (!rule.valid()) return std::nullopt;

    // Instantiate conclusion template with bindings
    Handle result = instantiate_template(rule.conclusion, bindings);
    if (!result.valid()) return std::nullopt;

    // Compute truth value
    TruthValue tv = compute_tv(rule, bindings);

    return RuleApplicationResult{&rule, bindings, result, tv};
}

std::vector<RuleApplicationResult> RuleApplicator::find_applicable(
    Handle target,
    size_t max_results
) {
    std::vector<RuleApplicationResult> results;

    for (const Rule* rule : rules_.get_rules_by_priority()) {
        if (!rules_.could_apply(*rule, target)) continue;

        // Try to match rule premise against target
        // Full implementation would use pattern matcher

        if (results.size() >= max_results) break;
    }

    return results;
}

std::vector<RuleApplicationResult> RuleApplicator::apply_all(
    Handle target,
    size_t max_results
) {
    std::vector<RuleApplicationResult> results;

    for (auto& applicable : find_applicable(target, max_results)) {
        auto result = apply(*applicable.rule, applicable.bindings);
        if (result) {
            results.push_back(std::move(*result));
        }
    }

    return results;
}

Handle RuleApplicator::instantiate_template(
    Handle templ,
    const BindingSet& bindings
) {
    if (!templ.valid()) return Handle{};

    AtomType type = space_.get_type(templ);

    // If it's a VariableNode, look up binding
    if (type == AtomType::VARIABLE_NODE) {
        std::string var_name(space_.get_name(templ));
        if (bindings.contains(var_name)) {
            return space_.make_handle(bindings.get(var_name));
        }
        return Handle{};  // Unbound variable
    }

    // If it's a node, return as-is
    if (is_node(type)) {
        return templ;
    }

    // If it's a link, recursively instantiate
    auto outgoing = space_.get_outgoing(templ);
    std::vector<Handle> new_outgoing;
    new_outgoing.reserve(outgoing.size());

    for (const Handle& h : outgoing) {
        Handle instantiated = instantiate_template(h, bindings);
        if (!instantiated.valid()) return Handle{};
        new_outgoing.push_back(instantiated);
    }

    // Create or get the instantiated link
    return space_.add_link(type, new_outgoing);
}

TruthValue RuleApplicator::compute_tv(
    const Rule& rule,
    const BindingSet& bindings
) {
    // Use rule's TV formula if provided, otherwise default
    if (rule.tv_formula.valid()) {
        // Execute formula (would need Scheme/grounded schema support)
    }

    // Default: product of premise TVs with rule TV weight
    TruthValue result = TruthValue::default_tv();

    // Scale by rule's own confidence
    result.confidence *= rule.tv.confidence;

    return result;
}

} // namespace opencog::ure
