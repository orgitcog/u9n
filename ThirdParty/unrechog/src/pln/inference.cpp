/**
 * @file inference.cpp
 * @brief PLN inference engine implementation
 */

#include <opencog/pln/inference.hpp>

#include <algorithm>

namespace opencog::pln {

// ============================================================================
// Built-in Rules
// ============================================================================

namespace rules {

InferenceRule make_deduction_rule() {
    InferenceRule rule;
    rule.name = "deduction";
    rule.priority = 1.0f;
    // Full implementation would set up patterns and formulas
    return rule;
}

InferenceRule make_inversion_rule() {
    InferenceRule rule;
    rule.name = "inversion";
    rule.priority = 0.8f;
    return rule;
}

InferenceRule make_modus_ponens_rule() {
    InferenceRule rule;
    rule.name = "modus-ponens";
    rule.priority = 1.0f;
    return rule;
}

InferenceRule make_abduction_rule() {
    InferenceRule rule;
    rule.name = "abduction";
    rule.priority = 0.6f;
    return rule;
}

InferenceRule make_and_rule() {
    InferenceRule rule;
    rule.name = "and-introduction";
    rule.priority = 0.9f;
    return rule;
}

InferenceRule make_or_rule() {
    InferenceRule rule;
    rule.name = "or-introduction";
    rule.priority = 0.9f;
    return rule;
}

InferenceRule make_not_rule() {
    InferenceRule rule;
    rule.name = "not-introduction";
    rule.priority = 0.9f;
    return rule;
}

std::vector<InferenceRule> get_standard_rules() {
    return {
        make_deduction_rule(),
        make_inversion_rule(),
        make_modus_ponens_rule(),
        make_abduction_rule(),
        make_and_rule(),
        make_or_rule(),
        make_not_rule()
    };
}

} // namespace rules

// ============================================================================
// PLNEngine Implementation
// ============================================================================

PLNEngine::PLNEngine(AtomSpace& space, InferenceConfig config)
    : space_(space)
    , config_(std::move(config))
    , matcher_(space_)
{
}

void PLNEngine::add_rule(InferenceRule rule) {
    rules_.push_back(std::move(rule));
}

void PLNEngine::add_rules(std::vector<InferenceRule> rules) {
    for (auto& rule : rules) {
        rules_.push_back(std::move(rule));
    }
}

void PLNEngine::clear_rules() {
    rules_.clear();
}

std::vector<InferenceResult> PLNEngine::forward_chain(Handle source) {
    std::vector<InferenceResult> results;

    if (!source.valid()) return results;

    size_t iterations = 0;
    std::vector<Handle> frontier{source};
    std::unordered_set<uint64_t> visited;

    while (!frontier.empty() && iterations < config_.max_iterations) {
        Handle current = frontier.back();
        frontier.pop_back();

        uint64_t hash = current.id().value;
        if (visited.contains(hash)) continue;
        visited.insert(hash);

        // Try each rule
        for (const auto& rule : rules_) {
            // Skip if rule doesn't meet confidence threshold
            // Full implementation would check rule applicability

            auto step_results = forward_step(current);
            for (auto& result : step_results) {
                results.push_back(std::move(result));

                // Check if target reached
                if (config_.target_reached && config_.target_reached(results.back().conclusion)) {
                    return results;
                }

                // Add conclusion to frontier
                if (results.size() < config_.max_results) {
                    frontier.push_back(results.back().conclusion);
                }
            }
        }

        ++iterations;
        ++total_inferences_;
    }

    return results;
}

std::vector<InferenceResult> PLNEngine::forward_chain(std::span<const Handle> sources) {
    std::vector<InferenceResult> all_results;

    for (Handle source : sources) {
        auto results = forward_chain(source);
        for (auto& r : results) {
            all_results.push_back(std::move(r));
        }
    }

    return all_results;
}

std::vector<InferenceResult> PLNEngine::forward_step(Handle source) {
    std::vector<InferenceResult> results;

    // For each rule, try to apply it
    for (const auto& rule : rules_) {
        // Pattern match to find applicable instances
        // This is a simplified implementation

        // For example, for deduction:
        // Find all A->B where A = source
        // Then find all B->C for those B
        // Compute A->C

        // Placeholder - would need full pattern matching
    }

    return results;
}

std::optional<InferenceResult> PLNEngine::backward_chain(Handle target) {
    if (!target.valid()) return std::nullopt;

    // For backward chaining:
    // 1. Check if target is already known (grounded)
    // 2. Find rules whose conclusion can unify with target
    // 3. Recursively prove premises

    // Check if grounded
    TruthValue tv = space_.get_tv(target);
    if (tv.confidence > config_.min_confidence) {
        InferenceResult result;
        result.conclusion = target;
        result.truth_value = tv;
        result.iterations_used = 0;
        return result;
    }

    // Try each rule
    for (const auto& rule : rules_) {
        // Check if rule conclusion can unify with target
        // If so, try to prove premises

        // Placeholder implementation
    }

    return std::nullopt;
}

std::vector<InferenceResult> PLNEngine::find_proofs(Handle target) {
    std::vector<InferenceResult> proofs;

    // Depth-first search for all proofs
    auto first_proof = backward_chain(target);
    if (first_proof) {
        proofs.push_back(std::move(*first_proof));
    }

    return proofs;
}

std::vector<InferenceResult> PLNEngine::apply_rule(
    const InferenceRule& rule,
    const BindingSet& bindings
) {
    std::vector<InferenceResult> results;

    // Get premises from bindings
    // Compute conclusion truth value using rule formula
    // Create conclusion atom if it doesn't exist

    // Placeholder
    return results;
}

bool PLNEngine::should_pursue(Handle h) const {
    if (!h.valid()) return false;

    // Check confidence threshold
    TruthValue tv = space_.get_tv(h);
    if (tv.confidence < config_.min_confidence) return false;

    // Check attention threshold if using attention
    if (config_.use_attention) {
        AttentionValue av = space_.get_av(h);
        if (av.sti < config_.attention_threshold) return false;
    }

    return true;
}

uint64_t PLNEngine::cache_key(
    const std::string& rule_name,
    const std::vector<Handle>& premises
) const {
    uint64_t key = std::hash<std::string>{}(rule_name);
    for (const Handle& h : premises) {
        key = hash_combine(key, h.id().value);
    }
    return key;
}

// ============================================================================
// IncrementalInference Implementation
// ============================================================================

IncrementalInference::IncrementalInference(PLNEngine& engine)
    : engine_(engine)
{
}

void IncrementalInference::add_stimulus(Handle atom) {
    if (atom.valid()) {
        pending_.push(atom);
    }
}

std::vector<InferenceResult> IncrementalInference::step() {
    if (pending_.empty()) return {};

    Handle current = pending_.front();
    pending_.pop();

    // Skip if already visited
    if (visited_.contains(current.id().value)) return {};
    visited_.insert(current.id().value);

    // Run one step of inference
    auto results = engine_.forward_step(current);

    // Add new conclusions to pending
    for (const auto& result : results) {
        if (!visited_.contains(result.conclusion.id().value)) {
            pending_.push(result.conclusion);
        }
    }

    return results;
}

std::vector<InferenceResult> IncrementalInference::run(size_t max_steps) {
    std::vector<InferenceResult> all_results;

    for (size_t i = 0; i < max_steps && has_pending(); ++i) {
        auto results = step();
        for (auto& r : results) {
            all_results.push_back(std::move(r));
        }
    }

    return all_results;
}

bool IncrementalInference::has_pending() const {
    return !pending_.empty();
}

} // namespace opencog::pln
