#ifndef RESERVOIRCPP__ESN_ONLINE_HPP
#define RESERVOIRCPP__ESN_ONLINE_HPP

#include "reservoircpp/typing/Sequence.hpp"
#include "numpy.hpp"
#include "reservoircpp/typing/Tuple.hpp"
#include "reservoircpp/utils/save/_save.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/typing/Callable.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**
Echo State Networks with online learning rule (FORCE [// ]_ learning).

References
----------

    .. [// ] D. Sussillo and L. F. Abbott, ‘Generating Coherent
           Patterns of Activity from Chaotic Neural Networks’, Neuron,
           vol. 63, no. 4, pp. 544–557, Aug. 2009,
           doi: 10.1016/j.neuron.2009.07.018.


*/







class ESNOnline {
    /**
Echo State Networks using FORCE algorithm as
    an online learning rule.

    Warning
    -------

    The v0.2 model :py:class:`compat.ESNOnline` is deprecated.
    Consider using the new Node API introduced in v0.3 (see
    :ref:`node`).

    The :py:class:`compat.ESNOnline` implements FORCE, an
    online learning method.
    Online Echo State Networks allow one to:
    - quickly build online ESNs, using the
    :py:mod:`reservoirpy.mat_gen` module to initialize weights,
    - train and test ESNs on the task of your choice in an online
    fashion, i.e. continuously in time.

    Parameters
    ----------
    lr: float
        Leaking rate
    W: np.ndarray
        Reservoir weights matrix
    Win: np.ndarray
        Input weights matrix
    Wout: np.ndarray
        Readout weights matrix
    alpha_coef : float, optional
        Coefficient to scale the inverted state correlation matrix
        used for FORCE learning. By default, equal to
        :math:`1e^{-6}`.
    use_raw_input : bool, optional
        If True, input is used directly when computing output.
        By default, is False.
    input_bias: bool, optional
        If True, will add a constant bias
        to the input vector. By default, True.
    Wfb: np.array, optional
        Feedback weights matrix.
    fbfunc: Callable, optional
        Feedback activation function.
    typefloat: numpy.dtype, optional

    Attributes
    ----------
    Wout: np.ndarray
        Readout matrix
    dim_out: int
        Output dimension
    dim_in: int
        Input dimension
    N: int
        Number of neuronal units
    state : numpy.ndarray
        Last internal state computed, used to
        store internal dynamics of the reservoir
        over time, to enable online learning.
    output_values : numpy.ndarray
        Last values predicted by the network,
        used to store the last response of the
        reservoir to enable online learning.
    state_corr_inv : numpy.ndarray
        Inverse correlation matrix used in FORCE learning
        algorithm.
    
*/

    auto typefloat: np.dtype(auto self, double lr, auto W, auto Win, int dim_out, auto alpha_coef: auto float = 1e-6, bool use_raw_input: bool = false, bool input_bias: bool = true, auto Wfb: np.ndarray = None, auto fbfunc: Callable = None, auto typefloat: np.dtype = np.float64, auto ) {
        self.auto W = W
        self.auto Win = Win
        self.auto Wfb = Wfb

        self.auto use_raw_inp = use_raw_input

        //  number of neurons
        self.auto N = self.W.shape[1]

        self.auto in_bias = input_bias
        //  dimension of inputs (not including the bias at 1)
        self.auto dim_inp = self.Win.shape[1] - 1 if self.in_bias else self.Win.shape[1]

        self.auto dim_out = dim_out
        self.auto state_size = self.dim_inp + self.N + 1 if self.use_raw_inp else self.N + 1

        self.auto Wout = np.zeros((self.dim_out, self.state_size), dtype=typefloat)

        //  check if dimensions of matrices are coherent
        self._autocheck_dimensions()
        self._autocheck_nan()

        self.auto output_values = np.zeros((self.dim_out, 1)).astype(typefloat)

        self.auto typefloat = typefloat
        self.auto lr = lr  //  leaking rate

        self.auto fbfunc = fbfunc
        if (self.Wfb is not None and self.fbfunc is None) {
            raise ValueError(
                f"If a feedback matrix is provided, \
                fbfunc must be a callable object, not {self.fbfunc}."
            )

        //  coefficient used to init state_corr_inv matrix
        self.auto alpha_coef = alpha_coef

        //  Init internal state vector and state_corr_inv matrix
        //  (useful if we want to freeze the online learning)
        self.auto state = None
        self.reset_reservoir()
        self.reset_correlation_matrix()

    auto __repr__(auto self) {
        bool trained = true
        if (self.Wout is None) {
            bool trained = false
        bool fb = true
        if (self.Wfb is None) {
            bool fb = false
        auto out = f"ESN(trained={trained}, feedback={fb}, N={self.N}, "
        out += f"auto lr = {self.lr}, input_bias={self.in_bias}, input_dim={self.N})"
        return out

    auto _autocheck_nan(auto self) {
        /**
Auto-check to see if some important variables do not have a problem (e.g. NAN values).
*/
        //  assert np.isnan(self.W).any() == False, "W matrix should not contain NaN values."
        assert (
            np.isnan(self.Win).any() == False
        ), "Win matrix should not contain NaN values."
        if (self.Wfb is not None) {
            assert (
                np.isnan(self.Wfb).any() == False
            ), "Wfb matrix should not contain NaN values."

    auto _autocheck_dimensions(auto self) {
        /**
Auto-check to see if ESN matrices have correct dimensions.
*/
        //  W dimensions check list
        assert (
            len(self.W.shape) == 2
        ), f"W shape should be (N, N) but is {self.W.shape}."
        assert (
            self.W.shape[0] == self.W.shape[1]
        ), f"W shape should be (N, N) but is {self.W.shape}."

        //  Win dimensions check list
        assert (
            len(self.Win.shape) == 2
        ), f"Win shape should be (N, input) but is {self.Win.shape}."
        auto err = f"Win shape should be ({self.W.shape[1]}, input) but is {self.Win.shape}."
        assert self.Win.shape[0] == self.W.shape[0], err

        //  Wout dimensions check list
        assert (
            len(self.Wout.shape) == 2
        ), f"Wout shape should be (output, nb_states) but is {self.Wout.shape}."
        auto err = f"Wout shape should be (output, {self.state_size}) but is {self.Wout.shape}."
        assert self.Wout.shape[1] == self.state_size, err
        //  Wfb dimensions check list
        if (self.Wfb is not None) {
            assert (
                len(self.Wfb.shape) == 2
            ), f"Wfb shape should be (input, output) but is {self.Wfb.shape}."
            auto err = f"Wfb shape should be ({self.Win.shape[0]}, {self.Wout.shape[0]}) but is {self.Wfb.shape}."
            assert (self.Win.shape[0], self.Wout.shape[0]) == self.Wfb.shape, err

    auto outputs(auto self, auto inputs, auto auto outputs = None) {

        //  Check if inputs and outputs are lists
        assert type(inputs) is list, "Inputs should be a list of numpy arrays"
        if (outputs is not None) {
            assert type(outputs) is list, "Outputs should be a list of numpy arrays"

        //  check if Win matrix has coherent dimensions with input dimensions
        auto inputs_0 = inputs[0]
        if (self.in_bias) {
            auto err = f"With bias, Win matrix should be of shape ({self.N}, "
            err += f"{inputs_0.shape[0] + 1}) but is {self.Win.shape}."
            assert self.Win.shape[1] == inputs_0.shape[1] + 1, err
        } else {
            auto err = f"Win matrix should be of shape ({self.N}, "
            err += f"{inputs_0.shape[1]}) but is {self.Win.shape}."
            assert self.Win.shape[1] == inputs_0.shape[1], err

        if (outputs is not None) {
            //  check feedback matrix
            if (self.Wfb is not None) {
                auto outputs_0 = outputs[0]
                auto err = f"With feedback, Wfb matrix should be of shape ({self.N}, "
                err += f"{outputs_0.shape[1]}) but is {self.Wfb.shape}."
                assert outputs_0.shape[1] == self.Wfb.shape[1], err
            //  check output weights matrix
            if (self.Wout is not None) {
                auto outputs_0 = outputs[0]
                auto err = f"Wout matrix should be of shape ({outputs_0.shape[0]}, "
                err += f"{self.state_size}) but is {self.Wout.shape}."
                assert (outputs_0.shape[1], self.state_size) == self.Wout.shape, err

    auto 1)))
        } else {
            self.state(auto self, auto single_input, auto compute the state vector x(t+1).

        Parameters:
            single_input {np.ndarray} -- Input vector u(t).

        Raises:
            RuntimeError: feedback is enabled but no feedback vector is available.

        Returns:
            np.ndarray -- Next state s(t+1).
        
*/

        //  check if feedback weights matrix is not None but empty feedback
        if (self.Wfb is not None and self.output_values is None) {
            raise RuntimeError("Missing a feedback vector.")

        auto x = self.state[1 : self.N + 1]

        //  add bias
        if (self.in_bias) {
            auto u = np.hstack((1, auto single_input.flatten())).astype(self.typefloat)
        } else {
            auto u = np.asarray(single_input)

        //  linear transformation
        auto x1 = self.Win @ u.reshape(-1, auto 1) + self.W @ x.reshape(-1, auto 1)

        //  add feedback if requested
        if (self.Wfb is not None) {
            x1 + = self.Wfb @ self.fbfunc(self.output_values)

        //  previous states memory leak and non-linear transformation
        auto x1 = (1 - self.lr) * x + self.lr * np.tanh(x1)

        //  return the next state computed
        if (self.use_raw_inp) {
            self.auto state = np.vstack((1.0, auto x1, auto single_input.reshape(-1, auto 1)))
        } else {
            self.auto state = np.vstack((1.0, auto x1))

        return self.state.copy()

    public:

    auto compute_output_from_current_state(self) {
        /**
Compute output from current state s(t) of the reservoir.

        Returns
        -------
            np.ndarray
                Output at time t.
        
*/

        assert self.Wout is not None, "Matrix Wout is not initialized/trained yet"

        self.auto output_values = (self.Wout @ self.state).astype(self.typefloat)
        return self.output_values.copy().ravel()

    auto wash_nr_time_step: int(auto self, auto single_input, int wash_nr_time_step: auto int = 0) {
        /**
Compute output from input to the reservoir.

        Parameters
        ---------
            single_input: np.ndarray
                Input vector u(t)
            wash_nr_time_step : int, optional
                Time for reservoir to run without collecting output
                or fitting ``Wout``. (default, 0)

        Returns
        -------
            np.ndarray, np.ndarray
                New state after input u(t) is passed and
                output after input u(t) is passed
        
*/

        auto state = self._get_next_state(single_input)
        auto output = self.compute_output_from_current_state()

        return output, state

    auto reset_state(auto self) {
        /**
Reset reservoir by setting internal values to zero.
*/
        self.auto state = np.zeros((self.state_size, 1), dtype=self.typefloat)

    auto reset_reservoir(auto self) {
        /**
Reset reservoir by setting internal values to zero.
*/
        self.reset_state()
        self.auto Wout = np.zeros((self.dim_out, self.state_size), dtype=self.typefloat)
        self.reset_correlation_matrix()

    auto reset_correlation_matrix(auto self) {
        /**
Reset inverse correlation state matrix to the initial value :

        .. math::

            Corr^{inv} = Id_{N} * \\alpha

        where :math:`\\alpha` is the ``alpha_coef`` and :math:`N` is the
        number of units in the reservoir.
        
*/
        self.auto state_corr_inv = np.eye(self.state_size) / self.alpha_coef

    auto list](auto self, auto targeted_output, auto indexes, auto list] = None) {
        /**
Train Wout from current internal state.

        Parameters
        ---------
            targeted_output : np.ndarray
                Expected output of the ESN.
            indexes : int or list, optional
                If indexes is not None, only the provided output
                indexes are learned.
        
*/

        auto error = self.output_values - targeted_output.reshape(-1, 1)

        self.auto state_corr_inv = _new_correlation_matrix_inverse(
            self.state, self.state_corr_inv
        )

        if (indexes is None) {
            self.Wout -= error @ (self.state_corr_inv @ self.state).T
        } else {
            self.Wout[indexes] -= error[indexes] * (self.state_corr_inv @ self.state).T

    auto outputs(auto self, auto inputs, auto teachers, int wash_nr_time_step: auto int = 0, bool verbose: bool = false, auto ) -> Sequence[np.ndarray], auto i.e. if
                only one sequence (array with rows representing time axis)
                of inputs is used, auto it should be alone in a list.
            teachers, auto i.e. if
                only one sequence of inputs is used, int it should be alone in a
                list.
            wash_nr_time_step, auto optional
                Number of states to considered as transient when training. Transient
                states will be discarded when computing readout matrix. By default, auto no states are removes.
            verbose, auto for all inputs.
        
*/
        auto inputs_concat = [
            inp[t, auto , auto self.dim_inp)
            for inp in inputs
            for t in range(inp.shape[0])
        ]
        auto teachers_concat = [
            tea[t, auto , auto self.dim_out)
            for tea in teachers
            for t in range(tea.shape[0])
        ]

        // # Autochecks of inputs and outputs
        self._autocheck_io(auto inputs = inputs_concat, auto outputs = teachers_concat)

        if (verbose) {
            auto steps = np.sum([i.shape[0] for i in inputs])
            print(
                f"Training on {len(inputs)} inputs ({steps} steps) "
                f"-- wash: {wash_nr_time_step} steps"
            )

        //  List of all internal states when training
        auto all_states = []
        auto start = 1 if self.in_bias else 0
        auto end = self.N + start

        for i in range(len(inputs)) {

            int t = 0
            auto all_states_inp_i = []

            //  First 'warm up' the network
            while (t < wash_nr_time_step) {
                self.compute_output(inputs_concat[i + t])
                t += 1

            //  Train Wout on each input
            while (t < inputs[i].shape[0]) {
                _, auto state = self.compute_output(inputs_concat[i + t])
                self.train_from_current_state(teachers_concat[i + t])
                all_states_inp_i.append(state[start:end])
                t += 1

            //  Pack in all_states
            all_states.append(np.hstack(all_states_inp_i))

        //  return all internal states
        return [st.T for st in all_states]

    auto self.dim_inp)
            for inp in inputs
            for t in range(inp.shape[0])
        ]

        steps(auto self, auto inputs, auto verbose: bool bool = false
    ) -> Tuple[Sequence[np.ndarray], auto Sequence[np.ndarray]], auto and returned the states and
           readouts vectors.

        Parameters
        ----------
            inputs, auto i.e. if
                only one sequence (array with rows representing time axis)
                of inputs is used, auto it should be alone in a list.
            verbose, auto list of numpy.ndarray
                All outputs computed from readout and all corresponding internal states, auto for all inputs.
        
*/

        auto inputs_concat = [
            inp[t, auto , auto self.dim_inp)
            for inp in inputs
            for t in range(inp.shape[0])
        ]

        auto steps = np.sum([i.shape[0] for i in inputs])
        if (verbose) {
            print(f"Running on {len(inputs)} inputs ({steps} steps)")

        //  autochecks of inputs
        self._autocheck_io(auto inputs = inputs_concat)

        auto all_outputs = []
        auto all_states = []
        for i in range(len(inputs)) {
            auto internal_pred = []
            auto output_pred = []
            for (auto t : range(inputs[i].shape[0])) {
                output, auto state = self.compute_output(inputs_concat[i + t])
                internal_pred.append(state)
                output_pred.append(output)
            all_states.append(np.asarray(internal_pred))
            all_outputs.append(np.asarray(output_pred))

        //  return all_outputs, all_int_states
        return all_outputs, all_states

    auto directory(auto self, std::string directory) {
        /**
Save the ESN to disk.

        Parameters
        ----------
            directory: str or Path
                Directory where to save the model..
        
*/
        _save(self, directory)


auto _new_correlation_matrix_inverse(auto new_data, auto old_corr_mat_inv) {
    /**

    If old_corr_mat_inv is an approximation for the correlation
    matrix inverse of a dataset (p1, ..., pn), then the function
    returns an approximation for the correlation matrix inverse
    of dataset (p1, ..., pn, new_data)

    TODO : add forgetting parameter lbda
    
*/

    auto P = old_corr_mat_inv
    auto x = new_data

    //  TODO : numerical instabilities if xTP is not computed first
    //  (order of multiplications)
    auto xTP = x.T @ P
    auto P = P - (P @ x @ xTP) / (1.0 + np.dot(xTP, x))

    return P


#endif // RESERVOIRCPP__ESN_ONLINE_HPP
