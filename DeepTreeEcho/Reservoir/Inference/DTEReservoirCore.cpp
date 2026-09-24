/*
 * DeepTreeEcho/Inference/DTEReservoirCore.cpp
 *
 * Ported from o9nn/opencog-esn ReservoirNode.cc — AtomSpace stripped,
 * Eigen math preserved verbatim. See header for details.
 */

#include "DTEReservoirCore.h"

#include <cmath>
#include <random>
#include <stdexcept>

#include <Eigen/Eigenvalues>

namespace dte
{

DTEReservoirCore::DTEReservoirCore(std::string name)
    : _name(std::move(name))
{
}

void DTEReservoirCore::initialize(std::size_t input_dim,
                                  std::size_t reservoir_dim,
                                  double spectral_radius,
                                  double leaking_rate,
                                  double sparsity,
                                  double input_scaling,
                                  unsigned int seed)
{
    if (input_dim == 0 || reservoir_dim == 0)
        throw std::invalid_argument(
            "DTEReservoirCore::initialize: dimensions must be non-zero");

    _input_dim = input_dim;
    _reservoir_dim = reservoir_dim;
    _spectral_radius = spectral_radius;
    _leaking_rate = leaking_rate;
    _sparsity = sparsity;
    _input_scaling = input_scaling;

    // Random number generation (identical scheme to ReservoirNode).
    std::mt19937 gen(seed);
    std::uniform_real_distribution<> uniform(-1.0, 1.0);
    std::uniform_real_distribution<> sparse_dist(0.0, 1.0);

    // Dense input weight matrix W_in scaled by input_scaling.
    _W_in = Eigen::MatrixXd::Zero(reservoir_dim, input_dim);
    for (std::size_t i = 0; i < reservoir_dim; ++i)
        for (std::size_t j = 0; j < input_dim; ++j)
            _W_in(i, j) = uniform(gen) * input_scaling;

    // Sparse recurrent weight matrix W.
    _W = Eigen::MatrixXd::Zero(reservoir_dim, reservoir_dim);
    for (std::size_t i = 0; i < reservoir_dim; ++i)
        for (std::size_t j = 0; j < reservoir_dim; ++j)
            if (sparse_dist(gen) > sparsity)
                _W(i, j) = uniform(gen);

    // Rescale W to the desired spectral radius.
    Eigen::EigenSolver<Eigen::MatrixXd> solver(_W);
    double current_sr = solver.eigenvalues().cwiseAbs().maxCoeff();
    if (current_sr > 0.0)
        _W *= (spectral_radius / current_sr);

    _state = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(reservoir_dim));
    _initialized = true;
}

void DTEReservoirCore::reset_state()
{
    if (_reservoir_dim > 0)
        _state = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(_reservoir_dim));
}

void DTEReservoirCore::check_input(const Eigen::VectorXd& input) const
{
    if (!_initialized)
        throw std::logic_error(
            "DTEReservoirCore::update: reservoir not initialized");
    if (input.size() != static_cast<Eigen::Index>(_input_dim))
        throw std::invalid_argument(
            "DTEReservoirCore::update: input dimension mismatch (expected " +
            std::to_string(_input_dim) + ", got " +
            std::to_string(static_cast<long>(input.size())) + ")");
}

Eigen::VectorXd DTEReservoirCore::update(const Eigen::VectorXd& input)
{
    check_input(input);

    // Leaky integrator state update:
    // s[t] = (1 - alpha) * s[t-1] + alpha * tanh(W_in * x[t] + W * s[t-1])
    Eigen::VectorXd pre_activation = _W_in * input + _W * _state;
    Eigen::VectorXd activation = pre_activation.array().tanh();
    _state = (1.0 - _leaking_rate) * _state + _leaking_rate * activation;

    return _state;
}

std::vector<double> DTEReservoirCore::update(const std::vector<double>& input)
{
    Eigen::VectorXd eigen_input = Eigen::Map<const Eigen::VectorXd>(
        input.data(), static_cast<Eigen::Index>(input.size()));
    Eigen::VectorXd new_state = update(eigen_input);
    return std::vector<double>(new_state.data(),
                               new_state.data() + new_state.size());
}

Eigen::MatrixXd DTEReservoirCore::run(const Eigen::MatrixXd& inputs)
{
    if (!_initialized)
        throw std::logic_error("DTEReservoirCore::run: reservoir not initialized");
    if (inputs.rows() != static_cast<Eigen::Index>(_input_dim))
        throw std::invalid_argument(
            "DTEReservoirCore::run: input row-dimension mismatch");

    Eigen::MatrixXd states(static_cast<Eigen::Index>(_reservoir_dim),
                           inputs.cols());
    for (Eigen::Index t = 0; t < inputs.cols(); ++t)
        states.col(t) = update(Eigen::VectorXd(inputs.col(t)));
    return states;
}

double DTEReservoirCore::measured_spectral_radius() const
{
    if (!_initialized) return 0.0;
    Eigen::EigenSolver<Eigen::MatrixXd> solver(_W);
    return solver.eigenvalues().cwiseAbs().maxCoeff();
}

DTEReservoirCore::Snapshot DTEReservoirCore::serialize() const
{
    Snapshot snap;
    snap.input_dim = _input_dim;
    snap.reservoir_dim = _reservoir_dim;
    snap.leaking_rate = _leaking_rate;
    snap.spectral_radius = _spectral_radius;
    snap.state.assign(_state.data(), _state.data() + _state.size());
    snap.input_weights.assign(_W_in.data(), _W_in.data() + _W_in.size());
    snap.recurrent_weights.assign(_W.data(), _W.data() + _W.size());
    return snap;
}

void DTEReservoirCore::deserialize(const Snapshot& snap)
{
    _input_dim = snap.input_dim;
    _reservoir_dim = snap.reservoir_dim;
    _leaking_rate = snap.leaking_rate;
    _spectral_radius = snap.spectral_radius;

    const auto rd = static_cast<Eigen::Index>(_reservoir_dim);
    const auto id = static_cast<Eigen::Index>(_input_dim);

    if (!snap.state.empty())
        _state = Eigen::Map<const Eigen::VectorXd>(
            snap.state.data(), static_cast<Eigen::Index>(snap.state.size()));
    else if (_reservoir_dim > 0)
        _state = Eigen::VectorXd::Zero(rd);

    if (!snap.input_weights.empty() && _reservoir_dim > 0 && _input_dim > 0)
        _W_in = Eigen::Map<const Eigen::MatrixXd>(
            snap.input_weights.data(), rd, id);

    if (!snap.recurrent_weights.empty() && _reservoir_dim > 0)
        _W = Eigen::Map<const Eigen::MatrixXd>(
            snap.recurrent_weights.data(), rd, rd);

    _initialized = (_reservoir_dim > 0 && _input_dim > 0);
}

} // namespace dte
