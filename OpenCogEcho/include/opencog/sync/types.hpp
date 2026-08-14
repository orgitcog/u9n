#pragma once
/**
 * @file types.hpp
 * @brief Core type definitions for the Synchronization Manager (F1.1.4)
 *
 * Defines the vocabulary types for cross-system synchronization:
 * - SubsystemId: identifies each cognitive subsystem in the tick pipeline
 * - SyncPhase: the 6-phase tick pipeline from temporal_system.hpp
 * - SyncEpoch: a numbered synchronization epoch with timing metadata
 * - SyncEvent: recorded synchronization events for diagnostics
 * - SyncConfig: global synchronization configuration
 *
 * Design principles (matching opencog::temporal::types.hpp):
 * - Small POD types (SyncEpoch = 16 bytes)
 * - SIMD-aligned state snapshots
 * - Deterministic, lock-free readable metrics
 *
 * Relationship to time-crystal-nn:
 *   nn4c temporal scales   → SubsystemRate (characteristic tick periods)
 *   nn9c hierarchy levels  → SubsystemId (brain regions mapped to subsystems)
 *   Oscillatory coupling   → SyncBarrier phase-coherence thresholds
 *   Universal/Particular   → Fast-path vs slow-path subsystem classification
 */

#include <opencog/core/types.hpp>
#include <opencog/temporal/types.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace opencog::sync {

// ============================================================================
// Subsystem Identification
// ============================================================================

/**
 * @brief Identifiers for each cognitive subsystem in the tick pipeline
 *
 * Maps to nn9c hierarchy levels:
 *   ENDOCRINE   → Level 6 (Hypothalamus / HomeostaticRegulator)
 *   NERVOUS     → Level 8 (ThalamicBody / AttentionRelay)
 *   TEMPORAL    → Level 2 (Neuron / ConceptUnit) — time crystal oscillators
 *   BRIDGE      → Cross-level (tensor ↔ atom conversion)
 *   ATTENTION   → Level 8 (ThalamicBody / ECAN attention gating)
 *   REASONING   → Level 4 (CortexDomain / PLN inference)
 *   ENTELECHY   → Cross-level (goal-directed behavior)
 *   AFI         → Cross-level (active inference / free energy)
 */
enum class SubsystemId : uint8_t {
    ENDOCRINE   = 0,   ///< VES — slow chemical broadcast
    NERVOUS     = 1,   ///< VNS — fast electrical routing
    TEMPORAL    = 2,   ///< TCS — multi-scale oscillatory
    BRIDGE      = 3,   ///< Type Conversion Layer (F1.1.3)
    ATTENTION   = 4,   ///< ECAN attention bank
    REASONING   = 5,   ///< PLN inference engine
    ENTELECHY   = 6,   ///< Goal-directed behavior
    AFI         = 7,   ///< Active inference / free energy

    COUNT       = 8
};

inline constexpr size_t SUBSYSTEM_COUNT = static_cast<size_t>(SubsystemId::COUNT);

