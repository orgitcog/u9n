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

        // forward_step already tries every registered rule against the
        // current atom, so it must be called exactly once per atom.
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

    if (!source.valid()) return results;

    const AtomType source_type = space_.get_type(source);

    for (const auto& rule : rules_) {
        if (rule.name == "deduction" && source_type == AtomType::IMPLICATION_LINK) {
            apply_deduction(source, results);
        } else if (rule.name == "inversion" && source_type == AtomType::IMPLICATION_LINK) {
            apply_inversion(source, results);
        } else if (rule.name == "modus-ponens" && is_node(source_type)) {
            // Modus ponens takes a premise atom A plus A->B; links (e.g.
            // implications on the frontier) are not valid premise atoms.
            apply_modus_ponens(source, results);
        }
    }

    return results;
}

// Given source A->B, find every B->C and derive A->C via deduction.
void PLNEngine::apply_deduction(Handle source, std::vector<InferenceResult>& results) {
    auto out = space_.get_outgoing(source);
    if (out.size() != 2) return;
    Handle a = out[0];
    Handle b = out[1];

    for (Handle bc : space_.get_incoming_by_type(b, AtomType::IMPLICATION_LINK)) {
        if (bc.id() == source.id()) continue;
        auto bc_out = space_.get_outgoing(bc);
        if (bc_out.size() != 2 || bc_out[0].id() != b.id()) continue;
        Handle c = bc_out[1];
        if (c.id() == a.id()) continue;

        TruthValue tv_ac = deduction(
            space_.get_tv(source), space_.get_tv(bc),
            space_.get_tv(b).strength, space_.get_tv(c).strength);
        if (tv_ac.confidence < config_.min_confidence) continue;

        Handle ac = space_.get_link(AtomType::IMPLICATION_LINK, {a, c});
        if (ac.valid()) {
            tv_ac = revision(space_.get_tv(ac), tv_ac);
            space_.set_tv(ac, tv_ac);
        } else {
            ac = space_.add_link(AtomType::IMPLICATION_LINK, {a, c}, tv_ac);
        }

        InferenceResult result;
        result.conclusion = ac;
        result.truth_value = tv_ac;
        result.iterations_used = 1;
        if (config_.record_proof) {
            result.proof.push_back(InferenceStep{"deduction", {source, bc}, ac, tv_ac});
        }
        results.push_back(std::move(result));
    }
}

// Given source A->B, derive B->A via Bayes inversion.
void PLNEngine::apply_inversion(Handle source, std::vector<InferenceResult>& results) {
    auto out = space_.get_outgoing(source);
    if (out.size() != 2) return;
    Handle a = out[0];
    Handle b = out[1];
    if (a.id() == b.id()) return;

    TruthValue tv_ba = inversion(
        space_.get_tv(source),
        space_.get_tv(a).strength, space_.get_tv(b).strength);
    if (tv_ba.confidence < config_.min_confidence) return;

    Handle ba = space_.get_link(AtomType::IMPLICATION_LINK, {b, a});
    if (ba.valid()) {
        tv_ba = revision(space_.get_tv(ba), tv_ba);
        space_.set_tv(ba, tv_ba);
    } else {
        ba = space_.add_link(AtomType::IMPLICATION_LINK, {b, a}, tv_ba);
    }

    InferenceResult result;
    result.conclusion = ba;
    result.truth_value = tv_ba;
    result.iterations_used = 1;
    if (config_.record_proof) {
        result.proof.push_back(InferenceStep{"inversion", {source}, ba, tv_ba});
    }
    results.push_back(std::move(result));
}

// Given source A and links A->B, strengthen each B via modus ponens.
void PLNEngine::apply_modus_ponens(Handle source, std::vector<InferenceResult>& results) {
    TruthValue tv_a = space_.get_tv(source);
    if (tv_a.confidence < config_.min_confidence) return;

    for (Handle ab : space_.get_incoming_by_type(source, AtomType::IMPLICATION_LINK)) {
        auto ab_out = space_.get_outgoing(ab);
        if (ab_out.size() != 2 || ab_out[0].id() != source.id()) continue;
        Handle b = ab_out[1];

        TruthValue tv_b = modus_ponens(tv_a, space_.get_tv(ab));
        if (tv_b.confidence < config_.min_confidence) continue;

        TruthValue merged = revision(space_.get_tv(b), tv_b);
        space_.set_tv(b, merged);

        InferenceResult result;
        result.conclusion = b;
        result.truth_value = merged;
        result.iterations_used = 1;
        if (config_.record_proof) {
            result.proof.push_back(InferenceStep{"modus-ponens", {source, ab}, b, merged});
        }
        results.push_back(std::move(result));
    }
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
