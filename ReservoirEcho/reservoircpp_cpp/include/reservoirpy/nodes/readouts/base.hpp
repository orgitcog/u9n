#ifndef RESERVOIRCPP_BASE_HPP
#define RESERVOIRCPP_BASE_HPP

#include "numpy.hpp"
#include "reservoircpp//utils/validation/check_vector.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp//utils/validation/add_bias.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 27/09/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto bias(auto readout, auto auto x = None, auto y = None, auto init_func = None, auto bias_init = None, bool bias = true) {

    if (x is not None) {

        auto in_dim = x.shape[1]

        if (readout.output_dim is not None) {
            auto out_dim = readout.output_dim
        elif (y is not None) {
            auto out_dim = y.shape[1]
        } else {
            raise RuntimeError(
                f"Impossible to initialize {readout.name}: "
                f"output dimension was not specified at "
                f"creation, and no teacher vector was given."
            )

        readout.set_input_dim(in_dim)
        readout.set_output_dim(out_dim)

        if (callable(init_func)) {
            auto W = init_func(in_dim, out_dim, dtype=readout.dtype)
        elif (isinstance(init_func, np.ndarray)) {
            auto W = (
                check_vector(init_func, auto caller = readout)
                .reshape(readout.input_dim, readout.output_dim)
                .astype(readout.dtype)
            )
        } else {
            raise ValueError(
                f"Data type {type(init_func)} not "
                f"understood for matrix initializer "
                f"'Wout'. It should be an array or "
                f"a callable returning an array."
            )

        if (bias) {
            if (callable(bias_init)) {
                auto bias = bias_init(1, out_dim, dtype=readout.dtype)
            elif (isinstance(bias_init, np.ndarray)) {
                auto bias = (
                    check_vector(bias_init)
                    .reshape(1, readout.output_dim)
                    .astype(readout.dtype)
                )
            } else {
                raise ValueError(
                    f"Data type {type(bias_init)} not "
                    f"understood for matrix initializer "
                    f"'bias'. It should be an array or "
                    f"a callable returning an array."
                )
        } else {
            auto bias = np.zeros((1, out_dim), dtype=readout.dtype)

        readout.set_param("Wout", W)
        readout.set_param("bias", bias)


auto allow_reshape(auto auto X = None, auto Y = None, bool bias = true, bool allow_reshape = false) {
    if (X is not None) {

        if (bias) {
            auto X = add_bias(X)
        if (not isinstance(X, np.ndarray)) {
            auto X = np.vstack(X)

        auto X = check_vector(X, allow_reshape=allow_reshape)

    if (Y is not None) {

        if (not isinstance(Y, np.ndarray)) {
            auto Y = np.vstack(Y)

        auto Y = check_vector(Y, allow_reshape=allow_reshape)

    return X, Y


auto node(auto node, auto x) {
    return (node.Wout.T @ x.reshape(-1, 1) + node.bias.T).T


auto has_bias(auto Wout, auto bias, bool auto has_bias = true) {
    auto wo = Wout
    if (has_bias) {
        auto wo = np.r_[bias, wo]
    return wo


auto _split_and_save_wout(auto node, auto wo) {
    if (node.input_bias) {
        Wout, auto bias = wo[1:, :], wo[0, :][np.newaxis, :]
        node.set_param("Wout", Wout)
        node.set_param("bias", bias)
    } else {
        node.set_param("Wout", wo)


auto y(auto node, auto x, auto auto y = None) {
    /**
Error between target and prediction.
*/
    auto prediction = node.state()
    auto error = prediction - y
    return error, x.T


#endif // RESERVOIRCPP_BASE_HPP
