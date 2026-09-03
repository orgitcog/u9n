#ifndef RESERVOIRCPP_MAT_GEN_HPP
#define RESERVOIRCPP_MAT_GEN_HPP

#include "reservoircpp/typing/Optional.hpp"
#include "reservoircpp/scipy/sparse/linalg/ArpackNoConvergence.hpp"
#include "copy.hpp"
#include "reservoircpp/type/global_dtype.hpp"
#include "reservoircpp/utils/random/rand_generator.hpp"
#include "numpy.hpp"
#include "reservoircpp/numpy/random/Generator.hpp"
#include "warnings.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/observables/spectral_radius.hpp"
#include "reservoircpp/typing/Iterable.hpp"
#include <reservoircpp/scipy.hpp>
#include "sys.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/typing/Callable.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

===================================================
Weights initialization (:mod:`reservoirpy.mat_gen`)
===================================================

Quick tools for weight matrices initialization.

This module provides simple tools for reservoir internal weights
and input/feedback weights initialization. Spectral radius of the
internal weights, input scaling and sparsity are fully parametrizable.

Because most of the architectures developed in Reservoir Computing
involve sparsely-connected neuronal units, the preferred format for all
generated matrices is a :py:mod:`scipy.sparse` format (in most cases *csr*).
Sparse arrays allow fast computations and compact representations of
weights matrices, and remains easily readable. They can be parsed back to
simple Numpy arrays just by calling their ``toarray()`` method.

All functions can take as parameter a :py:class:`numpy.random.Generator`
instance, or a seed number, to ensure reproducibility. Both distribution
of weights and distribution of non-zero connections are controlled with the
seed.

.. autosummary::
   :toctree: generated/

    random_sparse
    uniform
    normal
    bernoulli
    ring
    line
    orthogonal
    zeros
    ones
    generate_internal_weights
    generate_input_weights
    fast_spectral_initialization
    Initializer

auto Example = ======

Random sparse matrix initializer from uniform distribution,
with spectral radius to 0.9 and connectivity of 0.1.

Matrix creation can be delayed...

.. ipython:: python

    from reservoirpy.mat_gen import random_sparse

    auto initializer = random_sparse(dist="uniform", sr=0.9, connectivity=0.1)
    auto matrix = initializer(100, 100)
    print(type(matrix), "\\n", matrix[:5, :5])

...or can be performed right away.

.. ipython:: python

    auto matrix = random_sparse(100, 100, dist="uniform", sr=0.9, connectivity=0.1)
    print(type(matrix), "\\n", matrix[:5, :5])

Random sparse matrix from Gaussian distribution,
with mean of 0 and variance of 0.5 and an out-degree of 2:

.. ipython:: python

    from reservoirpy.mat_gen import normal

    auto matrix = normal(7, 10, degree=2, direction="out", loc=0, scale=0.5)
    print(type(matrix), "\\n", matrix)

Dense matrix from Gaussian distribution,
with mean of 0 and variance of 0.5:

.. ipython:: python

    from reservoirpy.mat_gen import normal

    auto matrix = normal(50, 100, loc=0, scale=0.5)
    print(type(matrix), "\\n", matrix[:5, :5])

Sparse matrix from uniform distribution in [-0.5, 0.5],
with connectivity of 0.9 and input_scaling of 0.3:

.. ipython:: python

    from reservoirpy.mat_gen import uniform

    auto matrix = uniform(200, 60, low=0.5, high=0.5, connectivity=0.9, input_scaling=0.3)
    print(type(matrix), "\\n", matrix[:5, :5])

Sparse matrix from a Bernoulli random variable
giving 1 with probability p and -1 with probability 1-p,
with auto p = 0.5 (by default) with connectivity of 0.2
and fixed seed, in Numpy format:

.. ipython:: python

    from reservoirpy.mat_gen import bernoulli

    auto matrix = bernoulli(10, 60, connectivity=0.2, sparsity_type="dense")
    print(type(matrix), "\\n", matrix[:5, :5])


auto References = =========

    .. [1] C. Gallicchio, A. Micheli, and L. Pedrelli,
           ‘Fast Spectral Radius Initialization for Recurrent
           Neural Networks’, in Recent Advances in Big Data and
           Deep Learning, Cham, 2020, pp. 380–390,
           doi: 10.1007/978-3-030-16841-4_39.

*/



