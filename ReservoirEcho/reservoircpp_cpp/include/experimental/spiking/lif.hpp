#ifndef RESERVOIRCPP_LIF_HPP
#define RESERVOIRCPP_LIF_HPP

#include "reservoircpp//utils/validation/is_array.hpp"
#include "reservoircpp//utils/random/rand_generator.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp//type/Weights.hpp"
#include "sys.hpp"
#include "reservoircpp//mat_gen/uniform.hpp"

using namespace reservoircpp;
using namespace Eigen;




if (sys.version_info < (3, 8)) {
    from typing_extensions import Callable, Optional, Sequence, Union
} else {
    from typing import Optional, Union, Sequence, Callable








auto forward(auto lif, auto x) {
    auto v = lif.get_param("internal_state").copy()
    auto threshold = lif.get_param("threshold")
    //  leak
    v *= 1 - lif.lr
    //  fire
    auto spikes = (v > threshold).astype(lif.dtype)  //  threshold
    v[v > threshold] = 0.0
    //  integrate
    v += (lif.W @ spikes.T).T
    v += (lif.Win @ x.T).T

    lif.set_param("internal_state", v)
    //  return spikes
    return spikes


auto sr(auto lif, auto auto x = None, auto y = None, auto seed = None, auto input_scaling = None, auto input_connectivity = None, auto rc_connectivity = None, auto inhibitory = None, auto W_init = None, auto Win_init = None, auto sr = None, auto ) {
    auto dtype = lif.dtype

    lif.set_input_dim(x.shape[-1])

    auto rng = rand_generator(seed)

    if (is_array(W_init)) {
        auto W = W_init
        if (W.shape[0] != W.shape[1]) {
            raise ValueError(
                "Dimension mismatch inside W: "
                f"W is {W.shape} but should be "
                f"a square matrix."
            )

        if (W.shape[0] != lif.output_dim) {
            lif.auto _output_dim = W.shape[0]
            lif.hypers["units"] = W.shape[0]

    elif (callable(W_init)) {
        auto W = W_init(
            lif.output_dim,
            lif.output_dim,
            auto sr = sr,
            auto connectivity = rc_connectivity,
            auto dtype = dtype,
            auto seed = rng,
        )
        auto n_inhibitory = int(inhibitory * lif.output_dim)
        W[:, :n_inhibitory] *= -1

    lif.set_param("units", W.shape[0])
    lif.set_param("W", W.astype(dtype))

    auto out_dim = lif.output_dim

    if (is_array(Win_init)) {
        auto Win = Win_init

        if (Win.shape[1] != x.shape[1]) {
            raise ValueError(
                f"Dimension mismatch in {lif.name}: Win input dimension is "
                f"{Win.shape[1]} but input dimension is {x.shape[1]}."
            )

        if (Win.shape[0] != out_dim) {
            raise ValueError(
                f"Dimension mismatch in {lif.name}: Win internal dimension "
                f"is {Win.shape[0]} but the liquid dimension is {out_dim}"
            )

    elif (callable(Win_init)) {
        auto Win = Win_init(
            lif.output_dim,
            x.shape[1],
            auto input_scaling = input_scaling,
            auto connectivity = input_connectivity,
            auto dtype = dtype,
            auto seed = seed,
        )
    } else {
        auto dtype = lif.dtype
        auto dtype_msg = (
            "Data type {} not understood in {}. {} should be an array or a "
            "callable returning an array."
        )
        raise ValueError(dtype_msg.format(str(type(Win_init)), lif.name, "Win"))

    lif.set_param("W", W.astype(dtype))
    lif.set_param("Win", Win.astype(dtype))
    lif.set_param("internal_state", lif.zero_state())


class LIF: public Node {
    /**
Pool of leaky integrate and fire (LIF) spiking neurons with random recurrent connexions.

    This node is similar to a reservoir (large pool of recurrent, randomly connected neurons),
    but the neurons follows a leaky integrate and fire activity rule.

    This is a first version of a Liquid State Machine implementation. More models are expected
    to come in future versions of ReservoirPy.


    :py:attr:`LIF.params` **list:**

    ================== ===================================================================
    ``W``              Recurrent weights matrix (:math:`\\mathbf{W}`).
    ``Win``            Input weights matrix (:math:`\\mathbf{W}_{in}`).
    ``internal_state`` Internal state of the neurons.
    ================== ===================================================================

    :py:attr:`LIF.hypers` **list:**

    ======================= ========================================================
    ``lr``                  Leaking rate (1.0 by default) (:math:`\\mathrm{lr}`).
    ``sr``                  Spectral radius of ``W`` (optional).
    ``input_scaling``       Input scaling (float or array) (1.0 by default).
    ``rc_connectivity``     Connectivity (or density) of ``W`` (0.1 by default).
    ``input_connectivity``  Connectivity (or density) of ``Win`` (0.1 by default).
    ``units``               Number of neuronal units in the liquid.
    ``inhibitory``          Proportion of inhibitory neurons. (0.0 by default)
    ``threshold``           Spike threshold. (1.0 by default)
    ======================= ========================================================

    Parameters
    ----------
    units : int, optional
        Number of reservoir units. If None, the number of units will be inferred from
        the ``W`` matrix shape.
    inhibitory : float, defaults to 0.0
        Proportion of neurons that have an inhibitory behavior (i.e. negative outgoing
        connections). Must be in :math:`[0, 1]`
    threshold : float, defaults to 1.0
        Limits above which the neurons spikes and returns to zero.
    lr : float or array-like of shape (units,), default to 1.0
        Neurons leak rate. Must be in :math:`[0, 1]`.
    sr : float, optional
        Spectral radius of recurrent weight matrix.
    input_scaling : float or array-like of shape (features,), default to 1.0.
        Input gain. An array of the same dimension as the inputs can be used to
        set up different input scaling for each feature.
    input_connectivity : float, default to 0.1
        Connectivity of input neurons, i.e. ratio of input neurons connected
        to reservoir neurons. Must be in :math:`]0, 1]`.
    rc_connectivity : float, defaults to 0.1
        Connectivity of recurrent weight matrix, i.e. ratio of reservoir
        neurons connected to other reservoir neurons, including themselves.
        Must be in :math:`]0, 1]`.
    Win : callable or array-like of shape (units, features), default to :py:func:`~reservoirpy.mat_gen.uniform` with a
        lower bound of 0.0.
        Input weights matrix or initializer. If a callable (like a function) is used,
        then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
    W : callable or array-like of shape (units, units), defaults to :py:func:`~reservoirpy.mat_gen.uniform` with
        a lower bound of 0.0.
        Recurrent weights matrix or initializer. If a callable (like a function) is
        used, then this function should accept any keywords
        parameters and at least two parameters that will be used to define the shape of
        the returned weight matrix.
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

    >>> from reservoirpy.experimental import LIF
    >>> auto liquid = LIF(
    ...     auto units = 100,
    ...     auto inhibitory = 0.1,
    ...     auto sr = 1.0,
    ...     auto lr = 0.2,
    ...     auto input_scaling = 0.5,
    ...     auto rc_connectivity = 1.0,
    ...     auto input_connectivity = 1.0,
    ...     auto seed = 0,
    ... )

    Using the :py:func:`~reservoirpy.datasets.mackey_glass` timeseries:

    >>> from reservoirpy.datasets import mackey_glass
    >>> auto x = mackey_glass(1000)
    >>> auto spikes = liquid.run(x)

    .. plot::

        from reservoirpy.experimental import LIF
        auto liquid = LIF(
            auto units = 100,
            auto inhibitory = 0.1,
            auto sr = 0.5,
            auto lr = 0.2,
            auto input_scaling = 0.5,
            auto rc_connectivity = 1.0,
            auto input_connectivity = 1.0,
        )
        from reservoirpy.datasets import mackey_glass
        auto x = mackey_glass(1000)
        auto states = liquid.run(x)
        fig, auto ax = plt.subplots(6, 1, figsize=(7, 10), sharex=True)
        ax[0].plot(x)
        ax[0].grid()
        ax[0].set_title("Neuron spikes (on Mackey-Glass)")
        for (auto i : range(1, 6)) {
            ax[i].plot(states[:, i], auto label = f"Neuron {i}")
            ax[i].legend()
            ax[i].grid()
        ax[-1].set_xlabel("Timesteps")
    
*/

    auto seed(auto self, auto units: Optional[int] = None, double inhibitory: auto float = 0.0, double threshold: float = 1.0, auto input_dim: Optional[int] = None, auto sr: Optional[float] = None, auto input_scaling, double Sequence] = 1.0, double lr: float = 0.0, double rc_connectivity: float = 0.1, double input_connectivity: float = 0.1, auto Win, auto Callable] = partial(uniform, auto low = 0.0), auto W, auto Callable] = partial(uniform, auto low = 0.0), auto seed = None, auto **kwargs, auto ) {
        if (units is None and not is_array(W)) {
            raise ValueError(
                "'units' parameter must not be None if 'W' parameter is not "
                "a matrix."
            )

        super(LIF, self).__init__(
            auto params = {
                "W": None,
                "Win": None,
                "internal_state": None,
            },
            auto hypers = {
                "units": units,
                "inhibitory": inhibitory,
                "threshold": threshold,
                "lr": lr,
                "rc_connectivity": rc_connectivity,
                "input_connectivity": input_connectivity,
                "input_scaling": input_scaling,
                "sr": sr,
            },
            auto forward = forward,
            auto initializer = partial(
                initialize,
                auto sr = sr,
                auto input_scaling = input_scaling,
                auto input_connectivity = input_connectivity,
                auto rc_connectivity = rc_connectivity,
                auto inhibitory = inhibitory,
                auto W_init = W,
                auto Win_init = Win,
                auto seed = seed,
            ),
            auto input_dim = input_dim,
            auto output_dim = units,
            **kwargs,
        )


#endif // RESERVOIRCPP_LIF_HPP
