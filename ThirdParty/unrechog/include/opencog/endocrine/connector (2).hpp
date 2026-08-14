#pragma once
/**
 * @file connector.hpp
 * @brief Endocrine connector interface and subsystem adapters
 *
 * Provides the EndocrineModulated concept and concrete adapters that
 * translate hormone bus state into parameter modulations for ECAN,
 * PLN, and other cognitive subsystems.
 */

#include <opencog/endocrine/hormone_bus.hpp>
#include <opencog/attention/attention_bank.hpp>
#include <opencog/pln/inference.hpp>

#include <concepts>
#include <functional>
#include <vector>

namespace opencog::endo {

// ============================================================================
// EndocrineModulated Concept
// ============================================================================

template<typename T>
concept EndocrineModulated = requires(T t, const HormoneBus& bus) {
    { t.apply_endocrine_modulation(bus) } -> std::same_as<void>;
    { t.current_mode() } -> std::convertible_to<CognitiveMode>;
};

// ============================================================================
// EndocrineConnector — Base for any modulated subsystem
// ============================================================================

class EndocrineConnector {
public:
    explicit EndocrineConnector(HormoneBus& bus) : bus_(bus) {}
    virtual ~EndocrineConnector() = default;

    /// Read current bus state
    [[nodiscard]] EndocrineState read_bus() const noexcept {
        return bus_.snapshot();
    }

    /// Current cognitive mode
    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    /// Get a specific hormone level
    [[nodiscard]] float hormone(HormoneId id) const noexcept {
        return bus_.concentration(id);
    }

    /// Register a modulation callback (called externally per tick)
    void register_modulation(std::function<void(const EndocrineState&)> cb) {
        modulation_cbs_.push_back(std::move(cb));
    }

    /// Signal an event to the bus
    void signal_event(HormoneId target, float amount) {
        bus_.produce(target, amount);
    }

    /// Apply all registered modulation callbacks
    void apply_modulations() {
        auto state = bus_.snapshot();
        for (auto& cb : modulation_cbs_) {
            cb(state);
        }
    }

protected:
    HormoneBus& bus_;
    std::vector<std::function<void(const EndocrineState&)>> modulation_cbs_;
};

// ============================================================================
// ECANEndocrineAdapter — Modulates attention parameters from hormones
// ============================================================================

/**
 * @brief Maps hormone concentrations to ECAN configuration parameters
 *
 * Mappings:
 *  - Norepinephrine → af_boundary (arousal widens/narrows focus)
 *  - Cortisol → spreading_rate (stress reduces diffuse spreading)
 *  - Dopamine(tonic) → stimulus_wage (reward state increases stimulus value)
 *  - Melatonin → forgetting_threshold (maintenance mode relaxes forgetting)
 *  - Serotonin → lti_decay_rate (patience preserves long-term importance)
 */
class ECANEndocrineAdapter : public EndocrineConnector {
public:
    ECANEndocrineAdapter(HormoneBus& bus, AttentionBank& bank)
        : EndocrineConnector(bus), bank_(bank)
    {
        // Store original config as baseline
        base_config_ = bank_.config();
    }

    /// Apply hormonal modulation to ECAN parameters
    void apply_endocrine_modulation(const HormoneBus& bus) {
        ECANConfig cfg = base_config_;

        float ne = bus.concentration(HormoneId::NOREPINEPHRINE);
        float cortisol = bus.concentration(HormoneId::CORTISOL);
        float da_tonic = bus.concentration(HormoneId::DOPAMINE_TONIC);
        float melatonin = bus.concentration(HormoneId::MELATONIN);
        float serotonin = bus.concentration(HormoneId::SEROTONIN);

        // High norepinephrine → lower AF boundary (more atoms in focus = broader attention)
        cfg.af_boundary = base_config_.af_boundary - ne * 50.0f;

        // High cortisol → reduced spreading (focused, not diffuse)
        cfg.spreading_rate = base_config_.spreading_rate * std::max(0.1f, 1.0f - cortisol * 0.6f);

        // High tonic dopamine → higher stimulus wages (reward amplification)
        cfg.stimulus_wage = base_config_.stimulus_wage * (1.0f + da_tonic * 0.5f);

        // High melatonin → relaxed forgetting (maintenance mode preserves more)
        cfg.forgetting_threshold = base_config_.forgetting_threshold * (1.0f - melatonin * 0.5f);

        // High serotonin → slower LTI decay (patience preserves long-term importance)
        cfg.lti_decay_rate = base_config_.lti_decay_rate * std::max(0.1f, 1.0f - serotonin * 0.5f);

        bank_.set_config(cfg);
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

private:
    AttentionBank& bank_;
    ECANConfig base_config_;
};

// ============================================================================
// PLNEndocrineAdapter — Modulates inference parameters from hormones
// ============================================================================

/**
 * @brief Maps hormone concentrations to PLN inference configuration
 *
 * Mappings:
 *  - Serotonin → max_iterations (patience allows deeper inference)
 *  - Cortisol → min_confidence (stress demands higher certainty)
 *  - T3/T4 → inference speed multiplier
 *  - Dopamine(tonic) → exploration tendency in rule selection
 */
class PLNEndocrineAdapter : public EndocrineConnector {
public:
    PLNEndocrineAdapter(HormoneBus& bus, pln::PLNEngine& engine)
        : EndocrineConnector(bus), engine_(engine)
    {
        base_config_ = engine_.config();
    }

    void apply_endocrine_modulation(const HormoneBus& bus) {
        pln::InferenceConfig cfg = base_config_;

        float serotonin = bus.concentration(HormoneId::SEROTONIN);
        float cortisol = bus.concentration(HormoneId::CORTISOL);
        float thyroid = bus.concentration(HormoneId::T3_T4);

        // Serotonin → more patient inference (more iterations)
        cfg.max_iterations = static_cast<size_t>(
            static_cast<float>(base_config_.max_iterations) * (0.5f + serotonin));

        // Cortisol → demand higher confidence (conservative under stress)
        cfg.min_confidence = base_config_.min_confidence + cortisol * 0.4f;

        // Thyroid → attention threshold (processing rate governor)
        cfg.attention_threshold = base_config_.attention_threshold * (1.5f - thyroid);

        engine_.set_config(cfg);
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

private:
    pln::PLNEngine& engine_;
    pln::InferenceConfig base_config_;
};

} // namespace opencog::endo
