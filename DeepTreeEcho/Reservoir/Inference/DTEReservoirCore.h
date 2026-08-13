/*
 * DeepTreeEcho/Inference/DTEReservoirCore.h
 *
 * Standalone Echo State Network reservoir for the Deep Tree Echo
 * in-engine inference layer (Gap A of the DTE integration map).
 *
 * Ported from o9nn/opencog-esn ReservoirNode (AGPLv3), with all
 * AtomSpace dependencies (Handle / Node / FloatValue / ClassServer)
 * stripped. The Eigen math is preserved verbatim:
 *
 *   s[t] = (1 - alpha) * s[t-1] + alpha * tanh(W_in * x[t] + W * s[t-1])
 *
 * where W is a sparse random recurrent matrix rescaled to a target
 * spectral radius via Eigen::EigenSolver, and W_in is a dense random
 * input matrix scaled by input_scaling.
 *
 * Plain C++17 + Eigen only — compiles in the CMake standalone build
 * and can be wrapped later by a UE component (no UE dependencies).
 */

#ifndef DTE_INFERENCE_RESERVOIR_CORE_H
#define DTE_INFERENCE_RESERVOIR_CORE_H

#include <cstddef>
#include <string>
#include <vector>

#include <Eigen/Dense>

namespace dte
{

/**
 * DTEReservoirCore — leaky-integrator ESN reservoir.
 *
 * Hyperparameters and state are plain members; Serialize()/Deserialize()
 * provide the flat-vector persistence that ReservoirNode previously
 * implemented via attached AtomSpace FloatValues.
 */
class DTEReservoirCore
{
public:
    /// Flat serialized form (replaces AtomSpace value storage).
    struct Snapshot
    {
        std::size_t input_dim = 0;
        std::size_t reservoir_dim = 0;
        double leaking_rate = 0.3;
        double spectral_radius = 0.9;
        std::vector<double> state;             // reservoir_dim
        std::vector<double> input_weights;     // reservoir_dim * input_dim (col-major)
        std::vector<double> recurrent_weights; // reservoir_dim * reservoir_dim (col-major)
    };

    explicit DTEReservoirCore(std::string name = "dte:reservoir");

    DTEReservoirCore(const DTEReservoirCore&) = default;
    DTEReservoirCore& operator=(const DTEReservoirCore&) = default;
    ~DTEReservoirCore() = default;

    // -------------------------------------------------------
    // Initialization
    // -------------------------------------------------------

    /**
     * Initialize the reservoir with random weights.
     *
     * @param input_dim       Input dimensionality
     * @param reservoir_dim   Reservoir dimensionality
     * @param spectral_radius Spectral radius of recurrent weights
     * @param leaking_rate    Leaking rate alpha for the state update
     * @param sparsity        Sparsity of recurrent matrix (fraction of zeros, 0-1)
     * @param input_scaling   Scaling factor for input weights
     * @param seed            Random seed for reproducibility
     */
    void initialize(std::size_t input_dim,
                    std::size_t reservoir_dim,
                    double spectral_radius = 0.9,
                    double leaking_rate = 0.3,
                    double sparsity = 0.9,
                    double input_scaling = 1.0,
                    unsigned int seed = 42);

    /// Reset the reservoir state to zeros (weights untouched).
    void reset_state();

    // -------------------------------------------------------
    // State update
    // -------------------------------------------------------

    /**
     * Perform one leaky-integrator update step.
     *
     * @param input Input vector x[t] (size must equal input_dim)
     * @return New reservoir state s[t]
     * @throws std::invalid_argument on dimension mismatch
     * @throws std::logic_error if not initialized
     */
    Eigen::VectorXd update(const Eigen::VectorXd& input);

    /// Convenience overload on std::vector (mirrors old FloatValue path).
    std::vector<double> update(const std::vector<double>& input);

    /**
     * Run a full input sequence through the reservoir, collecting the
     * state after each step (utility for readout training).
     *
     * @param inputs  Matrix whose columns are consecutive input vectors
     * @return Matrix whose columns are the corresponding reservoir states
     */
    Eigen::MatrixXd run(const Eigen::MatrixXd& inputs);

    // -------------------------------------------------------
    // Accessors
    // -------------------------------------------------------

    const std::string& name() const { return _name; }
    const Eigen::VectorXd& get_state() const { return _state; }
    const Eigen::MatrixXd& get_input_weights() const { return _W_in; }
    const Eigen::MatrixXd& get_recurrent_weights() const { return _W; }
    std::size_t get_reservoir_dim() const { return _reservoir_dim; }
    std::size_t get_input_dim() const { return _input_dim; }
    double get_leaking_rate() const { return _leaking_rate; }
    double get_spectral_radius() const { return _spectral_radius; }
    bool is_initialized() const { return _initialized; }

    /// Measured spectral radius of the (rescaled) recurrent matrix.
    double measured_spectral_radius() const;

    // -------------------------------------------------------
    // Serialization (replaces store_to_values / load_from_values)
    // -------------------------------------------------------

    Snapshot serialize() const;
    void deserialize(const Snapshot& snap);

private:
    std::string _name;

    Eigen::MatrixXd _W_in;   // Input weights  (reservoir_dim x input_dim)
    Eigen::MatrixXd _W;      // Recurrent weights (reservoir_dim x reservoir_dim)
    Eigen::VectorXd _state;  // Current state s[t]

    std::size_t _input_dim = 0;
    std::size_t _reservoir_dim = 0;
    double _leaking_rate = 0.3;
    double _spectral_radius = 0.9;
    double _sparsity = 0.9;
    double _input_scaling = 1.0;
    bool _initialized = false;

    void check_input(const Eigen::VectorXd& input) const;
};

} // namespace dte

#endif // DTE_INFERENCE_RESERVOIR_CORE_H
