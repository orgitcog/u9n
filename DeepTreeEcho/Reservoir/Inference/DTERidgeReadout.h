/*
 * DeepTreeEcho/Inference/DTERidgeReadout.h
 *
 * Standalone ridge-regression readout for the Deep Tree Echo
 * in-engine inference layer (Gap A of the DTE integration map).
 *
 * Ported from o9nn/opencog-esn RidgeNode (AGPLv3), with all AtomSpace
 * dependencies stripped. The math is preserved verbatim:
 *
 *   W_out = Y * S^T * (S * S^T + lambda * I)^{-1}
 *
 * solved via Cholesky (Eigen::LLT) with a pseudo-inverse fallback,
 * and prediction y = W_out * s + b.
 *
 * Plain C++17 + Eigen only — no UE dependencies.
 */

#ifndef DTE_INFERENCE_RIDGE_READOUT_H
#define DTE_INFERENCE_RIDGE_READOUT_H

#include <cstddef>
#include <string>
#include <vector>

#include <Eigen/Dense>

namespace dte
{

/**
 * DTERidgeReadout — batch-trained linear readout with L2 regularization,
 * typically driven by DTEReservoirCore states.
 */
class DTERidgeReadout
{
public:
    /// Flat serialized form (replaces AtomSpace value storage).
    struct Snapshot
    {
        std::size_t input_dim = 0;
        std::size_t output_dim = 0;
        double regularization = 1e-6;
        bool is_trained = false;
        std::vector<double> output_weights; // output_dim * input_dim (col-major)
        std::vector<double> bias;           // output_dim
    };

    explicit DTERidgeReadout(std::string name = "dte:ridge");

    // -------------------------------------------------------
    // Initialization
    // -------------------------------------------------------

    /**
     * Initialize the readout.
     *
     * @param input_dim      Reservoir state dimensionality
     * @param output_dim     Output dimensionality
     * @param regularization Ridge parameter lambda
     */
    void initialize(std::size_t input_dim,
                    std::size_t output_dim,
                    double regularization = 1e-6);

    /// Zero the weights, clear samples, mark untrained.
    void reset();

    // -------------------------------------------------------
    // Training
    // -------------------------------------------------------

    /**
     * Collect one (state, target) training sample.
     * @throws std::logic_error / std::invalid_argument on misuse.
     */
    void collect_sample(const Eigen::VectorXd& state,
                        const Eigen::VectorXd& target);

    /// Convenience overload on std::vector.
    void collect_sample(const std::vector<double>& state,
                        const std::vector<double>& target);

    /// Train from all collected samples.
    void train();

    /**
     * Train directly from batch matrices.
     *
     * @param states  input_dim x num_samples state matrix S
     * @param targets output_dim x num_samples target matrix Y
     */
    void train(const Eigen::MatrixXd& states, const Eigen::MatrixXd& targets);

    /// Discard collected samples.
    void clear_samples();

    // -------------------------------------------------------
    // Prediction
    // -------------------------------------------------------

    /// y = W_out * s + b. Throws std::logic_error if not trained.
    Eigen::VectorXd predict(const Eigen::VectorXd& state) const;

    /// Convenience overload on std::vector.
    std::vector<double> predict(const std::vector<double>& state) const;

    // -------------------------------------------------------
    // Accessors
    // -------------------------------------------------------

    const std::string& name() const { return _name; }
    const Eigen::MatrixXd& get_output_weights() const { return _W_out; }
    const Eigen::VectorXd& get_bias() const { return _bias; }
    std::size_t get_input_dim() const { return _input_dim; }
    std::size_t get_output_dim() const { return _output_dim; }
    double get_regularization() const { return _regularization; }
    bool is_trained() const { return _is_trained; }
    std::size_t num_samples() const { return _collected_states.size(); }

    // -------------------------------------------------------
    // Serialization
    // -------------------------------------------------------

    Snapshot serialize() const;
    void deserialize(const Snapshot& snap);

private:
    std::string _name;

    Eigen::MatrixXd _W_out; // output_dim x input_dim
    Eigen::VectorXd _bias;  // output_dim

    std::size_t _input_dim = 0;
    std::size_t _output_dim = 0;
    double _regularization = 1e-6;
    bool _is_trained = false;

    std::vector<Eigen::VectorXd> _collected_states;
    std::vector<Eigen::VectorXd> _collected_targets;
};

} // namespace dte

#endif // DTE_INFERENCE_RIDGE_READOUT_H
