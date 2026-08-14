#pragma once
/**
 * @file moral.hpp
 * @brief Moral perception engine — valence-mediated moral sensing
 *
 * Key innovation: moral perception BEFORE moral reasoning.
 *
 * The system detects moral salience in novel situations by fuzzy
 * pattern-matching against accumulated valence-tagged experience.
 * The endocrine system integrates distributed valence signals into
 * coherent felt-states accessible to introspection, enabling:
 *
 * "I recognize this pattern from my own valence memory. If I experienced
 *  negative valence in similar situations, and this other agent appears
 *  to be in a similar situation, they may be experiencing distress."
 */

#include <opencog/endocrine/affect.hpp>

namespace opencog::endo {

// ============================================================================
// Moral Perception Configuration
// ============================================================================

struct MoralConfig {
    float moral_salience_threshold{0.3f};  ///< Min salience to register
    float empathy_weight{0.5f};            ///< Weighting of other-model signals
    float novelty_weight{0.3f};            ///< Weighting of novel moral situations
    float suffering_sensitivity{0.7f};     ///< Sensitivity to negative valence patterns
    bool use_pln_for_inference{true};      ///< Use PLN for empathic inference
};

// ============================================================================
// MoralPerceptionEngine
// ============================================================================

class MoralPerceptionEngine {
public:
    MoralPerceptionEngine(
        AffectiveIntegration& affect,
        ValenceMemory& memory,
        HormoneBus& bus,
        AtomSpace& space)
        : affect_(affect)
        , memory_(memory)
        , bus_(bus)
        , space_(space)
    {}

    // ========================================================================
    // Core Pipeline
    // ========================================================================

    /**
     * @brief Evaluate the moral perception of a situation
     *
     * Pipeline:
     * 1. Compute raw affective signal via AffectiveIntegration
     * 2. Retrieve morally-tagged episodes (negative valence precedents)
     * 3. If other agents involved, run empathic inference
     * 4. Detect novelty (situations with no moral precedent)
     * 5. Integrate into MoralPerception with action bias
     */
    [[nodiscard]] MoralPerception evaluate(
        std::span<const Handle> situation_atoms)
    {
        MoralPerception result;

        // Step 1: Raw affective signal
        result.raw_signal = compute_raw_signal(situation_atoms);

        // Step 2: Retrieve moral associations (primarily negative valence)
        auto associations = retrieve_moral_associations(situation_atoms);
        result.moral_salience = compute_moral_salience(associations, result.raw_signal);

        // Step 3: Empathic inference (if other agents present)
        result.empathic_weight = compute_empathic_signal(situation_atoms, associations);

        // Step 4: Novel moral signal
        result.novel_moral_signal = compute_moral_novelty(situation_atoms, associations);

        // Step 5: Action bias
        result.action_bias = compute_action_bias(result);

        // Modulate endocrine system based on moral perception
        apply_moral_modulation(result);

        return result;
    }

    // ========================================================================
    // Empathic Modeling
    // ========================================================================

    /**
     * @brief Infer another agent's valence state from pattern matching
     *
     * Uses own valence memory as a reference: "situations that felt like X
     * to me may feel similarly to another agent in a similar situation."
     */
    [[nodiscard]] ValenceSignature empathic_inference(
        Handle other_agent_model)
    {
        // Get context atoms linked to the other agent model
        auto incoming = space_.get_incoming(other_agent_model);
        std::vector<Handle> context_atoms;
        for (auto& link : incoming) {
            auto outgoing = space_.get_outgoing(link);
            for (auto& atom : outgoing) {
                if (atom.id() != other_agent_model.id()) {
                    context_atoms.push_back(atom);
                }
            }
        }

        if (context_atoms.empty()) return ValenceSignature::neutral();

        // Run the same valence retrieval pipeline on the other's context
        auto matches = memory_.retrieve_similar(context_atoms, 0.2f, 20);

        float total_w = 0.0f;
        float weighted_v = 0.0f;
        float weighted_a = 0.0f;

        for (auto& match : matches) {
            float w = match.similarity * match.temporal_weight;
            weighted_v += match.valence.valence * w;
            weighted_a += match.valence.arousal * w;
            total_w += w;
        }

        if (total_w > 0.0f) {
            return {weighted_v / total_w,
                    std::clamp(weighted_a / total_w, 0.0f, 1.0f)};
        }
        return ValenceSignature::neutral();
    }

    // ========================================================================
    // Introspection
    // ========================================================================

    /**
     * @brief Externalize a moral perception as atoms for self-reflection
     */
    [[nodiscard]] Handle externalize_moral_perception(
        const MoralPerception& perception)
    {
        std::string name = "moral_perception_" + std::to_string(bus_.tick_count());
        Handle node = space_.add_node(AtomType::MORAL_SIGNAL_NODE, name,
            TruthValue{perception.moral_salience, perception.empathic_weight});

        memory_.set_valence(node.id(), perception.raw_signal);
        return node;
    }

    // ========================================================================
    // Learning
    // ========================================================================

