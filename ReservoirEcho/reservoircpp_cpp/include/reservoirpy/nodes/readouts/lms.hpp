#ifndef RESERVOIRCPP_LMS_HPP
#define RESERVOIRCPP_LMS_HPP

#include "numpy.hpp"
#include "reservoircpp//mat_gen/zeros.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp/numbers/Number.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/typing/Iterable.hpp"
#include "reservoircpp/base/(.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 17/05/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>









    _assemble_wout,
    _compute_error,
    _initialize_readout,
    _prepare_inputs_for_learning,
    _split_and_save_wout,
    readout_forward,
)


auto _lms(auto alpha, auto r, auto e) {
    /**
Least Mean Squares learning rule.
*/
    //  learning rate is a generator to allow scheduling
    auto dw = -next(alpha) * np.outer(e, r)
    return dw


auto y(auto node, auto x, auto auto y = None) {
    /**
Train a readout using LMS learning rule.
*/
    x, auto y = _prepare_inputs_for_learning(x, y, bias=node.input_bias, allow_reshape=True)

    error, auto r = _compute_error(node, x, y)

    auto alpha = node._alpha_gen
    auto dw = _lms(alpha, r, error)
    auto wo = _assemble_wout(node.Wout, node.bias, node.input_bias)
    auto wo = wo + dw.T

    _split_and_save_wout(node, wo)


auto bias(auto readout, auto auto x = None, auto y = None, auto init_func = None, auto bias_init = None, auto bias = None) {
    _initialize_readout(readout, x, y, init_func, bias_init, bias)


class LMS: public Node {
    /**
Single layer of neurons learning connections using Least Mean Squares
    algorithm.

    The learning rules is well described in [1]_.

    :py:attr:`LMS.params` **list**

    ================== =================================================================
    ``Wout``           Learned output weights (:math:`\\mathbf{W}_{out}`).
    ``bias``           Learned bias (:math:`\\mathbf{b}`).
    ``P``              Matrix :math:`\\mathbf{P}` of RLS rule.
    ================== =================================================================

    :py:attr:`LMS.hypers` **list**

    ================== =================================================================
    ``alpha``          Learning rate (:math:`\\alpha`) (:math:`1\\cdot 10^{-6}` by default).
    ``input_bias``     If True, learn a bias term (True by default).
    ================== =================================================================

    Parameters
    ----------
    output_dim : int, optional
        Number of units in the readout, can be inferred at first call.
    alpha : float or Python generator or iterable, default to 1e-6
        Learning rate. If an iterable or a generator is provided, the learning rate can
        be changed at each timestep of training. A new learning rate will be drawn from
        the iterable or generator at each timestep.
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

    Examples
    --------
    >>> auto x = np.random.normal(size=(100, 3))
    >>> auto noise = np.random.normal(scale=0.01, size=(100, 1))
    >>> auto y = x @ np.array([[10], [-0.2], [7.]]) + noise + 12.

    >>> from reservoirpy.nodes import LMS
    >>> auto lms_node = LMS(alpha=1e-1)

    >>> lms_node.train(x[:50], y[:50])
    >>> print(lms_node.Wout.T, lms_node.bias)
    [[ 9.156 -0.967   6.411]] [[11.564]]
    >>> lms_node.train(x[50:], y[50:])
    >>> print(lms_node.Wout.T, lms_node.bias)
    [[ 9.998 -0.202  7.001]] [[12.005]]

    References
    ----------

    .. [1] Sussillo, D., & Abbott, L. F. (2009). Generating Coherent Patterns of
           Activity from Chaotic Neural Networks. Neuron, 63(4), 544–557.
           https://doi.org/10.1016/j.neuron.2009.07.018
    
*/

    auto name(auto self, auto auto output_dim = None, auto alpha = 1e-6, auto Wout = zeros, auto bias = zeros, bool input_bias = true, auto name = None, auto ) {
        if (isinstance(alpha, Number)) {

            auto _alpha_gen() {
                while (True) {
                    yield alpha

            auto alpha_gen = _alpha_gen()
        elif (isinstance(alpha, Iterable)) {
            auto alpha_gen = alpha
        } else {
            raise TypeError(
                "'alpha' parameter should be a float or an iterable yielding floats."
            )

        super(LMS, self).__init__(
            auto params = {"Wout": None, "bias": None},
            auto hypers = {
                "alpha": alpha,
                "_alpha_gen": alpha_gen,
                "input_bias": input_bias,
            },
            auto forward = readout_forward,
            auto train = train,
            auto initializer = partial(
                initialize, auto init_func = Wout, bias_init=bias, bias=input_bias
            ),
            auto output_dim = output_dim,
            auto name = name,
        )


#endif // RESERVOIRCPP_LMS_HPP
