/**
 * @file engine.cpp
 * @brief URE engine implementation
 */

#include <opencog/ure/engine.hpp>

#include <algorithm>
#include <queue>
#include <random>

namespace opencog::ure {

// ============================================================================
// InferenceTree Implementation
// ============================================================================

size_t InferenceTree::depth() const {
    if (nodes.empty()) return 0;

    size_t max_depth = 0;
    for (const auto& node : nodes) {
        max_depth = std::max(max_depth, node.depth);
    }
    return max_depth;
}

void InferenceTree::for_each_node(std::function<void(const InferenceNode&)> fn) const {
    for (const auto& node : nodes) {
        fn(node);
    }
}

std::string InferenceTree::to_string(const AtomSpace& space) const {
    std::string result;

    std::function<void(size_t, int)> print_node = [&](size_t idx, int indent) {
        if (idx >= nodes.size()) return;

        const auto& node = nodes[idx];
        std::string prefix(indent * 2, ' ');

        result += prefix;
        if (node.rule_used) {
            result += "[" + node.rule_used->name + "] ";
        }
        result += space.to_short_string(node.atom);
        result += " (";
        result += std::to_string(node.tv.strength) + ", ";
        result += std::to_string(node.tv.confidence) + ")\n";

        for (size_t premise_idx : node.premise_indices) {
            print_node(premise_idx, indent + 1);
        }
    };

    print_node(root_index, 0);
    return result;
}

// ============================================================================
// SearchState Implementation
// ============================================================================

bool UREngine::SearchState::should_stop(const UREConfig& config) const {
    // Check iteration limit
    if (iterations >= config.max_iterations) return true;

    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed >= config.timeout) return true;

    // Check if frontier is empty
    if (frontier.empty()) return true;

    return false;
}

// ============================================================================
// UREngine Implementation
// ============================================================================

UREngine::UREngine(AtomSpace& space, UREConfig config)
    : space_(space)
    , config_(std::move(config))
    , rules_(space)
    , applicator_(space, rules_)
{
}

// ============================================================================
// Forward Chaining
// ============================================================================