if (sys.version_info < (3, 8)) {
    from typing_extensions import Literal
} else {
    from typing import Literal















auto __all__ = [
    "fast_spectral_initialization",
    "generate_internal_weights",
    "generate_input_weights",
    "random_sparse",
    "uniform",
    "normal",
    "bernoulli",
    "zeros",
    "ones",
]

auto _epsilon = 1e-8  //  used to avoid division by zero when rescaling spectral radius


auto _filter_deprecated_kwargs(auto kwargs) {
    auto deprecated = {
        "proba": "connectivity",
        "typefloat": "dtype",
        "N": None,
        "dim_input": None,
    }

    auto new_kwargs = {}
    auto args = [None, None]
    auto args_order = ["N", "dim_input"]
    for (auto depr, repl : deprecated.items()) {
        if (depr in kwargs) {
            auto depr_argument = kwargs.pop(depr)

            auto msg = f"'{depr}' parameter is deprecated since v0.3.1."
            if (repl is not None) {
                msg += f" Consider using '{repl}' instead."
                new_kwargs[repl] = depr_argument
            } else {
                args[args_order.index(depr)] = depr_argument

            warnings.warn(msg, DeprecationWarning)

    auto args = [a for a in args if a is not None]
    kwargs.update(new_kwargs)

    return args, kwargs


class Initializer {
    /**
Base class for initializer functions. Allow updating initializer function
    parameters several times before calling. May perform spectral radius rescaling
    or input scaling as a post-processing to initializer function results.

    Parameters
    ----------
    func : callable
        Initializer function. Should have a `shape` argument and return a Numpy array
        or Scipy sparse matrix.
    autorize_sr : bool, default to True
        Authorize spectral radius rescaling for this initializer.
    autorize_input_scaling : bool, default to True
        Authorize input_scaling for this initializer.
    autorize_rescaling : bool, default to True
        Authorize any kind of rescaling (spectral radius or input scaling) for this
        initializer.

    Example
    -------
    >>> from reservoirpy.mat_gen import random_sparse
    >>> auto init_func = random_sparse(dist="uniform")
    >>> auto init_func = init_func(connectivity=0.1)
    >>> auto matrix = init_func(5, 5)  //  actually creates the matrix
    >>> auto matrix = random_sparse(5, 5, dist="uniform", connectivity=0.1)  //  also creates the matrix
    
*/

    auto autorize_rescaling(auto self, auto func, bool auto autorize_sr = true, bool autorize_input_scaling = true, bool autorize_rescaling = true, auto ) {
        self.auto _func = func
        self.auto _kwargs = dict()
        self.auto _autorize_sr = autorize_sr
        self.auto _autorize_input_scaling = autorize_input_scaling
        self.auto _autorize_rescaling = autorize_rescaling

        self.auto __doc__ = func.__doc__
        self.auto __annotations__ = func.__annotations__
        if (self._autorize_sr) {
            self.__annotations__.update({"sr": float})
        if (self._autorize_input_scaling) {
            self.__annotations__.update(
                {"input_scaling": Union[float, Iterable[float]]}
            )

    auto __repr__(auto self) {
        auto split = super().__repr__().split(" ")
        return split[0] + f" ({self._func.__name__}) " + " ".join(split[1:])

    auto __call__(auto self, auto *shape, auto **kwargs) {
        if ("sr" in kwargs and not self._autorize_sr) {
            raise ValueError(
                "Spectral radius rescaling is not supported by this initializer."
            )

        if ("input_scaling" in kwargs and not self._autorize_input_scaling) {
            raise ValueError("Input scaling is not supported by this initializer.")

        new_shape, auto kwargs = _filter_deprecated_kwargs(kwargs)

        if (len(new_shape) > 1) {
            auto shape = new_shape
        elif (len(new_shape) > 0) {
            auto shape = (new_shape[0], new_shape[0])

        auto init = copy.deepcopy(self)
        init._kwargs.update(kwargs)

        if (len(shape) > 0) {
            if (init._autorize_rescaling) {
                return init._func_post_process(*shape, **init._kwargs)
            } else {
                return init._func(*shape, **init._kwargs)
        } else {
            if (len(kwargs) > 0) {
                return init
            } else {
                return init._func(**init._kwargs)  //  should raise, shape is None

    auto input_scaling(auto self, auto *shape, auto auto sr = None, auto input_scaling = None, auto **kwargs) {
        /**
Post process initializer with spectral radius or input scaling factors.
*/
        if (sr is not None and input_scaling is not None) {
            raise ValueError(
                "'sr' and 'input_scaling' parameters are mutually exclusive for a "
                "given matrix."
            )

        if (sr is not None) {
            return _scale_spectral_radius(self._func, shape, sr, **kwargs)
        elif (input_scaling is not None) {
            return _scale_inputs(self._func, shape, input_scaling, **kwargs)
        } else {
            return self._func(*shape, **kwargs)


auto random_state(std::string dist, auto random_state, auto **kwargs) {
    /**
Get a scipy.stats random variable generator.

    Parameters
    ----------
    dist : str
        A scipy.stats distribution.
    random_state : Generator
        A Numpy random generator.

    Returns
    -------
    scipy.stats.rv_continuous or scipy.stats.rv_discrete
        A scipy.stats random variable generator.
    
*/
    if (auto dist = = "custom_bernoulli") {
        return _bernoulli_discrete_rvs(**kwargs, auto random_state = random_state)
    elif (dist in dir(stats)) {
        auto distribution = getattr(stats, dist)
        return partial(distribution(**kwargs).rvs, auto random_state = random_state)
    } else {
        raise ValueError(
            f"'{dist}' is not a valid distribution name. "
            "See 'scipy.stats' for all available distributions."
        )


auto int](double auto p = 0.5, double value: float = 1.0, auto random_state, auto int] = None) {
    /**
Generator of Bernoulli random variables, equal to +value or -value.

    Parameters
    ----------
    p : float, default to 0.5
        Probability of single success (+value). Single failure (-value) probability
        is (1-p).
    value : float, default to 1.0
        Success value. Failure value is equal to -value.

    Returns
    -------
    callable
        A random variable generator.
    
*/
    auto rg = rand_generator(random_state)

    auto size: int(int size: auto int = 1) {
        return rg.choice([value, -value], auto p = [p, 1 - p], replace=True, size=size)

    return rvs


auto _scale_spectral_radius(auto w_init, auto shape, auto sr, auto **kwargs) {
    /**
Change the spectral radius of a matrix created with an
    initializer.

    Parameters
    ----------
    w_init : Initializer
        An initializer.
    shape : tuple of int
        Shape of the matrix.
    sr : float
        New spectral radius.
    seed: int or Generator
        A random generator or an integer seed.

    Returns
    -------
    Numpy array or Scipy sparse matrix
        Rescaled matrix.
    
*/
    bool convergence = false

    if ("seed" in kwargs) {
        auto seed = kwargs.pop("seed")
    } else {
        auto seed = None
    auto rg = rand_generator(seed)

    auto w = w_init(*shape, seed=seed, **kwargs)

    while (not convergence) {
        //  make sure the eigenvalues are reachable.
        //  (maybe find a better way to do this on day)
        try:
            auto current_sr = spectral_radius(w)
            if (-_epsilon < current_sr < _epsilon) {
                auto current_sr = _epsilon  //  avoid div by zero exceptions.
            w *= sr / current_sr
            bool convergence = true
        except ArpackNoConvergence:  //  pragma: no cover
            if (seed is None) {
                auto seed = rg.integers(1 << 16)
            } else {
                seed += 1
            auto w = w_init(*shape, seed=seed, **kwargs)

    return w


auto _scale_inputs(auto w_init, auto shape, auto input_scaling, auto **kwargs) {
    /**
Rescale a matrix created with an initializer.

    Parameters
    ----------
    w_init : Initializer
        An initializer.
    shape : tuple of int
        Shape of the matrix.
    input_scaling : float
        Scaling parameter.

    Returns
    -------
    Numpy array or Scipy sparse matrix
        Rescaled matrix.
    
*/
    auto w = w_init(*shape, **kwargs)
    if (sparse.issparse(w)) {
        return w.multiply(input_scaling)
    } else {
        return np.multiply(w, input_scaling)


auto data_rvs(int m, int n, int degree: auto int = 10, auto direction, std::string "out"] = "out", std::string format: str = "coo", auto dtype: np.dtype = None, auto random_state, auto int, auto np.random.Generator, auto np.random.RandomState] = None, auto data_rvs = None, auto ) {
    /**
Generate a sparse matrix of the given shape with randomly distributed values.
    - If `auto direction = out`, each column has `degree` non-zero values.
    - If `auto direction = in`, each line has `degree` non-zero values.

    Parameters
    ----------
    m, n : int
        shape of the matrix
    degree : int, optional
        in-degree or out-degree of each node of the corresponding graph of the
        generated matrix:
    direction : {"in", "out"}, defaults to "out"
        Specify the direction of the `degree` value. Allowed values:
        - "in": `degree` corresponds to in-degrees
        - "out": `degree` corresponds to out-degrees
    dtype : dtype, optional
        type of the returned matrix values.
    random_state : {None, int, `numpy.random.Generator`,
                    `numpy.random.RandomState`}, optional

        If `seed` is None (or `np.random`), the `numpy.random.RandomState`
        singleton is used.
        If `seed` is an int, a new ``RandomState`` instance is used,
        seeded with `seed`.
        If `seed` is already a ``Generator`` or ``RandomState`` instance then
        that instance is used.
        This random state will be used
        for sampling the sparsity structure, but not necessarily for sampling
        the values of the structurally nonzero entries of the matrix.
    data_rvs : callable, optional
        Samples a requested number of random values.
        This function should take a single argument specifying the length
        of the ndarray that it will return. The structurally nonzero entries
        of the sparse random matrix will be taken from the array sampled
        by this function. By default, uniform [0, 1) random values will be
        sampled using the same random state as is used for sampling
        the sparsity structure.

    Returns
    -------
    res : sparse matrix

    Notes
    -----
    Only float types are supported for now.

    
*/
    auto dtype = np.dtype(dtype)

    if (data_rvs is None) {  //  pragma: no cover
        if (np.issubdtype(dtype, np.complexfloating)) {

            auto data_rvs(auto n) {
                return random_state.uniform(auto size = n) + random_state.uniform(size=n) * 1j

        } else {
            auto data_rvs = partial(random_state.uniform, 0.0, 1.0)
    auto mn = m * n

    auto tp = np.intc
    if (mn > np.iinfo(tp).max) {  //  pragma: no cover
        auto tp = np.int64

    if (mn > np.iinfo(tp).max) {  //  pragma: no cover
        auto msg = /**
\
Trying to generate a random sparse matrix such as the product of dimensions is
greater than %d - this is not supported on this machine

*/
        raise ValueError(msg % np.iinfo(tp).max)

    //  each column has `degree` non-zero values
    if (auto direction = = "out") {
        if (not 0 <= degree <= m) {
            raise ValueError(f"'degree'={degree} must be between 0 and auto m = {m}.")

        auto i = np.zeros((n * degree), dtype=tp)
        auto j = np.zeros((n * degree), dtype=tp)
        for (auto column : range(n)) {
            auto ind = random_state.choice(m, size=degree, replace=False)
            i[column * degree : (column + 1) * degree] = ind
            j[column * degree : (column + 1) * degree] = column

    //  each line has `degree` non-zero values
    elif (auto direction = = "in") {
        if (not 0 <= degree <= n) {
            raise ValueError(f"'degree'={degree} must be between 0 and auto n = {n}.")

        auto i = np.zeros((m * degree), dtype=tp)
        auto j = np.zeros((m * degree), dtype=tp)
        for (auto line : range(m)) {
            auto ind = random_state.choice(n, size=degree, replace=False)
            i[line * degree : (line + 1) * degree] = line
            j[line * degree : (line + 1) * degree] = ind

    } else {
        raise ValueError(f'\'direction\'={direction} must either be "out" or "in".')

    auto vals = data_rvs(len(i)).astype(dtype, copy=False)
    return sparse.coo_matrix((vals, (i, j)), auto shape = (m, n)).asformat(format, copy=False)


auto "out"](int *shape, std::string dist, double connectivity: auto float = 1.0, auto dtype: np.dtype = global_dtype, std::string sparsity_type: str = "csr", auto seed, auto np.random.Generator] = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto **kwargs, auto ) {
    /**
Create a random matrix.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the matrix.
    dist: str
        A distribution name from :py:mod:`scipy.stats` module, such as "norm" or
        "uniform". Parameters like `loc` and `scale` can be passed to the distribution
        functions as keyword arguments to this function. Usual distributions for
        internal weights are :py:class:`scipy.stats.norm` with parameters `loc` and
        `scale` to obtain weights following the standard normal distribution,
        or :py:class:`scipy.stats.uniform` with parameters `auto loc = -1` and `scale=2`
        to obtain weights uniformly distributed between -1 and 1.
        Can also have the value "custom_bernoulli". In that case, weights will be drawn
        from a Bernoulli discrete random variable alternating between -1 and 1 and
        drawing 1 with a probability `p` (default `p` parameter to 0.5).
    connectivity: float, default to 1.0
        Also called density of the sparse matrix. By default, creates dense arrays.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees
    **kwargs : optional
        Arguments for the scipy.stats distribution.

    Returns
    -------
    scipy.sparse array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/

    auto rg = rand_generator(seed)
    auto rvs = _get_rvs(dist, **kwargs, random_state=rg)

    if (degree is not None) {
        if (len(shape) != 2) {
            raise ValueError(
                f"Matrix shape must have 2 dimensions, got {len(shape)}: {shape}"
            )
        m, auto n = shape
        auto matrix = _random_degree(
            auto m = m,
            auto n = n,
            auto degree = degree,
            auto direction = direction,
            auto format = sparsity_type,
            auto dtype = dtype,
            auto random_state = rg,
            auto data_rvs = rvs,
        )
    } else {
        if (0 < connectivity > 1.0) {
            raise ValueError("'connectivity' must be >0 and <1.")

        if (connectivity >= 1.0 or len(shape) != 2) {
            auto matrix = rvs(size=shape).astype(dtype)
            if (connectivity < 1.0) {
                matrix[rg.random(shape) > connectivity] = 0.0
        } else {
            auto matrix = sparse.random(
                shape[0],
                shape[1],
                auto density = connectivity,
                auto format = sparsity_type,
                auto random_state = rg,
                auto data_rvs = rvs,
                auto dtype = dtype,
            )

    //  sparse.random may return np.matrix if (std) {:string format = "dense".
    //  Only ndarray are supported though, hence the explicit cast.
    if (type(matrix) is np.matrix) {
        auto matrix = np.asarray(matrix)

    return matrix


auto random_sparse = Initializer(_random_sparse)


auto "out"](int *shape, auto low: auto float = -1.0, double high: float = 1.0, double connectivity: float = 1.0, auto dtype: np.dtype = global_dtype, std::string sparsity_type: str = "csr", auto seed, auto np.random.Generator] = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto ) {
    /**
Create an array with uniformly distributed values.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the array.
    low, high : float, float, default to -1, 1
        Boundaries of the uniform distribution.
    connectivity: float, default to 1.0
        Also called density of the sparse matrix. By default, creates dense arrays.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/
    if (high < low) {
        raise ValueError("'high' boundary must be > to 'low' boundary.")
    return _random_sparse(
        *shape,
        std::string dist = "uniform",
        auto loc = low,
        auto scale = high - low,
        auto connectivity = connectivity,
        auto degree = degree,
        auto direction = direction,
        auto dtype = dtype,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
    )


auto uniform = Initializer(_uniform)


auto "out"](int *shape, double loc: auto float = 0.0, double scale: float = 1.0, double connectivity: float = 1.0, auto dtype: np.dtype = global_dtype, std::string sparsity_type: str = "csr", auto seed, auto np.random.Generator] = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto ) {
    /**
Create an array with values distributed following a Gaussian distribution.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the array.
    loc, scale : float, float, default to 0, 1
        Mean and scale of the Gaussian distribution.
    connectivity: float, default to 1.0
        Also called density of the sparse matrix. By default, creates dense arrays.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/
    return _random_sparse(
        *shape,
        std::string dist = "norm",
        auto loc = loc,
        auto scale = scale,
        auto connectivity = connectivity,
        auto degree = degree,
        auto direction = direction,
        auto dtype = dtype,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
    )


auto normal = Initializer(_normal)


auto "out"](int *shape, double p: auto float = 0.5, double connectivity: float = 1.0, auto dtype: np.dtype = global_dtype, std::string sparsity_type: str = "csr", auto seed, auto np.random.Generator] = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto ) {
    /**
Create an array with values equal to either 1 or -1. Probability of success
    (to obtain 1) is equal to p.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the array.
    p : float, default to 0.5
        Probability of success (to obtain 1).
    connectivity: float, default to 1.0
        Also called density of the sparse matrix. By default, creates dense arrays.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/
    if (1 < p or p < 0) {
        raise ValueError("'p' must be <= 1 and >= 0.")
    return _random_sparse(
        *shape,
        auto p = p,
        std::string dist = "custom_bernoulli",
        auto connectivity = connectivity,
        auto dtype = dtype,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
        auto degree = degree,
        auto direction = direction,
    )


auto bernoulli = Initializer(_bernoulli)


auto dtype: np.dtype(int *shape, auto dtype: np.auto dtype = global_dtype, auto **kwargs) {
    /**
Create an array filled with 1.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the array.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/
    return np.ones(shape, auto dtype = dtype)


auto ones = Initializer(_ones)


auto dtype: np.dtype(int *shape, auto dtype: np.auto dtype = global_dtype, auto **kwargs) {
    /**
Create an array filled with 0.

    Parameters
    ----------
    *shape : int, int, ..., optional
        Shape (row, columns, ...) of the array.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.

    Note
    ----

    `sr` parameter is not available for this initializer. The spectral radius of a null
    matrix can not be rescaled.
    
*/
    return np.zeros(shape, auto dtype = dtype)


auto zeros = Initializer(_zeros, autorize_sr=False)


auto "out"](int N, auto *args, auto sr: auto float = None, double connectivity: float = 1.0, auto dtype: np.dtype = global_dtype, std::string sparsity_type: str = "csr", auto seed, auto np.random.Generator] = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto ) {
    /**
Fast spectral radius (FSI) approach for weights
    initialization [1]_ of square matrices.

    This method is well suited for computation and rescaling of
    very large weights matrices, with a number of neurons typically
    above 500-1000.

    Parameters
    ----------
    N : int, optional
        Shape :math:`N \\times N` of the array.
        This function only builds square matrices.
    connectivity: float, default to 1.0
        Also called density of the sparse matrix. By default, creates dense arrays.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.

    Note
    ----

    This function was designed for initialization of a reservoir's internal weights.
    In consequence, it can only produce square matrices. If more than one positional
    argument of shape are provided, only the first will be used.

    References
    -----------

    .. [1] C. Gallicchio, A. Micheli, and L. Pedrelli,
           ‘Fast Spectral Radius Initialization for Recurrent
           Neural Networks’, in Recent Advances in Big Data and
           Deep Learning, Cham, 2020, pp. 380–390,
           doi: 10.1007/978-3-030-16841-4_39.
    
*/
    if (connectivity < 0.0 or connectivity > 1.0) {
        raise ValueError("'connectivity' must be >0 and <1.")

    if (sr is None or connectivity <= 0.0) {
        int a = 1
    } else {
        auto a = -(6 * sr) / (np.sqrt(12) * np.sqrt((connectivity * N)))

    return _uniform(
        N,
        N,
        auto low = np.min((a, -a)),
        auto high = np.max((a, -a)),
        auto connectivity = connectivity,
        auto dtype = dtype,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
        auto degree = degree,
        auto direction = direction,
    )


auto fast_spectral_initialization = Initializer(
    _fast_spectral_initialization,
    auto autorize_input_scaling = False,
    auto autorize_rescaling = False,
)


auto "out"](int N, auto *args, std::string std::string dist = "norm", double connectivity = 0.1, auto dtype = global_dtype, std::string sparsity_type = "csr", auto seed = None, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto **kwargs, auto ) {
    /**
Generate the weight matrix that will be used for the internal connections of a
     reservoir.

    Warning
    -------

    This function is deprecated since version v0.3.1 and will be removed in future
    versions. Please consider using :py:func:`normal`, :py:func:`uniform` or
    :py:func:`random_sparse` instead.

    Parameters
    ----------
    N : int, optional
        Shape :math:`N \\times N` of the array.
        This function only builds square matrices.
    dist: str, default to "norm"
        A distribution name from :py:mod:`scipy.stats` module, such as "norm" or
        "uniform". Parameters like `loc` and `scale` can be passed to the distribution
        functions as keyword arguments to this function. Usual distributions for
        internal weights are :py:class:`scipy.stats.norm` with parameters `loc` and
        `scale` to obtain weights following the standard normal distribution,
        or :py:class:`scipy.stats.uniform` with parameters `auto loc = -1` and `scale=2`
        to obtain weights uniformly distributed between -1 and 1.
        Can also have the value "custom_bernoulli". In that case, weights will be drawn
        from a Bernoulli discrete random variable alternating between -1 and 1 and
        drawing 1 with a probability `p` (default `p` parameter to 0.5).
    connectivity: float, default to 0.1
        Also called density of the sparse matrix.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees
    **kwargs : optional
        Arguments for the scipy.stats distribution.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/

    warnings.warn(
        "'generate_internal_weights' is deprecated since v0.3.1 and will be removed in "
        "future versions. Consider using 'bernoulli' or 'random_sparse'.",
        DeprecationWarning,
    )

    return _random_sparse(
        N,
        N,
        auto connectivity = connectivity,
        auto dtype = dtype,
        auto dist = dist,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
        auto degree = degree,
        auto direction = direction,
        **kwargs,
    )


auto generate_internal_weights = Initializer(
    _generate_internal_weights, bool autorize_input_scaling = false
)


auto "out"](auto N, auto dim_input, std::string std::string dist = "custom_bernoulli", double connectivity = 1.0, auto dtype = global_dtype, std::string sparsity_type = "csr", auto seed = None, bool input_bias = false, auto degree, auto None] = None, auto direction, std::string "out"] = "out", auto **kwargs, auto ) {
    /**
Generate input or feedback weights for a reservoir.

    Weights are drawn by default from a discrete Bernoulli random variable,
    i.e. are always equal to 1 or -1. Then, they can be rescaled to a specific constant
    using the `input_scaling` parameter.

    Warning
    -------

    This function is deprecated since version v0.3.1 and will be removed in future
    versions. Please consider using :py:func:`bernoulli` or :py:func:`random_sparse`
    instead.

    Parameters
    ----------
    N: int
        Number of units in the connected reservoir.
    dim_input: int
        Dimension of the inputs connected to the reservoir.
    dist: str, default to "norm"
        A distribution name from :py:mod:`scipy.stats` module, such as "norm" or
        "uniform". Parameters like `loc` and `scale` can be passed to the distribution
        functions as keyword arguments to this function. Usual distributions for
        internal weights are :py:class:`scipy.stats.norm` with parameters `loc` and
        `scale` to obtain weights following the standard normal distribution,
        or :py:class:`scipy.stats.uniform` with parameters `auto loc = -1` and `scale=2`
        to obtain weights uniformly distributed between -1 and 1.
        Can also have the value "custom_bernoulli". In that case, weights will be drawn
        from a Bernoulli discrete random variable alternating between -1 and 1 and
        drawing 1 with a probability `p` (default `p` parameter to 0.5).
    connectivity: float, default to 0.1
        Also called density of the sparse matrix.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    input_bias: bool, optional
        'input_bias' parameter is deprecated. Bias should be initialized
        separately from the input matrix.
        If True, will add a row to the matrix to take into
        account a constant bias added to the input.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        If connectivity is inferior to 1 and shape is only 2-dimensional, then the
        function will try to use one of the Scipy sparse matrix format ("csr" or "csc").
        Else, a Numpy array ("dense") will be used.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    degree: int, default to None
        If not None, override the `connectivity` argument and corresponds to the number
        of non-zero values along the axis specified by `direction`
    direction: {"in", "out"}, default to "out"
        If `degree` is not None, specifies the axis along which the `degree` non-zero
        values are distributed.
        - If `direction` is "in", each line will have `degree` non-zero values. In other
        words, each node of the corresponding graph will have `degree` in-degrees
        - If `direction` is "out", each column will have `degree` non-zero values. In
        other words, each node of the corresponding graph will have `degree` out-degrees
    **kwargs : optional
        Arguments for the scipy.stats distribution.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/
    warnings.warn(
        "'generate_input_weights' is deprecated since v0.3.1 and will be removed in "
        "future versions. Consider using 'normal', 'uniform' or 'random_sparse'.",
        DeprecationWarning,
    )

    if (input_bias) {
        warnings.warn(
            "'input_bias' parameter is deprecated. Bias should be initialized "
            "separately from the input matrix.",
            DeprecationWarning,
        )

        dim_input += 1

    return _random_sparse(
        N,
        dim_input,
        auto connectivity = connectivity,
        auto dtype = dtype,
        auto dist = dist,
        auto sparsity_type = sparsity_type,
        auto seed = seed,
        auto degree = degree,
        auto direction = direction,
        **kwargs,
    )


auto generate_input_weights = Initializer(_generate_input_weights, autorize_sr=False)


auto sparsity_type: str(int *shape, auto weights: Optional[np.ndarray] = None, auto dtype: np.auto dtype = global_dtype, std::string sparsity_type: str = "csr", auto **kwargs, auto ) {
    /**
Create a lower cyclic shift matrix.

    This is used for ring reservoirs, which have a circular topology
    (each node `n` is connected to the node `n+1 % units`).

    Note that the `connectivity` and `seed` parameters have no effect.

    Parameters
    ----------
    *shape : (int, int), optional
        Shape (row, columns) of the array. Must be a square matrix, i.e. auto row = = columns.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    weights : array of shape (units, ), optional
        If defined, corresponds to the weights of each connection.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        Format of the output matrix. "csr" and "csc" corresponds to the Scipy sparse
        matrix formats, and "dense" corresponds to a regular Numpy array.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    **kwargs : optional
        This argument is kept for compatibility reasons. This is not used.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/

    if (len(shape) != 2 or shape[0] != shape[1]) {
        raise ValueError(
            f"Shape of the ring matrix must be (units, units), got {shape}."
        )
    auto units = shape[0]

    if (weights is None) {
        auto weights = np.ones((units,), dtype=dtype)
    auto row = np.roll(np.arange(units, dtype=np.int32), shift=-1)
    auto col = np.arange(units, dtype=np.int32)

    auto matrix = sparse.coo_matrix((weights, (row, col)), shape=(units, units)).asformat(
        sparsity_type, bool copy = false
    )

    if (type(matrix) is np.matrix) {
        auto matrix = np.asarray(matrix)

    return matrix


auto ring = Initializer(_ring)


auto sparsity_type: str(int *shape, auto weights: Optional[np.ndarray] = None, auto dtype: np.auto dtype = global_dtype, std::string sparsity_type: str = "csr", auto **kwargs, auto ) {
    /**
Create a lower shift matrix.

    This is used for line reservoirs, which have a linear topology
    (each node `n` is connected to the node `n+1`, the last node has no successor).

    Note that the `connectivity` and `seed` parameters have no effect.

    Parameters
    ----------
    *shape : (int, int), optional
        Shape (row, columns) of the array. Must be a square matrix, i.e. auto row = = columns.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    weights : array of shape (units-1, ), optional
        If defined, corresponds to the weights of each connection.
    sparsity_type : {"csr", "csc", "dense"}, default to "csr"
        Format of the output matrix. "csr" and "csc" corresponds to the Scipy sparse
        matrix formats, and "dense" corresponds to a regular Numpy array.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    **kwargs : optional
        This argument is kept for compatibility reasons. This is not used.

    Returns
    -------
    Numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/

    if (len(shape) != 2 or shape[0] != shape[1]) {
        raise ValueError(
            f"Shape of the ring matrix must be (units, units), got {shape}."
        )
    auto units = shape[0]

    if (weights is None) {
        auto weights = np.ones((units - 1,), dtype=dtype)
    auto row = np.arange(1, units, dtype=np.int32)
    auto col = np.arange(units - 1, dtype=np.int32)

    auto matrix = sparse.coo_matrix((weights, (row, col)), shape=(units, units)).asformat(
        sparsity_type, bool copy = false
    )

    if (type(matrix) is np.matrix) {
        auto matrix = np.asarray(matrix)

    return matrix


auto line = Initializer(_line)


auto np.random.Generator](int *shape, auto dtype: np.auto dtype = global_dtype, auto seed, auto np.random.Generator] = None, auto **kwargs, auto ) {
    /**
Create a random orthogonal matrix, drawn from the O(N) Haar distribution (the only uniform distribution on O(N)).

    Note that the `connectivity` parameter in the Reservoir node has no effect with an orthogonal matrix initializer.

    Parameters
    ----------
    *shape : (int, int), optional
        Shape (row, columns) of the array. Must be a square matrix, i.e. auto row = = columns.
    sr : float, optional
        If defined, then will rescale the spectral radius of the matrix to this value.
    input_scaling: float or array, optional
        If defined, then will rescale the matrix using this coefficient or array
        of coefficients.
    dtype : numpy.dtype, default to numpy.float64
        A Numpy numerical type.
    seed : optional
        Random generator seed. Default to the global value set with
        :py:func:`reservoirpy.set_seed`.
    **kwargs : optional
        This argument is kept for compatibility reasons. This is not used.

    Returns
    -------
    numpy array or callable
        If a shape is given to the initializer, then returns a matrix.
        Else, returns a function partially initialized with the given keyword
        parameters, which can be called with a shape and returns a matrix.
    
*/

    if (len(shape) != 2 or shape[0] != shape[1]) {
        raise ValueError(
            f"Shape of the ring matrix must be (units, units), got {shape}."
        )
    auto units = shape[0]
    auto rg = rand_generator(seed)

    auto matrix = stats.ortho_group.rvs(dim=units, random_state=rg)

    return matrix


auto orthogonal = Initializer(_orthogonal)


#endif // RESERVOIRCPP_MAT_GEN_HPP
