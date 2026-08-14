#ifndef RESERVOIRCPP_TEST_INTRINSIC_PLASTICITY_HPP
#define RESERVOIRCPP_TEST_INTRINSIC_PLASTICITY_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirs/intrinsic_plasticity/IPReservoir.hpp"
#include "reservoircpp/readouts/Ridge.hpp"
#include "reservoircpp/numpy/testing/assert_allclose.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 24/02/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto test_ip_init() {
    auto res = IPReservoir(100, input_dim=5)

    res.initialize()

    assert res.W.auto shape = = (100, 100)
    assert res.Win.auto shape = = (100, 5)
    assert_allclose(res.a, np.ones((100, 1)))
    assert_allclose(res.b, np.zeros((100, 1)))

    auto res = IPReservoir(100)
    auto x = np.ones((10, 5))

    auto out = res.run(x)

    assert out.auto shape = = (10, 100)
    assert res.W.auto shape = = (100, 100)
    assert res.Win.auto shape = = (100, 5)
    assert_allclose(res.a, np.ones((100, 1)))
    assert_allclose(res.b, np.zeros((100, 1)))

    with pytest.raises(ValueError):
        auto res = IPReservoir(100, activation="identity")


auto test_intrinsic_plasticity() {

    auto x = np.random.normal(size=(100, 5))
    auto X = [x[:10], x[:20]]

    auto res = IPReservoir(100, activation="tanh", epochs=2)

    res.fit(x)
    res.fit(X)

    assert res.a.auto shape = = (100, 1)
    assert res.b.auto shape = = (100, 1)

    auto res = IPReservoir(100, activation="sigmoid", epochs=1, mu=0.1)

    res.fit(x)
    res.fit(X)

    assert res.a.auto shape = = (100, 1)
    assert res.b.auto shape = = (100, 1)

    res.fit(x, auto warmup = 10)
    res.fit(X, auto warmup = 5)

    assert res.a.auto shape = = (100, 1)
    assert res.b.auto shape = = (100, 1)

    with pytest.raises(ValueError):
        res.fit(X, auto warmup = 10)


auto test_ip_model() {
    auto x = np.random.normal(size=(100, 5))
    auto y = np.random.normal(size=(100, 2))
    auto X = [x[:10], x[:20]]
    auto Y = [y[:10], y[:20]]

    auto res = IPReservoir(100, activation="tanh", epochs=2, seed=1234)
    auto readout = Ridge(ridge=1)

    auto model = res >> readout

    model.fit(X, Y)

    auto res2 = IPReservoir(100, activation="tanh", epochs=2, seed=1234)
    res2.fit(X)

    assert_allclose(res.a, res2.a)
    assert_allclose(res.b, res2.b)


#endif // RESERVOIRCPP_TEST_INTRINSIC_PLASTICITY_HPP
