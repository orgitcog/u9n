#pragma once
/**
 * @file guidance_connector.hpp
 * @brief Guidance Connector — External meta-cognitive oversight via Claude
 *
 * Monitors the endocrine system state and triggers guidance requests
 * when thresholds are crossed (sustained stress, moral novelty, coherence
 * drops, etc.). Applies received guidance directives as hormone nudges,
 * mode suggestions, and parameter overrides.
 *
 * Uses async futures so guidance requests never block the tick pipeline.
 * Supports pluggable backends (stub, HTTP, MMIO) via GuidanceBackend.
 */

#include <opencog/endocrine/connector.hpp>
#include <opencog/endocrine/guidance_types.hpp>
#include <opencog/endocrine/npu_adapter.hpp>
#include <opencog/endocrine/o9c2_adapter.hpp>
#include <opencog/endocrine/marduk_adapter.hpp>
#include <opencog/endocrine/touchpad_adapter.hpp>

#include <algorithm>
#include <cmath>
#include <future>
#include <memory>
#include <optional>

namespace opencog::endo {

/**
 * @brief Connects the endocrine system to an external guidance agent
 *
 * The guidance connector is the highest-level adapter in the system.
 * Unlike NPU and o9c2 adapters which run every tick, guidance operates
 * on a slower timescale: it monitors for trigger conditions and only
 * requests guidance when needed (or periodically).
 *
 * Trigger conditions:
 *   - Sustained cortisol above threshold (stress)
 *   - Moral novelty above threshold (novel moral situation)
 *   - COG_COHERENCE below floor (o9c2 losing coherence)
 *   - Mode stuck in STRESSED/THREAT for too long
 *   - Periodic strategic check-in
 *
 * Response application translates directives into hormone modulations
 * so that guidance influences the system through the same endocrine
 * pathway as all other signals — maintaining architectural consistency.
 */
class GuidanceConnector : public EndocrineConnector {
public:
    GuidanceConnector(HormoneBus& bus, std::unique_ptr<GuidanceBackend> backend,
                      GuidanceConfig config = {})
        : EndocrineConnector(bus)
        , backend_(std::move(backend))
        , config_(config)
    {}

    // === Optional cross-references for richer context ===

    void set_npu_source(NPUEndocrineAdapter* npu) noexcept { npu_adapter_ = npu; }
    void set_o9c2_source(O9C2EndocrineAdapter* o9c2) noexcept { o9c2_adapter_ = o9c2; }
    void set_marduk_source(MardukEndocrineAdapter* marduk) noexcept { marduk_adapter_ = marduk; }
    void set_touchpad_source(TouchpadEndocrineAdapter* tp) noexcept { touchpad_adapter_ = tp; }

    // === Moral perception source ===

    void set_moral_novelty(float novelty) noexcept { last_moral_novelty_ = novelty; }

    // === Main tick method ===

    /**
     * @brief Check triggers, send requests, apply pending responses
     *
     * Called once per tick from the EndocrineSystem pipeline, after
     * all adapters have done their read/write cycles.
     */
    void tick(const HormoneBus& bus) {
        ++tick_count_;
        ++ticks_since_last_request_;

        // First: check if a pending response has arrived
        apply_pending_response();

        // Second: check trigger conditions (only if cooldown has passed)
        if (ticks_since_last_request_ >= config_.cooldown_ticks && !has_pending_request()) {
            auto reason = check_triggers(bus);
            if (reason.has_value()) {
                send_request(bus, reason.value());
            }
        }
    }

    void apply_endocrine_modulation(const HormoneBus& /*bus*/) {
        // Guidance connector doesn't modulate parameters directly through
        // this method — it operates through tick() and applies responses
        // as hormone nudges. This satisfies the EndocrineModulated concept.
    }

    [[nodiscard]] CognitiveMode current_mode() const noexcept {
        return bus_.current_mode();
    }

    // === State queries ===

    [[nodiscard]] bool has_pending_request() const noexcept {
        return pending_response_.valid();
    }

    [[nodiscard]] uint64_t total_requests() const noexcept { return total_requests_; }
    [[nodiscard]] uint64_t total_responses() const noexcept { return total_responses_; }
    [[nodiscard]] uint64_t tick_count() const noexcept { return tick_count_; }

    [[nodiscard]] const GuidanceConfig& config() const noexcept { return config_; }
    void set_config(const GuidanceConfig& cfg) noexcept { config_ = cfg; }

    [[nodiscard]] GuidanceBackend& backend() noexcept { return *backend_; }

private:
    std::unique_ptr<GuidanceBackend> backend_;
    GuidanceConfig config_;

    // Optional cross-references (not owned)
    NPUEndocrineAdapter* npu_adapter_{nullptr};
    O9C2EndocrineAdapter* o9c2_adapter_{nullptr};
    MardukEndocrineAdapter* marduk_adapter_{nullptr};
    TouchpadEndocrineAdapter* touchpad_adapter_{nullptr};

