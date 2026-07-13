#pragma once
/**
 * @file synchronization_manager.hpp
 * @brief Top-level facade for cross-system synchronization (F1.1.4)
 *
 * Feature F1.1.4: Synchronization Manager
 *
 * SynchronizationManager is the primary entry point for coordinating
 * timing and synchronization between all cognitive subsystems. It unifies:
 *
 *   - TickScheduler: multi-rate tick scheduling (nn4c temporal scales)
 *   - SyncBarrier: phase-coherence-gated pipeline transitions
 *   - CoherenceMonitor: cross-system coherence tracking
 *   - Event history: diagnostic event ring buffer
 *
 * The manager implements the 6-phase tick pipeline:
 *   Phase 1 (PRODUCE):   Subsystems produce outputs (VES glands, VNS nuclei)
 *   Phase 2 (TRANSPORT): Buses transport signals (crystal bus, hormone bus)
 *   Phase 3 (READ):      Adapters read cross-system state
 *   Phase 4 (WRITE):     Adapters write feedback
 *   Phase 5 (GUIDANCE):  Async guidance triggers
 *   Phase 6 (ADVANCE):   Advance time, finalize epoch
 *
 * Integration with existing systems:
 *   - TemporalSystem: SynchronizationManager reads CrystalBus state
 *     for coherence-based barrier decisions
 *   - TypeConversionBridge (F1.1.3): synchronized during TRANSPORT phase
 *   - Message Protocol (F1.1.2): messages dispatched during phase boundaries
 *
 * Time Crystal Mapping (from time-crystal-nn skill):
 *   nn4c 9 temporal scales → SubsystemRate divisors
 *   nn9c 12 hierarchy levels → SubsystemId assignments
 *   Phase coupling → SyncBarrier coherence thresholds
 *   Universal/Particular sets → Fast/Slow rate tiers
 *
 * Usage:
 *   SynchronizationManager sync(crystal_bus, config);
 *   sync.tick(0.01f);   // Full pipeline tick
 *   auto metrics = sync.metrics();
 */

#include <opencog/sync/types.hpp>
#include <opencog/sync/sync_barrier.hpp>
#include <opencog/sync/tick_scheduler.hpp>
#include <opencog/sync/coherence_monitor.hpp>
#include <opencog/temporal/crystal_bus.hpp>

#include <functional>
#include <vector>

namespace opencog::sync {

/**
 * @brief Callback signature for subsystem tick dispatch
 *
 * The SynchronizationManager does not own subsystem instances.
 * Instead, it invokes this callback to let the owning system
 * tick the appropriate subsystem.
 *
 * @param id Which subsystem to tick
 * @param dt Time step for this tick
 * @param phase Which pipeline phase we are in
 */
using SubsystemTickFn = std::function<void(SubsystemId id, float dt, SyncPhase phase)>;

/**
 * @brief Facade for cross-system synchronization
 *
 * Owns a TickScheduler, SyncBarrier, and CoherenceMonitor.
 * Provides the single `tick(dt)` entry point that correctly
 * sequences all subsystem updates through the 6-phase pipeline.
 */
class SynchronizationManager {
public:
    explicit SynchronizationManager(temporal::CrystalBus& bus,
                                     SyncConfig config = {}) noexcept;

    // ---- Primary interface ----

    /**
     * @brief Execute one complete synchronization epoch
     *
     * Advances through all 6 pipeline phases, checking barriers
     * at each phase transition, scheduling subsystem ticks, and
     * updating coherence metrics.
     *
     * @param dt Time step in seconds
     */
    void tick(float dt) noexcept;

    /**
     * @brief Register the subsystem tick callback
     *
     * The callback is invoked for each subsystem that needs to
     * tick during the current epoch. The owning system dispatches
     * the tick to the appropriate subsystem instance.
     */
    void set_tick_callback(SubsystemTickFn callback) noexcept;

    /**
     * @brief Force resynchronization of all subsystems
     *
     * Resets all subsystem timing to the current epoch and
     * boosts crystal bus coupling to encourage phase locking.
     */
    void force_resync() noexcept;

    // ---- Component access ----

    [[nodiscard]] const TickScheduler& scheduler() const noexcept { return scheduler_; }
    [[nodiscard]] TickScheduler& scheduler() noexcept { return scheduler_; }

    [[nodiscard]] const SyncBarrier& barrier() const noexcept { return barrier_; }
    [[nodiscard]] SyncBarrier& barrier() noexcept { return barrier_; }

    [[nodiscard]] const CoherenceMonitor& monitor() const noexcept { return monitor_; }
    [[nodiscard]] CoherenceMonitor& monitor() noexcept { return monitor_; }

    // ---- Epoch state ----

    [[nodiscard]] const SyncEpoch& current_epoch() const noexcept { return epoch_; }
    [[nodiscard]] SyncPhase current_phase() const noexcept { return current_phase_; }
    [[nodiscard]] bool is_running() const noexcept { return running_; }

    // ---- Subsystem control ----

    /// Enable/disable a subsystem
    void set_subsystem_enabled(SubsystemId id, bool enabled) noexcept;

    /// Set rate divisor for a subsystem
    void set_rate_divisor(SubsystemId id, uint32_t divisor) noexcept;

    // ---- Metrics ----

    [[nodiscard]] const SyncMetrics& metrics() const noexcept { return metrics_; }
    void reset_metrics() noexcept;

    // ---- Event history ----

    [[nodiscard]] const std::vector<SyncEvent>& event_history() const noexcept { return events_; }
    void clear_history() noexcept;

    // ---- Configuration ----

    [[nodiscard]] const SyncConfig& config() const noexcept { return config_; }
    void set_config(const SyncConfig& config) noexcept;

private:
    /// Execute a single pipeline phase
    void execute_phase(SyncPhase phase, float dt) noexcept;

    /// Record a sync event
    void record_event(SyncEventType type, SyncPhase phase,
                      SubsystemId subsystem = SubsystemId::COUNT,
                      float value = 0.0f) noexcept;

    /// Dispatch ticks for subsystems scheduled in the given phase
    void dispatch_phase_ticks(SyncPhase phase, float dt) noexcept;

    temporal::CrystalBus& bus_;
    SyncConfig config_;

    TickScheduler scheduler_;
    SyncBarrier barrier_;
    CoherenceMonitor monitor_;

    SyncEpoch epoch_{};
    SyncPhase current_phase_{SyncPhase::PRODUCE};
    bool running_{false};

    SyncMetrics metrics_{};
    std::vector<SyncEvent> events_;
    float pre_resync_coupling_{0.0f};
    uint64_t resync_cooldown_epoch_{0};

    SubsystemTickFn tick_callback_;

    static constexpr uint64_t RESYNC_COOLDOWN = 16;
};

} // namespace opencog::sync
