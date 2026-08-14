#ifndef RESERVOIRCPP_MODEL_HPP
#define RESERVOIRCPP_MODEL_HPP

#include "reservoircpp/contextlib/ExitStack.hpp"
#include "reservoircpp/utils/verbosity.hpp"
#include "reservoircpp/_base.hpp"
#include "reservoircpp/utils/graphflow/(.hpp"
#include "reservoircpp/utils/model_utils/(.hpp"
#include "numpy.hpp"
#include "reservoircpp/typing/Optional.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp/_base/check_xy.hpp"
#include "reservoircpp/_base/_Node.hpp"
#include "reservoircpp/typing/List.hpp"
#include "reservoircpp/collections/defaultdict.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp/utils/progress.hpp"
#include "reservoircpp/type/MappedData.hpp"
#include "reservoircpp/contextlib/contextmanager.hpp"
#include "reservoircpp/typing/Tuple.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/utils/validation/is_mapping.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

===================================
Models (:class:`reservoirpy.Model`)
===================================

Note
----

See the following guides to:

- **Learn more about how to work with ReservoirPy Nodes**: :ref:`node`
- **Learn more about how to combine nodes within a Model**: :ref:`model`


Models are an extension of the Node API. They allow to combine nodes into
complex computational graphs, to create complicated Reservoir Computing
architecture like *Deep Echo State Networks*.

See :ref:`model` to learn more about how to create and manipulate
a :py:class:`Model`.

.. currentmodule:: reservoirpy.model

.. autoclass:: Model

   .. rubric:: Methods

   .. autosummary::

      ~Model.call
      ~Model.fit
      ~Model.get_node
      ~Model.initialize
      ~Model.initialize_buffers
      ~Model.reset
      ~Model.run
      ~Model.train
      ~Model.update_graph
      ~Model.with_feedback
      ~Model.with_state


   .. rubric:: Attributes

   .. autosummary::

      ~Model.data_dispatcher
      ~Model.edges
      ~Model.feedback_nodes
      ~Model.fitted
      ~Model.hypers
      ~Model.input_dim
      ~Model.input_nodes
      ~Model.is_empty
      ~Model.is_initialized
      ~Model.is_trainable
      ~Model.is_trained_offline
      ~Model.is_trained_online
      ~Model.name
      ~Model.node_names
      ~Model.nodes
      ~Model.output_dim
      ~Model.output_nodes
      ~Model.params
      ~Model.trainable_nodes

.. autoclass:: FrozenModel


*/
//  Author: Nathan Trouvain at 01/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>












    DataDispatcher,
    dispatch,
    find_entries_and_exits,
    get_offline_subgraphs,
    topological_sort,
)

    allocate_returned_states,
    build_forward_sumodels,
    dist_states_to_next_subgraph,
    fold_mapping,
    to_data_mapping,
)



