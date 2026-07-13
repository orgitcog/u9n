#pragma once
/**
 * @file coherence_monitor.hpp
 * @brief Cross-system coherence monitoring for the Synchronization Manager
 *
 * Feature F1.1.4: Synchronization Manager
 *
 * CoherenceMonitor tracks phase coherence across all cognitive subsystems,
 * detects coherence drops that indicate desynchronization, and triggers
 * resynchronization when needed.
 *
 * The monitor uses the CrystalBus as its primary coherence source, but also
 * tracks per-subsystem timing coherence (how closely subsystems follow their
 * expected tick rates).
 *
 * Coherence signals mapped from time-crystal-nn:
 *   Global coherence      → CrystalBus::global_coherence()
 *   Hierarchical coherence → CrystalBus::hierarchical_coherence()
 *   Timing coherence       → Deviation from expected tick intervals
 *   Cross-system coherence → Combined metric of all three
 *
 * The monitor maintains a coherence history ring buffer and supports
 * configurable alert thresholds for coherence drops.
 */

#include <opencog/sync/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace opencog::sync {

/**
 * @brief Coherence state at a point in time
 */
struct CoherenceSnapshot {
    uint64_t epoch{0};
    float global{0.0f};          ///< CrystalBus global coherence [0,1]
    float hierarchical{0.0f};    ///< Universal vs Particular sets [0,1]
    float timing{0.0f};          ///< Subsystem timing adherence [0,1]
    float composite{0.0f};       ///< Weighted combination [0,1]

    /// Compute composite from components
    void compute_composite(float w_global = 0.5f, float w_hier = 0.3f,
                           float w_timing = 0.2f) noexcept {
        composite = w_global * global + w_hier * hierarchical + w_timing * timing;
    }
};

/**
 * @brief Coherence alert level
 */
enum class CoherenceLevel : uint8_t {
    OPTIMAL,      ///< Coherence > 0.7 — all systems well-synchronized
    NORMAL,       ///< Coherence 0.4-0.7 — acceptable variation
    DEGRADED,     ///< Coherence 0.2-0.4 — noticeable desynchronization
    CRITICAL,     ///< Coherence < 0.2 — major desynchronization, resync needed
};

/// Human-readable level names
[[nodiscard]] constexpr std::string_view coherence_level_name(CoherenceLevel level) noexcept {
    constexpr std::string_view names[] = {
        "Optimal", "Normal", "Degraded", "Critical"
    };
    return names[static_cast<size_t>(level)];
}

/**
 * @brief Cross-system coherence monitor
 *
 * Usage:
 *   CoherenceMonitor monitor(crystal_bus);
 *   monitor.update(epoch, scheduler_states);
 *   if (monitor.needs_resync()) trigger_resync();
 */
class CoherenceMonitor {
public:
    using ResyncCallback = std::function<void(CoherenceLevel, float)>;

    explicit CoherenceMonitor(const temporal::CrystalBus& bus,
                               float resync_threshold = 0.1f,
                               size_t history_size = 128) noexcept
        : bus_(bus)
        , resync_threshold_(resync_threshold)
        , history_(history_size) {}

    /**
     * @brief Update coherence snapshot from current system state
     *
     * Reads CrystalBus coherence, computes timing coherence from
     * subsystem states, and updates the coherence history.
     *
     * @param epoch Current epoch
     * @param subsystem_states Per-subsystem state from TickScheduler
     * @param rates Per-subsystem rate configuration
     */
    void update(const SyncEpoch& epoch,
                const std::array<SubsystemState, SUBSYSTEM_COUNT>& subsystem_states,
                const std::array<SubsystemRate, SUBSYSTEM_COUNT>& rates) noexcept {

        CoherenceSnapshot snap;
        snap.epoch = epoch.number;
        snap.global = bus_.global_coherence();
        snap.hierarchical = bus_.hierarchical_coherence();
        snap.timing = compute_timing_coherence(epoch, subsystem_states, rates);
        snap.compute_composite();

        // Update history ring buffer
        if (history_pos_ < history_.size()) {
            history_[history_pos_] = snap;
            history_pos_ = (history_pos_ + 1) % history_.size();
            if (history_count_ < history_.size()) ++history_count_;
        }

        current_ = snap;

        // Check for coherence transitions
        auto new_level = classify(snap.composite);
        if (new_level != current_level_) {
            if (new_level > current_level_) {
                // Coherence degrading
                ++degradation_count_;
            }
            current_level_ = new_level;
        }

        // Check resync condition
        if (snap.composite < resync_threshold_ && !resync_pending_) {
            resync_pending_ = true;
            ++resync_trigger_count_;
            if (resync_callback_) {
                resync_callback_(current_level_, snap.composite);
            }
        } else if (snap.composite >= resync_threshold_ * 2.0f) {
            resync_pending_ = false;  // Coherence restored
        }
    }