    /**
     * @brief Record a moral outcome to build valence memory
     *
     * After experiencing or observing a moral situation, record the
     * outcome with its valence for future moral perception.
     */
    void record_moral_outcome(
        std::span<const Handle> situation_atoms,
        ValenceSignature experienced_valence,
        float moral_weight = 1.0f)
    {
        // Create valence-tagged members
        std::vector<std::pair<Handle, ValenceSignature>> members;
        members.reserve(situation_atoms.size());
        for (auto& atom : situation_atoms) {
            // Each member gets a weighted version of the overall valence
            ValenceSignature member_vs = {
                experienced_valence.valence * moral_weight,
                experienced_valence.arousal * moral_weight
            };
            members.emplace_back(atom, member_vs);
        }

        // Create the episode
        std::string name = "moral_episode_" + std::to_string(bus_.tick_count());
        memory_.create_episode(name, members, experienced_valence);
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(MoralConfig config) { config_ = config; }
    [[nodiscard]] const MoralConfig& config() const noexcept { return config_; }

private:
    AffectiveIntegration& affect_;
    ValenceMemory& memory_;
    HormoneBus& bus_;
    AtomSpace& space_;
    MoralConfig config_;

    // Step 1: Raw signal from felt-sense
    [[nodiscard]] ValenceSignature compute_raw_signal(
        std::span<const Handle> situation_atoms)
    {
        FeltSense sense = affect_.compute_felt_sense(situation_atoms);
        return sense.aggregate_valence;
    }

    // Step 2: Retrieve episodes with moral significance (negative valence)
    [[nodiscard]] std::vector<ValenceMemory::EpisodeMatch> retrieve_moral_associations(
        std::span<const Handle> situation_atoms)
    {
        auto all_matches = memory_.retrieve_similar(situation_atoms, 0.2f, 50);

        // Filter for morally salient episodes (significant valence)
        std::vector<ValenceMemory::EpisodeMatch> moral_matches;
        for (auto& match : all_matches) {
            if (match.valence.is_salient(config_.moral_salience_threshold)) {
                moral_matches.push_back(match);
            }
        }
        return moral_matches;
    }

    // Compute moral salience from associations and raw signal
    [[nodiscard]] float compute_moral_salience(
        const std::vector<ValenceMemory::EpisodeMatch>& associations,
        const ValenceSignature& raw_signal)
    {
        if (associations.empty()) {
            // No precedent — salience comes from raw signal alone
            return raw_signal.is_salient() ? raw_signal.magnitude() * 0.5f : 0.0f;
        }

        float salience = 0.0f;
        for (auto& match : associations) {
            // Weight by similarity, temporal recency, and negativity
            float negativity = std::max(0.0f, -match.valence.valence);
            salience += match.similarity * match.temporal_weight
                        * (negativity * config_.suffering_sensitivity
                           + match.valence.arousal * 0.3f);
        }

        return std::clamp(salience, 0.0f, 1.0f);
    }

    // Compute empathic signal (detecting potential distress in others)
    [[nodiscard]] float compute_empathic_signal(
        std::span<const Handle> situation_atoms,
        const std::vector<ValenceMemory::EpisodeMatch>& associations)
    {
        if (associations.empty()) return 0.0f;

        // The empathic signal is the degree to which matched precedents
        // involved negative valence — "I remember situations like this
        // being painful, so others in this situation may be suffering"
        float empathic = 0.0f;
        float total_weight = 0.0f;
        for (auto& match : associations) {
            float w = match.similarity * match.temporal_weight;
            float suffering = std::max(0.0f, -match.valence.valence) * match.valence.arousal;
            empathic += suffering * w;
            total_weight += w;
        }

        if (total_weight > 0.0f) {
            empathic /= total_weight;
        }

        return std::clamp(empathic * config_.empathy_weight, 0.0f, 1.0f);
    }

    // Detect novelty of moral situation
    [[nodiscard]] float compute_moral_novelty(
        std::span<const Handle> situation_atoms,
        const std::vector<ValenceMemory::EpisodeMatch>& associations)
    {
        if (associations.empty()) return 1.0f; // Completely novel

        // Novelty is inverse of best match
        float best = 0.0f;
        for (auto& match : associations) {
            best = std::max(best, match.similarity);
        }

        return (1.0f - best) * config_.novelty_weight + config_.novelty_weight;
    }

    // Determine action bias from integrated moral signal
    [[nodiscard]] CognitiveMode compute_action_bias(
        const MoralPerception& perception)
    {
        // High moral salience + high empathy → social/protective mode
        if (perception.moral_salience > 0.5f && perception.empathic_weight > 0.3f) {
            return CognitiveMode::SOCIAL;
        }

        // High moral salience + negative raw signal → threat/vigilant mode
        if (perception.moral_salience > 0.5f && perception.raw_signal.valence < -0.3f) {
            return CognitiveMode::VIGILANT;
        }

        // High novelty → reflective mode (need careful deliberation)
        if (perception.novel_moral_signal > 0.6f) {
            return CognitiveMode::REFLECTIVE;
        }

        // Low moral salience → current mode
        return bus_.current_mode();
    }

    // Apply moral perception to endocrine system
    void apply_moral_modulation(const MoralPerception& perception) {
        // High moral salience with suffering detected → prosocial hormones
        if (perception.moral_salience > config_.moral_salience_threshold) {
            // Oxytocin: prosocial/protective response
            bus_.produce(HormoneId::OXYTOCIN,
                perception.moral_salience * perception.empathic_weight * 0.2f);

            // Cortisol: empathic distress
            if (perception.raw_signal.valence < -0.3f) {
                bus_.produce(HormoneId::CRH,
                    std::abs(perception.raw_signal.valence) * 0.1f);
            }

            // Norepinephrine: moral alertness
            bus_.produce(HormoneId::NOREPINEPHRINE,
                perception.moral_salience * 0.1f);
        }

        // Novel moral situations → increased deliberation
        if (perception.novel_moral_signal > 0.5f) {
            bus_.produce(HormoneId::SEROTONIN, 0.05f); // Patience for deliberation
        }
    }
};

} // namespace opencog::endo
