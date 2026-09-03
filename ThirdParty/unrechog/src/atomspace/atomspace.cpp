/**
 * @file atomspace.cpp
 * @brief AtomSpace implementation
 */

#include <opencog/atomspace/atomspace.hpp>

#include <sstream>

namespace opencog {

AtomSpace::AtomSpace() = default;
AtomSpace::~AtomSpace() = default;

// ============================================================================
// Atom Creation
// ============================================================================

Handle AtomSpace::add_node(AtomType type, std::string_view name, TruthValue tv) {
    AtomId id = table_.add_node(type, name, tv);
    indices_.type_index.insert(type, id);
    return Handle{id, this};
}

Handle AtomSpace::add_link(AtomType type, std::initializer_list<Handle> outgoing, TruthValue tv) {
    std::vector<AtomId> ids;
    ids.reserve(outgoing.size());
    for (const Handle& h : outgoing) {
        ids.push_back(h.id());
    }
    return add_link(type, std::span<const AtomId>(ids), tv);
}

Handle AtomSpace::add_link(AtomType type, std::span<const Handle> outgoing, TruthValue tv) {
    std::vector<AtomId> ids = handles_to_ids(outgoing);
    return add_link(type, std::span<const AtomId>(ids), tv);
}

Handle AtomSpace::add_link(AtomType type, std::span<const AtomId> outgoing, TruthValue tv) {
    AtomId id = table_.add_link(type, outgoing, tv);
    indices_.type_index.insert(type, id);
    indices_.target_type_index.insert(type, id, outgoing);

    // Special indexing for ImplicationLinks (useful for forward chaining)
    if (type == AtomType::IMPLICATION_LINK && outgoing.size() >= 2) {
        AtomType premise_type = table_.get_type(outgoing[0]);
        indices_.implication_index.insert(id, premise_type);
    }

    return Handle{id, this};
}

// ============================================================================
// Atom Removal
// ============================================================================

bool AtomSpace::remove(Handle h, bool recursive) {
    if (!contains(h)) return false;
    return remove(h.id(), recursive);
}

bool AtomSpace::remove(AtomId id, bool recursive) {
    if (!table_.contains(id)) return false;

    AtomType type = table_.get_type(id);
    auto outgoing = table_.get_outgoing(id);

    // Remove from indices first
    indices_.type_index.remove(type, id);
    if (is_link(type)) {
        indices_.target_type_index.remove(type, id, outgoing);

        if (type == AtomType::IMPLICATION_LINK && outgoing.size() >= 2) {
            AtomType premise_type = table_.get_type(outgoing[0]);
            indices_.implication_index.remove(id, premise_type);
        }
    }

    return table_.remove_atom(id, recursive);
}

// ============================================================================
// Atom Lookup
// ============================================================================

Handle AtomSpace::get_node(AtomType type, std::string_view name) const {
    AtomId id = table_.get_node(type, name);
    return id.valid() ? Handle{id, const_cast<AtomSpace*>(this)} : Handle{};
}

Handle AtomSpace::get_link(AtomType type, std::initializer_list<Handle> outgoing) const {
    std::vector<AtomId> ids;
    ids.reserve(outgoing.size());
    for (const Handle& h : outgoing) {
        ids.push_back(h.id());
    }
    AtomId id = table_.get_link(type, ids);
    return id.valid() ? Handle{id, const_cast<AtomSpace*>(this)} : Handle{};
}

// ============================================================================
// Property Access
// ============================================================================

std::string_view AtomSpace::get_name(Handle h) const {
    return h.valid() ? table_.get_name(h.id()) : std::string_view{};
}

std::vector<Handle> AtomSpace::get_outgoing(Handle h) const {
    if (!h.valid()) return {};

    auto outgoing = table_.get_outgoing(h.id());
    std::vector<Handle> result;
    result.reserve(outgoing.size());
    for (AtomId id : outgoing) {
        result.emplace_back(id, const_cast<AtomSpace*>(this));
    }
    return result;
}

size_t AtomSpace::get_arity(Handle h) const {
    return h.valid() ? table_.get_arity(h.id()) : 0;
}

// ============================================================================
// Incoming Set
// ============================================================================

std::vector<Handle> AtomSpace::get_incoming(Handle h) const {
    if (!h.valid()) return {};

    auto incoming = table_.get_incoming(h.id());
    std::vector<Handle> result;
    result.reserve(incoming.size());
    for (AtomId id : incoming) {
        result.emplace_back(id, const_cast<AtomSpace*>(this));
    }
    return result;
}

std::vector<Handle> AtomSpace::get_incoming_by_type(Handle h, AtomType type) const {
    if (!h.valid()) return {};

    auto all = indices_.target_type_index.get_links(type, h.id());
    std::vector<Handle> result;
    result.reserve(all.size());
    for (AtomId id : all) {
        result.emplace_back(id, const_cast<AtomSpace*>(this));
    }
    return result;
}

// ============================================================================
// Type-Based Queries
// ============================================================================

std::vector<Handle> AtomSpace::get_atoms_by_type(AtomType type) const {
    auto ids = indices_.type_index.get_atoms_by_type(type);
    std::vector<Handle> result;
    result.reserve(ids.size());
    for (AtomId id : ids) {
        result.emplace_back(id, const_cast<AtomSpace*>(this));
    }
    return result;
}

size_t AtomSpace::count_atoms(AtomType type) const {
    return indices_.type_index.count_type(type);
}

// ============================================================================
// Iteration
// ============================================================================

void AtomSpace::for_each_atom(std::function<void(Handle)> fn) const {
    for (auto type : indices_.type_index.get_types()) {
        for_each_atom_of_type(type, fn);
    }
}

void AtomSpace::for_each_atom_of_type(AtomType type, std::function<void(Handle)> fn) const {
    for (AtomId id : indices_.type_index.get_atoms_by_type(type)) {
        fn(Handle{id, const_cast<AtomSpace*>(this)});
    }
}

// ============================================================================
// Utilities
// ============================================================================

void AtomSpace::clear() {
    // Clear indices first
    indices_.clear();

    // Then clear storage (would need to implement in AtomTable)
    // For now, just create a new table
    table_ = AtomTable{};
}

std::string AtomSpace::to_string(Handle h) const {
    if (!h.valid()) return "(invalid)";

    std::ostringstream ss;
    AtomType type = get_type(h);
    TruthValue tv = get_tv(h);

    ss << "(" << type_name(type);

    if (is_node(type)) {
        ss << " \"" << get_name(h) << "\"";
    } else {
        for (const Handle& out : get_outgoing(h)) {
            ss << " " << to_short_string(out);
        }
    }

    ss << ") ; " << tv.strength << " " << tv.confidence;

    return ss.str();
}

std::string AtomSpace::to_short_string(Handle h) const {
    if (!h.valid()) return "(invalid)";

    std::ostringstream ss;
    AtomType type = get_type(h);

    if (is_node(type)) {
        ss << get_name(h);
    } else {
        ss << "(" << type_name(type);
        for (const Handle& out : get_outgoing(h)) {
            ss << " " << to_short_string(out);
        }
        ss << ")";
    }

    return ss.str();
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<AtomId> AtomSpace::handles_to_ids(std::span<const Handle> handles) const {
    std::vector<AtomId> ids;
    ids.reserve(handles.size());
    for (const Handle& h : handles) {
        ids.push_back(h.id());
    }
    return ids;
}

} // namespace opencog
