#pragma once
/**
 * @file atom_table.hpp
 * @brief Data-oriented atom storage
 *
 * Structure of Arrays (SoA) design for cache efficiency.
 * Hot data (types, truth values) are contiguous in memory.
 * Cold data (names, outgoing sets) stored separately.
 */

#include <opencog/core/types.hpp>
#include <opencog/core/memory.hpp>

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <unordered_map>

namespace opencog {

// ============================================================================
// Node Data
// ============================================================================

/**
 * @brief Storage for node-specific data (names)
 */
struct NodeData {
    std::string name;
};

// ============================================================================
// Link Data
// ============================================================================

/**
 * @brief Storage for link-specific data (outgoing set)
 */
struct LinkData {
    std::vector<AtomId> outgoing;  // Atoms this link connects
};

// ============================================================================
// Atom Table - Structure of Arrays Design
// ============================================================================

/**
 * @brief Core atom storage with data-oriented design
 *
 * Design principles:
 * - Hot data (accessed every query) stored contiguously
 * - Cold data (accessed rarely) stored separately
 * - Lock-free reads with sharded writes
 * - Generation-based slot reuse
 */
class AtomTable {
public:
    static constexpr size_t INITIAL_CAPACITY = 1024;
    static constexpr size_t SHARD_COUNT = 16;  // For write sharding

    AtomTable();
    ~AtomTable();

    AtomTable(const AtomTable&) = delete;
    AtomTable& operator=(const AtomTable&) = delete;

    // ========================================================================
    // Atom Creation
    // ========================================================================

    /**
     * @brief Add a node to the table
     * @return AtomId of the created node (or existing if duplicate)
     */
    [[nodiscard]] AtomId add_node(
        AtomType type,
        std::string_view name,
        TruthValue tv = TruthValue::default_tv()
    );

    /**
     * @brief Add a link to the table
     * @return AtomId of the created link (or existing if duplicate)
     */
    [[nodiscard]] AtomId add_link(
        AtomType type,
        std::span<const AtomId> outgoing,
        TruthValue tv = TruthValue::default_tv()
    );

    /**
     * @brief Add a link with initializer list
     */
    [[nodiscard]] AtomId add_link(
        AtomType type,
        std::initializer_list<AtomId> outgoing,
        TruthValue tv = TruthValue::default_tv()
    ) {
        std::vector<AtomId> out(outgoing);
        return add_link(type, std::span<const AtomId>(out), tv);
    }

    // ========================================================================
    // Atom Removal
    // ========================================================================

    /**
     * @brief Remove an atom from the table
     * @return true if removed, false if not found or has incoming links
     */
    bool remove_atom(AtomId id, bool recursive = false);

    // ========================================================================
    // Atom Lookup
    // ========================================================================

    /**
     * @brief Check if an atom exists
     */
    [[nodiscard]] bool contains(AtomId id) const noexcept;

    /**
     * @brief Get a node by type and name
     */
    [[nodiscard]] AtomId get_node(AtomType type, std::string_view name) const;

    /**
     * @brief Get a link by type and outgoing set
     */
    [[nodiscard]] AtomId get_link(AtomType type, std::span<const AtomId> outgoing) const;

    // ========================================================================
    // Atom Properties (Hot Path - Lock-Free)
    // ========================================================================

    [[nodiscard]] AtomType get_type(AtomId id) const noexcept;
    [[nodiscard]] TruthValue get_tv(AtomId id) const noexcept;
    [[nodiscard]] AttentionValue get_av(AtomId id) const noexcept;
    [[nodiscard]] uint64_t get_hash(AtomId id) const noexcept;

    void set_tv(AtomId id, TruthValue tv) noexcept;
    void set_av(AtomId id, AttentionValue av) noexcept;

    // ========================================================================
    // Atom Properties (Cold Path)
    // ========================================================================

    [[nodiscard]] std::string_view get_name(AtomId id) const;
    [[nodiscard]] std::span<const AtomId> get_outgoing(AtomId id) const;
    [[nodiscard]] size_t get_arity(AtomId id) const;

    // ========================================================================
    // Incoming Set
    // ========================================================================

    [[nodiscard]] std::vector<AtomId> get_incoming(AtomId id) const;
    [[nodiscard]] size_t get_incoming_size(AtomId id) const noexcept;

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t node_count() const noexcept;
    [[nodiscard]] size_t link_count() const noexcept;

private:
    // ------------------------------------------------------------------------
    // Hot Data - Contiguous Arrays (accessed every query)
    // ------------------------------------------------------------------------
    std::vector<AtomHeader> headers_;           // Type, flags, hash
    std::vector<TruthValue> truth_values_;      // Strength, confidence
    std::vector<AttentionValue> attention_values_;  // STI, LTI, VLTI
    std::vector<uint16_t> generations_;         // For slot reuse validation

