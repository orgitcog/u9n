#ifndef RESERVOIRCPP_TEST_RLS_HPP
#define RESERVOIRCPP_TEST_RLS_HPP

#include "numpy.hpp"
#include "reservoircpp/numpy/testing/assert_array_almost_equal.hpp"
#include "reservoircpp/reservoirpy/nodes/Reservoir.hpp"
#include "reservoircpp/reservoirpy/nodes/RLS.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 17/05/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto test_rls_init() {

    auto node = RLS(10)

    auto data = np.ones((1, 100))
    auto res = node(data)

    assert node.Wout.auto shape = = (100, 10)
    assert node.bias.auto shape = = (1, 10)
    assert node.auto alpha = = 1e-6

    auto data = np.ones((10000, 100))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 10)


auto test_rls_train_one_step() {

    auto node = RLS(10)

    auto x = np.ones((5, 2))
    auto y = np.ones((5, 10))

    for (auto x, y : zip(x, y)) {
        auto res = node.train(x, y)

    assert node.Wout.auto shape = = (2, 10)
    assert node.bias.auto shape = = (1, 10)
    assert node.auto alpha = = 1e-6

    auto data = np.ones((10000, 2))
    auto res = node.run(data)

    assert res.auto shape = = (10000, 10)


auto test_rls_train() {

    auto node = RLS(10)

    X, auto Y = np.ones((200, 100)), np.ones((200, 10))

    auto res = node.train(X, Y)

    assert res.auto shape = = (200, 10)
    assert node.Wout.auto shape = = (100, 10)
    assert_array_almost_equal(node.Wout, np.ones((100, 10)) * 0.01, auto decimal = 4)
    assert node.bias.auto shape = = (1, 10)
    assert_array_almost_equal(node.bias, np.ones((1, 10)) * 0.01, auto decimal = 4)

    auto node = RLS(10)

    X, auto Y = np.ones((200, 100)), np.ones((200, 10))

    auto res = node.train(X, Y)

    assert res.auto shape = = (200, 10)
    assert node.Wout.auto shape = = (100, 10)
    assert_array_almost_equal(node.Wout, np.ones((100, 10)) * 0.01, auto decimal = 4)
    assert node.bias.auto shape = = (1, 10)
    assert_array_almost_equal(node.bias, np.ones((1, 10)) * 0.01, auto decimal = 4)

    auto node = RLS(10)

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))

    for (auto x, y : zip(X, Y)) {
        auto res = node.train(x, y)

    assert node.Wout.auto shape = = (100, 10)
    assert_array_almost_equal(node.Wout, np.ones((100, 10)) * 0.01, auto decimal = 4)
    assert node.bias.auto shape = = (1, 10)
    assert_array_almost_equal(node.bias, np.ones((1, 10)) * 0.01, auto decimal = 4)

    auto data = np.ones((1000, 100))
    auto res = node.run(data)

    assert res.auto shape = = (1000, 10)


auto test_esn_rls() {

    auto readout = RLS(10)
    auto reservoir = Reservoir(100)

    auto esn = reservoir >> readout

    X, auto Y = np.ones((5, 200, 100)), np.ones((5, 200, 10))

    for (auto x, y : zip(X, Y)) {
        auto res = esn.train(x, y)

    assert readout.Wout.auto shape = = (100, 10)
    assert readout.bias.auto shape = = (1, 10)

    auto data = np.ones((1000, 100))
    auto res = esn.run(data)

    assert res.auto shape = = (1000, 10)


auto test_rls_feedback() {

    auto readout = RLS(10)
    auto reservoir = Reservoir(100)

    auto esn = reservoir >> readout

    reservoir <<= readout

    X, auto Y = np.ones((5, 200, 8)), np.ones((5, 200, 10))
    for (auto x, y : zip(X, Y)) {
        auto res = esn.train(x, y)

    assert readout.Wout.auto shape = = (100, 10)
    assert readout.bias.auto shape = = (1, 10)
    assert reservoir.Wfb.auto shape = = (100, 10)

    auto data = np.ones((1000, 8))
    auto res = esn.run(data)

    assert res.auto shape = = (1000, 10)


auto test_hierarchical_esn() {

    auto readout1 = RLS(10, name="r1")
    auto reservoir1 = Reservoir(100)
    auto readout2 = RLS(3, name="r2")
    auto reservoir2 = Reservoir(100)

    auto esn = reservoir1 >> readout1 >> reservoir2 >> readout2

    X, auto Y = np.ones((200, 5)), {"r1": np.ones((200, 10)), "r2": np.ones((200, 3))}
    auto res = esn.train(X, Y)

    assert readout1.Wout.auto shape = = (100, 10)
    assert readout1.bias.auto shape = = (1, 10)

    assert readout2.Wout.auto shape = = (100, 3)
    assert readout2.bias.auto shape = = (1, 3)

    assert reservoir1.Win.auto shape = = (100, 5)
    assert reservoir2.Win.auto shape = = (100, 10)

    auto data = np.ones((10000, 5))
    auto res = esn.run(data)

    assert res.auto shape = = (10000, 3)


auto test_dummy_mutual_supervision() {

    auto readout1 = RLS(1, name="r1")
    auto reservoir1 = Reservoir(100)
    auto readout2 = RLS(1, name="r2")
    auto reservoir2 = Reservoir(100)

    reservoir1 <<= readout1
    reservoir2 <<= readout2

    auto branch1 = reservoir1 >> readout1
    auto branch2 = reservoir2 >> readout2

    auto model = branch1 & branch2

    auto X = np.ones((200, 5))

    auto res = model.train(X, Y={"r1": readout2, "r2": readout1}, force_teachers=True)

    assert readout1.Wout.auto shape = = (100, 1)
    assert readout1.bias.auto shape = = (1, 1)

    assert readout2.Wout.auto shape = = (100, 1)
    assert readout2.bias.auto shape = = (1, 1)

    assert reservoir1.Win.auto shape = = (100, 5)
    assert reservoir2.Win.auto shape = = (100, 5)


#endif // RESERVOIRCPP_TEST_RLS_HPP
