#pragma once
/**
 * @file inference.hpp
 * @brief PLN inference engine
 *
 * Provides high-level inference operations:
 * - Forward chaining
 * - Backward chaining
 * - Inference control
 */

#include <opencog/pln/formulas.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/pattern/matcher.hpp>

#include <functional>
#include <queue>
#include <unordered_set>

namespace opencog::pln {

// ============================================================================
// Inference Rule
// ============================================================================

/**
 * @brief Abstract inference rule
 *
 * Rules define:
 * - A pattern to match premises
 * - A formula to compute the conclusion's truth value
 * - A template for the conclusion
 */
struct InferenceRule {
    std::string name;
    Pattern premise_pattern;
    std::function<TruthValue(const std::vector<TruthValue>&)> formula;
    std::function<Handle(AtomSpace&, const BindingSet&)> conclusion_template;

    // Priority for rule selection
    float priority{1.0f};

    // Rule applicability check
    std::function<bool(const AtomSpace&, const BindingSet&)> applicable;
};

// ============================================================================
// Built-in Rules
// ============================================================================

namespace rules {

/**
 * @brief Create deduction rule: (A->B, B->C) => A->C
 */
[[nodiscard]] InferenceRule make_deduction_rule();

/**
 * @brief Create inversion rule: (A->B) => B->A
 */
[[nodiscard]] InferenceRule make_inversion_rule();

/**
 * @brief Create modus ponens rule: (A, A->B) => B
 */
[[nodiscard]] InferenceRule make_modus_ponens_rule();

/**
 * @brief Create abduction rule: (A->B, C->B) => A->C
 */
[[nodiscard]] InferenceRule make_abduction_rule();

/**
 * @brief Create AND introduction rule: (A, B) => (AND A B)
 */
[[nodiscard]] InferenceRule make_and_rule();

/**
 * @brief Create OR introduction rule: (A, B) => (OR A B)
 */
[[nodiscard]] InferenceRule make_or_rule();

/**
 * @brief Create NOT introduction rule: A => (NOT A)
 */
[[nodiscard]] InferenceRule make_not_rule();

/**
 * @brief Get all standard PLN rules
 */
[[nodiscard]] std::vector<InferenceRule> get_standard_rules();

} // namespace rules

// ============================================================================
// Inference Configuration
// ============================================================================

struct InferenceConfig {
    size_t max_iterations = 1000;         // Max inference steps
    size_t max_results = 100;             // Max conclusions to generate
    float min_confidence = 0.1f;          // Min confidence threshold
    float attention_threshold = 0.0f;     // Min STI for consideration

    bool use_attention = true;            // Use attention values for guidance
    bool allow_cycles = false;            // Allow inference cycles
    bool record_proof = true;             // Record inference steps

    // Termination conditions
    std::function<bool(const Handle&)> target_reached;  // Goal check
};

// ============================================================================
// Inference Step (for proof recording)
// ============================================================================

struct InferenceStep {
    std::string rule_name;
    std::vector<Handle> premises;
    Handle conclusion;
    TruthValue computed_tv;
};

// ============================================================================
// Inference Result
// ============================================================================

struct InferenceResult {
    Handle conclusion;
    TruthValue truth_value;
    std::vector<InferenceStep> proof;  // If record_proof enabled
    size_t iterations_used;
};

// ============================================================================
// PLN Inference Engine
// ============================================================================

/**
 * @brief Main PLN inference engine
 *
 * Supports both forward and backward chaining with:
 * - Attention-guided search
 * - Proof recording
 * - Configurable termination
 */
class PLNEngine {
public:
    explicit PLNEngine(AtomSpace& space, InferenceConfig config = {});

    // ========================================================================
    // Rule Management
    // ========================================================================

    void add_rule(InferenceRule rule);
    void add_rules(std::vector<InferenceRule> rules);
    void clear_rules();

    [[nodiscard]] const std::vector<InferenceRule>& rules() const { return rules_; }

    // ========================================================================
    // Forward Chaining
    // ========================================================================

    /**
     * @brief Run forward chaining from a source atom
     *
     * Applies rules to generate new conclusions, following
     * inference paths guided by attention.
     */
    [[nodiscard]] std::vector<InferenceResult> forward_chain(Handle source);

    /**
     * @brief Run forward chaining from multiple sources
     */
    [[nodiscard]] std::vector<InferenceResult> forward_chain(
        std::span<const Handle> sources
    );

    /**
     * @brief Run one step of forward chaining
     */
    [[nodiscard]] std::vector<InferenceResult> forward_step(Handle source);

    // ========================================================================
    // Backward Chaining
    // ========================================================================

    /**
     * @brief Run backward chaining to prove a target
     *
     * Searches for premises that could derive the target,
     * recursively until grounded facts are found.
     */
    [[nodiscard]] std::optional<InferenceResult> backward_chain(Handle target);

    /**
     * @brief Find all proofs for a target (up to max_results)
     */
    [[nodiscard]] std::vector<InferenceResult> find_proofs(Handle target);

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(InferenceConfig config) { config_ = std::move(config); }
    [[nodiscard]] const InferenceConfig& config() const { return config_; }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t total_inferences() const { return total_inferences_; }
    [[nodiscard]] size_t cache_hits() const { return cache_hits_; }

    void reset_stats() {
        total_inferences_ = 0;
        cache_hits_ = 0;
    }

private:
    AtomSpace& space_;
    InferenceConfig config_;
    std::vector<InferenceRule> rules_;
    PatternMatcher matcher_;

    // Inference cache to avoid redundant work
    std::unordered_map<uint64_t, TruthValue> inference_cache_;

    // Statistics
    size_t total_inferences_{0};
    size_t cache_hits_{0};

    // Internal methods
    [[nodiscard]] std::vector<InferenceResult> apply_rule(
        const InferenceRule& rule,
        const BindingSet& bindings
    );

    [[nodiscard]] bool should_pursue(Handle h) const;

    [[nodiscard]] uint64_t cache_key(
        const std::string& rule_name,
        const std::vector<Handle>& premises
    ) const;
};

// ============================================================================
// Incremental Inference
// ============================================================================

/**
 * @brief Manages incremental inference over time
 *
 * Useful for agents that need to reason continuously
 * while handling new information.
 */
class IncrementalInference {
public:
    explicit IncrementalInference(PLNEngine& engine);

    /**
     * @brief Add new information to the inference queue
     */
    void add_stimulus(Handle atom);

    /**
     * @brief Process one inference step
     * @return Any new conclusions generated
     */
    [[nodiscard]] std::vector<InferenceResult> step();

    /**
     * @brief Process multiple steps (up to limit)
     */
    [[nodiscard]] std::vector<InferenceResult> run(size_t max_steps);

    /**
     * @brief Check if there's pending inference work
     */
    [[nodiscard]] bool has_pending() const;

private:
    PLNEngine& engine_;
    std::queue<Handle> pending_;
    std::unordered_set<uint64_t> visited_;
};

} // namespace opencog::pln
