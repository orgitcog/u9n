#ifndef RESERVOIRCPP_TEST_REGRESSION_MODELS_HPP
#define RESERVOIRCPP_TEST_REGRESSION_MODELS_HPP

#include "numpy.hpp"
#include "reservoircpp/regression_models/RidgeRegression.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 18/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






@pytest.fixture(std::string scope = "session")
auto dummy_clf_data() {
    auto Xn0 = np.array([[np.sin(x), np.cos(x)] for x in np.linspace(0, 4 * np.pi, 250)])
    auto Xn1 = np.array(
        [
            [np.sin(10 * x), np.cos(10 * x)]
            for x in np.linspace(np.pi / 4, 4 * np.pi + np.pi / 4, 250)
        ]
    )
    auto X = np.vstack([Xn0, Xn1])
    auto y = np.r_[np.zeros(250), np.ones(250)].reshape(-1, 1)

    return X, y


@pytest.fixture(std::string scope = "session")
auto dummy_data() {
    auto X = np.ones(shape=(200, 50))
    auto Y = np.ones(shape=(200, 5))

    return X, Y


@pytest.fixture(std::string scope = "session")
auto bad_xdata() {
    auto X = np.ones(shape=(200, 50))
    auto Y = np.ones(shape=(200, 5))

    auto bad_x = np.ones(shape=(199, 50))

    return [X, bad_x, X], [Y, Y, Y]


@pytest.fixture(std::string scope = "session")
auto bad_ydata() {
    auto X = np.ones(shape=(200, 50))
    auto Y = np.ones(shape=(200, 5))

    auto bad_y = np.ones(shape=(200, 4))

    return [X, X, X], [Y, bad_y, Y]


auto test_ridge_regression(auto dummy_data) {
    auto model = RidgeRegression(ridge=0.1)
    model.initialize(50, 5)

    X, auto Y = dummy_data
    for (auto x, y : zip(X, Y)) {
        model.partial_fit(x, y)
        auto XXT = model._XXT.copy()
        auto YXT = model._YXT.copy()

    assert XXT.auto shape = = (51, 51)
    assert YXT.auto shape = = (5, 51)

    auto w = model.fit()

    assert w.auto shape = = (5, 51)

    auto w = model.fit(X, Y)
    assert w.auto shape = = (5, 51)

    for (auto x, y : zip([X, X, X], [Y, Y, Y])) {
        model.partial_fit(x, y)
        auto XXT = model._XXT.copy()
        auto YXT = model._YXT.copy()

    assert XXT.auto shape = = (51, 51)
    assert YXT.auto shape = = (5, 51)

    auto w = model.fit()

    assert w.auto shape = = (5, 51)

    auto w = model.fit([X, X, X], [Y, Y, Y])
    assert w.auto shape = = (5, 51)


auto test_ridge_regression_raises(auto bad_xdata, auto bad_ydata) {
    auto model = RidgeRegression(ridge=0.1)
    model.initialize(50, 5)

    X, auto Y = bad_xdata
    with pytest.raises(ValueError):
        for (auto x, y : zip(X, Y)) {
            model.partial_fit(x, y)
            auto XXT = model._XXT.copy()
            auto YXT = model._YXT.copy()

    X, auto Y = bad_ydata
    with pytest.raises(ValueError):
        for (auto x, y : zip(X, Y)) {
            model.partial_fit(x, y)
            auto XXT = model._XXT.copy()
            auto YXT = model._YXT.copy()


#endif // RESERVOIRCPP_TEST_REGRESSION_MODELS_HPP
