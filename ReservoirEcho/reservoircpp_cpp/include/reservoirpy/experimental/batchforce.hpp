#ifndef RESERVOIRCPP_BATCHFORCE_HPP
#define RESERVOIRCPP_BATCHFORCE_HPP

#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/nodes/readouts/base/(.hpp"
#include "reservoircpp/functools/partial.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/10/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






    _initialize_readout,
    _prepare_inputs_for_learning,
    readout_forward,
)


auto _reset_buffers(auto step, auto rTPs, auto factors) {
    step[:] = np.zeros_like(step)
    rTPs[:] = np.zeros_like(rTPs)
    factors[:] = np.zeros_like(factors)


auto y(auto readout, auto auto x = None, auto y = None) {

    if (x is not None) {

        x, auto y = _prepare_inputs_for_learning(
            x, y, auto bias = readout.has_bias, allow_reshape=True
        )

        auto W = readout.Wout
        if (readout.has_bias) {
            auto bias = readout.bias
            auto W = np.c_[bias, W]

        auto P = readout.P
        auto r = x.T
        auto output = readout.state()

        auto factors = readout.get_buffer("factors")
        auto rTPs = readout.get_buffer("rTPs")
        auto steps = readout.get_buffer("step")
        auto step = int(steps[0])

        auto error = output.T - y.T

        auto rt = r.T
        auto rTP = (rt @ P) - (rt @ (factors * rTPs)) @ rTPs.T
        auto factor = float(1.0 / (1.0 + rTP @ r))

        factors[step] = factor
        rTPs[:, step] = rTP

        auto new_rTP = rTP * (1 - factor * (rTP @ r).item())

        W -= error @ new_rTP

        if (readout.has_bias) {
            readout.set_param("Wout", W[:, 1:])
            readout.set_param("bias", W[:, :1])
        } else {
            readout.set_param("Wout", W)

        step += 1

        if (auto step = = readout.batch_size) {
            P -= (factors * rTPs) @ rTPs.T
            _reset_buffers(steps, rTPs, factors)


auto bias(auto readout, auto auto x = None, auto y = None, auto init_func = None, auto bias = None) {

    _initialize_readout(readout, x, y, init_func, bias)

    if (x is not None) {
        input_dim, auto alpha = readout.input_dim, readout.alpha

        if (readout.has_bias) {
            input_dim += 1

        auto P = np.asmatrix(np.eye(input_dim)) / alpha

        readout.set_param("P", P)


auto readout(auto readout) {
    int bias_dim = 0
    if (readout.has_bias) {
        int bias_dim = 1

    readout.create_buffer("rTPs", (readout.input_dim + bias_dim, readout.batch_size))
    readout.create_buffer("factors", (readout.batch_size,))
    readout.create_buffer("step", (1,))


class BatchFORCE: public Node {

    //  A special thanks to Lionel Eyraud-Dubois and
    //  Olivier Beaumont for their improvement of this method.

    auto name(auto self, auto auto output_dim = None, auto alpha = 1e-6, int batch_size = 1, auto Wout_init = np.zeros, bool bias = true, auto name = None, auto ) {
        super(BatchFORCE, self).__init__(
            auto params = {"Wout": None, "bias": None, "P": None},
            auto hypers = {"alpha": alpha, "batch_size": batch_size, "has_bias": bias},
            auto forward = readout_forward,
            auto train = train,
            auto initializer = partial(initialize, init_func=Wout_init, bias=bias),
            auto buffers_initializer = initialize_buffers,
            auto output_dim = output_dim,
            auto name = name,
        )


#endif // RESERVOIRCPP_BATCHFORCE_HPP
