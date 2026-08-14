/*
 * DeepTreeEcho/Inference/DTERidgeReadout.cpp
 *
 * Ported from o9nn/opencog-esn RidgeNode.cc — AtomSpace stripped,
 * Cholesky ridge-regression math preserved verbatim.
 */

#include "DTERidgeReadout.h"

#include <stdexcept>

#include <Eigen/Cholesky>

namespace dte
{

DTERidgeReadout::DTERidgeReadout(std::string name)
    : _name(std::move(name))
{
}

void DTERidgeReadout::initialize(std::size_t input_dim,
                                 std::size_t output_dim,
                                 double regularization)
{
    if (input_dim == 0 || output_dim == 0)
        throw std::invalid_argument(
            "DTERidgeReadout::initialize: dimensions must be non-zero");

    _input_dim = input_dim;
    _output_dim = output_dim;
    _regularization = regularization;

    _W_out = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(output_dim),
                                   static_cast<Eigen::Index>(input_dim));
    _bias = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(output_dim));
    _is_trained = false;
    clear_samples();
}

void DTERidgeReadout::reset()
{
    if (_output_dim > 0 && _input_dim > 0) {
        _W_out = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(_output_dim),
                                       static_cast<Eigen::Index>(_input_dim));
        _bias = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(_output_dim));
    }
    _is_trained = false;
    clear_samples();
}

void DTERidgeReadout::collect_sample(const Eigen::VectorXd& state,
                                     const Eigen::VectorXd& target)
{
    if (_input_dim == 0)
        throw std::logic_error(
            "DTERidgeReadout not initialized. Call initialize() first.");
    if (state.size() != static_cast<Eigen::Index>(_input_dim))
        throw std::invalid_argument(
            "DTERidgeReadout::collect_sample: state dimension mismatch");
    if (target.size() != static_cast<Eigen::Index>(_output_dim))
        throw std::invalid_argument(
            "DTERidgeReadout::collect_sample: target dimension mismatch");

    _collected_states.push_back(state);
    _collected_targets.push_back(target);
}

void DTERidgeReadout::collect_sample(const std::vector<double>& state,
                                     const std::vector<double>& target)
{
    Eigen::VectorXd s = Eigen::Map<const Eigen::VectorXd>(
        state.data(), static_cast<Eigen::Index>(state.size()));
    Eigen::VectorXd t = Eigen::Map<const Eigen::VectorXd>(
        target.data(), static_cast<Eigen::Index>(target.size()));
    collect_sample(s, t);
}

void DTERidgeReadout::train()
{
    if (_collected_states.empty())
        throw std::logic_error(
            "DTERidgeReadout::train: no samples collected. "
            "Call collect_sample() first.");

    const std::size_t num_samples = _collected_states.size();

    // Build state matrix S (input_dim x num_samples).
    Eigen::MatrixXd S(static_cast<Eigen::Index>(_input_dim),
                      static_cast<Eigen::Index>(num_samples));
    for (std::size_t i = 0; i < num_samples; ++i)
        S.col(static_cast<Eigen::Index>(i)) = _collected_states[i];

    // Build target matrix Y (output_dim x num_samples).
    Eigen::MatrixXd Y(static_cast<Eigen::Index>(_output_dim),
                      static_cast<Eigen::Index>(num_samples));
    for (std::size_t i = 0; i < num_samples; ++i)
        Y.col(static_cast<Eigen::Index>(i)) = _collected_targets[i];

    train(S, Y);
}

void DTERidgeReadout::train(const Eigen::MatrixXd& states,
                            const Eigen::MatrixXd& targets)
{
    if (_input_dim == 0 || _output_dim == 0)
        throw std::logic_error(
            "DTERidgeReadout not initialized. Call initialize() first.");
    if (states.rows() != static_cast<Eigen::Index>(_input_dim))
        throw std::invalid_argument(
            "DTERidgeReadout::train: state row-dimension mismatch");
    if (targets.rows() != static_cast<Eigen::Index>(_output_dim))
        throw std::invalid_argument(
            "DTERidgeReadout::train: target row-dimension mismatch");
    if (states.cols() != targets.cols())
        throw std::invalid_argument(
            "DTERidgeReadout::train: sample-count mismatch between S and Y");

    // Ridge regression: W_out = Y * S^T * (S * S^T + lambda * I)^{-1}
    Eigen::MatrixXd S_ST = states * states.transpose();
    Eigen::MatrixXd regularized = S_ST +
        _regularization * Eigen::MatrixXd::Identity(
            static_cast<Eigen::Index>(_input_dim),
            static_cast<Eigen::Index>(_input_dim));

    // Solve via Cholesky decomposition (more stable than direct inverse).
    Eigen::LLT<Eigen::MatrixXd> llt(regularized);
    if (llt.info() != Eigen::Success) {
        // Fall back to direct inverse if Cholesky fails.
        _W_out = targets * states.transpose() * regularized.inverse();
    } else {
        _W_out = llt.solve((targets * states.transpose()).transpose()).transpose();
    }

    // Bias set to zero (matches upstream RidgeNode behavior).
    _bias = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(_output_dim));

    _is_trained = true;
}

void DTERidgeReadout::clear_samples()
{
    _collected_states.clear();
    _collected_targets.clear();
}

Eigen::VectorXd DTERidgeReadout::predict(const Eigen::VectorXd& state) const
{
    if (!_is_trained)
        throw std::logic_error(
            "DTERidgeReadout::predict: not trained. Call train() first.");
    if (state.size() != static_cast<Eigen::Index>(_input_dim))
        throw std::invalid_argument(
            "DTERidgeReadout::predict: state dimension mismatch");

    return _W_out * state + _bias;
}

std::vector<double> DTERidgeReadout::predict(const std::vector<double>& state) const
{
    Eigen::VectorXd s = Eigen::Map<const Eigen::VectorXd>(
        state.data(), static_cast<Eigen::Index>(state.size()));
    Eigen::VectorXd out = predict(s);
    return std::vector<double>(out.data(), out.data() + out.size());
}

DTERidgeReadout::Snapshot DTERidgeReadout::serialize() const
{
    Snapshot snap;
    snap.input_dim = _input_dim;
    snap.output_dim = _output_dim;
    snap.regularization = _regularization;
    snap.is_trained = _is_trained;
    snap.output_weights.assign(_W_out.data(), _W_out.data() + _W_out.size());
    snap.bias.assign(_bias.data(), _bias.data() + _bias.size());
    return snap;
}

void DTERidgeReadout::deserialize(const Snapshot& snap)
{
    _input_dim = snap.input_dim;
    _output_dim = snap.output_dim;
    _regularization = snap.regularization;
    _is_trained = snap.is_trained;

    const auto od = static_cast<Eigen::Index>(_output_dim);
    const auto id = static_cast<Eigen::Index>(_input_dim);

    if (!snap.output_weights.empty() && _output_dim > 0 && _input_dim > 0)
        _W_out = Eigen::Map<const Eigen::MatrixXd>(
            snap.output_weights.data(), od, id);
    else if (_output_dim > 0 && _input_dim > 0)
        _W_out = Eigen::MatrixXd::Zero(od, id);

    if (!snap.bias.empty())
        _bias = Eigen::Map<const Eigen::VectorXd>(
            snap.bias.data(), static_cast<Eigen::Index>(snap.bias.size()));
    else if (_output_dim > 0)
        _bias = Eigen::VectorXd::Zero(od);
}

} // namespace dte
