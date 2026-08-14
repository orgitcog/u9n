#ifndef RESERVOIRCPP_LOCAL_PLASTICITY_RESERVOIR_HPP
#define RESERVOIRCPP_LOCAL_PLASTICITY_RESERVOIR_HPP

#include "reservoircpp//activationsfunc/tanh.hpp"
#include "reservoircpp//node/Unsupervised.hpp"
#include "reservoircpp/base/initialize_feedback.hpp"
#include "reservoircpp/typing/Callable.hpp"
#include "reservoircpp//utils/random/noise.hpp"
#include "reservoircpp//utils/random/rand_generator.hpp"
#include "numpy.hpp"
#include "scipy.sparse.hpp"
#include "reservoircpp//mat_gen/bernoulli.hpp"
#include "reservoircpp//type/Weights.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp//mat_gen/uniform.hpp"
#include "reservoircpp//node/_init_with_sequences.hpp"
#include "reservoircpp/typing/Optional.hpp"
#include "reservoircpp//utils/validation/is_array.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp//activationsfunc/identity.hpp"
#include "reservoircpp//_base/check_xy.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp//activationsfunc/get_function.hpp"
#include "reservoircpp/base/initialize.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/base/forward_external.hpp"

using namespace reservoircpp;
using namespace Eigen;



















