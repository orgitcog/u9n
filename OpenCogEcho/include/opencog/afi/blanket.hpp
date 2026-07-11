#pragma once
/**
 * @file blanket.hpp
 * @brief Markov Blanket Factory — create, merge, validate blankets
 *
 * Provides factory methods for constructing and manipulating Markov blankets
 * that define the statistical boundaries of cognitive districts in the
 * Cognitive City architecture.
 *
 * A valid Markov blanket partitions atoms into four non-overlapping sets:
 * - Sensory states: carry information inward from the environment
 * - Active states: carry influence outward to the environment
 * - Internal states: hidden states within the district
 * - External states: states outside the blanket entirely
 *
 * Design: static factory methods, header-only logic, thin .cpp compilation unit.
 */

#include <opencog/afi/types.hpp>
#include <opencog/core/types.hpp>

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace opencog::afi {

// ============================================================================
// Markov Blanket Factory
// ============================================================================

/**
 * @brief Factory for creating, merging, and validating Markov blankets
 *
 * All methods are static. MarkovBlanketFactory is a utility class
 * with no instance state.
 */
class MarkovBlanketFactory {
public:
    MarkovBlanketFactory() = delete;

    // -----------------------------------------------------------------------
    // Creation
    // -----------------------------------------------------------------------

    /**
     * @brief Create a blanket from categorized atom sets
     *
     * Moves the provided vectors into the blanket struct.
     * Caller is responsible for ensuring no overlaps (use validate()
     * to check post-hoc).
     */
    [[nodiscard]] static MarkovBlanket create(
        std::vector<AtomId> sensory,
        std::vector<AtomId> active,
        std::vector<AtomId> internal,
        std::vector<AtomId> external)
    {
        MarkovBlanket blanket;
        blanket.sensory_states  = std::move(sensory);
        blanket.active_states   = std::move(active);
        blanket.internal_states = std::move(internal);
        blanket.external_states = std::move(external);
        return blanket;
    }

    // -----------------------------------------------------------------------
    // Merge
    // -----------------------------------------------------------------------

    /**
     * @brief Merge two blankets when districts couple
     *
     * When two districts form a larger composite district:
     * - Internal states of both become internal states of the merged blanket
     * - Sensory/active states that were shared between the two districts
     *   become internal states (they no longer face the external world)
     * - Remaining sensory/active states form the new blanket boundary
     * - External states are the union of both external sets minus
     *   anything now internal
     *
     * Implementation: concatenation with deduplication. Shared boundary
     * atoms (present in both blankets' sensory or active sets) are
     * promoted to internal states.
     */
    [[nodiscard]] static MarkovBlanket merge(
        const MarkovBlanket& a, const MarkovBlanket& b)
    {
        // Build sets of boundary atoms for each blanket
        std::unordered_set<uint64_t> a_boundary;
        for (const auto& id : a.sensory_states)  a_boundary.insert(id.value);
        for (const auto& id : a.active_states)   a_boundary.insert(id.value);

        std::unordered_set<uint64_t> b_boundary;
        for (const auto& id : b.sensory_states)  b_boundary.insert(id.value);
        for (const auto& id : b.active_states)   b_boundary.insert(id.value);

        // Shared boundary atoms become internal
        std::unordered_set<uint64_t> shared;
        for (auto v : a_boundary) {
            if (b_boundary.count(v)) shared.insert(v);
        }

        MarkovBlanket merged;

        // Internal = union of both internals + shared boundary atoms
        std::unordered_set<uint64_t> internal_set;
        auto add_internals = [&](const std::vector<AtomId>& src) {
            for (const auto& id : src) {
                if (internal_set.insert(id.value).second) {
                    merged.internal_states.push_back(id);
                }
            }
        };
        add_internals(a.internal_states);
        add_internals(b.internal_states);
        for (auto v : shared) {
            if (internal_set.insert(v).second) {
                merged.internal_states.push_back(AtomId{v});
            }
        }

        // Sensory = union of sensory minus shared
        std::unordered_set<uint64_t> sensory_set;
        auto add_sensory = [&](const std::vector<AtomId>& src) {
            for (const auto& id : src) {
                if (!shared.count(id.value) && !internal_set.count(id.value)
                    && sensory_set.insert(id.value).second) {
                    merged.sensory_states.push_back(id);
                }
            }
        };
        add_sensory(a.sensory_states);
        add_sensory(b.sensory_states);

        // Active = union of active minus shared
        std::unordered_set<uint64_t> active_set;
        auto add_active = [&](const std::vector<AtomId>& src) {
            for (const auto& id : src) {
                if (!shared.count(id.value) && !internal_set.count(id.value)
                    && active_set.insert(id.value).second) {
                    merged.active_states.push_back(id);
                }
            }
        };
        add_active(a.active_states);
        add_active(b.active_states);

        // External = union of both externals minus anything now internal/boundary
        std::unordered_set<uint64_t> external_set;
        auto add_external = [&](const std::vector<AtomId>& src) {
            for (const auto& id : src) {
                if (!internal_set.count(id.value)
                    && !sensory_set.count(id.value)
                    && !active_set.count(id.value)
                    && external_set.insert(id.value).second) {
                    merged.external_states.push_back(id);
                }
            }
        };
        add_external(a.external_states);
        add_external(b.external_states);

        return merged;
    }

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------

    /**
     * @brief Validate a blanket: no overlaps between state partitions
     *
     * A valid blanket has four disjoint sets. Returns true if no atom
     * appears in more than one partition.
     */
    [[nodiscard]] static bool validate(const MarkovBlanket& blanket) noexcept {
        std::unordered_set<uint64_t> seen;

        auto check_partition = [&](const std::vector<AtomId>& partition) -> bool {
            for (const auto& id : partition) {
                if (!seen.insert(id.value).second) {
                    return false;  // Duplicate found
                }
            }
            return true;
        };

        return check_partition(blanket.sensory_states)
            && check_partition(blanket.active_states)
            && check_partition(blanket.internal_states)
            && check_partition(blanket.external_states);
    }
};

} // namespace opencog::afi
