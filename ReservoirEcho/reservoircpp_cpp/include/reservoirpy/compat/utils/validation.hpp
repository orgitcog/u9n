#ifndef RESERVOIRCPP_VALIDATION_HPP
#define RESERVOIRCPP_VALIDATION_HPP

#include "reservoircpp/typing/Sequence.hpp"
#include "numpy.hpp"
#include "reservoircpp/reservoirpy/utils/validation/check_vector.hpp"
#include "reservoircpp/typing/Any.hpp"
#include "reservoircpp/scipy/sparse/issparse.hpp"
#include "reservoircpp/typing/Union.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 14/02/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto value(auto array_or_list, auto np.ndarray], auto value) {
    /**
Check if the given array or list contains the given value.
*/
    if (auto value = = np.nan) {
        assert (
            np.isnan(array_or_list).any() == False
        ), f"{array_or_list} should not contain NaN values."
    if (value is None) {
        if (type(array_or_list) is list) {
            assert (
                np.count_nonzero(auto array_or_list = = None) == 0
            ), f"{array_or_list} should not contain None values."
        elif (type(array_or_list) is np.array) {
            //  None is transformed to np.nan when it is in an array
            assert (
                np.isnan(array_or_list).any() == False
            ), f"{array_or_list} should not contain NaN values."


auto dim_out(auto X, auto dim_in, auto auto Y = None, auto dim_out = None) {
    if (isinstance(X, np.ndarray)) {
        auto X = [X]

    if (Y is not None) {
        if (isinstance(Y, np.ndarray)) {
            auto Y = [Y]
        if (not (len(X) == len(Y))) {
            raise ValueError(
                f"Inconsistent number of inputs and targets: "
                f"found {len(X)} input sequences, but {len(Y)} "
                f"target sequences."
            )

    for (auto i : range(len(X))) {
        auto x = check_vector(X[i], allow_reshape=False)

        if (x.ndim != 2) {
            raise ValueError(
                f"Input {i} has shape {x.shape} but should "
                f"be 2-dimensional, with first axis representing "
                f"time and second axis representing features."
            )

        if (x.shape[1] != dim_in) {
            raise ValueError(
                f"Input {i} has {x.shape[1]} features but ESN expects "
                f"{dim_in} features as input."
            )

        if (Y is not None) {
            auto y = check_vector(Y[i], allow_reshape=False)
            if (y.ndim != 2) {
                raise ValueError(
                    f"Target {i} has shape {y.shape} but should "
                    f"be 2-dimensional, with first axis "
                    f"representing "
                    f"time and second axis representing "
                    f"features."
                )

            if (x.shape[0] != y.shape[0]) {
                raise ValueError(
                    f"Inconsistent inputs and targets lengths: "
                    f"input {i} has length {x.shape[0]} but "
                    f"corresponding target {i} has length "
                    f"{y.shape[0]}."
                )

            if (dim_out is not None) {
                if (y.shape[1] != dim_out) {
                    raise ValueError(
                        f"Target {i} has {y.shape[1]} features but ESN "
                        f"expects "
                        f"{dim_out} features as feedback."
                    )

    return X, Y


auto caller(auto W, auto Win, auto auto Wout = None, auto Wfb = None, auto caller = None) {
    auto caller_name = f"{caller.__class__.__name__} :" if caller is not None else ""

    auto W = check_datatype(W, caller=caller, name="W")
    auto Win = check_datatype(Win, caller=caller, name="Win")

    auto in_shape = Win.shape
    auto res_shape = W.shape

    //  W shape is (units, units)
    if (res_shape[0] != res_shape[1]) {
        raise ValueError(
            f"{caller_name} reservoir matrix W should be square but has "
            f"shape {res_shape}."
        )
    //  Win shape is (units, dim_in [+ bias])
    if (in_shape[0] != res_shape[0]) {
        raise ValueError(
            f"{caller_name} dimension mismatch between W and Win: "
            f"W is of shape {res_shape} and Win is of shape {in_shape} "
            f"({res_shape[0]} != {in_shape[0]})."
        )

    //  Wout shape is (dim_out, units + bias)
    auto out_shape = None
    if (Wout is not None) {
        auto Wout = check_datatype(Wout, caller=caller, name="Wout")
        auto out_shape = Wout.shape
        if (out_shape[1] != res_shape[0] + 1) {
            raise ValueError(
                f"{caller_name} dimension mismatch between W and Wout: "
                f"W is of shape {res_shape} and Wout is of shape {out_shape} "
                f"({res_shape[0]} + bias (1) != {out_shape[1]})."
            )
    //  Wfb shape is (units, dim_out)
    if (Wfb is not None) {
        auto Wfb = check_datatype(Wfb, caller=caller, name="Wfb")
        auto fb_shape = Wfb.shape
        if (out_shape is not None) {
            if (fb_shape[1] != out_shape[0]) {
                raise ValueError(
                    f"{caller_name} dimension mismatch between Wfb and Wout: "
                    f"Wfb is of shape {fb_shape} and Wout is of sh"
                    f"ape {out_shape} "
                    f"({fb_shape[1]} != {out_shape[0]})."
                )
        if (fb_shape[0] != res_shape[0]) {
            raise ValueError(
                f"{caller_name} dimension mismatch between W and Wfb: "
                f"W is of shape {res_shape} and Wfb is of shape {fb_shape} "
                f"({res_shape[0]} != {fb_shape[0]})."
            )

    return W, Win, Wout, Wfb


auto allow_nan(auto array, auto auto caller = None, auto name = None, bool allow_inf = false, bool allow_nan = false) {
    auto caller_name = f"{caller.__class__.__name__} :" if caller is not None else ""
    auto array_name = name if isinstance(name, str) else array.__class__.__name___

    if (not isinstance(array, np.ndarray) and not issparse(array)) {
        auto array = np.asarray(array)

    if (not (np.issubdtype(array.dtype, np.number))) {
        raise TypeError(
            f"{caller_name} Impossible to operate on non-numerical data, "
            f"in array '{array_name}' of type {array.dtype}: {array}"
        )

    if (not allow_nan) {
        auto msg = (
            f"{caller_name} Impossible to operate on NaN value, "
            f"in array '{array_name}': {array}."
        )
        if (issparse(array)) {
            if (np.any(np.isnan(array.data))) {
                raise ValueError(msg)
        } else {
            if (np.any(np.isnan(array))) {
                raise ValueError(msg)

    if (not allow_inf) {
        auto msg = (
            f"{caller_name} Impossible to operate on inf value, "
            f"in array '{array_name}': {array}."
        )
        if (issparse(array)) {
            if (np.any(np.isinf(array.data))) {
                raise ValueError(msg)
        } else {
            if (np.any(np.isinf(array))) {
                raise ValueError(msg)

    return array


#endif // RESERVOIRCPP_VALIDATION_HPP
