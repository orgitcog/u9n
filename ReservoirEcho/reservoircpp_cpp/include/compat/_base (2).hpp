#ifndef RESERVOIRCPP_COMPAT_BASE_HPP
#define RESERVOIRCPP_COMPAT_BASE_HPP

/**
 * @file compat/_base.hpp
 * @brief Base ESN (Echo State Network) class for backward compatibility
 * 
 * Provides the foundational ESN implementation for the reservoir computing framework.
 * This is the core component of the Deep Tree Echo cognitive architecture.
 */

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <random>
#include <stdexcept>
#include <optional>

#include "../numpy.hpp"
#include "../_version.hpp"
#include "../activationsfunc.hpp"

namespace reservoircpp {
namespace compat {

using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;
using SparseMatrix = Eigen::SparseMatrix<double>;
using ActivationFunc = std::function<Vector(const Vector&)>;

/**
 * @brief Configuration for ESN initialization
 */
struct ESNConfig {
    int units = 100;                    // Number of reservoir units
    double leak_rate = 1.0;             // Leak rate for leaky integrator
    double spectral_radius = 0.9;       // Spectral radius of recurrent weights
    double input_scaling = 1.0;         // Scaling factor for input weights
    double bias_scaling = 0.0;          // Scaling factor for bias
    double sparsity = 0.1;              // Sparsity of recurrent connections
    bool input_bias = true;             // Whether to add bias to inputs
    double noise_in = 0.0;              // Input noise level
    double noise_rc = 0.0;              // Recurrent noise level
    double noise_out = 0.0;             // Output noise level
    std::string activation = "tanh";    // Activation function name
    unsigned int seed = 0;              // Random seed (0 = random)
};

/**
 * @brief Base Echo State Network class
 * 
 * Implements the core ESN functionality including:
 * - State computation with leaky integration
 * - Feedback connections
 * - Noise injection
 * - Various activation functions
 */
class ESNBase {
public:
    ESNBase() = default;
    
    explicit ESNBase(const ESNConfig& config) : config_(config) {
        initialize();
    }

    ESNBase(const Matrix& W, const Matrix& Win, double lr = 1.0, 
            bool input_bias = true, const std::string& activation = "tanh")
        : config_({}), W_(W), Win_(Win) {
        config_.leak_rate = lr;
        config_.input_bias = input_bias;
        config_.activation = activation;
        config_.units = static_cast<int>(W.rows());
        
        activation_func_ = activations::get_function(activation);
        initialized_ = true;
    }

    virtual ~ESNBase() = default;

    // Initialize the ESN with random weights
    void initialize() {
        std::mt19937 gen(config_.seed == 0 ? std::random_device{}() : config_.seed);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        std::normal_distribution<double> normal(0.0, 1.0);

        // Initialize recurrent weights
        W_ = Matrix::Zero(config_.units, config_.units);
        for (int i = 0; i < config_.units; ++i) {
            for (int j = 0; j < config_.units; ++j) {
                if (dist(gen) < config_.sparsity) {
                    W_(i, j) = normal(gen);
                }
            }
        }

        // Scale to desired spectral radius
        Eigen::EigenSolver<Matrix> solver(W_, false);
        double current_sr = solver.eigenvalues().cwiseAbs().maxCoeff();
        if (current_sr > 0) {
            W_ *= config_.spectral_radius / current_sr;
        }

        // Initialize input weights
        int input_dim = input_dim_ > 0 ? input_dim_ : 1;
        int win_cols = config_.input_bias ? input_dim + 1 : input_dim;
        Win_ = Matrix::Zero(config_.units, win_cols);
        for (int i = 0; i < config_.units; ++i) {
            for (int j = 0; j < win_cols; ++j) {
                Win_(i, j) = dist(gen) * config_.input_scaling;
            }
        }

        // Set activation function
        activation_func_ = activations::get_function(config_.activation);
        
        initialized_ = true;
    }

