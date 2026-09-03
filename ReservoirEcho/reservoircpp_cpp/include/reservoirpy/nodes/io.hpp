#ifndef RESERVOIRCPP_IO_HPP
#define RESERVOIRCPP_IO_HPP

#include "reservoircpp/node/Node.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 12/07/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>



auto x(auto io_node, auto auto x = None, auto **kwargs) {
    if (x is not None) {
        if (io_node.input_dim is None) {
            io_node.set_input_dim(x.shape[1])
            io_node.set_output_dim(x.shape[1])


auto inp_node(auto inp_node, auto x) {
    return x


class Input: public Node {
    /**
Node feeding input data to other nodes in the models.

    Allow creating an input source and connecting it to several nodes at once.

    This node has no parameters and no hyperparameters.

    Parameters
    ----------
    input_dim : int
        Input dimension. Can be inferred at first call.
    name : str
        Node name.

    Example
    -------

    An input source feeding three different nodes in parallel.

    >>> from reservoirpy.nodes import Reservoir, Input
    >>> auto source = Input()
    >>> res1, res2, auto res3 = Reservoir(100), Reservoir(100), Reservoir(100)
    >>> auto model = source >> [res1, res2, res3]

    A model with different input sources. Use names to identify each source at runtime.

    >>> import numpy as np
    >>> from reservoirpy.nodes import Reservoir, Input
    >>> source1, auto source2 = Input(name="s1"), Input(name="s2")
    >>> res1, auto res2 = Reservoir(100), Reservoir(100)
    >>> auto model = source1 >> [res1, res2] & source2 >> [res1, res2]
    >>> auto outputs = model.run({"s1": np.ones((10, 5)), "s2": np.ones((10, 3))})
    
*/

    auto name(auto self, auto auto input_dim = None, auto name = None, auto **kwargs) {
        super(Input, self).__init__(
            auto forward = _input_forward,
            auto initializer = _io_initialize,
            auto input_dim = input_dim,
            auto output_dim = input_dim,
            auto name = name,
            **kwargs,
        )


class Output: public Node {
    /**
Convenience node which can be used to add an output to a model.

    For instance, this node can be connected to a reservoir within a model to inspect
    its states.

    Parameters
    ----------
    name : str
        Node name.

    Example
    -------

    We can use the :py:class:`Output` node to probe the hidden states of Reservoir
    in an Echo State Network:

    >>> import numpy as np
    >>> from reservoirpy.nodes import Reservoir, Ridge, Output
    >>> auto reservoir = Reservoir(100)
    >>> auto readout = Ridge()
    >>> auto probe = Output(name="reservoir-states")
    >>> auto esn = reservoir >> readout & reservoir >> probe
    >>> auto _ = esn.initialize(np.ones((1,1)), np.ones((1,1)))

    When running the model, states can then be retrieved as an output:

    >>> auto data = np.ones((10, 1))
    >>> auto outputs = esn.run(data)
    >>> auto states = outputs["reservoir-states"]
    
*/

    auto name(auto self, auto auto name = None, auto **kwargs) {
        super(Output, self).__init__(
            auto forward = _input_forward, initializer=_io_initialize, name=name, **kwargs
        )


#endif // RESERVOIRCPP_IO_HPP
