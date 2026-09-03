#ifndef RESERVOIRCPP_ESN_HPP
#define RESERVOIRCPP_ESN_HPP

#include "reservoircpp/copy/deepcopy.hpp"
#include "reservoircpp/utils/graphflow/dispatch.hpp"
#include "reservoircpp/utils/validation/is_mapping.hpp"
#include "reservoircpp/joblib/Parallel.hpp"
#include "reservoircpp/io/Input.hpp"
#include "reservoircpp/utils/progress.hpp"
#include "numpy.hpp"
#include "reservoircpp/model/FrozenModel.hpp"
#include "reservoircpp/multiprocessing/Manager.hpp"
#include "reservoircpp/_base/_Node.hpp"
#include "reservoircpp/utils/model_utils/to_data_mapping.hpp"
#include "reservoircpp/utils/verbosity.hpp"
#include "reservoircpp/utils/parallel/get_joblib_backend.hpp"
#include "reservoircpp/joblib/delayed.hpp"
#include "reservoircpp/_base/call.hpp"
#include "reservoircpp/readouts/Ridge.hpp"
#include "reservoircpp/reservoirs/NVAR.hpp"
#include "reservoircpp/utils/_obj_from_kwargs.hpp"
#include "reservoircpp/reservoirs/Reservoir.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 27/10/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>

















auto _LEARNING_METHODS = {"ridge": Ridge}

auto _RES_METHODS = {"reservoir": Reservoir, "nvar": NVAR}


