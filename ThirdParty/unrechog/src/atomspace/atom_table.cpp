/**
 * @file atom_table.cpp
 * @brief AtomTable implementation
 */

#include <opencog/atomspace/atom_table.hpp>

#include <algorithm>
#include <stdexcept>

namespace opencog {

AtomTable::AtomTable() {
    // Reserve initial capacity
    headers_.reserve(INITIAL_CAPACITY);
    truth_values_.reserve(INITIAL_CAPACITY);
    attention_values_.reserve(INITIAL_CAPACITY);
    generations_.reserve(INITIAL_CAPACITY);
    node_data_.reserve(INITIAL_CAPACITY);
    link_data_.reserve(INITIAL_CAPACITY);
    incoming_sets_.reserve(INITIAL_CAPACITY);
}

AtomTable::~AtomTable() = default;

// ============================================================================
// Hash Computation
// ============================================================================

uint64_t AtomTable::compute_node_hash(AtomType type, std::string_view name) const {
    uint64_t h = static_cast<uint64_t>(type);
    for (char c : name) {
        h = hash_combine(h, static_cast<uint64_t>(c));
    }
    return h;
}

uint64_t AtomTable::compute_link_hash(AtomType type, std::span<const AtomId> outgoing) const {
    uint64_t h = static_cast<uint64_t>(type);
    for (AtomId id : outgoing) {
        h = hash_combine(h, id.value);
    }
    return h;
}

// ============================================================================
// Slot Management
// ============================================================================

AtomId AtomTable::allocate_slot() {
    if (!free_slots_.empty()) {
        uint64_t slot = free_slots_.back();
        free_slots_.pop_back();

        // Increment generation to invalidate old references
        uint16_t gen = ++generations_[slot];
        return AtomId::make(slot, gen);
    }

    // Need new slot
    uint64_t slot = headers_.size();
    headers_.push_back({});
    truth_values_.push_back({});
    attention_values_.push_back({});
    generations_.push_back(1);
    node_data_.push_back(nullptr);
    link_data_.push_back(nullptr);
    incoming_sets_.push_back({});

    return AtomId::make(slot, 1);
}

void AtomTable::free_slot(AtomId id) {
    if (!is_valid_slot(id)) return;

    uint64_t slot = id.index();
    headers_[slot].type = AtomType::INVALID;
    node_data_[slot].reset();
    link_data_[slot].reset();
    incoming_sets_[slot].clear();

    free_slots_.push_back(slot);
}

// ============================================================================
// Atom Creation
// ============================================================================

AtomId AtomTable::add_node(AtomType type, std::string_view name, TruthValue tv) {
    if (!is_node(type)) {
        throw std::invalid_argument("Type must be a node type");
    }

    uint64_t hash = compute_node_hash(type, name);

    // Check for existing atom
    {
        std::shared_lock lock(global_mutex_);
        auto it = hash_index_.find(hash);
        if (it != hash_index_.end() && is_valid_slot(it->second)) {
            // Verify it's the same atom (handle hash collisions)
            auto& data = node_data_[it->second.index()];
            if (data && data->name == name &&
                headers_[it->second.index()].type == type) {
                return it->second;
            }
        }
    }

    // Create new atom
    std::unique_lock lock(global_mutex_);

    // Double-check after acquiring write lock
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end() && is_valid_slot(it->second)) {
        auto& data = node_data_[it->second.index()];
        if (data && data->name == name &&
            headers_[it->second.index()].type == type) {
            return it->second;
        }
    }

    AtomId id = allocate_slot();
    uint64_t slot = id.index();

    headers_[slot] = {type, 0, 0, hash};
    truth_values_[slot] = tv;
    attention_values_[slot] = AttentionValue::default_av();

    node_data_[slot] = std::make_unique<NodeData>();
    node_data_[slot]->name = std::string(name);

    hash_index_[hash] = id;

    atom_count_.fetch_add(1, std::memory_order_relaxed);
    node_count_.fetch_add(1, std::memory_order_relaxed);

    return id;
}

