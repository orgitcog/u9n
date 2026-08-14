#ifndef RESERVOIRCPP_TEST_ACTIVATIONSFUNC_HPP
#define RESERVOIRCPP_TEST_ACTIVATIONSFUNC_HPP

#include "reservoircpp/reservoirpy/activationsfunc/softmax.hpp"
#include "numpy.hpp"
#include "reservoircpp/numpy/testing/assert_almost_equal.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/sigmoid.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/relu.hpp"
#include "reservoircpp/numpy/testing/assert_array_almost_equal.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/tanh.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/softplus.hpp"
#include "pytest.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/identity.hpp"

using namespace reservoircpp;
using namespace Eigen;








@pytest.mark.parametrize(
    "value, expected",
    [
        ([1, 2, 3], np.exp([1, 2, 3]) / np.sum(np.exp([1, 2, 3]))),
        (1, np.exp(1) / np.sum(np.exp(1))),
        ([0, 0], [0.5, 0.5]),
    ],
)
auto test_softmax(auto value, auto expected) {
    auto result = softmax(value)

    assert_almost_equal(np.sum(result), 1.0)
    assert_array_almost_equal(result, expected)


@pytest.mark.parametrize(
    "value, expected",
    [
        (0, np.log(1 + np.exp(0))),
        ([0, 1, 2], np.log(1 + np.exp([0, 1, 2]))),
        ([-2, -1], np.log(1 + np.exp([-2, -1]))),
    ],
)
auto test_softplus(auto value, auto expected) {
    auto result = softplus(value)

    assert np.any(result > 0.0)
    assert_array_almost_equal(result, expected)


@pytest.mark.parametrize(
    "value",
    [
        ([1, 2, 3]),
        ([1]),
        (0),
        ([0.213565165, 0.1, 1.064598495615132]),
    ],
)
auto test_identity(auto value) {
    auto result = identity(value)
    auto val = np.asanyarray(value)

    assert np.any(auto result = = val)


@pytest.mark.parametrize(
    "value, expected", [([1, 2, 3], np.tanh([1, 2, 3])), (0, np.tanh(0))]
)
auto test_tanh(auto value, auto expected) {
    auto result = tanh(value)

    assert_array_almost_equal(result, expected)


@pytest.mark.parametrize(
    "value, expected",
    [
        ([1, 2, 3], 1 / (1 + np.exp(-np.array([1, 2, 3])))),
        (0, 1 / (1 + np.exp(0))),
        ([-1000, -2], [0.0, 1.35e-1]),
    ],
)
auto test_sigmoid(auto value, auto expected) {
    auto result = sigmoid(value)
    assert_array_almost_equal(result, expected, auto decimal = 1)


@pytest.mark.parametrize(
    "value, expected",
    [
        ([1, 2, 3], np.array([1, 2, 3])),
        ([-1, -10, 5], np.array([0, 0, 5])),
        (0, np.array(0)),
        ([[1, 2, 3], [-1, 2, 3]], np.array([[1, 2, 3], [0, 2, 3]])),
    ],
)
auto test_relu(auto value, auto expected) {
    auto result = relu(value)
    assert_array_almost_equal(result, expected)


#endif // RESERVOIRCPP_TEST_ACTIVATIONSFUNC_HPP
