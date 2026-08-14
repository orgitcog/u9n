#pragma once
/**
 * @file hormone_bus.hpp
 * @brief Central broadcast bus for the Virtual Endocrine System
 *
 * The HormoneBus holds a shared concentration vector that all virtual glands
 * write to and all modulated subsystems read from. It provides:
 * - Lock-free atomic reads (hot path)
 * - SIMD-optimized exponential decay toward baselines
 * - Cognitive mode classification via nearest-centroid in hormone space
 * - History ring buffer for trend analysis
 * - Callbacks for mode transitions and threshold crossings
 */

#include <opencog/endocrine/types.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace opencog::endo {

class HormoneBus {
public:
    explicit HormoneBus(HormoneBusConfig config = {})
        : config_(config)
    {
        // Initialize all concentrations to zero
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            concentrations_[i].store(0.0f, std::memory_order_relaxed);
        }

        // Default channel configs
        setup_default_channels();

        // Pre-compute decay factors from half-lives
        recompute_decay_factors();

        // Allocate history ring buffer
        history_.resize(config_.history_length);

        // Initialize mode prototypes
        setup_mode_prototypes();

        // Initialize gain arrays to 1.0 (no modification)
        production_gains_.fill(1.0f);
        decay_gains_.fill(1.0f);
    }

    // ========================================================================
    // Hot Path — Lock-Free Reads
    // ========================================================================

    /// Read a single hormone concentration (lock-free)
    [[nodiscard]] float concentration(HormoneId id) const noexcept {
        return concentrations_[static_cast<size_t>(id)].load(std::memory_order_relaxed);
    }

    /// Snapshot all concentrations (lock-free, may be slightly inconsistent across channels)
    [[nodiscard]] EndocrineState snapshot() const noexcept {
        EndocrineState state;
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            state.concentrations[i] = concentrations_[i].load(std::memory_order_relaxed);
        }
        return state;
    }

    /// Current cognitive mode (lock-free)
    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return current_mode_.load(std::memory_order_relaxed);
    }

    // ========================================================================
    // Production — Atomic Updates
    // ========================================================================

    /// Add production to a hormone channel (scaled by production gain)
    void produce(HormoneId id, float amount) noexcept {
        auto idx = static_cast<size_t>(id);
        float old_val = concentrations_[idx].load(std::memory_order_relaxed);
        float new_val = std::clamp(old_val + amount * production_gains_[idx], 0.0f, saturation_ceilings_[idx]);
        concentrations_[idx].store(new_val, std::memory_order_release);
    }

    /// Set a hormone to a specific level
    void set_concentration(HormoneId id, float level) noexcept {
        auto idx = static_cast<size_t>(id);
        concentrations_[idx].store(
            std::clamp(level, 0.0f, saturation_ceilings_[idx]),
            std::memory_order_release
        );
    }

    // ========================================================================
    // Dynamics — Advance One Time Step
    // ========================================================================

    /// Full tick: decay, clamp, record history, update mode
    void tick() {
        decay_all();
        record_history();
        update_mode();
        tick_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /// SIMD-optimized exponential decay toward baselines
    void decay_all() noexcept {
        // Load current concentrations into local array
        alignas(SIMD_ALIGN) float conc[HORMONE_COUNT];
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            conc[i] = concentrations_[i].load(std::memory_order_relaxed);
        }

#ifdef __AVX2__
        if (config_.enable_simd) {
            // Process 8 channels at a time (4 iterations for 32 channels)
            for (size_t i = 0; i < HORMONE_COUNT; i += 8) {
                __m256 v_conc = _mm256_load_ps(conc + i);
                __m256 v_decay = _mm256_load_ps(decay_factors_.data() + i);
                __m256 v_base = _mm256_load_ps(baselines_.data() + i);
                // concentration = baseline + (concentration - baseline) * decay_factor
                __m256 delta = _mm256_sub_ps(v_conc, v_base);
                delta = _mm256_mul_ps(delta, v_decay);
                v_conc = _mm256_add_ps(v_base, delta);
                _mm256_store_ps(conc + i, v_conc);
            }
        } else
#endif
        {
            // Scalar fallback
            for (size_t i = 0; i < HORMONE_COUNT; ++i) {
                float delta = conc[i] - baselines_[i];
                conc[i] = baselines_[i] + delta * decay_factors_[i];
            }
        }

        // Write back atomically with clamping
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            concentrations_[i].store(
                std::clamp(conc[i], 0.0f, saturation_ceilings_[i]),
                std::memory_order_release
            );
        }
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_channel_config(HormoneId id, HormoneChannelConfig cfg) {
        std::unique_lock lock(config_mutex_);
        auto idx = static_cast<size_t>(id);
        channel_configs_[idx] = cfg;
        baselines_[idx] = cfg.baseline;
        saturation_ceilings_[idx] = cfg.saturation_ceiling;
        recompute_decay_factor(idx);
    }

    [[nodiscard]] const HormoneChannelConfig& channel_config(HormoneId id) const {
        return channel_configs_[static_cast<size_t>(id)];
    }

    void set_config(HormoneBusConfig config) {
        std::unique_lock lock(config_mutex_);
        config_ = config;
        history_.resize(config_.history_length);
        recompute_decay_factors();
    }

    [[nodiscard]] const HormoneBusConfig& config() const noexcept {
        return config_;
    }

    // ========================================================================
    // Production / Decay Gains (Cloninger temperament modulation)
    // ========================================================================

    /// Set production gains (multiplicative on produce())
    void set_production_gains(const std::array<float, HORMONE_COUNT>& gains) noexcept {
        production_gains_ = gains;
    }

    /// Set decay gains (multiplicative on half-life)
    void set_decay_gains(const std::array<float, HORMONE_COUNT>& gains) noexcept {
        decay_gains_ = gains;
        // Recompute decay factors with new gains
        recompute_decay_factors();
    }

    [[nodiscard]] const std::array<float, HORMONE_COUNT>& production_gains() const noexcept {
        return production_gains_;
    }

    [[nodiscard]] const std::array<float, HORMONE_COUNT>& decay_gains() const noexcept {
        return decay_gains_;
    }

    // ========================================================================
    // History
    // ========================================================================

    /// Access the history ring buffer (most recent first)
    [[nodiscard]] const EndocrineState& history_at(size_t ticks_ago) const {
        size_t idx = (history_head_ + history_.size() - ticks_ago) % history_.size();
        return history_[idx];
    }

    /// Average concentration over a window of recent ticks
    [[nodiscard]] float average_concentration(HormoneId id, size_t window) const {
        window = std::min(window, std::min(history_.size(),
                          static_cast<size_t>(tick_count_.load(std::memory_order_relaxed))));
        if (window == 0) return concentration(id);

        float sum = 0.0f;
        auto ch = static_cast<size_t>(id);
        for (size_t i = 0; i < window; ++i) {
            size_t idx = (history_head_ + history_.size() - i) % history_.size();
            sum += history_[idx].concentrations[ch];
        }
        return sum / static_cast<float>(window);
    }

    // ========================================================================
    // Callbacks
    // ========================================================================

    /// Called when cognitive mode transitions
    void on_mode_change(std::function<void(CognitiveMode /*old*/, CognitiveMode /*new_mode*/)> cb) {
        mode_change_cb_ = std::move(cb);
    }

    /// Called when a hormone crosses a threshold (rising)
    void on_threshold_crossed(HormoneId id, float threshold,
                              std::function<void(HormoneId, float)> cb) {
        threshold_cbs_.emplace_back(id, threshold, std::move(cb));
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t tick_count() const noexcept {
        return tick_count_.load(std::memory_order_relaxed);
    }

private:
    // === Hot data: SIMD-aligned contiguous arrays ===
    alignas(SIMD_ALIGN) std::array<std::atomic<float>, HORMONE_COUNT> concentrations_{};
    alignas(SIMD_ALIGN) std::array<float, HORMONE_COUNT> decay_factors_{};
    alignas(SIMD_ALIGN) std::array<float, HORMONE_COUNT> saturation_ceilings_{};
    alignas(SIMD_ALIGN) std::array<float, HORMONE_COUNT> baselines_{};
    alignas(SIMD_ALIGN) std::array<float, HORMONE_COUNT> production_gains_{};
    alignas(SIMD_ALIGN) std::array<float, HORMONE_COUNT> decay_gains_{};

    // === Channel configs (cold) ===
    std::array<HormoneChannelConfig, HORMONE_COUNT> channel_configs_{};

    // === Mode detection ===
    std::atomic<CognitiveMode> current_mode_{CognitiveMode::RESTING};
    std::array<EndocrineState, COGNITIVE_MODE_COUNT> mode_prototypes_{};

    // === History ring buffer ===
    std::vector<EndocrineState> history_;
    size_t history_head_{0};

    // === Callbacks ===
    std::function<void(CognitiveMode, CognitiveMode)> mode_change_cb_;
    std::vector<std::tuple<HormoneId, float, std::function<void(HormoneId, float)>>> threshold_cbs_;

    // === Config ===
    HormoneBusConfig config_;
    std::atomic<size_t> tick_count_{0};
    mutable std::shared_mutex config_mutex_;

    // === Initialization helpers ===

    void setup_default_channels() {
        auto set = [this](HormoneId id, float half_life, float ceiling, float baseline) {
            auto idx = static_cast<size_t>(id);
            channel_configs_[idx] = {id, half_life, ceiling, baseline, 0.0f};
            saturation_ceilings_[idx] = ceiling;
            baselines_[idx] = baseline;
        };

        //                   HormoneId              half_life  ceiling  baseline
        set(HormoneId::CRH,             5.0f,      1.0f,    0.05f);
        set(HormoneId::ACTH,            10.0f,     1.0f,    0.05f);
        set(HormoneId::CORTISOL,        30.0f,     1.0f,    0.15f);
        set(HormoneId::DOPAMINE_TONIC,  20.0f,     1.0f,    0.3f);
        set(HormoneId::DOPAMINE_PHASIC, 3.0f,      1.0f,    0.0f);
        set(HormoneId::SEROTONIN,       50.0f,     1.0f,    0.4f);
        set(HormoneId::NOREPINEPHRINE,  8.0f,      1.0f,    0.1f);
        set(HormoneId::OXYTOCIN,        15.0f,     1.0f,    0.1f);
        set(HormoneId::T3_T4,           100.0f,    1.0f,    0.5f);
        set(HormoneId::MELATONIN,       12.0f,     1.0f,    0.0f);
        set(HormoneId::INSULIN,         10.0f,     1.0f,    0.2f);
        set(HormoneId::GLUCAGON,        8.0f,      1.0f,    0.1f);
        set(HormoneId::IL6,             20.0f,     1.0f,    0.05f);
        set(HormoneId::ANANDAMIDE,      6.0f,      1.0f,    0.1f);
        set(HormoneId::NPU_LOAD,        10.0f,     1.0f,    0.0f);
        set(HormoneId::COG_COHERENCE,   10.0f,     1.0f,    0.0f);

        // Marduk integration channels (16-17)
        set(HormoneId::MARDUK_LOAD,     10.0f,     1.0f,    0.0f);
        set(HormoneId::ORG_COHERENCE,   10.0f,     1.0f,    0.0f);

        // VirtualTouchpad integration channels (18-19)
        set(HormoneId::TOUCHPAD_LOAD,      10.0f,     1.0f,    0.0f);
        set(HormoneId::TOUCHPAD_COHERENCE, 10.0f,     1.0f,    0.0f);

        // Interoceptive channels (20-31) — Porges/Craig/McEwen
        set(HormoneId::VAGAL_TONE,          50.0f,     1.0f,    0.5f);
        set(HormoneId::SYMPATHETIC_DRIVE,   10.0f,     1.0f,    0.3f);
        set(HormoneId::DORSAL_VAGAL,        30.0f,     1.0f,    0.0f);
        set(HormoneId::CARDIAC_COHERENCE,   40.0f,     1.0f,    0.5f);
        set(HormoneId::RESPIRATORY_RHYTHM,  20.0f,     1.0f,    0.5f);
        set(HormoneId::GUT_BRAIN_SIGNAL,    60.0f,     1.0f,    0.3f);
        set(HormoneId::IMMUNE_EXTENDED,     80.0f,     1.0f,    0.1f);
        set(HormoneId::INSULAR_INTEGRATION, 30.0f,     1.0f,    0.5f);
        set(HormoneId::ALLOSTATIC_LOAD,     500.0f,    5.0f,    0.0f);
        set(HormoneId::PROPRIOCEPTIVE_TONE, 25.0f,     1.0f,    0.5f);
        set(HormoneId::NOCICEPTIVE_SIGNAL,  5.0f,      1.0f,    0.0f);
        set(HormoneId::THERMOREGULATORY,    100.0f,    1.0f,    0.5f);
    }

    void recompute_decay_factors() {
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            recompute_decay_factor(i);
        }
    }

    void recompute_decay_factor(size_t idx) {
        // decay_factor = exp(-ln(2) / half_life) per tick
        // Adjusted by global multiplier and per-channel decay gain
        float hl = channel_configs_[idx].half_life * config_.global_decay_multiplier * decay_gains_[idx];
        if (hl > 0.0f) {
            decay_factors_[idx] = std::exp(-0.693147f / hl); // ln(2) ≈ 0.693147
        } else {
            decay_factors_[idx] = 0.0f; // Instant decay
        }
    }

    void record_history() {
        history_head_ = (history_head_ + 1) % history_.size();
        history_[history_head_] = snapshot();
    }

    void update_mode() {
        EndocrineState state = snapshot();
        CognitiveMode best = CognitiveMode::RESTING;
        float best_dist = state.distance_to(mode_prototypes_[0]);

        for (size_t i = 1; i < COGNITIVE_MODE_COUNT; ++i) {
            float dist = state.distance_to(mode_prototypes_[i]);
            if (dist < best_dist) {
                best_dist = dist;
                best = static_cast<CognitiveMode>(i);
            }
        }

        CognitiveMode old = current_mode_.exchange(best, std::memory_order_release);
        if (old != best && mode_change_cb_) {
            mode_change_cb_(old, best);
        }

        // Check threshold callbacks
        for (auto& [hid, threshold, cb] : threshold_cbs_) {
            float val = state[hid];
            if (val >= threshold) {
                cb(hid, val);
            }
        }
    }

    void setup_mode_prototypes() {
        // Each prototype defines the "ideal" hormone constellation for a mode.
        // Mode classification finds the nearest prototype.

        auto& resting = mode_prototypes_[0];
        // Low everything, near baselines — default zeroed state works

        auto& exploratory = mode_prototypes_[1];
        exploratory[HormoneId::DOPAMINE_TONIC] = 0.6f;
        exploratory[HormoneId::SEROTONIN] = 0.4f;
        exploratory[HormoneId::NOREPINEPHRINE] = 0.2f;

        auto& focused = mode_prototypes_[2];
        focused[HormoneId::NOREPINEPHRINE] = 0.6f;
        focused[HormoneId::CORTISOL] = 0.3f;
        focused[HormoneId::DOPAMINE_TONIC] = 0.4f;

        auto& stressed = mode_prototypes_[3];
        stressed[HormoneId::CORTISOL] = 0.7f;
        stressed[HormoneId::NOREPINEPHRINE] = 0.6f;
        stressed[HormoneId::CRH] = 0.4f;
        stressed[HormoneId::ACTH] = 0.4f;

        auto& social = mode_prototypes_[4];
        social[HormoneId::OXYTOCIN] = 0.7f;
        social[HormoneId::SEROTONIN] = 0.5f;
        social[HormoneId::DOPAMINE_TONIC] = 0.4f;

        auto& reflective = mode_prototypes_[5];
        reflective[HormoneId::SEROTONIN] = 0.7f;
        reflective[HormoneId::NOREPINEPHRINE] = 0.1f;
        reflective[HormoneId::DOPAMINE_TONIC] = 0.3f;

        auto& vigilant = mode_prototypes_[6];
        vigilant[HormoneId::NOREPINEPHRINE] = 0.8f;
        vigilant[HormoneId::CORTISOL] = 0.4f;

        auto& maintenance = mode_prototypes_[7];
        maintenance[HormoneId::MELATONIN] = 0.8f;
        maintenance[HormoneId::SEROTONIN] = 0.3f;

        auto& reward = mode_prototypes_[8];
        reward[HormoneId::DOPAMINE_PHASIC] = 0.8f;
        reward[HormoneId::DOPAMINE_TONIC] = 0.5f;

        auto& threat = mode_prototypes_[9];
        threat[HormoneId::CORTISOL] = 0.9f;
        threat[HormoneId::NOREPINEPHRINE] = 0.8f;
        threat[HormoneId::CRH] = 0.7f;
        threat[HormoneId::ACTH] = 0.6f;

        (void)resting; // suppress unused warning
    }
};

} // namespace opencog::endo
