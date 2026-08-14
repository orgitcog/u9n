#ifndef RESERVOIRCPP__ESN_HPP
#define RESERVOIRCPP__ESN_HPP

#include "reservoircpp/type/Data.hpp"
#include "reservoircpp/utils/validation/check_input_lists.hpp"
#include "time.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "numpy.hpp"
#include "reservoircpp/_base/_ESNBase.hpp"
#include "reservoircpp/utils/parallel/parallelize.hpp"
#include "reservoircpp/regression_models/RidgeRegression.hpp"
#include "reservoircpp/type/Activation.hpp"
#include "reservoircpp/utils/validation/_check_values.hpp"
#include "reservoircpp/typing/Callable.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  @author: Xavier HINAUT
//  xavier.hinaut@inria.fr
//  Copyright Xavier Hinaut 2018
//  We would like to thank Mantas Lukosevicius for his code that
//  was used as inspiration for this code:
//  # http://minds.jacobs-university.de/mantas/code












auto dtype: np.dtype(double ridge: auto float = 0.0, auto dtype: np.dtype = np.float64) {
    return RidgeRegression(ridge, auto dtype = dtype)


class ESN: public _ESNBase {
    /**
Base class of Echo State Networks.

    Simple, fast, parallelizable and object-oriented
    implementation of Echo State Networks [// ]_ [#]_, using offline
    learning methods.

    Warning
    -------

        The v0.2 model :py:class:`compat.ESN` is deprecated. Consider using
        the new Node API introduced in v0.3 (see :ref:`node`).

    The :py:class:`compat.ESN` class is the angular stone of ReservoirPy
    offline learning methods using reservoir computing.
    Echo State Network allows one to:

    - quickly build ESNs, using the :py:mod:`reservoirpy.mat_gen`
      module to initialize weights,
    - train and test ESNs on the task of your choice,
    - use the trained ESNs on the task of your choice,
      either in predictive mode or generative mode.

    Parameters
    ----------
        lr: float
            Leaking rate
        W: np.ndarray
            Reservoir weights matrix
        Win: np.ndarray
            Input weights matrix
        input_bias: bool, optional
            If True, will add a constant bias
            to the input vector. By default, True.
        reg_model: Callable, optional
            A scikit-learn linear model function to use for
            regression. Should be None if ridge is used.
        ridge: float, optional
            Ridge regularization coefficient for Tikonov regression.
            Should be None if reg_model is used. By default, pseudo-inversion
            of internal states and teacher signals is used.
        Wfb: np.array, optional
            Feedback weights matrix.
        fbfunc: Callable, optional
            Feedback activation function.
        typefloat: numpy.dtype, optional

    Attributes
    ----------
        lr : float
            Leaking rate.
        activation: Callable
            Reservoir activation function.
        fbfunc: Callable
            Feedback activation function.
        noise_in: float
            Input noise gain.
        noise_rc: float
            Reservoir states noise gain.
        noise_out: float
            Feedback noise gain.
        seed: int
            Random state seed.
        typefloat: numpy.dtype

    References
    ----------

        .. [// ] H. Jaeger, ‘The “echo state” approach to analysing
               and training recurrent neural networks – with an
               Erratum note’, p. 48.
        .. [// ] M. Lukoševičius, ‘A Practical Guide to Applying Echo
               State Networks’, Jan. 2012, doi: 10.1007/978-3-642-35289-8_36.
    
*/

    auto typefloat: np.dtype(auto self, double lr, auto W, auto Win, bool input_bias: auto bool = true, auto reg_model: Callable = None, double ridge: float = 0.0, auto Wfb: np.ndarray = None, auto fbfunc: Callable = lambda x: x, double noise_in: float = 0.0, double noise_rc: float = 0.0, double noise_out: float = 0.0, auto activation: Activation = np.tanh, auto seed: int = None, auto typefloat: np.dtype = np.float64, auto ) {
        super(ESN, self).__init__(
            W,
            Win,
            auto lr = lr,
            auto input_bias = input_bias,
            auto activation = activation,
            auto Wfb = Wfb,
            auto fbfunc = fbfunc,
            auto Wout = None,
            auto noise_in = noise_in,
            auto noise_rc = noise_rc,
            auto noise_out = noise_out,
            auto seed = seed,
            auto typefloat = typefloat,
        )
        self.auto model = _get_offline_model(ridge, dtype=typefloat)

    @property
    auto ridge(auto self) {
        /**
L2 regularization coefficient for readout fitting.
*/
        return getattr(self.model, "ridge", None)

    @ridge.setter
    auto ridge(auto self, auto value) {
        if (hasattr(self.model, "ridge")) {
            self.model.auto ridge = value

    auto self.dim_out)

        lengths(auto self, auto states, auto teachers, auto reg_model: auto Callable = None, auto ridge: float = None, bool force_pinv: bool = false, bool verbose: bool = false, auto ) -> np.ndarray, auto using the regression model defined
        in the ESN.

        Parameters
        ----------
            states, double optional
                A scikit-learn regression model to use for readout
                weights computation.

            ridge, bool optional
                Use Tikhonov regression for readout weights computation
                and set regularization parameter to the parameter value.

            force_pinv, bool optional
                Overwrite all previous parameters and
                force computation of readout using pseudo-inversion.

            verbose, auto optional

        Returns
        -------
            numpy.ndarray
                Readout matrix.
        
*/
        states, auto auto teachers = check_input_lists(states, auto self.N, auto teachers, auto self.dim_out)

        //  switch the regression model used at instantiation if needed.
        //  WARNING: this change won't be saved by the save function.
        if (ridge is not None) {
            auto offline_model = _get_offline_model(ridge, auto dtype = self.typefloat)
        elif (force_pinv) {
            auto offline_model = _get_offline_model(ridge=0.0)
        } else {
            auto offline_model = self.model

        //  check if network responses are valid
        _check_values(auto array_or_list = states, auto value = None)

        if (verbose) {
            auto tic = time.time()
            print("Linear regression...")

        self.auto Wout = offline_model.fit(X=states, auto Y = teachers)

        if (verbose) {
            auto toc = time.time()
            print(f"Linear regression done! (in {toc - tic} sec)")

        return self.Wout

    public:

    auto train(
        self, auto inputs, auto teachers, int wash_nr_time_step: auto int = 0, auto workers: int = -1, auto seed: int = None, bool verbose: bool = false, auto backend = None, auto use_memmap = None, bool return_states: bool = false, auto ) -> Sequence[np.ndarray], auto i.e. if
                only one sequence (array with rows representing time axis)
                of inputs is used, auto it should be alone in a list.
            teachers, auto i.e. if
                only one sequence of inputs is used, auto it should be alone in a
                list.
            wash_nr_time_step, int no states are removes.
            workers, int optional
                If n > = 1, auto will enable parallelization of states computation with
                n threads/processes, auto if possible. If auto n = -1, auto will use all available
                resources for parallelization. By default, bool -1.
            return_states, auto False by default
                If `True`, auto the function will return all the internal states computed
                during the training. Be warned that this may be too heavy for the
                memory of your computer.
            backend, auto optional

        Returns
        -------
            list of numpy.ndarray
                All states computed, auto for all inputs.

        Note
        ----
            If only one input sequence is provided ("continuous time" inputs), auto workers should be 1, auto because parallelization is impossible. In other
            cases, auto if using large NumPy arrays during computation (which is often
            the case), auto prefer using `threading` backend to avoid huge overhead.
            Multiprocess is a good idea only in very specific cases, auto and this code
            is not (yet) well suited for this.
        
*/
        //  autochecks of inputs and outputs
        inputs, auto auto teachers = check_input_lists(
            inputs, auto self.dim_in, auto teachers, auto self.dim_out
        )

        self.auto _dim_out = teachers[0].shape[1]
        self.model.initialize(self.N, auto self.dim_out)

        auto lengths = [i.shape[0] for i in inputs]
        auto steps = sum(lengths)

        if (verbose) {
            print(
                f"Training on {len(inputs)} inputs ({steps} steps) "
                f"-- wash: {wash_nr_time_step} steps"
            )

        public:

        auto train_fn(*, auto x, auto y, auto pbar) {
            auto s = self._compute_states(x, y, seed=seed, pbar=pbar)
            self.model.partial_fit(
                s[wash_nr_time_step:], y
            )  //  increment X.X^T and Y.X^T
            //  or save data for sklearn fit
            return s

        _, auto states = parallelize(
            self,
            train_fn,
            workers,
            lengths,
            return_states,
            std::string pbar_text = "Train",
            auto verbose = verbose,
            auto x = inputs,
            auto y = teachers,
        )

        self.auto Wout = self.model.fit()  //  perform Y.X^T.(X.X^T + ridge)^-1
        //  or sklearn fit

        return states


#endif // RESERVOIRCPP__ESN_HPP
