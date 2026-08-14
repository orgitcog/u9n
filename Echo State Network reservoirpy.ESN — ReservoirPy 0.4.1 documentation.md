---
title: "Echo State Network reservoirpy.ESN — ReservoirPy 0.4.1 documentation"
source: "https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.ESN.html"
author:
published:
created: 2025-12-22
description:
tags:
  - "clippings"
---
## Echo State Network

*class* reservoirpy.ESN (

*reservoir:[Reservoir](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Reservoir.html#reservoirpy.nodes.Reservoir "reservoirpy.nodes.reservoir.Reservoir") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)") \= None*,

*readout:[Ridge](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Ridge.html#reservoirpy.nodes.Ridge "reservoirpy.nodes.ridge.Ridge") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)") \= None*,

*feedback:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)") \= False*,

*input\_to\_readout:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)") \= False*,

*return\_reservoir\_activity:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)") \= False*,

*\*\* kwargs*,

) [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/esn.py#L14-L127) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN "Link to this definition")

Simple Echo State Network.

This class is provided as a wrapper for a simple reservoir connected to a readout.

Parameters:

- **units** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *optional*) – Number of reservoir units. If None, the number of units will be inferred from the `W` matrix shape.
- **reservoir** ([*Node*](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")*,* *optional*) – A Node instance to use as a reservoir, such as a [`Reservoir`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Reservoir.html#reservoirpy.nodes.Reservoir "reservoirpy.nodes.Reservoir") node.
- **readout** ([*Node*](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")*,* *optional*) – A Node instance to use as a readout, such as a [`Ridge`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Ridge.html#reservoirpy.nodes.Ridge "reservoirpy.nodes.Ridge") node (only this one is supported).
- **feedback** ([*bool*](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")*,* *defaults to False*) – If True, the readout is connected to the reservoir through a feedback connection.
- **input\_to\_readout** ([*bool*](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")*,* *defaults to False*) – If True, the input is directly fed to the readout. See [Input-to-readout connections](https://reservoirpy.readthedocs.io/en/latest/user_guide/advanced_demo.html#Input-to-readout-connections).
- **return\_reservoir\_activity** ([*bool*](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")*,* *defaults to False*) – If True, the model outputs a dict with the reservoir activity in addition to the readout output.
- **\*\*kwargs** – Arguments passed to the reservoir and readout.

See also

`Reservoir`, `Ridge`

Example

```
>>> from reservoirpy import ESN
>>>
>>> model = ESN(units=100, sr=0.9, ridge=1e-6)  # reservoir and readout parameters at once
>>> model.fit(x_train, y_train)
>>>
>>> model = ESN(reservoir=Reservoir(100, sr=0.9), readout=Ridge(1e-5))  # passing nodes as parameters
>>>
>>> model = ESN(units=100, input_to_readout=True, feedback=True)  # more complex model
```

Methods

| (x\[, y\]) | Initializes a `Model` instance at runtime, using samples of data to infer all `Node` dimensions and instantiate the feedback buffers. |
| --- | --- |
| (\[x\]) | Call the Model function on a single step of data and update its state. |
| (\[x, iters, workers\]) | Run the Model on a sequence of data. |
| (\[x, iters, workers\]) | Alias for [`run()`](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model.run "reservoirpy.model.Model.run"). |
| (x\[, y, warmup, workers\]) | Offline fitting method of a Model. |
| (x\[, y\]) | Fit the Model in an online fashion. |

Attributes

|  | A [`Reservoir`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Reservoir.html#reservoirpy.nodes.Reservoir "reservoirpy.nodes.Reservoir") or a [`NVAR`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.NVAR.html#reservoirpy.nodes.NVAR "reservoirpy.nodes.NVAR") instance. |
| --- | --- |
|  | A [`Ridge`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Ridge.html#reservoirpy.nodes.Ridge "reservoirpy.nodes.Ridge") instance. |
|  | Is readout connected to reservoir through feedback (False by default). |
|  | Is the readout directly receiving the input (False by default). |
|  | Input node, if `input_to_readout` is set to True |
|  | List of Nodes contained in the model, in insertion order. |
|  | List of `(NodeA, d, NodeB)` edges, representing a connection from `NodeA` to `NodeB` with a delay of `d`. |
|  | List of Nodes that expects an input |
|  | List of Nodes expected to output their values |
|  | Dictionary that associates a name to a Node with that name in the model. |
|  | List of nodes that can be trained |
|  | List of Nodes contained in the model, in the order they should be run. |
|  | Dictionary that associates a list of all Nodes connected to the key Node. |
|  | Dictionary that associates a list of all Nodes to which the key node is connected. |
|  | If True, the model can be trained (with [`fit()`](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model.fit "reservoirpy.model.Model.fit")). |
|  | If True, the model has multiple Node inputs |
|  | If True, the model has multiple outputs and returns a dictionary that associates a node name to a node output. |
|  | If True, the model can be trained in parallel (offline). |
|  | If True, the model and its Nodes has been initialized. |

children*:[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node"),[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.children "Link to this definition")

Dictionary that associates a list of all Nodes to which the key node is connected.

edges*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[tuple](https://docs.python.org/3/library/stdtypes.html#tuple "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node"),[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)"),[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.edges "Link to this definition")

List of `(NodeA, d, NodeB)` edges, representing a connection from `NodeA` to `NodeB` with a delay of `d`.

execution\_order*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.execution_order "Link to this definition")

List of Nodes contained in the model, in the order they should be run.

Is readout connected to reservoir through feedback (False by default).

Dictionary of edges -> np.ndarray where arrays contain the values to be sent to the receiving node.

fit (

*x:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\]*,

*y:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] | None \= None*,

*warmup:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") \= 0*,

*workers:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") \= 1*,

) → [Model](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model "reservoirpy.model.Model") [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L519-L583) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.fit "Link to this definition")

Offline fitting method of a Model.

Parameters:

- **x** ([*list*](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") *or* *array-like* *of* *shape* *(**\[**series**,* *\]* *timesteps**,* *input\_dim**) or* *a mapping* *of* *input**,* *optional*) – Input sequences dataset.
- **y** ([*list*](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") *or* *array-like* *of* *shape* *(**\[**series**\]**,* *timesteps**,* *output\_dim**)**, or* *a mapping* *of* *input optional*) – Teacher signals dataset. If None, the method will try to fit the Node in an unsupervised way, if possible.
- **warmup** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *default to 0*) – Number of timesteps to consider as warmup and discard at the beginning of each timeseries before training.
- **workers** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")) –

Returns:

Node trained offline.

Return type:

[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")

initialize (

*x:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] | array(d) | dict\[str,array(d)\]*,

*y:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] | array(d) | dict\[str,array(d)\] | None \= None*,

) [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L180-L254) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.initialize "Link to this definition")

Initializes a `Model` instance at runtime, using samples of data to infer all `Node` dimensions and instantiate the feedback buffers.

Parameters:

- **x** ([*numpy.ndarray*](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)") *or* [*dict*](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") *of* [*numpy.ndarray*](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)")) – A vector of shape (1, ndim) corresponding to a timestep of data, or a dictionary mapping node names to vector of shapes (1, ndim of corresponding node).
- **y** ([*numpy.ndarray*](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)") *or* [*dict*](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") *of* [*numpy.ndarray*](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)")*,* *optional*) – A vector of shape (1, ndim) corresponding to a timestep of target data, or a dictionary mapping node names to vector of shapes (1, ndim of corresponding node).

initialized*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.initialized "Link to this definition")

If True, the model and its Nodes has been initialized.

input\_node*:[Input](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Input.html#reservoirpy.nodes.Input "reservoirpy.nodes.io.Input") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)")* *\= None* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.input_node "Link to this definition")

Input node, if `input_to_readout` is set to True

input\_to\_readout*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.input_to_readout "Link to this definition")

Is the readout directly receiving the input (False by default).

inputs*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.inputs "Link to this definition")

List of Nodes that expects an input

is\_multi\_input*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.is_multi_input "Link to this definition")

If True, the model has multiple Node inputs

is\_multi\_output*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.is_multi_output "Link to this definition")

If True, the model has multiple outputs and returns a dictionary that associates a node name to a node output.

is\_online*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.is_online "Link to this definition")

If True, the model can be trained online (with [`partial_fit()`](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model.partial_fit "reservoirpy.model.Model.partial_fit")).

is\_parallel*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.is_parallel "Link to this definition")

If True, the model can be trained in parallel (offline).

is\_trainable*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.is_trainable "Link to this definition")

If True, the model can be trained (with [`fit()`](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model.fit "reservoirpy.model.Model.fit")).

named\_nodes*:[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[str](https://docs.python.org/3/library/stdtypes.html#str "(in Python v3.13)"),[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.named_nodes "Link to this definition")

Dictionary that associates a name to a Node with that name in the model.

nodes*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.nodes "Link to this definition")

List of Nodes contained in the model, in insertion order.

output\_node*:[Output](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Output.html#reservoirpy.nodes.Output "reservoirpy.nodes.io.Output") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)")* *\= None* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.output_node "Link to this definition")

Output node for the reservoir, if `return_reservoir_activity` is set to True

outputs*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.outputs "Link to this definition")

List of Nodes expected to output their values

parents*:[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node"),[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.parents "Link to this definition")

Dictionary that associates a list of all Nodes connected to the key Node.

partial\_fit (

*x:array(t,d) | dict\[str,array(t,d)\]*,

*y:array(t,d) | dict\[str,array(t,d)\] | None \= None*,

) → array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L461-L517) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.partial_fit "Link to this definition")

Fit the Model in an online fashion.

This method both trains the Model parameters and produce predictions on the run. Calling updates the Model without resetting the parameters, unlike .

Parameters:

- **x** (*array-like* *of* *shape* *(**timesteps**,* *input\_dim**)*) – Input sequence of data.
- **y** (*array-like* *of* *shape* *(**timesteps**,* *output\_dim**)**,* *optional.*) – Target sequence of data. If None, the Node will train in an unsupervised way, if possible.

Returns:

All outputs computed during the training.

Return type:

array of shape (timesteps, output\_dim) or mapping of arrays

predict (

*x:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] | None \= None*,

*iters:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)") \= None*,

*workers:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") \= 1*,

) → array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L411-L430) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.predict "Link to this definition")

