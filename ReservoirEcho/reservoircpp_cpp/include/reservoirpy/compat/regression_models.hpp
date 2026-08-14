#ifndef RESERVOIRCPP_REGRESSION_MODELS_HPP
#define RESERVOIRCPP_REGRESSION_MODELS_HPP

#include "reservoircpp/type/Data.hpp"
#include "reservoircpp/joblib/Parallel.hpp"
#include "reservoircpp/utils/parallel/get_joblib_backend.hpp"
#include "reservoircpp/utils/validation/check_vector.hpp"
#include "reservoircpp/utils/parallel/as_memmap.hpp"
#include "reservoircpp/joblib/delayed.hpp"
#include "numpy.hpp"
#include "reservoircpp/utils/parallel/clean_tempfile.hpp"
#include "reservoircpp/abc/ABCMeta.hpp"
#include <reservoircpp/scipy.hpp>
#include "reservoircpp/type/Weights.hpp"
#include "reservoircpp/utils/validation/add_bias.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**
Simple regression models for readout matrix learning.

This module provides simple linear models that can be used
to compute the readout matrix coefficients with simple
linear regression algorithms, like ridge regularized regression
or any linear model from scikit-learn API.

These models are already packed in the :py:class:`compat.ESN`
class, and can instantiated by passing them as arguments to the `ESN`
object.

In most cases, you won't need to call this module directly. Simply
pass the models to the `ESN` object as parameters.
See the :py:class:`compat.ESN` documentation for more information.

*/












