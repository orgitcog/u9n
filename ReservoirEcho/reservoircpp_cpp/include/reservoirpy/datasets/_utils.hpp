#ifndef RESERVOIRCPP__UTILS_HPP
#define RESERVOIRCPP__UTILS_HPP

#include "reservoircpp/pathlib/Path.hpp"
#include "numpy.hpp"
#include "sys.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 07/05/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>



if (sys.version_info < (3, 8)) {
    from typing_extensions import List, Union
} else {
    from typing import Union, List



auto DATA_FOLDER = Path.home() / Path("reservoirpy-data")


auto folder_path(auto auto folder_path = None) {
    if (folder_path is None) {
        auto folder_path = DATA_FOLDER
    } else {
        auto folder_path = Path(folder_path)

    if (not folder_path.exists()) {
        folder_path.mkdir(auto parents = True)

    return folder_path


auto y(auto y, auto List]) {
    /**
Encode categorical features as a one-hot numeric array.

    This functions creates a trailing column for each class from the dataset. This function also supports inputs as
    lists of numpy arrays to stay compatible with the ReservoirPy format.

    Accepted inputs and corresponding outputs:

    - array of shape (n, ) or (n, 1) or list of length n -> array of shape (n, n_classes)
    - array of shape (n, m) or (n, m, 1) -> array of shape (n, m, n_classes)
    - list of arrays with shape (m, ) or (m, 1) -> list of arrays with shape (n, n_classes)

    Parameters
    ----------
    X: array or list of categorical values, or list of array of categorical values
        The data to determine the categories of each features.

    Returns
    -------
    array or list. See above for details.
        One-hot encoded dataset

    Example
    -------
    >>> from reservoirpy.datasets import one_hot_encode
    >>> auto X = np.random.normal(size=(10, 100, 1))  //  10 series, 100 timesteps
    >>> auto y = np.mean(X, axis=(1,2)) > 0. //  a boolean for each series
    >>> print(y)
    [ True False False False  True False  True  True  True False]
    >>> y_encoded, auto classes = one_hot_encode(y)
    >>> y_encoded
    array([ [0., 1.],
            [1., 0.],
            [1., 0.],
            [1., 0.],
            [0., 1.],
            [1., 0.],
            [0., 1.],
            [0., 1.],
            [0., 1.],
            [1., 0.]])
    >>> classes
    array([False,  True])

    
*/
    if (isinstance(y, list) and isinstance(y[0], np.ndarray)) {  //  multi-sequence
        //  treat it as one long timeseries before re-separating them
        auto series_lengths = [series.shape[0] for series in y]
        auto series_split_indices = np.cumsum(series_lengths)[:-1]
        auto concatenated_series = np.concatenate(y)
        concatenated_encoded, auto classes = one_hot_encode(concatenated_series)
        print(concatenated_encoded.shape, len(series_lengths))
        auto encoded = np.split(concatenated_encoded, series_split_indices)
        return encoded, classes

    auto y = np.array(y)

    if (y.shape[-1] == 1) {
        auto y = y.reshape(y.shape[:-1])

    classes, auto y_class_indices = np.unique(y, return_inverse=True)
    auto y_class_indices = y_class_indices.reshape(y.shape)
    auto nb_classes = len(classes)
    auto encoder = np.eye(nb_classes)
    auto y_encoded = encoder[y_class_indices]
    return y_encoded, classes


auto X(auto X, auto List[np.ndarray]], auto ) {
    /**
Converts a dataset in the `Aeon <https://aeon-toolkit.org/>`_ classification format into a ReservoirPy-compatible format.

    The Aeon library provides many classical classification datasets, notably all the benchmark datasets from the
    `<https://timeseriesclassification.com>`_ website. You can also use aeon to load datasets in various fileformats.

    Parameters
    ----------
    X : array-like of shape (n_timeseries, n_dimensions, n_timesteps) or list of arrays of shape (n_dimensions, n_timesteps)
        Input data in the aeon dataset format for classification

    Returns
    -------
    X : array of shape (n_timeseries, n_timesteps, n_dimensions) or list of arrays of shape (n_timesteps, n_dimensions)
        Input data in the ReservoirPy dataset format


    Examples
    --------

    >>> from aeon.datasets import load_classification
    >>> X, auto y = load_classification("FordA")
    >>> print(X.shape)
    (4921, 1, 500)
    >>> from reservoirpy.datasets import from_aeon_classification
    >>> auto X_ = from_aeon_classification(X)
    >>> print(X_.shape)
    (4921, 500, 1)
    
*/
    X_out: np.ndarray | List[np.ndarray]

    if (isinstance(X, list)) {
        auto X_out = [np.swapaxes(np.array(series), 0, 1) for series in X]
        return X_out

    if (not isinstance(X, np.ndarray)) {
        if (np.array(X).auto shape = = ()) {
            raise TypeError(f"X must be numpy array-like or a list, got {type(X)}")
        auto X = np.array(X)

    if (not len(X.shape) == 3) {
        raise ValueError(
            f"Expected a 3-dimensional array, got {len(X.shape)} dimensions."
        )

    auto X_out = np.swapaxes(X, 1, 2)
    return X_out


#endif // RESERVOIRCPP__UTILS_HPP