Alias for [`run()`](https://reservoirpy.readthedocs.io/en/latest/api/reservoirpy.model.html#reservoirpy.model.Model.run "reservoirpy.model.Model.run").

Parameters:

- **x** (*array* *(**\[**s**,**\]* *t**,* *d**)**,* [*list*](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") *of* *arrays* *(**t**,* *d**) or* *a mapping* *of* *them**,* *optional*) – A timeseries, multiple timeseries, or a mapping of timeseries or multiple timeseries. Input of the Model.
- **iters** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *optional*) – If `x` is `None`, a dimensionless timeseries of length `iters` is used instead.
- **workers** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *default to 1*) – Number of workers used for parallelization. If set to -1, all available workers (threads or processes) are used.

Returns:

A sequence of output vectors. If the model has multiple outputs, a mapping is returned instead.

Return type:

array of shape (\[n\_inputs,\] timesteps, output\_dim) or [list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") of arrays, or [dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)")

readout*:[TrainableNode](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.TrainableNode.html#reservoirpy.node.TrainableNode "reservoirpy.node.TrainableNode")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.readout "Link to this definition")

A [`Ridge`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Ridge.html#reservoirpy.nodes.Ridge "reservoirpy.nodes.Ridge") instance.

reservoir*:[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.reservoir "Link to this definition")

A [`Reservoir`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.Reservoir.html#reservoirpy.nodes.Reservoir "reservoirpy.nodes.Reservoir") or a [`NVAR`](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.nodes.NVAR.html#reservoirpy.nodes.NVAR "reservoirpy.nodes.NVAR") instance.

reset () → [tuple](https://docs.python.org/3/library/stdtypes.html#tuple "(in Python v3.13)") \[[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node"),[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[str](https://docs.python.org/3/library/stdtypes.html#str "(in Python v3.13)"),[ndarray](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)")\]\],[dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") \[[tuple](https://docs.python.org/3/library/stdtypes.html#tuple "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node"),[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)"),[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\],[ndarray](https://numpy.org/doc/stable/reference/generated/numpy.ndarray.html#numpy.ndarray "(in NumPy v2.3)") \[[tuple](https://docs.python.org/3/library/stdtypes.html#tuple "(in Python v3.13)") \[[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)"),[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")\],[dtype](https://numpy.org/doc/stable/reference/generated/numpy.dtype.html#numpy.dtype "(in NumPy v2.3)") \[[floating](https://numpy.org/doc/stable/reference/arrays.scalars.html#numpy.floating "(in NumPy v2.3)")\]\]\]\] [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L585-L596) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.reset "Link to this definition")

Reset all Node states and buffers in the Model.

Returns:

**dict\[str, np.array\], dict\[Edge, array\]**

Return type:

previous states of the Nodes and previous feedback buffer values.

return\_reservoir\_activity*:[bool](https://docs.python.org/3/library/functions.html#bool "(in Python v3.13)")* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.return_reservoir_activity "Link to this definition")

Are the reservoir states returned by the model along the readout output (False by default).

run (

*x:array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] | None \= None*,

*iters:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") | [None](https://docs.python.org/3/library/constants.html#None "(in Python v3.13)") \= None*,

*workers:[int](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)") \= 1*,

) → array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\] | dict\[str,array(t,d) | array(s,t,d) | ~typing.Sequence\[array(t,d)\]\] [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L355-L409) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.run "Link to this definition")

Run the Model on a sequence of data.

Parameters:

- **x** (*array* *(**\[**s**,**\]* *t**,* *d**)**,* [*list*](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") *of* *arrays* *(**t**,* *d**) or* *a mapping* *of* *them**,* *optional*) – A timeseries, multiple timeseries, or a mapping of timeseries or multiple timeseries. Input of the Model.
- **iters** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *optional*) – If `x` is `None`, a dimensionless timeseries of length `iters` is used instead.
- **workers** ([*int*](https://docs.python.org/3/library/functions.html#int "(in Python v3.13)")*,* *default to 1*) – Number of workers used for parallelization. If set to -1, all available workers (threads or processes) are used.

Returns:

A sequence of output vectors. If the model has multiple outputs, a mapping is returned instead.

Return type:

array of shape (\[n\_inputs,\] timesteps, output\_dim) or [list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") of arrays, or [dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)")

step (

*x:array(d) | dict\[str,array(d)\] | None \= None*,

) → array(d,) | dict\[str,array(d,)\] [\[source\]](https://github.com/reservoirpy/reservoirpy/blob/master/reservoirpy/model.py#L281-L317) [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.step "Link to this definition")

Call the Model function on a single step of data and update its state.

Parameters:

**x** (*array* *of* *shape* *(**input\_dim**,**)**,* *optional*) – One single step of input data. If None, an empty array is used instead and the Node is assumed to have an input\_dim of 0

Returns:

An output vector. If the model has multiple outputs, a dictionary is returned instead.

Return type:

array of shape (output\_dim,) or [dict](https://docs.python.org/3/library/stdtypes.html#dict "(in Python v3.13)") of 1D arrays.

trainable\_nodes*:[list](https://docs.python.org/3/library/stdtypes.html#list "(in Python v3.13)") \[[Node](https://reservoirpy.readthedocs.io/en/latest/api/generated/reservoirpy.node.Node.html#reservoirpy.node.Node "reservoirpy.node.Node")\]* [#](https://reservoirpy.readthedocs.io/en/latest/api/#reservoirpy.ESN.trainable_nodes "Link to this definition")

List of nodes that can be trained