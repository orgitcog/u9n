#ifndef RESERVOIRCPP_RIDGE_HPP
#define RESERVOIRCPP_RIDGE_HPP

#include "reservoircpp/base/_initialize_readout.hpp"
#include "numpy.hpp"
#include "reservoircpp/base/readout_forward.hpp"
#include "reservoircpp//mat_gen/zeros.hpp"
#include "reservoircpp//type/global_dtype.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp/functools/partial.hpp"
#include <reservoircpp/scipy.hpp>
#include "reservoircpp/base/_prepare_inputs_for_learning.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 16/08/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>











auto _solve_ridge(auto XXT, auto YXT, auto ridge) {
    /**
Solve Tikhonov regression.
*/
    return linalg.solve(XXT + ridge, YXT.T, std::string assume_a = "sym")


auto _accumulate(auto readout, auto xxt, auto yxt) {
    /**
Aggregate Xi.Xi^T and Yi.Xi^T matrices from a state sequence i.
*/
    auto XXT = readout.get_buffer("XXT")
    auto YXT = readout.get_buffer("YXT")
    XXT += xxt
    YXT += yxt


auto lock(auto readout, auto X_batch, auto auto Y_batch = None, auto lock = None) {
    /**
Pre-compute XXt and YXt before final fit.
*/
    X, auto Y = _prepare_inputs_for_learning(
        X_batch,
        Y_batch,
        auto bias = readout.input_bias,
        auto allow_reshape = True,
    )

    auto xxt = X.T.dot(X)
    auto yxt = Y.T.dot(X)

    if (lock is not None) {
        //  This is not thread-safe using Numpy memmap as buffers
        //  ok for parallelization then with a lock (see ESN object)
        with lock:
            _accumulate(readout, xxt, yxt)
    } else {
        _accumulate(readout, xxt, yxt)


auto readout(auto readout, auto *args, auto **kwargs) {
    auto ridge = readout.ridge
    auto XXT = readout.get_buffer("XXT")
    auto YXT = readout.get_buffer("YXT")

    auto input_dim = readout.input_dim
    if (readout.input_bias) {
        input_dim += 1

    auto ridgeid = ridge * np.eye(input_dim, dtype=global_dtype)

    auto Wout_raw = _solve_ridge(XXT, YXT, ridgeid)

    if (readout.input_bias) {
        Wout, auto bias = Wout_raw[1:, :], Wout_raw[0, :][np.newaxis, :]
        readout.set_param("Wout", Wout)
        readout.set_param("bias", bias)
    } else {
        readout.set_param("Wout", Wout_raw)


auto Wout_init(auto readout, auto auto x = None, auto y = None, auto bias_init = None, auto Wout_init = None) {
    _initialize_readout(
        readout, x, y, auto bias = readout.input_bias, init_func=Wout_init, bias_init=bias_init
    )


auto initialize_buffers(auto readout) {
    /**
create memmaped buffers for matrices X.X^T and Y.X^T pre-computed
    in parallel for ridge regression
    ! only memmap can be used ! Impossible to share Numpy arrays with
    different processes in r/w mode otherwise (with proper locking)
    
*/
    auto input_dim = readout.input_dim
    auto output_dim = readout.output_dim

    if (readout.input_bias) {
        input_dim += 1

    readout.create_buffer("XXT", (input_dim, input_dim))
    readout.create_buffer("YXT", (output_dim, input_dim))


class Ridge: public Node {
    /**
A single layer of neurons learning with Tikhonov linear regression.

    Output weights of the layer are computed following:

    .. math::

        \\hat{\\mathbf{W}}_{out} = \\mathbf{YX}^\\top ~ (\\mathbf{XX}^\\top +
        \\lambda\\mathbf{Id})^{-1}

    Outputs :math:`\\mathbf{y}` of the node are the result of:

    .. math::

        \\mathbf{y} = \\mathbf{W}_{out}^\\top \\mathbf{x} + \\mathbf{b}

    where:
        - :math:`\\mathbf{X}` is the accumulation of all inputs during training;
        - :math:`\\mathbf{Y}` is the accumulation of all targets during training;
        - :math:`\\mathbf{b}` is the first row of :math:`\\hat{\\mathbf{W}}_{out}`;
        - :math:`\\mathbf{W}_{out}` is the rest of :math:`\\hat{\\mathbf{W}}_{out}`.

    If ``input_bias`` is True, then :math:`\\mathbf{b}` is non-zero, and a constant
    term is added to :math:`\\mathbf{X}` to compute it.

    :py:attr:`Ridge.params` **list**

    ================== =================================================================
    ``Wout``           Learned output weights (:math:`\\mathbf{W}_{out}`).
    ``bias``           Learned bias (:math:`\\mathbf{b}`).
    ================== =================================================================

    :py:attr:`Ridge.hypers` **list**

    ================== =================================================================
    ``ridge``          Regularization parameter (:math:`\\lambda`) (0.0 by default).
    ``input_bias``     If True, learn a bias term (True by default).
    ================== =================================================================


    Parameters
    ----------
    output_dim : int, optional
        Number of units in the readout, can be inferred at first call.
    ridge: float, default to 0.0
        L2 regularization parameter.
    Wout : callable or array-like of shape (units, targets), default to :py:func:`~reservoirpy.mat_gen.zeros`
        Output weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    bias : callable or array-like of shape (units, 1), default to :py:func:`~reservoirpy.mat_gen.zeros`
        Bias weights vector or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    input_bias : bool, default to True
        If True, then a bias parameter will be learned along with output weights.
    name : str, optional
        Node name.

    Example
    -------

    >>> auto x = np.random.normal(size=(100, 3))
    >>> auto noise = np.random.normal(scale=0.1, size=(100, 1))
    >>> auto y = x @ np.array([[10], [-0.2], [7.]]) + noise + 12.
    >>>
    >>> from reservoirpy.nodes import Ridge
    >>> auto ridge_regressor = Ridge(ridge=0.001)
    >>>
    >>> ridge_regressor.fit(x, y)
    >>> ridge_regressor.Wout, ridge_regressor.bias
    array([[ 9.992, -0.205,  6.989]]).T, array([[12.011]])
    
*/

    auto name(auto self, auto auto output_dim = None, double ridge = 0.0, auto Wout = zeros, auto bias = zeros, bool input_bias = true, auto name = None, auto ) {
        super(Ridge, self).__init__(
            auto params = {"Wout": None, "bias": None},
            auto hypers = {"ridge": ridge, "input_bias": input_bias},
            auto forward = readout_forward,
            auto partial_backward = partial_backward,
            auto backward = backward,
            auto output_dim = output_dim,
            auto initializer = partial(initialize, Wout_init=Wout, bias_init=bias),
            auto buffers_initializer = initialize_buffers,
            auto name = name,
        )


#endif // RESERVOIRCPP_RIDGE_HPP
