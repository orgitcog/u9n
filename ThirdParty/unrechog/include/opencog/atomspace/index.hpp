#pragma once
/**
 * @file index.hpp
 * @brief Efficient indexing structures for AtomSpace queries
 *
 * Provides O(1) or O(log n) lookup for common query patterns:
 * - By type
 * - By type and target
 * - By incoming atom
 */

#include <opencog/core/types.hpp>

#include <algorithm>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opencog {

// ============================================================================
// Type Index
// ============================================================================

/**
 * @brief Index atoms by their type for fast type-based queries
 *
 * Supports both exact type lookup and subtype queries.
 */
class TypeIndex {
public:
    /**
     * @brief Add an atom to the index
     */
    void insert(AtomType type, AtomId id) {
        std::unique_lock lock(mutex_);
        by_type_[type].push_back(id);
    }

    /**
     * @brief Remove an atom from the index
     */
    void remove(AtomType type, AtomId id) {
        std::unique_lock lock(mutex_);
        auto it = by_type_.find(type);
        if (it != by_type_.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
        }
    }

    /**
     * @brief Get all atoms of a given type
     */
    [[nodiscard]] std::vector<AtomId> get_atoms_by_type(AtomType type) const {
        std::shared_lock lock(mutex_);
        auto it = by_type_.find(type);
        if (it == by_type_.end()) return {};
        return it->second;
    }

    /**
     * @brief Get count of atoms of a given type
     */
    [[nodiscard]] size_t count_type(AtomType type) const {
        std::shared_lock lock(mutex_);
        auto it = by_type_.find(type);
        if (it == by_type_.end()) return 0;
        return it->second.size();
    }

    /**
     * @brief Get all types present in the index
     */
    [[nodiscard]] std::vector<AtomType> get_types() const {
        std::shared_lock lock(mutex_);
        std::vector<AtomType> types;
        types.reserve(by_type_.size());
        for (const auto& [type, _] : by_type_) {
            types.push_back(type);
        }
        return types;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        by_type_.clear();
    }

private:
    std::unordered_map<AtomType, std::vector<AtomId>> by_type_;
    mutable std::shared_mutex mutex_;
};

// ============================================================================
// Target Type Index
// ============================================================================

/**
 * @brief Index links by (link_type, target_atom) for fast incoming queries
 *
 * Answers: "Give me all links of type T that point to atom A"
 */
class TargetTypeIndex {
    struct Key {
        AtomType link_type;
        AtomId target;

        bool operator==(const Key& other) const noexcept {
            return link_type == other.link_type && target == other.target;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            return hash_combine(
                static_cast<uint64_t>(k.link_type),
                k.target.value
            );
        }
    };

public:
    /**
     * @brief Add a link to the index
     * @param link_type Type of the link
     * @param link_id The link's AtomId
     * @param targets All atoms in the link's outgoing set
     */
    void insert(AtomType link_type, AtomId link_id, std::span<const AtomId> targets) {
        std::unique_lock lock(mutex_);
        for (AtomId target : targets) {
            index_[Key{link_type, target}].push_back(link_id);
        }
    }

    /**
     * @brief Remove a link from the index
     */
    void remove(AtomType link_type, AtomId link_id, std::span<const AtomId> targets) {
        std::unique_lock lock(mutex_);
        for (AtomId target : targets) {
            Key key{link_type, target};
            auto it = index_.find(key);
            if (it != index_.end()) {
                auto& vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), link_id), vec.end());
                if (vec.empty()) {
                    index_.erase(it);
                }
            }
        }
    }

    /**
     * @brief Get all links of a type pointing to a target
     */
    [[nodiscard]] std::vector<AtomId> get_links(AtomType link_type, AtomId target) const {
        std::shared_lock lock(mutex_);
        auto it = index_.find(Key{link_type, target});
        if (it == index_.end()) return {};
        return it->second;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        index_.clear();
    }

private:
    std::unordered_map<Key, std::vector<AtomId>, KeyHash> index_;
    mutable std::shared_mutex mutex_;
};

// ============================================================================
// Implication Index
// ============================================================================

/**
 * @brief Specialized index for ImplicationLinks (for forward chaining)
 *
 * Maps: premise_type -> list of ImplicationLinks with that premise type
 */
class ImplicationIndex {
public:
    void insert(AtomId implication, AtomType premise_type) {
        std::unique_lock lock(mutex_);
        by_premise_type_[premise_type].push_back(implication);
    }

    void remove(AtomId implication, AtomType premise_type) {
        std::unique_lock lock(mutex_);
        auto it = by_premise_type_.find(premise_type);
        if (it != by_premise_type_.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), implication), vec.end());
        }
    }

    [[nodiscard]] std::vector<AtomId> get_implications_for(AtomType premise_type) const {
        std::shared_lock lock(mutex_);
        auto it = by_premise_type_.find(premise_type);
        if (it == by_premise_type_.end()) return {};
        return it->second;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        by_premise_type_.clear();
    }

private:
    std::unordered_map<AtomType, std::vector<AtomId>> by_premise_type_;
    mutable std::shared_mutex mutex_;
};

// ============================================================================
// Composite Index Manager
// ============================================================================

/**
 * @brief Manages all indices for an AtomSpace
 */
class IndexManager {
public:
    TypeIndex type_index;
    TargetTypeIndex target_type_index;
    ImplicationIndex implication_index;

    void clear() {
        type_index.clear();
        target_type_index.clear();
        implication_index.clear();
    }
};

} // namespace opencog