AtomId AtomTable::add_link(AtomType type, std::span<const AtomId> outgoing, TruthValue tv) {
    if (!is_link(type)) {
        throw std::invalid_argument("Type must be a link type");
    }

    // Validate outgoing atoms exist
    for (AtomId id : outgoing) {
        if (!contains(id)) {
            throw std::invalid_argument("Outgoing atom does not exist");
        }
    }

    uint64_t hash = compute_link_hash(type, outgoing);

    // Check for existing atom
    {
        std::shared_lock lock(global_mutex_);
        auto it = hash_index_.find(hash);
        if (it != hash_index_.end() && is_valid_slot(it->second)) {
            auto& data = link_data_[it->second.index()];
            if (data && data->outgoing.size() == outgoing.size() &&
                headers_[it->second.index()].type == type) {
                bool match = true;
                for (size_t i = 0; i < outgoing.size(); ++i) {
                    if (data->outgoing[i] != outgoing[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) return it->second;
            }
        }
    }

    // Create new atom
    std::unique_lock lock(global_mutex_);

    // Double-check
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end() && is_valid_slot(it->second)) {
        auto& data = link_data_[it->second.index()];
        if (data && data->outgoing.size() == outgoing.size() &&
            headers_[it->second.index()].type == type) {
            bool match = true;
            for (size_t i = 0; i < outgoing.size(); ++i) {
                if (data->outgoing[i] != outgoing[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return it->second;
        }
    }

    AtomId id = allocate_slot();
    uint64_t slot = id.index();

    headers_[slot] = {type, 0, 0, hash};
    truth_values_[slot] = tv;
    attention_values_[slot] = AttentionValue::default_av();

    link_data_[slot] = std::make_unique<LinkData>();
    link_data_[slot]->outgoing.assign(outgoing.begin(), outgoing.end());

    hash_index_[hash] = id;

    // Update incoming sets
    for (AtomId target : outgoing) {
        add_to_incoming(target, id);
    }

    atom_count_.fetch_add(1, std::memory_order_relaxed);
    link_count_.fetch_add(1, std::memory_order_relaxed);

    return id;
}

// ============================================================================
// Atom Removal
// ============================================================================

bool AtomTable::remove_atom(AtomId id, bool recursive) {
    if (!is_valid_slot(id)) return false;

    std::unique_lock lock(global_mutex_);

    uint64_t slot = id.index();

    // Check for incoming links
    if (!incoming_sets_[slot].empty()) {
        if (!recursive) {
            return false;  // Can't remove - has incoming links
        }

        // Recursively remove incoming links first
        auto incoming = incoming_sets_[slot];  // Copy to avoid iterator invalidation
        for (AtomId link_id : incoming) {
            remove_atom(link_id, true);
        }
    }

    // Remove from outgoing atoms' incoming sets
    if (link_data_[slot]) {
        for (AtomId target : link_data_[slot]->outgoing) {
            remove_from_incoming(target, id);
        }
    }

    // Remove from hash index
    uint64_t hash = headers_[slot].hash;
    hash_index_.erase(hash);

    // Update statistics
    if (is_node(headers_[slot].type)) {
        node_count_.fetch_sub(1, std::memory_order_relaxed);
    } else {
        link_count_.fetch_sub(1, std::memory_order_relaxed);
    }
    atom_count_.fetch_sub(1, std::memory_order_relaxed);

    // Free the slot
    free_slot(id);

    return true;
}

// ============================================================================
// Atom Lookup
// ============================================================================

AtomId AtomTable::get_node(AtomType type, std::string_view name) const {
    uint64_t hash = compute_node_hash(type, name);

    std::shared_lock lock(global_mutex_);
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end() && is_valid_slot(it->second)) {
        auto& data = node_data_[it->second.index()];
        if (data && data->name == name &&
            headers_[it->second.index()].type == type) {
            return it->second;
        }
    }
    return ATOM_NULL;
}

AtomId AtomTable::get_link(AtomType type, std::span<const AtomId> outgoing) const {
    uint64_t hash = compute_link_hash(type, outgoing);

    std::shared_lock lock(global_mutex_);
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end() && is_valid_slot(it->second)) {
        auto& data = link_data_[it->second.index()];
        if (data && data->outgoing.size() == outgoing.size() &&
            headers_[it->second.index()].type == type) {
            bool match = true;
            for (size_t i = 0; i < outgoing.size(); ++i) {
                if (data->outgoing[i] != outgoing[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return it->second;
        }
    }
    return ATOM_NULL;
}

// ============================================================================
// Cold Path Accessors
// ============================================================================

std::string_view AtomTable::get_name(AtomId id) const {
    if (!is_valid_slot(id)) return {};

    std::shared_lock lock(global_mutex_);
    auto& data = node_data_[id.index()];
    if (data) {
        return data->name;
    }
    return {};
}

std::span<const AtomId> AtomTable::get_outgoing(AtomId id) const {
    if (!is_valid_slot(id)) return {};

    std::shared_lock lock(global_mutex_);
    auto& data = link_data_[id.index()];
    if (data) {
        return data->outgoing;
    }
    return {};
}

size_t AtomTable::get_arity(AtomId id) const {
    if (!is_valid_slot(id)) return 0;

    std::shared_lock lock(global_mutex_);
    auto& data = link_data_[id.index()];
    if (data) {
        return data->outgoing.size();
    }
    return 0;
}

// ============================================================================
// Incoming Set Management
// ============================================================================

void AtomTable::add_to_incoming(AtomId target, AtomId link) {
    if (!is_valid_slot(target)) return;
    incoming_sets_[target.index()].push_back(link);
    headers_[target.index()].incoming_count++;
}

void AtomTable::remove_from_incoming(AtomId target, AtomId link) {
    if (!is_valid_slot(target)) return;
    auto& incoming = incoming_sets_[target.index()];
    incoming.erase(
        std::remove(incoming.begin(), incoming.end(), link),
        incoming.end()
    );
    if (headers_[target.index()].incoming_count > 0) {
        headers_[target.index()].incoming_count--;
    }
}

std::vector<AtomId> AtomTable::get_incoming(AtomId id) const {
    if (!is_valid_slot(id)) return {};

    std::shared_lock lock(global_mutex_);
    return incoming_sets_[id.index()];
}

size_t AtomTable::get_incoming_size(AtomId id) const noexcept {
    if (!is_valid_slot(id)) return 0;
    return headers_[id.index()].incoming_count;
}

} // namespace opencog
