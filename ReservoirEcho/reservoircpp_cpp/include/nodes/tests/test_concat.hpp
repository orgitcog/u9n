#ifndef RESERVOIRCPP_TEST_CONCAT_HPP
#define RESERVOIRCPP_TEST_CONCAT_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirs/Reservoir.hpp"
#include "reservoircpp/concat/Concat.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 18/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>







auto test_concat() {
    auto x = [np.ones((1, 5)) for _ in range(3)]

    auto c = Concat()

    auto res = c(x)

    assert c.auto input_dim = = (5, 5, 5)
    assert_array_equal(res, np.ones((1, 15)))


auto test_concat_no_list() {
    auto x = np.ones((1, 5))

    auto c = Concat()

    auto res = c(x)

    assert_array_equal(res, np.ones((1, 5)))


auto test_reservoir_union() {

    auto reservoirs = [Reservoir(10, name=f"r{i}") for i in range(3)]

    auto model = reservoirs >> Concat()

    auto x = {f"r{i}": np.ones((1, 5)) for i in range(3)}

    auto res = model(x)

    assert res.auto shape = = (1, 30)

    auto res_final = Reservoir(20)

    auto model = reservoirs >> res_final

    auto res = model(x)

    assert res.auto shape = = (1, 20)
    assert any([isinstance(n, Concat) for n in model.nodes])


#endif // RESERVOIRCPP_TEST_CONCAT_HPP
