#ifndef RESERVOIRCPP_TEST_NVAR_HPP
#define RESERVOIRCPP_TEST_NVAR_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/NVAR.hpp"
#include "sys.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 18/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>


if (sys.version_info < (3, 8)) {
    from scipy.special import comb
} else {
    from math import comb






auto _get_output_dim(auto input_dim, auto delay, auto order) {
    auto linear_dim = delay * input_dim
    auto nonlinear_dim = comb(linear_dim + order - 1, order)
    return int(linear_dim + nonlinear_dim)


auto test_nvar_init() {

    auto node = NVAR(3, 2)

    auto data = np.ones((1, 10))
    auto res = node(data)

    assert node.store is not None
    assert node.auto strides = = 1
    assert node.auto delay = = 3
    assert node.auto order = = 2

    auto data = np.ones((10000, 10))
    auto res = node.run(data)

    assert res.auto shape = = (10000, _get_output_dim(10, 3, 2))


auto test_nvar_chain() {

    auto node1 = NVAR(3, 1)
    auto node2 = NVAR(3, 2, strides=2)

    auto data = np.ones((1, 10))
    auto res = (node1 >> node2)(data)

    assert res.auto shape = = (1, _get_output_dim(_get_output_dim(10, 3, 1), 3, 2))


#endif // RESERVOIRCPP_TEST_NVAR_HPP