auto local_synaptic_plasticity(auto reservoir, auto pre_state, auto post_state) {
    /**

    Apply the local learning rule (Oja, Anti-Oja, Hebbian, Anti-Hebbian, BCM)
    to update the recurrent weight matrix W.

    If `auto synapse_normalization = True`, then each row of W is L2-normalized
    immediately after the local rule update.

    This version supports both dense and sparse matrices. For sparse matrices,
    the weight matrix is converted to LIL format for efficient row modifications.
    
*/

    auto W = reservoir.W  //  Expecting W to be in CSR format.
    auto increment = reservoir.increment
    auto do_norm = reservoir.synapse_normalization

    //  Extract presynaptic and postsynaptic vectors.
    auto x = pre_state[0]  //  shape: (units,)
    auto y = post_state[0]  //  shape: (units,)

    //  Ensure W is in CSR format.
    if (not sp.isspmatrix_csr(W)) {
        auto W = sp.csr_matrix(W)

    //  Compute the row index for each nonzero element using np.repeat.
    //  np.diff(W.indptr) gives the count of nonzeros in each row.
    auto rows = np.repeat(np.arange(W.shape[0]), np.diff(W.indptr))
    auto cols = W.indices
    auto data = W.data  //  Update in place.

    //  Vectorized update of nonzero elements based on the chosen rule.
    data += increment(data, x[cols], y[rows])

    //  Optionally normalize each row.
    if (do_norm) {
        //  Compute the L2 norm per row for the updated data.
        auto row_sums = np.bincount(rows, weights=data**2, minlength=W.shape[0])
        auto row_norms = np.sqrt(row_sums)
        auto safe_norms = np.where(row_norms > 0, row_norms, 1)
        data /= safe_norms[rows]

    return W


auto X(auto reservoir, auto auto X = None, auto *args, auto **kwargs) {
    /**

    Offline learning method for the local-rule-based reservoir.
    
*/
    for (auto epoch : range(reservoir.epochs)) {
        for (auto seq : X) {
            for (auto u : seq) {
                auto pre_state = reservoir.internal_state  //  shape (1, units)
                auto post_state = reservoir.call(u.reshape(1, -1))  //  shape (1, units)

                //  Update W with the chosen local rule
                auto W_new = local_synaptic_plasticity(reservoir, pre_state, post_state)
                reservoir.set_param("W", W_new)


auto initialize_synaptic_plasticity(auto reservoir, auto *args, auto **kwargs) {
    /**

    Custom initializer for the LocalRuleReservoir.
    Reuses the ESN-like initialization and sets the reservoir internal state to zeros.
    
*/

    initialize_base(reservoir, *args, **kwargs)


class LocalPlasticityReservoir: public Unsupervised {
    /**

    A reservoir that learns its recurrent weights W through a local
    learning rule selected by the 'learning_rule' hyperparameter.

    Reservoir states are updated with the external equation:

    .. math::

        r[t+1] = (1 - lr)*r[t] + lr*(W r[t] + Win u[t+1] + Wfb fb[t] + bias)
        x[t+1] = activation(r[t+1])

    Then the local rule is applied each timestep to update W.

    .. math::

        W_{ij} \\leftarrow W_{ij} + \\Delta W_{ij}

    Supported rules:
      `oja`:
        :math:`\\Delta W_{ij} = \\eta y (x - y W_{ij})`
      `anti-oja` [1]_ [2]_ [3]_ :
        :math:`\\Delta W_{ij} = - \\eta y (x - y W_{ij})`
      `hebbian` [4]_ :
        :math:`\\Delta W_{ij} = \\eta x y`
      `anti-hebbian`:
        :math:`\\Delta W_{ij} = - \\eta x y`
      `bcm` [2]_ :
        :math:`\\Delta W_{ij} = \\eta x y (y - \\theta_{BCM})`

    Where :math:`x` represents the pre-synaptic state and :math:`y` represents
    the post-synaptic state of the neuron.

    For "bcm", you can set a threshold 'bcm_theta' (default `0.0`).

    If `auto synapse_normalization = True`, then after each local-rule update
    on a row i of W, the row is rescaled to unit L2 norm. [4]_



    Parameters
    ----------
    units : int, optional
        Number of reservoir units. If None, the number of units will be inferred from
        the ``W`` matrix shape.
    local_rule : str, default to `oja`
        One of `"oja"`, `"anti-oja"`, `"hebbian"`, `"anti-hebbian"`, `"bcm"`.
    eta : float, default to 1e-3.
        Local learning rate for the weight update.
    bcm_theta : float, default to 0.0
        The threshold used in the "bcm" rule.
    synapse_normalization : bool, default to True
        If True, L2-normalize each row of W after its update.
    epochs : int, default to 1
        Number of training iterations.
    sr : float, optional
        Spectral radius of recurrent weight matrix.
    lr : float or array-like of shape (units,), default to 1.0
        Neurons leak rate. Must be in :math:`[0, 1]`.
    input_bias : bool, default to True
        If False, no bias is added to inputs.
    noise_rc : float, default to 0.0
        Gain of noise applied to reservoir activations.
    noise_in : float, default to 0.0
        Gain of noise applied to input inputs.
    noise_fb : float, default to 0.0
        Gain of noise applied to feedback signal.
    noise_type : str, default to "normal"
        Distribution of noise. Must be a Numpy random variable generator
        distribution (see :py:class:`numpy.random.Generator`).
    noise_kwargs : dict, optional
        Keyword arguments to pass to the noise generator, such as `low` and `high`
        values of uniform distribution.
    input_scaling : float or array-like of shape (features,), default to 1.0.
        Input gain. An array of the same dimension as the inputs can be used to
        set up different input scaling for each feature.
    bias_scaling: float, default to 1.0
        Bias gain.
    fb_scaling : float or array-like of shape (features,), default to 1.0
        Feedback gain. An array of the same dimension as the feedback can be used to
        set up different feedback scaling for each feature.
    input_connectivity : float, default to 0.1
        Connectivity of input neurons, i.e. ratio of input neurons connected
        to reservoir neurons. Must be in :math:`]0, 1]`.
    rc_connectivity : float, default to 0.1
        Connectivity of recurrent weight matrix, i.e. ratio of reservoir
        neurons connected to other reservoir neurons, including themselves.
        Must be in :math:`]0, 1]`.
    fb_connectivity : float, default to 0.1
        Connectivity of feedback neurons, i.e. ratio of feedback neurons
        connected to reservoir neurons. Must be in :math:`]0, 1]`.
    Win : callable or array-like of shape (units, features), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Input weights matrix or initializer. If a callable (like a function) is
        used,
        then this function should accept any keywords
        parameters and at least two parameters that will be used to define the
        shape of
        the returned weight matrix.
    W : callable or array-like of shape (units, units), default to :py:func:`~reservoirpy.mat_gen.uniform`
        Recurrent weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the
        shape of
        the returned weight matrix.
    Wfb : callable or array-like of shape (units, feedback), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Feedback weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the
        shape of
        the returned weight matrix.
    bias : callable or array-like of shape (units, 1), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Bias weights vector or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the
        shape of
        the returned weight matrix.
    feedback_dim : int, optional
        Feedback dimension. Can be inferred at first call.
    fb_activation : str or callable, default to :py:func:`~reservoirpy.activationsfunc.identity`
        Feedback activation function.
        - If a str, should be a :py:mod:`~reservoirpy.activationsfunc`
        function name.
        - If a callable, should be an element-wise operator on arrays.
    activation : {"tanh", "sigmoid"}, default to "tanh"
        Reservoir units activation function.
    input_dim : int, optional
        Input dimension. Can be inferred at first call.
    name : str, optional
        Node name.
    dtype : Numpy dtype, default to np.float64
        Numerical type for node parameters.
    seed : int or :py:class:`numpy.random.Generator`, optional
        A random state seed, for noise generation.


    References
    ----------

    .. [1] Babinec, Š., & Pospíchal, J. (2007). Improving the prediction
           accuracy of echo state neural networks by anti-Oja’s learning.
           In International Conference on Artificial Neural Networks (pp. 19-28).
           Berlin, Heidelberg: Springer Berlin Heidelberg.
           https://doi.org/10.1007/978-3-540-74690-4_3

    .. [2] Yusoff, M. H., Chrol-Cannon, J., & Jin, Y. (2016). Modeling neural
           plasticity in echo state networks for classification and regression.
           Information Sciences, 364, 184-196.
           https://doi.org/10.1016/j.ins.2015.11.017

    .. [3] Morales, G. B., Mirasso, C. R., & Soriano, M. C. (2021). Unveiling
           the role of plasticity rules in reservoir computing. Neurocomputing,
           461, 705-715. https://doi.org/10.1016/j.neucom.2020.05.127

    .. [4] Wang, X., Jin, Y., & Hao, K. (2021). Synergies between synaptic and
           intrinsic plasticity in echo state networks. Neurocomputing,
           432, 32-43. https://doi.org/10.1016/j.neucom.2020.12.007

    Example
    -------
    >>> auto reservoir = LocalPlasticityReservoir(
    ...     auto units = 100, sr=0.9, local_rule="hebbian",
    ...     auto eta = 1e-3, epochs=5, synapse_normalization=True
    ... )
    >>> //  Fit on data timeseries
    >>> reservoir.fit(X_data, auto warmup = 10)
    >>> //  Then run
    >>> auto states = reservoir.run(X_data)
    
*/

    auto seed(auto self, auto units: Optional[int] = None, std::string //  local rule choice
        local_rule: std::string str = "oja", auto eta: float = 1e-3, double bcm_theta: float = 0.0, bool synapse_normalization: bool = false, int epochs: int = 1, auto //  standard reservoir params
        sr: Optional[float] = None, double lr: auto float = 1.0, bool input_bias: bool = true, double noise_rc: float = 0.0, double noise_in: float = 0.0, double noise_fb: float = 0.0, std::string noise_type: str = "normal", auto noise_kwargs: Optional[Dict] = None, auto input_scaling, double Sequence] = 1.0, double bias_scaling: float = 1.0, auto fb_scaling, double Sequence] = 1.0, double input_connectivity: Optional[float] = 0.1, double rc_connectivity: Optional[float] = 0.1, double fb_connectivity: Optional[float] = 0.1, auto Win, auto Callable] = bernoulli, auto W, auto Callable] = uniform, auto Wfb, auto Callable] = bernoulli, auto bias, auto Callable] = bernoulli, auto feedback_dim: Optional[int] = None, auto fb_activation, auto Callable] = identity, auto activation, auto Callable] = tanh, auto name = None, auto seed = None, auto **kwargs, auto ) {
        if (units is None and not is_array(W)) {
            raise ValueError(
                "'units' parameter must not be None if 'W' parameter is not a matrix."
            )

        auto rng = rand_generator(seed=seed)
        auto noise_kwargs = dict() if noise_kwargs is None else noise_kwargs

        //  Validate local rule name
        auto local_rule = local_rule.lower()

        if (auto local_rule = = "oja") {

            auto increment(auto weights, auto pre_state, auto post_state) {
                return eta * post_state * (pre_state - post_state * weights)

        elif (auto local_rule = = "anti-oja") {

            auto increment(auto weights, auto pre_state, auto post_state) {
                return -eta * post_state * (pre_state - post_state * weights)

        elif (auto local_rule = = "hebbian") {

            auto increment(auto weights, auto pre_state, auto post_state) {
                return eta * post_state * pre_state

        elif (auto local_rule = = "anti-hebbian") {

            auto increment(auto weights, auto pre_state, auto post_state) {
                return -eta * post_state * pre_state

        elif (auto local_rule = = "bcm") {

            auto increment(auto weights, auto pre_state, auto post_state) {
                return eta * post_state * (post_state - bcm_theta) * pre_state

        } else {
            raise ValueError(
                f"Unknown learning rule '{local_rule}'. Choose from: "
                "['oja', 'anti-oja', 'hebbian', 'anti-hebbian', 'bcm']."
            )

        self.auto increment = increment

        super(LocalPlasticityReservoir, self).__init__(
            auto fb_initializer = partial(
                initialize_feedback,
                auto Wfb_init = Wfb,
                auto fb_scaling = fb_scaling,
                auto fb_connectivity = fb_connectivity,
                auto seed = seed,
            ),
            auto params = {
                "W": None,
                "Win": None,
                "Wfb": None,
                "bias": None,
                "internal_state": None,
            },
            auto hypers = {
                "local_rule": local_rule,
                "bcm_theta": bcm_theta,
                "eta": eta,
                "synapse_normalization": synapse_normalization,
                "sr": sr,
                "lr": lr,
                "epochs": epochs,
                "input_bias": input_bias,
                "input_scaling": input_scaling,
                "fb_scaling": fb_scaling,
                "rc_connectivity": rc_connectivity,
                "input_connectivity": input_connectivity,
                "fb_connectivity": fb_connectivity,
                "noise_in": noise_in,
                "noise_rc": noise_rc,
                "noise_out": noise_fb,
                "noise_type": noise_type,
                "activation": get_function(activation)
                if isinstance(activation, str)
                else activation,
                "fb_activation": get_function(fb_activation)
                if isinstance(fb_activation, str)
                else fb_activation,
                "units": units,
                "noise_generator": partial(noise, auto rng = rng, **noise_kwargs),
            },
            auto forward = forward_external,
            auto initializer = partial(
                initialize_synaptic_plasticity,
                auto input_bias = input_bias,
                auto bias_scaling = bias_scaling,
                auto sr = sr,
                auto input_scaling = input_scaling,
                auto input_connectivity = input_connectivity,
                auto rc_connectivity = rc_connectivity,
                auto W_init = W,
                auto Win_init = Win,
                auto bias_init = bias,
                auto seed = seed,
            ),
            auto backward = sp_backward,
            auto output_dim = units,
            auto feedback_dim = feedback_dim,
            auto name = name,
            **kwargs,
        )

    @property
    bool fitted(auto self) {
        //  For an unsupervised node that can always be updated,
        //  we set `auto fitted = True` after first initialization/training.
        return True

    auto _(auto self, auto X_batch, auto auto Y_batch = None, int warmup = 0, auto **kwargs
    ) -> "LocalPlasticityReservoir", auto auto _ = check_xy(self, auto X_batch, auto allow_n_inputs = False)
        X, auto auto _ = _init_with_sequences(self, auto X)

        self.initialize_buffers()

        for i in range(len(X)) {
            auto X_seq = X[i]

            if (X_seq.shape[0] <= warmup) {
                raise ValueError(
                    f"Warmup set to {warmup} timesteps, "
                    f"but one timeseries is only {X_seq.shape[0]} long."
                )

            //  Run warmup if specified
            if (warmup > 0) {
                self.run(X_seq[:warmup])

            self._partial_backward(self, X_seq[warmup:])

        return self


#endif // RESERVOIRCPP_LOCAL_PLASTICITY_RESERVOIR_HPP
