#pragma once
/**
 * @file nerve_bus.hpp
 * @brief Central routed signaling bus for the Virtual Nervous System
 *
 * The NerveBus holds a 64-channel activation vector with a 64×64 connectivity
 * matrix that routes signals between channels. Unlike the VES HormoneBus
 * (broadcast), the NerveBus uses matrix multiplication for signal propagation.
 *
 * Key differences from HormoneBus:
 * - 64 channels (vs 32), bipolar [-1,+1] (vs [0,1])
 * - Routed via connectivity matrix (vs broadcast)
 * - Fast linear decay modulated by urgency (vs exponential toward baseline)
 * - Short history (100 ticks vs 1000)
 * - Processing level classification (4 levels vs 10 cognitive modes)
 * - 10ms tick rate (vs 100ms)
 */

#include <opencog/nervous/types.hpp>

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

namespace opencog::nerv {

class NerveBus {
public:
    explicit NerveBus(NerveBusConfig config = {})
        : config_(config)
    {
        // Initialize all activations to zero
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            activations_[i].store(0.0f, std::memory_order_relaxed);
            urgencies_[i] = 0.0f;
        }

        // Zero connectivity matrix
        connectivity_.fill(0.0f);

        // Setup default pathways
        setup_default_connectivity();

        // Allocate history ring buffer
        history_.resize(config_.history_length);

        // Initialize level prototypes
        setup_level_prototypes();
    }

    // ========================================================================
    // Hot Path — Lock-Free Reads
    // ========================================================================

    /// Read a single channel activation (lock-free)
    [[nodiscard]] float activation(NeuralChannelId id) const noexcept {
        return activations_[static_cast<size_t>(id)].load(std::memory_order_relaxed);
    }