std::vector<UREResult> UREngine::forward_chain(std::span<const Handle> sources) {
    auto start_time = std::chrono::steady_clock::now();
    std::vector<UREResult> results;

    SearchState state;
    state.start_time = start_time;

    // Initialize frontier with sources
    for (Handle h : sources) {
        if (h.valid()) {
            state.frontier.push_back(h);
            state.visited.insert(h.id().value);
        }
    }

    while (!state.should_stop(config_) && results.size() < config_.max_results) {
        Handle current = select_next(state);
        if (!current.valid()) break;

        // Apply rules to current atom
        auto step_results = forward_step(current);

        for (auto& app_result : step_results) {
            // Check result filter
            if (config_.result_filter && !config_.result_filter(app_result.result)) {
                continue;
            }

            // Check confidence threshold
            if (app_result.computed_tv.confidence < config_.min_result_confidence) {
                continue;
            }

            // Record the application
            if (config_.record_proofs) {
                record_application(state, app_result);
            }

            // Build result
            UREResult ure_result;
            ure_result.conclusion = app_result.result;
            ure_result.tv = app_result.computed_tv;
            ure_result.iterations_used = state.iterations;

            if (config_.record_proofs) {
                ure_result.proof = build_proof_tree(state);
            }

            auto elapsed = std::chrono::steady_clock::now() - start_time;
            ure_result.time_taken = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);

            results.push_back(std::move(ure_result));
            stats_.atoms_created++;

            // Callback
            if (config_.on_result) {
                config_.on_result(app_result);
            }

            // Add to frontier if not visited
            if (!state.visited.contains(app_result.result.id().value)) {
                state.frontier.push_back(app_result.result);
                state.visited.insert(app_result.result.id().value);
            }
        }

        state.iterations++;
        stats_.total_iterations++;

        if (config_.on_iteration) {
            config_.on_iteration(state.iterations);
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    stats_.total_time += std::chrono::duration_cast<std::chrono::microseconds>(elapsed);

    return results;
}

std::optional<UREResult> UREngine::forward_chain_to(
    std::span<const Handle> sources,
    Handle target
) {
    // Set up target check
    auto original_filter = config_.result_filter;
    config_.result_filter = [&target, &original_filter](Handle h) {
        if (h.id() == target.id()) return true;
        if (original_filter) return original_filter(h);
        return true;
    };

    auto results = forward_chain(sources);

    // Restore original filter
    config_.result_filter = original_filter;

    // Find the target in results
    for (auto& result : results) {
        if (result.conclusion.id() == target.id()) {
            return result;
        }
    }

    return std::nullopt;
}

std::vector<RuleApplicationResult> UREngine::forward_step(Handle source) {
    std::vector<RuleApplicationResult> results;

    // Get applicable rules
    AtomType type = space_.get_type(source);
    auto applicable_rules = rules_.get_rules_for_type(type);

    // Also try generic rules
    auto generic_rules = rules_.get_rules_by_priority();

    for (const Rule* rule : applicable_rules) {
        if (!rule || rule->name.empty()) continue;

        // Try to apply rule
        auto rule_results = applicator_.find_applicable(source);
        for (auto& result : rule_results) {
            results.push_back(std::move(result));
            stats_.rules_applied++;
        }
    }

    return results;
}

// ============================================================================
// Backward Chaining
// ============================================================================

std::optional<UREResult> UREngine::backward_chain(Handle target) {
    auto start_time = std::chrono::steady_clock::now();

    if (!target.valid()) return std::nullopt;

    // Check if already grounded
    TruthValue tv = space_.get_tv(target);
    if (tv.confidence >= config_.min_result_confidence) {
        UREResult result;
        result.conclusion = target;
        result.tv = tv;
        result.iterations_used = 0;
        return result;
    }

    // Find rules whose conclusion could unify with target
    SearchState state;
    state.start_time = start_time;
    state.frontier.push_back(target);

    while (!state.should_stop(config_)) {
        if (state.frontier.empty()) break;

        Handle current = state.frontier.back();
        state.frontier.pop_back();

        auto step_results = backward_step(current);

        for (auto& [rule, bindings] : step_results) {
            // Try to prove premises
            // This would recursively call backward_chain on each premise

            // For now, check if premises are grounded
            bool all_grounded = true;
            // ... premise checking logic ...

            if (all_grounded) {
                // Apply rule
                auto app_result = applicator_.apply(*rule, bindings);
                if (app_result) {
                    UREResult result;
                    result.conclusion = app_result->result;
                    result.tv = app_result->computed_tv;
                    result.iterations_used = state.iterations;

                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    result.time_taken = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);

                    return result;
                }
            }
        }

        state.iterations++;
    }

    return std::nullopt;
}

std::vector<UREResult> UREngine::find_all_proofs(Handle target, size_t max_proofs) {
    std::vector<UREResult> proofs;

    // Run backward chaining multiple times with different strategies
    auto first = backward_chain(target);
    if (first) {
        proofs.push_back(std::move(*first));
    }

    // Additional proof search could use different rule orderings, etc.

    return proofs;
}

std::vector<std::pair<const Rule*, BindingSet>> UREngine::backward_step(Handle target) {
    std::vector<std::pair<const Rule*, BindingSet>> results;

    // Find rules whose conclusion could unify with target
    for (const Rule* rule : rules_.get_rules_by_priority()) {
        if (!rule || !rule->valid()) continue;

        // Check if conclusion could unify
        // Full implementation would use pattern matcher

        // Placeholder: simple type check
        if (space_.get_type(rule->conclusion) == space_.get_type(target)) {
            BindingSet bindings;
            results.emplace_back(rule, bindings);
        }
    }

    return results;
}

// ============================================================================
// Hybrid Chaining
// ============================================================================

std::optional<UREResult> UREngine::bidirectional_chain(
    std::span<const Handle> sources,
    Handle target
) {
    // Run forward and backward in alternation
    // Meet in the middle

    SearchState forward_state, backward_state;
    forward_state.start_time = std::chrono::steady_clock::now();
    backward_state.start_time = forward_state.start_time;

    // Initialize
    for (Handle h : sources) {
        forward_state.frontier.push_back(h);
        forward_state.visited.insert(h.id().value);
    }
    backward_state.frontier.push_back(target);
    backward_state.visited.insert(target.id().value);

    while (!forward_state.should_stop(config_) && !backward_state.should_stop(config_)) {
        // One forward step
        if (!forward_state.frontier.empty()) {
            Handle current = select_next(forward_state);
            auto results = forward_step(current);

            for (auto& result : results) {
                // Check if we've reached something in backward frontier
                if (backward_state.visited.contains(result.result.id().value)) {
                    // Found connection!
                    UREResult ure_result;
                    ure_result.conclusion = result.result;
                    ure_result.tv = result.computed_tv;
                    return ure_result;
                }

                forward_state.frontier.push_back(result.result);
                forward_state.visited.insert(result.result.id().value);
            }
        }

        // One backward step
        if (!backward_state.frontier.empty()) {
            Handle current = backward_state.frontier.back();
            backward_state.frontier.pop_back();

            // Check if connected to forward
            if (forward_state.visited.contains(current.id().value)) {
                UREResult result;
                result.conclusion = current;
                result.tv = space_.get_tv(current);
                return result;
            }

            auto premises = backward_step(current);
            // Add premises to backward frontier
        }
    }

    return std::nullopt;
}

