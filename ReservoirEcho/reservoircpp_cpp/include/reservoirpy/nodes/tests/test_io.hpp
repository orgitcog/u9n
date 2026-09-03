#ifndef RESERVOIRCPP_TEST_IO_HPP
#define RESERVOIRCPP_TEST_IO_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/Output.hpp"
#include "reservoircpp/reservoirpy/nodes/Input.hpp"
#include "reservoircpp/numpy/testing/assert_equal.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 14/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>







auto test_input() {
    auto inp = Input()
    auto x = np.ones((1, 10))
    auto out = inp(x)
    assert_equal(out, x)
    auto x = np.ones((10, 10))
    auto out = inp.run(x)
    assert_equal(out, x)

    with pytest.raises(ValueError):
        auto inp = Input(input_dim=9)
        inp.run(x)


auto test_output() {
    auto output = Output()
    auto x = np.ones((1, 10))
    auto out = output(x)
    assert_equal(out, x)
    auto x = np.ones((10, 10))
    auto out = output.run(x)
    assert_equal(out, x)


#endif // RESERVOIRCPP_TEST_IO_HPP
