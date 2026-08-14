#pragma once
/**
 * @file adapter.hpp
 * @brief Entelechy Endocrine Adapter — Bidirectional coupling
 *
 * Bridges the Ontogenetic Entelechy system (Civic Angel, Cloninger temperament,
 * interoceptive model) with the Virtual Endocrine System.
 *
 * Three-phase operation within the VES tick pipeline:
 *
 *   Phase 0 (BEFORE glands): Apply Cloninger personality gains to the
 *     hormone bus via production_gains and decay_gains. This shapes the
 *     *sensitivity* of the entire endocrine cascade.
 *
 *   Phase 7 (AFTER adapter feedback): Update the InteroceptiveModel from
 *     the bus snapshot and write computed interoceptive channels (20-31)
 *     back to the bus.
 *
 *   Phase 8 (END of tick): Civic Angel self-observation. Self-throttled
 *     (typically every ~10 ticks). Also performs edge-triggered feedback:
 *     self-coherence crossings, free energy spikes, polyvagal transitions.
 *
 * Cloninger gains are computed each tick (Phase 0) because they are cheap
 * and the temperament profile may be updated by the developmental system.
 *
 * FEEDBACK SIGNALS (edge-triggered, applied in apply_feedback):
 *   Self coherence crossing 0.7 upward → DA_PHASIC +0.1, SEROTONIN +0.05
 *   Free energy spike > 5.0             → CORTISOL +0.1, NE +0.05
 *   Polyvagal tier change to DORSAL     → CORTISOL +0.1
 *   Polyvagal tier change to VENTRAL    → OXYTOCIN +0.05
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/entelechy/civic_angel.hpp>
#include <opencog/entelechy/cloninger.hpp>
#include <opencog/entelechy/interoceptive.hpp>

#include <algorithm>
#include <cmath>

namespace opencog::endo {

class EntelechyEndocrineAdapter : public EndocrineConnector {
public:
    EntelechyEndocrineAdapter(HormoneBus& bus, entelechy::CivicAngel& angel)
        : EndocrineConnector(bus)
        , angel_(angel)
    {
        // Initialize previous state for edge-triggered feedback
        prev_self_coherence_ = angel_.self_coherence();
        prev_free_energy_ = angel_.total_free_energy();
        prev_polyvagal_ = angel_.interoceptive().dominant_tier();
    }

    // ========================================================================
    // Phase 0: Cloninger gains BEFORE gland production
    // ========================================================================

    /**
     * @brief Compute personality-shaped production/decay gains
     *
     * Returns a GainProfile that the EndocrineSystem applies to the HormoneBus
     * before any gland production occurs. This way, temperament shapes
     * *sensitivity* to hormones, not hormone levels directly.
     */
    [[nodiscard]] entelechy::GainProfile compute_gains() const {
        return angel_.cloninger().compute_gains();
    }

    // ========================================================================
    // Phase 7: Interoceptive model update
    // ========================================================================

    /**
     * @brief Update the interoceptive model from the current bus state
     *
     * Reads the hormone bus snapshot and feeds it into the InteroceptiveModel
     * (Porges polyvagal hierarchy, Craig insular integration, McEwen allostatic
     * load). This runs AFTER all adapter feedback (Phase 4) so that the
     * interoceptive model reflects the full endocrine state.
     */
    void update_interoceptive() {
        EndocrineState state = bus_.snapshot();
        angel_.interoceptive().update(state);
    }

    /**
     * @brief Write computed interoceptive values to bus channels 20-31
     *
     * After the InteroceptiveModel has been updated, write its computed
     * values back to the bus. These channels (VAGAL_TONE through
     * THERMOREGULATORY) are set directly, not produced additively.
     */
    void write_interoceptive_to_bus() {
        auto values = angel_.interoceptive().to_bus_values();

        // Channels 20-31 map to the 12 interoceptive values
        constexpr uint8_t INTEROCEPTIVE_BASE = 20;
        for (size_t i = 0; i < values.size(); ++i) {
            auto id = static_cast<HormoneId>(INTEROCEPTIVE_BASE + i);
            bus_.set_concentration(id, values[i]);
        }
    }

    // ========================================================================
    // Phase 8: Civic Angel observation
    // ========================================================================

    /**
     * @brief Trigger Civic Angel self-observation (self-throttled)
     *
     * Delegates to angel_.tick() which internally self-throttles
     * (typically only observing every ~10 VES ticks).
     */
    void observe(uint64_t current_tick) {
        angel_.tick(current_tick);
    }

    // ========================================================================
    // Combined Phase 7+8 update (called from EndocrineSystem::tick Phase 3)
    // ========================================================================

    /**
     * @brief Apply endocrine modulation (Phase 3 adapter read)
     *
     * In the standard adapter read phase, we update the interoceptive
     * model and write its state back to the bus, and increment our tick.
     */
    void apply_endocrine_modulation(const HormoneBus& /*bus*/) {
        // Interoceptive update deferred to Phase 7 (after adapter feedback).
        // Only increment tick counter here in Phase 3.
        tick_count_++;
    }

    // ========================================================================
    // Feedback: edge-triggered signals (Phase 4)
    // ========================================================================

    /**
     * @brief Write edge-triggered feedback signals to the bus
     *
     * Follows the Marduk adapter pattern: compare current state to previous
     * state and emit hormonal signals on significant transitions.
     *
     * Edge triggers:
     *   1. Self-coherence crossing 0.7 upward → positive self-integration
     *   2. Free energy spike > 5.0             → prediction crisis
     *   3. Polyvagal tier change               → autonomic state shift
     */
    void apply_feedback() {
        const float self_coherence = angel_.self_coherence();
        const float free_energy = angel_.total_free_energy();
        const entelechy::PolyvagalTier polyvagal = angel_.interoceptive().dominant_tier();

        // --- 1. Self-coherence crossing 0.7 upward → reward ---
        if (self_coherence >= 0.7f && prev_self_coherence_ < 0.7f) {
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.1f);
            bus_.produce(HormoneId::SEROTONIN, 0.05f);
        }

        // --- 2. Free energy spike > 5.0 → prediction crisis ---
        if (free_energy > 5.0f && prev_free_energy_ <= 5.0f) {
            bus_.produce(HormoneId::CORTISOL, 0.1f);
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.05f);
        }

        // --- 3. Polyvagal tier change ---
        if (polyvagal != prev_polyvagal_) {
            // Transition to DORSAL_VAGAL (freeze/collapse) → stress
            if (polyvagal == entelechy::PolyvagalTier::DORSAL_VAGAL) {
                bus_.produce(HormoneId::CORTISOL, 0.1f);
            }
            // Transition to VENTRAL_VAGAL (social engagement) → trust
            if (polyvagal == entelechy::PolyvagalTier::VENTRAL_VAGAL) {
                bus_.produce(HormoneId::OXYTOCIN, 0.05f);
            }
        }

        // Update previous state for next tick's edge detection
        prev_self_coherence_ = self_coherence;
        prev_free_energy_ = free_energy;
        prev_polyvagal_ = polyvagal;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    [[nodiscard]] entelechy::CivicAngel& angel() noexcept { return angel_; }
    [[nodiscard]] const entelechy::CivicAngel& angel() const noexcept { return angel_; }

private:
    entelechy::CivicAngel& angel_;
    uint64_t tick_count_{0};

    // Previous state for edge-triggered feedback
    float prev_self_coherence_{0.0f};
    float prev_free_energy_{0.0f};
    entelechy::PolyvagalTier prev_polyvagal_{entelechy::PolyvagalTier::VENTRAL_VAGAL};
};

} // namespace opencog::endo