// ============================================================================
// Internal Methods
// ============================================================================

Handle UREngine::select_next(SearchState& state) {
    if (state.frontier.empty()) return Handle{};

    switch (config_.strategy) {
        case SearchStrategy::BFS: {
            Handle h = state.frontier.front();
            state.frontier.erase(state.frontier.begin());
            return h;
        }

        case SearchStrategy::DFS: {
            Handle h = state.frontier.back();
            state.frontier.pop_back();
            return h;
        }

        case SearchStrategy::BEST_FIRST: {
            // Find highest priority
            size_t best_idx = 0;
            float best_priority = -std::numeric_limits<float>::infinity();

            for (size_t i = 0; i < state.frontier.size(); ++i) {
                float p = compute_priority(state.frontier[i], nullptr);
                if (p > best_priority) {
                    best_priority = p;
                    best_idx = i;
                }
            }

            Handle h = state.frontier[best_idx];
            state.frontier.erase(state.frontier.begin() + best_idx);
            return h;
        }

        case SearchStrategy::ATTENTION: {
            if (!attention_) {
                // Fallback to BFS
                Handle h = state.frontier.front();
                state.frontier.erase(state.frontier.begin());
                return h;
            }

            // Select by STI
            size_t best_idx = 0;
            float best_sti = -std::numeric_limits<float>::infinity();

            for (size_t i = 0; i < state.frontier.size(); ++i) {
                float sti = space_.get_av(state.frontier[i]).sti;
                if (config_.use_attention) {
                    sti *= config_.attention_boost;
                }
                if (sti > best_sti) {
                    best_sti = sti;
                    best_idx = i;
                }
            }

            Handle h = state.frontier[best_idx];
            state.frontier.erase(state.frontier.begin() + best_idx);
            return h;
        }

        case SearchStrategy::RANDOM: {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, state.frontier.size() - 1);
            size_t idx = dist(rng);

            Handle h = state.frontier[idx];
            state.frontier.erase(state.frontier.begin() + idx);
            return h;
        }

        case SearchStrategy::ITERATIVE_DEEPENING:
            // Would need additional depth tracking
            return state.frontier.back();
    }

    return Handle{};
}

float UREngine::compute_priority(Handle h, const Rule* rule) const {
    if (config_.priority_fn) {
        BindingSet empty;
        if (rule) {
            return config_.priority_fn(*rule, h, empty);
        }
    }

    // Default priority: based on truth value and attention
    float priority = 0.0f;

    TruthValue tv = space_.get_tv(h);
    priority += tv.strength * tv.confidence;

    if (config_.use_attention && attention_) {
        AttentionValue av = space_.get_av(h);
        priority += av.sti * config_.attention_boost;
    }

    return priority;
}

void UREngine::record_application(
    SearchState& state,
    const RuleApplicationResult& result
) {
    InferenceNode node;
    node.atom = result.result;
    node.rule_used = result.rule;
    node.bindings = result.bindings;
    node.tv = result.computed_tv;
    node.depth = state.proof_nodes.empty() ? 0 : state.proof_nodes.back().depth + 1;

    state.proof_nodes.push_back(std::move(node));
}

InferenceTree UREngine::build_proof_tree(const SearchState& state) const {
    InferenceTree tree;
    tree.nodes = state.proof_nodes;
    tree.root_index = tree.nodes.empty() ? 0 : tree.nodes.size() - 1;
    return tree;
}

// ============================================================================
// UREQuery Implementation
// ============================================================================

UREQuery& UREQuery::using_rules(const RuleBase& rules) {
    for (const auto& rule : rules.get_all_rules()) {
        engine_.rules().add_rule(rule);
    }
    return *this;
}

} // namespace opencog::ure
