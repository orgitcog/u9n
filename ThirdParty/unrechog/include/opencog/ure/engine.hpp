#pragma once
/**
 * @file engine.hpp
 * @brief Unified Rule Engine main interface
 *
 * The URE provides:
 * - Forward chaining (data-driven inference)
 * - Backward chaining (goal-driven inference)
 * - Configurable search strategies
 * - Integration with attention allocation
 */

#include <opencog/ure/rule.hpp>
#include <opencog/attention/attention_bank.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <queue>
#include <variant>

namespace opencog::ure {

// ============================================================================
// Search Strategies
// ============================================================================

/**
 * @brief How to select the next rule/target
 */
enum class SearchStrategy {
    BFS,              // Breadth-first (fair exploration)
    DFS,              // Depth-first (follow chains deeply)
    BEST_FIRST,       // Priority queue by heuristic
    ATTENTION,        // Follow attention values
    RANDOM,           // Random selection
    ITERATIVE_DEEPENING  // DFS with increasing depth limit
};

/**
 * @brief Custom priority function for best-first search
 */
using PriorityFunction = std::function<float(const Rule&, Handle, const BindingSet&)>;

// ============================================================================
// URE Configuration
// ============================================================================

struct UREConfig {
    // Search control
    SearchStrategy strategy = SearchStrategy::ATTENTION;
    size_t max_iterations = 1000;
    size_t max_results = 100;
    std::chrono::milliseconds timeout{5000};

    // Rule selection
    float min_rule_confidence = 0.1f;
    bool allow_rule_repetition = false;

    // Result filtering
    float min_result_confidence = 0.1f;
    std::function<bool(Handle)> result_filter;

    // Attention integration
    bool use_attention = true;
    float attention_boost = 2.0f;  // Multiplier for high-STI atoms

    // Proof recording
    bool record_proofs = true;
    size_t max_proof_depth = 50;

    // Custom priority (for BEST_FIRST)
    PriorityFunction priority_fn;

    // Callbacks
    std::function<void(const RuleApplicationResult&)> on_result;
    std::function<void(size_t iteration)> on_iteration;
};

// ============================================================================
// Inference Tree Node
// ============================================================================

/**
 * @brief Node in the inference/proof tree
 */
struct InferenceNode {
    Handle atom;
    const Rule* rule_used{nullptr};
    BindingSet bindings;
    TruthValue tv;
    std::vector<size_t> premise_indices;  // Indices of premise nodes
    size_t depth{0};
};

/**
 * @brief Complete inference tree (proof)
 */
struct InferenceTree {
    std::vector<InferenceNode> nodes;
    size_t root_index{0};

    [[nodiscard]] const InferenceNode& root() const { return nodes[root_index]; }
    [[nodiscard]] bool empty() const { return nodes.empty(); }
    [[nodiscard]] size_t depth() const;

    // Tree traversal
    void for_each_node(std::function<void(const InferenceNode&)> fn) const;
    [[nodiscard]] std::string to_string(const AtomSpace& space) const;
};

// ============================================================================
// URE Result
// ============================================================================

struct UREResult {
    Handle conclusion;
    TruthValue tv;
    InferenceTree proof;  // If record_proofs enabled
    size_t iterations_used;
    std::chrono::microseconds time_taken;

    [[nodiscard]] bool valid() const { return conclusion.valid(); }
};

// ============================================================================
// Unified Rule Engine
// ============================================================================

/**
 * @brief Main URE class
 *
 * Provides both forward and backward chaining with:
 * - Pluggable search strategies
 * - Attention-guided inference
 * - Proof recording
 * - Resource limits
 */
class UREngine {
public:
    UREngine(AtomSpace& space, UREConfig config = {});

    // Set optional attention bank for attention-guided search
    void set_attention_bank(AttentionBank* bank) { attention_ = bank; }

    // ========================================================================
    // Rule Management
    // ========================================================================

    RuleBase& rules() { return rules_; }
    const RuleBase& rules() const { return rules_; }

    // ========================================================================
    // Forward Chaining
    // ========================================================================

    /**
     * @brief Run forward chaining from source atoms
     *
     * Applies rules to generate new conclusions,
     * adding them to the AtomSpace.
     */
    [[nodiscard]] std::vector<UREResult> forward_chain(
        std::span<const Handle> sources
    );

    [[nodiscard]] std::vector<UREResult> forward_chain(Handle source) {
        return forward_chain(std::span<const Handle>(&source, 1));
    }

    /**
     * @brief Run forward chaining until a target is found
     */
    [[nodiscard]] std::optional<UREResult> forward_chain_to(
        std::span<const Handle> sources,
        Handle target
    );

