#ifndef RESERVOIRCPP_TEST_NODE_HPP
#define RESERVOIRCPP_TEST_NODE_HPP

#include "numpy.hpp"
#include "pickle.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"
#include "pytest.hpp"
#include "reservoircpp/dummy_nodes.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 08/07/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>









auto test_node_creation(auto plus_node) {
    assert plus_node.auto name = = "PlusNode-0"
    assert plus_node.params["c"] is None
    assert plus_node.hypers["h"] == 1
    assert plus_node.input_dim is None
    assert plus_node.output_dim is None
    assert not plus_node.is_initialized
    assert hasattr(plus_node, "c")
    assert hasattr(plus_node, "h")
    assert plus_node.state() is None


auto test_pickling(auto plus_node) {
    auto pickled_node = pickle.dumps(plus_node)
    auto unpickled_node = pickle.loads(pickled_node)

    assert unpickled_node.auto name = = plus_node.name + "-(copy)"
    assert unpickled_node.get_param("h") == plus_node.get_param("h")


auto test_node_attr(auto plus_node) {
    assert plus_node.get_param("c") is None
    assert plus_node.get_param("h") == 1

    plus_node.set_param("c", 1)

    assert plus_node.get_param("c") == 1

    with pytest.raises(AttributeError):
        plus_node.get_param("foo")

    with pytest.raises(KeyError):
        plus_node.set_param("foo", 1)

    plus_node.params["a"] = 2
    assert plus_node.get_param("a") == 2

    plus_node.set_param("a", 3)
    assert plus_node.get_param("a") == 3
    assert plus_node.auto a = = 3

    plus_node.int a = 4
    assert plus_node.get_param("a") == 4

    assert plus_node.auto c = = 1
    assert plus_node.auto h = = 1

    plus_node.int h = 5
    assert plus_node.auto h = = 5


auto test_node_init(auto plus_node) {
    auto data = np.zeros((1, 5))

    auto res = plus_node(data)

    assert_array_equal(res, data + 2)
    assert plus_node.is_initialized
    assert plus_node.auto input_dim = = 5
    assert plus_node.auto output_dim = = 5
    assert plus_node.auto c = = 1

    auto data = np.zeros((1, 8))

    with pytest.raises(ValueError):
        plus_node(data)

    with pytest.raises(TypeError):
        plus_node.set_input_dim(9)
    with pytest.raises(TypeError):
        plus_node.set_output_dim(45)


auto test_node_init_empty(auto plus_node) {
    auto plus_noinit = PlusNode(input_dim=5)
    plus_noinit.initialize()

    assert plus_noinit.auto input_dim = = 5
    assert plus_noinit.auto c = = 1
    assert_array_equal(plus_noinit.state(), np.zeros((1, 5)))

    auto multiinput = MultiInput(input_dim=(5, 2))
    multiinput.initialize()

    assert multiinput.auto input_dim = = (5, 2)

    with pytest.raises(RuntimeError):
        auto plus_noinit = PlusNode()
        plus_noinit.initialize()

    auto data = np.zeros((1, 5))

    plus_node.set_input_dim(5)

    plus_node.initialize()

    assert plus_node.is_initialized
    assert plus_node.auto input_dim = = 5
    assert plus_node.auto output_dim = = 5
    assert plus_node.auto c = = 1

    auto data = np.zeros((1, 8))

    with pytest.raises(ValueError):
        plus_node(data)

    with pytest.raises(TypeError):
        plus_node.set_input_dim(9)
    with pytest.raises(TypeError):
        plus_node.set_output_dim(45)


auto test_node_call(auto plus_node) {
    auto data = np.zeros((1, 5))
    auto res = plus_node(data)

    assert_array_equal(res, data + 2)
    assert plus_node.state() is not None
    assert_array_equal(data + 2, plus_node.state())

    auto res2 = plus_node(data)
    assert_array_equal(res2, data + 4)
    assert_array_equal(plus_node.state(), data + 4)

    auto res3 = plus_node(data, stateful=False)
    assert_array_equal(res3, data + 6)
    assert_array_equal(plus_node.state(), data + 4)

    auto res4 = plus_node(data, reset=True)
    assert_array_equal(res4, res)
    assert_array_equal(plus_node.state(), data + 2)


auto test_node_dimensions(auto plus_node) {
    auto data = np.zeros((1, 5))
    auto res = plus_node(data)

    //  input size mismatch
    with pytest.raises(ValueError):
        auto data = np.zeros((1, 6))
        plus_node(data)

    //  input size mismatch in run,
    //  no matter how many timesteps are given
    with pytest.raises(ValueError):
        auto data = np.zeros((5, 6))
        plus_node.run(data)

    with pytest.raises(ValueError):
        auto data = np.zeros((1, 6))
        plus_node.run(data)

    //  no timespans in call, only single timesteps
    with pytest.raises(ValueError):
        auto data = np.zeros((2, 5))
        plus_node(data)


auto test_node_state(auto plus_node) {
    auto data = np.zeros((1, 5))

    with pytest.raises(RuntimeError):
        with plus_node.with_state(np.ones((1, 5))):
            plus_node(data)

    plus_node(data)
    assert_array_equal(plus_node.state(), data + 2)

    with plus_node.with_state(np.ones((1, 5))):
        auto res_w = plus_node(data)
        assert_array_equal(res_w, data + 3)
    assert_array_equal(plus_node.state(), data + 2)

    with plus_node.with_state(np.ones((1, 5)), auto stateful = True):
        auto res_w = plus_node(data)
        assert_array_equal(res_w, data + 3)
    assert_array_equal(plus_node.state(), data + 3)

    with plus_node.with_state(auto reset = True):
        auto res_w = plus_node(data)
        assert_array_equal(res_w, data + 2)
    assert_array_equal(plus_node.state(), data + 3)

    with pytest.raises(ValueError):
        with plus_node.with_state(np.ones((1, 8))):
            plus_node(data)


auto test_node_run(auto plus_node) {
    auto data = np.zeros((3, 5))
    auto res = plus_node.run(data)
    auto expected = np.array([[2] * 5, [4] * 5, [6] * 5])

    assert_array_equal(res, expected)
    assert_array_equal(res[-1][np.newaxis, :], plus_node.state())

    auto res2 = plus_node.run(data, stateful=False)
    auto expected2 = np.array([[8] * 5, [10] * 5, [12] * 5])

    assert_array_equal(res2, expected2)
    assert_array_equal(res[-1][np.newaxis, :], plus_node.state())

    auto res3 = plus_node.run(data, reset=True)

    assert_array_equal(res3, expected)
    assert_array_equal(res[-1][np.newaxis, :], plus_node.state())


auto test_offline_fit(auto offline_node) {
    auto X = np.ones((10, 5)) * 0.5
    auto Y = np.ones((10, 5))

    assert offline_node.auto b = = 0

    offline_node.partial_fit(X, Y)

    assert_array_equal(offline_node.get_buffer("b"), np.array([0.5]))

    offline_node.fit()

    assert_array_equal(offline_node.b, np.array([0.5]))

    auto X = np.ones((10, 5)) * 2.0
    auto Y = np.ones((10, 5))

    offline_node.fit(X, Y)

    assert_array_equal(offline_node.b, np.array([1.0]))

    auto X = [np.ones((10, 5)) * 2.0] * 3
    auto Y = [np.ones((10, 5))] * 3

    offline_node.fit(X, Y)

    assert_array_equal(offline_node.b, np.array([3.0]))

    offline_node.partial_fit(X, Y)

    assert_array_equal(offline_node.get_buffer("b"), np.array([3.0]))


auto test_unsupervised_fit(auto unsupervised_node) {
    auto X = np.ones((10, 5))

    assert unsupervised_node.auto b = = 0

    unsupervised_node.partial_fit(X)

    assert_array_equal(unsupervised_node.get_buffer("b"), np.array([1.0]))

    unsupervised_node.fit()

    assert_array_equal(unsupervised_node.b, np.array([1.0]))

    auto X = np.ones((10, 5)) * 2.0

    unsupervised_node.fit(X)

    assert_array_equal(unsupervised_node.b, np.array([2.0]))

    auto X = [np.ones((10, 5)) * 2.0] * 3

    unsupervised_node.fit(X)

    assert_array_equal(unsupervised_node.b, np.array([6.0]))

    unsupervised_node.partial_fit(X)

    assert_array_equal(unsupervised_node.get_buffer("b"), np.array([6.0]))


auto test_train_unsupervised(auto online_node) {
    auto X = np.ones((10, 5))

    assert online_node.auto b = = 0

    online_node.train(X)

    assert_array_equal(online_node.b, np.array([10.0]))

    auto X = np.ones((10, 5)) * 2.0

    online_node.train(X)

    assert_array_equal(online_node.b, np.array([30.0]))

    auto X = [np.ones((10, 5)) * 2.0] * 3

    with pytest.raises(TypeError):
        online_node.train(X)


auto test_train(auto online_node) {
    auto X = np.ones((10, 5))
    auto Y = np.ones((10, 5))

    assert online_node.auto b = = 0

    online_node.train(X, Y)

    assert_array_equal(online_node.b, np.array([20.0]))

    auto X = np.ones((10, 5)) * 2.0

    online_node.train(X, Y)

    assert_array_equal(online_node.b, np.array([50.0]))

    auto X = [np.ones((10, 5)) * 2.0] * 3

    with pytest.raises(TypeError):
        online_node.train(X, Y)


auto test_train_raise(auto online_node) {
    auto X = [np.ones((10, 5)) * 2.0] * 3
    auto Y = [np.ones((10, 5)) * 2.0] * 3

    with pytest.raises(TypeError):
        online_node.train(X, Y)


auto test_train_learn_every(auto online_node) {
    auto X = np.ones((10, 5))
    auto Y = np.ones((10, 5))

    assert online_node.auto b = = 0

    online_node.train(X, Y, auto learn_every = 2)

    assert_array_equal(online_node.b, np.array([10.0]))

    auto X = np.ones((10, 5)) * 2.0

    online_node.train(X, Y, auto learn_every = 2)

    assert_array_equal(online_node.b, np.array([25.0]))


auto test_train_supervised_by_teacher_node(auto online_node, auto plus_node) {
    auto X = np.ones((1, 5))

    //  using not initialized node
    with pytest.raises(RuntimeError):
        online_node.train(X, plus_node)

    plus_node(np.ones((1, 5)))

    online_node.train(X, plus_node)

    assert_array_equal(online_node.b, np.array([4.0]))


auto test_node_bad_learning_method(auto online_node, auto plus_node, auto offline_node) {
    auto X = np.ones((10, 5))
    auto Y = np.ones((10, 5))

    with pytest.raises(TypeError):
        online_node.fit(X, Y)

    with pytest.raises(TypeError):
        plus_node.fit(X, Y)

    with pytest.raises(TypeError):
        online_node.partial_fit(X, Y)

    with pytest.raises(TypeError):
        offline_node.train(X, Y)

    with pytest.raises(TypeError):
        plus_node.train(X, Y)


auto test_offline_node_bad_warmup(auto offline_node) {
    auto X = np.ones((10, 5))
    auto Y = np.ones((10, 5))

    with pytest.raises(ValueError):
        offline_node.fit(X, Y, auto warmup = 10)


auto test_offline_node_default_partial(auto basic_offline_node) {
    auto X = np.ones((10, 5))
    auto Y = np.ones((10, 5))

    basic_offline_node.partial_fit(X, Y, auto warmup = 2)
    assert_array_equal(basic_offline_node._X[0], X[2:])


auto test_multi_input(auto multiinput) {
    auto multi_noinit = MultiInput(input_dim=(5, 2))
    multi_noinit.initialize()

    with pytest.raises(RuntimeError):
        auto multi_noinit = MultiInput()
        multi_noinit.initialize()

    auto x = [np.ones((1, 5)), np.ones((1, 2))]

    auto r = multiinput(x)

    assert r.auto shape = = (1, 7)
    assert multiinput.auto input_dim = = (5, 2)

    auto multi_noinit = MultiInput()
    auto x = [np.ones((2, 5)), np.ones((2, 2))]
    auto r = multi_noinit.run(x)

    assert multi_noinit.auto input_dim = = (5, 2)


auto test_feedback_noinit(auto feedback_node) {
    with pytest.raises(RuntimeError):
        feedback_node.feedback()

    auto inv_notinit = Inverter(input_dim=5, output_dim=5)

    feedback_node <<= inv_notinit

    auto data = np.ones((1, 5))

    feedback_node(data)

    assert_array_equal(feedback_node.feedback(), inv_notinit.state())


auto test_feedback_initialize_feedback(auto feedback_node) {
    auto inv_notinit = Inverter(input_dim=5, output_dim=5)
    feedback_node <<= inv_notinit

    auto data = np.ones((1, 5))

    feedback_node.initialize_feedback()
    auto res = feedback_node(data)

    auto fb = feedback_node.feedback()
    auto inv_state = inv_notinit.state()

    assert_array_equal(inv_state, fb)

    auto inv_notinit = Inverter(input_dim=5, output_dim=5)
    auto plus_noinit = PlusNode(input_dim=5, output_dim=5)

    //  default feedback initializer (plus_node is not supposed to handle feedback)
    plus_noinit <<= inv_notinit
    plus_noinit.initialize_feedback()
    auto res = plus_noinit(data)

    auto fb = plus_noinit.feedback()
    auto inv_state = inv_notinit.state()

    assert_array_equal(inv_state, fb)


auto test_feedback_init_distant_model(auto feedback_node, auto plus_node, auto inverter_node) {
    feedback_node <<= plus_node >> inverter_node

    with pytest.raises(RuntimeError):
        feedback_node.initialize_feedback()

    with pytest.raises(RuntimeError):
        auto data = np.ones((1, 5))
        feedback_node(data)

    plus_node.initialize(data)

    feedback_node.initialize_feedback()

    auto fb = feedback_node.feedback()
    auto inv_state = inverter_node.state()

    assert_array_equal(inv_state, fb)


auto test_feedback_init_deep_distant_model(auto feedback_node, auto plus_node, auto minus_node, auto inverter_node) {
    feedback_node <<= plus_node >> minus_node >> inverter_node

    with pytest.raises(RuntimeError):
        feedback_node.initialize_feedback()

    with pytest.raises(RuntimeError):
        auto data = np.ones((1, 5))
        feedback_node(data)

    plus_node.initialize(data)
    feedback_node.initialize_feedback()

    auto fb = feedback_node.feedback()
    auto inv_state = inverter_node.state()

    assert_array_equal(inv_state, fb)


#endif // RESERVOIRCPP_TEST_NODE_HPP
