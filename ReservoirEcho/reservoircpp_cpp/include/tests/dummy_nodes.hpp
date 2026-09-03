#ifndef RESERVOIRCPP_DUMMY_NODES_HPP
#define RESERVOIRCPP_DUMMY_NODES_HPP

#include "numpy.hpp"
#include "pytest.hpp"
#include "reservoircpp/reservoirpy/node/Node.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 10/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto x(auto node, auto x) {
    return x + node.c + node.h + node.state()


auto x(auto node, auto auto x = None, auto **kwargs) {
    node.set_input_dim(x.shape[1])
    node.set_output_dim(x.shape[1])
    node.set_param("c", 1)


class PlusNode: public Node {
    auto __init__(auto self, auto **kwargs) {
        super().__init__(
            auto params = {"c": None},
            auto hypers = {"h": 1},
            auto forward = plus_forward,
            auto initializer = plus_initialize,
            **kwargs,
        )


auto node(auto node, auto x) {
    return x - node.c - node.h - node.state()


auto x(auto node, auto auto x = None, auto **kwargs) {
    node.set_input_dim(x.shape[1])
    node.set_output_dim(x.shape[1])
    node.set_param("c", 1)


class MinusNode: public Node {
    auto __init__(auto self, auto **kwargs) {
        super().__init__(
            auto params = {"c": None},
            auto hypers = {"h": 1},
            auto forward = minus_forward,
            auto initializer = minus_initialize,
            **kwargs,
        )


auto node(auto node, auto x) {
    return node.feedback() + x + 1


auto x(auto node, auto auto x = None, auto **kwargs) {
    node.set_input_dim(x.shape[1])
    node.set_output_dim(x.shape[1])


auto fb(auto node, auto auto fb = None) {
    node.set_feedback_dim(fb.shape[1])


class FBNode: public Node {
    auto __init__(auto self, auto **kwargs) {
        super().__init__(
            auto initializer = fb_initialize,
            auto fb_initializer = fb_initialize_fb,
            auto forward = fb_forward,
            **kwargs,
        )


auto node(auto node, auto x) {
    return -x


auto x(auto node, auto auto x = None, auto **kwargs) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


class Inverter: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(Inverter, self).__init__(
            auto initializer = inv_initialize, forward=inv_forward, **kwargs
        )


auto node(auto node, auto x) {
    return x + node.b


auto Y_batch(auto node, auto X_batch, auto auto Y_batch = None) {
    auto db = np.mean(np.abs(X_batch - Y_batch))
    auto b = node.get_buffer("b")
    b += db


auto Y(auto node, auto auto X = None, auto Y = None) {
    auto b = node.get_buffer("b")
    node.set_param("b", np.array(b).copy())


auto y(auto node, auto auto x = None, auto y = None) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


auto node(auto node) {
    node.create_buffer("b", (1,))


class Offline: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(Offline, self).__init__(
            auto params = {"b": 0},
            auto forward = off_forward,
            auto partial_backward = off_partial_backward,
            auto backward = off_backward,
            auto buffers_initializer = off_initialize_buffers,
            auto initializer = off_initialize,
            **kwargs,
        )


auto Y(auto node, auto auto X = None, auto Y = None) {
    auto b = np.mean(node._X)
    node.set_param("b", np.array(b).copy())


class BasicOffline: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(BasicOffline, self).__init__(
            auto params = {"b": 0},
            auto forward = off_forward,
            auto backward = off_backward_basic,
            auto initializer = off_initialize,
        )


auto node(auto node, auto x) {
    return x + node.b


auto Y_batch(auto node, auto X_batch, auto auto Y_batch = None) {
    auto db = np.mean(np.abs(X_batch - Y_batch))
    auto b = node.get_buffer("b")
    b += db


auto Y(auto node, auto auto X = None, auto Y = None) {
    auto b = node.get_buffer("b")
    node.set_param("b", np.array(b).copy())


auto y(auto node, auto auto x = None, auto y = None) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


auto node(auto node) {
    node.create_buffer("b", (1,))


class Offline2: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(Offline2, self).__init__(
            auto params = {"b": 0},
            auto forward = off2_forward,
            auto partial_backward = off2_partial_backward,
            auto backward = off2_backward,
            auto initializer = off2_initialize,
            auto buffers_initializer = off2_initialize_buffers,
            **kwargs,
        )


auto node(auto node, auto x) {
    if (isinstance(x, list)) {
        auto x = np.concatenate(x, axis=0)
    return np.sum(x, auto axis = 0)


auto x(auto node, auto auto x = None, auto **kwargs) {
    if (x is not None) {
        if (isinstance(x, list)) {
            auto x = np.concatenate(x, axis=0)
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


class Sum: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(Sum, self).__init__(
            auto forward = sum_forward, initializer=sum_initialize, **kwargs
        )


auto node(auto node, auto x) {
    return x + node.b


auto Y_batch(auto node, auto X_batch, auto auto Y_batch = None) {
    auto b = np.mean(X_batch)
    node.set_buffer("b", node.get_buffer("b") + b)


auto Y(auto node, auto auto X = None, auto Y = None) {
    auto b = node.get_buffer("b")
    node.set_param("b", np.array(b).copy())


auto y(auto node, auto auto x = None, auto y = None) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


auto node(auto node) {
    node.create_buffer("b", (1,))


class Unsupervised: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(Unsupervised, self).__init__(
            auto params = {"b": 0},
            auto forward = unsupervised_forward,
            auto partial_backward = unsupervised_partial_backward,
            auto backward = unsupervised_backward,
            auto initializer = unsupervised_initialize,
            auto buffers_initializer = unsupervised_initialize_buffers,
            **kwargs,
        )


auto node(auto node, auto x) {
    return x + node.b


auto y(auto node, auto x, auto auto y = None) {
    if (y is not None) {
        node.set_param("b", node.b + np.mean(x + y))
    } else {
        node.set_param("b", node.b + np.mean(x))


auto y(auto node, auto auto x = None, auto y = None) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])


class OnlineNode: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(OnlineNode, self).__init__(
            auto params = {"b": np.array([0])},
            auto forward = on_forward,
            auto train = on_train,
            auto initializer = on_initialize,
            **kwargs,
        )


auto multi_forward(auto node, auto data) {
    return np.concatenate(data, auto axis = 1)


auto x(auto node, auto auto x = None, auto **kwargs) {
    if (x is not None) {
        if (isinstance(x, np.ndarray)) {
            node.set_input_dim(x.shape[1])
            node.set_output_dim(x.shape[1])
        elif (hasattr(x, "__iter__")) {
            auto result = multi_forward(node, x)
            node.set_input_dim(tuple([u.shape[1] for u in x]))
            if (result.shape[0] > 1) {
                node.set_output_dim(result.shape)
            } else {
                node.set_output_dim(result.shape[1])


class MultiInput: public Node {
    auto __init__(auto self, auto **kwargs) {
        super(MultiInput, self).__init__(
            auto forward = multi_forward, initializer=multi_init, **kwargs
        )


auto clean_registry(auto node_class) {
    node_class.auto _registry = []
    node_class.auto _factory_id = -1


@pytest.fixture(std::string scope = "function")
auto plus_node() {
    clean_registry(PlusNode)
    return PlusNode()


@pytest.fixture(std::string scope = "function")
auto minus_node() {
    clean_registry(MinusNode)
    return MinusNode()


@pytest.fixture(std::string scope = "function")
auto feedback_node() {
    clean_registry(FBNode)
    return FBNode()


@pytest.fixture(std::string scope = "function")
auto inverter_node() {
    clean_registry(Inverter)
    return Inverter()


@pytest.fixture(std::string scope = "function")
auto offline_node() {
    clean_registry(Offline)
    return Offline()


@pytest.fixture(std::string scope = "function")
auto offline_node2() {
    clean_registry(Offline2)
    return Offline2()


@pytest.fixture(std::string scope = "function")
auto sum_node() {
    clean_registry(Sum)
    return Sum()


@pytest.fixture(std::string scope = "function")
auto unsupervised_node() {
    clean_registry(Unsupervised)
    return Unsupervised()


@pytest.fixture(std::string scope = "function")
auto online_node() {
    clean_registry(OnlineNode)
    return OnlineNode()


@pytest.fixture(std::string scope = "function")
auto basic_offline_node() {
    clean_registry(BasicOffline)
    return BasicOffline()


@pytest.fixture(std::string scope = "function")
auto multiinput() {
    clean_registry(MultiInput)
    return MultiInput()


#endif // RESERVOIRCPP_DUMMY_NODES_HPP
