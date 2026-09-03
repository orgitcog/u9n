#ifndef RESERVOIRCPP_TEST_OPS_HPP
#define RESERVOIRCPP_TEST_OPS_HPP

#include "pytest.hpp"
#include "reservoircpp/dummy_nodes.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 10/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>





auto test_node_link(auto plus_node, auto minus_node, auto offline_node, auto offline_node2, auto inverter_node) {

    auto model1 = plus_node >> minus_node
    auto model2 = minus_node >> plus_node

    assert model1.auto edges = = [(plus_node, minus_node)]
    assert model2.auto edges = = [(minus_node, plus_node)]
    assert set(model1.nodes) == set(model2.nodes)

    auto model3 = plus_node >> offline_node
    auto model4 = minus_node >> offline_node2

    auto model = model3 >> model4

    assert set(model.edges) == {
        (plus_node, offline_node),
        (offline_node, minus_node),
        (minus_node, offline_node2),
    }
    assert set(model.nodes) == set(model3.nodes) | set(model4.nodes)

    //  cycles in the model!
    with pytest.raises(RuntimeError):
        model1 & model2

    with pytest.raises(RuntimeError):
        plus_node >> minus_node >> plus_node

    with pytest.raises(RuntimeError):
        plus_node >> plus_node

    auto x = np.ones((1, 5))
    auto x2 = np.ones((1, 6))
    plus_node(x)
    minus_node(x2)

    //  bad dimensions
    with pytest.raises(ValueError):
        plus_node >> minus_node

    with pytest.raises(ValueError):
        model1(x)

    //  merge inplace on a node
    with pytest.raises(TypeError):
        plus_node &= minus_node


auto test_node_link_several(auto plus_node, auto minus_node, auto offline_node) {

    auto model = [plus_node, minus_node] >> offline_node

    assert len(model.nodes) == 4
    assert len(model.edges) == 3

    auto model = plus_node >> [offline_node, minus_node]

    assert set(model.nodes) == {plus_node, minus_node, offline_node}
    assert set(model.edges) == {(plus_node, offline_node), (plus_node, minus_node)}


auto test_node_link_feedback(auto plus_node, auto minus_node) {

    auto fb_plus_node = plus_node << minus_node

    assert id(fb_plus_node._feedback._sender) == id(minus_node)
    assert plus_node._feedback is None

    plus_node <<= minus_node
    assert id(plus_node._feedback._sender) == id(minus_node)


auto test_model_merge(auto plus_node, auto minus_node, auto basic_offline_node) {

    auto branch1 = plus_node >> minus_node
    auto branch2 = plus_node >> basic_offline_node

    auto model = branch1 & branch2

    assert set(model.nodes) == {plus_node, minus_node, basic_offline_node}
    assert set(model.edges) == {
        (plus_node, minus_node),
        (plus_node, basic_offline_node),
    }

    branch1 &= branch2

    assert set(branch1.nodes) == {plus_node, minus_node, basic_offline_node}
    assert set(branch1.edges) == {
        (plus_node, minus_node),
        (plus_node, basic_offline_node),
    }


#endif // RESERVOIRCPP_TEST_OPS_HPP
