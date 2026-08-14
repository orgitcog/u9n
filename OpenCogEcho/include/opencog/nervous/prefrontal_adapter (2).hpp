#pragma once
/**
 * @file prefrontal_adapter.hpp
 * @brief PrefrontalMardukAdapter — Bidirectional coupling to Marduk executive
 *
 * READ: PREFRONTAL_EXEC → task_urgency, validation_threshold
 * WRITE: Marduk tasks → BASAL_GANGLIA_GO, overload → FATIGUE_SIGNAL
 *
 * Uses the abstract MardukInterface from the endocrine system for decoupling.
 */

#include <opencog/nervous/connector.hpp>
#include <opencog/endocrine/marduk_types.hpp>

namespace opencog::nerv {

class PrefrontalMardukAdapter : public NeuralConnector {
public:
    PrefrontalMardukAdapter(NerveBus& bus, endo::MardukInterface& marduk)
        : NeuralConnector(bus)
        , marduk_(marduk)
    {
        base_config_ = marduk_.current_config();
        prev_exec_ = 0.0f;
    }

    void read_signals(const NerveBus& bus) override {
        endo::MardukEndocrineConfig cfg = base_config_;

        float exec = bus.activation(NeuralChannelId::PREFRONTAL_EXEC);
        float task_dispatch = bus.activation(NeuralChannelId::MOTOR_TASK_DISPATCH);
        float go = bus.activation(NeuralChannelId::BASAL_GANGLIA_GO);
        float nogo = bus.activation(NeuralChannelId::BASAL_GANGLIA_NOGO);
        float conflict = bus.activation(NeuralChannelId::ANTERIOR_CINGULATE);

        // Executive signal → task urgency (strong exec = prioritize tasks)
        float exec_delta = rising_edge(exec, prev_exec_, 0.03f);
        cfg.task_urgency = std::clamp(
            base_config_.task_urgency + std::abs(exec) * 0.3f + exec_delta * 0.2f,
            0.0f, 1.0f);

        // GO/NOGO balance → task exploration
        float action_bias = go - std::abs(nogo);
        cfg.task_exploration = std::clamp(
            base_config_.task_exploration + action_bias * 0.2f,
            0.0f, 1.0f);

        // Conflict → validation threshold (high conflict = be more careful)
        cfg.validation_threshold = std::clamp(
            base_config_.validation_threshold + std::abs(conflict) * 0.25f,
            0.0f, 1.0f);

        // Task dispatch motor signal → autonomy rate
        if (std::abs(task_dispatch) > 0.1f) {
            cfg.autonomy_rate = std::clamp(
                base_config_.autonomy_rate + std::abs(task_dispatch) * 0.2f,
                0.0f, 1.0f);
        }

        // Maintain current mode tag
        cfg.active_mode = marduk_.active_mode();

        marduk_.apply_config(cfg);

        // Check for mode transition based on processing level
        update_mode_transition();

        prev_exec_ = exec;
    }

    void write_feedback() override {
        auto tel = marduk_.telemetry();
        auto ex = marduk_.execution();

        // Task completions → GO signal reinforcement
        if (tel.tasks_completed > prev_tasks_completed_) {
            bus_.fire(NeuralChannelId::BASAL_GANGLIA_GO, 0.3f, 0.5f);
            bus_.fire(NeuralChannelId::REWARD_PREDICTION, 0.2f, 0.3f);
        }

        // High queue depth → fatigue + conflict
        if (tel.task_queue_depth > 0.7f) {
            bus_.fire(NeuralChannelId::FATIGUE_SIGNAL,
                      (tel.task_queue_depth - 0.7f) * 1.5f, 0.3f);
        }

        // Organization metric → coherence signal
        if (std::abs(ex.organization - prev_organization_) > 0.03f) {
            bus_.fire(NeuralChannelId::COHERENCE_SIGNAL,
                      ex.organization * 0.3f, 0.2f);
        }

        // Optimization complete → reward prediction
        if (tel.optimization_cycles > prev_optimization_) {
            bus_.fire(NeuralChannelId::REWARD_PREDICTION, 0.3f, 0.4f);
        }

        prev_tasks_completed_ = tel.tasks_completed;
        prev_organization_ = ex.organization;
        prev_optimization_ = tel.optimization_cycles;
    }

    /// Set hysteresis ticks for mode transition
    void set_hysteresis_ticks(size_t n) noexcept { hysteresis_ticks_ = n; }

private:
    endo::MardukInterface& marduk_;
    endo::MardukEndocrineConfig base_config_;
    float prev_exec_{0.0f};
    uint64_t prev_tasks_completed_{0};
    float prev_organization_{0.0f};
    uint64_t prev_optimization_{0};

    // Mode transition hysteresis
    ProcessingLevel current_level_{ProcessingLevel::CORTICAL};
    size_t level_persist_count_{0};
    size_t hysteresis_ticks_{5};

    void update_mode_transition() {
        ProcessingLevel level = bus_.current_level();

        if (level == current_level_) {
            ++level_persist_count_;
        } else {
            current_level_ = level;
            level_persist_count_ = 1;
        }

        if (level_persist_count_ < hysteresis_ticks_) return;

        auto current = marduk_.active_mode();
        auto suggested = level_to_mode(level);

        if (suggested != current) {
            marduk_.transition_mode(suggested);
            base_config_ = endo::MARDUK_MODE_PRESETS[static_cast<size_t>(suggested)];
            level_persist_count_ = 0;
        }
    }

    [[nodiscard]] static endo::MardukOperationalMode level_to_mode(
            ProcessingLevel level) noexcept {
        switch (level) {
        case ProcessingLevel::SPINAL_REFLEX:
        case ProcessingLevel::BRAINSTEM:
            return endo::MardukOperationalMode::VERIFICATION_GUARDIAN;
        case ProcessingLevel::LIMBIC:
            return endo::MardukOperationalMode::KNOWLEDGE_ARCHITECT;
        case ProcessingLevel::CORTICAL:
            return endo::MardukOperationalMode::TASK_EXECUTOR;
        default:
            return endo::MardukOperationalMode::TASK_EXECUTOR;
        }
    }
};

} // namespace opencog::nerv