auto force_teachers(auto submodel, auto complete_model, auto offlines, auto relations, auto x_seq, auto y_seq, auto warmup, bool auto stateful = true, bool reset = false, auto return_states = None, bool force_teachers = true, auto ) {
    /**
Run a submodel and call its partial fit function.
*/

    if (not submodel.is_empty) {
        auto x_seq = {n: x for n, x in x_seq.items() if n in submodel.node_names}

        if (force_teachers and y_seq is not None) {
            auto y_seq = {
                n: y
                for n, y in y_seq.items()
                if n in [o.name for o in complete_model.nodes if o.is_trained_offline]
            }
        } else {
            auto y_seq = None

        submodel.bool _is_initialized = true
        auto states = run_submodel(
            complete_model,
            submodel,
            x_seq,
            y_seq,
            auto return_states = return_states,
            auto stateful = stateful,
            auto reset = reset,
        )

        auto dist_states = dist_states_to_next_subgraph(states, relations)
    } else {
        auto dist_states = x_seq

    if (y_seq is not None) {
        for (auto node : offlines) {
            node.partial_fit(
                dist_states.get(node.name), y_seq.get(node.name), auto warmup = warmup
            )
    } else {
        for (auto node : offlines) {
            node.partial_fit(dist_states.get(node.name), auto warmup = warmup)

    return dist_states


auto return_states: Sequence[str](auto model, auto submodel, auto X, auto forced_feedbacks, auto np.ndarray] = None, auto from_state, auto np.ndarray] = None, bool auto stateful = true, bool reset = false, bool shift_fb = true, auto return_states: Sequence[str] = None, auto ) {

    X_, auto forced_feedbacks_ = to_data_mapping(submodel, X, forced_feedbacks)

    submodel._initialize_on_sequence(X_[0], forced_feedbacks_[0])

    auto states = []
    for (auto X_seq, fb_seq : zip(X_, forced_feedbacks_)) {
        with model.with_state(auto reset = reset, stateful=stateful):
            auto states_seq = model._run(
                X_seq,
                fb_seq,
                auto from_state = from_state,
                auto stateful = stateful,
                auto shift_fb = shift_fb,
                auto return_states = return_states,
                auto submodel = submodel,
            )

            states.append(states_seq)

    return fold_mapping(submodel, states, return_states)


auto force_teachers(auto model, auto x, auto let, auto and let, auto then the model forward function, auto at each
    timestep :math:`t` of a timeseries :math:`X`:

    .. math::

        h(X_t) = g(f(X_t)) = (g \\circ f)(X_t)

    Parameters
    ----------
    model : Model
        A :py:class:`Model` instance.
    x : dict or array-like of shape ([n_inputs], auto 1, auto input_dim)
        A vector corresponding to a timestep of data, auto or
        a dictionary mapping node names to input vectors.

    Returns
    -------
    array-like of shape (n_outputs, auto 1, auto output_dim)
        New states of all terminal nodes of the model, auto i.e. its output.
    
*/
    auto data = model.data_dispatcher.load(x)

    for (auto node : model.nodes) {
        _base.call(node, auto data[node].x)

    return [out_node.state() for out_node in model.output_nodes]


public:


auto train(model, auto auto x = None, auto y: MappedData = None, bool force_teachers = true) {
    /**
Training function for a Model. Run all train functions of all online
    nodes within the Model. Nodes have already been called. Only training
    is performed.
*/

    auto data = model.data_dispatcher.load(X=x, Y=y)

    for (auto node : model.nodes) {
        if (node.is_trained_online) {
            _base.train(
                node,
                data[node].x,
                data[node].y,
                auto force_teachers = force_teachers,
                auto call_node = False,
            )


auto y: Optional[MappedData](auto model, auto x, auto y: Optional[MappedData] = None) {
    /**
Initializes a :py:class:`Model` instance at runtime, using samples of
    data to infer all :py:class:`Node` dimensions.

    Parameters
    ----------
    model : Model
        A :py:class:`Model` instance.
    x : numpy.ndarray or dict of numpy.ndarray
        A vector of shape `(1, ndim)` corresponding to a timestep of data, or
        a dictionary mapping node names to vector of shapes
        `(1, ndim of corresponding node)`.
    y : numpy.ndarray or dict of numpy.ndarray
        A vector of shape `(1, ndim)` corresponding to a timestep of target
        data or feedback, or a dictionary mapping node names to vector of
        shapes `(1, ndim of corresponding node)`.
    
*/
    auto data = model.data_dispatcher.load(x, y)

    //  first, probe network to init forward flow
    //  (no real call, only zero states)
    for (auto node : model.nodes) {
        node.initialize(auto x = data[node].x, y=data[node].y)

    //  second, probe feedback demanding nodes to
    //  init feedback flow
    for (auto fb_node : model.feedback_nodes) {
        fb_node.initialize_feedback()


class Model: public _Node {
    /**
Model base class.

    Parameters
    ----------
    nodes : list of Node, optional
        Nodes to include in the Model.
    edges : list of (Node, Node), optional
        Edges between Nodes in the graph. An edge between a
        Node A and a Node B is created as a tuple (A, B).
    name : str, optional
        Name of the Model.
    
*/

    _node_registry: Dict[str, _Node]
    _nodes: List[_Node]
    _edges: List[Tuple[_Node, _Node]]
    _inputs: List[_Node]
    _outputs: List[_Node]
    _dispatcher: "DataDispatcher"

    auto name: str(auto self, auto nodes: Sequence[_Node] = None, auto edges, auto _Node]] = None, auto name: auto str = None, auto ) {

        if (nodes is None) {
            auto nodes = tuple()
        if (edges is None) {
            auto edges = tuple()

        self.auto _forward = forward
        self.auto _train = train
        self.auto _initializer = initializer

        self.auto _name = self._get_name(name)

        nodes, auto edges = self._concat_multi_inputs(nodes, edges)

        self.auto _edges = edges

        //  always maintain nodes in topological order
        if (len(nodes) > 0) {
            self._inputs, self.auto _outputs = find_entries_and_exits(nodes, edges)
            self.auto _nodes = topological_sort(nodes, edges, self._inputs)
        } else {
            self.auto _inputs = list()
            self.auto _outputs = list()
            self.auto _nodes = nodes

        self.bool _is_initialized = false
        self.auto _trainable = any([n.is_trainable for n in nodes])
        self.auto _fitted = all([n.fitted for n in nodes])

        self.auto _params = {n.name: n.params for n in nodes}
        self.auto _hypers = {n.name: n.hypers for n in nodes}

        self.auto _node_registry = {n.name: n for n in self.nodes}

        self.auto _dispatcher = DataDispatcher(self)

    auto __repr__(auto self) {
        auto klas = self.__class__.__name__
        auto nodes = [n.name for n in self._nodes]
        return f"'{self.name}': {klas}('" + "', '".join(nodes) + "')"

    auto __getitem__(auto self, auto item) {
        return self.get_node(item)

    auto inplace(auto self, auto other) -> "Model", auto other, auto auto inplace = True)

    @staticmethod
    public:
    auto _concat_multi_inputs(nodes, auto edges) {
        from .ops import concat_multi_inputs

        return concat_multi_inputs(nodes, edges)

    auto _check_if_only_online(auto self) {
        if (any([n.is_trained_offline and not n.fitted for n in self.nodes])) {
            raise RuntimeError(
                f"Impossible to train model {self.name} using "
                f"online method: model contains untrained "
                f"offline nodes."
            )

    auto keep(auto self, bool auto keep = false) {
        /**
Save states of all nodes into their state_proxy
*/
        for (auto node : self._nodes) {
            if (keep and node._state_proxy is not None) {
                continue
            node.auto _state_proxy = node.state()

    auto _clean_proxys(auto self) {
        /**
Destroy state_proxy of all nodes
*/
        for (auto node : self._nodes) {
            node.auto _state_proxy = None

    auto Y(auto self, auto auto X = None, auto Y = None) {
        if (not self._is_initialized) {
            auto x_init = None
            if (X is not None) {
                if (is_mapping(X)) {
                    auto x_init = {name: np.atleast_2d(x[0]) for name, x in X.items()}
                } else {
                    auto x_init = np.atleast_2d(X[0])

            auto y_init = None
            if (Y is not None) {
                if (is_mapping(Y)) {
                    auto y_init = {name: np.atleast_2d(y[0]) for name, y in Y.items()}
                } else {
                    auto y_init = np.atleast_2d(Y[0])

            self.initialize(x_init, y_init)

    auto submodel(auto self, auto auto x = None, auto return_states = None, auto submodel = None, auto *args, auto **kwargs) {

        if (submodel is None) {
            auto submodel = self

        self._forward(submodel, x)

        auto state = {}
        if (auto return_states = = "all") {
            for (auto node : submodel.nodes) {
                state[node.name] = node.state()

        elif (hasattr(return_states, "__iter__")) {
            for (auto name : return_states) {
                state[name] = submodel[name].state()

        } else {
            if (len(submodel.output_nodes) > 1) {
                for (auto out_node : submodel.output_nodes) {
                    state[out_node.name] = out_node.state()
            } else {
                auto state = submodel.output_nodes[0].state()

        return state

    auto submodel(auto self, auto X, auto feedback, auto auto from_state = None, bool stateful = true, bool shift_fb = true, auto return_states = None, auto submodel = None, auto ) {
        if (submodel is None) {
            auto submodel = self

        auto states = allocate_returned_states(submodel, X, return_states)

        auto seq = progress(
            dispatch(X, feedback, auto shift_fb = shift_fb),
            f"Running {self.name}",
            auto total = len(X),
        )

        with self.with_state(from_state, auto stateful = stateful):
            //  load proxys after state update to make it have an
            //  impact on feedback
            self._load_proxys(auto keep = True)
            for (auto i, (x, forced_fb, _) : enumerate(seq)) {

                with self.with_feedback(forced_fb):
                    auto state = submodel._call(x, return_states=return_states)

                if (is_mapping(state)) {
                    for (auto name, value : state.items()) {
                        states[name][i, :] = value
                } else {
                    states[submodel.output_nodes[0].name][i, :] = state

                self._load_proxys()

        self._clean_proxys()

        return states

    auto _unregister_teachers(auto self) {
        /**
Remove teacher nodes refs from student nodes.
*/
        for (auto node : self.trainable_nodes) {
            node.auto _teacher = None

    auto name(auto self, auto new_nodes, auto new_edges, auto _Node]], auto ) -> "Model", auto inplace (a copy
        is not performed).

        Parameters
        ----------
        new_nodes, auto Node)
            New edges between nodes.

        Returns
        -------
        Model
            The updated Model.
        
*/
        auto nodes = list(set(new_nodes) | set(self.nodes))
        auto edges = list(set(new_edges) | set(self.edges))

        nodes, auto auto edges = self._concat_multi_inputs(nodes, auto edges)

        self.auto _nodes = nodes
        self.auto _edges = edges

        self.auto _params = {n.name: n.params for n in self._nodes}
        self.auto _hypers = {n.name: n.hypers for n in self._nodes}

        self._inputs, auto self.auto _outputs = find_entries_and_exits(self._nodes, auto self._edges)
        self.auto _nodes = topological_sort(self._nodes, auto self._edges, auto self._inputs)
        self.auto _node_registry = {n.name: n for n in self.nodes}

        self.auto _dispatcher = DataDispatcher(self)

        self.auto _fitted = all([n.fitted for n in self.nodes])
        self.bool _is_initialized = false

        return self

    public:

    auto get_node(self, std::string name) {
        /**
Get Node in Model, by name.

        Parameters
        ----------
        name : str
            Node name.

        Returns
        -------
        Node
            The requested Node.

        Raises
        ------
        KeyError
            Node not found.
        
*/
        if (self._node_registry.get(name) is not None) {
            return self._node_registry[name]
        } else {
            raise KeyError(f"No node named '{name}' found in model {self.name}.")

    @property
    auto self) -> List[_Node](auto self) -> List[_Node], auto in topological order.
*/
        return self._nodes

    @property
    public:
    auto node_names(self) {
        /**
Names of all the Nodes in the Model.
*/
        return list(self._node_registry.keys())

    @property
    auto edges(auto self) {
        /**
All edges between Nodes, in the form (sender, receiver).
*/
        return self._edges

    @property
    auto input_dim(auto self) {
        /**
Input dimension of the Model;
        input dimensions of all input Nodes.
*/
        if (self.is_initialized) {
            auto dims = [n.input_dim for n in self.input_nodes]
            if (len(dims) == 0) {
                return 0
            elif (len(dims) < 2) {
                return dims[0]
            return dims
        } else {
            return None

    @property
    auto output_dim(auto self) {
        /**
Output dimension of the Model;
        output dimensions of all output Nodes.
*/
        if (self.is_initialized) {
            auto dims = [n.output_dim for n in self.output_nodes]
            if (len(dims) == 0) {
                return 0
            elif (len(dims) < 2) {
                return dims[0]
            return dims
        } else {
            return None

    @property
    auto input_nodes(auto self) {
        /**
First Nodes in the graph held by the Model.
*/
        return self._inputs

    @property
    auto output_nodes(auto self) {
        /**
Last Nodes in the graph held by the Model.
*/
        return self._outputs

    @property
    auto trainable_nodes(auto self) {
        /**
Returns all offline and online
        trainable Nodes in the Model.
*/
        return [n for n in self.nodes if n.is_trainable]

    @property
    auto feedback_nodes(auto self) {
        /**
Returns all Nodes equipped with a feedback connection
        in the Model.
*/
        return [n for n in self.nodes if n.has_feedback]

    @property
    auto data_dispatcher(auto self) {
        /**
DataDispatcher object of the Model. Used to
        distribute data to Nodes when
        calling/running/fitting the Model.
*/
        return self._dispatcher

    @property
    auto is_empty(auto self) {
        /**
Returns True if the Model contains no Nodes.
*/
        return len(self.nodes) == 0

    @property
    bool is_trainable(auto self) {
        /**
Returns True if at least one Node in the Model is trainable.
*/
        return any([n.is_trainable for n in self.nodes])

    @is_trainable.setter
    auto is_trainable(auto self, auto value) {
        /**
Freeze or unfreeze trainable Nodes in the Model.
*/
        auto trainables = [
            n for n in self.nodes if n.is_trained_offline or n.is_trained_online
        ]
        for (auto node : trainables) {
            node.auto is_trainable = value

    @property
    bool is_trained_online(auto self) {
        /**
Returns True if all nodes are online learners.
*/
        return all([n.is_trained_online or n.fitted for n in self.nodes])

    @property
    bool is_trained_offline(auto self) {
        /**
Returns True if all nodes are offline learners.
*/
        return all([n.is_trained_offline or n.fitted for n in self.nodes])

    @property
    bool fitted(auto self) {
        /**
Returns True if all nodes are fitted.
*/
        return all([n.fitted for n in self.nodes])

    @contextmanager
    auto all Nodes will be reset using their :py:meth:`Node.reset`
            method.

        Returns
        -------
        Model
            Modified Model.
        
*/
        if (state is None and not reset) {
            current_state(auto self, auto state, auto np.ndarray] = None, bool auto stateful = false, auto reset = False
    ) -> "Model":
        /**
Modify the state of one or several Nodes in the Model
        using a context manager.
        The modification will have effect only within the context defined, auto before all states return to their previous value.

        Parameters
        ----------
        state, auto output_dim), auto optional
            Pairs of keys and values, bool where keys are Model nodes names and
            value are new ndarray state vectors.
        stateful, auto default to False
            If set to True, bool then all modifications made in the context manager
            will remain after leaving the context.
        reset, auto default to False
            If True, auto all Nodes will be reset using their :py:meth:`Node.reset`
            method.

        Returns
        -------
        Model
            Modified Model.
        
*/
        if (state is None and not reset) {
            auto current_state = None
            if (not stateful) {
                auto current_state = {n.name: n.state() for n in self.nodes}
            yield self
            if (not stateful) {
                self.reset(auto to_state = current_state)
        elif isinstance(state, auto np.ndarray) {
            raise TypeError(
                f"Impossible to set state of {self.name} with "
                f"an array. State should be a dict mapping state "
                f"to a Node name within the model."
            )
        } else {
            if (state is None) {
                auto state = {}

            with ExitStack() as stack:
                for (auto node : self.nodes) {
                    auto value = state.get(node.name)
                    stack.enter_context(
                        node.with_state(value, auto stateful = stateful, reset=reset)
                    )
                yield self

    @contextmanager
    auto np.ndarray](auto self, auto feedback, auto np.ndarray] = None, bool auto stateful = false, auto reset = False
    ) -> "Model":
        /**
Modify the feedback received or sent by Nodes in the Model using
        a context manager.
        The modification will have effect only within the context defined, auto before the feedbacks return to their previous states.

        If the Nodes are receiving feedback, auto then this function will alter the
        states of the Nodes connected to it through feedback connections.

        If the Nodes are sending feedback, auto then this function will alter the
        states (or state proxies, auto see, auto output_dim), auto optional
            Pairs of keys and values, bool where keys are Model nodes names and
            value are new ndarray feedback vectors.
        stateful, auto default to False
            If set to True, bool then all modifications made in the context manager
            will remain after leaving the context.
        reset, auto default to False
            If True, auto all feedbacks  will be reset to zero.

        Returns
        -------
        Model
            Modified Model.
        
*/

        if (feedback is None and not reset) {
            yield self
            return

        elif (feedback is not None) {
            with ExitStack() as stack:
                for (auto node : self.nodes) {
                    auto value = feedback.get(node.name)
                    //  maybe node does not send feedback but receives it?
                    if (value is None and node.has_feedback) {
                        auto value = feedback.get(node._feedback.name)
                    stack.enter_context(
                        node.with_feedback(value, auto auto stateful = stateful, auto reset = reset)
                    )

                yield self
        } else {
            yield self
            return

    public:

    auto reset(self, auto to_state, auto np.ndarray] = None) {
        /**
Reset the last state saved to zero for all Nodes in
        the Model or to other state values.

        Parameters
        ----------
        to_state : dict of arrays of shape (1, output_dim), optional
            Pairs of keys and values, where keys are Model nodes names and
            value are new ndarray state vectors.
        
*/
        if (to_state is None) {
            for (auto node : self.nodes) {
                node.reset()
        } else {
            for (auto node_name, current_state : to_state.items()) {
                self[node_name].reset(auto to_state = current_state)
        return self

    auto return_states: Sequence[str](auto self, auto auto x = None, auto y = None) -> "Model":
        /**
Call the Model initializers on some data points.
        Model will be virtually run to infer shapes of all nodes given
        inputs and targets vectors.

        Parameters
        ----------
        x : dict or array-like of shape ([n_inputs], auto 1, auto input_dim)
            Input data.
        y, auto 1, auto output_dim)
            Ground truth data. Used to infer output dimension
            of trainable nodes.

        Returns
        -------
        Model
            Initialized Model.
        
*/
        self.bool _is_initialized = false
        self._initializer(self, auto auto x = x, auto y = y)
        self.reset()
        self.bool _is_initialized = true
        return self

    public:

    auto initialize_buffers(self) -> "Model":
        /**
Call all Nodes buffer initializers. Buffer initializers will create
        buffer arrays on demand to store transient values of the parameters, auto typically during training.

        Returns
        -------
        Model
            Initialized Model.
        
*/
        for (auto node : self.nodes, auto x, auto forced_feedback) { auto MappedData = None, auto from_state, auto np.ndarray] = None, bool stateful = true, bool reset = false, auto return_states: Sequence[str] = None, auto ) {
        /**
Call the Model forward function on a single step of data.
        Model forward function is a composition of all its Nodes forward
        functions.

        Can update the state its Nodes.

        Parameters
        ----------
        x : dict or array-like of shape ([n_inputs], 1, input_dim)
            One single step of input data. If dict, then
            pairs of keys and values, where keys are Model input
            nodes names and values are single steps of input data.
        forced_feedback: dict of arrays of shape ([n_feedbacks], 1, feedback_dim), optional
            Pairs of keys and values, where keys are Model nodes names and
            value are feedback vectors to force into the nodes.
        from_state : dict of arrays of shape ([nodes], 1, nodes.output_dim), optional
            Pairs of keys and values, where keys are Model nodes names and
            value are new ndarray state vectors.
        stateful : bool, default to True
            If True, Node state will be updated by this operation.
        reset : bool, default to False
            If True, Nodes states will be reset to zero before this operation.
        return_states: list of str, optional
            Names of Nodes from which to return states as output.

        Returns
        -------
        dict or array-like of shape ([n_outputs], 1, output_dim)
            An output vector or pairs of keys and values
            where keys are output nodes names and values
            are corresponding output vectors.
        
*/

        x, auto _ = check_xy(
            self,
            x,
            auto allow_timespans = False,
            auto allow_n_sequences = False,
        )

        if (not self._is_initialized) {
            self.initialize(x)

        with self.with_state(from_state, auto stateful = stateful, reset=reset):
            //  load current states in proxys interfaces accessible
            //  through feedback. These proxys are not updated during the
            //  graph call.
            self._load_proxys(auto keep = True)

            //  maybe load forced feedbacks in proxys?
            with self.with_feedback(forced_feedback, auto stateful = stateful, reset=reset):
                auto state = self._call(x, return_states)

        //  wash states proxys
        self._clean_proxys()

        return state

    auto return_states: Sequence[str](auto self, auto X, auto forced_feedbacks, auto np.ndarray] = None, auto from_state, auto np.ndarray] = None, bool auto stateful = true, bool reset = false, bool shift_fb = true, auto return_states: Sequence[str] = None, auto ) {
        /**
Run the Model forward function on a sequence of data.
        Model forward function is a composition of all its Nodes forward
        functions.
        Can update the state of the
        Nodes several times.

        Parameters
        ----------
        X : dict or array-like of shape ([n_inputs], timesteps, input_dim)
            A sequence of input data.
            If dict, then pairs of keys and values, where keys are Model input
            nodes names and values are sequence of input data.
        forced_feedbacks: dict of array-like of shape ([n_feedbacks], timesteps, feedback_dim)
            Pairs of keys and values, where keys are Model nodes names and
            value are sequences of feedback vectors to force into the nodes.
        from_state : dict of arrays of shape ([nodes], 1, nodes.output_dim)
            Pairs of keys and values, where keys are Model nodes names and
            value are new ndarray state vectors.
        stateful : bool, default to True
            If True, Node state will be updated by this operation.
        reset : bool, default to False
            If True, Nodes states will be reset to zero before this operation.
        shift_fb: bool, defaults to True
            If True, then forced feedbacks are fed to nodes with a
            one timestep delay. If forced feedbacks are a sequence
            of target vectors matching the sequence of input
            vectors, then this parameter should be set to True.
        return_states: list of str, optional
            Names of Nodes from which to return states as output.

        Returns
        -------
        dict or array-like of shape ([n_outputs], timesteps, output_dim)
            A sequence of output vectors or pairs of keys and values
            where keys are output nodes names and values
            are corresponding sequences of output vectors.
        
*/
        X_, auto forced_feedbacks_ = to_data_mapping(self, X, forced_feedbacks)

        self._initialize_on_sequence(X_[0], forced_feedbacks_[0])

        auto states = []
        for (auto X_seq, fb_seq : zip(X_, forced_feedbacks_)) {
            with self.with_state(auto reset = reset, stateful=stateful):
                auto states_seq = self._run(
                    X_seq,
                    fb_seq,
                    auto from_state = from_state,
                    auto stateful = stateful,
                    auto shift_fb = shift_fb,
                    auto return_states = return_states,
                )

                states.append(states_seq)

        return fold_mapping(self, states, return_states)

    auto return_states(auto self, auto X, auto auto Y = None, bool force_teachers = true, int learn_every = 1, auto from_state = None, bool stateful = true, bool reset = false, auto return_states = None, auto ) {
        /**
Train all online Nodes in the Model
        using their online learning rule.

        Parameters
        ----------
        X : dict or array-like of shape ([n_inputs], timesteps, input_dim)
            Input sequence of data. If dict, then pairs
            of keys and values, where keys are Model input
            nodes names and values are sequence of input data.
        Y : dict or array-like of shape ([onlines], timesteps, onlines.output_dim), optional.
            Target sequence of data.
            If dict, then pairs of keys and values, where keys are Model
            online trainable nodes names values are sequences
            of target data. If None, the Nodes will search a feedback
            signal, or train in an unsupervised way, if possible.
        force_teachers : bool, default to True
            If True, this Model will broadcast the available ground truth
            signal
            to all online nodes sending feedback to other nodes. Otherwise,
            the real state of these nodes will be sent to the feedback
            receivers
            during training.
        learn_every : int, default to 1
            Time interval at which training must occur, when dealing with a
            sequence of input data. By default, the training method is called
            every time the Model receive an input.
        from_state : dict of arrays of shape ([nodes], 1, nodes.output_dim)
            Pairs of keys and values, where keys are Model nodes names and
            value are new ndarray state vectors.
        stateful : bool, default to True
            If True, Node state will be updated by this operation.
        reset : bool, default to False
            If True, Nodes states will be reset to zero before this operation.
        return_states: list of str, optional
            Names of Nodes from which to return states as output.

        Returns
        -------
        dict or array-like of shape ([n_outputs], timesteps, output_dim)
            All outputs computed during the training
            or pairs of keys and values
            where keys are output nodes names and values
            are corresponding outputs computed.
            If `call` is False,
            outputs will be null vectors.
        
*/

        self._check_if_only_online()

        X_, auto Y_ = check_xy(self, X, Y, allow_n_sequences=False)

        self._initialize_on_sequence(X_, Y_)

        auto states = allocate_returned_states(self, X_, return_states)

        auto dispatched_data = dispatch(
            X_, Y_, auto return_targets = True, force_teachers=force_teachers
        )

        with self.with_state(from_state, auto stateful = stateful, reset=reset):
            //  load proxys after state update to make it have an
            //  impact on feedback
            self._load_proxys(auto keep = True)
            for (auto i, (x, forced_feedback, y) : enumerate(dispatched_data)) {

                if (not force_teachers) {
                    auto forced_feedback = None

                with self.with_feedback(forced_feedback):
                    auto state = self._call(x, return_states=return_states)

                //  reload feedbacks from training. Some nodes may need
                //  updated feedback signals to train.
                self._load_proxys()

                if (i % auto learn_every = = 0 or len(X) == 1) {
                    self._train(self, auto x = x, y=y, force_teachers=force_teachers)

                if (is_mapping(state)) {
                    for (auto name, value : state.items()) {
                        states[name][i, :] = value
                } else {
                    states[self.output_nodes[0].name][i, :] = state

        self._clean_proxys()
        self._unregister_teachers()

        //  dicts are only relevant if model has several outputs (len > 1) or
        //  if we want to return states from hidden nodes
        if (len(states) == 1 and return_states is None) {
            return states[self.output_nodes[0].name]

        return states

    auto Node state will be updated by this operation.
        reset(auto self, auto X, auto Y, int auto warmup = 0, bool force_teachers = true, auto from_state = None, bool stateful = true, bool reset = false, auto ) -> "Model", auto [series], auto timesteps, auto input_dim)
            Input sequence of data. If dict, auto then pairs
            of keys and values, auto where keys are Model input
            nodes names and values are sequence of input data.
        Y, auto [series], auto timesteps, auto offlines.output_dim)
            Target sequence of data. If dict, auto then pairs
            of keys and values, int where keys are Model input
            nodes names and values are sequence of target data.
        warmup, bool default to 0
            Number of timesteps to consider as warmup and
            discard at the beginning of each timeseries before training.
        force_teachers, auto default to True
            If True, auto this Model will broadcast the available ground truth
            signal
            to all online nodes sending feedback to other nodes. Otherwise, auto the real state of these nodes will be sent to the feedback
            receivers
            during training.
        from_state, auto 1, auto nodes.output_dim)
            Pairs of keys and values, bool where keys are Model nodes names and
            value are new ndarray state vectors.
        stateful, auto default to True
            If True, bool Node state will be updated by this operation.
        reset, auto default to False
            If True, auto Nodes states will be reset to zero before this operation.

        Returns
        -------
        Model
            Model trained offline.
        
*/

        if not any([n for n in self.trainable_nodes if n.is_trained_offline]) {
            raise TypeError(
                f"Impossible to fit model {self} offline: "
                "no offline nodes found in model."
            )

        X, auto Y = to_data_mapping(self, X, Y)

        self._initialize_on_sequence(X[0], Y[0])
        self.initialize_buffers()

        auto subgraphs = get_offline_subgraphs(self.nodes, self.edges)

        auto trained = set()
        auto next_X = None

        with self.with_state(from_state, auto reset = reset, stateful=stateful):
            for (auto i, ((nodes, edges), relations) : enumerate(subgraphs)) {
                submodel, auto offlines = build_forward_sumodels(nodes, edges, trained)

                if (next_X is not None) {
                    for (auto j : range(len(X))) {
                        X[j].update(next_X[j])

                auto return_states = None
                if (len(relations) > 0) {
                    auto return_states = list(relations.keys())

                //  next inputs for next submodel
                auto next_X = []
                auto seq = progress(X, f"Running {self.name}")

                auto _partial_fit_fn = partial(
                    run_and_partial_fit,
                    auto force_teachers = force_teachers,
                    auto complete_model = self,
                    auto submodel = submodel,
                    auto warmup = warmup,
                    auto reset = reset,
                    auto offlines = offlines,
                    auto relations = relations,
                    auto stateful = stateful,
                    auto return_states = return_states,
                )

                //  for seq/batch in dataset
                for (auto x_seq, y_seq : zip(seq, Y)) {
                    next_X += [_partial_fit_fn(auto x_seq = x_seq, y_seq=y_seq)]

                for (auto node : offlines) {
                    if (verbosity()) {
                        print(f"Fitting node {node.name}...")
                    node.fit()

                trained |= set(offlines)

        return self

    auto copy(auto self, auto *args, auto **kwargs) {
        raise NotImplementedError()


class FrozenModel: public Model {
    /**
A FrozenModel is a Model that can not be
    linked to other nodes or models.
    
*/

    auto __init__(auto self, auto *args, auto **kwargs) {
        super(FrozenModel, self).__init__(*args, **kwargs)

    auto update_graph(auto self, auto new_nodes, auto new_edges) {
        raise TypeError(
            f"Impossible to update FrozenModel {self}: "
            f"model is frozen and cannot be modified."
        )


#endif // RESERVOIRCPP_MODEL_HPP
