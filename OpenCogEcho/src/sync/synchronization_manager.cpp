/**
 * @file synchronization_manager.cpp
 * @brief Implementation of the SynchronizationManager facade (F1.1.4)
 *
 * Feature F1.1.4: Synchronization Manager
 *
 * Implements the 6-phase tick pipeline with phase-coherence barriers,
 * multi-rate scheduling, and cross-system coherence monitoring.
 */

#include <opencog/sync/synchronization_manager.hpp>

#include <algorithm>

namespace opencog::sync {

// ============================================================================
// Construction
// ============================================================================

SynchronizationManager::SynchronizationManager(
    temporal::CrystalBus& bus, SyncConfig config) noexcept
    : bus_(bus)
    , config_(config)
    , scheduler_(config)
    , barrier_(bus, config.barrier_coherence_threshold, config.barrier_max_wait)
    , monitor_(bus, config.resync_coherence_threshold, config.max_history) {

    events_.reserve(config.max_history);

    // Wire barrier events into the event history
    barrier_.set_callback([this](SyncEventType type, SyncPhase phase, float coherence) {
        record_event(type, phase, SubsystemId::COUNT, coherence);
    });

    // Wire coherence monitor resync callback
    monitor_.set_resync_callback([this](CoherenceLevel level, float coherence) {
        record_event(SyncEventType::COHERENCE_DROP, current_phase_,
                     SubsystemId::COUNT, coherence);
    });
}

// ============================================================================
// Primary tick
// ============================================================================

void SynchronizationManager::tick(float dt) noexcept {
    running_ = true;

    // Advance epoch
    epoch_.number++;
    epoch_.dt = dt;
    epoch_.wall_elapsed = dt;  // Caller may override for real wall-clock

    record_event(SyncEventType::EPOCH_START, SyncPhase::PRODUCE);

    // Execute each pipeline phase in order
    for (uint8_t p = 0; p < SYNC_PHASE_COUNT; ++p) {
        auto phase = static_cast<SyncPhase>(p);
        execute_phase(phase, dt);
    }

    // Update coherence monitor
    monitor_.update(epoch_, scheduler_.states(), config_.rates);

    // Update aggregate metrics
    float coherence = bus_.global_coherence();
    metrics_.update_coherence(coherence);

    // Update mean epoch dt before publishing the new epoch count
    if (metrics_.total_epochs > 0) {
        metrics_.mean_epoch_dt =
            metrics_.mean_epoch_dt * 0.99f + dt * 0.01f;
    } else {
        metrics_.mean_epoch_dt = dt;
    }
    metrics_.total_epochs = epoch_.number;

    // Check for resync
    if (monitor_.needs_resync()) {
        force_resync();
    }

    // Restore coupling after resync cooldown
    if (resync_cooldown_epoch_ > 0 && epoch_.number >= resync_cooldown_epoch_) {
        float current_coupling = bus_.config().global_coupling;
        bus_.set_global_coupling(std::max(0.0f, current_coupling - resync_coupling_boost_));
        resync_coupling_boost_ = 0.0f;
        resync_cooldown_epoch_ = 0;
    }

    running_ = false;
}

// ============================================================================
// Pipeline phase execution
// ============================================================================

void SynchronizationManager::execute_phase(SyncPhase phase, float dt) noexcept {
    current_phase_ = phase;
    record_event(SyncEventType::PHASE_ENTER, phase);

    // Enter barrier for this phase transition
    barrier_.enter(phase, epoch_.number);

    // Check barrier once and proceed; this pipeline is single-threaded.
    auto result = barrier_.check();
    if (result == BarrierResult::WAITING) {
        metrics_.barrier_waits++;
    } else if (result == BarrierResult::FORCE_RELEASED) {
        metrics_.barrier_waits++;
        metrics_.resync_count++;
    }

    metrics_.total_barriers++;

    // Dispatch subsystem ticks for this phase
    dispatch_phase_ticks(phase, dt);
}

void SynchronizationManager::dispatch_phase_ticks(SyncPhase phase, float dt) noexcept {
    if (!tick_callback_) return;

    auto subsystems = scheduler_.subsystems_for_phase(phase);
    auto scheduled = scheduler_.schedule(epoch_);

    for (auto id : subsystems) {
        // Check if this subsystem is in the schedule
        bool in_schedule = false;
        float effective_dt = dt;

        for (const auto& entry : scheduled) {
            if (entry.id == id) {
                in_schedule = true;
                effective_dt = entry.dt;
                break;
            }
        }

        if (in_schedule) {
            tick_callback_(id, effective_dt, phase);
            scheduler_.record_tick(id, epoch_, effective_dt);
            metrics_.subsystem_ticks[static_cast<size_t>(id)]++;
            record_event(SyncEventType::SUBSYSTEM_TICK, phase, id, effective_dt);
        }
    }
}

// ============================================================================
// Force resynchronization
// ============================================================================

void SynchronizationManager::force_resync() noexcept {
    record_event(SyncEventType::RESYNC_TRIGGERED, current_phase_);

    // Temporarily boost coupling and restore it after a cooldown.
    float current_coupling = bus_.config().global_coupling;
    resync_coupling_boost_ = std::min(0.2f, 1.0f - current_coupling);
    bus_.set_global_coupling(current_coupling + resync_coupling_boost_);
    resync_cooldown_epoch_ = epoch_.number + RESYNC_COOLDOWN;

    // Reset all subsystem timing to the current epoch without falsifying ticks
    for (size_t i = 0; i < SUBSYSTEM_COUNT; ++i) {
        scheduler_.reset_timing(static_cast<SubsystemId>(i), epoch_.number);
    }

    metrics_.resync_count++;
}

// ============================================================================
// Subsystem control
// ============================================================================

void SynchronizationManager::set_tick_callback(SubsystemTickFn callback) noexcept {
    tick_callback_ = std::move(callback);
}

void SynchronizationManager::set_subsystem_enabled(SubsystemId id, bool enabled) noexcept {
    scheduler_.set_enabled(id, enabled);
}

void SynchronizationManager::set_rate_divisor(SubsystemId id, uint32_t divisor) noexcept {
    scheduler_.set_rate_divisor(id, divisor);
    config_.rates[static_cast<size_t>(id)].divisor = std::max(divisor, 1u);
}

// ============================================================================
// Metrics & History
// ============================================================================

void SynchronizationManager::reset_metrics() noexcept {
    metrics_.reset();
}

void SynchronizationManager::clear_history() noexcept {
    events_.clear();
}

void SynchronizationManager::set_config(const SyncConfig& config) noexcept {
    config_ = config;
    scheduler_.set_config(config);
    barrier_.set_threshold(config.barrier_coherence_threshold);
    barrier_.set_max_wait(config.barrier_max_wait);
    monitor_.set_resync_threshold(config.resync_coherence_threshold);
}

void SynchronizationManager::record_event(SyncEventType type, SyncPhase phase,
                                            SubsystemId subsystem, float value) noexcept {
    if (events_.size() >= config_.max_history) {
        // Ring buffer: overwrite oldest
        events_.erase(events_.begin());
    }
    events_.push_back(SyncEvent{
        type, epoch_.number, phase, subsystem,
        bus_.global_coherence(), value
    });
}

} // namespace opencog::sync
