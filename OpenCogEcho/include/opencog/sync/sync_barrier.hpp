#pragma once
/**
 * @file sync_barrier.hpp
 * @brief Phase-aligned synchronization barrier for the tick pipeline
 *
 * Feature F1.1.4: Synchronization Manager
 *
 * SyncBarrier implements a phase-coherence-gated barrier that controls
 * transitions between tick pipeline phases. The barrier checks the
 * CrystalBus global_coherence() metric against a configurable threshold:
 *
 *   - If coherence >= threshold: barrier releases immediately
 *   - If coherence < threshold: barrier waits (up to max_wait epochs)
 *   - After max_wait: barrier force-releases and records a resync event
 *
 * This maps to the nn4c/nn9c time crystal concept of phase-coupled
 * dynamics: subsystems naturally synchronize via oscillatory coupling,
 * and the barrier ensures they reach sufficient coherence before
 * proceeding to the next pipeline phase.
 *
 * Triadic synchronization points from the 12-step cognitive cycle
 * ({1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}) are implemented as
 * barrier checkpoints between specific phase transitions.
 */

#include <opencog/sync/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>

#include <functional>

namespace opencog::sync {

/**
 * @brief Outcome of a barrier check
 */
enum class BarrierResult : uint8_t {
    RELEASED,        ///< Coherence met, proceed to next phase
    WAITING,         ///< Coherence not met, continue waiting
    FORCE_RELEASED,  ///< Max wait exceeded, forced release
};

/**
 * @brief Phase-coherence-gated synchronization barrier
 *
 * Usage:
 *   SyncBarrier barrier(crystal_bus, config);
 *   barrier.enter(SyncPhase::PRODUCE, epoch);
 *   // ... subsystem ticks ...
 *   auto result = barrier.check();  // RELEASED, WAITING, or FORCE_RELEASED
 */
class SyncBarrier {
public:
    /// Callback for barrier events (optional diagnostics hook)
    using EventCallback = std::function<void(SyncEventType, SyncPhase, float)>;

    explicit SyncBarrier(const temporal::CrystalBus& bus,
                         float coherence_threshold = 0.3f,
                         uint32_t max_wait = 4) noexcept
        : bus_(bus)
        , threshold_(coherence_threshold)
        , max_wait_(max_wait) {}

    /**
     * @brief Enter a new barrier at the given phase transition
     *
     * Resets wait counter and captures initial coherence state.
     *
     * @param phase The pipeline phase we are transitioning FROM
     * @param epoch Current epoch number
     */
    void enter(SyncPhase phase, uint64_t epoch) noexcept {
        current_phase_ = phase;
        entry_epoch_ = epoch;
        wait_count_ = 0;
        entry_coherence_ = bus_.global_coherence();
    }

    /**
     * @brief Check whether the barrier should release
     *
     * Evaluates CrystalBus global_coherence() against the threshold.
     * If coherence is met, releases immediately. If not, increments
     * the wait counter. If max_wait is exceeded, force-releases.
     *
     * @return BarrierResult indicating the barrier state
     */
    [[nodiscard]] BarrierResult check() noexcept {
        float coherence = bus_.global_coherence();
        ++total_checks_;

        if (coherence >= effective_threshold(current_phase_)) {
            // Natural release — coherence threshold met
            exit_coherence_ = coherence;
            if (callback_) {
                callback_(SyncEventType::BARRIER_RELEASE, current_phase_, coherence);
            }
            return BarrierResult::RELEASED;
        }

        ++wait_count_;

        if (wait_count_ >= max_wait_) {
            // Force release — max wait exceeded
            exit_coherence_ = coherence;
            ++force_release_count_;
            if (callback_) {
                callback_(SyncEventType::RESYNC_TRIGGERED, current_phase_, coherence);
            }
            return BarrierResult::FORCE_RELEASED;
        }

        // Still waiting
        if (callback_) {
            callback_(SyncEventType::BARRIER_WAIT, current_phase_, coherence);
        }
        return BarrierResult::WAITING;
    }

    /**
     * @brief Check if this transition is a triadic synchronization point
     *
     * Triadic points from the 12-step cognitive cycle where all three
     * consciousness streams align:
     *   Phase 0→1 (Produce→Transport): {1,5,9} point
     *   Phase 2→3 (Read→Write):        {2,6,10} point
     *   Phase 4→5 (Guidance→Advance):   {4,8,12} point
     *
     * At triadic points, the coherence threshold is elevated to ensure
     * tighter synchronization.
     */
    [[nodiscard]] bool is_triadic_point(SyncPhase phase) const noexcept {
        auto p = static_cast<uint8_t>(phase);
        return p == 0 || p == 2 || p == 4;
    }

    /**
     * @brief Get the effective coherence threshold for a phase
     *
     * Triadic synchronization points use a higher threshold (1.5x base)
     * to enforce tighter coupling at critical pipeline transitions.
     */
    [[nodiscard]] float effective_threshold(SyncPhase phase) const noexcept {
        float base = threshold_;
        if (is_triadic_point(phase)) {
            base = std::min(base * 1.5f, 1.0f);
        }
        return base;
    }

    // ---- Configuration ----

    void set_threshold(float t) noexcept { threshold_ = std::clamp(t, 0.0f, 1.0f); }
    [[nodiscard]] float threshold() const noexcept { return threshold_; }

    void set_max_wait(uint32_t w) noexcept { max_wait_ = w; }
    [[nodiscard]] uint32_t max_wait() const noexcept { return max_wait_; }

    void set_callback(EventCallback cb) noexcept { callback_ = std::move(cb); }

    // ---- Metrics ----

    [[nodiscard]] uint32_t wait_count() const noexcept { return wait_count_; }
    [[nodiscard]] uint64_t total_checks() const noexcept { return total_checks_; }
    [[nodiscard]] uint64_t force_release_count() const noexcept { return force_release_count_; }
    [[nodiscard]] float entry_coherence() const noexcept { return entry_coherence_; }
    [[nodiscard]] float exit_coherence() const noexcept { return exit_coherence_; }
    [[nodiscard]] SyncPhase current_phase() const noexcept { return current_phase_; }

private:
    const temporal::CrystalBus& bus_;
    float threshold_;
    uint32_t max_wait_;

    SyncPhase current_phase_{SyncPhase::PRODUCE};
    uint64_t entry_epoch_{0};
    uint32_t wait_count_{0};
    float entry_coherence_{0.0f};
    float exit_coherence_{0.0f};

    uint64_t total_checks_{0};
    uint64_t force_release_count_{0};

    EventCallback callback_;
};

} // namespace opencog::sync
