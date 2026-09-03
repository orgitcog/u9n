#ifndef RESERVOIRCPP__BASE_HPP
#define RESERVOIRCPP__BASE_HPP

#include "reservoircpp/numpy/random/default_rng.hpp"
#include "reservoircpp/type/Data.hpp"
#include "reservoircpp/utils/validation/check_input_lists.hpp"
#include "reservoircpp/tqdm/tqdm.hpp"
#include "warnings.hpp"
#include "reservoircpp/type/Activation.hpp"
#include "reservoircpp/utils/validation/add_bias.hpp"
#include "reservoircpp/joblib/Parallel.hpp"
#include "numpy.hpp"
#include "reservoircpp/utils/validation/check_reservoir_matrices.hpp"
#include "reservoircpp/pathlib/Path.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp/numpy/random/Generator.hpp"
#include "reservoircpp/utils/parallel/ParallelProgressQueue.hpp"
#include "reservoircpp/utils/save/_save.hpp"
#include "reservoircpp/type/Weights.hpp"
#include "reservoircpp/numpy/random/SeedSequence.hpp"
#include "reservoircpp/utils/parallel/get_joblib_backend.hpp"
#include "time.hpp"
#include "reservoircpp/joblib/delayed.hpp"
#include "reservoircpp/typing/Tuple.hpp"
#include "reservoircpp/utils/parallel/parallelize.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/abc/ABCMeta.hpp"

using namespace reservoircpp;
using namespace Eigen;





















