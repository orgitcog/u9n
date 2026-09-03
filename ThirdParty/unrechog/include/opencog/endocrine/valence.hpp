#pragma once
/**
 * @file valence.hpp
 * @brief Valence-tagged episodic memory — AtomSpace-native implementation
 *
 * ValenceMemory maintains external parallel SoA storage for valence signatures
 * indexed by AtomId. Episodes are subgraphs in AtomSpace with ValenceSignature
 * annotations. Fuzzy retrieval uses the pattern matcher with partial matching.
 */

#include <opencog/endocrine/types.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/pattern/matcher.hpp>
#include <opencog/pattern/generator.hpp>

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace opencog::endo {

// ============================================================================
// ValenceMemory — External parallel storage for valence signatures
// ============================================================================

class ValenceMemory {
public:
    explicit ValenceMemory(AtomSpace& space) : space_(space) {}

    // ========================================================================
    // Valence Operations
    // ========================================================================

    /// Tag an atom with a valence signature
    void set_valence(AtomId id, ValenceSignature vs) {
        auto idx = id.index();
        std::unique_lock lock(valence_mutex_);
        ensure_capacity(idx);
        valence_values_[idx] = vs;
        has_valence_[idx] = true;
    }

    /// Retrieve valence for an atom (returns neutral if unset)
    [[nodiscard]] ValenceSignature get_valence(AtomId id) const {
        auto idx = id.index();
        std::shared_lock lock(valence_mutex_);
        if (idx < valence_values_.size() && has_valence_[idx]) {
            return valence_values_[idx];
        }
        return ValenceSignature::neutral();
    }

    /// Check if an atom has a valence annotation
    [[nodiscard]] bool has_valence(AtomId id) const {
        auto idx = id.index();
        std::shared_lock lock(valence_mutex_);
        return idx < has_valence_.size() && has_valence_[idx];
    }

    // ========================================================================
    // Episode Construction
    // ========================================================================

    /**
     * @brief Create a new episode from atoms with valence annotations
     *
     * Constructs an EpisodeNode and EpisodeLinks in AtomSpace, and
     * stores valence signatures for each member atom.
     */
    [[nodiscard]] Handle create_episode(
        std::string_view name,
        std::span<const std::pair<Handle, ValenceSignature>> members,
        ValenceSignature overall_valence)
    {
        // Create the episode node
        Handle episode = space_.add_node(AtomType::EPISODE_NODE, name,
            TruthValue{std::abs(overall_valence.valence), overall_valence.arousal});

        // Tag the episode node with its overall valence
        set_valence(episode.id(), overall_valence);

        // Link each member atom
        for (auto& [member, vs] : members) {
            Handle link = space_.add_link(AtomType::EPISODE_LINK,
                {episode, member},
                TruthValue{std::abs(vs.valence), vs.arousal});
            set_valence(member.id(), vs);
        }

        // Track the episode
        {
            std::unique_lock lock(valence_mutex_);
            episode_ids_.push_back(episode.id());
            episode_timestamps_.push_back(tick_counter_);
        }

        return episode;
    }

    /// Add an atom to an existing episode
    void add_to_episode(Handle episode, Handle atom, ValenceSignature vs) {
        space_.add_link(AtomType::EPISODE_LINK,
            {episode, atom},
            TruthValue{std::abs(vs.valence), vs.arousal});
        set_valence(atom.id(), vs);
    }

    // ========================================================================
    // Fuzzy Retrieval
    // ========================================================================

    /// Scored episode match result
    struct EpisodeMatch {
        Handle episode;
        float similarity;     ///< Overall similarity score [0, 1]
        ValenceSignature valence; ///< Episode's valence
        float temporal_weight;   ///< Recency weighting
    };

    /**
     * @brief Retrieve episodes similar to the given situation atoms
     *
     * Scores each episode by:
     * 1. Fraction of situation atoms present in the episode
     * 2. Valence similarity
     * 3. Temporal weighting (recent stronger, but intense old memories persist)
     */
    [[nodiscard]] std::vector<EpisodeMatch> retrieve_similar(
        std::span<const Handle> situation_atoms,
        float min_similarity = 0.3f,
        size_t max_results = 20) const
    {
        std::vector<EpisodeMatch> results;
        std::shared_lock lock(valence_mutex_);

        for (size_t i = 0; i < episode_ids_.size(); ++i) {
            AtomId ep_id = episode_ids_[i];
            Handle episode(ep_id, &space_);

            // Get episode members via incoming EpisodeLinks
            auto incoming = space_.get_incoming(episode);
            std::vector<AtomId> member_ids;
            for (auto& link_handle : incoming) {
                // Check it's an EpisodeLink with this episode as first element
                member_ids.push_back(link_handle.id());
            }

            // Score: fraction of situation atoms matched
            size_t matches = 0;
            for (auto& sit_atom : situation_atoms) {
                // Check if this situation atom appears in the episode's links
                auto sit_incoming = space_.get_incoming(sit_atom);
                for (auto& link : sit_incoming) {
                    if (space_.get_type(link) == AtomType::EPISODE_LINK) {
                        auto outgoing = space_.get_outgoing(link);
                        if (!outgoing.empty() && outgoing[0].id() == ep_id) {
                            matches++;
                            break;
                        }
                    }
                }
            }

            if (situation_atoms.empty()) continue;
            float frac = static_cast<float>(matches) / static_cast<float>(situation_atoms.size());
            if (frac < min_similarity) continue;

            ValenceSignature ep_vs = get_valence_unlocked(ep_id);
            float tw = compute_temporal_weight(i);
            float score = frac * tw;

            results.push_back({episode, score, ep_vs, tw});
        }

        // Sort by score descending
        std::sort(results.begin(), results.end(),
            [](const EpisodeMatch& a, const EpisodeMatch& b) {
                return a.similarity > b.similarity;
            });

        if (results.size() > max_results) {
            results.resize(max_results);
        }

        return results;
    }

    /**
     * @brief Retrieve episodes by valence range
     */
    [[nodiscard]] std::vector<Handle> retrieve_by_valence(
        float min_valence, float max_valence,
        float min_arousal = 0.0f) const
    {
        std::vector<Handle> results;
        std::shared_lock lock(valence_mutex_);

        for (auto& ep_id : episode_ids_) {
            ValenceSignature vs = get_valence_unlocked(ep_id);
            if (vs.valence >= min_valence && vs.valence <= max_valence
                && vs.arousal >= min_arousal)
            {
                results.emplace_back(ep_id, &space_);
            }
        }
        return results;
    }

    // ========================================================================
    // Temporal Management
    // ========================================================================

    /// Advance the tick counter (call each update cycle)
    void tick() { tick_counter_++; }

    /// Compute temporal weight for an episode (recent = stronger)
    [[nodiscard]] float temporal_weight(Handle episode) const {
        std::shared_lock lock(valence_mutex_);
        for (size_t i = 0; i < episode_ids_.size(); ++i) {
            if (episode_ids_[i] == episode.id()) {
                return compute_temporal_weight(i);
            }
        }
        return 0.0f;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t episode_count() const {
        std::shared_lock lock(valence_mutex_);
        return episode_ids_.size();
    }

    [[nodiscard]] size_t valenced_atom_count() const {
        std::shared_lock lock(valence_mutex_);
        size_t count = 0;
        for (bool b : has_valence_) if (b) count++;
        return count;
    }

private:
    AtomSpace& space_;

    // External SoA storage for valence (parallel to AtomTable)
    mutable std::shared_mutex valence_mutex_;
    std::vector<ValenceSignature> valence_values_;
    std::vector<bool> has_valence_;

    // Episode tracking
    std::vector<AtomId> episode_ids_;
    std::vector<size_t> episode_timestamps_; // Tick when episode was created
    size_t tick_counter_{0};

    void ensure_capacity(size_t idx) {
        if (idx >= valence_values_.size()) {
            valence_values_.resize(idx + 1);
            has_valence_.resize(idx + 1, false);
        }
    }

    /// Get valence without acquiring lock (caller must hold lock)
    [[nodiscard]] ValenceSignature get_valence_unlocked(AtomId id) const {
        auto idx = id.index();
        if (idx < valence_values_.size() && has_valence_[idx]) {
            return valence_values_[idx];
        }
        return ValenceSignature::neutral();
    }

    /// Temporal weight: exponential decay with arousal-based persistence
    [[nodiscard]] float compute_temporal_weight(size_t episode_index) const {
        if (episode_index >= episode_timestamps_.size()) return 0.0f;

        size_t age = tick_counter_ - episode_timestamps_[episode_index];
        float base_decay = std::exp(-static_cast<float>(age) * 0.001f);

        // High-arousal memories resist temporal decay (like trauma/joy)
        ValenceSignature vs = get_valence_unlocked(episode_ids_[episode_index]);
        float arousal_persistence = 1.0f + vs.arousal * 2.0f;

        return std::min(1.0f, base_decay * arousal_persistence);
    }
};

} // namespace opencog::endo