    // ========================================================================
    // Backward Chaining
    // ========================================================================

    /**
     * @brief Run backward chaining to prove a target
     *
     * Searches for rules whose conclusions unify with the target,
     * then recursively tries to prove the premises.
     */
    [[nodiscard]] std::optional<UREResult> backward_chain(Handle target);

    /**
     * @brief Find all proofs for a target
     */
    [[nodiscard]] std::vector<UREResult> find_all_proofs(
        Handle target,
        size_t max_proofs = SIZE_MAX
    );

    // ========================================================================
    // Hybrid Chaining
    // ========================================================================

    /**
     * @brief Bidirectional search from both sources and target
     *
     * More efficient for some problems than pure forward/backward.
     */
    [[nodiscard]] std::optional<UREResult> bidirectional_chain(
        std::span<const Handle> sources,
        Handle target
    );

    // ========================================================================
    // Single Step Operations
    // ========================================================================

    /**
     * @brief Perform one forward chaining step
     */
    [[nodiscard]] std::vector<RuleApplicationResult> forward_step(Handle source);

    /**
     * @brief Perform one backward chaining step
     */
    [[nodiscard]] std::vector<std::pair<const Rule*, BindingSet>> backward_step(
        Handle target
    );

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(UREConfig config) { config_ = std::move(config); }
    [[nodiscard]] const UREConfig& config() const { return config_; }

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t total_iterations{0};
        size_t rules_applied{0};
        size_t atoms_created{0};
        size_t cache_hits{0};
        std::chrono::microseconds total_time{0};
    };

    [[nodiscard]] Stats stats() const { return stats_; }
    void reset_stats() { stats_ = Stats{}; }

private:
    AtomSpace& space_;
    UREConfig config_;
    RuleBase rules_;
    RuleApplicator applicator_;
    AttentionBank* attention_{nullptr};

    Stats stats_;

    // Search state
    struct SearchState {
        std::vector<Handle> frontier;
        std::unordered_set<uint64_t> visited;
        std::vector<InferenceNode> proof_nodes;
        size_t iterations{0};
        std::chrono::steady_clock::time_point start_time;

        bool should_stop(const UREConfig& config) const;
    };

    // Internal methods
    [[nodiscard]] Handle select_next(SearchState& state);
    [[nodiscard]] float compute_priority(Handle h, const Rule* rule) const;

    void record_application(
        SearchState& state,
        const RuleApplicationResult& result
    );

    [[nodiscard]] InferenceTree build_proof_tree(const SearchState& state) const;
};

// ============================================================================
// Convenience: Query Interface
// ============================================================================

/**
 * @brief Fluent interface for URE queries
 *
 * Usage:
 *   auto results = UREQuery(space)
 *       .from(source)
 *       .using_rules(rules)
 *       .with_strategy(SearchStrategy::BEST_FIRST)
 *       .max_iterations(100)
 *       .forward_chain();
 */
class UREQuery {
public:
    explicit UREQuery(AtomSpace& space) : engine_(space) {}

    UREQuery& from(Handle source) {
        sources_.push_back(source);
        return *this;
    }

    UREQuery& from(std::vector<Handle> sources) {
        sources_ = std::move(sources);
        return *this;
    }

    UREQuery& to(Handle target) {
        target_ = target;
        return *this;
    }

    UREQuery& using_rules(const RuleBase& rules);

    UREQuery& with_strategy(SearchStrategy s) {
        config_.strategy = s;
        return *this;
    }

    UREQuery& max_iterations(size_t n) {
        config_.max_iterations = n;
        return *this;
    }

    UREQuery& max_results(size_t n) {
        config_.max_results = n;
        return *this;
    }

    UREQuery& timeout(std::chrono::milliseconds ms) {
        config_.timeout = ms;
        return *this;
    }

    UREQuery& with_attention(AttentionBank& bank) {
        engine_.set_attention_bank(&bank);
        config_.use_attention = true;
        return *this;
    }

    [[nodiscard]] std::vector<UREResult> forward_chain() {
        engine_.set_config(config_);
        return engine_.forward_chain(sources_);
    }

    [[nodiscard]] std::optional<UREResult> backward_chain() {
        engine_.set_config(config_);
        return target_ ? engine_.backward_chain(*target_) : std::nullopt;
    }

    [[nodiscard]] std::optional<UREResult> solve() {
        engine_.set_config(config_);
        if (target_) {
            return engine_.forward_chain_to(sources_, *target_);
        }
        return std::nullopt;
    }

private:
    UREngine engine_;
    UREConfig config_;
    std::vector<Handle> sources_;
    std::optional<Handle> target_;
};

} // namespace opencog::ure
