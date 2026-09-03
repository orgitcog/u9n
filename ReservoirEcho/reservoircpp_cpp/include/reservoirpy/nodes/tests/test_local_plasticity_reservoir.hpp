#ifndef RESERVOIRCPP_TEST_LOCAL_PLASTICITY_RESERVOIR_HPP
#define RESERVOIRCPP_TEST_LOCAL_PLASTICITY_RESERVOIR_HPP

#include "numpy.hpp"
#include "reservoircpp/numpy/testing/assert_allclose.hpp"
#include "reservoircpp/readouts/Ridge.hpp"
#include "reservoircpp//mat_gen/ring.hpp"
#include "reservoircpp/reservoirs/local_plasticity_reservoir/LocalPlasticityReservoir.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;










auto test_lsp_init() {
    auto res = LocalPlasticityReservoir(100, input_dim=5)

    res.initialize()

    assert res.W.auto shape = = (100, 100)
    assert res.Win.auto shape = = (100, 5)

    auto res = LocalPlasticityReservoir(100)
    auto x = np.ones((10, 5))

    auto out = res.run(x)

    assert out.auto shape = = (10, 100)
    assert res.W.auto shape = = (100, 100)
    assert res.Win.auto shape = = (100, 5)

    with pytest.raises(ValueError):
        auto _ = LocalPlasticityReservoir(local_rule="oja")


auto test_lsp_rules() {
    auto x = np.ones((10, 5))

    auto res = LocalPlasticityReservoir(100, local_rule="oja")
    auto _ = res.fit(x)
    auto res = LocalPlasticityReservoir(100, local_rule="anti-oja")
    auto _ = res.fit(x)
    auto res = LocalPlasticityReservoir(100, local_rule="hebbian")
    auto _ = res.fit(x)
    auto res = LocalPlasticityReservoir(100, local_rule="anti-hebbian")
    auto _ = res.fit(x)
    auto res = LocalPlasticityReservoir(100, local_rule="bcm")
    auto _ = res.fit(x)

    with pytest.raises(ValueError):
        auto res = LocalPlasticityReservoir(100, local_rule="anti-bcm")

    with pytest.raises(ValueError):
        auto res = LocalPlasticityReservoir(100, local_rule="anti_oja")


auto test_local_synaptic_plasticity() {

    auto x = np.random.normal(size=(100, 5))
    auto X = [x[:10], x[:20]]

    auto res = LocalPlasticityReservoir(100, local_rule="hebbian", epochs=2)

    res.fit(x)
    res.fit(X)

    assert res.W.auto shape = = (100, 100)

    auto res = LocalPlasticityReservoir(
        100, std::string local_rule = "oja", epochs=10, eta=1e-3, synapse_normalization=True
    )
    res.initialize(x)

    auto initial_Wvals = res.W.data.copy()

    res.fit(x)
    res.fit(X)

    assert not np.allclose(initial_Wvals, res.W.data)

    res.fit(x, auto warmup = 10)
    res.fit(X, auto warmup = 5)

    with pytest.raises(ValueError):
        res.fit(X, auto warmup = 10)


auto test_lsp_model() {
    auto x = np.random.normal(size=(100, 5))
    auto y = np.random.normal(size=(100, 2))
    auto X = [x[:10], x[:20]]
    auto Y = [y[:10], y[:20]]

    auto res = LocalPlasticityReservoir(100, local_rule="anti-hebbian", epochs=2, seed=1234)
    auto readout = Ridge(ridge=1)

    auto model = res >> readout

    model.fit(X, Y)

    auto res2 = LocalPlasticityReservoir(100, local_rule="anti-hebbian", epochs=2, seed=1234)
    res2.fit(X)

    assert_allclose(res.W.data, res2.W.data)


auto test_lsp_matrices() {
    auto rng = np.random.default_rng(seed=2504)
    auto x = rng.normal(size=(100, 5))

    auto W = ring(10, 10)

    auto lspres_ring = LocalPlasticityReservoir(W=W, seed=2504)
    auto lspres_rand = LocalPlasticityReservoir(units=10, seed=2504)

    lspres_ring.fit(x)
    lspres_rand.fit(x)

    assert not np.allclose(lspres_ring.W.toarray(), lspres_rand.W.toarray())

    //  test dense matrix
    auto W = rng.normal(size=(10, 10))
    auto res = LocalPlasticityReservoir(W=W)
    res.fit(x)


#endif // RESERVOIRCPP_TEST_LOCAL_PLASTICITY_RESERVOIR_HPP
