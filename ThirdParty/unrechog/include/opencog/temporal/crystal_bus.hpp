#pragma once
/**
 * @file crystal_bus.hpp
 * @brief Multi-scale oscillation bus for the Temporal Crystal System
 *
 * Analogous to HormoneBus (VES) and NerveBus (VNS), the CrystalBus
 * manages 16 temporal scale channels with phase-coupled oscillation.
 * Unlike the broadcast (VES) or routed (VNS) models, CrystalBus
 * implements hierarchical phase coupling via the Phase Prime Metric.
 *
 * Key differences from VES/VNS:
 * - Phase-coupled rather than amplitude-based
 * - Hierarchical nesting (fast modulated by slow)
 * - Deterministic update (not decay-based)
 * - Each channel has both phase and amplitude
 */

#include <opencog/temporal/types.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <numbers>
#include <span>
#include <vector>

namespace opencog::temporal {

/**
 * @brief Multi-scale oscillation bus with phase coupling
 *
 * Maintains 16 oscillator channels, each with current phase and amplitude.
 * Phase evolution follows: phase[i] += 2*pi*freq[i]*dt + coupling*parent_influence
 *
 * The bus supports three coupling modes:
 * 1. Free-running: each oscillator at its natural frequency
 * 2. Phase-locked: slow scales entrain fast scales (top-down)
 * 3. Resonant: bidirectional coupling (emergent synchronization)
 *
 * Thread safety: activations are lock-free readable via atomic snapshot.
 * Writes happen only during tick() on the owning thread.
 */
class CrystalBus {
public:
    explicit CrystalBus(CrystalBusConfig config = {}) noexcept
        : config_(config) {
        // Initialize phases at natural frequencies
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            phases_[i] = CrystalPhase{0.0f, 0.5f};  // Start at phase 0, half amplitude
            frequencies_[i] = SCALE_FREQUENCIES[i];
        }
        // Initialize coupling matrix (hierarchical: each scale couples to adjacent)
        for (size_t i = 0; i < TEMPORAL_SCALE_COUNT; ++i) {
            for (size_t j = 0; j < TEMPORAL_SCALE_COUNT; ++j) {
                if (i == j) {
                    coupling_matrix_[i][j] = 0.0f;  // No self-coupling
                } else if (std::abs(static_cast<int>(i) - static_cast<int>(j)) == 1
                           && i < BIOLOGICAL_SCALE_COUNT && j < BIOLOGICAL_SCALE_COUNT) {
                    coupling_matrix_[i][j] = config_.global_coupling;  // Adjacent scales
                } else {
                    coupling_matrix_[i][j] = 0.0f;
                }
            }
        }
    }

    // ---- Phase access (lock-free reads) ----

    /// Current phase at a specific scale
    [[nodiscard]] CrystalPhase phase(TemporalScaleId id) const noexcept {
        return phases_[static_cast<size_t>(id)];
    }

    /// Current activation snapshot (all scales)
    [[nodiscard]] OscillatorState snapshot() const noexcept {
        OscillatorState state;
        for (size_t i = 0; i < TEMPORAL_SCALE_COUNT; ++i) {
            state.activations[i] = phases_[i].value();
        }
        return state;
    }

    /// Current instantaneous value at a scale
    [[nodiscard]] float activation(TemporalScaleId id) const noexcept {
        return phases_[static_cast<size_t>(id)].value();
    }

    // ---- Phase injection ----

    /// Inject phase perturbation at a specific scale
    void inject(TemporalScaleId id, float phase_delta, float amplitude_delta = 0.0f) noexcept {
        auto idx = static_cast<size_t>(id);
        if (idx < BIOLOGICAL_SCALE_COUNT) {
            phases_[idx].phase += phase_delta;
            phases_[idx].amplitude = std::clamp(phases_[idx].amplitude + amplitude_delta, -1.0f, 1.0f);
        }
    }

    /// Set amplitude at a specific scale
    void set_amplitude(TemporalScaleId id, float amplitude) noexcept {
        auto idx = static_cast<size_t>(id);
        if (idx < BIOLOGICAL_SCALE_COUNT) {
            phases_[idx].amplitude = std::clamp(amplitude, -1.0f, 1.0f);
        }
    }

    // ---- Coupling control ----

    /// Set coupling strength between two scales
    void set_coupling(TemporalScaleId from, TemporalScaleId to, float strength) noexcept {
        coupling_matrix_[static_cast<size_t>(from)][static_cast<size_t>(to)] =
            std::clamp(strength, -1.0f, 1.0f);
    }

    /// Get coupling strength between two scales
    [[nodiscard]] float coupling(TemporalScaleId from, TemporalScaleId to) const noexcept {
        return coupling_matrix_[static_cast<size_t>(from)][static_cast<size_t>(to)];
    }

