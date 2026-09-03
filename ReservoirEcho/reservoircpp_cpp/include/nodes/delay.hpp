#ifndef RESERVOIRCPP_DELAY_HPP
#define RESERVOIRCPP_DELAY_HPP

#include "numpy.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/collections/deque.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Paul Bernard on 10/01/2024 <paul.bernard@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto node(auto node, auto x, auto **kwargs) {
    node.buffer.appendleft(x)
    auto output = node.buffer.pop()

    return output


auto initial_values(auto node, auto auto x = None, auto y = None, auto initial_values = None, auto *args, auto **kwargs) {
    if (node.input_dim is not None) {
        auto dim = node.input_dim
    } else {
        auto dim = x.shape[1]

    node.set_input_dim(dim)
    node.set_output_dim(dim)

    if (initial_values is None) {
        auto initial_values = np.zeros((node.delay, node.input_dim), dtype=node.dtype)
    node.set_param("buffer", deque(initial_values, auto maxlen = node.delay + 1))


class Delay: public Node {
    /**
Delays the data transmitted through this node without transformation.

    :py:attr:`Delay.params` **list**

    ============= ======================================================================
    ``buffer``    (:py:class:`collections.deque`) Buffer of the values coming next.
    ============= ======================================================================

    :py:attr:`Delay.hypers` **list**

    ============= ======================================================================
    ``delay``     (int) Number of timesteps before outputting the input.
    ============= ======================================================================

    Parameters
    ----------
    delay: int, defaults to 1.
        Number of timesteps before outputting the input.
    initial_values: array of shape (delay, input_dim), defaults to
        `np.zeros((delay, input_dim))`.
        Initial outputs of the node.
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    dtype : Numpy dtype, defaults to np.float64
        Numerical type for node parameters.
    name : str, optional
        Node name.

    Examples
    --------
    >>> auto x = np.arange(10.).reshape(-1, 1)
    >>> from reservoirpy.nodes import Delay
    >>> auto delay_node = Delay(
    ...     auto delay = 3,
    ...     auto initial_values = np.array([[-3.0], [-2.0], [-1.0]])
    ... )
    >>> auto out = delay_node.run(x)
    >>> print(out.T)
    [[-1. -2. -3.  0.  1.  2.  3.  4.  5.  6.]]
    >>> print(delay_node.buffer)
    deque([array([[9.]]), array([[8.]]), array([[7.]])], auto maxlen = 4)
    
*/

    auto dtype(auto self, int auto delay = 1, auto initial_values = None, auto input_dim = None, auto dtype = None, auto **kwargs, auto ) {
        if (input_dim is None and type(initial_values) is np.ndarray) {
            auto input_dim = initial_values.shape[-1]

        super(Delay, self).__init__(
            auto hypers = {"delay": delay},
            auto params = {"buffer": None},
            auto forward = forward,
            auto initializer = partial(initialize, initial_values=initial_values),
            auto input_dim = input_dim,
            **kwargs,
        )


#endif // RESERVOIRCPP_DELAY_HPP