    // Async response tracking
    std::future<GuidanceResponse> pending_response_;

    // State tracking
    uint64_t tick_count_{0};
    uint64_t ticks_since_last_request_{1000};  // Start high to allow immediate first request
    uint64_t total_requests_{0};
    uint64_t total_responses_{0};
    float last_moral_novelty_{0.0f};

    // Stress tracking (rolling window)
    size_t stressed_ticks_{0};

    // ================================================================
    // Trigger Detection
    // ================================================================

    /**
     * @brief Check all trigger conditions, return the highest-priority reason
     */
    [[nodiscard]] std::optional<GuidanceReason> check_triggers(const HormoneBus& bus) const {
        // Priority 1: Moral novelty
        if (last_moral_novelty_ > config_.moral_novelty_threshold) {
            return GuidanceReason::MORAL_NOVELTY;
        }

        // Priority 2: Sustained high stress
        float cortisol = bus.concentration(HormoneId::CORTISOL);
        if (cortisol > config_.stress_threshold) {
            CognitiveMode mode = bus.current_mode();
            if (mode == CognitiveMode::STRESSED || mode == CognitiveMode::THREAT) {
                // Check if we've been stressed for a while
                // (We use a simpler check: current cortisol above threshold)
                return GuidanceReason::HIGH_STRESS;
            }
        }

        // Priority 3: Coherence drop
        float coherence = bus.concentration(HormoneId::COG_COHERENCE);
        if (coherence < config_.coherence_floor && o9c2_adapter_) {
            return GuidanceReason::EMERGENCE_DROP;
        }

        // Priority 4: Marduk cognitive overload
        if (marduk_adapter_) {
            float marduk_load = bus.concentration(HormoneId::MARDUK_LOAD);
            if (marduk_load > config_.marduk_overload_threshold) {
                return GuidanceReason::MARDUK_OVERLOAD;
            }
        }

        // Priority 5: Hemispheric divergence (Left-Right coherence gap)
        if (marduk_adapter_ && o9c2_adapter_) {
            float cog_coh = bus.concentration(HormoneId::COG_COHERENCE);
            float org_coh = bus.concentration(HormoneId::ORG_COHERENCE);
            float divergence = std::abs(cog_coh - org_coh);
            if (divergence > config_.hemispheric_divergence_threshold) {
                return GuidanceReason::HEMISPHERIC_DIVERGENCE;
            }
        }

        // Priority 6: Touchpad overload
        if (touchpad_adapter_) {
            float tp_load = bus.concentration(HormoneId::TOUCHPAD_LOAD);
            if (tp_load > config_.touchpad_overload_threshold) {
                return GuidanceReason::TOUCHPAD_OVERLOAD;
            }
        }

        // Priority 7: NPU error recovery
        if (npu_adapter_) {
            NPUTelemetry tel = npu_adapter_->npu().telemetry();
            if (tel.has_error && bus.concentration(HormoneId::NPU_LOAD) > 0.8f) {
                return GuidanceReason::ERROR_RECOVERY;
            }
        }

        // Priority 8: Periodic strategic check-in
        if (ticks_since_last_request_ >= config_.periodic_interval) {
            return GuidanceReason::STRATEGIC;
        }

        return std::nullopt;
    }

    // ================================================================
    // Request Construction
    // ================================================================

    void send_request(const HormoneBus& bus, GuidanceReason reason) {
        if (!backend_ || !backend_->is_available()) return;

        GuidanceRequest req;
        req.urgency = compute_urgency(reason);
        req.reason = reason;
        req.endocrine_snapshot = bus.snapshot();
        req.current_mode = bus.current_mode();
        req.moral_novelty = last_moral_novelty_;
        req.stress_level = bus.concentration(HormoneId::CORTISOL);
        req.tick = tick_count_;

        // Enrich with o9c2 state if available
        if (o9c2_adapter_) {
            req.current_persona = o9c2_adapter_->o9c2().active_persona();
            req.emergence = o9c2_adapter_->o9c2().emergence();
        }

        // Enrich with NPU state if available
        if (npu_adapter_) {
            req.npu_telemetry = npu_adapter_->npu().telemetry();
        }

        // Enrich with Marduk state if available
        if (marduk_adapter_) {
            req.marduk_mode = marduk_adapter_->marduk().active_mode();
            req.marduk_telemetry = marduk_adapter_->marduk().telemetry();
            req.execution_metrics = marduk_adapter_->marduk().execution();
        }

        // Enrich with Touchpad state if available
        if (touchpad_adapter_) {
            req.touchpad_mode = touchpad_adapter_->touchpad().active_mode();
            req.touchpad_telemetry = touchpad_adapter_->touchpad().telemetry();
            req.gesture_metrics = touchpad_adapter_->touchpad().gesture_metrics();
        }

        // Build context description
        req.context_description = build_context(reason, bus);

        // Send async
        pending_response_ = backend_->request(req);
        ticks_since_last_request_ = 0;
        ++total_requests_;
    }

