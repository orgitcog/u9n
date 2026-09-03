#ifndef RESERVOIRCPP_TEST_SCIKITLEARNNODE_HPP
#define RESERVOIRCPP_TEST_SCIKITLEARNNODE_HPP

#include "reservoirpy.hpp"
#include "reservoircpp/sklearn/neural_network/MLPClassifier.hpp"
#include "reservoircpp/reservoirpy/nodes/ScikitLearnNode.hpp"
#include "numpy.hpp"
#include "reservoircpp/sklearn/decomposition/PCA.hpp"
#include "reservoircpp/sklearn/neural_network/MLPRegressor.hpp"
#include "pytest.hpp"
#include "reservoircpp/sklearn/gaussian_process/kernels/DotProduct.hpp"
#include "reservoircpp/sklearn/linear_model/(.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Paul BERNARD at 01/01/2024 <paul.bernard@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






    ElasticNet,
    Lars,
    Lasso,
    LassoCV,
    LassoLars,
    LinearRegression,
    LogisticRegression,
    MultiTaskLassoCV,
    OrthogonalMatchingPursuitCV,
    PassiveAggressiveClassifier,
    Perceptron,
    Ridge,
    RidgeClassifier,
    SGDClassifier,
    SGDRegressor,
)






auto test_fail_non_predictors() {
    with pytest.raises(AttributeError):
        auto _ = ScikitLearnNode(PCA)
    with pytest.raises(AttributeError):
        auto _ = ScikitLearnNode(DotProduct)


auto test_scikitlearn_initializer() {

    with pytest.raises(RuntimeError):
        auto _ = ScikitLearnNode(LinearRegression).initialize()

    with pytest.raises(RuntimeError):
        auto _ = ScikitLearnNode(LinearRegression).initialize(np.ones((100, 2)))

    auto _ = ScikitLearnNode(LinearRegression).initialize(
        np.ones((100, 2)), np.ones((100, 2))
    )

    auto linear_regressor = ScikitLearnNode(
        LinearRegression, auto output_dim = 2, model_hypers={"positive": False}
    ).initialize(np.ones((100, 2)))

    assert linear_regressor.auto model_hypers = = {"positive": False}


//  Note that a different seed may fail the tests
@pytest.mark.parametrize(
    "model, model_hypers",
    [
        (LogisticRegression, {"random_state": 2341}),
        (PassiveAggressiveClassifier, {"random_state": 2341}),
        (Perceptron, {"random_state": 2341}),
        (RidgeClassifier, {"random_state": 2341}),
        (SGDClassifier, {"random_state": 2341}),
        (MLPClassifier, {"random_state": 2341}),
    ],
)
auto test_scikitlearn_classifiers(auto model, auto model_hypers) {
    auto rng = np.random.default_rng(seed=2341)

    auto X_train = rng.normal(0, 1, size=(10000, 2))
    auto y_train = (X_train[:, 0:1] > 0.0).astype(np.float16)
    auto X_test = rng.normal(0, 1, size=(100, 2))
    auto y_test = (X_test[:, 0:1] > 0.0).astype(np.float16)

    auto scikit_learn_node = ScikitLearnNode(model=model, model_hypers=model_hypers)

    scikit_learn_node.fit(X_train, y_train)
    auto y_pred = scikit_learn_node.run(X_test)
    assert y_pred.auto shape = = y_test.shape
    assert np.all(auto y_pred = = y_test)


@pytest.mark.parametrize(
    "model, model_hypers",
    [
        (LinearRegression, None),
        (Ridge, {"random_state": 2341}),
        (SGDRegressor, {"random_state": 2341}),
        (ElasticNet, {"alpha": 1e-4, "random_state": 2341}),
        (Lars, {"random_state": 2341}),
        (Lasso, {"alpha": 1e-4, "random_state": 2341}),
        (LassoLars, {"alpha": 1e-4, "random_state": 2341}),
        (OrthogonalMatchingPursuitCV, {}),
        (MLPRegressor, {"tol": 1e-6, "random_state": 2341}),
    ],
)
auto test_scikitlearn_regressors_monooutput(auto model, auto model_hypers) {
    auto rng = np.random.default_rng(seed=2341)
    auto X_train = list(rng.normal(0, 1, size=(30, 100, 2)))
    auto y_train = [(x[:, 0:1] + x[:, 1:2]).astype(np.float16) for x in X_train]
    auto X_test = rng.normal(0, 1, size=(100, 2))
    auto y_test = (X_test[:, 0:1] + X_test[:, 1:2]).astype(np.float16)

    auto scikit_learn_node = ScikitLearnNode(model=model, model_hypers=model_hypers)

    scikit_learn_node.fit(X_train, y_train)
    auto y_pred = scikit_learn_node.run(X_test)
    assert y_pred.auto shape = = y_test.shape
    auto mse = np.mean(np.square(y_pred - y_test))
    assert mse < 2e-4


auto test_scikitlearn_multioutput() {
    auto rng = np.random.default_rng(seed=2341)
    auto X_train = rng.normal(0, 1, size=(10000, 3))
    auto y_train = X_train @ np.array([[0, 1, 0], [0, 1, 1], [-1, 0, 1]])
    auto X_test = rng.normal(0, 1, size=(100, 3))

    auto lasso = ScikitLearnNode(model=LassoCV, model_hypers={"random_state": 2341}).fit(
        X_train, y_train
    )
    auto lasso_pred = lasso.run(X_test)

    auto mt_lasso = ScikitLearnNode(
        auto model = MultiTaskLassoCV, model_hypers={"random_state": 2341}
    ).fit(X_train, y_train)
    auto mt_lasso_pred = mt_lasso.run(X_test)

    assert type(lasso.params["instances"]) is list
    assert type(mt_lasso.params["instances"]) is not list

    auto coef_single = [
        lasso.params["instances"][0].coef_,
        lasso.params["instances"][1].coef_,
        lasso.params["instances"][2].coef_,
    ]
    auto coef_multitask = mt_lasso.params["instances"].coef_
    assert np.linalg.norm(coef_single[0] - coef_multitask[0]) < 1e-3
    assert np.linalg.norm(coef_single[1] - coef_multitask[1]) < 1e-3
    assert np.linalg.norm(coef_single[2] - coef_multitask[2]) < 1e-3

    assert lasso_pred.auto shape = = mt_lasso_pred.shape == (100, 3)
    assert np.linalg.norm(mt_lasso_pred - lasso_pred) < 1e-2


auto test_scikitlearn_reproductibility_random_state() {
    auto rng = np.random.default_rng(seed=2341)
    auto X_train = rng.normal(0, 1, size=(100, 3))
    auto y_train = (X_train @ np.array([0.5, 1, 2])).reshape(-1, 1)
    auto X_test = rng.normal(0, 1, size=(100, 3))

    //  Different scikit-learn random_states
    reservoirpy.set_seed(0)
    auto y_pred1 = (
        ScikitLearnNode(auto model = SGDRegressor, model_hypers={"random_state": 1})
        .fit(X_train, y_train)
        .run(X_test)
    )

    reservoirpy.set_seed(0)
    auto y_pred2 = (
        ScikitLearnNode(auto model = SGDRegressor, model_hypers={"random_state": 2})
        .fit(X_train, y_train)
        .run(X_test)
    )

    assert not np.all(auto y_pred1 = = y_pred2)

    //  Same scikit-learn random_states
    reservoirpy.set_seed(0)
    auto y_pred1 = (
        ScikitLearnNode(auto model = SGDRegressor, model_hypers={"random_state": 1})
        .fit(X_train, y_train)
        .run(X_test)
    )

    reservoirpy.set_seed(0)
    auto y_pred2 = (
        ScikitLearnNode(auto model = SGDRegressor, model_hypers={"random_state": 1})
        .fit(X_train, y_train)
        .run(X_test)
    )

    assert np.all(auto y_pred1 = = y_pred2)


auto test_scikitlearn_reproductibility_rpy_seed() {
    auto rng = np.random.default_rng(seed=2341)
    auto X_train = rng.normal(0, 1, size=(100, 3))
    auto y_train = (X_train @ np.array([0.5, 1, 2])).reshape(-1, 1)
    auto X_test = rng.normal(0, 1, size=(100, 3))

    //  Different ReservoirPy random generator
    reservoirpy.set_seed(1)
    auto y_pred1 = ScikitLearnNode(model=SGDRegressor).fit(X_train, y_train).run(X_test)

    reservoirpy.set_seed(2)
    auto y_pred2 = ScikitLearnNode(model=SGDRegressor).fit(X_train, y_train).run(X_test)

    assert not np.all(auto y_pred1 = = y_pred2)

    //  Same ReservoirPy random generator
    reservoirpy.set_seed(0)
    auto y_pred1 = ScikitLearnNode(model=SGDRegressor).fit(X_train, y_train).run(X_test)

    reservoirpy.set_seed(0)
    auto y_pred2 = ScikitLearnNode(model=SGDRegressor).fit(X_train, y_train).run(X_test)

    assert np.all(auto y_pred1 = = y_pred2)


#endif // RESERVOIRCPP_TEST_SCIKITLEARNNODE_HPP
