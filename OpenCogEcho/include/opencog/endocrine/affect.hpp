#pragma once
/**
 * @file affect.hpp
 * @brief Affective integration layer — felt-sense from valence memory
 *
 * Takes attentional focus atoms, retrieves valence + similar episodes,
 * computes a weighted centroid aggregation in (valence, arousal) space,
 * and produces a FeltSense that can modulate the endocrine system
 * and be externalized for introspective reasoning.
 */

#include <opencog/endocrine/valence.hpp>
#include <opencog/endocrine/hormone_bus.hpp>

namespace opencog::endo {

// ============================================================================
// AffectiveIntegration
// ============================================================================

class AffectiveIntegration {
public:
    AffectiveIntegration(ValenceMemory& memory, HormoneBus& bus, AtomSpace& space)
        : memory_(memory), bus_(bus), space_(space)
    {}

    // ========================================================================
    // Core Operation
    // ========================================================================

    /**
     * @brief Compute aggregate felt-sense from current attentional focus
     *
     * Pipeline:
     * 1. For each focus atom, retrieve its direct valence
     * 2. For each focus atom, retrieve similar episodes
     * 3. Weight episode valences by: match * temporal * arousal_persistence
     * 4. Compute weighted centroid in (valence, arousal) space
     * 5. Certainty = total evidence mass
     * 6. Novelty = inverse of best match confidence
     */
    [[nodiscard]] FeltSense compute_felt_sense(
        std::span<const Handle> focus_atoms)
    {
        if (focus_atoms.empty()) return current_sense_;

        float total_weight = 0.0f;
        float weighted_valence = 0.0f;
        float weighted_arousal = 0.0f;
        float best_match = 0.0f;

        // Direct valence from focus atoms
        for (auto& atom : focus_atoms) {
            if (memory_.has_valence(atom.id())) {
                ValenceSignature vs = memory_.get_valence(atom.id());
                float w = vs.arousal + 0.1f; // Weight by intensity
                weighted_valence += vs.valence * w;
                weighted_arousal += vs.arousal * w;
                total_weight += w;
            }
        }

        // Similar episode retrieval
        auto matches = memory_.retrieve_similar(focus_atoms, 0.2f, 50);
        for (auto& match : matches) {
            float w = match.similarity * match.temporal_weight;
            weighted_valence += match.valence.valence * w;
            weighted_arousal += match.valence.arousal * w;
            total_weight += w;
            best_match = std::max(best_match, match.similarity);
        }

        // Compute aggregate
        FeltSense sense;
        if (total_weight > 0.0f) {
            sense.aggregate_valence = {
                weighted_valence / total_weight,
                std::clamp(weighted_arousal / total_weight, 0.0f, 1.0f)
            };
        }
        sense.certainty = std::min(1.0f, total_weight);
        sense.novelty = 1.0f - best_match; // High novelty = no strong precedent
        sense.suggested_mode = compute_suggested_mode(sense.aggregate_valence);

        current_sense_ = sense;

        // Notify listeners
        if (sense_change_cb_) {
            sense_change_cb_(sense);
        }

        return sense;
    }

    // ========================================================================
    // Endocrine Integration
    // ========================================================================

    /**
     * @brief Translate felt-sense into endocrine modulation signals
     *
     * Positive + high arousal → dopamine burst
     * Negative + high arousal → CRH (stress)
     * Positive + low arousal → serotonin boost
     * Negative + low arousal → melatonin/withdrawal
     * High certainty → reduced cortisol (confidence calms)
     * High novelty → norepinephrine spike
     */
    void modulate_endocrine(const FeltSense& sense) {
        float v = sense.aggregate_valence.valence;
        float a = sense.aggregate_valence.arousal;

        // Positive affect with high arousal → reward signal
        if (v > 0.2f && a > 0.4f) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, v * a * 0.3f);
        }

        // Negative affect with high arousal → stress signal
        if (v < -0.2f && a > 0.4f) {
            bus_.produce(HormoneId::CRH, std::abs(v) * a * 0.2f);
        }

        // Positive affect with low arousal → mood boost
        if (v > 0.2f && a < 0.4f) {
            bus_.produce(HormoneId::SEROTONIN, v * 0.1f);
        }

        // High certainty calms
        if (sense.certainty > 0.5f) {
            // Slight cortisol reduction (confidence effect)
            float current_cortisol = bus_.concentration(HormoneId::CORTISOL);
            if (current_cortisol > 0.2f) {
                bus_.set_concentration(HormoneId::CORTISOL,
                    current_cortisol * (1.0f - sense.certainty * 0.05f));
            }
        }

        // High novelty → arousal
        if (sense.novelty > 0.5f) {
            bus_.produce(HormoneId::NOREPINEPHRINE, sense.novelty * 0.15f);
        }
    }

    // ========================================================================
    // Introspective Access
    // ========================================================================

    /**
     * @brief Create atoms representing the current felt-state
     *
     * Externalizes the felt-sense as AtomSpace structures so the system
     * can reason about its own affective state (metacognition).
     */
    [[nodiscard]] Handle externalize_felt_sense(const FeltSense& sense) {
        // Create a FeltSenseNode with TV encoding valence/arousal
        std::string name = "felt_sense_" + std::to_string(bus_.tick_count());
        Handle fs_node = space_.add_node(AtomType::FELT_SENSE_NODE, name,
            TruthValue{
                (sense.aggregate_valence.valence + 1.0f) * 0.5f, // Map [-1,1] to [0,1]
                sense.certainty
            });

        memory_.set_valence(fs_node.id(), sense.aggregate_valence);
        return fs_node;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] const FeltSense& current_felt_sense() const noexcept {
        return current_sense_;
    }

    void on_felt_sense_change(std::function<void(const FeltSense&)> cb) {
        sense_change_cb_ = std::move(cb);
    }

private:
    ValenceMemory& memory_;
    HormoneBus& bus_;
    AtomSpace& space_;

    FeltSense current_sense_;
    std::function<void(const FeltSense&)> sense_change_cb_;

    [[nodiscard]] CognitiveMode compute_suggested_mode(
        const ValenceSignature& vs) const noexcept
    {
        if (vs.valence > 0.3f && vs.arousal > 0.5f) return CognitiveMode::REWARD;
        if (vs.valence < -0.5f && vs.arousal > 0.6f) return CognitiveMode::THREAT;
        if (vs.valence > 0.2f && vs.arousal < 0.3f) return CognitiveMode::REFLECTIVE;
        if (vs.valence < -0.3f && vs.arousal < 0.3f) return CognitiveMode::MAINTENANCE;
        if (vs.arousal > 0.5f) return CognitiveMode::VIGILANT;
        return CognitiveMode::RESTING;
    }
};

} // namespace opencog::endo
