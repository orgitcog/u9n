/**
 * @file matcher.cpp
 * @brief Pattern matcher implementation
 */

#include <opencog/pattern/matcher.hpp>

#include <algorithm>
#include <vector>

namespace opencog {

PatternMatcher::PatternMatcher(const AtomSpace& space, MatcherConfig config)
    : space_(space), config_(std::move(config))
{
}

// ============================================================================
// Core Matching
// ============================================================================

generator<MatchResult> PatternMatcher::match(const Pattern& pattern) {
    BindingSet initial_bindings;
    size_t result_count = 0;

    for (auto& result : match_term(pattern.body, initial_bindings)) {
        // Apply additional clause if present
        if (pattern.clause) {
            auto clause_result = match_atom(*pattern.clause, result.matched_atom, result.bindings);
            if (!clause_result) continue;
        }

        co_yield result;

        if (++result_count >= config_.max_results) {
            co_return;
        }

        if (config_.progress_callback) {
            config_.progress_callback(result_count);
        }
    }
}

std::optional<MatchResult> PatternMatcher::match_atom(
    const PatternTerm& pattern,
    AtomId atom,
    BindingSet bindings
) {
    auto result = unify_term(pattern, atom, std::move(bindings));
    if (result) {
        return MatchResult{*result, atom, 1.0f};
    }
    return std::nullopt;
}

std::optional<MatchResult> PatternMatcher::find_first(const Pattern& pattern) {
    for (auto& result : match(pattern)) {
        return result;
    }
    return std::nullopt;
}

std::vector<MatchResult> PatternMatcher::find_all(const Pattern& pattern, size_t limit) {
    std::vector<MatchResult> results;
    for (auto& result : match(pattern)) {
        results.push_back(std::move(result));
        if (results.size() >= limit) break;
    }
    return results;
}

size_t PatternMatcher::count_matches(const Pattern& pattern) {
    size_t count = 0;
    for ([[maybe_unused]] auto& _ : match(pattern)) {
        ++count;
    }
    return count;
}

bool PatternMatcher::any_match(const Pattern& pattern) {
    for ([[maybe_unused]] auto& _ : match(pattern)) {
        return true;
    }
    return false;
}

// ============================================================================
// Specialized Queries
// ============================================================================

generator<AtomId> PatternMatcher::filter(
    std::function<bool(const AtomSpace&, Handle)> predicate
) {
    // for_each_atom takes a plain callback, so matches are collected first
    // and yielded afterwards (co_yield cannot appear inside the lambda).
    std::vector<AtomId> matches;
    space_.for_each_atom([&](Handle h) {
        if (!predicate || predicate(space_, h)) {
            matches.push_back(h.id());
        }
    });

    for (AtomId id : matches) {
        co_yield id;
    }
}

generator<AtomId> PatternMatcher::filter_by_type(
    AtomType type,
    std::function<bool(const AtomSpace&, Handle)> predicate
) {
    for (Handle h : space_.get_atoms_by_type(type)) {
        if (!predicate || predicate(space_, h)) {
            co_yield h.id();
        }
    }
}

// ============================================================================
// Internal Matching
// ============================================================================

generator<MatchResult> PatternMatcher::match_term(
    const PatternTerm& term,
    BindingSet bindings
) {
    if (auto* grounded = std::get_if<GroundedTerm>(&term)) {
        for (auto& r : match_grounded(*grounded, bindings)) {
            co_yield r;
        }
    }
    else if (auto* variable = std::get_if<VariableTerm>(&term)) {
        for (auto& r : match_variable(*variable, bindings)) {
            co_yield r;
        }
    }
    else if (auto* typed = std::get_if<TypedTerm>(&term)) {
        for (auto& r : match_typed(*typed, bindings)) {
            co_yield r;
        }
    }
    else if (auto* link_ptr = std::get_if<std::shared_ptr<LinkPattern>>(&term)) {
        if (*link_ptr) {
            for (auto& r : match_link_pattern(**link_ptr, bindings)) {
                co_yield r;
            }
        }
    }
    else if (auto* glob = std::get_if<GlobTerm>(&term)) {
        // A top-level glob matches a single atom, so it only applies when
        // its count range admits exactly one element.
        if (glob->min_count <= 1 && 1 <= glob->max_count) {
            std::vector<AtomId> ids;
            space_.for_each_atom([&](Handle h) { ids.push_back(h.id()); });
            for (AtomId id : ids) {
                BindingSet new_bindings = bindings;
                new_bindings.bind(glob->name, id);
                co_yield MatchResult{new_bindings, id, 1.0f};
            }
        }
    }
}

generator<MatchResult> PatternMatcher::match_grounded(
    const GroundedTerm& term,
    BindingSet bindings
) {
    if (space_.contains(term.atom)) {
        co_yield MatchResult{bindings, term.atom, 1.0f};
    }
}

generator<MatchResult> PatternMatcher::match_variable(
    const VariableTerm& term,
    BindingSet bindings
) {
    // Check if already bound
    if (bindings.contains(term.name)) {
        AtomId bound_id = bindings.get(term.name);
        if (space_.contains(bound_id)) {
            // Check type constraint
            if (!term.type_constraint ||
                type_matches(*term.type_constraint, space_.atom_table().get_type(bound_id))) {
                co_yield MatchResult{bindings, bound_id, 1.0f};
            }
        }
        co_return;
    }

    // Find all matching atoms
    if (term.type_constraint) {
        // Search atoms of specific type
        for (Handle h : space_.get_atoms_by_type(*term.type_constraint)) {
            BindingSet new_bindings = bindings;
            new_bindings.bind(term.name, h.id());
            co_yield MatchResult{new_bindings, h.id(), 1.0f};
        }
    } else {
        // Search all atoms (expensive!) — collect first, since co_yield
        // cannot be used inside the for_each_atom callback.
        std::vector<AtomId> ids;
        space_.for_each_atom([&](Handle h) { ids.push_back(h.id()); });
        for (AtomId id : ids) {
            BindingSet new_bindings = bindings;
            new_bindings.bind(term.name, id);
            co_yield MatchResult{new_bindings, id, 1.0f};
        }
    }
}

generator<MatchResult> PatternMatcher::match_typed(
    const TypedTerm& term,
    BindingSet bindings
) {
    for (Handle h : space_.get_atoms_by_type(term.type)) {
        co_yield MatchResult{bindings, h.id(), 1.0f};
    }
}

generator<MatchResult> PatternMatcher::match_link_pattern(
    const LinkPattern& pattern,
    BindingSet bindings
) {
    // Get all links of this type
    for (Handle link : space_.get_atoms_by_type(pattern.type)) {
        auto outgoing = space_.atom_table().get_outgoing(link.id());

        // Try to unify outgoing sets
        auto result = unify_outgoing(pattern.outgoing, outgoing, bindings);
        if (result) {
            co_yield MatchResult{*result, link.id(), 1.0f};
        }
    }
}

generator<MatchResult> PatternMatcher::match_and(
    const AndPattern& pattern,
    BindingSet bindings
) {
    if (pattern.terms.empty()) {
        co_yield MatchResult{bindings, ATOM_NULL, 1.0f};
        co_return;
    }

    // Match first term, then recursively match rest
    for (auto& first_result : match_term(pattern.terms[0], bindings)) {
        if (pattern.terms.size() == 1) {
            co_yield first_result;
        } else {
            // Create And pattern with remaining terms
            AndPattern rest{std::vector<PatternTerm>(
                pattern.terms.begin() + 1, pattern.terms.end())};
            for (auto& rest_result : match_and(rest, first_result.bindings)) {
                co_yield rest_result;
            }
        }
    }
}

generator<MatchResult> PatternMatcher::match_or(
    const OrPattern& pattern,
    BindingSet bindings
) {
    for (const auto& term : pattern.terms) {
        for (auto& result : match_term(term, bindings)) {
            co_yield result;
        }
    }
}

bool PatternMatcher::match_not(
    const NotPattern& pattern,
    const BindingSet& bindings
) {
    // Not matches if the sub-pattern does NOT match
    for ([[maybe_unused]] auto& _ : match_term(pattern.term, bindings)) {
        return false;  // Found a match, so NOT fails
    }
    return true;  // No match found, so NOT succeeds
}

// ============================================================================
// Unification
// ============================================================================

std::optional<BindingSet> PatternMatcher::unify_outgoing(
    std::span<const PatternTerm> pattern_outgoing,
    std::span<const AtomId> atom_outgoing,
    BindingSet bindings
) {
    const bool has_glob = std::any_of(
        pattern_outgoing.begin(), pattern_outgoing.end(),
        [](const PatternTerm& t) { return std::holds_alternative<GlobTerm>(t); });

    if (has_glob) {
        return unify_outgoing_glob(pattern_outgoing, atom_outgoing, 0, 0, std::move(bindings));
    }

    if (pattern_outgoing.size() != atom_outgoing.size()) {
        return std::nullopt;
    }

    for (size_t i = 0; i < pattern_outgoing.size(); ++i) {
        auto result = unify_term(pattern_outgoing[i], atom_outgoing[i], bindings);
        if (!result) {
            return std::nullopt;
        }
        bindings = *result;
    }

    return bindings;
}

std::optional<BindingSet> PatternMatcher::unify_outgoing_glob(
    std::span<const PatternTerm> pattern_outgoing,
    std::span<const AtomId> atom_outgoing,
    size_t pattern_index,
    size_t atom_index,
    BindingSet bindings
) {
    if (pattern_index == pattern_outgoing.size()) {
        if (atom_index == atom_outgoing.size()) {
            return bindings;
        }
        return std::nullopt;
    }

    const PatternTerm& term = pattern_outgoing[pattern_index];

    if (auto* glob = std::get_if<GlobTerm>(&term)) {
        const size_t remaining = atom_outgoing.size() - atom_index;
        const size_t max_take = std::min(glob->max_count, remaining);
        for (size_t take = glob->min_count; take <= max_take; ++take) {
            BindingSet branch = bindings;
            // BindingSet maps a name to a single AtomId, so record the
            // binding only for single-atom captures.
            if (take == 1) {
                branch.bind(glob->name, atom_outgoing[atom_index]);
            }
            auto result = unify_outgoing_glob(
                pattern_outgoing, atom_outgoing,
                pattern_index + 1, atom_index + take, std::move(branch));
            if (result) {
                return result;
            }
        }
        return std::nullopt;
    }

    if (atom_index >= atom_outgoing.size()) {
        return std::nullopt;
    }

    auto result = unify_term(term, atom_outgoing[atom_index], std::move(bindings));
    if (!result) {
        return std::nullopt;
    }
    return unify_outgoing_glob(
        pattern_outgoing, atom_outgoing,
        pattern_index + 1, atom_index + 1, std::move(*result));
}

std::optional<BindingSet> PatternMatcher::unify_term(
    const PatternTerm& term,
    AtomId atom,
    BindingSet bindings
) {
    if (auto* grounded = std::get_if<GroundedTerm>(&term)) {
        if (grounded->atom == atom) {
            return bindings;
        }
        return std::nullopt;
    }

    if (auto* variable = std::get_if<VariableTerm>(&term)) {
        // Check type constraint
        if (variable->type_constraint) {
            AtomType actual = space_.atom_table().get_type(atom);
            if (!type_matches(*variable->type_constraint, actual)) {
                return std::nullopt;
            }
        }

        // Check existing binding
        if (bindings.contains(variable->name)) {
            if (bindings.get(variable->name) == atom) {
                return bindings;
            }
            return std::nullopt;
        }

        // Create new binding
        bindings.bind(variable->name, atom);
        return bindings;
    }

    if (auto* typed = std::get_if<TypedTerm>(&term)) {
        AtomType actual = space_.atom_table().get_type(atom);
        if (type_matches(typed->type, actual)) {
            return bindings;
        }
        return std::nullopt;
    }

    if (auto* link_ptr = std::get_if<std::shared_ptr<LinkPattern>>(&term)) {
        if (!*link_ptr) return std::nullopt;

        const auto& pattern = **link_ptr;
        AtomType actual = space_.atom_table().get_type(atom);

        if (!type_matches(pattern.type, actual)) {
            return std::nullopt;
        }

        auto outgoing = space_.atom_table().get_outgoing(atom);
        return unify_outgoing(pattern.outgoing, outgoing, bindings);
    }

    if (auto* glob = std::get_if<GlobTerm>(&term)) {
        // Unifying against exactly one atom.
        if (glob->min_count <= 1 && 1 <= glob->max_count) {
            if (bindings.contains(glob->name)) {
                if (bindings.get(glob->name) == atom) {
                    return bindings;
                }
                return std::nullopt;
            }
            bindings.bind(glob->name, atom);
            return bindings;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

bool PatternMatcher::type_matches(AtomType pattern_type, AtomType atom_type) const {
    if (!config_.check_type_hierarchy) {
        return pattern_type == atom_type;
    }

    // Direct match
    if (pattern_type == atom_type) return true;

    // Type hierarchy: NODE matches any node type
    if (pattern_type == AtomType::NODE && is_node(atom_type)) return true;

    // Type hierarchy: LINK matches any link type
    if (pattern_type == AtomType::LINK && is_link(atom_type)) return true;

    return false;
}

} // namespace opencog
