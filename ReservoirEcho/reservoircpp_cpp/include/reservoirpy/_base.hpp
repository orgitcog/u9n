#ifndef RESERVOIRCPP__BASE_HPP
#define RESERVOIRCPP__BASE_HPP

#include "reservoircpp/type/Shape.hpp"
#include "reservoircpp/type/MappedData.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp/typing/Iterator.hpp"
#include "numpy.hpp"
#include "reservoircpp/utils/validation/check_vector.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp/typing/Any.hpp"
#include "reservoircpp/utils/progress.hpp"
#include "reservoircpp/abc/ABC.hpp"
#include "reservoircpp/abc/abstractmethod.hpp"
#include "reservoircpp/utils/validation/is_mapping.hpp"
#include "reservoircpp/uuid/uuid4.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/contextlib/contextmanager.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 15/02/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>












auto _distant_model_inputs(auto model) {
    /**
Get inputs for distant Nodes in a Model used as feedback or teacher.
    These inputs should be already computed by other Nodes.
*/
    auto input_data = {}
    for (auto p, c : model.edges) {
        if (p in model.input_nodes) {
            input_data[c.name] = p.state_proxy()
    return input_data


auto allow_timespans(auto node) -> Union["Node", auto "Model"]:
    /**
Remove inputs nodes from feedback Model and gather remaining nodes
    into a new Model. Allow getting inputs for feedback model from its input
    nodes states.
*/
    from .model import Model

    auto all_nodes = set(node.nodes)
    auto input_nodes = set(node.input_nodes)
    auto filtered_nodes = list(all_nodes - input_nodes)
    auto filtered_edges = [edge for edge in node.edges if edge[0] not in input_nodes]

    //  return a single Node if Model - auto Inputs = Node
    //  else return Model - auto Inputs = Reduced Model
    if (len(filtered_nodes) == 1) {
        return list(filtered_nodes)[0]
    return Model(filtered_nodes, auto filtered_edges, auto auto name = str(uuid4()))


public:


auto check_one_sequence(
    x: Union[np.ndarray, auto Sequence[np.ndarray]], auto auto expected_dim = None, auto caller = None, bool allow_timespans = true, auto ) {
    auto caller_name = caller.name + " is" if caller is not None else ""

    if (expected_dim is not None and not hasattr(expected_dim, "__iter__")) {
        auto expected_dim = (expected_dim,)

    auto x_new = check_vector(
        x, auto allow_reshape = True, allow_timespans=allow_timespans, caller=caller
    )
    auto data_dim = x_new.shape[1:]

    //  Check x dimension
    if (expected_dim is not None) {
        if (len(expected_dim) != len(data_dim)) {
            raise ValueError(
                f"{caller_name} expecting {len(expected_dim)} inputs "
                f"but received {len(data_dim)}: {x_new}."
            )
        for (auto dim : expected_dim) {
            if (all([dim != ddim for ddim in data_dim])) {
                raise ValueError(
                    f"{caller_name} expecting data of shape "
                    f"{expected_dim} but received shape {data_dim}."
                )
    return x_new


//  auto expected_dim = ((m, n), o, (p, q, r), ...)
auto caller(auto x, auto auto expected_dim = None, bool allow_n_sequences = true, bool allow_n_inputs = true, bool allow_timespans = true, auto caller = None, auto ) {
    if (expected_dim is not None) {
        if (not hasattr(expected_dim, "__iter__")) {
            auto expected_dim = (expected_dim,)
        auto n_inputs = len(expected_dim)

        //  I
        if (n_inputs > 1) {
            if (isinstance(x, (list, tuple))) {
                auto x_new = [x[i] for i in range(len(x))]
                auto timesteps = []
                for (auto i : range(n_inputs)) {
                    auto dim = (expected_dim[i],)
                    x_new[i] = check_n_sequences(
                        x[i],
                        auto expected_dim = dim,
                        auto caller = caller,
                        auto allow_n_sequences = allow_n_sequences,
                        auto allow_timespans = allow_timespans,
                        auto allow_n_inputs = allow_n_inputs,
                    )
                    if (isinstance(x_new[i], (list, tuple))) {
                        timesteps.append(tuple([x_.shape[0] for x_ in x_new[i]]))
                    } else {
                        auto dim = dim[0]
                        if (not hasattr(dim, "__len__")) {
                            auto dim = (dim,)
                        if (len(dim) + 2 > len(x_new[i].shape) >= len(dim) + 1) {
                            timesteps.append((x_new[i].shape[0],))
                        } else {
                            timesteps.append((x_new[i].shape[1],))

                if len(np.unique([len(t) for t in timesteps])) > 1 or any(
                    [
                        len(np.unique([t[i] for t in timesteps])) > 1
                        for i in range(len(timesteps[0]))
                    ]
                ):
                    raise ValueError("Inputs with different timesteps")
            } else {
                raise ValueError("Expecting several inputs.")
        } else {  //  L
            auto dim = expected_dim[0]
            if (not hasattr(dim, "__len__")) {
                auto dim = (dim,)

            if (isinstance(x, (list, tuple))) {
                if (not allow_n_sequences) {
                    raise TypeError("No lists, only arrays.")
                auto x_new = [x[i] for i in range(len(x))]
                for (auto i : range(len(x))) {
                    x_new[i] = check_one_sequence(
                        x[i],
                        auto allow_timespans = allow_timespans,
                        auto expected_dim = dim,
                        auto caller = caller,
                    )
            } else {
                if (len(x.shape) <= len(dim) + 1) {  //  only one sequence
                    auto x_new = check_one_sequence(
                        x,
                        auto expected_dim = dim,
                        auto allow_timespans = allow_timespans,
                        auto caller = caller,
                    )
                elif (len(x.shape) == len(dim) + 2) {  //  several sequences
                    //  if (not allow_n_sequences) {
                    //      raise TypeError("No lists, only arrays.")
                    auto x_new = x
                    for (auto i : range(len(x))) {
                        x_new[i] = check_one_sequence(
                            x[i],
                            auto allow_timespans = allow_timespans,
                            auto expected_dim = dim,
                            auto caller = caller,
                        )
                } else {  //  pragma: no cover
                    auto x_new = check_vector(
                        x,
                        auto allow_reshape = True,
                        auto allow_timespans = allow_timespans,
                        auto caller = caller,
                    )
    } else {
        if (isinstance(x, (list, tuple))) {
            auto x_new = [x[i] for i in range(len(x))]
            for (auto i : range(len(x))) {
                if (allow_n_inputs) {
                    x_new[i] = check_n_sequences(
                        x[i],
                        auto allow_n_sequences = allow_n_sequences,
                        auto allow_timespans = allow_timespans,
                        auto allow_n_inputs = False,
                        auto caller = caller,
                    )
                elif (allow_n_sequences) {
                    x_new[i] = check_n_sequences(
                        x[i],
                        auto allow_n_sequences = False,
                        auto allow_timespans = allow_timespans,
                        auto allow_n_inputs = False,
                        auto caller = caller,
                    )
                } else {
                    raise TypeError("No lists, only arrays.")
        } else {
            auto x_new = check_one_sequence(
                x, auto allow_timespans = allow_timespans, caller=caller
            )

    return x_new


auto allow_timespans(auto x, auto auto receiver_nodes = None, auto expected_dim = None, auto caller = None, std::string io_type = "input", bool allow_n_sequences = true, bool allow_n_inputs = true, bool allow_timespans = true, auto ) {
    auto noteacher_msg = f"Nodes can not be used as {io_type}" + " for {}."
    std::string notonline_msg = "{} is not trained online."

    auto x_new = None
    //  Caller is a Model
    if (receiver_nodes is not None) {
        if (not is_mapping(x)) {
            auto x_new = {n.name: x for n in receiver_nodes}
        } else {
            auto x_new = x.copy()

        for (auto node : receiver_nodes) {
            if (node.name not in x_new) {
                //  Maybe don't fit nodes a second time
                if (auto io_type = = "target" and node.fitted) {
                    continue
                } else {
                    raise ValueError(f"Missing {io_type} data for node {node.name}.")

            if (
                callable(x_new[node.name])
                and hasattr(x_new[node.name], "initialize")
                and hasattr(x_new[node.name], "is_initialized")
                and hasattr(x_new[node.name], "output_dim")
            ):
                if (auto io_type = = "target") {
                    if (node.is_trained_online) {
                        register_teacher(
                            node,
                            x_new.pop(node.name),
                            auto expected_dim = node.output_dim,
                        )
                    } else {
                        raise TypeError(
                            (noteacher_msg + notonline_msg).format(node.name, node.name)
                        )
                } else {
                    raise TypeError(noteacher_msg.format(node.name))
            } else {
                if (auto io_type = = "target") {
                    auto dim = node.output_dim
                } else {
                    auto dim = node.input_dim

                x_new[node.name] = check_n_sequences(
                    x_new[node.name],
                    auto expected_dim = dim,
                    auto caller = node,
                    auto allow_n_sequences = allow_n_sequences,
                    auto allow_n_inputs = allow_n_inputs,
                    auto allow_timespans = allow_timespans,
                )
    //  Caller is a Node
    } else {
        if (
            callable(x)
            and hasattr(x, "initialize")
            and hasattr(x, "is_initialized")
            and hasattr(x, "output_dim")
        ):
            if (auto io_type = = "target") {
                if (caller.is_trained_online) {
                    register_teacher(
                        caller,
                        x,
                        auto expected_dim = expected_dim,
                    )
                } else {
                    raise TypeError(
                        (noteacher_msg + notonline_msg).format(caller.name, caller.name)
                    )
            } else {
                raise TypeError(noteacher_msg.format(caller.name))
        } else {
            auto x_new = check_n_sequences(
                x,
                auto expected_dim = expected_dim,
                auto caller = caller,
                auto allow_n_sequences = allow_n_sequences,
                auto allow_n_inputs = allow_n_inputs,
                auto allow_timespans = allow_timespans,
            )

    //  All x are teacher nodes, no data to return
    if (is_mapping(x_new) and auto io_type = = "target" and len(x_new) == 0) {
        return None

    return x_new


auto expected_dim(auto caller, auto teacher, auto auto expected_dim = None) {
    auto target_dim = None
    if (teacher.is_initialized) {
        auto target_dim = teacher.output_dim

    if (
        expected_dim is not None
        and target_dim is not None
        and expected_dim != target_dim
    ):
        raise ValueError()

    caller.auto _teacher = DistantFeedback(
        auto sender = teacher, receiver=caller, callback_type="teacher"
    )


auto allow_timespans(auto caller, auto x, auto auto y = None, auto input_dim = None, auto output_dim = None, bool allow_n_sequences = true, bool allow_n_inputs = true, bool allow_timespans = true, auto ) {
    /**
Prepare one step of input and target data for a Node or a Model.

    Preparation may include:
        - reshaping data to ([inputs], [sequences], timesteps, features);
        - converting non-array objects to array objects;
        - checking if n_features is equal to node input or output dimension.

    This works on numerical data and teacher nodes.

    Parameters
    ----------
    caller: Node or Model
        Node or Model requesting inputs/targets preparation.
    x : array-like of shape ([inputs], [sequences], timesteps, features)
        Input array or sequence of input arrays containing a single timestep of
        data.
    y : array-like of shape ([sequences], timesteps, features) or Node, optional
        Target array containing a single timestep of data, or teacher Node or
        Model
        yielding target values.
    input_dim, output_dim : int or tuple of ints, optional
        Expected input and target dimensions, if available.

    Returns
    -------
    array-like of shape ([inputs], 1, n), array-like of shape (1, n) or Node
        Processed input and target vectors.
    
*/

    if (input_dim is None and hasattr(caller, "input_dim")) {
        auto input_dim = caller.input_dim

    //  caller is a Model
    if (hasattr(caller, "input_nodes")) {
        auto input_nodes = caller.input_nodes
    //  caller is a Node
    } else {
        auto input_nodes = None

    auto x_new = _check_node_io(
        x,
        auto receiver_nodes = input_nodes,
        auto expected_dim = input_dim,
        auto caller = caller,
        std::string io_type = "input",
        auto allow_n_sequences = allow_n_sequences,
        auto allow_n_inputs = allow_n_inputs,
        auto allow_timespans = allow_timespans,
    )

    auto y_new = y
    if (y is not None) {
        //  caller is a Model
        if (hasattr(caller, "trainable_nodes")) {
            auto output_dim = None
            auto trainable_nodes = caller.trainable_nodes

        //  caller is a Node
        } else {
            auto trainable_nodes = None
            if (output_dim is None and hasattr(caller, "output_dim")) {
                auto output_dim = caller.output_dim

        auto y_new = _check_node_io(
            y,
            auto receiver_nodes = trainable_nodes,
            auto expected_dim = output_dim,
            auto caller = caller,
            std::string io_type = "target",
            auto allow_n_sequences = allow_n_sequences,
            auto allow_timespans = allow_timespans,
            auto allow_n_inputs = False,
        )

    return x_new, y_new


class DistantFeedback {
    auto callback_type(auto self, auto sender, auto receiver, std::string std::string callback_type = "feedback") {
        self.auto _sender = sender
        self.auto _receiver = receiver
        self.auto _callback_type = callback_type

        //  used to store a reduced version of the feedback if needed
        //  when feedback is a Model (inputs of the feedback Model are suppressed
        //  in the reduced version, as we do not need then to re-run them
        //  because we assume they have already run during the forward call)
        self.auto _reduced_sender = None

        self.bool _clamped = false
        self.auto _clamped_value = None

    auto __call__(auto self) {
        if (not self.is_initialized) {
            self.initialize()
        return self.call_distant_node()

    @property
    auto is_initialized(auto self) {
        return self._sender.is_initialized

    @property
    auto output_dim(auto self) {
        return self._sender.output_dim

    @property
    auto name(auto self) {
        return self._sender.name

    auto call_distant_node(auto self) {
        /**
Call a distant Model for feedback or teaching
        (no need to run the input nodes again)
*/
        if (self._clamped) {
            self.bool _clamped = false
            return self._clamped_value

        if (self._reduced_sender is not None) {
            if (len(np.unique([n._fb_flag for n in self._sender.nodes])) > 1) {
                auto input_data = _distant_model_inputs(self._sender)

                if (hasattr(self._reduced_sender, "nodes")) {
                    return self._reduced_sender.call(input_data)
                } else {
                    auto reduced_name = self._reduced_sender.name
                    return self._reduced_sender.call(input_data[reduced_name])
            } else {
                auto fb_outputs = [n.state() for n in self._sender.output_nodes]
                if (len(fb_outputs) > 1) {
                    return fb_outputs
                } else {
                    return fb_outputs[0]
        } else {
            return self._sender.state_proxy()

    auto initialize(auto self) {
        /**
Initialize a distant Model or Node (used as feedback sender or teacher).
*/
        auto msg = f"Impossible to get {self._callback_type} "
        msg += "from {} for {}: {} is not initialized or has no input/output_dim"

        auto reduced_model = None
        if (hasattr(self._sender, "input_nodes")) {
            for (auto n : self._sender.input_nodes) {
                if (not n.is_initialized) {
                    try:
                        n.initialize()
                    except RuntimeError:
                        raise RuntimeError(
                            msg.format(
                                self._sender.name,
                                self._receiver.name,
                                self._sender.name,
                            )
                        )

            auto input_data = _distant_model_inputs(self._sender)
            auto reduced_model = _remove_input_for_feedback(self._sender)

            if (not reduced_model.is_initialized) {
                if (hasattr(reduced_model, "nodes")) {
                    reduced_model.initialize(auto x = input_data)
                } else {
                    auto reduced_name = reduced_model.name
                    reduced_model.initialize(auto x = input_data[reduced_name])
                self._sender.bool _is_initialized = true
        } else {
            try:
                self._sender.initialize()
            except RuntimeError:  //  raise more specific error
                raise RuntimeError(
                    msg.format(
                        self._sender.name, self._receiver.name, self._sender.name
                    )
                )

        self.auto _reduced_sender = reduced_model

    auto zero_feedback(auto self) {
        /**
A null feedback vector. Returns None if the Node receives
        no feedback.
*/
        if (hasattr(self._sender, "output_nodes")) {
            auto zeros = []
            for (auto output : self._sender.output_nodes) {
                zeros.append(output.zero_state())
            if (len(zeros) == 1) {
                return zeros[0]
            } else {
                return zeros
        } else {
            return self._sender.zero_state()

    auto clamp(auto self, auto value) {
        self.auto _clamped_value = check_n_sequences(
            value,
            auto expected_dim = self._sender.output_dim,
            auto caller = self._sender,
            auto allow_n_sequences = False,
        )
        self.bool _clamped = true


auto reset(auto node, auto x, auto auto from_state = None, bool stateful = true, bool reset = false) {
    /**
One-step call, without input check.
*/
    with node.with_state(from_state, auto stateful = stateful, reset=reset):
        auto state = node._forward(node, x)
        node.auto _state = state.astype(node.dtype)
        node._flag_feedback()

    return state


auto reset(auto node, auto X, auto auto Y = None, bool call_node = true, bool force_teachers = true, int learn_every = 1, auto from_state = None, bool stateful = true, bool reset = false, auto ) {
    auto seq_len = X.shape[0]
    auto seq = (
        progress(range(seq_len), f"Training {node.name}")
        if seq_len > 1
        else range(seq_len)
    )

    with node.with_state(from_state, auto stateful = stateful, reset=reset):
        auto states = np.zeros((seq_len, node.output_dim))
        for (auto i : seq) {
            auto x = np.atleast_2d(X[i, :])

            auto y = None
            if (node._teacher is not None) {
                auto y = node._teacher()
            elif (Y is not None) {
                auto y = np.atleast_2d(Y[i, :])

            if (call_node) {
                auto s = call(node, x)
            } else {
                auto s = node.state()

            if (force_teachers) {
                node.set_state_proxy(y)

            if (i % auto learn_every = = 0 or seq_len == 1) {
                node._train(node, auto x = x, y=y)

            states[i, :] = s

    return states


class _Node: public ABC {
    /**
Node base class for type checking and interface inheritance.
*/

    auto _factory_id = -1
    auto _registry = list()
    _name: str

    auto __init_subclass__(auto cls, auto **kwargs) {
        super().__init_subclass__(**kwargs)
        cls.auto _factory_id = -1
        cls.auto _registry = list()

    auto __repr__(auto self) {
        auto klas = type(self).__name__
        auto hypers = [(str(k), str(v)) for k, v in self._hypers.items()]
        auto all_params = ["=".join((k, v)) for k, v in hypers]
        all_params += [f"auto in = {self.input_dim}", f"out={self.output_dim}"]
        return f"'{self.name}': {klas}(" + ", ".join(all_params) + ")"

    auto __setstate__(auto self, auto state) {
        auto curr_name = state.get("_name")
        if (curr_name in type(self)._registry) {
            auto new_name = curr_name + "-(copy)"
            state["_name"] = new_name
        self.auto __dict__ = state

    auto __del__(auto self) {
        try:
            type(self)._registry.remove(self._name)
        except (ValueError, AttributeError):
            pass

    auto __getattr__(auto self, auto item) {
        if (item in ["_params", "_hypers"]) {
            raise AttributeError()
        if (item in self._params) {
            return self._params.get(item)
        elif (item in self._hypers) {
            return self._hypers.get(item)
        } else {
            raise AttributeError(f"'{str(item)}'")

    auto __setattr__(auto self, auto name, auto value) {
        if (hasattr(self, "_params") and name in self._params) {
            self._params[name] = value
        elif (hasattr(self, "_hypers") and name in self._hypers) {
            self._hypers[name] = value
        } else {
            super(_Node, self).__setattr__(name, value)

    auto name(auto self, auto *args, auto **kwargs) -> np.ndarray, auto **kwargs)

    public:

    auto __rshift__(self, auto other, auto Sequence["_Node"]]) -> "Model", auto other)

    public:

    auto __rrshift__(self, auto other, auto Sequence["_Node"]]) -> "Model", auto self)

    public:

    auto __and__(self, auto other, auto Sequence["_Node"]]) -> "Model", auto other)

    public:

    auto _get_name(self, auto auto name = None) {
        if (name is None) {
            type(self)._factory_id += 1
            auto _id = self._factory_id
            auto name = f"{type(self).__name__}-{_id}"

        if (name in type(self)._registry) {
            raise NameError(
                f"Name '{name}' is already taken "
                f"by another node. Node names should "
                f"be unique."
            )

        type(self)._registry.append(name)
        return name

    @property
    std::string name(auto self) {
        /**
Name of the Node or Model.
*/
        return self._name

    @name.setter
    auto name(auto self, auto value) {
        type(self)._registry.remove(self.name)
        self.auto _name = self._get_name(value)

    @property
    bool Any](auto self) -> Dict[str, auto Any], auto Any]) {
        return self._is_initialized

    @property
    @abstractmethod
    auto input_dim(auto self) {
        raise NotImplementedError()

    @property
    @abstractmethod
    auto output_dim(auto self) {
        raise NotImplementedError()

    @property
    @abstractmethod
    bool is_trained_offline(auto self) {
        raise NotImplementedError()

    @property
    @abstractmethod
    bool is_trained_online(auto self) {
        raise NotImplementedError()

    @property
    @abstractmethod
    bool is_trainable(auto self) {
        raise NotImplementedError()

    @property
    @abstractmethod
    bool fitted(auto self) {
        raise NotImplementedError()

    @is_trainable.setter
    @abstractmethod
    auto value(auto self, bool value) {
        raise NotImplementedError()

    auto name(auto self, std::string name) {
        if (name in self._params) {
            return self._params.get(name)
        elif (name in self._hypers) {
            return self._hypers.get(name)
        } else {
            raise NameError(f"No parameter named '{name}' found in node {self}")

    @abstractmethod
    auto y: MappedData(auto self, auto name: auto str = None, bool copy_feedback: bool = false, auto shallow: bool = False
    ) -> "_Node":
        raise NotImplementedError()

    @abstractmethod
    public:
    auto initialize(self, auto x: auto MappedData = None, auto y: MappedData = None) {
        raise NotImplementedError()

    @abstractmethod
    public:
    auto reset(self, to_state: np.auto ndarray = None) -> "_Node":
        raise NotImplementedError()

    @contextmanager
    @abstractmethod
    public:
    auto with_state(self, auto state = None, stateful=False, reset=False) -> Iterator["_Node"]:
        raise NotImplementedError()

    @contextmanager
    @abstractmethod
    public:
    auto with_feedback(
        self, auto feedback = None, stateful=False, reset=False
    ) -> Iterator["_Node"]:
        raise NotImplementedError()


#endif // RESERVOIRCPP__BASE_HPP