    [[nodiscard]] static GuidanceUrgency compute_urgency(GuidanceReason reason) noexcept {
        switch (reason) {
        case GuidanceReason::MORAL_NOVELTY:           return GuidanceUrgency::HIGH;
        case GuidanceReason::HIGH_STRESS:             return GuidanceUrgency::HIGH;
        case GuidanceReason::ERROR_RECOVERY:          return GuidanceUrgency::HIGH;
        case GuidanceReason::MARDUK_OVERLOAD:         return GuidanceUrgency::HIGH;
        case GuidanceReason::TOUCHPAD_OVERLOAD:       return GuidanceUrgency::MEDIUM;
        case GuidanceReason::EMERGENCE_DROP:           return GuidanceUrgency::MEDIUM;
        case GuidanceReason::MODE_CONFLICT:            return GuidanceUrgency::MEDIUM;
        case GuidanceReason::HEMISPHERIC_DIVERGENCE:   return GuidanceUrgency::MEDIUM;
        case GuidanceReason::PERSONA_QUESTION:         return GuidanceUrgency::LOW;
        case GuidanceReason::STRATEGIC:                return GuidanceUrgency::LOW;
        case GuidanceReason::UNCERTAINTY:              return GuidanceUrgency::MEDIUM;
        default:                                       return GuidanceUrgency::LOW;
        }
    }

    [[nodiscard]] static std::string build_context(GuidanceReason reason,
                                                    const HormoneBus& bus) {
        std::string ctx = "Reason: ";
        ctx += reason_name(reason);
        ctx += " | Mode: ";
        ctx += mode_name(bus.current_mode());
        ctx += " | Cortisol: ";
        ctx += std::to_string(bus.concentration(HormoneId::CORTISOL));
        return ctx;
    }

    // ================================================================
    // Response Application
    // ================================================================

    void apply_pending_response() {
        if (!pending_response_.valid()) return;

        // Check if ready without blocking
        auto status = pending_response_.wait_for(std::chrono::milliseconds(0));
        if (status != std::future_status::ready) return;

        GuidanceResponse resp = pending_response_.get();
        if (!resp.valid) return;

        ++total_responses_;
        apply_response(resp);
    }

    void apply_response(const GuidanceResponse& resp) {
        // Apply directive-specific actions
        switch (resp.directive) {
        case GuidanceDirective::SUGGEST_MODE:
            nudge_toward_mode(resp.suggested_mode);
            break;

        case GuidanceDirective::SUGGEST_PERSONA:
            if (o9c2_adapter_) {
                o9c2_adapter_->suggest_transition(resp.suggested_persona);
            }
            break;

        case GuidanceDirective::ADJUST_PARAMETERS:
            apply_parameter_overrides(resp);
            break;

        case GuidanceDirective::REDUCE_STRESS:
            bus_.produce(HormoneId::SEROTONIN, 0.15f);
            bus_.produce(HormoneId::ANANDAMIDE, 0.1f);
            break;

        case GuidanceDirective::INCREASE_CAUTION:
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.1f);
            bus_.produce(HormoneId::CORTISOL, 0.05f);
            break;

        case GuidanceDirective::ENCOURAGE_EXPLORE:
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.15f);
            break;

        case GuidanceDirective::STRATEGIC_PAUSE:
            bus_.produce(HormoneId::MELATONIN, 0.2f);
            bus_.produce(HormoneId::SEROTONIN, 0.1f);
            break;

        case GuidanceDirective::SUGGEST_MARDUK_MODE:
            if (marduk_adapter_) {
                marduk_adapter_->suggest_transition(resp.suggested_marduk_mode);
            }
            break;

        case GuidanceDirective::MARDUK_STRATEGIC_PAUSE:
            // Slow Marduk processing: boost consolidation hormones, reduce urgency
            bus_.produce(HormoneId::MELATONIN, 0.15f);
            bus_.produce(HormoneId::SEROTONIN, 0.1f);
            bus_.produce(HormoneId::ANANDAMIDE, 0.1f);
            break;

        case GuidanceDirective::SUGGEST_TOUCHPAD_MODE:
            if (touchpad_adapter_) {
                touchpad_adapter_->suggest_transition(resp.suggested_touchpad_mode);
            }
            break;

        case GuidanceDirective::TOUCHPAD_CALIBRATE:
            if (touchpad_adapter_) {
                touchpad_adapter_->suggest_transition(TouchpadMode::CALIBRATING);
            }
            break;

        case GuidanceDirective::NO_ACTION:
        default:
            break;
        }