    // ---- State queries ----

    /// Current coherence snapshot
    [[nodiscard]] const CoherenceSnapshot& current() const noexcept { return current_; }

    /// Current coherence level
    [[nodiscard]] CoherenceLevel level() const noexcept { return current_level_; }

    /// Whether a resynchronization is needed
    [[nodiscard]] bool needs_resync() const noexcept { return resync_pending_; }

    /// Acknowledge resync (clear pending flag)
    void acknowledge_resync() noexcept { resync_pending_ = false; }

    // ---- History ----

    /// Get historical snapshot at offset ticks back (0 = most recent)
    [[nodiscard]] CoherenceSnapshot history(size_t ticks_back) const noexcept {
        if (ticks_back >= history_count_) return {};
        size_t idx = (history_pos_ + history_.size() - 1 - ticks_back) % history_.size();
        return history_[idx];
    }

    /// Number of snapshots in history
    [[nodiscard]] size_t history_count() const noexcept { return history_count_; }

    /// Coherence trend over the last N snapshots (positive = improving)
    [[nodiscard]] float trend(size_t window = 16) const noexcept {
        if (history_count_ < 2) return 0.0f;
        size_t n = std::min(window, history_count_);
        float oldest = history(n - 1).composite;
        float newest = history(0).composite;
        return newest - oldest;
    }

    // ---- Configuration ----

    void set_resync_threshold(float t) noexcept { resync_threshold_ = std::clamp(t, 0.0f, 1.0f); }
    [[nodiscard]] float resync_threshold() const noexcept { return resync_threshold_; }

    void set_resync_callback(ResyncCallback cb) noexcept { resync_callback_ = std::move(cb); }

    // ---- Metrics ----

    [[nodiscard]] uint64_t degradation_count() const noexcept { return degradation_count_; }
    [[nodiscard]] uint64_t resync_trigger_count() const noexcept { return resync_trigger_count_; }

private:
    /// Classify composite coherence into a level
    [[nodiscard]] static CoherenceLevel classify(float coherence) noexcept {
        if (coherence >= 0.7f) return CoherenceLevel::OPTIMAL;
        if (coherence >= 0.4f) return CoherenceLevel::NORMAL;
        if (coherence >= 0.2f) return CoherenceLevel::DEGRADED;
        return CoherenceLevel::CRITICAL;
    }

    /**
     * @brief Compute timing coherence from subsystem tick adherence
     *
     * Measures how closely each subsystem follows its expected tick rate.
     * A subsystem that ticks exactly on schedule has timing coherence 1.0.
     * Skipped or late ticks reduce coherence.
     *
     * @return Timing coherence [0,1]
     */
    [[nodiscard]] float compute_timing_coherence(
        const SyncEpoch& epoch,
        const std::array<SubsystemState, SUBSYSTEM_COUNT>& states,
        const std::array<SubsystemRate, SUBSYSTEM_COUNT>& rates) const noexcept {

        float total_coherence = 0.0f;
        size_t active_count = 0;

        for (size_t i = 0; i < SUBSYSTEM_COUNT; ++i) {
            if (!states[i].enabled) continue;
            ++active_count;

            uint32_t divisor = rates[i].divisor;
            if (divisor == 0) divisor = 1;

            // Expected tick interval
            uint64_t expected_interval = divisor;
            uint64_t actual_interval = epoch.number - states[i].last_tick_epoch;

            if (actual_interval == 0) {
                total_coherence += 1.0f;
                continue;
            }

            // Coherence = 1 - normalized deviation
            float deviation = std::abs(
                static_cast<float>(actual_interval) -
                static_cast<float>(expected_interval)
            ) / static_cast<float>(expected_interval);

            total_coherence += std::max(0.0f, 1.0f - deviation);
        }

        return active_count > 0 ? total_coherence / static_cast<float>(active_count) : 1.0f;
    }

    const temporal::CrystalBus& bus_;
    float resync_threshold_;

    CoherenceSnapshot current_{};
    CoherenceLevel current_level_{CoherenceLevel::NORMAL};
    bool resync_pending_{false};

    // History ring buffer
    std::vector<CoherenceSnapshot> history_;
    size_t history_pos_{0};
    size_t history_count_{0};

    // Metrics
    uint64_t degradation_count_{0};
    uint64_t resync_trigger_count_{0};

    ResyncCallback resync_callback_;
};

} // namespace opencog::sync
