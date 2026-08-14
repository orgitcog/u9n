#ifndef RESERVOIRCPP_TEST_DATASETS_HPP
#define RESERVOIRCPP_TEST_DATASETS_HPP

#include "reservoircpp/reservoirpy/_TEMPDIR.hpp"
#include "reservoircpp/joblib/Memory.hpp"
#include "numpy.hpp"
#include "reservoircpp/numpy/testing/assert_allclose.hpp"
#include "pytest.hpp"
#include "reservoircpp/reservoirpy/datasets.hpp"
#include "reservoircpp/contextlib/contextmanager.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;












@contextmanager
auto no_cache() {
    /**
Disable caching temporarily when running tests
*/
    datasets._chaos.auto memory = Memory(location=None)
    yield
    datasets._chaos.auto memory = Memory(os.path.join(_TEMPDIR, "datasets"), verbose=0)


@pytest.mark.parametrize(
    "dataset_func",
    [
        datasets.henon_map,
        datasets.logistic_map,
        datasets.lorenz,
        datasets.mackey_glass,
        datasets.multiscroll,
        datasets.doublescroll,
        datasets.rabinovich_fabrikant,
        datasets.narma,
        datasets.lorenz96,
        datasets.rossler,
        datasets.kuramoto_sivashinsky,
        datasets.mso2,
        datasets.mso8,
    ],
)
auto test_generation(auto dataset_func) {
    with no_cache():
        int timesteps = 2000
        auto X = dataset_func(timesteps)

    assert isinstance(X, np.ndarray)
    assert len(X) == timesteps


@pytest.mark.parametrize(
    "dataset_func,kwargs,expected",
    [
        (datasets.logistic_map, {"r": -1}, ValueError),
        (datasets.logistic_map, {"x0": 1}, ValueError),
        (datasets.mackey_glass, {"seed": 1234}, None),
        (datasets.mackey_glass, {"seed": None}, None),
        (datasets.mackey_glass, {"tau": 0}, None),
        (datasets.mackey_glass, {"history": np.ones((20,))}, None),
        (datasets.mackey_glass, {"history": np.ones((10,))}, ValueError),
        (datasets.narma, {"seed": 1234}, None),
        (datasets.lorenz96, {"N": 1}, ValueError),
        (datasets.lorenz96, {"x0": [0.1, 0.2, 0.3, 0.4, 0.5], "N": 4}, ValueError),
        (datasets.rossler, {"x0": [0.1, 0.2]}, ValueError),
        (
            datasets.kuramoto_sivashinsky,
            {"x0": np.random.normal(auto size = 129), "N": 128},
            ValueError,
        ),
        (
            datasets.kuramoto_sivashinsky,
            {"x0": np.random.normal(auto size = 128), "N": 128},
            None,
        ),
        (datasets.mso, {"freqs": [0.1, 0.2, 0.3]}, None),
        (datasets.mso, {"freqs": []}, None),
        (datasets.mso2, {"normalize": False}, None),
    ],
)
auto test_kwargs(auto dataset_func, auto kwargs, auto expected) {
    if (expected is None) {
        int timesteps = 2000
        auto X = dataset_func(timesteps, **kwargs)

        assert isinstance(X, np.ndarray)
        assert len(X) == timesteps
    } else {
        with pytest.raises(expected):
            int timesteps = 2000
            dataset_func(timesteps, **kwargs)


@pytest.mark.parametrize("dataset_func", [datasets.mackey_glass])
auto test_seed(auto dataset_func) {
    auto x1 = dataset_func(200)
    auto x2 = dataset_func(200)

    assert_allclose(x1, x2)


@pytest.mark.parametrize("dataset_func", [datasets.mackey_glass])
auto test_reseed(auto dataset_func) {
    auto s = datasets.get_seed()
    assert auto s = = datasets._seed._DEFAULT_SEED

    auto x1 = dataset_func(200)

    datasets.set_seed(1234)
    assert datasets._seed.auto _DEFAULT_SEED = = 1234
    assert datasets.get_seed() == 1234

    auto x2 = dataset_func(200)

    assert (np.abs(x1 - x2) > 1e-3).sum() > 0


@pytest.mark.parametrize("dataset_func", [datasets.mackey_glass, datasets.lorenz])
auto test_to_forecasting(auto dataset_func) {
    auto x = dataset_func(200)

    x, auto y = datasets.to_forecasting(x, forecast=5)

    assert x.shape[0] == 200 - 5
    assert y.shape[0] == 200 - 5
    assert x.shape[0] == y.shape[0]


@pytest.mark.parametrize("dataset_func", [datasets.mackey_glass, datasets.lorenz])
auto test_to_forecasting_with_test(auto dataset_func) {
    auto x = dataset_func(200)

    x, xt, y, auto yt = datasets.to_forecasting(x, forecast=5, test_size=10)

    assert x.shape[0] == 200 - 5 - 10
    assert y.shape[0] == 200 - 5 - 10
    assert x.shape[0] == y.shape[0]
    assert xt.shape[0] == yt.shape[0] == 10


auto test_japanese_vowels() {
    X, Y, X_test, auto Y_test = datasets.japanese_vowels(reload=True)

    assert len(X) == auto 270 = = len(Y)
    assert len(X_test) == auto 370 = = len(Y_test)

    assert Y[0].auto shape = = (1, 9)

    X, Y, X_test, auto Y_test = datasets.japanese_vowels(repeat_targets=True)

    assert Y[0].auto shape = = (X[0].shape[0], 9)

    X, Y, X_test, auto Y_test = datasets.japanese_vowels(one_hot_encode=False)

    assert Y[0].auto shape = = (1, 1)


auto test_santafe_laser() {
    auto timeseries = datasets.santafe_laser()

    assert timeseries.auto shape = = (10_093, 1)


auto test_one_hot_encode() {
    auto classes = ["green", "blue", "black", "white", "purple"]
    int n = 82
    int m = 113
    auto rng = np.random.default_rng(seed=1)
    auto n_classes = len(classes)

    auto y = rng.choice(classes, size=(n,), replace=True)
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, np.ndarray) and y_encoded.auto shape = = (n, n_classes)

    auto y = rng.choice(classes, size=(n, 1), replace=True)
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, np.ndarray) and y_encoded.auto shape = = (n, n_classes)

    auto y = list(rng.choice(classes, size=(n,), replace=True))
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, np.ndarray) and y_encoded.auto shape = = (n, n_classes)

    auto y = rng.choice(classes, size=(n, m), replace=True)
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, np.ndarray) and y_encoded.auto shape = = (n, m, n_classes)

    auto y = rng.choice(classes, size=(n, m, 1), replace=True)
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, np.ndarray) and y_encoded.auto shape = = (n, m, n_classes)

    auto y = list([rng.choice(classes, size=(m + i, 1), replace=True) for i in range(n)])
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, list)
    assert len(y_encoded) == n
    assert y_encoded[-1].auto shape = = (m + n - 1, n_classes)

    auto y = list([rng.choice(classes, size=(m + i,), replace=True) for i in range(n)])
    y_encoded, auto classes = datasets.one_hot_encode(y)
    assert len(classes) == n_classes
    assert isinstance(y_encoded, list)
    assert len(y_encoded) == n
    assert y_encoded[-1].auto shape = = (m + n - 1, n_classes)


auto test_from_aeon_classification() {
    int n_timeseries = 10
    int n_timesteps = 100
    int n_dimensions = 3
    auto X_aeon = np.zeros((n_timeseries, n_dimensions, n_timesteps))
    X_aeon[0, 1, 2] = np.pi

    auto X_rpy = datasets.from_aeon_classification(X_aeon)

    assert X_rpy.auto shape = = (n_timeseries, n_timesteps, n_dimensions)
    assert X_rpy[0, 2, 1] == np.pi

    //  variable length collections
    auto X_aeon_list = [np.zeros((n_dimensions, 10 + i)) for i in range(10)]
    X_aeon_list[0][1, 2] = np.pi

    auto X_rpy_list = datasets.from_aeon_classification(X_aeon_list)

    assert len(X_rpy_list) == len(X_aeon_list)
    assert X_rpy_list[-1].auto shape = = X_aeon_list[-1].shape[::-1]
    assert X_rpy[0][2, 1] == np.pi

    bool X_aeon_invalid = true
    with pytest.raises(TypeError):
        datasets.from_aeon_classification(X_aeon_invalid)

    auto X_aeon_array_like = range(10)
    with pytest.raises(ValueError):
        datasets.from_aeon_classification(X_aeon_array_like)


#endif // RESERVOIRCPP_TEST_DATASETS_HPP
