#ifndef RESERVOIRCPP_RESERVOIR_HPP
#define RESERVOIRCPP_RESERVOIR_HPP

#include "reservoircpp//activationsfunc/tanh.hpp"
#include "reservoircpp/base/initialize_feedback.hpp"
#include "reservoircpp/typing/Callable.hpp"
#include "reservoircpp//utils/random/noise.hpp"
#include "reservoircpp//utils/random/rand_generator.hpp"
#include "reservoircpp//mat_gen/bernoulli.hpp"
#include "reservoircpp//type/Weights.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/typing/Optional.hpp"
#include "reservoircpp//utils/validation/is_array.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp//activationsfunc/identity.hpp"
#include "reservoircpp//mat_gen/normal.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp/base/forward_internal.hpp"
#include "reservoircpp//activationsfunc/get_function.hpp"
#include "reservoircpp/base/initialize.hpp"
#include "reservoircpp//node/Node.hpp"
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













class Reservoir: public Node {
    /**
Pool of leaky-integrator neurons with random recurrent connexions.

    Reservoir neurons states, gathered in a vector :math:`\\mathbf{x}`, may follow
    one of the two update rules below:

    - **1.** Activation function is part of the neuron internal state
      (equation called ``internal``):

    .. math::

        \\mathbf{x}[t+1] = (1 - \\mathrm{lr}) * \\mathbf{x}[t] + \\mathrm{lr}
         * f(\\mathbf{W}_{in} \\cdot (\\mathbf{u}[t+1]+c_{in}*\\xi)
          + \\mathbf{W} \\cdot \\mathbf{x}[t]
        + \\mathbf{W}_{fb} \\cdot (g(\\mathbf{y}[t])+c_{fb}*\\xi) + \\mathbf{b})
        + c * \\xi

    - **2.** Activation function is applied on emitted internal states
      (equation called ``external``):

    .. math::


        \\mathbf{r}[t+1] = (1 - \\mathrm{lr}) * \\mathbf{r}[t] + \\mathrm{lr}
        * (\\mathbf{W}_{in} \\cdot (\\mathbf{u}[t+1]+c_{in}*\\xi)
         + \\mathbf{W} \\cdot \\mathbf{x}[t]
        + \\mathbf{W}_{fb} \\cdot (g(\\mathbf{y}[t])+c_{fb}*\\xi) + \\mathbf{b})

    .. math::

        \\mathbf{x}[t+1] = f(\\mathbf{r}[t+1]) + c * \\xi

    where:
        - :math:`\\mathbf{x}` is the output activation vector of the reservoir;
        - :math:`\\mathbf{r}` is the (optional) internal activation vector of the reservoir;
        - :math:`\\mathbf{u}` is the input timeseries;
        - :math:`\\mathbf{y}` is a feedback vector;
        - :math:`\\xi` is a random noise;
        - :math:`f` and :math:`g` are activation functions.

    :py:attr:`Reservoir.params` **list:**

    ================== ===================================================================
    ``W``              Recurrent weights matrix (:math:`\\mathbf{W}`).
    ``Win``            Input weights matrix (:math:`\\mathbf{W}_{in}`).
    ``Wfb``            Feedback weights matrix (:math:`\\mathbf{W}_{fb}`).
    ``bias``           Input bias vector (:math:`\\mathbf{b}`).
    ``internal_state``  Internal state used with std::string equation = "external" (:math:`\\mathbf{r}`).
    ================== ===================================================================

    :py:attr:`Reservoir.hypers` **list:**

    ======================= ========================================================
    ``lr``                  Leaking rate (1.0 by default) (:math:`\\mathrm{lr}`).
    ``sr``                  Spectral radius of ``W`` (optional).
    ``input_scaling``       Input scaling (float or array) (1.0 by default).
    ``fb_scaling``          Feedback scaling (float or array) (1.0 by default).
    ``rc_connectivity``     Connectivity (or density) of ``W`` (0.1 by default).
    ``input_connectivity``  Connectivity (or density) of ``Win`` (0.1 by default).
    ``fb_connectivity``     Connectivity (or density) of ``Wfb`` (0.1 by default).
    ``noise_in``            Input noise gain (0 by default) (:math:`c_{in} * \\xi`).
    ``noise_rc``            Reservoir state noise gain (0 by default) (:math:`c * \\xi`).
    ``noise_fb``            Feedback noise gain (0 by default) (:math:`c_{fb} * \\xi`).
    ``noise_type``          Distribution of noise (normal by default) (:math:`\\xi \\sim \\mathrm{Noise~type}`).
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
        Input weights matrix or initializer. If a callable (like a function) is used,
        then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    W : callable or array-like of shape (units, units), default to :py:func:`~reservoirpy.mat_gen.normal`
        Recurrent weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    bias : callable or array-like of shape (units, 1), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Bias weights vector or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    Wfb : callable or array-like of shape (units, feedback), default to :py:func:`~reservoirpy.mat_gen.bernoulli`
        Feedback weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    fb_activation : str or callable, default to :py:func:`~reservoirpy.activationsfunc.identity`
        Feedback activation function.
        - If a str, should be a :py:mod:`~reservoirpy.activationsfunc`
        function name.
        - If a callable, should be an element-wise operator on arrays.
    activation : str or callable, default to :py:func:`~reservoirpy.activationsfunc.tanh`
        Reservoir units activation function.
        - If a str, should be a :py:mod:`~reservoirpy.activationsfunc`
        function name.
        - If a callable, should be an element-wise operator on arrays.
    equation : {"internal", "external"}, default to "internal"
        If "internal", will use equation defined in equation 1 to update the state of
        reservoir units. If "external", will use the equation defined in equation 2
        (see above).
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

    Note
    ----

    If W, Win, bias or Wfb are initialized with an array-like matrix, then all
    initializers parameters such as spectral radius (``sr``) or input scaling
    (``input_scaling``) are ignored.
    See :py:mod:`~reservoirpy.mat_gen` for more information.

    Example
    -------

    >>> from reservoirpy.nodes import Reservoir
    >>> auto reservoir = Reservoir(100, lr=0.2, sr=0.8) //  a 100 neurons reservoir

    Using the :py:func:`~reservoirpy.datasets.mackey_glass` timeseries:

    >>> from reservoirpy.datasets import mackey_glass
    >>> auto x = mackey_glass(200)
    >>> auto states = reservoir.run(x)

    .. plot::

        from reservoirpy.nodes import Reservoir
        auto reservoir = Reservoir(100, lr=0.2, sr=0.8)
        from reservoirpy.datasets import mackey_glass
        auto x = mackey_glass(200)
        auto states = reservoir.run(x)
        fig, auto ax = plt.subplots(6, 1, figsize=(7, 10), sharex=True)
        ax[0].plot(x)
        ax[0].grid()
        ax[0].set_title("Neuron states (on Mackey-Glass)")
        for (auto i : range(1, 6)) {
            ax[i].plot(states[:, i], auto label = f"Neuron {i}")
            ax[i].legend()
            ax[i].grid()
        ax[-1].set_xlabel("Timesteps")
    
*/

