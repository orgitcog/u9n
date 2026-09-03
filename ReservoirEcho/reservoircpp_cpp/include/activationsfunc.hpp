#ifndef RESERVOIRCPP_ACTIVATIONSFUNC_HPP
#define RESERVOIRCPP_ACTIVATIONSFUNC_HPP

/**
 * @file activationsfunc.hpp
 * @brief Activation functions for reservoir computing
 * 
 * Provides common activation functions for reservoir, feedback and output layers.
 * 
 * Available functions:
 *   - identity: f(x) = x
 *   - sigmoid: f(x) = 1 / (1 + exp(-x))
 *   - tanh: f(x) = tanh(x)
 *   - relu: f(x) = max(0, x)
 *   - softmax: f(x_k) = exp(x_k) / sum(exp(x_i))
 *   - softplus: f(x) = ln(1 + exp(x))
 * 
 * Author: Nathan Trouvain at 01/06/2021 <nathan.trouvain@inria.fr>
 * Licence: MIT License
 * Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>
 */

#include <Eigen/Dense>
#include <cmath>
#include <string>
#include <functional>
#include <unordered_map>
#include <stdexcept>

namespace reservoircpp {
namespace activations {

using Array = Eigen::ArrayXd;
using Matrix = Eigen::ArrayXXd;

/**
 * @brief Identity activation function
 * 
 * f(x) = x
 */
inline Array identity(const Array& x) {
    return x;
}

inline Matrix identity(const Matrix& x) {
    return x;
}

/**
 * @brief Sigmoid activation function
 * 
 * f(x) = 1 / (1 + exp(-x))
 */
inline Array sigmoid(const Array& x) {
    return 1.0 / (1.0 + (-x).exp());
}

inline Matrix sigmoid(const Matrix& x) {
    return 1.0 / (1.0 + (-x).exp());
}

/**
 * @brief Hyperbolic tangent activation function
 * 
 * f(x) = tanh(x)
 */
inline Array tanh(const Array& x) {
    return x.tanh();
}

inline Matrix tanh(const Matrix& x) {
    return x.tanh();
}

/**
 * @brief ReLU (Rectified Linear Unit) activation function
 * 
 * f(x) = max(0, x)
 */
inline Array relu(const Array& x) {
    return x.max(0.0);
}

inline Matrix relu(const Matrix& x) {
    return x.max(0.0);
}

/**
 * @brief Leaky ReLU activation function
 * 
 * f(x) = x if x > 0 else alpha * x
 */
inline Array leaky_relu(const Array& x, double alpha = 0.01) {
    return (x > 0.0).select(x, alpha * x);
}

/**
 * @brief Softplus activation function
 * 
 * f(x) = ln(1 + exp(x))
 * 
 * Smooth approximation of ReLU.
 */
inline Array softplus(const Array& x) {
    return (1.0 + x.exp()).log();
}

inline Matrix softplus(const Matrix& x) {
    return (1.0 + x.exp()).log();
}

/**
 * @brief Softmax activation function
 * 
 * f(x_k) = exp(beta * x_k) / sum(exp(beta * x_i))
 * 
 * @param x Input array
 * @param beta Temperature parameter (default 1.0)
 */
inline Array softmax(const Array& x, double beta = 1.0) {
    Array exp_x = (beta * x).exp();
    return exp_x / exp_x.sum();
}

/**
 * @brief ELU (Exponential Linear Unit) activation function
 * 
 * f(x) = x if x > 0 else alpha * (exp(x) - 1)
 */
inline Array elu(const Array& x, double alpha = 1.0) {
    return (x > 0.0).select(x, alpha * (x.exp() - 1.0));
}

/**
 * @brief SELU (Scaled Exponential Linear Unit) activation function
 */
inline Array selu(const Array& x) {
    constexpr double alpha = 1.6732632423543772848170429916717;
    constexpr double scale = 1.0507009873554804934193349852946;
    return scale * (x > 0.0).select(x, alpha * (x.exp() - 1.0));
}

/**
 * @brief Swish activation function
 * 
 * f(x) = x * sigmoid(x)
 */
inline Array swish(const Array& x) {
    return x * sigmoid(x);
}

/**
 * @brief GELU (Gaussian Error Linear Unit) activation function
 * 
 * Approximate version: f(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
 */
inline Array gelu(const Array& x) {
    constexpr double sqrt_2_over_pi = 0.7978845608028654;
    return 0.5 * x * (1.0 + (sqrt_2_over_pi * (x + 0.044715 * x.cube())).tanh());
}

// Function type for activation functions
using ActivationFunc = std::function<Array(const Array&)>;

/**
 * @brief Get activation function by name
 * 
 * @param name Name of the activation function
 * @return Activation function
 * @throws std::invalid_argument if name is not recognized
 */
inline ActivationFunc get_function(const std::string& name) {
    static std::unordered_map<std::string, ActivationFunc> functions;
    static bool initialized = false;
    
    if (!initialized) {
        functions["identity"] = [](const Array& x) -> Array { return identity(x); };
        functions["id"] = [](const Array& x) -> Array { return identity(x); };
        functions["sigmoid"] = [](const Array& x) -> Array { return sigmoid(x); };
        functions["sig"] = [](const Array& x) -> Array { return sigmoid(x); };
        functions["tanh"] = [](const Array& x) -> Array { return tanh(x); };
        functions["relu"] = [](const Array& x) -> Array { return relu(x); };
        functions["re"] = [](const Array& x) -> Array { return relu(x); };
        functions["leaky_relu"] = [](const Array& x) -> Array { return leaky_relu(x); };
        functions["softplus"] = [](const Array& x) -> Array { return softplus(x); };
        functions["sp"] = [](const Array& x) -> Array { return softplus(x); };
        functions["softmax"] = [](const Array& x) -> Array { return softmax(x); };
        functions["smax"] = [](const Array& x) -> Array { return softmax(x); };
        functions["elu"] = [](const Array& x) -> Array { return elu(x); };
        functions["selu"] = [](const Array& x) -> Array { return selu(x); };
        functions["swish"] = [](const Array& x) -> Array { return swish(x); };
        functions["gelu"] = [](const Array& x) -> Array { return gelu(x); };
        initialized = true;
    }

    auto it = functions.find(name);
    if (it == functions.end()) {
        throw std::invalid_argument("Unknown activation function: " + name);
    }
    return it->second;
}

/**
 * @brief Check if activation function name is valid
 */
inline bool is_valid_activation(const std::string& name) {
    static const std::unordered_map<std::string, bool> valid_names = {
        {"identity", true}, {"id", true},
        {"sigmoid", true}, {"sig", true},
        {"tanh", true},
        {"relu", true}, {"re", true},
        {"leaky_relu", true},
        {"softplus", true}, {"sp", true},
        {"softmax", true}, {"smax", true},
        {"elu", true}, {"selu", true},
        {"swish", true}, {"gelu", true}
    };
    return valid_names.count(name) > 0;
}

} // namespace activations
} // namespace reservoircpp

#endif // RESERVOIRCPP_ACTIVATIONSFUNC_HPP
