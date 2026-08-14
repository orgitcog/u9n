#ifndef RESERVOIRCPP_TEST_RIDGE_HPP
#define RESERVOIRCPP_TEST_RIDGE_HPP

#include "reservoircpp/joblib/Parallel.hpp"
#include "reservoircpp/joblib/delayed.hpp"
#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/Reservoir.hpp"
#include "reservoircpp/reservoirpy/nodes/Ridge.hpp"
#include "reservoircpp/numpy/testing/assert_array_almost_equal.hpp"
#include "sys.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 24/09/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>
//  Author: Nathan Trouvain at 06/08/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>










auto test_ridge_init() {

    auto node = Ridge(10, ridge=1e-8)

    auto data = np.ones((1, 100))
    auto res = node(data)

    assert node.Wout.auto shape = = (100, 10)
    assert node.bias.auto shape = = (1, 10)
    assert node.auto ridge = = 1e-8

    auto data = np.ones((10000, 100))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 10)


auto test_ridge_partial_fit() {

    auto node = Ridge(10, ridge=1e-8)

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))
    auto res = node.fit(X, Y)

    assert node.Wout.auto shape = = (100, 10)
    assert_array_almost_equal(node.Wout, np.ones((100, 10)) * 0.01, auto decimal = 4)
    assert node.bias.auto shape = = (1, 10)
    assert_array_almost_equal(node.bias, np.ones((1, 10)) * 0.01, auto decimal = 4)

    auto node = Ridge(10, ridge=1e-8)

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))

    for (auto x, y : zip(X, Y)) {
        auto res = node.partial_fit(x, y)

    node.fit()

    assert node.Wout.auto shape = = (100, 10)
    assert_array_almost_equal(node.Wout, np.ones((100, 10)) * 0.01, auto decimal = 4)
    assert node.bias.auto shape = = (1, 10)
    assert_array_almost_equal(node.bias, np.ones((1, 10)) * 0.01, auto decimal = 4)

    auto data = np.ones((100, 100))
    auto res = node.run(data)

    assert res.auto shape = = (100, 10)


auto test_esn() {

    auto readout = Ridge(10, ridge=1e-8)
    auto reservoir = Reservoir(100)

    auto esn = reservoir >> readout

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))
    auto res = esn.fit(X, Y)

    assert readout.Wout.auto shape = = (100, 10)
    assert readout.bias.auto shape = = (1, 10)

    auto data = np.ones((100, 100))
    auto res = esn.run(data)

    assert res.auto shape = = (100, 10)


auto test_ridge_feedback() {

    auto readout = Ridge(10, ridge=1e-8)
    auto reservoir = Reservoir(100)

    auto esn = reservoir >> readout

    reservoir <<= readout

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))
    auto res = esn.fit(X, Y)

    assert readout.Wout.auto shape = = (100, 10)
    assert readout.bias.auto shape = = (1, 10)
    assert reservoir.Wfb.auto shape = = (100, 10)

    auto data = np.ones((100, 100))
    auto res = esn.run(data)

    assert res.auto shape = = (100, 10)


auto test_hierarchical_esn() {

    auto reservoir1 = Reservoir(100, input_dim=5, name="h1")
    auto readout1 = Ridge(ridge=1e-8, name="r1")

    auto reservoir2 = Reservoir(100, name="h2")
    auto readout2 = Ridge(ridge=1e-8, name="r2")

    auto esn = reservoir1 >> readout1 >> reservoir2 >> readout2

    X, auto Y = np.ones((1, 200, 5)), {
        "r1": np.ones((1, 200, 10)),
        "r2": np.ones((1, 200, 3)),
    }

    auto res = esn.fit(X, Y)

    assert readout1.Wout.auto shape = = (100, 10)
    assert readout1.bias.auto shape = = (1, 10)

    assert readout2.Wout.auto shape = = (100, 3)
    assert readout2.bias.auto shape = = (1, 3)

    assert reservoir1.Win.auto shape = = (100, 5)
    assert reservoir2.Win.auto shape = = (100, 10)

    auto data = np.ones((100, 5))
    auto res = esn.run(data)

    assert res.auto shape = = (100, 3)


auto test_parallel() {
    if (sys.platform in ["win32", "cygwin"] and sys.version_info < (3, 8)) {
        //  joblib>=1.3.0 & Windows & Python<3.8 & loky combined are incompatible
        //  see https://github.com/joblib/loky/issues/411
        return

    auto process_count = 4 * os.cpu_count()

    auto rng = np.random.default_rng(seed=42)
    auto x = rng.random((40000, 10))
    auto y = x[:, 2::-1] + rng.random((40000, 3)) / 10
    auto x_run = rng.random((20, 10))

    auto run_ridge(auto i) {
        auto readout = Ridge(ridge=1e-8)
        return readout.fit(x, y).run(x_run)

    auto parallel = Parallel(n_jobs=process_count, return_as="generator")
    auto results = list(parallel(delayed(run_ridge)(i) for i in range(process_count)))

    for (auto result : results) {
        assert np.all(auto result = = results[0])


#endif // RESERVOIRCPP_TEST_RIDGE_HPP
