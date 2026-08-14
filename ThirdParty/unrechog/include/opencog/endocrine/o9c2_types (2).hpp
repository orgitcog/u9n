#pragma once
/**
 * @file o9c2_types.hpp
 * @brief Abstract o9c2 Deep Tree Echo interface and types for endocrine integration
 *
 * Defines persona configurations, emergence metrics, and the abstract
 * O9C2Interface that allows the endocrine system to modulate the Deep
 * Tree Echo cognitive architecture's hyperparameters and receive
 * emergence metric feedback.
 *
 * The four persona presets correspond to coherent points in the
 * hyperparameter space of the six computational paradigms (ESN,
 * P-Systems, B-Series, J-Surface, Emotion Theory, Transformer Attention).
 */

#include <cmath>
#include <cstdint>
#include <string_view>

namespace opencog::endo {

// ============================================================================
// o9c2 Persona Identifiers
// ============================================================================

/// Named persona configurations — coherent cognitive styles
enum class O9C2Persona : uint8_t {
    CONTEMPLATIVE_SCHOLAR = 0,  ///< Deep reflective, high memory, slow dynamics
    DYNAMIC_EXPLORER      = 1,  ///< Fast adaptive, breadth over depth
    CAUTIOUS_ANALYST      = 2,  ///< Conservative, maximal stability
    CREATIVE_VISIONARY    = 3,  ///< Edge-of-chaos, divergent thinking
};

inline constexpr size_t O9C2_PERSONA_COUNT = 4;

[[nodiscard]] constexpr std::string_view persona_name(O9C2Persona p) noexcept {
    constexpr std::string_view names[] = {
        "Contemplative Scholar", "Dynamic Explorer",
        "Cautious Analyst", "Creative Visionary"
    };
    auto idx = static_cast<size_t>(p);
    return idx < O9C2_PERSONA_COUNT ? names[idx] : "Unknown";
}

// ============================================================================
// Persona Hyperparameters
// ============================================================================

/**
 * @brief Core hyperparameters for the o9c2 cognitive architecture
 *
 * These control the dynamics across all six computational paradigms:
 * - spectral_radius: ESN memory depth and echo stability
 * - input_scaling: Sensitivity to incoming signals
 * - leak_rate: Speed of state evolution (fast vs slow dynamics)
 * - membrane_permeability: P-System boundary openness
 */
struct O9C2PersonaConfig {
    float spectral_radius{0.85f};       ///< [0.5, 0.99]: memory depth
    float input_scaling{0.5f};          ///< [0.1, 0.9]: input sensitivity
    float leak_rate{0.5f};              ///< [0.1, 0.9]: dynamics speed
    float membrane_permeability{0.5f};  ///< [0.1, 0.9]: boundary openness
    O9C2Persona active_persona{O9C2Persona::CONTEMPLATIVE_SCHOLAR};
};

/// Named preset configurations matching the o9c2 specification
inline constexpr O9C2PersonaConfig PERSONA_PRESETS[O9C2_PERSONA_COUNT] = {
    // Contemplative Scholar: deep, slow, reflective
    {0.95f, 0.3f, 0.2f, 0.3f, O9C2Persona::CONTEMPLATIVE_SCHOLAR},
    // Dynamic Explorer: fast, broad, novelty-seeking
    {0.70f, 0.8f, 0.8f, 0.7f, O9C2Persona::DYNAMIC_EXPLORER},
    // Cautious Analyst: maximal stability, conservative
    {0.99f, 0.2f, 0.3f, 0.2f, O9C2Persona::CAUTIOUS_ANALYST},
    // Creative Visionary: edge-of-chaos, generative
    {0.85f, 0.7f, 0.6f, 0.6f, O9C2Persona::CREATIVE_VISIONARY},
};

// ============================================================================
// Emergence Metrics
// ============================================================================

/**
 * @brief Quantifiable emergence metrics from the o9c2 architecture
 *
 * These measure the quality of the cognitive process:
 * - wisdom: balanced optimization across all dimensions
 * - complexity: richness of internal state diversity
 * - coherence: integration and mutual information across subsystems
 * - stability: robustness of patterns and noise resistance
 * - adaptability: responsiveness to novel input and contexts
 */
struct EmergenceMetrics {
    float wisdom{0.0f};        ///< [0, 1]: balanced optimization
    float complexity{0.0f};    ///< [0, 1]: state diversity
    float coherence{0.0f};     ///< [0, 1]: subsystem integration
    float stability{0.0f};     ///< [0, 1]: noise resistance
    float adaptability{0.0f};  ///< [0, 1]: novelty responsiveness

    /// Aggregate emergence score (geometric mean prevents one metric dominating)
    [[nodiscard]] float aggregate() const noexcept {
        // Clamp all to positive to avoid NaN from pow
        float w = std::max(0.001f, wisdom);
        float cx = std::max(0.001f, complexity);
        float co = std::max(0.001f, coherence);
        float s = std::max(0.001f, stability);
        float a = std::max(0.001f, adaptability);
        return std::pow(w * cx * co * s * a, 0.2f);
    }
};

// ============================================================================
// o9c2 Interface — Abstract bridge to the Deep Tree Echo architecture
// ============================================================================

/**
 * @brief Abstract o9c2 interface for endocrine coupling
 *
 * The endocrine adapter reads emergence metrics for feedback to the
 * hormone bus and writes hormonally-modulated hyperparameters to
 * control the cognitive dynamics.
 */
struct O9C2Interface {
    virtual ~O9C2Interface() = default;

    // === Read (emergence metrics for feedback) ===

    /// Current active persona
    [[nodiscard]] virtual O9C2Persona active_persona() const noexcept = 0;

    /// Current hyperparameters (may be interpolated, not a pure preset)
    [[nodiscard]] virtual O9C2PersonaConfig current_config() const noexcept = 0;

    /// Current emergence metrics
    [[nodiscard]] virtual EmergenceMetrics emergence() const noexcept = 0;

    // === Write (endocrine modulation) ===

    /// Apply hormonally-modulated hyperparameters
    virtual void apply_config(const O9C2PersonaConfig& cfg) = 0;

    /// Request a persona transition (may be gradual, not instant)
    virtual void transition_persona(O9C2Persona target) = 0;
};

} // namespace opencog::endo