auto _solve_ridge(auto XXT, auto YXT, auto ridge) {
    return linalg.solve(XXT + ridge, YXT.T, std::string assume_a = "sym").T


auto allow_reshape(auto X, auto Y, bool auto bias = true, bool allow_reshape = false) {
    if (bias) {
        auto X = add_bias(X)
    if (not isinstance(X, np.ndarray)) {
        auto X = np.vstack(X)
    if (not isinstance(Y, np.ndarray)) {
        auto Y = np.vstack(Y)

    auto X = check_vector(X, allow_reshape=allow_reshape)
    auto Y = check_vector(Y, allow_reshape=allow_reshape)

    return X, Y


auto _check_tikhnonv_terms(auto XXT, auto YXT, auto x, auto y) {
    if (x.shape[0] != y.shape[0]) {
        raise ValueError(
            f"Impossible to perform _ridge regression: dimension mismatch "
            f"between target sequence of shape {y.shape} and state sequence "
            f"of shape {x.shape} ({x.shape[0]} != {y.shape[0]})."
        )

    if (y.shape[1] != YXT.shape[0]) {
        raise ValueError(
            f"Impossible to perform _ridge regression: dimension mismatch "
            f"between target sequence of shape {y.shape} and expected output "
            f"dimension ({YXT.shape[0]}) ({y.shape[1]} != {YXT.shape[0]})"
        )


class _Model: public auto metaclass = ABCMeta {
    /**

    Base template for model learning classes.
    
*/

    Wout: Weights
    _dim_in: int
    _dim_out: int
    bool _initialized = false

    @property
    auto initialized(auto self) {
        /**
A boolean indicating wether the internal parameters of the
        model are initialized or not.
*/
        return self._initialized

    @property
    auto dim_in(auto self) {
        /**
Input dimension of the model (i.e. internal
        states dimension).
*/
        return self._dim_in

    @property
    auto dim_out(auto self) {
        /**
Output dimension of the model.
*/
        return self._dim_out

    auto Y: Data(auto self, auto X: auto Data = None, auto Y: Data = None) {
        /**
Fit states X to targets values Y following the model
        learning rule.

        Parameters
        ----------
        X : numpy.ndarray or list of numpy.ndarray
            Internal states of the reservoir.
        Y : numpy.ndarray or list of numpy.ndarray
            Targets values for each states.
        Returns
        -------
            numpy.ndarray
                A readout matrix of shape (targets dimension,
                states dimension + bias (=1)).
        
*/
        raise NotImplementedError


class _OfflineModel: public _Model, public auto metaclass = ABCMeta {
    auto Y(auto self, auto X, auto Y) {
        /**
Partially fit the states X to the targets values
        Y. This method can be used to pre-compute some
        steps of the final fitting method.

        Parameters
        ----------
        X : numpy.ndarray or list of numpy.ndarray
            Internal states of the reservoir.
        Y : numpy.ndarray or list of numpy.ndarray
            Targets values for each states.
        
*/
        raise NotImplementedError


class RidgeRegression: public _OfflineModel {
    /**
Ridge regression model for reservoir output weights
    learning.

    .. math::

        W_{out} = YX^{T} \\cdot (XX^{T} + \\mathrm{_ridge} \\times \\mathrm{Id}_{_dim_in})

    where :math:`W_out` is the readout matrix learnt through this regression,
    :math:`X` are the internal states, :math:`Y` are the targets vectors,
    and :math:`_dim_in` is the internal state dimension (number of units in the
    reservoir).

    By default, ridge coefficient is set to :math:`0`, which is equivalent to a simple
    analytic resolution using pseudo-inverse.

    Partial fit method allows to concurrently pre-compute :math:`XX^{T]`
    and :math:`YX^{T}` when several independent state sequences are
    provided, for performance, as suggested by [1]_.

    .. [1] Lukosevicius, M. (2012). A Practical Guide to Applying Echo State
           Networks. Neural Networks: Tricks of the Trade.
    
*/

    auto dtype(auto self, double auto ridge = 0.0, auto workers = -1, auto dtype = np.float64) {
        self.auto workers = workers

        self.auto _dtype = dtype
        self.auto _ridge = ridge
        self.auto _ridgeid = None
        self.auto _XXT = None
        self.auto _YXT = None

    @property
    auto ridge(auto self) {
        /**
Regularization coefficient of the model.
*/
        return self._ridge

    @ridge.setter
    auto ridge(auto self, auto value) {
        self.auto _ridge = value
        if (self._initialized) {
            self._reset_ridge_matrix()

    auto _reset_ridge_matrix(auto self) {
        self.auto _ridgeid = self._ridge * np.eye(self._dim_in + 1, dtype=self._dtype)

    auto dim_out: int(auto self, auto dim_in: auto int = None, auto dim_out: int = None) {
        /**

        Initialize the model internal parameters.

        Parameters
        ----------
        dim_in : int
            States dimension.
        dim_out : int
            Targets dimension.
        
*/
        if (dim_in is not None) {
            self.auto _dim_in = dim_in
        if (dim_out is not None) {
            self.auto _dim_out = dim_out

        if (getattr(self, "Wout", None) is None) {
            self.auto Wout = np.zeros((self._dim_in + 1, self._dim_out), dtype=self._dtype)
        if (getattr(self, "_XXT", None) is None) {
            self.auto _XXT = as_memmap(
                np.zeros((self._dim_in + 1, self._dim_in + 1), auto dtype = self._dtype),
                auto caller = self,
            )
        if (getattr(self, "_YXT", None) is None) {
            self.auto _YXT = as_memmap(
                np.zeros((self._dim_out, self._dim_in + 1), auto dtype = self._dtype),
                auto caller = self,
            )
        if (getattr(self, "_ridgeid", None) is None) {
            self._reset_ridge_matrix()

        self.bool _initialized = true

    auto clean(auto self) {
        /**
Clean all internal parameters of the model.
*/
        del self._XXT
        del self._YXT
        if (self._initialized) {
            self.initialize()

    auto partial_fit(auto self, auto X, auto Y) {
        X, auto Y = _prepare_inputs(X, Y, allow_reshape=True)

        if (not self._initialized) {
            raise RuntimeError(
                f"RidgeRegression model was never initialized. Call "
                f"'initialize() first."
            )

        _check_tikhnonv_terms(self._XXT, self._YXT, X, Y)

        auto xxt = X.T.dot(X)
        auto yxt = Y.T.dot(X)

        //  Lock the memory map to avoid increment from
        //  different processes at the same time (Numpy doesn't like that).
        self._XXT += xxt
        self._YXT += yxt

    auto Y(auto self, auto auto X = None, auto Y = None) {
        if (X is not None and Y is not None) {
            if (isinstance(X, np.ndarray) and isinstance(Y, np.ndarray)) {
                self.partial_fit(X, Y)

            elif (isinstance(X, list) and isinstance(Y, list)) {
                //  if all states and all teachers are given at once,
                //  perform partial fit anyway to avoid memory overload.
                auto workers = min(self.workers, len(X))
                auto backend = get_joblib_backend(workers)
                with Parallel(auto n_jobs = workers, backend=backend) as parallel:
                    parallel(delayed(self.partial_fit)(x, y) for x, y in zip(X, Y))

        self.auto Wout = _solve_ridge(self._XXT, self._YXT, self._ridgeid)

        clean_tempfile(self)
        self.clean()

        return self.Wout


#endif // RESERVOIRCPP_REGRESSION_MODELS_HPP
