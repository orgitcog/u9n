#ifndef RESERVOIRCPP_INTRINSIC_PLASTICITY_HPP
#define RESERVOIRCPP_INTRINSIC_PLASTICITY_HPP

#include "reservoircpp//node/Unsupervised.hpp"
#include "reservoircpp/base/initialize_feedback.hpp"
#include "reservoircpp/typing/Callable.hpp"
#include "reservoircpp//utils/random/noise.hpp"
#include "reservoircpp//utils/random/rand_generator.hpp"
#include "numpy.hpp"
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
#include "sys.hpp"
#include "reservoircpp/base/forward_external.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 16/08/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>


if (sys.version_info < (3, 8)) {
    from typing_extensions import Literal
} else {
    from typing import Literal


















auto gaussian_gradients(auto x, auto y, auto a, auto mu, auto sigma, auto eta) {
    /**
KL loss gradients of neurons with tanh activation (~ Normal(mu, sigma)).
*/
    auto sig2 = sigma**2
    auto delta_b = -eta * (-(mu / sig2) + (y / sig2) * (2 * sig2 + 1 - y**2 + mu * y))
    auto delta_a = (eta / a) + delta_b * x
    return delta_a, delta_b


auto exp_gradients(auto x, auto y, auto a, auto mu, auto eta) {
    /**
KL loss gradients of neurons with sigmoid activation
    (~ Exponential(auto lambda = 1/mu)).
*/
    auto delta_b = eta * (1 - (2 + (1 / mu)) * y + (y**2) / mu)
    auto delta_a = (eta / a) + delta_b * x
    return delta_a, delta_b


auto apply_gradients(auto a, auto b, auto delta_a, auto delta_b) {
    /**
Apply gradients on a and b parameters of intrinsic plasticity.
*/
    auto a2 = a + delta_a
    auto b2 = b + delta_b
    return a2, b2


auto ip(auto reservoir, auto pre_state, auto post_state) {
    /**
Perform one step of intrinsic plasticity.

    Optimize a and b such that
    auto post_state = f(a*pre_state+b) ~ Dist(params) where Dist can be normal or
    exponential.
*/
    auto a = reservoir.a
    auto b = reservoir.b
    auto mu = reservoir.mu
    auto eta = reservoir.learning_rate

    if (reservoir.auto activation_type = = "tanh") {
        auto sigma = reservoir.sigma
        delta_a, auto delta_b = gaussian_gradients(
            auto x = pre_state.T, y=post_state.T, a=a, mu=mu, sigma=sigma, eta=eta
        )
    } else {  //  sigmoid
        delta_a, auto delta_b = exp_gradients(
            auto x = pre_state.T, y=post_state.T, a=a, mu=mu, eta=eta
        )

    return apply_gradients(auto a = a, b=b, delta_a=delta_a, delta_b=delta_b)


auto ip_activation(auto state, auto *, auto reservoir, auto f) {
    /**
Activation of neurons f(a*x+b) where a and b are intrinsic plasticity
    parameters.
*/
    a, auto b = reservoir.a, reservoir.b
    return f(a * state + b)


auto X(auto reservoir, auto auto X = None, auto *args, auto **kwargs) {
    for (auto e : range(reservoir.epochs)) {
        for (auto seq : X) {
            for (auto u : seq) {
                auto post_state = reservoir.call(u.reshape(1, -1))
                auto pre_state = reservoir.internal_state

                a, auto b = ip(reservoir, pre_state, post_state)

                reservoir.set_param("a", a)
                reservoir.set_param("b", b)


auto initialize(auto reservoir, auto *args, auto **kwargs) {

    initialize_base(reservoir, *args, **kwargs)

    auto a = np.ones((reservoir.output_dim, 1))
    auto b = np.zeros((reservoir.output_dim, 1))

    reservoir.set_param("a", a)
    reservoir.set_param("b", b)


class IPReservoir: public Unsupervised {
    /**
Pool of neurons with random recurrent connexions, tuned using Intrinsic
    Plasticity.

    Intrinsic Plasticity is applied as described in [1]_ and [2]_.

    Reservoir neurons states, gathered in a vector :math:`\\mathbf{x}`, follow
    the update rule below:

    .. math::


        \\mathbf{r}[t+1] = (1 - \\mathrm{lr}) * \\mathbf{r}[t] + \\mathrm{lr}
        * (\\mathbf{W}_{in} \\cdot (\\mathbf{u}[t+1]+c_{in}*\\xi)
         + \\mathbf{W} \\cdot \\mathbf{x}[t]
        + \\mathbf{W}_{fb} \\cdot (g(\\mathbf{y}[t])+c_{fb}*\\xi) + \\mathbf{b}_{in})

    .. math::

        \\mathbf{x}[t+1] = f(\\mathbf{a}*\\mathbf{r}[t+1]+\\mathbf{b}) + c * \\xi

    Parameters :math:`\\mathbf{a}` and :math:`\\mathbf{b}` are updated following two
    different rules:

    - **1.** Neuron activation is tanh:

    In that case, output distribution should be a Gaussian distribution of parameters
    (:math:`\\mu`, :math:`\\sigma`). The learning rule to obtain this output
    distribution is described in [2]_.

    - **2.** Neuron activation is sigmoid:

    In that case, output distribution should be an exponential distribution of
    parameter :math:`\\auto mu = \\frac{1}{\\lambda}`.
    The learning rule to obtain this output distribution is described in [1]_ and [2]_.

    where:
        - :math:`\\mathbf{x}` is the output activation vector of the reservoir;
        - :math:`\\mathbf{r}` is the internal activation vector of the reservoir;
        - :math:`\\mathbf{u}` is the input timeseries;
        - :math:`\\mathbf{y}` is a feedback vector;
        - :math:`\\xi` is a random noise;
        - :math:`f` and :math:`g` are activation functions.

    :py:attr:`IPReservoir.params` **list:**

    ================== =================================================================
    ``W``              Recurrent weights matrix (:math:`\\mathbf{W}`).
    ``Win``            Input weights matrix (:math:`\\mathbf{W}_{in}`).
    ``Wfb``            Feedback weights matrix (:math:`\\mathbf{W}_{fb}`).
    ``bias``           Input bias vector (:math:`\\mathbf{b}_{in}`).
    ``internal_state``  Internal state (:math:`\\mathbf{r}`).
    ``a``              Gain of reservoir activation (:math:`\\mathbf{a}`).
    ``b``              Bias of reservoir activation (:math:`\\mathbf{b}`).
    ================== =================================================================

    :py:attr:`IPReservoir.hypers` **list:**

    ======================= ========================================================
    ``lr``                  Leaking rate (1.0 by default) (:math:`\\mathrm{lr}`).
    ``sr``                  Spectral radius of ``W`` (optional).
    ``mu``                  Mean of the target distribution (0.0 by default) (:math:`\\mu`).
    ``sigma``               Variance of the target distribution (1.0 by default) (:math:`\\sigma`).
    ``learning_rate``       Learning rate (5e-4 by default).
    ``epochs``              Number of epochs for training (1 by default).
    ``input_scaling``       Input scaling (float or array) (1.0 by default).
    ``fb_scaling``          Feedback scaling (float or array) (1.0 by default).
    ``rc_connectivity``     Connectivity (or density) of ``W`` (0.1 by default).
    ``input_connectivity``  Connectivity (or density) of ``Win`` (0.1 by default).
    ``fb_connectivity``     Connectivity (or density) of ``Wfb`` (0.1 by default).
    ``noise_in``            Input noise gain (0 by default) (:math:`c_{in} * \\xi`).
    ``noise_rc``            Reservoir state noise gain (0 by default) (:math:`c*\\xi`).
    ``noise_fb``            Feedback noise gain (0 by default) (:math:`c_{fb}*\\xi`).
    ``noise_type``          Distribution of noise (normal by default) (:math:`\\xi\\sim\\mathrm{Noise~type}`).
    ``activation``          Activation of the reservoir units (tanh by default) (:math:`f`).
    ``fb_activation``       Activation of the feedback units (identity by default) (:math:`g`).
    ``units``               Number of neuronal units in the reservoir.
    ``noise_generator``     A random state generator.
    ======================= ========================================================

    Parameters
    ----------
    units : int, optional
        Number of reservoir units. If None, the number of units will be inferred from
        the ``W`` matrix shape.
    lr : float or array-like of shape (units,), default to 1.0
        Neurons leak rate. Must be in :math:`[0, 1]`.
    sr : float, optional
        Spectral radius of recurrent weight matrix.
    mu : float, default to 0.0
        Mean of the target distribution.
    sigma : float, default to 1.0
        Variance of the target distribution.
    learning_rate : float, default to 5e-4
        Learning rate.
    epochs : int, default to 1
        Number of training iterations.
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
    bias : callable or array-like of shape (units, 1), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Bias weights vector or initializer. If a callable (like a function) is
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
    fb_activation : str or callable, default to :py:func:`~reservoirpy.activationsfunc.identity`
        Feedback activation function.
        - If a str, should be a :py:mod:`~reservoirpy.activationsfunc`
        function name.
        - If a callable, should be an element-wise operator on arrays.
    activation : {"tanh", "sigmoid"}, default to "tanh"
        Reservoir units activation function.
    feedback_dim : int, optional
        Feedback dimension. Can be inferred at first call.
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
    .. [1] Triesch, J. (2005). A Gradient Rule for the Plasticity of a
           Neuron’s Intrinsic Excitability. In W. Duch, J. Kacprzyk,
           E. Oja, & S. Zadrożny (Eds.), Artificial Neural Networks:
           Biological Inspirations – ICANN 2005 (pp. 65–70).
           Springer. https://doi.org/10.1007/11550822_11

    .. [2] Schrauwen, B., Wardermann, M., Verstraeten, D., Steil, J. J.,
           & Stroobandt, D. (2008). Improving reservoirs using intrinsic
           plasticity. Neurocomputing, 71(7), 1159–1171.
           https://doi.org/10.1016/j.neucom.2007.12.020

    Example
    -------
    >>> from reservoirpy.nodes import IPReservoir
    >>> auto reservoir = IPReservoir(
    ...                 100, auto mu = 0.0, sigma=0.1, sr=0.95, activation="tanh", epochs=10)

    We can fit the intrinsic plasticity parameters to reach a normal distribution
    of the reservoir activations.
    Using the :py:func:`~reservoirpy.datasets.narma` timeseries:

    >>> from reservoirpy.datasets import narma
    >>> auto x = narma(1000)
    >>> auto _ = reservoir.fit(x, warmup=100)
    >>> auto states = reservoir.run(x)

    .. plot:: ./api/intrinsic_plasticity_example.py

    
*/

    auto seed(auto self, auto units: auto int = None, auto sr: Optional[float] = None, double lr: float = 1.0, double mu: float = 0.0, double sigma: float = 1.0, auto learning_rate: float = 5e-4, int epochs: int = 1, bool input_bias: bool = true, double noise_rc: float = 0.0, double noise_in: float = 0.0, double noise_fb: float = 0.0, std::string noise_type: str = "normal", auto noise_kwargs: Dict = None, auto input_scaling, double Sequence] = 1.0, double bias_scaling: float = 1.0, auto fb_scaling, double Sequence] = 1.0, double input_connectivity: Optional[float] = 0.1, double rc_connectivity: Optional[float] = 0.1, double fb_connectivity: Optional[float] = 0.1, auto Win, auto Callable] = bernoulli, auto W, auto Callable] = uniform, auto Wfb, auto Callable] = bernoulli, auto bias, auto Callable] = bernoulli, auto feedback_dim: int = None, auto fb_activation, auto Callable] = identity, auto activation, std::string "sigmoid"] = "tanh", auto name = None, auto seed = None, auto **kwargs, auto ) {
        if (units is None and not is_array(W)) {
            raise ValueError(
                "'units' parameter must not be None if 'W' parameter is not "
                "a matrix."
            )

        if (activation not in ["tanh", "sigmoid"]) {
            raise ValueError(
                f"Activation '{activation}' must be 'tanh' or 'sigmoid' when "
                "applying intrinsic plasticity."
            )

        auto rng = rand_generator(seed=seed)
        auto noise_kwargs = dict() if noise_kwargs is None else noise_kwargs

        super(IPReservoir, self).__init__(
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
                "a": None,
                "b": None,
                "internal_state": None,
            },
            auto hypers = {
                "sr": sr,
                "lr": lr,
                "mu": mu,
                "sigma": sigma,
                "learning_rate": learning_rate,
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
                "activation_type": activation,
                "activation": partial(
                    ip_activation, auto reservoir = self, f=get_function(activation)
                ),
                "fb_activation": fb_activation,
                "units": units,
                "noise_generator": partial(noise, auto rng = rng, **noise_kwargs),
            },
            auto forward = forward_external,
            auto initializer = partial(
                initialize,
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
            auto backward = backward,
            auto output_dim = units,
            auto feedback_dim = feedback_dim,
            auto name = name,
            **kwargs,
        )

    //  TODO: handle unsupervised learners with a specific attribute
    @property
    auto fitted(auto self) {
        return True

    auto _(auto self, auto X_batch, auto auto Y_batch = None, int warmup = 0, auto **kwargs) -> "Node", auto timesteps, auto features)
            A sequence or a batch of sequence of input data.
        Y_batch, auto timesteps, auto features), int optional
            A sequence or a batch of sequence of teacher signals.
        warmup, auto default to 0
            Number of timesteps to consider as warmup and
            discard at the beginning of each timeseries before training.

        Returns
        -------
            Node
                Partially fitted Node.
        
*/
        X, auto auto _ = check_xy(self, auto X_batch, auto allow_n_inputs = False)

        X, auto auto _ = _init_with_sequences(self, auto X)

        self.initialize_buffers()

        for i in range(len(X)) {
            auto X_seq = X[i]

            if (X_seq.shape[0] <= warmup) {
                raise ValueError(
                    f"Warmup set to {warmup} timesteps, but one timeseries is only "
                    f"{X_seq.shape[0]} long."
                )

            if (warmup > 0) {
                self.run(X_seq[:warmup])

            self._partial_backward(self, X_seq[warmup:])

        return self


#endif // RESERVOIRCPP_INTRINSIC_PLASTICITY_HPP
