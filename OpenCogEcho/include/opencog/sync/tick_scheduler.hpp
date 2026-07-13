#pragma once
/**
 * @file tick_scheduler.hpp
 * @brief Multi-rate tick scheduler for cognitive subsystems
 *
 * Feature F1.1.4: Synchronization Manager
 *
 * TickScheduler implements a multi-rate scheduling strategy inspired by
 * the nn4c time crystal neuron's 9 temporal scales. Each subsystem has
 * a natural tick rate defined by its SubsystemRate:
 *
 *   FAST tier  (divisor 1):  VNS, TCS, Attention, Bridge — every tick
 *   MEDIUM tier (divisor 4): PLN, Entelechy, AFI — every 4th tick
 *   SLOW tier  (divisor 16): VES — every 16th tick
 *
 * The scheduler determines which subsystems should tick in a given epoch,
 * respects rate divisors, handles staleness detection, and provides the
 * ordered tick list for each pipeline phase.
 *
 * Phase ordering within a tick follows the 6-phase pipeline:
 *   1. PRODUCE:   VES glands, VNS nuclei
 *   2. TRANSPORT: hormone bus, nerve bus
 *   3. READ:      adapters read cross-system state
 *   4. WRITE:     adapters write feedback
 *   5. GUIDANCE:  async trigger/receive
 *   6. ADVANCE:   time advance, epoch increment
 */

#include <opencog/sync/types.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace opencog::sync {

/**
 * @brief Schedule entry — which subsystem ticks and with what dt
 */
struct TickEntry {
    SubsystemId id;
    float dt;              ///< Effective dt for this subsystem
    bool is_forced{false}; ///< Whether this tick was forced (staleness)
};

/**
 * @brief Multi-rate tick scheduler for cognitive subsystems
 *
 * Usage:
 *   TickScheduler sched(config);
 *   auto entries = sched.schedule(epoch);
 *   for (auto& entry : entries) {
 *       tick_subsystem(entry.id, entry.dt);
 *   }
 *   sched.record_tick(entry.id, epoch);
 */
class TickScheduler {
public:
    explicit TickScheduler(const SyncConfig& config = {}) noexcept
        : config_(config) {
        // Initialize subsystem states
        for (size_t i = 0; i < SUBSYSTEM_COUNT; ++i) {
            auto id = static_cast<SubsystemId>(i);
            states_[i].id = id;
            states_[i].enabled = true;
        }
    }

    /**
     * @brief Compute the tick schedule for a given epoch
     *
     * Returns an ordered list of subsystems that should tick in this epoch,
     * based on their rate divisors and staleness. The order follows the
     * 6-phase pipeline priority.
     *
     * @param epoch Current synchronization epoch
     * @return Ordered list of TickEntries
     */
    [[nodiscard]] std::vector<TickEntry> schedule(const SyncEpoch& epoch) const noexcept {
        std::vector<TickEntry> entries;
        entries.reserve(SUBSYSTEM_COUNT);

        for (size_t i = 0; i < SUBSYSTEM_COUNT; ++i) {
            auto id = static_cast<SubsystemId>(i);
            if (!states_[i].enabled) continue;

            const auto& rate = config_.rates[i];
            bool should_tick = epoch.should_tick(rate.divisor);
            bool is_stale = states_[i].is_stale(epoch.number, config_.max_staleness);
            bool forced = !should_tick && is_stale;

            if (should_tick || forced) {
                // Compute effective dt: accumulated time since last tick
                float effective_dt = epoch.dt;
                if (states_[i].last_tick_epoch > 0) {
                    uint64_t elapsed_epochs = epoch.number - states_[i].last_tick_epoch;
                    effective_dt = epoch.dt * static_cast<float>(elapsed_epochs);
                }

                entries.push_back(TickEntry{id, effective_dt, forced});
            }
        }

        // Sort by priority (higher priority first within same tier)
        std::sort(entries.begin(), entries.end(), [this](const TickEntry& a, const TickEntry& b) {
            const auto& ra = config_.rates[static_cast<size_t>(a.id)];
            const auto& rb = config_.rates[static_cast<size_t>(b.id)];
            if (ra.tier != rb.tier) return ra.tier < rb.tier;  // FAST before MEDIUM before SLOW
            return ra.priority > rb.priority;
        });

        return entries;
    }

