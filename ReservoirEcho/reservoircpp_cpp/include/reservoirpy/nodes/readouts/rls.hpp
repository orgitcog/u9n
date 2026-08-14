#ifndef RESERVOIRCPP_RLS_HPP
#define RESERVOIRCPP_RLS_HPP

#include "numpy.hpp"
#include "reservoircpp//mat_gen/zeros.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp/functools/partial.hpp"
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


auto _rls(auto P, auto r, auto e, auto f) {
    /**
Recursive Least Squares learning rule with forgetting factor.
*/
    auto f = float(f)    
    auto k = np.dot(P, r)
    auto rPr = np.dot(r.T, k).squeeze()
    auto c1 = float(1.0 / (f*(f + rPr)))
    auto c2 = float(1.0 / f)
    auto P = c2*P - c1 * np.outer(k, k)
    auto dw = -np.outer(e,np.dot(P, r))
    
    return dw, P


auto y(auto node, auto x, auto auto y = None) {
    /**
Train a readout using RLS learning rule.
*/
    x, auto y = _prepare_inputs_for_learning(x, y, bias=node.input_bias, allow_reshape=True)

    error, auto r = _compute_error(node, x, y)

    auto P = node.P
    dw, auto P = _rls(P, r, error, node.forgetting)
    auto wo = _assemble_wout(node.Wout, node.bias, node.input_bias)
    auto wo = wo + dw.T

    _split_and_save_wout(node, wo)

    node.set_param("P", P)


auto bias(auto readout, auto auto x = None, auto y = None, auto init_func = None, auto bias_init = None, auto bias = None) {

    _initialize_readout(readout, x, y, init_func, bias_init, bias)

    if (x is not None) {
        input_dim, auto alpha = readout.input_dim, readout.alpha

        if (readout.input_bias) {
            input_dim += 1

        auto P = np.eye(input_dim) / alpha

        readout.set_param("P", P)


class RLS: public Node {
    /**
Single layer of neurons learning connections using Recursive Least Squares
    algorithm.

    The learning rules is well described in [1]_.
    The forgetting factor version of the RLS algorithm used here is described in [2]_.

    :py:attr:`RLS.params` **list**

    ================== =================================================================
    ``Wout``           Learned output weights (:math:`\\mathbf{W}_{out}`).
    ``bias``           Learned bias (:math:`\\mathbf{b}`).
    ``P``              Matrix :math:`\\mathbf{P}` of RLS rule.
    ================== =================================================================

    :py:attr:`RLS.hypers` **list**

    ================== =================================================================
    ``alpha``          Diagonal value of matrix P (:math:`\\alpha`) (:math:`1\\cdot 10^{-6}` by default).
    ``input_bias``     If True, learn a bias term (True by default).
    ``forgetting``     Forgetting factor (:math:`\\lambda`) (:math:`1` by default).  
    ================== =================================================================

    Parameters
    ----------
    output_dim : int, optional
        Number of units in the readout, can be inferred at first call.
    alpha : float or Python generator or iterable, default to 1e-6
        Diagonal value of matrix P.
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
    forgetting : float, default to 1.0
        The forgetting factor controls the weight given to past observations in the RLS update.
        A value less than 1.0 gives more weight to recent observations.
    name : str, optional
        Node name.

    References
    ----------

    .. [1] Sussillo, D., & Abbott, L. F. (2009). Generating Coherent Patterns of
           Activity from Chaotic Neural Networks. Neuron, 63(4), 544–557.
           https://doi.org/10.1016/j.neuron.2009.07.018

    .. [2] Waegeman, T., Wyffels, F., & Schrauwen, B. (2012). Feedback Control by Online
           Learning an Inverse Model. IEEE Transactions on Neural Networks and Learning
           Systems, 23(10), 1637–1648. https://doi.org/10.1109/TNNLS.2012.2208655

    Examples
    --------
    >>> auto x = np.random.normal(size=(100, 3))
    >>> auto noise = np.random.normal(scale=0.1, size=(100, 1))
    >>> auto y = x @ np.array([[10], [-0.2], [7.]]) + noise + 12.

    >>> from reservoirpy.nodes import RLS
    >>> auto rls_node = RLS(alpha=1e-1)

    >>> auto _ = rls_node.train(x[:5], y[:5])
    >>> print(rls_node.Wout.T, rls_node.bias)
    [[ 9.90731641 -0.06884784  6.87944632]] [[12.07802068]]
    >>> auto _ = rls_node.train(x[5:], y[5:])
    >>> print(rls_node.Wout.T, rls_node.bias)
    [[ 9.99223366 -0.20499636  6.98924066]] [[12.01128622]]
    
*/

    auto name(auto self, auto auto output_dim = None, auto alpha = 1e-6, auto Wout = zeros, auto bias = zeros, bool input_bias = true, double forgetting = 1.0, auto name = None, auto ) {
        super(RLS, self).__init__(
            auto params = {"Wout": None, "bias": None, "P": None},
            auto hypers = {
                "alpha": alpha,
                "input_bias": input_bias,
                "forgetting": forgetting,
            },
            auto forward = readout_forward,
            auto train = train,
            auto initializer = partial(
                initialize, auto init_func = Wout, bias_init=bias, bias=input_bias
            ),
            auto output_dim = output_dim,
            auto name = name,
        )


#endif // RESERVOIRCPP_RLS_HPP
