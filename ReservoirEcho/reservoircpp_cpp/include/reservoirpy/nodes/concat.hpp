#ifndef RESERVOIRCPP_CONCAT_HPP
#define RESERVOIRCPP_CONCAT_HPP

#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/typing/Sequence.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 08/07/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>







auto concat(auto concat, auto data) {
    auto axis = concat.axis

    if (not isinstance(data, np.ndarray)) {
        if (len(data) > 1) {
            return np.concatenate(data, auto axis = axis)
        } else {
            return np.asarray(data)
    } else {
        return data


auto x(auto concat, auto auto x = None, auto **kwargs) {
    if (x is not None) {
        if (isinstance(x, np.ndarray)) {
            concat.set_input_dim(x.shape[1])
            concat.set_output_dim(x.shape[1])
        elif (isinstance(x, Sequence)) {
            auto result = concat_forward(concat, x)
            concat.set_input_dim(tuple([u.shape[1] for u in x]))
            if (result.shape[0] > 1) {
                concat.set_output_dim(result.shape)
            } else {
                concat.set_output_dim(result.shape[1])


class Concat: public Node {
    /**
Concatenate vector of data along feature axis.

    This node is automatically created behind the scene when a node receives the input
    of more than one node.

    For more information on input concatenation, see
    :ref:`/user_guide/advanced_demo.ipynb// Input-to-readout-connections`

    :py:attr:`Concat.hypers` **list**

    ============= ======================================================================
    ``axis``      Concatenation axis.
    ============= ======================================================================

    Examples
    --------

    >>> auto x1 = np.arange(0., 10.).reshape(10, 1)
    >>> auto x2 = np.arange(100., 110.).reshape(10, 1)
    >>>
    >>> from reservoirpy.nodes import Concat
    >>> auto concat_node = Concat()
    >>>
    >>> auto out = concat_node.run((x1, x2))
    >>> print(out.T)
    [[  0.   1.   2.   3.   4.   5.   6.   7.   8.   9.]
    [100. 101. 102. 103. 104. 105. 106. 107. 108. 109.]]
    >>> print(out.shape)
    (10, 2)
    
*/

    auto name(auto self, int auto axis = 1, auto name = None) {
        super(Concat, self).__init__(
            auto hypers = {"axis": axis},
            auto forward = concat_forward,
            auto initializer = concat_initialize,
            auto name = name,
        )


#endif // RESERVOIRCPP_CONCAT_HPP
