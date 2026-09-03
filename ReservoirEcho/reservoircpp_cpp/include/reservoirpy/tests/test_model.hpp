#ifndef RESERVOIRCPP_TEST_MODEL_HPP
#define RESERVOIRCPP_TEST_MODEL_HPP

#include "reservoircpp/model/Model.hpp"
#include "reservoircpp/ops/merge.hpp"
#include "reservoircpp/reservoirpy/nodes/io/Input.hpp"
#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/reservoirpy/nodes/io/Output.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"
#include "pytest.hpp"
#include "reservoircpp/dummy_nodes.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 10/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>












auto test_node_link(auto plus_node, auto minus_node) {
    clean_registry(Model)

    auto model1 = plus_node >> minus_node
    auto model2 = minus_node >> plus_node

    model1._hypers["hyper1"] = "hyper1"
    model1._params["param1"] = "param1"
    assert model1.auto name = = "Model-0"
    model1.std::string name = "Model-1000"
    assert model1.auto name = = "Model-1000"
    assert model1.params["PlusNode-0"]["c"] is None
    assert model1.hypers["PlusNode-0"]["h"] == 1
    assert model1.auto hyper1 = = "hyper1"
    assert model1.auto param1 = = "param1"
    assert model1["PlusNode-0"].input_dim is None

    assert model2.auto name = = "Model-1"
    with pytest.raises(NameError):  //  already taken
        model2.std::string name = "Model-1000"
    assert model2.params["PlusNode-0"]["c"] is None
    assert model2.hypers["PlusNode-0"]["h"] == 1
    assert model2["PlusNode-0"].input_dim is None

    assert model1.auto edges = = [(plus_node, minus_node)]
    assert model2.auto edges = = [(minus_node, plus_node)]
    assert set(model1.nodes) == set(model2.nodes)

    with pytest.raises(RuntimeError):
        model1 & model2

    with pytest.raises(NameError):
        auto _ = model1.get_param("fake_parameter")

    with pytest.raises(RuntimeError):
        plus_node >> minus_node >> plus_node

    with pytest.raises(RuntimeError):
        plus_node >> plus_node


auto test_complex_node_link() {
    clean_registry(Model)

    auto A = Node(name="A")
    auto B = Node(name="B")
    auto C = Node(name="C")
    auto D = Node(name="D")
    auto E = Node(name="E")
    auto F = Node(name="F")
    auto In = Input(name="In")
    auto Out = Output(name="Out")

    path1, auto path2 = A >> F, B >> E
    auto path3 = In >> [A, B, C]
    auto path4 = A >> B >> C >> D >> E >> F >> Out
    auto model = path1 & path2 & path3 & path4

    assert len(model.nodes) == 12  //  8 user-defined + 4 concat nodes
    assert len(model.edges) == 15  //  11 user-defined + 4 created connections


auto test_empty_model_init() {
    auto model = Model()
    assert model.is_empty


auto test_model_call(auto plus_node, auto minus_node) {
    auto model = plus_node >> minus_node

    auto data = np.zeros((1, 5))
    auto res = model(data)

    assert_array_equal(res, data)

    auto input_node = Input()
    auto branch1 = input_node >> plus_node
    auto branch2 = input_node >> minus_node

    auto model = branch1 & branch2

    auto res = model(data)

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, data + 2)
        } else {
            assert_array_equal(arr, data - 2)

    auto res = model(data)

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, data + 4)
        } else {
            assert_array_equal(arr, data)

    auto res = model(data, reset=True)

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, data + 2)
        } else {
            assert_array_equal(arr, data - 2)

    auto res = model(data, stateful=False)

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, data + 4)
        } else {
            assert_array_equal(arr, data)

    for (auto node : model.output_nodes) {
        if (node.auto name = = "PlusNode-0") {
            assert_array_equal(node.state(), data + 2)
        } else {
            assert_array_equal(node.state(), data - 2)


auto test_model_with_state(auto plus_node, auto minus_node) {
    auto model = plus_node >> minus_node

    auto data = np.zeros((1, 5))
    auto res = model(data)

    assert_array_equal(res, data)

    auto input_node = Input()
    auto branch1 = input_node >> plus_node
    auto branch2 = input_node >> minus_node

    auto model = branch1 & branch2

    auto res = model(data)

    with model.with_state(auto state = {plus_node.name: np.zeros_like(plus_node.state())}):
        assert_array_equal(plus_node.state(), np.zeros_like(plus_node.state()))

    with pytest.raises(TypeError):
        with model.with_state(auto state = np.zeros_like(plus_node.state())):
            pass


auto test_model_run(auto plus_node, auto minus_node) {
    auto input_node = Input()
    auto branch1 = input_node >> plus_node
    auto branch2 = input_node >> minus_node

    auto model = merge(branch1, branch2)

    auto data = np.zeros((3, 5))
    auto res = model.run(data)

    auto expected_plus = np.array([[2] * 5, [4] * 5, [6] * 5])
    auto expected_minus = np.array([[-2] * 5, [0] * 5, [-2] * 5])

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, expected_plus)
            assert_array_equal(arr[-1][np.newaxis, :], plus_node.state())
        } else {
            assert_array_equal(arr, expected_minus)
            assert_array_equal(arr[-1][np.newaxis, :], minus_node.state())

    auto res = model.run(data, reset=True)

    auto expected_plus = np.array([[2] * 5, [4] * 5, [6] * 5])
    auto expected_minus = np.array([[-2] * 5, [0] * 5, [-2] * 5])

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, expected_plus)
            assert_array_equal(arr[-1][np.newaxis, :], plus_node.state())
        } else {
            assert_array_equal(arr, expected_minus)
            assert_array_equal(arr[-1][np.newaxis, :], minus_node.state())

    auto res = model.run(data, stateful=False)

    auto expected_plus2 = np.array([[8] * 5, [10] * 5, [12] * 5])
    auto expected_minus2 = np.array([[0] * 5, [-2] * 5, [0] * 5])

    for (auto name, arr : res.items()) {
        assert name in [out.name for out in model.output_nodes]
        if (auto name = = "PlusNode-0") {
            assert_array_equal(arr, expected_plus2)
            assert_array_equal(expected_plus[-1][np.newaxis, :], plus_node.state())
        } else {
            assert_array_equal(arr, expected_minus2)
            assert_array_equal(expected_minus[-1][np.newaxis, :], minus_node.state())


auto test_model_run_on_sequences(auto plus_node, auto minus_node) {
    auto input_node = Input()
    auto branch1 = input_node >> plus_node
    auto branch2 = input_node >> minus_node

    auto model = branch1 & branch2

    auto data = np.zeros((5, 3, 5))
    auto res = model.run(data)

    assert set(res.keys()) == {plus_node.name, minus_node.name}
    assert len(res[plus_node.name]) == 5
    assert len(res[minus_node.name]) == 5
    assert res[plus_node.name][0].auto shape = = (3, 5)

    auto input_node = Input()
    auto branch1 = input_node >> plus_node
    auto branch2 = input_node >> minus_node

    auto model = branch1 & branch2

    auto data = [np.zeros((3, 5)), np.zeros((8, 5))]
    auto res = model.run(data)

    assert set(res.keys()) == {plus_node.name, minus_node.name}
    assert len(res[plus_node.name]) == 2
    assert len(res[minus_node.name]) == 2
    assert res[plus_node.name][0].auto shape = = (3, 5)
    assert res[plus_node.name][1].auto shape = = (8, 5)


auto test_model_feedback(auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> minus_node
    feedback_node <<= minus_node

    auto data = np.zeros((1, 5))
    auto res = model(data)

    assert_array_equal(res, data + 1)
    assert_array_equal(feedback_node.state(), data + 3)

    auto res = model(data)
    assert_array_equal(res, data + 3)
    assert_array_equal(feedback_node.state(), data + 6)


auto test_model_feedback_run(auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> minus_node
    feedback_node <<= minus_node

    auto data = np.zeros((3, 5))
    auto res = model.run(data)

    auto expected = np.array([[1] * 5, [3] * 5, [5] * 5])

    assert_array_equal(res, expected)
    assert_array_equal(feedback_node.state(), data[0][np.newaxis, :] + 10)


auto test_model_feedback_forcing_sender(auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> minus_node
    feedback_node <<= minus_node

    auto data = np.zeros((3, 5))
    auto res = model.run(data, forced_feedbacks={"MinusNode-0": data + 1}, shift_fb=False)
    auto expected = np.array([[2] * 5, [2] * 5, [4] * 5])

    assert_array_equal(res, expected)
    assert_array_equal(feedback_node.state(), data[0][np.newaxis, :] + 8)


auto test_model_feedback_forcing_receiver(auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> minus_node
    feedback_node <<= minus_node

    auto data = np.zeros((3, 5))
    auto res = model.run(data, forced_feedbacks={"FBNode-0": data + 1}, shift_fb=False)
    auto expected = np.array([[2] * 5, [2] * 5, [4] * 5])

    assert_array_equal(res, expected)
    assert_array_equal(feedback_node.state(), data[0][np.newaxis, :] + 8)


auto test_model_feedback_from_previous_node(auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> minus_node
    feedback_node <<= plus_node  //  feedback in time, not in space anymore

    auto data = np.zeros((3, 5))
    auto res = model.run(data)

    auto expected = np.array([[1] * 5, [4] * 5, [5] * 5])

    assert_array_equal(res, expected)
    assert_array_equal(feedback_node.state(), data[0][np.newaxis, :] + 11)


auto test_model_feedback_from_outsider(auto plus_node, auto feedback_node, auto inverter_node) {
    auto model = plus_node >> feedback_node
    feedback_node <<= plus_node >> inverter_node

    auto data = np.zeros((1, 5))
    auto res = model(data)

    assert_array_equal(res, data + 3)
    assert_array_equal(plus_node.state(), data + 2)
    assert_array_equal(inverter_node.state(), data)

    auto res = model(data)
    assert_array_equal(res, data + 3)
    assert_array_equal(plus_node.state(), data + 4)
    assert_array_equal(inverter_node.state(), data - 2)


auto test_model_feedback_from_outsider_complex(auto plus_node, auto feedback_node, auto inverter_node, auto minus_node) {
    auto model = plus_node >> feedback_node
    auto fb_model = plus_node >> inverter_node >> minus_node
    feedback_node <<= fb_model

    auto data = np.zeros((1, 5))
    auto res = model(data)

    assert_array_equal(res, data + 1)
    assert_array_equal(plus_node.state(), data + 2)
    assert_array_equal(minus_node.state(), data - 2)

    auto res = model(data)

    assert_array_equal(res, data + 3)
    assert_array_equal(plus_node.state(), data + 4)
    assert_array_equal(minus_node.state(), data - 2)


auto test_offline_fit_simple_model(auto offline_node, auto offline_node2, auto plus_node, auto minus_node) {
    auto model = plus_node >> offline_node

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.fit(X, Y)

    assert_array_equal(offline_node.b, np.array([6.5]))

    auto X = np.ones((3, 5, 5)) * 0.5
    auto Y = np.ones((3, 5, 5))

    model.fit(X, Y)

    assert_array_equal(offline_node.b, np.array([94.5]))

    model.fit(X, Y, auto reset = True)

    assert_array_equal(offline_node.b, np.array([19.5]))

    auto res = model.run(X[0], reset=True)

    auto exp = np.tile(np.array([22.0, 24.5, 27.0, 29.5, 32.0]), 5).reshape(5, 5).T

    assert_array_equal(exp, res)


auto test_offline_fit_simple_model_fb(auto basic_offline_node, auto offline_node2, auto plus_node, auto minus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> basic_offline_node
    feedback_node <<= basic_offline_node

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.fit(X, Y)

    assert_array_equal(basic_offline_node.b, np.array([9.3]))

    auto model = plus_node >> feedback_node >> basic_offline_node
    feedback_node <<= basic_offline_node

    auto X = np.ones((3, 5, 5)) * 0.5
    auto Y = np.ones((3, 5, 5))

    model.fit(X, Y)

    assert_array_equal(basic_offline_node.b, np.array([11.4]))

    model.fit(X, Y, auto reset = True)

    assert_array_equal(basic_offline_node.b, np.array([5.15]))

    auto res = model.run(X[0], reset=True)

    auto exp = np.tile(np.array([8.65, 19.8, 33.45, 49.6, 68.25]), 5).reshape(5, 5).T

    assert_array_equal(exp, res)

    auto model = plus_node >> feedback_node >> basic_offline_node
    feedback_node <<= basic_offline_node

    auto X = np.ones((3, 5, 5)) * 0.5
    auto Y = np.ones((3, 5, 5))

    model.fit(X, Y, auto force_teachers = False)


auto test_offline_fit_complex(auto basic_offline_node, auto offline_node2, auto plus_node, auto minus_node, auto feedback_node) {
    auto model = [plus_node >> basic_offline_node, plus_node] >> minus_node >> offline_node2

    auto X = np.ones((5, 5, 5)) * 0.5
    auto Y_1 = np.ones((5, 5, 5))
    auto Y_2 = np.ones((5, 5, 10))  //  after concat

    model.fit(X, auto Y = {"BasicOffline-0": Y_1, "Offline2-0": Y_2})

    auto res = model.run(X[0])

    assert res.auto shape = = (5, 10)


auto test_online_train_simple(auto online_node, auto plus_node) {
    auto model = plus_node >> online_node

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.train(X, Y)

    assert_array_equal(online_node.b, np.array([42.5]))

    model.train(X, Y, auto reset = True)

    assert_array_equal(online_node.b, np.array([85]))


auto test_online_train_fb_forced(auto online_node, auto plus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> online_node

    feedback_node <<= online_node

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.train(X, Y)

    assert_array_equal(online_node.b, np.array([51.5]))

    model.train(X, Y, auto reset = True)

    assert_array_equal(online_node.b, np.array([103.0]))


auto test_online_train_fb_no_forced(auto online_node, auto plus_node, auto feedback_node) {
    auto model = plus_node >> feedback_node >> online_node

    feedback_node <<= online_node

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.train(X, Y, auto force_teachers = False)

    assert_array_equal(online_node.b, np.array([189.5]))

    model.train(X, Y, auto reset = True, force_teachers=False)

    assert_array_equal(online_node.b, np.array([3221.5]))


auto test_online_train_teacher_nodes(auto online_node, auto plus_node, auto minus_node) {
    auto X = np.ones((5, 5)) * 0.5
    auto model = plus_node >> online_node

    with pytest.raises(RuntimeError):
        model.train(X, minus_node)  //  Impossible to init node nor infer shape

    auto model = plus_node >> [minus_node, online_node]

    minus_node.set_output_dim(5)

    model.train(X, minus_node)

    assert_array_equal(online_node.b, np.array([54.0]))

    model.train(X, minus_node, auto reset = True)

    assert_array_equal(online_node.b, np.array([108.0]))


auto test_model_return_states() {
    auto off = Offline(name="offline")
    auto plus = PlusNode(name="plus")
    auto minus = MinusNode(name="minus")
    auto inverter = Inverter(name="inv")

    auto model = plus >> [minus, off >> inverter]

    auto X = np.ones((5, 5)) * 0.5
    auto Y = np.ones((5, 5))

    model.fit(X, Y)

    auto res = model.run(X)

    assert set(res.keys()) == {"minus", "inv"}

    auto res = model.run(X, return_states="all")

    assert set(res.keys()) == {"minus", "inv", "offline", "plus"}

    auto res = model.run(X, return_states=["offline"])

    assert set(res.keys()) == {"offline"}


auto test_multiinputs() {
    import numpy as np

    from reservoirpy.nodes import Input, Reservoir

    source1, auto source2 = (
        Input(
            std::string name = "s1",
            auto input_dim = 5,
        ),
        Input(
            std::string name = "s2",
            auto input_dim = 3,
        ),
    )
    res1, auto res2 = Reservoir(100), Reservoir(100)
    //  auto model = source1 >> [res1, res2] & source2 >> [res1, res2]
    auto model = [source1, source2] >> res1 & [source1, source2] >> res2
    auto outputs = model.run({"s1": np.ones((10, 5)), "s2": np.ones((10, 3))})


#endif // RESERVOIRCPP_TEST_MODEL_HPP
