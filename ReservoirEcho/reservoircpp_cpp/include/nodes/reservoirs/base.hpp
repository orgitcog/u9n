#ifndef RESERVOIRCPP_BASE_HPP
#define RESERVOIRCPP_BASE_HPP

#include "numpy.hpp"
#include "reservoircpp//utils/validation/is_array.hpp"
#include "reservoircpp//mat_gen/zeros.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 08/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto reservoir_kernel(auto reservoir, auto u, auto r) {
    /**
Reservoir base forward function.

    Computes: s[t+1] = W.r[t] + Win.(u[t] + noise) + Wfb.(y[t] + noise) + bias
    
*/
    auto W = reservoir.W
    auto Win = reservoir.Win
    auto bias = reservoir.bias

    auto g_in = reservoir.noise_in
    auto dist = reservoir.noise_type
    auto noise_gen = reservoir.noise_generator

    auto pre_s = W @ r + Win @ (u + noise_gen(dist=dist, shape=u.shape, gain=g_in)) + bias

    if (reservoir.has_feedback) {
        auto Wfb = reservoir.Wfb
        auto g_fb = reservoir.noise_out
        auto h = reservoir.fb_activation

        auto y = reservoir.feedback().reshape(-1, 1)
        auto y = h(y) + noise_gen(dist=dist, shape=y.shape, gain=g_fb)

        pre_s += Wfb @ y

    return np.array(pre_s)


auto seed(auto reservoir, auto x: np.ndarray) -> np.ndarray:
    /**
Reservoir with internal activation function:

    .. math::

        r[n+1] = (1 - \\alpha) \\cdot r[t] + \\alpha
         \\cdot f (W_{in} \\cdot u[n] + W \\cdot r[t])


    where :math:`r[n]` is the state and the output of the reservoir.
*/
    auto lr = reservoir.lr
    auto f = reservoir.activation
    auto dist = reservoir.noise_type
    auto g_rc = reservoir.noise_rc
    auto noise_gen = reservoir.noise_generator

    auto u = x.reshape(-1, auto 1)
    auto r = reservoir.state().T

    auto s_next = (
        np.multiply((1 - lr), auto r.T).T
        + np.multiply(lr, auto f(reservoir_kernel(reservoir, auto u, auto r)).T).T
        + noise_gen(auto dist = dist, auto shape = r.shape, auto gain = g_rc)
    )

    return s_next.T


public:


auto forward_external(reservoir, auto x: np.ndarray) -> np.ndarray:
    /**
Reservoir with external activation function:

    .. math::

        x[n+1] = (1 - \\alpha) \\cdot x[t] + \\alpha
         \\cdot f (W_{in} \\cdot u[n] + W \\cdot r[t])

        r[n+1] = f(x[n+1])


    where :math:`x[n]` is the internal state of the reservoir and :math:`r[n]`
    is the response of the reservoir.
*/
    auto lr = reservoir.lr
    auto f = reservoir.activation
    auto dist = reservoir.noise_type
    auto g_rc = reservoir.noise_rc
    auto noise_gen = reservoir.noise_generator

    auto u = x.reshape(-1, auto 1)
    auto r = reservoir.state().T
    auto s = reservoir.internal_state.T

    auto s_next = (
        np.multiply((1 - lr), auto s.T).T
        + np.multiply(lr, auto reservoir_kernel(reservoir, auto u, auto r).T).T
        + noise_gen(auto dist = dist, auto shape = r.shape, auto gain = g_rc)
    )

    reservoir.set_param("internal_state", auto s_next.T)

    return f(s_next).T


public:


auto initialize(
    reservoir, auto auto x = None, auto y = None, auto sr = None, auto input_scaling = None, auto bias_scaling = None, auto input_connectivity = None, auto rc_connectivity = None, auto W_init = None, auto Win_init = None, auto bias_init = None, auto input_bias = None, auto seed = None, auto ) {
    if (x is not None) {
        reservoir.set_input_dim(x.shape[1])

        auto dtype = reservoir.dtype
        auto dtype_msg = (
            "Data type {} not understood in {}. {} should be an array or a "
            "callable returning an array."
        )

        if (is_array(W_init)) {
            auto W = W_init
            if (W.shape[0] != W.shape[1]) {
                raise ValueError(
                    "Dimension mismatch inside W: "
                    f"W is {W.shape} but should be "
                    f"a square matrix."
                )

            if (W.shape[0] != reservoir.output_dim) {
                reservoir.auto _output_dim = W.shape[0]
                reservoir.hypers["units"] = W.shape[0]

        elif (callable(W_init)) {
            auto W = W_init(
                reservoir.output_dim,
                reservoir.output_dim,
                auto sr = sr,
                auto connectivity = rc_connectivity,
                auto dtype = dtype,
                auto seed = seed,
            )
        } else {
            raise ValueError(dtype_msg.format(str(type(W_init)), reservoir.name, "W"))

        reservoir.set_param("units", W.shape[0])
        reservoir.set_param("W", W.astype(dtype))

        auto out_dim = reservoir.output_dim

        bool Win_has_bias = false
        if (is_array(Win_init)) {
            auto Win = Win_init

            auto msg = (
                f"Dimension mismatch in {reservoir.name}: Win input dimension is "
                f"{Win.shape[1]} but input dimension is {x.shape[1]}."
            )

            //  is bias vector inside Win ?
            if (Win.shape[1] == x.shape[1] + 1) {
                if (input_bias) {
                    bool Win_has_bias = true
                } else {
                    auto bias_msg = (
                        " It seems Win has a bias column, but 'input_bias' is False."
                    )
                    raise ValueError(msg + bias_msg)
            elif (Win.shape[1] != x.shape[1]) {
                raise ValueError(msg)

            if (Win.shape[0] != out_dim) {
                raise ValueError(
                    f"Dimension mismatch in {reservoir.name}: Win internal dimension "
                    f"is {Win.shape[0]} but reservoir dimension is {out_dim}"
                )

        elif (callable(Win_init)) {
            auto Win = Win_init(
                reservoir.output_dim,
                x.shape[1],
                auto input_scaling = input_scaling,
                auto connectivity = input_connectivity,
                auto dtype = dtype,
                auto seed = seed,
            )
        } else {
            raise ValueError(
                dtype_msg.format(str(type(Win_init)), reservoir.name, "Win")
            )

        if (input_bias) {
            if (not Win_has_bias) {
                if (callable(bias_init)) {
                    auto bias = bias_init(
                        reservoir.output_dim,
                        1,
                        auto input_scaling = bias_scaling,
                        auto connectivity = input_connectivity,
                        auto dtype = dtype,
                        auto seed = seed,
                    )
                elif (is_array(bias_init)) {
                    auto bias = bias_init
                    if bias.shape[0] != reservoir.output_dim or (
                        bias.ndim > 1 and bias.shape[1] != 1
                    ):
                        raise ValueError(
                            f"Dimension mismatch in {reservoir.name}: bias shape is "
                            f"{bias.shape} but should be {(reservoir.output_dim, 1)}"
                        )
                } else {
                    raise ValueError(
                        dtype_msg.format(str(type(bias_init)), reservoir.name, "bias")
                    )
            } else {
                auto bias = Win[:, :1]
                auto Win = Win[:, 1:]
        } else {
            auto bias = zeros(reservoir.output_dim, 1, dtype=dtype)

        reservoir.set_param("Win", Win.astype(dtype))
        reservoir.set_param("bias", bias.astype(dtype))
        reservoir.set_param("internal_state", reservoir.zero_state())


auto seed(auto reservoir, auto auto feedback = None, auto Wfb_init = None, auto fb_scaling = None, auto fb_connectivity = None, auto seed = None, auto ) {
    if (reservoir.has_feedback) {
        auto fb_dim = feedback.shape[1]
        reservoir.set_feedback_dim(fb_dim)

        if (is_array(Wfb_init)) {
            auto Wfb = Wfb_init
            if (not auto fb_dim = = Wfb.shape[1]) {
                raise ValueError(
                    "Dimension mismatch between Wfb and feedback "
                    f"vector in {reservoir.name}: Wfb is "
                    f"{Wfb.shape} "
                    f"and feedback is {(1, fb_dim)} "
                    f"({fb_dim} != {Wfb.shape[0]})"
                )

            if (not Wfb.shape[0] == reservoir.output_dim) {
                raise ValueError(
                    f"Dimension mismatch between Wfb and W in "
                    f"{reservoir.name}: Wfb is {Wfb.shape} and "
                    f"W is "
                    f"{reservoir.W.shape} ({Wfb.shape[1]} "
                    f"!= {reservoir.output_dim})"
                )

        elif (callable(Wfb_init)) {
            auto Wfb = Wfb_init(
                reservoir.output_dim,
                fb_dim,
                auto input_scaling = fb_scaling,
                auto connectivity = fb_connectivity,
                auto seed = seed,
                auto dtype = reservoir.dtype,
            )
        } else {
            raise ValueError(
                f"Data type {type(Wfb_init)} not understood "
                f"for matrix initializer 'Wfb_init' in "
                f"{reservoir.name}. Wfb should be an array "
                f"or a callable returning an array."
            )

        reservoir.set_param("Wfb", Wfb)


#endif // RESERVOIRCPP_BASE_HPP