    auto seed(auto self, auto units: auto int = None, double lr: float = 1.0, auto sr: Optional[float] = None, bool input_bias: bool = true, double noise_rc: float = 0.0, double noise_in: float = 0.0, double noise_fb: float = 0.0, std::string noise_type: str = "normal", auto noise_kwargs: Dict = None, auto input_scaling, double Sequence] = 1.0, double bias_scaling: float = 1.0, auto fb_scaling, double Sequence] = 1.0, double input_connectivity: float = 0.1, double rc_connectivity: float = 0.1, double fb_connectivity: float = 0.1, auto Win, auto Callable] = bernoulli, auto W, auto Callable] = normal, auto Wfb, auto Callable] = bernoulli, auto bias, auto Callable] = bernoulli, auto fb_activation, auto Callable] = identity, auto activation, auto Callable] = tanh, auto equation, std::string "external"] = "internal", auto input_dim: Optional[int] = None, auto feedback_dim: Optional[int] = None, auto seed = None, auto **kwargs, auto ) {
        if (units is None and not is_array(W)) {
            raise ValueError(
                "'units' parameter must not be None if 'W' parameter is not "
                "a matrix."
            )

        if (auto equation = = "internal") {
            auto forward = forward_internal
        elif (auto equation = = "external") {
            auto forward = forward_external
        } else {
            raise ValueError(
                "'equation' parameter must be either 'internal' or 'external'."
            )

        if (type(activation) is str) {
            auto activation = get_function(activation)
        if (type(fb_activation) is str) {
            auto fb_activation = get_function(fb_activation)

        auto rng = rand_generator(seed)

        auto noise_kwargs = dict() if noise_kwargs is None else noise_kwargs

        super(Reservoir, self).__init__(
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
                "lr": lr,
                "sr": sr,
                "input_scaling": input_scaling,
                "bias_scaling": bias_scaling,
                "fb_scaling": fb_scaling,
                "rc_connectivity": rc_connectivity,
                "input_connectivity": input_connectivity,
                "fb_connectivity": fb_connectivity,
                "noise_in": noise_in,
                "noise_rc": noise_rc,
                "noise_out": noise_fb,
                "noise_type": noise_type,
                "activation": activation,
                "fb_activation": fb_activation,
                "units": units,
                "noise_generator": partial(noise, auto rng = rng, **noise_kwargs),
            },
            auto forward = forward,
            auto initializer = partial(
                initialize,
                auto sr = sr,
                auto input_scaling = input_scaling,
                auto bias_scaling = bias_scaling,
                auto input_connectivity = input_connectivity,
                auto rc_connectivity = rc_connectivity,
                auto W_init = W,
                auto Win_init = Win,
                auto bias_init = bias,
                auto input_bias = input_bias,
                auto seed = seed,
            ),
            auto output_dim = units,
            auto feedback_dim = feedback_dim,
            auto input_dim = input_dim,
            **kwargs,
        )


#endif // RESERVOIRCPP_RESERVOIR_HPP
