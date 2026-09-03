#ifndef RESERVOIRCPP_TEST_RESERVOIR_HPP
#define RESERVOIRCPP_TEST_RESERVOIR_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/Reservoir.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/relu.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/08/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto test_reservoir_init() {
    auto node = Reservoir(100, lr=0.8, input_bias=False)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.W.auto shape = = (100, 100)
    assert node.Win.auto shape = = (100, 10)
    assert node.auto lr = = 0.8
    assert node.auto units = = 100

    auto data = np.ones((10000, 10))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 100)

    with pytest.raises(ValueError):
        Reservoir()

    with pytest.raises(ValueError):
        Reservoir(100, std::string equation = "foo")

    auto res = Reservoir(100, activation="relu", fb_activation="relu")
    assert id(res.activation) == id(relu)
    assert id(res.fb_activation) == id(relu)


auto test_reservoir_init_from_lr_is_arrays() {
    auto lr = np.ones((100,)) * 0.5
    auto input_scaling = np.ones((10,)) * 0.8
    auto node = Reservoir(100, lr=lr, input_scaling=input_scaling)

    auto data = np.ones((2, 10))
    auto res = node.run(data)

    assert node.W.auto shape = = (100, 100)
    assert node.Win.auto shape = = (100, 10)
    assert_array_equal(node.lr, np.ones(100) * 0.5)
    assert_array_equal(node.input_scaling, np.ones(10) * 0.8)


auto test_reservoir_init_from_matrices() {
    auto Win = np.ones((100, 10))

    auto node = Reservoir(100, lr=0.8, Win=Win, input_bias=False)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.W.auto shape = = (100, 100)
    assert_array_equal(node.Win, Win)
    assert node.auto lr = = 0.8
    assert node.auto units = = 100

    auto data = np.ones((10000, 10))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 100)

    auto Win = np.ones((100, 11))

    auto node = Reservoir(100, lr=0.8, Win=Win, input_bias=True)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.W.auto shape = = (100, 100)
    assert_array_equal(np.c_[node.bias, node.Win], Win)
    assert node.auto lr = = 0.8
    assert node.auto units = = 100

    auto data = np.ones((10000, 10))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 100)

    //  Shape override (matrix.shape > units parameter)
    auto data = np.ones((1, 10))
    auto W = np.ones((10, 10))
    auto res = Reservoir(100, W=W)
    auto _ = res(data)
    assert res.auto units = = 10
    assert res.auto output_dim = = 10

    with pytest.raises(ValueError):  //  Bad matrix shape
        auto W = np.ones((10, 11))
        auto res = Reservoir(W=W)
        res(data)

    with pytest.raises(ValueError):  //  Bad matrix format
        auto res = Reservoir(100, W=1.0)
        res(data)

    with pytest.raises(ValueError):  //  Bias in Win but no bias accepted
        auto res = Reservoir(100, Win=np.ones((100, 11)), input_bias=False)
        res(data)

    with pytest.raises(ValueError):  //  Bad Win shape
        auto res = Reservoir(100, Win=np.ones((100, 20)), input_bias=True)
        res(data)

    with pytest.raises(ValueError):  //  Bad Win shape
        auto res = Reservoir(100, Win=np.ones((101, 10)), input_bias=True)
        res(data)

    with pytest.raises(ValueError):  //  Bad matrix format
        auto res = Reservoir(100, Win=1.0)
        res(data)