    // Compute next state
    Vector get_next_state(const Vector& input, 
                          const std::optional<Vector>& feedback = std::nullopt,
                          const std::optional<Vector>& last_state = std::nullopt) {
        if (!initialized_) {
            throw std::runtime_error("ESN not initialized");
        }

        // Initialize state
        Vector x = last_state.value_or(Vector::Zero(config_.units));

        // Prepare input with optional bias
        Vector u = input;
        if (config_.input_bias) {
            u.conservativeResize(u.size() + 1);
            u(u.size() - 1) = 1.0;
        }

        // Add input noise
        if (config_.noise_in > 0) {
            u += config_.noise_in * Vector::Random(u.size());
        }

        // Linear transformation
        Vector x1 = Win_ * u + W_ * x;

        // Add feedback if available
        if (feedback.has_value() && Wfb_.size() > 0) {
            Vector fb = fb_func_ ? fb_func_(feedback.value()) : feedback.value();
            if (config_.noise_out > 0) {
                fb += config_.noise_out * Vector::Random(fb.size());
            }
            x1 += Wfb_ * fb;
        }

        // Add recurrent noise
        if (config_.noise_rc > 0) {
            x1 += config_.noise_rc * Vector::Random(x1.size());
        }

        // Leaky integration with activation
        Eigen::ArrayXd x1_arr = x1.array();
        Eigen::ArrayXd activated = activation_func_(x1_arr.matrix()).array();
        Vector result = (1.0 - config_.leak_rate) * x + 
                        config_.leak_rate * activated.matrix();

        return result;
    }

    // Compute states for a sequence of inputs
    Matrix compute_states(const Matrix& inputs, 
                          const std::optional<Matrix>& forced_teacher = std::nullopt,
                          const std::optional<Vector>& init_state = std::nullopt) {
        int n_samples = static_cast<int>(inputs.rows());
        Matrix states(n_samples, config_.units);

        Vector current_state = init_state.value_or(Vector::Zero(config_.units));
        std::optional<Vector> feedback = std::nullopt;

        for (int t = 0; t < n_samples; ++t) {
            current_state = get_next_state(inputs.row(t).transpose(), 
                                           feedback, current_state);
            states.row(t) = current_state.transpose();

            // Update feedback for next step
            if (forced_teacher.has_value()) {
                feedback = forced_teacher->row(t).transpose();
            } else if (Wout_.size() > 0) {
                feedback = Wout_ * current_state;
            }
        }

        return states;
    }

    // Train readout weights using ridge regression
    void fit(const Matrix& inputs, const Matrix& targets, double ridge = 1e-6) {
        Matrix states = compute_states(inputs);
        
        // Ridge regression: Wout = (S^T S + ridge*I)^-1 S^T Y
        Matrix StS = states.transpose() * states;
        StS.diagonal().array() += ridge;
        
        Wout_ = StS.ldlt().solve(states.transpose() * targets).transpose();
        trained_ = true;
    }

    // Run the ESN on input data
    Matrix run(const Matrix& inputs, 
               const std::optional<Vector>& init_state = std::nullopt) {
        if (!trained_ && Wout_.size() == 0) {
            throw std::runtime_error("ESN not trained - call fit() first");
        }

        Matrix states = compute_states(inputs, std::nullopt, init_state);
        return states * Wout_.transpose();
    }

    // Reset internal state
    void reset() {
        current_state_ = Vector::Zero(config_.units);
    }

    // Getters
    int units() const { return config_.units; }
    int dim_in() const { return input_dim_; }
    int dim_out() const { return output_dim_; }
    double leak_rate() const { return config_.leak_rate; }
    double spectral_radius() const { return config_.spectral_radius; }
    bool is_trained() const { return trained_; }
    bool is_initialized() const { return initialized_; }

    const Matrix& W() const { return W_; }
    const Matrix& Win() const { return Win_; }
    const Matrix& Wout() const { return Wout_; }
    const Matrix& Wfb() const { return Wfb_; }

    // Setters
    void set_W(const Matrix& W) { W_ = W; }
    void set_Win(const Matrix& Win) { Win_ = Win; }
    void set_Wout(const Matrix& Wout) { Wout_ = Wout; trained_ = true; }
    void set_Wfb(const Matrix& Wfb) { Wfb_ = Wfb; }
    void set_input_dim(int dim) { input_dim_ = dim; }
    void set_output_dim(int dim) { output_dim_ = dim; }

    // String representation
    std::string repr() const {
        return "ESNBase(units=" + std::to_string(config_.units) + 
               ", lr=" + std::to_string(config_.leak_rate) +
               ", sr=" + std::to_string(config_.spectral_radius) +
               ", trained=" + (trained_ ? "true" : "false") + ")";
    }

protected:
    ESNConfig config_;
    Matrix W_;              // Recurrent weights
    Matrix Win_;            // Input weights
    Matrix Wout_;           // Output weights
    Matrix Wfb_;            // Feedback weights
    
    ActivationFunc activation_func_;
    std::function<Vector(const Vector&)> fb_func_;
    
    Vector current_state_;
    
    int input_dim_ = 0;
    int output_dim_ = 0;
    bool initialized_ = false;
    bool trained_ = false;
};

} // namespace compat
} // namespace reservoircpp

#endif // RESERVOIRCPP_COMPAT_BASE_HPP
