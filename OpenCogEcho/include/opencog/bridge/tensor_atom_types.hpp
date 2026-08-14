#pragma once
/**
 * @file tensor_atom_types.hpp
 * @brief Shared types for the Neural Tensor ↔ OpenCog Atom conversion bridge
 *
 * Feature F1.1.3: Type Conversion Layer
 * Converts between neural tensors (Eigen) and symbolic atoms (OpenCog AtomSpace).
 */

#include <opencog/core/types.hpp>

#include <Eigen/Core>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace opencog::bridge {

// ============================================================================
// Conversion Errors
// ============================================================================

enum class ConversionError : uint8_t {
    None = 0,
    EmptyInput,           // Input tensor/atom set is empty
    DimensionMismatch,    // Tensor dimensions don't match expected layout
    InvalidAtomType,      // Atom type not convertible
    AtomNotFound,         // Referenced atom missing from AtomSpace
    NaNDetected,          // Tensor contains NaN values
    InfDetected,          // Tensor contains Inf values
    ThresholdFiltered,    // All values below threshold (not an error per se)
    InternalError         // Unexpected failure
};

[[nodiscard]] constexpr std::string_view error_name(ConversionError e) noexcept {
    switch (e) {
        case ConversionError::None:              return "None";
        case ConversionError::EmptyInput:        return "EmptyInput";
        case ConversionError::DimensionMismatch: return "DimensionMismatch";
        case ConversionError::InvalidAtomType:   return "InvalidAtomType";
        case ConversionError::AtomNotFound:      return "AtomNotFound";
        case ConversionError::NaNDetected:       return "NaNDetected";
        case ConversionError::InfDetected:       return "InfDetected";
        case ConversionError::ThresholdFiltered: return "ThresholdFiltered";
        case ConversionError::InternalError:     return "InternalError";
    }
    return "Unknown";
}

// ============================================================================
// Conversion Quality
// ============================================================================

enum class ConversionQuality : uint8_t {
    Lossless,         // Exact round-trip possible
    HighPrecision,    // < 1e-6 error
    NormalPrecision,  // < 1e-3 error
    LowPrecision,     // > 1e-3 error (e.g., thresholding discarded values)
    Approximate       // Structural approximation (topology preserved, values shifted)
};

// ============================================================================
// Tensor Descriptor
// ============================================================================

/// Describes the semantic meaning of a tensor for conversion routing.
enum class TensorSemantic : uint8_t {
    Activation,       // Neuron activation vector (1-D)
    WeightMatrix,     // Connection weight matrix (2-D)
    ReservoirState,   // Full ESN reservoir state vector (1-D)
    TemporalPattern,  // Time-series of activations (2-D: timesteps × features)
    AttentionMap,     // Attention/salience map (1-D)
    Generic           // Unspecified — convert element-wise
};

struct TensorDescriptor {
    TensorSemantic semantic{TensorSemantic::Generic};
    std::string label;            // Human-readable label (e.g., "stream1_state")
    int rows{0};
    int cols{0};

    [[nodiscard]] bool is_vector() const noexcept { return cols <= 1; }
    [[nodiscard]] bool is_matrix() const noexcept { return rows > 0 && cols > 1; }
    [[nodiscard]] int size() const noexcept { return is_vector() ? rows : rows * cols; }
};

// ============================================================================
// Conversion Policy
// ============================================================================

/// Controls how tensor values map to atom properties.
struct ConversionPolicy {
    // Thresholding — activations below this are skipped
    float activation_threshold{0.01f};

    // Normalization — if true, normalize tensor to [0,1] before conversion
    bool normalize{false};

    // Naming — prefix for generated atom names
    std::string name_prefix{"neuron"};

    // Truth value mapping
    //   strength = activation value (or normalized)
    //   confidence = fixed value or derived from activation magnitude
    float default_confidence{0.9f};
    bool derive_confidence_from_magnitude{false};

    // Attention value mapping — map activation to STI
    bool map_activation_to_sti{true};
    float sti_scale{1.0f};

    // Round-trip tolerance for validation
    float round_trip_epsilon{1e-5f};

    // Default policies
    [[nodiscard]] static ConversionPolicy strict() noexcept {
        ConversionPolicy p;
        p.activation_threshold = 0.0f;
        p.normalize = false;
        p.default_confidence = 0.99f;
        p.round_trip_epsilon = 1e-6f;
        return p;
    }

    [[nodiscard]] static ConversionPolicy relaxed() noexcept {
        ConversionPolicy p;
        p.activation_threshold = 0.1f;
        p.normalize = true;
        p.default_confidence = 0.8f;
        p.round_trip_epsilon = 1e-3f;
        return p;
    }
};

// ============================================================================
// Conversion Result
// ============================================================================

/// Result of a conversion operation, holding either a value or an error.
template<typename T>
struct ConversionResult {
    T value{};
    ConversionError error{ConversionError::None};
    ConversionQuality quality{ConversionQuality::Lossless};
    int elements_converted{0};
    int elements_skipped{0};

    [[nodiscard]] bool ok() const noexcept { return error == ConversionError::None; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] static ConversionResult success(T val, int converted, int skipped = 0,
                                                   ConversionQuality q = ConversionQuality::Lossless) {
        return {std::move(val), ConversionError::None, q, converted, skipped};
    }

    [[nodiscard]] static ConversionResult fail(ConversionError err) {
        return {T{}, err, ConversionQuality::Approximate, 0, 0};
    }
};

// ============================================================================
// Conversion Metrics
// ============================================================================

struct ConversionMetrics {
    uint64_t tensor_to_atom_count{0};
    uint64_t atom_to_tensor_count{0};
    uint64_t total_elements_converted{0};
    uint64_t total_elements_skipped{0};
    uint64_t error_count{0};
    double total_latency_us{0.0};

    [[nodiscard]] double average_latency_us() const noexcept {
        auto total = tensor_to_atom_count + atom_to_tensor_count;
        return total > 0 ? total_latency_us / static_cast<double>(total) : 0.0;
    }

    void reset() noexcept { *this = ConversionMetrics{}; }
};

} // namespace opencog::bridge