    /// Snapshot all activations (lock-free, may be slightly inconsistent)
    [[nodiscard]] NeuralState snapshot() const noexcept {
        NeuralState state;
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            state.activations[i] = activations_[i].load(std::memory_order_relaxed);
        }
        return state;
    }

    /// Current processing level (lock-free)
    [[nodiscard]] ProcessingLevel current_level() const noexcept {
        return current_level_.load(std::memory_order_relaxed);
    }

    // ========================================================================
    // Signal Injection — Atomic Updates
    // ========================================================================

    /// Fire an excitatory or inhibitory signal on a channel
    void fire(NeuralChannelId id, float act, float urgency = 1.0f) noexcept {
        auto idx = static_cast<size_t>(id);
        float old_val = activations_[idx].load(std::memory_order_relaxed);
        float new_val = std::clamp(old_val + act, -1.0f, 1.0f);
        activations_[idx].store(new_val, std::memory_order_release);
        urgencies_[idx] = std::max(urgencies_[idx], urgency);
    }

    /// Inhibit a channel (convenience: negative activation)
    void inhibit(NeuralChannelId id, float strength) noexcept {
        fire(id, -std::abs(strength));
    }

    /// Set a channel to a specific activation level
    void set_activation(NeuralChannelId id, float level) noexcept {
        auto idx = static_cast<size_t>(id);
        activations_[idx].store(std::clamp(level, -1.0f, 1.0f),
                                 std::memory_order_release);
    }

    // ========================================================================
    // Dynamics — Advance One Time Step
    // ========================================================================

    /// Full tick: propagate, decay, record history, update level
    void tick() {
        if (config_.enable_propagation) {
            propagate_all();
        }
        decay_all();
        record_history();
        update_level();
        tick_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // ========================================================================
    // Connectivity Matrix
    // ========================================================================

    /// Set a synaptic weight between two channels
    void set_synapse(NeuralChannelId from, NeuralChannelId to, float weight) {
        auto f = static_cast<size_t>(from);
        auto t = static_cast<size_t>(to);
        connectivity_[f * NEURAL_CHANNEL_COUNT + t] = weight;
    }

    /// Get a synaptic weight
    [[nodiscard]] float synapse(NeuralChannelId from, NeuralChannelId to) const {
        auto f = static_cast<size_t>(from);
        auto t = static_cast<size_t>(to);
        return connectivity_[f * NEURAL_CHANNEL_COUNT + t];
    }

    /// Set a full pathway with a single weight
    void set_pathway(NeuralChannelId from, NeuralChannelId to,
                     float weight, NeuralPolarity /*polarity*/ = NeuralPolarity::SOMATIC) {
        set_synapse(from, to, weight);
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(NerveBusConfig config) {
        std::unique_lock lock(config_mutex_);
        config_ = config;
        history_.resize(config_.history_length);
    }

    [[nodiscard]] const NerveBusConfig& config() const noexcept {
        return config_;
    }

    // ========================================================================
    // History
    // ========================================================================

    /// Access the history ring buffer (most recent first)
    [[nodiscard]] const NeuralState& history_at(size_t ticks_ago) const {
        size_t idx = (history_head_ + history_.size() - ticks_ago) % history_.size();
        return history_[idx];
    }

    /// Average activation over a window of recent ticks
    [[nodiscard]] float average_activation(NeuralChannelId id, size_t window) const {
        window = std::min(window, std::min(history_.size(),
                          static_cast<size_t>(tick_count_.load(std::memory_order_relaxed))));
        if (window == 0) return activation(id);

        float sum = 0.0f;
        auto ch = static_cast<size_t>(id);
        for (size_t i = 0; i < window; ++i) {
            size_t idx = (history_head_ + history_.size() - i) % history_.size();
            sum += history_[idx].activations[ch];
        }
        return sum / static_cast<float>(window);
    }

    // ========================================================================
    // Callbacks
    // ========================================================================

    /// Called when processing level transitions
    void on_level_change(std::function<void(ProcessingLevel, ProcessingLevel)> cb) {
        level_change_cb_ = std::move(cb);
    }

    /// Called when a channel crosses a threshold (rising edge)
    void on_threshold_crossed(NeuralChannelId id, float threshold,
                              std::function<void(NeuralChannelId, float)> cb) {
        threshold_cbs_.emplace_back(id, threshold, std::move(cb));
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t tick_count() const noexcept {
        return tick_count_.load(std::memory_order_relaxed);
    }

    // ========================================================================
    // Reflex Arcs
    // ========================================================================

    /// Register a reflex arc (direct sensory→motor shortcut)
    void add_reflex(ReflexArc arc) {
        reflexes_.push_back(arc);
    }

    /// Process all reflex arcs (called in tick pipeline phase 5)
    void process_reflexes() {
        for (auto& r : reflexes_) {
            float sensory = activations_[static_cast<size_t>(r.sensory_channel)]
                                .load(std::memory_order_relaxed);
            if (std::abs(sensory) >= r.threshold) {
                float response = sensory * r.gain;
                fire(r.motor_channel, response, 1.0f);
            }
        }
    }

private:
    // === Hot data: SIMD-aligned contiguous arrays ===
    alignas(SIMD_ALIGN) std::array<std::atomic<float>, NEURAL_CHANNEL_COUNT> activations_{};
    alignas(SIMD_ALIGN) std::array<float, NEURAL_CHANNEL_COUNT> urgencies_{};

    // 64×64 connectivity matrix (16KB, fits L1 cache)
    alignas(SIMD_ALIGN) std::array<float, NEURAL_CHANNEL_COUNT * NEURAL_CHANNEL_COUNT> connectivity_{};

    // === Level detection ===
    std::atomic<ProcessingLevel> current_level_{ProcessingLevel::CORTICAL};
    std::array<NeuralState, PROCESSING_LEVEL_COUNT> level_prototypes_{};

    // === History ring buffer ===
    std::vector<NeuralState> history_;
    size_t history_head_{0};

    // === Callbacks ===
    std::function<void(ProcessingLevel, ProcessingLevel)> level_change_cb_;
    std::vector<std::tuple<NeuralChannelId, float,
                           std::function<void(NeuralChannelId, float)>>> threshold_cbs_;

    // === Reflex arcs ===
    std::vector<ReflexArc> reflexes_;

    // === Config ===
    NerveBusConfig config_;
    std::atomic<size_t> tick_count_{0};
    mutable std::shared_mutex config_mutex_;

    // === Signal Propagation ===

    /// Matrix-vector multiply: new_activations = connectivity × activations
    void propagate_all() noexcept {
        // Load current activations into local array
        alignas(SIMD_ALIGN) float current[NEURAL_CHANNEL_COUNT];
        alignas(SIMD_ALIGN) float propagated[NEURAL_CHANNEL_COUNT];

        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            current[i] = activations_[i].load(std::memory_order_relaxed);
            propagated[i] = 0.0f;
        }

        // Matrix-vector multiply: propagated[to] = sum(connectivity[from][to] * current[from])
#ifdef __AVX2__
        if (config_.enable_simd) {
            for (size_t from = 0; from < NEURAL_CHANNEL_COUNT; ++from) {
                if (std::abs(current[from]) < 0.001f) continue; // skip silent channels
                __m256 v_src = _mm256_set1_ps(current[from]);
                for (size_t to = 0; to < NEURAL_CHANNEL_COUNT; to += 8) {
                    __m256 v_conn = _mm256_load_ps(
                        connectivity_.data() + from * NEURAL_CHANNEL_COUNT + to);
                    __m256 v_prop = _mm256_load_ps(propagated + to);
                    v_prop = _mm256_fmadd_ps(v_src, v_conn, v_prop);
                    _mm256_store_ps(propagated + to, v_prop);
                }
            }
        } else
#endif
        {
            for (size_t from = 0; from < NEURAL_CHANNEL_COUNT; ++from) {
                if (std::abs(current[from]) < 0.001f) continue;
                for (size_t to = 0; to < NEURAL_CHANNEL_COUNT; ++to) {
                    propagated[to] += current[from] *
                        connectivity_[from * NEURAL_CHANNEL_COUNT + to];
                }
            }
        }

        // Add propagated signals to current activations
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            if (std::abs(propagated[i]) > 0.001f) {
                float val = current[i] + propagated[i];
                activations_[i].store(std::clamp(val, -1.0f, 1.0f),
                                       std::memory_order_release);
            }
        }
    }

    // === Decay ===

    /// Fast linear decay modulated by urgency
    void decay_all() noexcept {
        alignas(SIMD_ALIGN) float conc[NEURAL_CHANNEL_COUNT];
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            conc[i] = activations_[i].load(std::memory_order_relaxed);
        }

#ifdef __AVX2__
        if (config_.enable_simd) {
            __m256 v_decay = _mm256_set1_ps(config_.global_decay_rate);
            __m256 v_zero = _mm256_setzero_ps();
            // Process 8 channels at a time (8 iterations for 64 channels)
            for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; i += 8) {
                __m256 v_act = _mm256_load_ps(conc + i);
                __m256 v_urg = _mm256_load_ps(urgencies_.data() + i);
                // Effective decay = global_decay * (1.0 + urgency)
                __m256 v_one = _mm256_set1_ps(1.0f);
                __m256 v_eff_decay = _mm256_mul_ps(v_decay, _mm256_add_ps(v_one, v_urg));
                // Decay toward zero: activation *= (1.0 - effective_decay)
                __m256 v_factor = _mm256_sub_ps(v_one, v_eff_decay);
                v_factor = _mm256_max_ps(v_factor, v_zero); // clamp factor >= 0
                v_act = _mm256_mul_ps(v_act, v_factor);
                _mm256_store_ps(conc + i, v_act);
                // Decay urgency too
                __m256 v_urg_decay = _mm256_set1_ps(0.5f);
                v_urg = _mm256_mul_ps(v_urg, v_urg_decay);
                _mm256_store_ps(const_cast<float*>(urgencies_.data() + i), v_urg);
            }
        } else