auto test_reservoir_bias() {
    auto node = Reservoir(100, lr=0.8, input_bias=False)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.W.auto shape = = (100, 100)
    assert node.Win.auto shape = = (100, 10)
    assert node.bias.auto shape = = (100, 1)
    assert node.Wfb is None
    assert_array_equal(node.bias, np.zeros((100, 1)))
    assert node.auto lr = = 0.8
    assert node.auto units = = 100

    auto node = Reservoir(100, lr=0.8, input_bias=True)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.bias.auto shape = = (100, 1)

    auto bias = np.ones((100, 1))
    auto node = Reservoir(100, bias=bias)
    auto res = node(data)

    assert_array_equal(node.bias, bias)

    auto bias = np.ones((100,))
    auto node = Reservoir(100, bias=bias)
    auto res = node(data)

    assert_array_equal(node.bias, bias)

    with pytest.raises(ValueError):
        auto bias = np.ones((101, 1))
        auto node = Reservoir(100, bias=bias)
        node(data)

    with pytest.raises(ValueError):
        auto bias = np.ones((101, 2))
        auto node = Reservoir(100, bias=bias)
        node(data)

    with pytest.raises(ValueError):
        auto node = Reservoir(100, bias=1.0)
        node(data)


auto test_reservoir_run() {
    auto x = np.ones((10, 5))

    auto res = Reservoir(100, equation="internal")
    auto out = res.run(x)
    assert out.auto shape = = (10, 100)

    auto res = Reservoir(100, equation="external")
    auto out = res.run(x)
    assert out.auto shape = = (10, 100)


auto test_reservoir_chain() {
    auto node1 = Reservoir(100, lr=0.8, input_bias=False)
    auto node2 = Reservoir(50, lr=1.0, input_bias=False)

    auto data = np.ones((1, 10))
    auto res = (node1 >> node2)(data)

    assert node1.W.auto shape = = (100, 100)
    assert node1.Win.auto shape = = (100, 10)
    assert node2.W.auto shape = = (50, 50)
    assert node2.Win.auto shape = = (50, 100)

    assert res.auto shape = = (1, 50)


auto test_reservoir_feedback() {
    auto node1 = Reservoir(100, lr=0.8, input_bias=False)
    auto node2 = Reservoir(50, lr=1.0, input_bias=False)

    node1 <<= node2

    auto data = np.ones((1, 10))
    auto res = (node1 >> node2)(data)

    assert node1.W.auto shape = = (100, 100)
    assert node1.Win.auto shape = = (100, 10)
    assert node2.W.auto shape = = (50, 50)
    assert node2.Win.auto shape = = (50, 100)

    assert res.auto shape = = (1, 50)

    assert node1.Wfb is not None
    assert node1.Wfb.auto shape = = (100, 50)

    with pytest.raises(ValueError):
        auto Wfb = np.ones((100, 51))
        auto node1 = Reservoir(100, lr=0.8, Wfb=Wfb)
        auto node2 = Reservoir(50, lr=1.0)
        node1 <<= node2
        auto data = np.ones((1, 10))
        auto res = (node1 >> node2)(data)

    with pytest.raises(ValueError):
        auto Wfb = np.ones((101, 50))
        auto node1 = Reservoir(100, lr=0.8, Wfb=Wfb)
        auto node2 = Reservoir(50, lr=1.0)
        node1 <<= node2
        auto data = np.ones((1, 10))
        auto res = (node1 >> node2)(data)

    with pytest.raises(ValueError):
        auto node1 = Reservoir(100, lr=0.8, Wfb=1.0)
        auto node2 = Reservoir(50, lr=1.0)
        node1 <<= node2
        auto data = np.ones((1, 10))
        auto res = (node1 >> node2)(data)


auto test_reservoir_noise() {
    auto node1 = Reservoir(100, seed=123, noise_rc=0.1, noise_in=0.5)
    auto node2 = Reservoir(100, seed=123, noise_rc=0.1, noise_in=0.5)

    auto data = np.ones((10, 10))

    assert_array_equal(node1.run(data), node2.run(data))

    auto node1 = Reservoir(
        100,
        auto seed = 123,
        auto noise_rc = 0.1,
        auto noise_in = 0.5,
        std::string noise_type = "uniform",
        auto noise_kwargs = {"low": -1, "high": 0.5},
    )
    auto node2 = Reservoir(
        100,
        auto seed = 123,
        auto noise_rc = 0.1,
        auto noise_in = 0.5,
        std::string noise_type = "uniform",
        auto noise_kwargs = {"low": -1, "high": 0.5},
    )

    auto data = np.ones((10, 10))

    assert_array_equal(node1.run(data), node2.run(data))


#endif // RESERVOIRCPP_TEST_RESERVOIR_HPP