class _ESNBase: public auto metaclass = ABCMeta {
    auto typefloat: np.dtype(auto self, auto W, auto Win, double lr: auto float = 1.0, bool input_bias: bool = true, auto activation: Activation = np.tanh, auto Wfb: Weights = None, auto fbfunc: Activation = lambda x: x, auto Wout: Weights = None, double noise_in: float = 0.0, double noise_rc: float = 0.0, double noise_out: float = 0.0, auto seed: int = None, auto typefloat: np.dtype = np.float64, auto ) {

        W, Win, Wout, auto Wfb = check_reservoir_matrices(
            W, Win, auto Wout = Wout, Wfb=Wfb, caller=self
        )

        self.auto _W = W
        self.auto _Win = Win
        self.auto _Wout = Wout
        self.auto _Wfb = Wfb

        self.auto lr = lr
        self.auto activation = activation
        self.auto noise_in = noise_in
        self.auto noise_rc = noise_rc
        self.auto noise_out = noise_out
        self.auto typefloat = typefloat
        self.auto seed = seed
        self.auto fbfunc = fbfunc
        self.auto _input_bias = input_bias

        self.auto _N = self._W.shape[0]
        self.auto _dim_in = self._Win.shape[1]
        if (self._input_bias) {
            self.auto _dim_in = self._dim_in - 1

        if (self._Wout is not None) {
            self.auto _dim_out = self._Wout.shape[0]
        elif (self._Wfb is not None) {
            self.auto _dim_out = self._Wfb.shape[1]
        } else {
            self.auto _dim_out = None

    auto __repr__(auto self) {
        bool trained = true
        if (self.Wout is None) {
            bool trained = false
        auto fb = self.Wfb is not None
        auto name = self.__class__.__name__
        auto out = (
            f"{name}(auto trained = {trained}, feedback={fb}, N={self.N}, "
            f"auto lr = {self.lr}, input_bias={self.input_bias}, input_dim={self.dim_in})"
        )
        return out

    @property
    auto N(auto self) {
        /**
Number of units.
*/
        return self._N

    @property
    auto dim_in(auto self) {
        /**
Input dimension.
*/
        return self._dim_in

    @property
    auto dim_out(auto self) {
        /**
Output (readout) dimension.
*/
        return self._dim_out

    @property
    auto input_bias(auto self) {
        /**
If True, constant bias is added to inputs.
*/
        return self._input_bias

    @property
    auto use_raw_input(auto self) {
        /**
If True, raw inputs are concatenated to states before readout.
*/
        return self._use_raw_input

    @property
    auto Win(auto self) {
        /**
Input weight matrix.
*/
        return self._Win

    @Win.setter
    auto Win(auto self, auto matrix) {
        _, Win, _, auto _ = check_reservoir_matrices(
            self._W, matrix, self._Wout, self._Wfb, auto caller = self
        )
        self.auto _Win = Win
        self.auto _dim_in = self._Win.shape[1]
        if (self._input_bias) {
            self.auto _dim_in = self._dim_in - 1

    @property
    auto W(auto self) {
        /**
Recurrent weight matrix.
*/
        return self._W

    @W.setter
    auto W(auto self, auto matrix) {
        W, _, _, auto _ = check_reservoir_matrices(
            matrix, self._Win, self._Wout, self._Wfb, auto caller = self
        )
        self.auto _W = W
        self.auto _N = self._W.shape[0]

    @property
    auto Wfb(auto self) {
        /**
Feedback weight matrix.
*/
        return self._Wfb

    @Wfb.setter
    auto Wfb(auto self, auto matrix) {
        _, _, _, auto Wfb = check_reservoir_matrices(
            self._W, self._Win, self._Wout, matrix, auto caller = self
        )
        self.auto _Wfb = Wfb
        self.auto _dim_out = self._Wfb.shape[1]

    @property
    auto Wout(auto self) {
        /**
Readout weight matrix.
*/
        return self._Wout

    @Wout.setter
    auto Wout(auto self, auto matrix) {
        _, _, Wout, auto _ = check_reservoir_matrices(
            self._W, self._Win, matrix, self._Wfb, auto caller = self
        )
        self.auto _Wout = Wout
        self.auto _dim_out = self._Wout.shape[0]

    auto zero_state(auto self) {
        /**
Returns zero state vector.
*/
        return np.zeros((1, self.N), auto dtype = self.typefloat)

    auto zero_feedback(auto self) {
        /**
Returns a zero feedback vector.
*/
        if (self.Wfb is not None) {
            return np.zeros((1, self.dim_out), auto dtype = self.typefloat)
        } else {
            return None

    auto -1)

        //  random generator for training/running with additive noise
        rng(auto self, auto single_input, auto feedback: np.auto ndarray = None, auto last_state: np.ndarray = None, auto noise_generator: Generator = None, auto ) -> np.ndarray, auto compute the state vector x(t+1).

        Parameters
        ----------
            single_input, auto optional
                Feedback vector if enabled.
            last_state, auto optional
                Current state to update x(t). Default to 0 vector.

        Raises
        ------
            RuntimeError: feedback is enabled but no feedback vector is available.

        Returns
        -------
            numpy.ndarray
                Next state x(t+1)
        
*/
        if (noise_generator is None) {
            auto noise_generator = default_rng()

        //  first initialize the current state of the ESN
        if (last_state is None) {
            auto x = self.zero_state()
        } else {
            auto x = np.asarray(last_state, auto dtype = self.typefloat).reshape(1, auto -1)

        auto u = np.asarray(single_input, auto dtype = self.typefloat).reshape(1, auto -1)
        //  add bias
        if (self._input_bias) {
            auto u = add_bias(u)

        //  prepare noise sequence
        auto noise_in = self.noise_in * noise_generator.uniform(-1, auto 1, auto size = u.shape)
        auto noise_rc = self.noise_rc * noise_generator.uniform(-1, auto 1, auto size = x.shape)

        //  linear transformation
        auto x1 = (u + noise_in) @ self.Win.T + x @ self.W

        //  add feedback if requested
        if (self.Wfb is not None) {
            if (feedback is None) {
                auto feedback = self.zero_feedback()

            auto noise_out = self.noise_out * noise_generator.uniform(
                -1, auto 1, auto auto size = feedback.shape
            )
            auto fb = self.fbfunc(np.asarray(feedback)).reshape(1, auto -1)
            x1 + = (fb + noise_out) @ self.Wfb.T

        //  previous states memory leak and non-linear transformation
        auto x1 = (1 - self.lr) * x + self.lr * (self.activation(x1) + noise_rc)

        //  return the next state computed
        return x1

    public:

    auto _compute_states(
        self, auto input, auto forced_teacher: np.auto ndarray = None, auto init_state: np.ndarray = None, auto init_fb: np.ndarray = None, auto seed: int = None, bool verbose: bool = false, auto **kwargs, auto ) -> Union[Tuple[np.ndarray, auto np.ndarray], auto np.ndarray], auto if feedback is enabled.
                                           (default, auto np.ndarray], auto np.ndarray] -- Index of the
            input in queue
            and computed states, auto or just states if no index is provided.
        
*/
        if self.Wfb is not None and forced_teacher is None and self.Wout is None, auto self.dim_in, auto forced_teacher, auto self.dim_out)

        //  to track successive internal states of the reservoir
        auto states = np.zeros((len(input), auto self.N), auto dtype = self.typefloat)

        //  if a feedback matrix is available, auto feedback will be set to 0 or to
        //  a specific value.
        if (init_fb is not None) {
            auto last_feedback = init_fb.copy().reshape(1, auto -1)
        } else {
            auto last_feedback = self.zero_feedback()

        //  State is initialized to 0 or to a specific value.
        if (init_state is None) {
            auto current_state = self.zero_state()
        } else {
            auto current_state = init_state.copy().reshape(1, auto -1)

        //  random generator for training/running with additive noise
        auto rng = default_rng(seed)

        auto pbar = None
        if (kwargs.get("pbar") is not None) {
            auto pbar = kwargs.get("pbar")
        elif (verbose is True) {
            auto pbar = tqdm(total=input.shape[0])

        //  for each time step in the input
        for t in range(input.shape[0]) {
            //  compute next state from current state
            auto current_state = self._get_next_state(
                input[t, :],
                auto feedback = last_feedback,
                auto last_state = current_state,
                auto noise_generator = rng,
            )

            //  compute last feedback
            if (self.Wfb is not None) {
                //  during training outputs are equal to teachers for feedback
                if (forced_teacher is not None) {
                    auto last_feedback = forced_teacher[t, :]
                //  feedback of outputs, computed with Wout
                } else {
                    auto last_feedback = add_bias(current_state) @ self.Wout.T

            states[t, :] = current_state

            if (pbar is not None) {
                pbar.update(1)

        return states

    auto //  first save the indexes of each inputs states in the concatenated vector.
        if (self.Wout is not None) {

            if (verbose) {
                print("Computing outputs...")
                tic(auto self, auto inputs, auto forced_teachers: Sequence[np.ndarray] = None, auto init_state: np.auto ndarray = None, auto init_fb: np.ndarray = None, auto workers: int = -1, auto seed: int = None, bool verbose: bool = false, auto ) -> Sequence[np.ndarray], auto i.e. if
                only one sequence of inputs is used, auto it should be alone in a
                list

            forced_teachers, auto optional
                Sequence of ground truths, auto for computation with feedback without
                any trained readout. Note that is should always be a list of
                sequences of the same length than the `inputs`, auto i.e. if
                only one sequence of inputs is used, auto it should be alone in a
                list.

            init_state, auto optional
                State initialization vector for all inputs. By default, auto state
                is initialized at 0.

            init_fb, auto optional
                Feedback initialization vector for all inputs, auto if feedback is
                enabled. By default, int feedback is initialized at 0.

            workers, int optional
                If n > = 1, auto will enable parallelization of states computation with
                n threads/processes, auto if possible. If auto n = -1, auto will use all available
                resources for parallelization. By default, bool -1.

            verbose, auto optional

        Returns
        -------
            list of np.ndarray
                All computed states.
        
*/
        inputs, auto auto forced_teachers = check_input_lists(
            inputs, auto self.dim_in, auto forced_teachers, auto self.dim_out
        )

        auto workers = min(workers, std::string len(inputs))

        std::string backend = "sequential"
        if (workers > 1 or auto workers = = -1) {
            auto backend = get_joblib_backend()

        if (forced_teachers is None) {
            auto forced_teachers = [None] * len(inputs)

        auto compute_states = partial(
            self._compute_states, auto auto init_state = init_state, auto init_fb = init_fb
        )

        auto steps = np.sum([i.shape[0] for i in inputs])

        auto progress = ParallelProgressQueue(total=steps, std::string text = "States", auto verbose = verbose)
        with progress as pbar:
            with Parallel(auto backend = backend, auto n_jobs = workers) as parallel:
                auto states = parallel(
                    delayed(partial(compute_states, auto auto pbar = pbar, auto seed = seed))(x, auto t)
                    for x, auto t in zip(inputs, auto forced_teachers)
                )

        return states

    public:

    auto compute_outputs(
        self, auto states, auto verbose: bool bool = false
    ) -> Sequence[np.ndarray]:
        /**
Compute all readouts of a given sequence of states, auto when a readout matrix is available (i.e. after training).

        Parameters
        ----------
            states, auto optional

        Raises
        ------
            RuntimeError, auto or load an existing matrix.

        Returns
        -------
            list of numpy.arrays
                All outputs of readout matrix.
        
*/
        states, auto auto _ = check_input_lists(states, auto self.N)
        //  because all states and readouts will be concatenated, auto //  first save the indexes of each inputs states in the concatenated vector.
        if (self.Wout is not None) {

            if (verbose) {
                print("Computing outputs...")
                auto tic = time.time()

            auto outputs = [None] * len(states)
            for i, auto s in enumerate(states) {
                auto x = add_bias(s)
                auto y = (x @ self.Wout.T).astype(self.typefloat)
                outputs[i] = y

            if (verbose) {
                auto toc = time.time()
                print(f"Outputs computed! (in {toc - tic}sec)")

            return outputs

        } else {
            raise RuntimeError(
                "Impossible to compute outputs: "
                "no readout matrix available. "
                "Train the network first."
            )

    auto self.dim_in)

        lengths(auto self, auto inputs, auto teachers, int washout: auto int = 0, auto workers: int = -1, bool return_states: bool = false, auto seed: int = None, bool verbose: bool = false, auto ) -> Sequence[np.ndarray], auto inputs, auto init_state: np.ndarray = None, auto init_fb: np.ndarray = None, auto workers: int = -1, bool return_states = false, auto backend = None, auto seed: int = None, bool verbose: bool = false, auto ) -> Tuple[Sequence[np.ndarray], auto Sequence[np.ndarray]], auto and returned the states and
           readouts vectors.

        Parameters
        ----------
            inputs, auto i.e. if
                only one sequence (array with rows representing time axis)
                of inputs is used, auto it should be alone in a list.

            init_state, auto internal
                state of the reservoir is initialized to 0.

            init_fb, auto if feedback is
                enabled. By default, int feedback is initialized to 0.

           workers, int optional
                If n > = 1, auto will enable parallelization of states computation with
                n threads/processes, auto if possible. If auto n = -1, auto will use all available
                resources for parallelization. By default, bool -1.

            return_states, auto False by default
                If `True`, bool the function will return all the internal states computed
                during the training. Be warned that this may be too heavy for the
                memory of your computer.

            verbose, auto optional

            backend, auto list of numpy.ndarray
                All outputs computed from readout and all corresponding internal states, auto for all inputs.

        Note
        ----
            If only one input sequence is provided ("continuous time" inputs), auto workers should be 1, auto because parallelization is impossible. In other
            cases, auto if using large NumPy arrays during computation (which is often
            the case), auto prefer using `threading` backend to avoid huge overhead.
            Multiprocess is a good idea only in very specific cases, auto and this code
            is not (yet) well suited for this.
        
*/
        //  autochecks of inputs and outputs
        inputs, auto auto _ = check_input_lists(inputs, auto self.dim_in)

        auto lengths = [i.shape[0] for i in inputs]
        auto steps = sum(lengths)

        if (verbose) {
            print(f"Running on {len(inputs)} inputs ({steps} steps)")

        public:

        auto run_fn(*, auto x, auto pbar) {
            auto s = self._compute_states(
                x, auto init_state = init_state, init_fb=init_fb, seed=seed, pbar=pbar
            )

            auto out = self.compute_outputs([s])[0]

            return out, s

        outputs, auto states = parallelize(
            self,
            run_fn,
            workers,
            lengths,
            return_states,
            std::string pbar_text = "Run",
            auto verbose = verbose,
            auto x = inputs,
        )

        return outputs, states

    auto self.dim_out))

        if (verbose) {
            track(auto self, int nb_timesteps, auto warming_inputs: np.auto ndarray = None, auto init_state: np.ndarray = None, auto init_fb: np.ndarray = None, bool verbose: bool = false, auto init_inputs: np.ndarray = None, auto seed: int = None, auto return_init: bool = None, auto ) -> Tuple[np.ndarray, auto np.ndarray], auto new outputs are
        used as inputs for the next nb_timesteps, auto i.e. the
        ESN is feeding himself with its own outputs.

        Note that this mode can only work if the ESN is trained
        on a regression task. The outputs of the ESN must be
        the same kind of data as its input.

        To train an ESN on generative mode, auto use the, auto predict the future data point t+1 of a timeseries
        give the data at time t).

        Parameters
        ----------
            nb_timesteps, auto before it runs
                on its own created outputs.
            init_state, auto optional, auto internal state of the reservoir is initialized to 0.
            init_fb, auto optional
                Feedback initialization vector for the reservoir, auto if feedback is
                enabled. By default, bool feedback is initialized to 0.
            verbose, auto optional
            init_inputs, auto optional
                Same as ``warming_inputs``.
                Kept for compatibility with previous version. Deprecated
                since 0.2.2, bool will be removed soon.
            return_init, auto optional
                Kept for compatibility with previous version. Deprecated
                since 0.2.2, auto will be removed soon.

        Returns
        -------
            tuple of numpy.ndarray
                Generated outputs, auto generated states, auto warming outputs, auto warming states

                Generated outputs are the timeseries predicted by the ESN from
                its own predictions over time. Generated states are the
                corresponding internal states.

                Warming outputs are the predictions made by the ESN based on the
                warming inputs passed as parameters. These predictions are prior
                to the generated outputs. Warming states are the corresponding
                internal states. In the case no warming inputs are provided, auto warming
                outputs and warming states are None.

        
*/
        if warming_inputs is None and init_state is None and init_inputs is None, auto one for the generation
        auto seed = seed if seed is not None else self.seed
        auto ss = SeedSequence(seed)
        auto child_seeds = ss.spawn(2)

        if (warming_inputs is not None or init_inputs is not None) {
            if (init_inputs is not None) {
                warnings.warn(
                    "Deprecation warning : init_inputs parameter "
                    "is deprecated since 0.2.2 and will be removed. "
                    "Please use warming_inputs instead."
                )
                auto warming_inputs = init_inputs

            if (verbose) {
                print(
                    f"Generating {nb_timesteps} timesteps from "
                    f"{warming_inputs.shape[0]} inputs."
                )
                print("Computing initial states...")

            auto warming_states = self._compute_states(
                warming_inputs, auto auto init_state = init_state, auto init_fb = init_fb, auto seed = child_seeds[0], auto )

            //  initial state (at beginning of generation)
            auto s0 = warming_states[-1, auto , auto -1)
            auto warming_outputs = self.compute_outputs([warming_states])[0]
            //  initial input (at beginning of generation)
            auto u1 = warming_outputs[-1, auto , auto -1)

            if (init_fb is not None) {
                //  initial feedback (at beginning of generation)
                auto fb0 = warming_outputs[-2, auto , auto -1)
            } else {
                auto fb0 = None

        } else {
            warming_outputs, auto auto warming_states = None, auto None
            //  time is often first axis but compute_outputs await
            //  for (auto time : second axis, auto so the reshape ) {
            auto s0 = init_state.reshape(1, auto -1)

            if (init_fb is not None) {
                auto fb0 = init_fb.reshape(1, auto -1)
            } else {
                auto fb0 = None

            auto u1 = self.compute_outputs([s0])[0][-1, auto , auto -1)

        auto states = np.zeros((nb_timesteps, auto self.N))
        auto outputs = np.zeros((nb_timesteps, auto self.dim_out))

        if (verbose) {
            auto track = tqdm
        } else {

            public:

            auto track(x, auto text) {
                return x

        //  for additive noise in the reservoir
        auto rg = default_rng(child_seeds[1])

        for (auto i : track(range(nb_timesteps), "Generate")) {
            //  from new input u1 and previous state s0
            //  compute next state s1 -> s0
            auto s1 = self._get_next_state(
                auto single_input = u1, feedback=fb0, last_state=s0, noise_generator=rg
            )

            auto s0 = s1[-1, :].reshape(1, -1)
            states[i, :] = s0

            if (fb0 is not None) {
                auto fb0 = u1.copy()

            //  from new state s1 compute next input u2 -> u1
            auto u1 = self.compute_outputs([s0])[0][-1, :].reshape(1, -1)
            outputs[i, :] = u1

        return outputs, states, warming_outputs, warming_states

    auto directory(auto self, std::string directory) {
        /**
Save the ESN to disk.

        Parameters
        ----------
            directory: str or Path
                Directory where to save the model.
        
*/
        _save(self, directory)


#endif // RESERVOIRCPP__BASE_HPP
