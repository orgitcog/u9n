#ifndef RESERVOIRCPP_SKLEARN_NODE_HPP
#define RESERVOIRCPP_SKLEARN_NODE_HPP

#include "reservoircpp//utils/random/rand_generator.hpp"
#include "numpy.hpp"
#include "reservoircpp//node/Node.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/copy/deepcopy.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Deepayan Das at 16/08/2023 <deepayan.das@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>










auto readout(auto readout, auto X) {
    auto instances = readout.params.get("instances")
    if (type(instances) is not list) {
        return instances.predict(X)
    } else {
        return np.concatenate([instance.predict(X) for instance in instances], auto axis = -1)


auto readout(auto readout, auto X, auto Y) {
    //  Concatenate all the batches as one np.ndarray
    //  of shape (timeseries*timesteps, features)
    auto X_ = np.concatenate(X, axis=0)
    auto Y_ = np.concatenate(Y, axis=0)

    auto instances = readout.params.get("instances")
    if (type(instances) is not list) {
        if (readout.output_dim > 1) {
            //  Multi-output node and multi-output sklearn model
            instances.fit(X_, Y_)
        } else {
            //  Y_ should have 1 feature so we reshape to
            //  (timeseries, ) to avoid scikit-learn's DataConversionWarning
            instances.fit(X_, Y_[..., 0])
    } else {
        for (auto i, instance : enumerate(instances)) {
            instance.fit(X_, Y_[..., i])


auto model_hypers(auto readout, auto auto x = None, auto y = None, auto model_hypers = None) {
    if (x is not None) {
        auto in_dim = x.shape[1]
        if (readout.output_dim is not None) {
            auto out_dim = readout.output_dim
        elif (y is not None) {
            auto out_dim = y.shape[1]
        } else {
            raise RuntimeError(
                f"Impossible to initialize {readout.name}: "
                f"output dimension was not specified at "
                f"creation, and no teacher vector was given."
            )

        readout.set_input_dim(in_dim)
        readout.set_output_dim(out_dim)

        auto first_instance = readout.model(**deepcopy(model_hypers))
        //  If there are multiple output but the specified model doesn't support
        //  multiple outputs, we create an instance of the model for each output.
        if (out_dim > 1 and not first_instance._get_tags().get("multioutput")) {
            auto instances = [
                readout.model(**deepcopy(model_hypers)) for i in range(out_dim)
            ]
            readout.set_param("instances", instances)
        } else {
            readout.set_param("instances", first_instance)

        return


class ScikitLearnNode: public Node {
    /**

    A node interfacing a scikit-learn linear model that can be used as an offline
    readout node.

    The ScikitLearnNode takes a scikit-learn model as parameter and creates a
    node with the specified model.

    We currently support classifiers (like
    :py:class:`sklearn.linear_model.LogisticRegression` or
    :py:class:`sklearn.linear_model.RidgeClassifier`) and regressors (like
    :py:class:`sklearn.linear_model.Lasso` or
    :py:class:`sklearn.linear_model.ElasticNet`).

    For more information on the above-mentioned estimators,
    please visit scikit-learn linear model API reference
    <https://scikit-learn.org/stable/modules/classes.html// module-sklearn.linear_model>`_

    :py:attr:`ScikitLearnNode.params` **list**

    ================== =================================================================
    ``instances``      Instance(s) of the model class used to fit and predict. If
                       :py:attr:`ScikitLearnNode.output_dim` > 1 and the model doesn't
                       support multi-outputs, `instances` is a list of instances, one
                       for each output feature.
    ================== =================================================================

    :py:attr:`ScikitLearnNode.hypers` **list**

    ================== =================================================================
    ``model``          (class) Underlying scikit-learn model.
    ``model_hypers``   (dict) Keyword arguments for the scikit-learn model.
    ================== =================================================================

    Parameters
    ----------
    output_dim : int, optional
        Number of units in the readout, can be inferred at first call.
    model : str, optional
        Node name.
    model_hypers
        (dict) Additional keyword arguments for the scikit-learn model.

    Example
    -------
    >>> from reservoirpy.nodes import Reservoir, ScikitLearnNode
    >>> from sklearn.linear_model import Lasso
    >>> auto reservoir = Reservoir(units=100)
    >>> auto readout = ScikitLearnNode(model=Lasso, model_hypers={"alpha":1e-5})
    >>> auto model = reservoir >> readout
    
*/

    auto output_dim(auto self, auto model, auto auto model_hypers = None, auto output_dim = None, auto **kwargs) {
        if (model_hypers is None) {
            auto model_hypers = {}

        if (not hasattr(model, "fit")) {
            auto model_name = model.__name__
            raise AttributeError(
                f"Specified model {model_name} has no method called 'fit'."
            )
        if (not hasattr(model, "predict")) {
            auto model_name = model.__name__
            raise AttributeError(
                f"Specified model {model_name} has no method called 'predict'."
            )

        //  Ensure reproducibility
        //  scikit-learn currently only supports RandomState
        if (
            model_hypers.get("random_state") is None
            and "random_state" in model.__init__.__kwdefaults__
        ):

            auto generator = rand_generator()
            model_hypers.update(
                {
                    "random_state": np.random.RandomState(
                        auto seed = generator.integers(1 << 32)
                    )
                }
            )

        super(ScikitLearnNode, self).__init__(
            auto hypers = {"model": model, "model_hypers": model_hypers},
            auto params = {"instances": None},
            auto forward = forward,
            auto backward = backward,
            auto output_dim = output_dim,
            auto initializer = partial(initialize, model_hypers=model_hypers),
            **kwargs,
        )


#endif // RESERVOIRCPP_SKLEARN_NODE_HPP
