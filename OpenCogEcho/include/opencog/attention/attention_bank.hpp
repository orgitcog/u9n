#pragma once
/**
 * @file attention_bank.hpp
 * @brief Lock-free Economic Attention Network implementation
 *
 * ECAN manages cognitive resources through:
 * - Attention values (STI, LTI, VLTI)
 * - Attention allocation (spreading activation)
 * - Forgetting (garbage collection of low-importance atoms)
 *
 * This implementation uses lock-free atomics for high concurrency.
 */

#include <opencog/core/types.hpp>
#include <opencog/atomspace/atomspace.hpp>

#include <atomic>
#include <functional>
#include <random>
#include <vector>

namespace opencog {

// ============================================================================
// Configuration
// ============================================================================

struct ECANConfig {
    // Attention funds
    float initial_sti_funds = 100000.0f;
    float initial_lti_funds = 100000.0f;

    // Spreading activation
    float spreading_rate = 0.1f;          // Fraction of STI to spread
    float hebbian_weight = 0.5f;          // HebbianLink strength factor

    // Forgetting
    float forgetting_threshold = -100.0f;  // STI below which atoms may be forgotten
    float lti_decay_rate = 0.01f;         // LTI decay per cycle

    // Attentional focus
    float af_boundary = 0.0f;             // STI threshold for attentional focus
    size_t af_max_size = 1000;            // Maximum size of attentional focus

    // Rent
    float sti_rent = 1.0f;                // STI cost per cycle for being in AF
    float lti_rent = 0.1f;                // LTI cost per cycle

    // Wages (for atoms that perform useful work)
    float stimulus_wage = 10.0f;          // STI reward for stimulus
};

// ============================================================================
// Attention Bank
// ============================================================================

/**
 * @brief Central attention value management with lock-free operations
 *
 * The AttentionBank manages the "economy" of attention:
 * - Total STI and LTI are conserved (like money)
 * - Atoms "earn" attention by being useful
 * - Atoms "spend" attention as rent for being in focus
 * - Low-attention atoms get forgotten
 */
class AttentionBank {
public:
    explicit AttentionBank(AtomSpace& space, ECANConfig config = {});
    ~AttentionBank();

    AttentionBank(const AttentionBank&) = delete;
    AttentionBank& operator=(const AttentionBank&) = delete;

    // ========================================================================
    // Core Operations
    // ========================================================================

    /**
     * @brief Stimulate an atom (increase its STI)
     * @param id Atom to stimulate
     * @param amount Amount of STI to add
     * @return The new STI value
     */
    float stimulate(AtomId id, float amount);

    /**
     * @brief Transfer STI between atoms (spreading activation)
     */
    void transfer_sti(AtomId from, AtomId to, float amount);

    /**
     * @brief Spread attention from an atom to its neighbors
     */
    void spread_activation(AtomId source);

    /**
     * @brief Run one cycle of attention dynamics
     *
     * This includes:
     * - Collecting rent from atoms in attentional focus
     * - Decaying LTI values
     * - Potentially forgetting low-importance atoms
     */
    void update_cycle();

    // ========================================================================
    // Attention Queries
    // ========================================================================

    /**
     * @brief Get atoms in attentional focus (sorted by STI descending)
     */
    [[nodiscard]] std::vector<AtomId> get_attentional_focus() const;

    /**
     * @brief Get atoms in AF of a specific type
     */
    [[nodiscard]] std::vector<AtomId> get_attentional_focus_by_type(AtomType type) const;

    /**
     * @brief Check if atom is in attentional focus
     */
    [[nodiscard]] bool in_attentional_focus(AtomId id) const;

    /**
     * @brief Get current AF boundary (dynamic)
     */
    [[nodiscard]] float get_af_boundary() const noexcept;

    // ========================================================================
    // Fund Management
    // ========================================================================

    [[nodiscard]] float get_sti_funds() const noexcept;
    [[nodiscard]] float get_lti_funds() const noexcept;

    /**
     * @brief Add STI funds to the system
     */
    void add_sti_funds(float amount);

    /**
     * @brief Add LTI funds to the system
     */
    void add_lti_funds(float amount);

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t get_af_size() const noexcept;
    [[nodiscard]] float get_total_sti() const;  // Sum of all atoms' STI
    [[nodiscard]] float get_max_sti() const;
    [[nodiscard]] float get_min_sti() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const ECANConfig& config() const noexcept { return config_; }
    void set_config(ECANConfig config) { config_ = config; }

    // ========================================================================
    // Forgetting
    // ========================================================================

    /**
     * @brief Mark atoms for potential forgetting
     * @return Number of atoms marked
     */
    size_t mark_for_forgetting();

    /**
     * @brief Actually remove forgotten atoms
     * @return Number of atoms removed
     */
    size_t forget();

    /**
     * @brief Set callback for when atoms are forgotten
     */
    void on_forget(std::function<void(AtomId)> callback);

private:
    AtomSpace& space_;
    ECANConfig config_;

    // Lock-free fund management
    std::atomic<float> sti_funds_;
    std::atomic<float> lti_funds_;

    // Attentional focus tracking
    mutable std::atomic<size_t> af_size_{0};
    std::atomic<float> af_boundary_;

    // Forgetting state
    std::vector<AtomId> forgetting_candidates_;
    std::function<void(AtomId)> forget_callback_;

    // RNG for stochastic processes
    mutable std::mt19937 rng_{std::random_device{}()};

    // Internal helpers
    void collect_rent();
    void decay_lti();
    void update_af_boundary();
    [[nodiscard]] float compute_hebbian_weight(AtomId from, AtomId to) const;
};

// ============================================================================
// Importance Diffusion Agent
// ============================================================================

/**
 * @brief Background agent that continuously spreads attention
 *
 * Runs in its own thread, periodically:
 * - Selecting high-STI atoms
 * - Spreading their attention to neighbors
 * - Updating attention statistics
 */
class ImportanceDiffusionAgent {
public:
    ImportanceDiffusionAgent(AttentionBank& bank, AtomSpace& space);
    ~ImportanceDiffusionAgent();

    void start();
    void stop();
    void run_once();

    void set_interval_ms(unsigned int ms) { interval_ms_ = ms; }

private:
    AttentionBank& bank_;
    AtomSpace& space_;
    std::atomic<bool> running_{false};
    unsigned int interval_ms_ = 100;
};

// ============================================================================
// Forgetting Agent
// ============================================================================

/**
 * @brief Background agent that removes low-importance atoms
 */
class ForgettingAgent {
public:
    ForgettingAgent(AttentionBank& bank, AtomSpace& space);
    ~ForgettingAgent();

    void start();
    void stop();
    void run_once();

    void set_interval_ms(unsigned int ms) { interval_ms_ = ms; }

private:
    AttentionBank& bank_;
    AtomSpace& space_;
    std::atomic<bool> running_{false};
    unsigned int interval_ms_ = 1000;
};

} // namespace opencog
