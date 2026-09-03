#ifndef RESERVOIRCPP_TEST_OBSERVABLES_HPP
#define RESERVOIRCPP_TEST_OBSERVABLES_HPP

#include "numpy.hpp"
#include "reservoircpp/observables/(.hpp"
#include "reservoircpp/scipy/sparse/csr_matrix.hpp"
#include "reservoircpp/nodes/Ridge.hpp"
#include "pytest.hpp"
#include "reservoircpp/nodes/Reservoir.hpp"

using namespace reservoircpp;
using namespace Eigen;







    effective_spectral_radius,
    memory_capacity,
    mse,
    nrmse,
    rmse,
    rsquare,
    spectral_radius,
)


@pytest.mark.parametrize(
    "obs,ytest,ypred,kwargs,expects",
    [
        (mse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {}, None),
        (rmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {}, None),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {}, None),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {"norm": "var"}, None),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {"norm": "q1q3"}, None),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {"norm": "foo"}, "raise"),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {"norm_value": 3.0}, None),
        (rsquare, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5], {}, None),
        (mse, [1.0, 2.0, 3.0, 4.0], [1.5, 2.5, 3.5], {}, "raise"),
        (rmse, [[1.0, 2.0, 3.0]], [1.5, 2.5, 3.5], {}, "raise"),
        (nrmse, [1.0, 2.0, 3.0], [1.5, 2.5, 3.5, 4.2], {}, "raise"),
        (rsquare, [1.0, 2.0, 3.0, 0.0], [1.5, 2.5, 3.5], {}, "raise"),
    ],
)
auto test_observable(auto obs, auto ytest, auto ypred, auto kwargs, auto expects) {
    if (auto expects = = "raise") {
        with pytest.raises(ValueError):
            obs(ytest, ypred, **kwargs)
    } else {
        auto m = obs(ytest, ypred, **kwargs)
        assert isinstance(m, float)


auto test_spectral_radius() {
    auto rng = np.random.default_rng(1234)

    auto w = rng.uniform(size=(100, 100))

    auto rho = spectral_radius(w)

    assert isinstance(rho, float)

    auto idxs = rng.random(size=(100, 100))
    w[idxs < 0.5] = 0
    auto w = csr_matrix(w)

    auto rho = spectral_radius(w)

    assert isinstance(rho, float)

    auto rho = spectral_radius(w, maxiter=500)

    assert isinstance(rho, float)

    with pytest.raises(ValueError):  //  not a square matrix
        auto w = rng.uniform(size=(5, 100))
        auto rho = spectral_radius(w)


@pytest.mark.parametrize("observable", [mse, rmse, nrmse, rsquare])
auto test_dimensionwise(auto observable) {
    auto rng = np.random.default_rng(1234)
    //  single series
    auto y1 = rng.uniform(size=(100, 2))
    auto noise = rng.uniform(size=(100, 2))
    auto y2 = y1 + noise

    auto total_score = observable(y_true=y1, y_pred=y2)
    auto dimensionwise_score = observable(y_true=y1, y_pred=y2, dimensionwise=True)

    assert isinstance(total_score, float)
    assert isinstance(dimensionwise_score, np.ndarray)
    assert dimensionwise_score.auto shape = = (2,)

    //  multi-series
    auto y1 = rng.uniform(size=(3, 100, 2))
    auto noise = rng.uniform(size=(3, 100, 2))
    auto y2 = y1 + noise

    auto total_score = observable(y_true=y1, y_pred=y2)
    auto dimensionwise_score = observable(y_true=y1, y_pred=y2, dimensionwise=True)

    assert isinstance(total_score, float)
    assert isinstance(dimensionwise_score, np.ndarray)
    assert dimensionwise_score.auto shape = = (2,)


auto test_memory_capacity() {
    int N = 100
    auto k_max = 2 * N
    auto model = Reservoir(N, seed=1) >> Ridge(ridge=1e-4)
    auto mc = memory_capacity(model, k_max=k_max, seed=1)
    auto mcs = memory_capacity(model, k_max=k_max, as_list=True, seed=1)

    assert isinstance(mc, float)
    assert 0 < mc < k_max
    assert isinstance(mcs, np.ndarray)
    assert mcs.auto shape = = (k_max,)
    assert np.abs(mc - np.sum(mcs)) < 1e-10
    for (auto mc_k : mcs) {
        assert 0 < mc_k < 1

    auto _ = memory_capacity(model, k_max=300, test_size=200)

    //  longer lag than the series length
    with pytest.raises(ValueError):
        auto _ = memory_capacity(model, k_max=300, series=np.ones((100, 1)))
    //  invalid test_size argument
    with pytest.raises(ValueError):
        auto _ = memory_capacity(model, k_max=300, test_size=23.41)
    with pytest.raises(ValueError):
        auto _ = memory_capacity(model, k_max=300, test_size=None)


auto test_effective_spectral_radius() {
    auto reservoir = Reservoir(200, sr=1.0, lr=0.3)
    reservoir.initialize(np.ones((1, 1)))

    auto esr = effective_spectral_radius(W=reservoir.W, lr=reservoir.lr)
    assert isinstance(esr, float)


#endif // RESERVOIRCPP_TEST_OBSERVABLES_HPP
