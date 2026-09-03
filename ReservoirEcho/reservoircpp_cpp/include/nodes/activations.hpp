#ifndef RESERVOIRCPP_ACTIVATIONS_HPP
#define RESERVOIRCPP_ACTIVATIONS_HPP

#include "reservoircpp/activationsfunc/get_function.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/functools/partial.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/10/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto node(auto node, auto x, auto **kwargs) {
    return node.f(x, **kwargs)


auto x(auto node, auto auto x = None, auto *args, auto **kwargs) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


class Softmax: public Node {
    /**
Softmax activation function.

    .. math::

        auto y_k = \\frac{e^{\\beta x_k}}{\\sum_{i=1}^{n} e^{\\beta x_i}}

    :py:attr:`Softmax.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.softmax`).
    ``beta``      Softmax :math:`\\beta` parameter (1.0 by default).
    ============= ======================================================================

    Parameters
    ----------
    beta: float, default to 1.0
        Beta parameter of softmax.
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto beta(auto self, double auto beta = 1.0, auto **kwargs) {
        super(Softmax, self).__init__(
            auto hypers = {"f": get_function("softmax"), "beta": beta},
            auto forward = partial(forward, beta=beta),
            auto initializer = initialize,
            **kwargs,
        )


class Softplus: public Node {
    /**
Softplus activation function.

    .. math::

        f(x) = \\mathrm{ln}(1 + e^{x})

    :py:attr:`Softplus.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.softplus`).
    ============= ======================================================================

    Parameters
    ----------
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto __init__(auto self, auto **kwargs) {
        super(Softplus, self).__init__(
            auto hypers = {"f": get_function("softplus")},
            auto forward = forward,
            auto initializer = initialize,
            **kwargs,
        )


class Sigmoid: public Node {
    /**
Sigmoid activation function.

    .. math::

        f(x) = \\frac{1}{1 + e^{-x}}

    :py:attr:`Sigmoid.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.sigmoid`).
    ============= ======================================================================

    Parameters
    ----------
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto __init__(auto self, auto **kwargs) {
        super(Sigmoid, self).__init__(
            auto hypers = {"f": get_function("sigmoid")},
            auto forward = forward,
            auto initializer = initialize,
            **kwargs,
        )


class Tanh: public Node {
    /**
Hyperbolic tangent activation function.

    .. math::

        f(x) = \\frac{e^x - e^{-x}}{e^x + e^{-x}}

    :py:attr:`Tanh.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.tanh`).
    ============= ======================================================================

    Parameters
    ----------
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto __init__(auto self, auto **kwargs) {
        super(Tanh, self).__init__(
            auto hypers = {"f": get_function("tanh")},
            auto forward = forward,
            auto initializer = initialize,
            **kwargs,
        )


class Identity: public Node {
    /**
Identity function.

    .. math::

        f(x) = x

    Provided for convenience.

    :py:attr:`Identity.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.identity`).
    ============= ======================================================================

    Parameters
    ----------
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto __init__(auto self, auto **kwargs) {
        super(Identity, self).__init__(
            auto hypers = {"f": get_function("identity")},
            auto forward = forward,
            auto initializer = initialize,
            **kwargs,
        )


class ReLU: public Node {
    /**
ReLU activation function.

    .. math::

        f(x) = x ~~ \\mathrm{if} ~~ x > 0 ~~ \\mathrm{else} ~~ 0

    :py:attr:`ReLU.hypers` **list**

    ============= ======================================================================
    ``f``         Activation function (:py:func:`reservoir.activationsfunc.relu`).
    ============= ======================================================================

    Parameters
    ----------
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    
*/

    auto __init__(auto self, auto **kwargs) {
        super(ReLU, self).__init__(
            auto hypers = {"f": get_function("relu")},
            auto forward = forward,
            auto initializer = initialize,
            **kwargs,
        )


#endif // RESERVOIRCPP_ACTIVATIONS_HPP
