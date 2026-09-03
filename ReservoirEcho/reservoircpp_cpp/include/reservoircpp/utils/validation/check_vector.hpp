#ifndef RESERVOIRCPP_UTILS_VALIDATION_CHECK_VECTOR_HPP
#define RESERVOIRCPP_UTILS_VALIDATION_CHECK_VECTOR_HPP

#include <vector>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>

namespace reservoircpp {
namespace utils {
namespace validation {

/**
 * @brief Vector validation utilities
 * 
 * Provides validation functions for input vectors in reservoir computing.
 */

// Check if vector is empty
template<typename T>
void check_not_empty(const std::vector<T>& vec, const std::string& name = "vector") {
    if (vec.empty()) {
        throw std::invalid_argument(name + " must not be empty");
    }
}

// Check vector size
template<typename T>
void check_size(const std::vector<T>& vec, std::size_t expected, 
                const std::string& name = "vector") {
    if (vec.size() != expected) {
        throw std::invalid_argument(name + " size mismatch: expected " + 
                                    std::to_string(expected) + ", got " + 
                                    std::to_string(vec.size()));
    }
}

// Check minimum size
template<typename T>
void check_min_size(const std::vector<T>& vec, std::size_t min_size,
                    const std::string& name = "vector") {
    if (vec.size() < min_size) {
        throw std::invalid_argument(name + " size too small: minimum " + 
                                    std::to_string(min_size) + ", got " + 
                                    std::to_string(vec.size()));
    }
}

// Check for NaN values (floating point)
template<typename T>
void check_no_nan(const std::vector<T>& vec, const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (std::isnan(vec[i])) {
            throw std::invalid_argument(name + " contains NaN at index " + 
                                        std::to_string(i));
        }
    }
}

// Check for infinite values (floating point)
template<typename T>
void check_no_inf(const std::vector<T>& vec, const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (std::isinf(vec[i])) {
            throw std::invalid_argument(name + " contains Inf at index " + 
                                        std::to_string(i));
        }
    }
}

// Check for finite values (no NaN or Inf)
template<typename T>
void check_finite(const std::vector<T>& vec, const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (!std::isfinite(vec[i])) {
            throw std::invalid_argument(name + " contains non-finite value at index " + 
                                        std::to_string(i));
        }
    }
}

// Check value range
template<typename T>
void check_range(const std::vector<T>& vec, T min_val, T max_val,
                 const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] < min_val || vec[i] > max_val) {
            throw std::out_of_range(name + " value out of range at index " + 
                                    std::to_string(i) + ": " + std::to_string(vec[i]) +
                                    " not in [" + std::to_string(min_val) + ", " + 
                                    std::to_string(max_val) + "]");
        }
    }
}

// Check positive values
template<typename T>
void check_positive(const std::vector<T>& vec, const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] <= T(0)) {
            throw std::invalid_argument(name + " must contain positive values, "
                                        "found " + std::to_string(vec[i]) + 
                                        " at index " + std::to_string(i));
        }
    }
}

// Check non-negative values
template<typename T>
void check_non_negative(const std::vector<T>& vec, const std::string& name = "vector") {
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] < T(0)) {
            throw std::invalid_argument(name + " must contain non-negative values, "
                                        "found " + std::to_string(vec[i]) + 
                                        " at index " + std::to_string(i));
        }
    }
}

// Check if vector is normalized (sum to 1)
template<typename T>
void check_normalized(const std::vector<T>& vec, T tolerance = T(1e-6),
                      const std::string& name = "vector") {
    T sum = T(0);
    for (const auto& v : vec) {
        sum += v;
    }
    if (std::abs(sum - T(1)) > tolerance) {
        throw std::invalid_argument(name + " must be normalized (sum to 1), "
                                    "sum is " + std::to_string(sum));
    }
}

// Comprehensive validation
template<typename T>
void validate_input(const std::vector<T>& vec, const std::string& name = "input") {
    check_not_empty(vec, name);
    check_finite(vec, name);
}

} // namespace validation
} // namespace utils
} // namespace reservoircpp

#endif // RESERVOIRCPP_UTILS_VALIDATION_CHECK_VECTOR_HPP