#endif
        {
            for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
                float eff_decay = config_.global_decay_rate * (1.0f + urgencies_[i]);
                float factor = std::max(0.0f, 1.0f - eff_decay);
                conc[i] *= factor;
                urgencies_[i] *= 0.5f; // urgency decays fast
            }
        }

        // Write back with clamping
        for (size_t i = 0; i < NEURAL_CHANNEL_COUNT; ++i) {
            activations_[i].store(std::clamp(conc[i], -1.0f, 1.0f),
                                   std::memory_order_release);
        }
    }

    // === History ===

    void record_history() {
        history_head_ = (history_head_ + 1) % history_.size();
        history_[history_head_] = snapshot();
    }

    // === Level Classification ===

    void update_level() {
        NeuralState state = snapshot();
        ProcessingLevel best = ProcessingLevel::SPINAL_REFLEX;
        float best_dist = state.distance_to(level_prototypes_[0]);

        for (size_t i = 1; i < PROCESSING_LEVEL_COUNT; ++i) {
            float dist = state.distance_to(level_prototypes_[i]);
            if (dist < best_dist) {
                best_dist = dist;
                best = static_cast<ProcessingLevel>(i);
            }
        }

        ProcessingLevel old = current_level_.exchange(best, std::memory_order_release);
        if (old != best && level_change_cb_) {
            level_change_cb_(old, best);
        }

        // Check threshold callbacks
        for (auto& [cid, threshold, cb] : threshold_cbs_) {
            float val = state[cid];
            if (std::abs(val) >= threshold) {
                cb(cid, val);
            }
        }
    }

    // === Default Setup ===

    void setup_default_connectivity() {
        // Sympathetic pathways (fast, 1-3 ticks)
        set_synapse(NeuralChannelId::THREAT_AFFERENT,
                    NeuralChannelId::AMYGDALA_VALENCE, 0.8f);
        set_synapse(NeuralChannelId::AMYGDALA_VALENCE,
                    NeuralChannelId::SYMPATHETIC_OUT, 0.7f);
        set_synapse(NeuralChannelId::NOVELTY_SIGNAL,
                    NeuralChannelId::RETICULAR_ACTIVATION, 0.6f);
        set_synapse(NeuralChannelId::RETICULAR_ACTIVATION,
                    NeuralChannelId::THALAMIC_GATE, 0.5f);
        set_synapse(NeuralChannelId::ERROR_SIGNAL,
                    NeuralChannelId::ANTERIOR_CINGULATE, 0.7f);
        set_synapse(NeuralChannelId::ANTERIOR_CINGULATE,
                    NeuralChannelId::BASAL_GANGLIA_NOGO, 0.6f);

        // Parasympathetic pathways (slow, homeostatic)
        set_synapse(NeuralChannelId::HOMEOSTATIC_SIGNAL,
                    NeuralChannelId::HYPOTHALAMIC_BRIDGE, 0.4f);
        set_synapse(NeuralChannelId::HYPOTHALAMIC_BRIDGE,
                    NeuralChannelId::PARASYMPATHETIC_OUT, 0.3f);
        set_synapse(NeuralChannelId::COHERENCE_SIGNAL,
                    NeuralChannelId::INSULA_INTEROCEPTION, 0.4f);
        set_synapse(NeuralChannelId::INSULA_INTEROCEPTION,
                    NeuralChannelId::ENDOCRINE_NUDGE, 0.3f);
        set_synapse(NeuralChannelId::FATIGUE_SIGNAL,
                    NeuralChannelId::BRAINSTEM_AUTONOMIC, 0.5f);
        set_synapse(NeuralChannelId::BRAINSTEM_AUTONOMIC,
                    NeuralChannelId::REPAIR_SIGNAL, 0.4f);

        // Somatic pathways (voluntary, deliberate)
        set_synapse(NeuralChannelId::PREFRONTAL_EXEC,
                    NeuralChannelId::BASAL_GANGLIA_GO, 0.6f);
        set_synapse(NeuralChannelId::BASAL_GANGLIA_GO,
                    NeuralChannelId::MOTOR_TASK_DISPATCH, 0.5f);
        set_synapse(NeuralChannelId::REASONING_CORTEX,
                    NeuralChannelId::WORKING_MEMORY, 0.5f);
        set_synapse(NeuralChannelId::WORKING_MEMORY,
                    NeuralChannelId::MOTOR_INFERENCE, 0.4f);
        set_synapse(NeuralChannelId::LANGUAGE_CORTEX,
                    NeuralChannelId::ASSOCIATION_CORTEX, 0.5f);
        set_synapse(NeuralChannelId::ASSOCIATION_CORTEX,
                    NeuralChannelId::MOTOR_EXPRESSION, 0.4f);

        // Thalamic relay (sensory gating)
        set_synapse(NeuralChannelId::VISUAL_PRIMARY,
                    NeuralChannelId::THALAMIC_RELAY_1, 0.5f);
        set_synapse(NeuralChannelId::SOMATOSENSORY,
                    NeuralChannelId::THALAMIC_RELAY_2, 0.5f);
        set_synapse(NeuralChannelId::THALAMIC_GATE,
                    NeuralChannelId::THALAMIC_RELAY_1, 0.3f);  // gating modulation
        set_synapse(NeuralChannelId::THALAMIC_GATE,
                    NeuralChannelId::THALAMIC_RELAY_2, 0.3f);

        // Memory pathways
        set_synapse(NeuralChannelId::HIPPOCAMPAL_ENCODE,
                    NeuralChannelId::MOTOR_MEMORY_STORE, 0.5f);
        set_synapse(NeuralChannelId::HIPPOCAMPAL_RECALL,
                    NeuralChannelId::WORKING_MEMORY, 0.4f);

        // Cerebellar prediction
        set_synapse(NeuralChannelId::CEREBELLAR_PREDICT,
                    NeuralChannelId::TEMPORAL_CORTEX, 0.4f);
        set_synapse(NeuralChannelId::CEREBELLAR_ERROR,
                    NeuralChannelId::ANTERIOR_CINGULATE, 0.5f);

        // Inhibitory connections (basal ganglia GO/NOGO antagonism)
        set_synapse(NeuralChannelId::BASAL_GANGLIA_NOGO,
                    NeuralChannelId::BASAL_GANGLIA_GO, -0.5f);

        // Social → Oxytocin pathway
        set_synapse(NeuralChannelId::SOCIAL_AFFERENT,
                    NeuralChannelId::ENDOCRINE_NUDGE, 0.3f);

        // Reward → action facilitation
        set_synapse(NeuralChannelId::REWARD_PREDICTION,
                    NeuralChannelId::BASAL_GANGLIA_GO, 0.4f);

        // Pain → sympathetic activation
        set_synapse(NeuralChannelId::PAIN_SIGNAL,
                    NeuralChannelId::SYMPATHETIC_OUT, 0.6f);

        // Default reflex arcs
        reflexes_.push_back({NeuralChannelId::THREAT_AFFERENT,
                             NeuralChannelId::SYMPATHETIC_OUT, 0.7f, 0.9f, 1});
        reflexes_.push_back({NeuralChannelId::PAIN_SIGNAL,
                             NeuralChannelId::MOTOR_GESTURE_EMIT, 0.6f, 0.8f, 1});
    }

    void setup_level_prototypes() {
        // SPINAL_REFLEX: high sensory + motor, minimal processing
        auto& reflex = level_prototypes_[0];
        reflex[NeuralChannelId::THREAT_AFFERENT] = 0.8f;
        reflex[NeuralChannelId::SYMPATHETIC_OUT] = 0.7f;
        reflex[NeuralChannelId::PAIN_SIGNAL] = 0.5f;

        // BRAINSTEM: autonomic regulation, moderate arousal
        auto& brainstem = level_prototypes_[1];
        brainstem[NeuralChannelId::BRAINSTEM_AUTONOMIC] = 0.7f;
        brainstem[NeuralChannelId::RETICULAR_ACTIVATION] = 0.6f;
        brainstem[NeuralChannelId::AROUSAL_SIGNAL] = 0.5f;
        brainstem[NeuralChannelId::HYPOTHALAMIC_BRIDGE] = 0.4f;

        // LIMBIC: emotional processing, memory
        auto& limbic = level_prototypes_[2];
        limbic[NeuralChannelId::AMYGDALA_VALENCE] = 0.6f;
        limbic[NeuralChannelId::AMYGDALA_AROUSAL] = 0.5f;
        limbic[NeuralChannelId::HIPPOCAMPAL_ENCODE] = 0.5f;
        limbic[NeuralChannelId::INSULA_INTEROCEPTION] = 0.4f;

        // CORTICAL: reasoning, executive function
        auto& cortical = level_prototypes_[3];
        cortical[NeuralChannelId::PREFRONTAL_EXEC] = 0.7f;
        cortical[NeuralChannelId::REASONING_CORTEX] = 0.6f;
        cortical[NeuralChannelId::WORKING_MEMORY] = 0.5f;
        cortical[NeuralChannelId::ASSOCIATION_CORTEX] = 0.5f;
        cortical[NeuralChannelId::ANTERIOR_CINGULATE] = 0.4f;
    }
};

} // namespace opencog::nerv
