#pragma once
/**
 * @file atomspace.hpp
 * @brief Main AtomSpace interface
 *
 * The AtomSpace is the central knowledge representation structure.
 * It combines:
 * - AtomTable: Core storage
 * - Indices: Fast lookup
 * - Query interface: Pattern matching entry point
 */

#include <opencog/atomspace/atom_table.hpp>
#include <opencog/atomspace/index.hpp>

#include <functional>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

namespace opencog {

/**
 * @brief The central knowledge hypergraph
 *
 * Provides a high-level interface for:
 * - Creating and managing atoms
 * - Querying the knowledge base
 * - Accessing truth and attention values
 */
class AtomSpace {
public:
    AtomSpace();
    ~AtomSpace();

    AtomSpace(const AtomSpace&) = delete;
    AtomSpace& operator=(const AtomSpace&) = delete;

    // ========================================================================
    // Atom Creation - Fluent Builder Interface
    // ========================================================================

    /**
     * @brief Create a node
     *
     * Usage:
     *   auto cat = space.add_node(CONCEPT_NODE, "Cat");
     *   auto cat = space.add_node(CONCEPT_NODE, "Cat", TruthValue{0.9, 0.8});
     */
    [[nodiscard]] Handle add_node(
        AtomType type,
        std::string_view name,
        TruthValue tv = TruthValue::default_tv()
    );

    /**
     * @brief Create a link
     *
     * Usage:
     *   auto link = space.add_link(INHERITANCE_LINK, {cat, animal});
     */
    [[nodiscard]] Handle add_link(
        AtomType type,
        std::initializer_list<Handle> outgoing,
        TruthValue tv = TruthValue::default_tv()
    );

    [[nodiscard]] Handle add_link(
        AtomType type,
        std::span<const Handle> outgoing,
        TruthValue tv = TruthValue::default_tv()
    );

    [[nodiscard]] Handle add_link(
        AtomType type,
        std::span<const AtomId> outgoing,
        TruthValue tv = TruthValue::default_tv()
    );

    // ========================================================================
    // Atom Removal
    // ========================================================================

    /**
     * @brief Remove an atom
     * @param recursive If true, also remove atoms that would become orphaned
     * @return true if removed
     */
    bool remove(Handle h, bool recursive = false);
    bool remove(AtomId id, bool recursive = false);

    // ========================================================================
    // Atom Lookup
    // ========================================================================

    /**
     * @brief Get a node by type and name
     */
    [[nodiscard]] Handle get_node(AtomType type, std::string_view name) const;

    /**
     * @brief Get a link by type and outgoing set
     */
    [[nodiscard]] Handle get_link(AtomType type, std::initializer_list<Handle> outgoing) const;

    /**
     * @brief Check if an atom exists
     */
    [[nodiscard]] bool contains(Handle h) const noexcept;
    [[nodiscard]] bool contains(AtomId id) const noexcept;

    // ========================================================================
    // Handle Resolution
    // ========================================================================

    /**
     * @brief Create a handle from an AtomId
     */
    [[nodiscard]] Handle make_handle(AtomId id) const;

    // ========================================================================
    // Atom Properties
    // ========================================================================

    [[nodiscard]] AtomType get_type(Handle h) const noexcept;
    [[nodiscard]] std::string_view get_name(Handle h) const;
    [[nodiscard]] std::vector<Handle> get_outgoing(Handle h) const;
    [[nodiscard]] size_t get_arity(Handle h) const;

    [[nodiscard]] TruthValue get_tv(Handle h) const noexcept;
    void set_tv(Handle h, TruthValue tv);

    [[nodiscard]] AttentionValue get_av(Handle h) const noexcept;
    void set_av(Handle h, AttentionValue av);

    // ========================================================================
    // Incoming Set
    // ========================================================================

    /**
     * @brief Get all links pointing to this atom
     */
    [[nodiscard]] std::vector<Handle> get_incoming(Handle h) const;

    /**
     * @brief Get incoming links of a specific type
     */
    [[nodiscard]] std::vector<Handle> get_incoming_by_type(Handle h, AtomType type) const;

    // ========================================================================
    // Type-Based Queries
    // ========================================================================

    /**
     * @brief Get all atoms of a given type
     */
    [[nodiscard]] std::vector<Handle> get_atoms_by_type(AtomType type) const;

    /**
     * @brief Get count of atoms of a given type
     */
    [[nodiscard]] size_t count_atoms(AtomType type) const;

    // ========================================================================
    // Iteration
    // ========================================================================

    /**
     * @brief Iterate over all atoms
     */
    void for_each_atom(std::function<void(Handle)> fn) const;

    /**
     * @brief Iterate over atoms of a type
     */
    void for_each_atom_of_type(AtomType type, std::function<void(Handle)> fn) const;

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t node_count() const noexcept;
    [[nodiscard]] size_t link_count() const noexcept;

    // ========================================================================
    // Utilities
    // ========================================================================

    /**
     * @brief Clear all atoms
     */
    void clear();

    /**
     * @brief Get a string representation of an atom
     */
    [[nodiscard]] std::string to_string(Handle h) const;

    /**
     * @brief Get short string representation
     */
    [[nodiscard]] std::string to_short_string(Handle h) const;

    // ========================================================================
    // Direct Access (for advanced use)
    // ========================================================================

    [[nodiscard]] AtomTable& atom_table() noexcept { return table_; }
    [[nodiscard]] const AtomTable& atom_table() const noexcept { return table_; }

    [[nodiscard]] IndexManager& indices() noexcept { return indices_; }
    [[nodiscard]] const IndexManager& indices() const noexcept { return indices_; }

private:
    AtomTable table_;
    IndexManager indices_;

    // Helper to convert Handle vector to AtomId span
    [[nodiscard]] std::vector<AtomId> handles_to_ids(std::span<const Handle> handles) const;
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline bool AtomSpace::contains(Handle h) const noexcept {
    return h.valid() && h.space() == this && table_.contains(h.id());
}

inline bool AtomSpace::contains(AtomId id) const noexcept {
    return table_.contains(id);
}

inline Handle AtomSpace::make_handle(AtomId id) const {
    return Handle{id, const_cast<AtomSpace*>(this)};
}

inline AtomType AtomSpace::get_type(Handle h) const noexcept {
    return h.valid() ? table_.get_type(h.id()) : AtomType::INVALID;
}

inline TruthValue AtomSpace::get_tv(Handle h) const noexcept {
    return h.valid() ? table_.get_tv(h.id()) : TruthValue{};
}

inline void AtomSpace::set_tv(Handle h, TruthValue tv) {
    if (h.valid()) table_.set_tv(h.id(), tv);
}

inline AttentionValue AtomSpace::get_av(Handle h) const noexcept {
    return h.valid() ? table_.get_av(h.id()) : AttentionValue{};
}

inline void AtomSpace::set_av(Handle h, AttentionValue av) {
    if (h.valid()) table_.set_av(h.id(), av);
}

inline size_t AtomSpace::size() const noexcept {
    return table_.size();
}

inline size_t AtomSpace::node_count() const noexcept {
    return table_.node_count();
}

inline size_t AtomSpace::link_count() const noexcept {
    return table_.link_count();
}

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Check if a handle represents a node
 */
[[nodiscard]] inline bool is_node(const AtomSpace& as, Handle h) {
    return is_node(as.get_type(h));
}

/**
 * @brief Check if a handle represents a link
 */
[[nodiscard]] inline bool is_link(const AtomSpace& as, Handle h) {
    return is_link(as.get_type(h));
}

} // namespace opencog