/// Human-readable subsystem names
[[nodiscard]] constexpr std::string_view subsystem_name(SubsystemId id) noexcept {
    constexpr std::string_view names[] = {
        "Endocrine(VES)", "Nervous(VNS)", "Temporal(TCS)", "Bridge",
        "Attention(ECAN)", "Reasoning(PLN)", "Entelechy", "ActiveInference(AFI)"
    };
    auto idx = static_cast<size_t>(id);
    return idx < SUBSYSTEM_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Tick Pipeline Phases (from temporal_system.hpp documentation)
// ============================================================================

/**
 * @brief The 6-phase tick pipeline for system-wide synchronization
 *
 * Each tick advances through these phases in order:
 *   Phase 1: Glands update → hormones produced
 *   Phase 2: Hormone bus tick → decay, mode detection
 *   Phase 3: Adapters READ → modulate targets
 *   Phase 4: Adapters WRITE → feedback to buses
 *   Phase 5: Guidance → async trigger/receive
 *   Phase 6: Valence → advance time, epoch increment
 */
enum class SyncPhase : uint8_t {
    PRODUCE     = 0,   ///< Phase 1: subsystems produce outputs
    TRANSPORT   = 1,   ///< Phase 2: buses transport signals
    READ        = 2,   ///< Phase 3: adapters read cross-system state
    WRITE       = 3,   ///< Phase 4: adapters write feedback
    GUIDANCE    = 4,   ///< Phase 5: async guidance triggers
    ADVANCE     = 5,   ///< Phase 6: advance time, finalize epoch

    PHASE_COUNT = 6
};

inline constexpr size_t SYNC_PHASE_COUNT = static_cast<size_t>(SyncPhase::PHASE_COUNT);

/// Human-readable phase names
[[nodiscard]] constexpr std::string_view phase_name(SyncPhase phase) noexcept {
    constexpr std::string_view names[] = {
        "Produce", "Transport", "Read", "Write", "Guidance", "Advance"
    };
    auto idx = static_cast<size_t>(phase);
    return idx < SYNC_PHASE_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Subsystem Rate Classification (from nn4c temporal scales)
// ============================================================================

/**
 * @brief Rate tier for a subsystem's natural tick frequency
 *
 * Maps to nn4c's Particular/Universal set distinction:
 *   FAST   = Particular Sets (< 0.5s period, local processing)
 *   MEDIUM = Transition zone
 *   SLOW   = Universal Sets (>= 0.5s period, global state)
 */
enum class RateTier : uint8_t {
    FAST   = 0,   ///< Every tick (VNS, TCS, Attention, Bridge)
    MEDIUM = 1,   ///< Every N ticks (PLN, Entelechy, AFI)
    SLOW   = 2,   ///< Every M ticks (VES)
};

/**
 * @brief Rate configuration for a subsystem
 */
struct SubsystemRate {
    SubsystemId id;
    RateTier tier;
    uint32_t divisor;       ///< Tick every `divisor` master ticks (1 = every tick)
    float priority;         ///< [0,1] scheduling priority within same tier

    /// Default rates matching biological timescales
    static constexpr SubsystemRate defaults(SubsystemId id) noexcept {
        switch (id) {
            case SubsystemId::NERVOUS:    return {id, RateTier::FAST,   1, 1.0f};
            case SubsystemId::TEMPORAL:   return {id, RateTier::FAST,   1, 0.9f};
            case SubsystemId::ATTENTION:  return {id, RateTier::FAST,   1, 0.8f};
            case SubsystemId::BRIDGE:     return {id, RateTier::FAST,   1, 0.7f};
            case SubsystemId::REASONING:  return {id, RateTier::MEDIUM, 4, 0.6f};
            case SubsystemId::ENTELECHY:  return {id, RateTier::MEDIUM, 4, 0.5f};
            case SubsystemId::AFI:        return {id, RateTier::MEDIUM, 8, 0.4f};
            case SubsystemId::ENDOCRINE:  return {id, RateTier::SLOW,  16, 0.3f};
            default:                      return {id, RateTier::MEDIUM, 4, 0.5f};
        }
    }
};

// ============================================================================
// Sync Epoch — 16 bytes
// ============================================================================

/**
 * @brief A numbered synchronization epoch with timing metadata
 *
 * Each master tick increments the epoch. Subsystems check the epoch
 * number against their rate divisor to decide whether to tick.
 */
struct alignas(16) SyncEpoch {
    uint64_t number{0};          ///< Monotonic epoch counter
    float dt{0.0f};              ///< Time step for this epoch (seconds)
    float wall_elapsed{0.0f};    ///< Wall-clock elapsed since last epoch (seconds)

    [[nodiscard]] bool should_tick(uint32_t divisor) const noexcept {
        return divisor == 0 || (number % divisor) == 0;
    }

    constexpr auto operator<=>(const SyncEpoch&) const = default;
};

static_assert(sizeof(SyncEpoch) == 16);

// ============================================================================
// Sync Event — diagnostics / history
// ============================================================================

/**
 * @brief Type of synchronization event recorded for diagnostics
 */
enum class SyncEventType : uint8_t {
    EPOCH_START,           ///< New epoch began
    PHASE_ENTER,           ///< Entered a pipeline phase
    SUBSYSTEM_TICK,        ///< A subsystem completed its tick
    BARRIER_WAIT,          ///< Waiting at a sync barrier
    BARRIER_RELEASE,       ///< Barrier released (coherence threshold met)
    COHERENCE_DROP,        ///< Cross-system coherence dropped below threshold
    COHERENCE_RESTORE,     ///< Cross-system coherence restored above threshold
    RESYNC_TRIGGERED,      ///< Forced resynchronization triggered
};

/// Human-readable event type names
[[nodiscard]] constexpr std::string_view event_type_name(SyncEventType type) noexcept {
    constexpr std::string_view names[] = {
        "EpochStart", "PhaseEnter", "SubsystemTick", "BarrierWait",
        "BarrierRelease", "CoherenceDrop", "CoherenceRestore", "ResyncTriggered"
    };
    return names[static_cast<size_t>(type)];
}

/**
 * @brief A recorded synchronization event
 */
struct SyncEvent {
    SyncEventType type;
    uint64_t epoch;
    SyncPhase phase;
    SubsystemId subsystem;     ///< Which subsystem (if applicable)
    float coherence;           ///< Coherence at event time
    float value;               ///< Event-specific value
};

// ============================================================================
// Subsystem State Snapshot
// ============================================================================

/**
 * @brief Per-subsystem state tracked by the SynchronizationManager
 */
struct SubsystemState {
    SubsystemId id;
    bool enabled{true};           ///< Whether this subsystem participates
    uint64_t last_tick_epoch{0};  ///< Last epoch this subsystem ticked
    uint64_t tick_count{0};       ///< Total ticks executed
    float last_dt{0.0f};         ///< Last dt used
    float cumulative_time{0.0f}; ///< Total simulated time for this subsystem

    [[nodiscard]] bool is_stale(uint64_t current_epoch, uint32_t max_staleness) const noexcept {
        return enabled && (current_epoch - last_tick_epoch) > max_staleness;
    }
};

// ============================================================================
// Sync Metrics
// ============================================================================

/**
 * @brief Aggregate synchronization metrics
 */
struct SyncMetrics {
    uint64_t total_epochs{0};
    uint64_t total_barriers{0};
    uint64_t barrier_waits{0};        ///< Times a barrier had to wait
    uint64_t resync_count{0};         ///< Forced resynchronizations
    float mean_coherence{0.0f};       ///< Running mean coherence
    float min_coherence{1.0f};        ///< Minimum observed coherence
    float max_coherence{0.0f};        ///< Maximum observed coherence
    float mean_epoch_dt{0.0f};        ///< Mean epoch duration
    std::array<uint64_t, SUBSYSTEM_COUNT> subsystem_ticks{};

    void update_coherence(float c) noexcept {
        if (total_epochs == 0) {
            mean_coherence = c;
        } else {
            // Exponential moving average (alpha = 0.01)
            mean_coherence = mean_coherence * 0.99f + c * 0.01f;
        }
        min_coherence = std::min(min_coherence, c);
        max_coherence = std::max(max_coherence, c);
    }

    void reset() noexcept { *this = SyncMetrics{}; }
};

// ============================================================================
// Sync Configuration
// ============================================================================

/**
 * @brief Global configuration for the SynchronizationManager
 */
struct SyncConfig {
    /// Master tick rate (target dt in seconds, 0 = variable)
    float target_dt{0.01f};

    /// Phase-coherence threshold for barrier release [0,1]
    /// Maps to CrystalBus global_coherence() — when coherence exceeds
    /// this threshold, the barrier releases and the next phase begins.
    float barrier_coherence_threshold{0.3f};

    /// Maximum epochs a barrier will wait before force-releasing
    uint32_t barrier_max_wait{4};

    /// Coherence threshold below which a resync is triggered
    float resync_coherence_threshold{0.1f};

    /// Maximum staleness (epochs) before a subsystem is force-ticked
    uint32_t max_staleness{32};

    /// Per-subsystem rate overrides (uses defaults if empty)
    std::array<SubsystemRate, SUBSYSTEM_COUNT> rates{
        SubsystemRate::defaults(SubsystemId::ENDOCRINE),
        SubsystemRate::defaults(SubsystemId::NERVOUS),
        SubsystemRate::defaults(SubsystemId::TEMPORAL),
        SubsystemRate::defaults(SubsystemId::BRIDGE),
        SubsystemRate::defaults(SubsystemId::ATTENTION),
        SubsystemRate::defaults(SubsystemId::REASONING),
        SubsystemRate::defaults(SubsystemId::ENTELECHY),
        SubsystemRate::defaults(SubsystemId::AFI),
    };

    /// Maximum sync events to retain in history ring buffer
    size_t max_history{512};
};

} // namespace opencog::sync