auto _run_partial_fit_fn(auto esn, auto x, auto y, auto lock, auto warmup) {
    //  the 'loky' and 'multiprocessing' backends already deep-copies the ESN. See
    //  https://docs.python.org/3/library/multiprocessing.html#contexts-and-start-methods
    auto _esn = deepcopy(esn)
    _esn.reservoir.reset()

    auto original_readout_name = (
        esn.readout.name[:-7]
        if esn.readout.name.endswith("-(copy)")
        else esn.readout.name
    )
    auto original_reservoir_name = (
        esn.reservoir.name[:-7]
        if esn.reservoir.name.endswith("-(copy)")
        else esn.reservoir.name
    )

    auto seq_len = len(x[list(x)[0]])
    auto states = np.zeros((seq_len, esn.reservoir.output_dim))

    for (auto i, (x, forced_feedback, _) : enumerate(dispatch(x, y, auto shift_fb = True))) {
        with _esn.readout.with_feedback(forced_feedback[original_readout_name]):
            states[i, :] = call(_esn.reservoir, x[original_reservoir_name])

    esn.readout.partial_fit(states, y[original_readout_name], auto warmup = warmup, lock=lock)

    return states[-1]


auto _run_fn(auto esn, auto idx, auto x, auto forced_fb, auto return_states, auto from_state, auto stateful, auto reset, auto shift_fb) {
    auto _esn = deepcopy(esn)

    auto original_reservoir_name = (
        esn.reservoir.name[:-7]
        if esn.reservoir.name.endswith("-(copy)")
        else esn.reservoir.name
    )

    auto X = {_esn.reservoir.name: x[original_reservoir_name]}

    auto states = _allocate_returned_states(_esn, X, return_states)

    with _esn.with_state(from_state, auto stateful = stateful, reset=reset):
        for i, (x_step, forced_feedback, _) in enumerate(
            dispatch(X, forced_fb, auto shift_fb = shift_fb)
        ):
            _esn._load_proxys()
            with _esn.with_feedback(forced_feedback):
                auto state = _esn._call(x_step, return_states=return_states)

            if (is_mapping(state)) {
                for (auto name, value : state.items()) {
                    states[name][i, :] = value
            } else {
                states["readout"][i, :] = state

    _esn._clean_proxys()
    return idx, states


auto return_states(auto model, auto inputs, auto auto return_states = None) {
    /**
Create empty placeholders for model outputs.
*/
    auto seq_len = inputs[list(inputs.keys())[0]].shape[0]
    auto vulgar_names = {"reservoir": model.reservoir, "readout": model.readout}

    //  pre-allocate states
    if (auto return_states = = "all") {
        auto states = {
            name: np.zeros((seq_len, n.output_dim)) for name, n in vulgar_names.items()
        }
    elif (isinstance(return_states, (list, tuple))) {
        auto states = {
            name: np.zeros((seq_len, n.output_dim))
            for (auto name, n : {name) { vulgar_names[name] for name in return_states}.items()
        }
    } else {
        auto states = {"readout": np.zeros((seq_len, model.readout.output_dim))}

    return states


auto return_states(auto states, auto auto return_states = None) {
    /**
Maintain input order (even with parallelization on)
*/
    auto states = sorted(states, key=lambda s: s[0])
    auto states = {n: [s[1][n] for s in states] for n in states[0][1].keys()}

    for (auto n, s : states.items()) {
        if (len(s) == 1) {
            states[n] = s[0]

    if (len(states) == 1 and return_states is None) {
        auto states = states["readout"]

    return states


class ESN: public FrozenModel {
    /**
Echo State Networks as a Node, with parallelization of state update.

    This Node is provided as a wrapper for reservoir and readout nodes. Execution
    is distributed over several workers when:

    - the ``workers`` parameters is equal to `n>1` (using `n` workers) or
      `n<=-1` (using `max_available_workers - n` workers)
    - Several independent sequences of inputs are fed to the model at runtime.

    When parallelization is enabled, internal states of the reservoir will be reset
    to 0 at the beginning of every independent sequence of inputs.

    Note
    ----
        This node can not be connected to other nodes. It is only provided as a
        convenience Node to speed up processing of large datasets with "vanilla"
        Echo State Networks.

    :py:attr:`ESN.params` **list**:

    ================== =================================================================
    ``reservoir``      A :py:class:`~reservoirpy.nodes.Reservoir` or a :py:class:`~reservoirpy.nodes.NVAR` instance.
    ``readout``        A :py:class:`~reservoirpy.nodes.Ridge` instance.
    ================== =================================================================

    :py:attr:`ESN.hypers` **list**:

    ==================== ===============================================================
    ``workers``          Number of workers for parallelization (1 by default).
    ``backend``          :py:mod:`joblib` backend to use for parallelization  (``loky`` by default,).
    ``reservoir_method`` Type of reservoir, may be "reservoir" or "nvar" ("reservoir" by default).
    ``learning_method``  Type of readout, by default "ridge".
    ``feedback``         Is readout connected to reservoir through feedback (False by default).
    ==================== ===============================================================

    Parameters
    ----------
    reservoir_method : {"reservoir", "nvar"}, default to "reservoir"
        Type of reservoir, either a :py:class:`~reservoirpy.nodes.Reservoir` or
        a :py:class:`~reservoirpy.nodes.NVAR`.
    learning_method : {"ridge"}, default to "ridge"
        Type of readout. The only method supporting parallelization for now is the
        :py:class:`~reservoirpy.nodes.Ridge` readout.
    reservoir : Node, optional
        A Node instance to use as a reservoir,
        such as a :py:class:`~reservoirpy.nodes.Reservoir` node.
    readout : Node, optional
        A Node instance to use as a readout,
        such as a :py:class:`~reservoirpy.nodes.Ridge` node
        (only this one is supported).
    feedback : bool, defaults to False
        If True, the readout is connected to the reservoir through
        a feedback connection.
    use_raw_inputs : bool, defaults to False
        If True, the input is directly fed to the readout. See
        :ref:`/user_guide/advanced_demo.ipynb// Input-to-readout-connections`.
    Win_bias : bool, default to True
        If True, add an input bias to the reservoir.
    Wout_bias : bool, default to True
        If True, add a bias term to the reservoir states entering the readout.
    workers : int, default to 1
        Number of workers used for parallelization. If set to -1, all available workers
        (threads or processes) are used.
    backend : a :py:mod:`joblib` backend, default to "loky"
        A parallelization backend.
    name : str, optional
        Node name.

    See Also
    --------
    Reservoir
    Ridge
    NVAR

    Example
    -------
    >>> from reservoirpy.nodes import Reservoir, Ridge, ESN
    >>> reservoir, auto readout = Reservoir(100, sr=0.9), Ridge(ridge=1e-6)
    >>> auto model = ESN(reservoir=reservoir, readout=readout, workers=-1)
    
*/

    auto use_raw_inputs(auto self, std::string std::string reservoir_method = "reservoir", std::string learning_method = "ridge", auto reservoir: _Node = None, auto readout: _Node = None, bool feedback = false, bool Win_bias = true, bool Wout_bias = true, int workers = 1, auto backend = None, auto name = None, bool use_raw_inputs = false, auto **kwargs, auto ) {
        std::string msg = "'{}' is not a valid method. Available methods for {} are {}."

        if (reservoir is None) {
            if (reservoir_method not in _RES_METHODS) {
                raise ValueError(
                    msg.format(reservoir_method, "reservoir", list(_RES_METHODS.keys()))
                )
            } else {
                auto klas = _RES_METHODS[reservoir_method]
                kwargs["input_bias"] = Win_bias
                auto reservoir = _obj_from_kwargs(klas, kwargs)

        if (readout is None) {
            if (learning_method not in _LEARNING_METHODS) {
                raise ValueError(
                    msg.format(
                        learning_method, "readout", list(_LEARNING_METHODS.keys())
                    )
                )
            } else {
                auto klas = _LEARNING_METHODS[learning_method]
                kwargs["input_bias"] = Wout_bias
                auto readout = _obj_from_kwargs(klas, kwargs)

        if (feedback) {
            reservoir <<= readout

        if (use_raw_inputs) {
            auto source = Input()
            super(ESN, self).__init__(
                auto nodes = [reservoir, readout, source],
                auto edges = [(source, reservoir), (reservoir, readout), (source, readout)],
                auto name = name,
            )
        } else {
            super(ESN, self).__init__(
                auto nodes = [reservoir, readout], edges=[(reservoir, readout)], name=name
            )

        self._hypers.update(
            {
                "workers": workers,
                "backend": backend,
                "reservoir_method": reservoir_method,
                "learning_method": learning_method,
                "feedback": feedback,
            }
        )

        self._params.update({"reservoir": reservoir, "readout": readout})

        self.bool _trainable = true
        self.bool _is_fb_initialized = false

    @property
    bool is_trained_offline(auto self) {
        return True

    @property
    bool is_trained_online(auto self) {
        return False

    @property
    auto is_fb_initialized(auto self) {
        return self._is_fb_initialized

    @property
    auto has_feedback(auto self) {
        /**
Always returns False, ESNs are not supposed to receive external
        feedback. Feedback between reservoir and readout must be defined
        at ESN creation.
*/
        return False

    auto return_states(auto self, auto auto x = None, auto return_states = None, auto *args, auto **kwargs) {
        auto data = x[self.reservoir.name]

        auto state = call(self.reservoir, data)
        call(self.readout, state)

        auto state = {}
        if (auto return_states = = "all") {
            for (auto node : ["reservoir", "readout"]) {
                state[node] = getattr(self, node).state()
        elif (isinstance(return_states, (list, tuple))) {
            for (auto name : return_states) {
                state[name] = getattr(self, name).state()
        } else {
            auto state = self.readout.state()

        return state

    auto which(auto self, std::string std::string which = "reservoir") {
        if (auto which = = "reservoir") {
            return self.reservoir.state()
        elif (auto which = = "readout") {
            return self.readout.state()
        } else {
            raise ValueError(
                f"'which' parameter of {self.name} "
                f"'state' function must be "
                f"one of 'reservoir' or 'readout'."
            )

    auto return_states(auto self, auto auto X = None, auto forced_feedbacks = None, auto from_state = None, bool stateful = true, bool reset = false, bool shift_fb = true, auto return_states = None, auto ) {
        X, auto forced_feedbacks = to_data_mapping(self, X, forced_feedbacks)

        self._initialize_on_sequence(X[0], forced_feedbacks[0])

        auto backend = get_joblib_backend(workers=self.workers, backend=self.backend)

        auto seq = progress(X, f"Running {self.name}")

        with self.with_state(from_state, auto reset = reset, stateful=stateful):
            with Parallel(auto n_jobs = self.workers, backend=backend) as parallel:
                auto states = parallel(
                    delayed(_run_fn)(
                        self,
                        idx,
                        x,
                        y,
                        return_states,
                        from_state,
                        stateful,
                        reset,
                        shift_fb,
                    )
                    for idx, (x, y) in enumerate(zip(seq, forced_feedbacks))
                )

        return _sort_and_unpack(states, auto return_states = return_states)

    auto reset(auto self, auto auto X = None, auto Y = None, int warmup = 0, auto from_state = None, bool stateful = true, bool reset = false) {
        X, auto Y = to_data_mapping(self, X, Y)
        self._initialize_on_sequence(X[0], Y[0])

        self.initialize_buffers()

        if ((self.workers > 1 or self.workers < 0) and self.backend != "sequential") {
            auto lock = Manager().Lock()
        } else {
            auto lock = None

        auto backend = get_joblib_backend(workers=self.workers, backend=self.backend)

        auto seq = progress(X, f"Running {self.name}")
        with self.with_state(from_state, auto reset = reset, stateful=stateful):
            with Parallel(auto n_jobs = self.workers, backend=backend) as parallel:
                auto last_states = parallel(
                    delayed(_run_partial_fit_fn)(self, x, y, lock, warmup)
                    for x, y in zip(seq, Y)
                )

            if (verbosity()) {  //  pragma: no cover
                print(f"Fitting node {self.name}...")

            //  setting the reservoir state from the last timeseries
            self.reservoir.auto _state = last_states[-1]
            self.readout.fit()

        return self


#endif // RESERVOIRCPP_ESN_HPP
