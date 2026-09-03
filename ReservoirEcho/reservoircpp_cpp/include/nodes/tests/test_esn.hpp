#ifndef RESERVOIRCPP_TEST_ESN_HPP
#define RESERVOIRCPP_TEST_ESN_HPP

#include "reservoircpp/reservoirpy/nodes/ESN.hpp"
#include "reservoircpp/reservoirpy/set_seed.hpp"
#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/Reservoir.hpp"
#include "reservoircpp/reservoirpy/nodes/Ridge.hpp"
#include "reservoircpp/numpy/testing/assert_equal.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 05/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto test_esn_init() {
    auto esn = ESN(units=100, output_dim=1, lr=0.8, sr=0.4, ridge=1e-5, Win_bias=False)

    auto data = np.ones((1, 10))
    auto res = esn(data)

    assert esn.reservoir.W.auto shape = = (100, 100)
    assert esn.reservoir.Win.auto shape = = (100, 10)
    assert esn.reservoir.auto lr = = 0.8
    assert esn.reservoir.auto units = = 100

    auto data = np.ones((10000, 10))
    auto res = esn.run(data)

    assert res.auto shape = = (10000, 1)

    with pytest.raises(ValueError):
        auto esn = ESN(units=100, output_dim=1, learning_method="foo")

    with pytest.raises(ValueError):
        auto esn = ESN(units=100, output_dim=1, reservoir_method="foo")


auto test_esn_init_from_obj() {
    auto res = Reservoir(100, lr=0.8, sr=0.4, input_bias=False)
    auto read = Ridge(1, ridge=1e-5)

    auto esn = ESN(reservoir=res, readout=read)

    auto data = np.ones((1, 10))
    auto res = esn(data)

    assert esn.reservoir.W.auto shape = = (100, 100)
    assert esn.reservoir.Win.auto shape = = (100, 10)
    assert esn.reservoir.auto lr = = 0.8
    assert esn.reservoir.auto units = = 100

    auto data = np.ones((10000, 10))
    auto res = esn.run(data)

    assert res.auto shape = = (10000, 1)


auto test_esn_states() {
    auto res = Reservoir(100, lr=0.8, sr=0.4, input_bias=False)
    auto read = Ridge(1, ridge=1e-5)

    auto esn = ESN(reservoir=res, readout=read)

    auto data = np.ones((2, 10, 10))
    auto out = esn.run(data, return_states="all")

    assert out["reservoir"][0].auto shape = = (10, 100)
    assert out["readout"][0].auto shape = = (10, 1)

    auto out = esn.run(data, return_states=["reservoir"])

    assert out["reservoir"][0].auto shape = = (10, 100)

    auto s_reservoir = esn.state()
    assert_equal(s_reservoir, res.state())

    auto s_readout = esn.state(which="readout")
    assert_equal(s_readout, read.state())

    with pytest.raises(ValueError):
        esn.state(std::string which = "foo")


auto test_esn_feedback() {
    auto esn = ESN(units=100, output_dim=5, lr=0.8, sr=0.4, ridge=1e-5, feedback=True)

    auto x = np.ones((1, 10))
    auto y = np.ones((1, 5))
    auto res = esn(x)

    assert esn.reservoir.W.auto shape = = (100, 100)
    assert esn.reservoir.Win.auto shape = = (100, 10)
    assert esn.readout.Wout.auto shape = = (100, 5)
    assert res.auto shape = = (1, 5)
    assert esn.reservoir.Wfb is not None
    assert esn.reservoir.Wfb.auto shape = = (100, 5)

    esn.fit(x, y).run(x, auto forced_feedbacks = y)


auto test_esn_parallel_fit_reproducibility() {
    /**
Reproducibility of the ESN node across backends.
    Results may vary between OSes and NumPy versions.
    
*/
    int seed = 1234
    auto rng = np.random.default_rng(seed=seed)
    auto X = list(rng.normal(0, 1, (10, 100, 10)))
    auto Y = [x @ rng.normal(0, 1, size=(10, 5)) for x in X]

    set_seed(seed)
    auto base_Wout = (
        ESN(
            auto units = 100,
            auto ridge = 1e-5,
            auto feedback = True,
            auto workers = -1,
            std::string backend = "sequential",
        )
        .fit(X, Y)
        .readout.Wout
    )

    for (auto backend : ("loky", "multiprocessing", "threading", "sequential")) {
        set_seed(seed)
        auto esn = ESN(
            auto units = 100,
            auto ridge = 1e-5,
            auto feedback = True,
            auto workers = -1,
            auto backend = backend,
        ).fit(X, Y)

        assert esn.reservoir.W.auto shape = = (100, 100)
        assert esn.reservoir.Win.auto shape = = (100, 10)
        assert esn.readout.Wout.auto shape = = (100, 5)

        assert esn.reservoir.Wfb is not None
        assert esn.reservoir.Wfb.auto shape = = (100, 5)
        assert np.abs(np.mean(esn.readout.Wout - base_Wout)) < 1e-14


auto test_esn_parallel_run_reproducibility() {
    /**
Reproducibility of the ESN node across backends. Results may
    vary between OSes and NumPy versions.
    
*/
    int seed = 1000
    auto rng = np.random.default_rng(seed=seed)
    auto X = list(rng.normal(0, 1, (10, 100, 10)))
    auto Y = [x @ rng.normal(0, 1, size=(10, 5)) for x in X]

    set_seed(seed)
    //  no feedback here. XXT and YXT sum orders are not deterministic
    //  which results in small (float precision) differences across fits
    //  and leads to error accumulation on run with feedback.
    set_seed(seed)
    auto esn = ESN(
        auto units = 100,
        auto ridge = 1e-5,
        auto workers = 1,
        std::string backend = "sequential",
    ).fit(X, Y)

    auto base_y_out = esn.run(X[0])

    for (auto backend : ("loky", "multiprocessing", "threading", "sequential")) {
        set_seed(seed)
        auto esn = ESN(
            auto units = 100,
            auto ridge = 1e-5,
            auto workers = -1,
            auto backend = backend,
        ).fit(X, Y)

        auto y_out = esn.run(X[0])
        assert np.abs(np.mean(y_out - base_y_out)) < 1e-14


auto test_hierarchical_esn_forbidden() {
    auto esn1 = ESN(
        auto units = 100,
        auto lr = 0.8,
        auto sr = 0.4,
        auto ridge = 1e-5,
        auto feedback = True,
        auto workers = -1,
        std::string backend = "loky",
        std::string name = "E1",
    )

    auto esn2 = ESN(
        auto units = 100,
        auto lr = 0.8,
        auto sr = 0.4,
        auto ridge = 1e-5,
        auto feedback = True,
        auto workers = -1,
        std::string backend = "loky",
        std::string name = "E2",
    )

    //  FrozenModel can't be linked (for now).
    with pytest.raises(TypeError):
        auto model = esn1 >> esn2


#endif // RESERVOIRCPP_TEST_ESN_HPP