        // Apply direct hormone nudges (always, regardless of directive)
        for (size_t i = 0; i < HORMONE_COUNT; ++i) {
            float nudge = resp.hormone_nudges[i];
            if (nudge > 0.001f) {
                bus_.produce(static_cast<HormoneId>(i), nudge);
            }
            // Note: negative nudges would require a suppress/consume API
            // which the bus doesn't have — we only produce positive amounts
        }
    }

    /**
     * @brief Produce hormones that drive the bus toward a target mode's
     *        prototype constellation
     */
    void nudge_toward_mode(CognitiveMode target) {
        // Small, targeted hormone productions to shift toward the target mode
        switch (target) {
        case CognitiveMode::REFLECTIVE:
            bus_.produce(HormoneId::SEROTONIN, 0.15f);
            break;
        case CognitiveMode::EXPLORATORY:
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.1f);
            bus_.produce(HormoneId::SEROTONIN, 0.05f);
            break;
        case CognitiveMode::FOCUSED:
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.15f);
            bus_.produce(HormoneId::CORTISOL, 0.05f);
            break;
        case CognitiveMode::SOCIAL:
            bus_.produce(HormoneId::OXYTOCIN, 0.2f);
            bus_.produce(HormoneId::SEROTONIN, 0.1f);
            break;
        case CognitiveMode::RESTING:
            // Produce nothing — let decay bring system to rest
            break;
        case CognitiveMode::VIGILANT:
            bus_.produce(HormoneId::NOREPINEPHRINE, 0.2f);
            break;
        case CognitiveMode::MAINTENANCE:
            bus_.produce(HormoneId::MELATONIN, 0.2f);
            break;
        case CognitiveMode::REWARD:
            bus_.produce(HormoneId::DOPAMINE_PHASIC, 0.2f);
            bus_.produce(HormoneId::DOPAMINE_TONIC, 0.1f);
            break;
        default:
            break;
        }
    }

    void apply_parameter_overrides(const GuidanceResponse& resp) {
        // These are direct parameter overrides — bypass the normal
        // hormone → parameter mapping by adjusting the adapters directly

        if (o9c2_adapter_) {
            O9C2PersonaConfig cfg = o9c2_adapter_->o9c2().current_config();
            bool changed = false;

            if (GuidanceResponse::has_override(resp.override_spectral_radius)) {
                cfg.spectral_radius = std::clamp(resp.override_spectral_radius, 0.5f, 0.99f);
                changed = true;
            }
            if (GuidanceResponse::has_override(resp.override_input_scaling)) {
                cfg.input_scaling = std::clamp(resp.override_input_scaling, 0.1f, 0.9f);
                changed = true;
            }
            if (GuidanceResponse::has_override(resp.override_leak_rate)) {
                cfg.leak_rate = std::clamp(resp.override_leak_rate, 0.1f, 0.9f);
                changed = true;
            }

            if (changed) {
                o9c2_adapter_->o9c2().apply_config(cfg);
            }
        }

        if (npu_adapter_ && GuidanceResponse::has_override(resp.override_creativity)) {
            NPUEndocrineConfig cfg = npu_adapter_->npu().current_config();
            cfg.creativity = std::clamp(resp.override_creativity, 0.1f, 1.0f);
            npu_adapter_->npu().apply_config(cfg);
        }

        // Apply Marduk parameter overrides
        if (marduk_adapter_) {
            MardukEndocrineConfig mcfg = marduk_adapter_->marduk().current_config();
            bool mchanged = false;

            if (GuidanceResponse::has_override(resp.override_task_urgency)) {
                mcfg.task_urgency = std::clamp(resp.override_task_urgency, 0.0f, 1.0f);
                mchanged = true;
            }
            if (GuidanceResponse::has_override(resp.override_validation_threshold)) {
                mcfg.validation_threshold = std::clamp(resp.override_validation_threshold, 0.0f, 1.0f);
                mchanged = true;
            }

            if (mchanged) {
                marduk_adapter_->marduk().apply_config(mcfg);
            }
        }

        // Apply Touchpad parameter overrides
        if (touchpad_adapter_) {
            TouchpadEndocrineConfig tcfg = touchpad_adapter_->touchpad().current_config();
            bool tchanged = false;

            if (GuidanceResponse::has_override(resp.override_sensitivity)) {
                tcfg.sensitivity = std::clamp(resp.override_sensitivity, 0.0f, 1.0f);
                tchanged = true;
            }
            if (GuidanceResponse::has_override(resp.override_coherence_requirement)) {
                tcfg.coherence_requirement = std::clamp(resp.override_coherence_requirement, 0.0f, 1.0f);
                tchanged = true;
            }

            if (tchanged) {
                touchpad_adapter_->touchpad().apply_config(tcfg);
            }
        }
    }
};

} // namespace opencog::endo