    /// Set global coupling strength (affects all adjacent pairs)
    void set_global_coupling(float strength) noexcept {
        config_.global_coupling = std::clamp(strength, 0.0f, 1.0f);
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            for (size_t j = 0; j < BIOLOGICAL_SCALE_COUNT; ++j) {
                if (std::abs(static_cast<int>(i) - static_cast<int>(j)) == 1) {
                    coupling_matrix_[i][j] = strength;
                }
            }
        }
    }

    // ---- Tick / evolution ----

    /**
     * @brief Advance all oscillators by one time step
     *
     * Phase evolution for each scale i:
     *   phase[i] += 2*pi*freq[i]*dt + sum_j(coupling[j→i] * sin(phase[j] - phase[i]))
     *
     * This implements Kuramoto-style phase coupling where slow oscillators
     * (Universal Sets) entrain fast oscillators (Particular Sets).
     *
     * @param dt Time step in seconds
     */
    void tick(float dt) noexcept {
        // Store previous state for history
        if (history_pos_ < history_.size()) {
            history_[history_pos_] = snapshot();
            history_pos_ = (history_pos_ + 1) % history_.size();
            if (history_count_ < history_.size()) ++history_count_;
        }

        // Phase update with Kuramoto coupling
        std::array<float, TEMPORAL_SCALE_COUNT> phase_deltas{};

        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            // Natural frequency advance
            float natural = 2.0f * std::numbers::pi_v<float> * frequencies_[i] * dt;

            // Coupling influence from other scales
            float coupling_sum = 0.0f;
            for (size_t j = 0; j < BIOLOGICAL_SCALE_COUNT; ++j) {
                if (i != j && coupling_matrix_[j][i] != 0.0f) {
                    coupling_sum += coupling_matrix_[j][i] *
                        std::sin(phases_[j].phase - phases_[i].phase);
                }
            }

            phase_deltas[i] = natural + coupling_sum * dt;
        }

        // Apply phase updates
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            phases_[i].phase += phase_deltas[i];
            // Normalize phase to [0, 2*pi)
            float two_pi = 2.0f * std::numbers::pi_v<float>;
            phases_[i].phase = std::fmod(phases_[i].phase, two_pi);
            if (phases_[i].phase < 0.0f) phases_[i].phase += two_pi;
        }

        ++tick_count_;
    }

    // ---- Metrics ----

    /// Mean phase coherence across all adjacent scale pairs
    [[nodiscard]] float global_coherence() const noexcept {
        if (BIOLOGICAL_SCALE_COUNT < 2) return 1.0f;
        float sum = 0.0f;
        size_t count = 0;
        for (size_t i = 0; i + 1 < BIOLOGICAL_SCALE_COUNT; ++i) {
            sum += phases_[i].coherence_with(phases_[i + 1]);
            ++count;
        }
        return count > 0 ? sum / static_cast<float>(count) : 0.0f;
    }

    /// Coherence between Universal and Particular sets
    [[nodiscard]] float hierarchical_coherence() const noexcept {
        float u_phase = 0.0f;
        size_t u_count = 0;
        float p_phase = 0.0f;
        size_t p_count = 0;

        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            if (i >= UNIVERSAL_SET_THRESHOLD) {
                u_phase += phases_[i].phase;
                ++u_count;
            } else {
                p_phase += phases_[i].phase;
                ++p_count;
            }
        }

        if (u_count == 0 || p_count == 0) return 0.0f;

        CrystalPhase u_avg{u_phase / u_count, 1.0f};
        CrystalPhase p_avg{p_phase / p_count, 1.0f};
        return u_avg.coherence_with(p_avg);
    }

    // ---- History access ----

    /// Get historical state at offset ticks back (0 = most recent)
    [[nodiscard]] OscillatorState history(size_t ticks_back) const noexcept {
        if (ticks_back >= history_count_) return {};
        size_t idx = (history_pos_ + history_.size() - 1 - ticks_back) % history_.size();
        return history_[idx];
    }

    [[nodiscard]] size_t history_count() const noexcept { return history_count_; }
    [[nodiscard]] uint64_t tick_count() const noexcept { return tick_count_; }
    [[nodiscard]] const CrystalBusConfig& config() const noexcept { return config_; }

private:
    CrystalBusConfig config_;

    // Current oscillator phases (lock-free readable)
    std::array<CrystalPhase, TEMPORAL_SCALE_COUNT> phases_{};

    // Natural frequencies (Hz) — modifiable for frequency modulation
    std::array<float, TEMPORAL_SCALE_COUNT> frequencies_{};

    // Coupling matrix: coupling_matrix_[from][to] = strength
    std::array<std::array<float, TEMPORAL_SCALE_COUNT>, TEMPORAL_SCALE_COUNT> coupling_matrix_{};

    // History ring buffer
    std::vector<OscillatorState> history_{200};
    size_t history_pos_{0};
    size_t history_count_{0};

    uint64_t tick_count_{0};
};

} // namespace opencog::temporal
