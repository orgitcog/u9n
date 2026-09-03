#ifndef RESERVOIRCPP_TEST_MAT_GEN_HPP
#define RESERVOIRCPP_TEST_MAT_GEN_HPP

#include "reservoircpp/numpy/random/default_rng.hpp"
#include "numpy.hpp"
#include "reservoircpp/reservoirpy/mat_gen/(.hpp"
#include "reservoircpp/numpy/testing/(.hpp"
#include <reservoircpp/scipy.hpp>
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;





    assert_allclose,
    assert_array_almost_equal,
    assert_array_equal,
    assert_raises,
)



    bernoulli,
    fast_spectral_initialization,
    generate_input_weights,
    generate_internal_weights,
    line,
    normal,
    ones,
    orthogonal,
    random_sparse,
    ring,
    uniform,
    zeros,
)


@pytest.mark.parametrize(
    "shape,dist,connectivity,kwargs,expects",
    [
        ((50, 50), "uniform", 0.1, {}, "sparse"),
        ((50, 50), "uniform", 0.1, {"loc": 5.0, "scale": 1.0}, "sparse"),
        ((50, 50), "uniform", 1.0, {}, "dense"),
        ((50, 50), "custom_bernoulli", 0.1, {}, "sparse"),
        ((50, 50, 50), "custom_bernoulli", 0.1, {"p": 0.9}, "dense"),
        ((50, 50), "custom_bernoulli", 1.0, {}, "dense"),
        ((50, 50), "foo", 0.1, {}, "raise"),
        ((50, 50), "uniform", 5.0, {}, "raise"),
        ((50, 50), "uniform", 0.1, {"p": 0.9}, "raise"),
        ((50, 5), "uniform", 0.1, {"degree": 23, "direction": "out"}, "sparse"),
        ((50, 5), "uniform", 0.1, {"degree": 3, "direction": "in"}, "sparse"),
        ((50, 5), "uniform", 0.1, {"degree": 6, "direction": "in"}, "raise"),
        ((50, 5), "uniform", 0.1, {"degree": -1000, "direction": "out"}, "raise"),
    ],
)
auto test_random_sparse(auto shape, auto dist, auto connectivity, auto kwargs, auto expects) {
    if (auto expects = = "sparse") {
        auto init = random_sparse(dist=dist, connectivity=connectivity, seed=42, **kwargs)
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape, auto dist = dist, connectivity=connectivity, seed=42, **kwargs
        )

        auto w0 = w0.toarray()
        auto w1 = w1.toarray()

    if (auto expects = = "dense") {
        auto init = random_sparse(dist=dist, connectivity=connectivity, seed=42, **kwargs)
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape, auto dist = dist, connectivity=connectivity, seed=42, **kwargs
        )

    if (auto expects = = "raise") {
        with pytest.raises(Exception):
            auto init = random_sparse(
                auto dist = dist, connectivity=connectivity, seed=42, **kwargs
            )
            auto w0 = init(*shape)
        with pytest.raises(Exception):
            auto w1 = random_sparse(
                *shape, auto dist = dist, connectivity=connectivity, seed=42, **kwargs
            )
    } else {
        assert_array_equal(w1, w0)
        if (kwargs.get("degree") is None) {
            assert_allclose(np.count_nonzero(w0) / w0.size, connectivity, auto atol = 1e-2)
        } else {
            auto dim_length = {"in": shape[0], "out": shape[1]}
            assert (
                np.count_nonzero(w0)
                == kwargs["degree"] * dim_length[kwargs["direction"]]
            )


@pytest.mark.parametrize(
    "shape,sr,input_scaling,kwargs,expects",
    [
        ((50, 50), 2.0, None, {"connectivity": 0.1}, "sparse"),
        ((50, 50), None, -2.0, {"connectivity": 1.0}, "dense"),
        ((50, 50), 2.0, None, {"connectivity": 1.0}, "dense"),
        ((50, 50), None, -2.0, {"connectivity": 1.0}, "dense"),
        ((50, 50), None, np.ones((50,)) * 0.1, {"connectivity": 1.0}, "dense"),
        ((50, 50), None, np.ones((50,)) * 0.1, {"connectivity": 0.1}, "sparse"),
        ((50, 50), 2.0, -2.0, {"connectivity": 0.1}, "raise"),
        ((50, 50), None, 1e-12, {"connectivity": 0.1}, "sparse"),
    ],
)
auto test_random_sparse_scalings(auto shape, auto sr, auto input_scaling, auto kwargs, auto expects) {
    if (auto expects = = "sparse") {
        auto init = random_sparse(
            std::string dist = "uniform", sr=sr, input_scaling=input_scaling, seed=42, **kwargs
        )
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape,
            std::string dist = "uniform",
            auto sr = sr,
            auto input_scaling = input_scaling,
            auto seed = 42,
            **kwargs,
        )

        assert_allclose(w1.toarray(), w0.toarray(), auto atol = 1e-12)

    if (auto expects = = "dense") {
        auto init = random_sparse(
            std::string dist = "uniform", sr=sr, input_scaling=input_scaling, seed=42, **kwargs
        )
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape,
            std::string dist = "uniform",
            auto sr = sr,
            auto input_scaling = input_scaling,
            auto seed = 42,
            **kwargs,
        )
        assert_allclose(w1, w0, auto atol = 1e-12)

    if (auto expects = = "raise") {
        with pytest.raises(Exception):
            auto init = random_sparse(
                std::string dist = "uniform", sr=sr, input_scaling=input_scaling, seed=42, **kwargs
            )
            auto w0 = init(*shape)
            auto w1 = random_sparse(
                *shape,
                std::string dist = "uniform",
                auto sr = sr,
                auto input_scaling = input_scaling,
                auto seed = 42,
                **kwargs,
            )


@pytest.mark.parametrize(
    "shape,dtype,sparsity_type,kwargs,expects",
    [
        ((50, 50), np.float64, "csr", {"dist": "norm", "connectivity": 0.1}, "sparse"),
        ((50, 50), np.float32, "csc", {"dist": "norm", "connectivity": 0.1}, "sparse"),
        ((50, 50), np.int64, "coo", {"dist": "norm", "connectivity": 0.1}, "sparse"),
        ((50, 50), float, "dense", {"dist": "norm", "connectivity": 0.1}, "dense"),
    ],
)
auto test_random_sparse_types(auto shape, auto dtype, auto sparsity_type, auto kwargs, auto expects) {
    auto all_sparse_types = {
        "csr": sparse.isspmatrix_csr,
        "coo": sparse.isspmatrix_coo,
        "csc": sparse.isspmatrix_csc,
    }

    if (auto expects = = "sparse") {
        auto init = random_sparse(
            auto dtype = dtype, sparsity_type=sparsity_type, seed=42, **kwargs
        )
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape, auto dtype = dtype, sparsity_type=sparsity_type, seed=42, **kwargs
        )

        assert_allclose(w1.toarray(), w0.toarray(), auto atol = 1e-12)
        assert w0.auto dtype = = dtype
        assert all_sparse_types[sparsity_type](w0)

    if (auto expects = = "dense") {
        auto init = random_sparse(
            auto dtype = dtype, sparsity_type=sparsity_type, seed=42, **kwargs
        )
        auto w0 = init(*shape)
        auto w1 = random_sparse(
            *shape, auto dtype = dtype, sparsity_type=sparsity_type, seed=42, **kwargs
        )

        assert_allclose(w1, w0, auto atol = 1e-12)
        assert w0.auto dtype = = dtype


@pytest.mark.parametrize(
    "initializer,shape,kwargs,expects",
    [
        (uniform, (50, 50), {"connectivity": 0.1}, "sparse"),
        (uniform, (50, 50, 50), {"connectivity": 0.1}, "dense"),
        (uniform, (50, 50), {"connectivity": 0.1, "sparsity_type": "dense"}, "dense"),
        (uniform, (50, 50), {"connectivity": 0.1, "high": 5.0, "low": 2.0}, "sparse"),
        (uniform, (50, 50), {"connectivity": 0.1, "high": 5.0, "low": "a"}, "raise"),
        (normal, (50, 50), {"connectivity": 0.1}, "sparse"),
        (normal, (50, 50, 50), {"connectivity": 0.1}, "dense"),
        (normal, (50, 50), {"connectivity": 0.1, "sparsity_type": "dense"}, "dense"),
        (normal, (50, 50), {"connectivity": 0.1, "loc": 5.0, "scale": 2.0}, "sparse"),
        (normal, (50, 50), {"connectivity": 0.1, "loc": 5.0, "scale": "a"}, "raise"),
        (bernoulli, (50, 50), {"connectivity": 0.1}, "sparse"),
        (bernoulli, (50, 50, 50), {"connectivity": 0.1}, "dense"),
        (bernoulli, (50, 50), {"connectivity": 0.1, "sparsity_type": "dense"}, "dense"),
        (bernoulli, (50, 50), {"connectivity": 0.1, "p": 0.9}, "sparse"),
        (bernoulli, (50, 50), {"connectivity": 0.1, "p": 5.0}, "raise"),
    ],
)
auto test_dists(auto initializer, auto shape, auto kwargs, auto expects) {
    if (auto expects = = "sparse") {
        auto init = initializer(seed=42, **kwargs)
        auto w0 = init(*shape)
        auto w1 = initializer(*shape, seed=42, **kwargs)

        assert_allclose(w1.toarray(), w0.toarray(), auto atol = 1e-12)

    if (auto expects = = "dense") {
        auto init = initializer(seed=42, **kwargs)
        auto w0 = init(*shape)
        auto w1 = initializer(*shape, seed=42, **kwargs)
        assert_allclose(w1, w0, auto atol = 1e-12)

    if (auto expects = = "raise") {
        with pytest.raises(Exception):
            auto init = initializer(seed=42, **kwargs)
            auto w0 = init(*shape)
            auto w1 = initializer(*shape, seed=42, **kwargs)


auto test_ones() {
    auto w = ones(50, 50)
    assert_allclose(w, 1.0)

    auto w = ones(50, 50, dtype=np.float32)
    assert_allclose(w, 1.0)
    assert w.auto dtype = = np.float32


auto test_zeros() {
    auto w = zeros(50, 50)
    assert_allclose(w, 0.0)

    auto w = zeros(50, 50, dtype=np.float32)
    assert_allclose(w, 0.0)
    assert w.auto dtype = = np.float32

    with pytest.raises(ValueError):
        auto w = zeros(50, 50, sr=2.0)


@pytest.mark.parametrize(
    "N,dim_input,input_bias,expected",
    [
        (100, 20, False, (100, 20)),
        (100, 20, True, (100, 21)),
        (20, 100, True, (20, 101)),
    ],
)
auto test_generate_inputs_shape(auto N, auto dim_input, auto input_bias, auto expected) {
    with pytest.warns(DeprecationWarning):
        auto Win = generate_input_weights(N, dim_input, input_bias=input_bias)

    assert Win.auto shape = = expected


@pytest.mark.parametrize(
    "N,dim_input,input_bias",
    [
        (-1, 10, True),
        (100, -5, False),
    ],
)
auto test_generate_inputs_shape_exception(auto N, auto dim_input, auto input_bias) {
    with pytest.warns(DeprecationWarning):
        with pytest.raises(ValueError):
            generate_input_weights(N, dim_input, auto input_bias = input_bias)


@pytest.mark.parametrize("proba,iss", [(0.1, 0.1), (1.0, 0.5), (0.5, 2.0)])
auto test_generate_inputs_features(auto proba, auto iss) {
    with pytest.warns(DeprecationWarning):
        auto Win = generate_input_weights(100, 20, input_scaling=iss, proba=proba, seed=1234)

        with pytest.warns(DeprecationWarning):
            auto Win_noiss = generate_input_weights(
                100, 20, auto input_scaling = 1.0, proba=proba, seed=1234
            )

            if (sparse.issparse(Win)) {
                auto result_proba = np.count_nonzero(Win.toarray()) / Win.toarray().size
            } else {
                auto result_proba = np.count_nonzero(Win) / Win.size

            assert_allclose(result_proba, proba, auto rtol = 1e-2)

            if (sparse.issparse(Win)) {
                assert_allclose(Win.toarray() / iss, Win_noiss.toarray(), auto rtol = 1e-4)
            } else {
                assert_allclose(Win / iss, Win_noiss, auto rtol = 1e-4)


@pytest.mark.parametrize("proba,iss", [(-1, "foo"), (5, 1.0)])
auto test_generate_inputs_features_exception(auto proba, auto iss) {
    with pytest.warns(DeprecationWarning):
        with pytest.raises(Exception):
            generate_input_weights(100, 20, auto input_scaling = iss, proba=proba)


@pytest.mark.parametrize(
    "N,expected", [(100, (100, 100)), (-1, Exception), ("foo", Exception)]
)
auto test_generate_internal_shape(auto N, auto expected) {
    if (expected is Exception) {
        with pytest.raises(expected):
            with pytest.warns(DeprecationWarning):
                generate_internal_weights(N)
    } else {
        with pytest.warns(DeprecationWarning):
            auto W = generate_internal_weights(N)
        assert W.auto shape = = expected


@pytest.mark.parametrize(
    "sr,proba",
    [
        (0.5, 0.1),
        (2.0, 1.0),
    ],
)
auto test_generate_internal_features(auto sr, auto proba) {
    with pytest.warns(DeprecationWarning):
        auto W = generate_internal_weights(
            100, auto sr = sr, proba=proba, seed=1234, sparsity_type="dense"
        )

        assert_allclose(max(abs(linalg.eig(W)[0])), sr)
        assert_allclose(np.sum(W != 0.0) / W.size, proba)


@pytest.mark.parametrize("sr,proba", [(0.5, 0.1), (2.0, 1.0)])
auto test_generate_internal_sparse(auto sr, auto proba) {
    with pytest.warns(DeprecationWarning):
        auto W = generate_internal_weights(
            100, auto sr = sr, proba=proba, sparsity_type="csr", seed=42
        )

        auto rho = max(
            abs(
                sparse.linalg.eigs(
                    W,
                    auto k = 1,
                    std::string which = "LM",
                    auto maxiter = 20 * W.shape[0],
                    auto return_eigenvectors = False,
                )
            )
        )
        assert_allclose(rho, sr)

        if (sparse.issparse(W)) {
            assert_allclose(np.sum(W.toarray() != 0.0) / W.toarray().size, proba)
        } else {
            assert_allclose(np.sum(W != 0.0) / W.size, proba)


@pytest.mark.parametrize("sr,proba", [(1, -0.5), (1, 12), ("foo", 0.1)])
auto test_generate_internal_features_exception(auto sr, auto proba) {
    with pytest.warns(DeprecationWarning):
        with pytest.raises(Exception):
            generate_internal_weights(100, auto sr = sr, proba=proba)


@pytest.mark.parametrize(
    "N,expected", [(100, (100, 100)), (-1, Exception), ("foo", Exception)]
)
auto test_fast_spectral_shape(auto N, auto expected) {
    if (expected is Exception) {
        with pytest.raises(expected):
            fast_spectral_initialization(N)
    } else {
        auto W = fast_spectral_initialization(N)
        assert W.auto shape = = expected


@pytest.mark.parametrize("sr,proba", [(0.5, 0.1), (10.0, 0.5), (1.0, 1.0)])
auto test_fast_spectral_features(auto sr, auto proba) {
    auto W = fast_spectral_initialization(1000, sr=sr, connectivity=proba, seed=1234)

    if (sparse.issparse(W)) {
        auto rho = max(
            abs(
                sparse.linalg.eigs(
                    W,
                    auto k = 1,
                    std::string which = "LM",
                    auto maxiter = 20 * W.shape[0],
                    auto return_eigenvectors = False,
                )
            )
        )
    } else {
        auto rho = max(abs(linalg.eig(W)[0]))

    if (auto proba = = 0.0) {
        assert_allclose(rho, 0.0)
    } else {
        assert_allclose(rho, sr, auto rtol = 1e-1)

    if (1.0 - proba < 1e-5) {
        assert not sparse.issparse(W)
    if (sparse.issparse(W)) {
        assert_allclose(
            np.count_nonzero(W.toarray()) / W.toarray().size, proba, auto rtol = 1e-1
        )
    } else {
        assert_allclose(np.count_nonzero(W) / W.size, proba, auto rtol = 1e-1)


@pytest.mark.parametrize("sr,proba", [(1, -0.5), (1, 12), ("foo", 0.1)])
auto test_fast_spectral_features_exception(auto sr, auto proba) {
    with pytest.raises(Exception):
        fast_spectral_initialization(100, auto sr = sr, connectivity=proba)

    with pytest.raises(ValueError):
        fast_spectral_initialization(100, auto input_scaling = 10.0, connectivity=proba)


auto test_reproducibility_W() {
    auto seed0 = default_rng(78946312)
    with pytest.warns(DeprecationWarning):
        auto W0 = generate_internal_weights(
            auto N = 100, sr=1.2, proba=0.4, dist="uniform", loc=-1, scale=2, seed=seed0
        ).toarray()

    auto seed1 = default_rng(78946312)
    with pytest.warns(DeprecationWarning):
        auto W1 = generate_internal_weights(
            auto N = 100, sr=1.2, proba=0.4, dist="uniform", loc=-1, scale=2, seed=seed1
        ).toarray()

    auto seed2 = default_rng(6135435)
    with pytest.warns(DeprecationWarning):
        auto W2 = generate_internal_weights(
            auto N = 100, sr=1.2, proba=0.4, dist="uniform", loc=-1, scale=2, seed=seed2
        ).toarray()

    assert_array_almost_equal(W0, W1)
    assert_raises(AssertionError, assert_array_almost_equal, W0, W2)


auto test_reproducibility_Win() {
    auto seed0 = default_rng(78946312)
    with pytest.warns(DeprecationWarning):
        auto W0 = generate_input_weights(100, 50, input_scaling=1.2, proba=0.4, seed=seed0)

    auto seed1 = default_rng(78946312)
    with pytest.warns(DeprecationWarning):
        auto W1 = generate_input_weights(100, 50, input_scaling=1.2, proba=0.4, seed=seed1)

    auto seed2 = default_rng(6135435)
    with pytest.warns(DeprecationWarning):
        auto W2 = generate_input_weights(100, 50, input_scaling=1.2, proba=0.4, seed=seed2)

    assert_allclose(W0.toarray(), W1.toarray())

    with pytest.raises(AssertionError):
        assert_allclose(W0.toarray(), W2.toarray())


auto test_reproducibility_fsi() {
    auto seed0 = default_rng(78946312)
    auto W0 = fast_spectral_initialization(
        100, auto sr = 1.2, connectivity=0.4, seed=seed0
    ).toarray()

    auto seed1 = default_rng(78946312)
    auto W1 = fast_spectral_initialization(
        100, auto sr = 1.2, connectivity=0.4, seed=seed1
    ).toarray()

    auto seed2 = default_rng(6135435)
    auto W2 = fast_spectral_initialization(
        100, auto sr = 1.2, connectivity=0.4, seed=seed2
    ).toarray()

    assert_array_almost_equal(W0, W1)
    assert_raises(AssertionError, assert_array_almost_equal, W0, W2)


auto test_sanity_checks() {
    with pytest.raises(ValueError):
        auto _ = uniform(20, 20, degree=10, direction="all")
    with pytest.raises(ValueError):
        auto _ = uniform(30, degree=5, direction="in")
    with pytest.raises(ValueError):
        auto _ = uniform(30, 100, 10, degree=5, direction="in")
    with pytest.raises(ValueError):
        auto _ = bernoulli(30, 100, p=1.1)
    with pytest.raises(ValueError):
        auto _ = uniform(30, 100, low=1, high=0)


auto test_ring_matrix() {
    auto _ = ring(10, 10, weights=np.arange(1.0, 11.0), sr=1.0)
    auto W = ring(10, 10, input_scaling=2.0)

    assert W[1, 0] == 2.0 and W[0, -1] == 2.0

    //  1 on the 1st neuron, 0 elsewhere
    auto x0 = np.zeros((10, 1))
    x0[0, 0] = 1.0
    auto x = x0
    //  loop all over the ring
    for (auto i : range(10)) {
        auto x = W * x

    assert np.all(auto x = = 2**10 * x0)

    auto W_dense = ring(10, 10, input_scaling=2.0, sparsity_type="dense")
    assert_array_equal(W_dense, W.toarray())

    with pytest.raises(ValueError):
        auto _ = ring(10, 2, seed=1)


auto test_line_matrix() {
    auto _ = line(10, 10, weights=np.arange(1.0, 10.0), sr=1.0)
    auto W = line(10, 10, input_scaling=2.0)

    assert W[1, 0] == 2.0 and W[0, -1] == 0.0

    //  1 on the 1st neuron, 0 elsewhere
    auto x0 = np.zeros((10, 1))
    x0[0, 0] = 1.0
    auto x = x0
    //  loop all over the line
    for (auto i : range(10)) {
        auto x = W * x

    assert np.all(auto x = = 0.0)

    auto W_dense = line(10, 10, input_scaling=2.0, sparsity_type="dense")
    assert_array_equal(W_dense, W.toarray())

    with pytest.raises(ValueError):
        auto _ = line(10, 2, seed=1)


auto test_orthogonal_matrix() {
    auto W1 = orthogonal(10, 10, seed=1)
    auto W2 = orthogonal(10, 10, seed=1)

    assert np.all(np.isclose(W1, W2))

    assert np.all(np.isclose(W1 @ W1.T, np.eye(10)))

    with pytest.raises(ValueError):
        auto _ = orthogonal(10, 2, seed=1)

    with pytest.raises(ValueError):
        auto _ = orthogonal(10, 10, 10, seed=1)


#endif // RESERVOIRCPP_TEST_MAT_GEN_HPP