    /**
     * @brief Get subsystems scheduled for a specific pipeline phase
     *
     * Phase→subsystem mapping:
     *   PRODUCE:   ENDOCRINE, NERVOUS
     *   TRANSPORT: TEMPORAL (crystal bus), BRIDGE
     *   READ:      ATTENTION, REASONING
     *   WRITE:     (adapters — handled by SynchronizationManager)
     *   GUIDANCE:  ENTELECHY, AFI
     *   ADVANCE:   (time advance — handled by SynchronizationManager)
     */
    [[nodiscard]] std::vector<SubsystemId> subsystems_for_phase(SyncPhase phase) const noexcept {
        switch (phase) {
            case SyncPhase::PRODUCE:
                return {SubsystemId::ENDOCRINE, SubsystemId::NERVOUS};
            case SyncPhase::TRANSPORT:
                return {SubsystemId::TEMPORAL, SubsystemId::BRIDGE};
            case SyncPhase::READ:
                return {SubsystemId::ATTENTION, SubsystemId::REASONING};
            case SyncPhase::WRITE:
                return {};  // Adapters — managed externally
            case SyncPhase::GUIDANCE:
                return {SubsystemId::ENTELECHY, SubsystemId::AFI};
            case SyncPhase::ADVANCE:
                return {};  // Time advance — managed externally
            default:
                return {};
        }
    }

    /**
     * @brief Record that a subsystem completed its tick
     */
    void record_tick(SubsystemId id, const SyncEpoch& epoch) noexcept {
        auto idx = static_cast<size_t>(id);
        if (idx < SUBSYSTEM_COUNT) {
            states_[idx].last_tick_epoch = epoch.number;
            states_[idx].last_dt = epoch.dt;
            states_[idx].cumulative_time += epoch.dt;
            ++states_[idx].tick_count;
        }
    }

    /**
     * @brief Enable or disable a subsystem
     */
    void set_enabled(SubsystemId id, bool enabled) noexcept {
        auto idx = static_cast<size_t>(id);
        if (idx < SUBSYSTEM_COUNT) {
            states_[idx].enabled = enabled;
        }
    }

    /**
     * @brief Check if a subsystem is enabled
     */
    [[nodiscard]] bool is_enabled(SubsystemId id) const noexcept {
        auto idx = static_cast<size_t>(id);
        return idx < SUBSYSTEM_COUNT && states_[idx].enabled;
    }

    /**
     * @brief Get state of a specific subsystem
     */
    [[nodiscard]] const SubsystemState& state(SubsystemId id) const noexcept {
        return states_[static_cast<size_t>(id)];
    }

    /**
     * @brief Get all subsystem states
     */
    [[nodiscard]] const std::array<SubsystemState, SUBSYSTEM_COUNT>& states() const noexcept {
        return states_;
    }

    /**
     * @brief Set rate divisor for a subsystem
     */
    void set_rate_divisor(SubsystemId id, uint32_t divisor) noexcept {
        auto idx = static_cast<size_t>(id);
        if (idx < SUBSYSTEM_COUNT) {
            config_.rates[idx].divisor = std::max(divisor, 1u);
        }
    }

    /**
     * @brief Get count of stale subsystems
     */
    [[nodiscard]] size_t stale_count(uint64_t current_epoch) const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < SUBSYSTEM_COUNT; ++i) {
            if (states_[i].is_stale(current_epoch, config_.max_staleness)) {
                ++count;
            }
        }
        return count;
    }

private:
    SyncConfig config_;
    std::array<SubsystemState, SUBSYSTEM_COUNT> states_{};
};

} // namespace opencog::sync