    // ------------------------------------------------------------------------
    // Cold Data - Separate Storage (accessed occasionally)
    // ------------------------------------------------------------------------
    std::vector<std::unique_ptr<NodeData>> node_data_;   // Node names
    std::vector<std::unique_ptr<LinkData>> link_data_;   // Link outgoing sets

    // ------------------------------------------------------------------------
    // Incoming Sets - Per-atom list of links pointing to it
    // ------------------------------------------------------------------------
    std::vector<std::vector<AtomId>> incoming_sets_;

    // ------------------------------------------------------------------------
    // Indices for Fast Lookup
    // ------------------------------------------------------------------------
    std::unordered_map<uint64_t, AtomId> hash_index_;  // hash -> AtomId

    // ------------------------------------------------------------------------
    // Free List for Slot Reuse
    // ------------------------------------------------------------------------
    std::vector<uint64_t> free_slots_;

    // ------------------------------------------------------------------------
    // Concurrency Control
    // ------------------------------------------------------------------------
    mutable std::shared_mutex global_mutex_;  // For structural changes
    mutable std::array<std::mutex, SHARD_COUNT> shard_mutexes_;  // For updates

    // ------------------------------------------------------------------------
    // Statistics
    // ------------------------------------------------------------------------
    std::atomic<size_t> atom_count_{0};
    std::atomic<size_t> node_count_{0};
    std::atomic<size_t> link_count_{0};

    // ------------------------------------------------------------------------
    // Internal Methods
    // ------------------------------------------------------------------------
    [[nodiscard]] uint64_t compute_node_hash(AtomType type, std::string_view name) const;
    [[nodiscard]] uint64_t compute_link_hash(AtomType type, std::span<const AtomId> outgoing) const;

    [[nodiscard]] AtomId allocate_slot();
    void free_slot(AtomId id);

    [[nodiscard]] size_t shard_for(AtomId id) const noexcept {
        return id.index() % SHARD_COUNT;
    }

    void add_to_incoming(AtomId target, AtomId link);
    void remove_from_incoming(AtomId target, AtomId link);

    [[nodiscard]] bool is_valid_slot(AtomId id) const noexcept;
};

// ============================================================================
// Inline Implementations (Hot Path)
// ============================================================================

inline bool AtomTable::contains(AtomId id) const noexcept {
    if (!id.valid() || id.index() >= headers_.size()) return false;
    return headers_[id.index()].type != AtomType::INVALID &&
           generations_[id.index()] == id.generation();
}

inline AtomType AtomTable::get_type(AtomId id) const noexcept {
    if (!is_valid_slot(id)) return AtomType::INVALID;
    return headers_[id.index()].type;
}

inline TruthValue AtomTable::get_tv(AtomId id) const noexcept {
    if (!is_valid_slot(id)) return TruthValue{};
    return truth_values_[id.index()];
}

inline AttentionValue AtomTable::get_av(AtomId id) const noexcept {
    if (!is_valid_slot(id)) return AttentionValue{};
    return attention_values_[id.index()];
}

inline uint64_t AtomTable::get_hash(AtomId id) const noexcept {
    if (!is_valid_slot(id)) return 0;
    return headers_[id.index()].hash;
}

inline void AtomTable::set_tv(AtomId id, TruthValue tv) noexcept {
    if (is_valid_slot(id)) {
        truth_values_[id.index()] = tv;
    }
}

inline void AtomTable::set_av(AtomId id, AttentionValue av) noexcept {
    if (is_valid_slot(id)) {
        attention_values_[id.index()] = av;
    }
}

inline size_t AtomTable::size() const noexcept {
    return atom_count_.load(std::memory_order_relaxed);
}

inline size_t AtomTable::node_count() const noexcept {
    return node_count_.load(std::memory_order_relaxed);
}

inline size_t AtomTable::link_count() const noexcept {
    return link_count_.load(std::memory_order_relaxed);
}

inline bool AtomTable::is_valid_slot(AtomId id) const noexcept {
    return id.valid() &&
           id.index() < headers_.size() &&
           headers_[id.index()].type != AtomType::INVALID &&
           generations_[id.index()] == id.generation();
}

} // namespace opencog
