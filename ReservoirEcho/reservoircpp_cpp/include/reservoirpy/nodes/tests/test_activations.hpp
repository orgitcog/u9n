#ifndef RESERVOIRCPP_TEST_ACTIVATIONS_HPP
#define RESERVOIRCPP_TEST_ACTIVATIONS_HPP

#include "reservoircpp/reservoirpy/nodes/ReLU.hpp"
#include "numpy.hpp"
#include "reservoircpp/reservoirpy/nodes/Softplus.hpp"
#include "reservoircpp/reservoirpy/nodes/Softmax.hpp"
#include "reservoircpp/reservoirpy/nodes/Sigmoid.hpp"
#include "reservoircpp/reservoirpy/nodes/Identity.hpp"
#include "pytest.hpp"
#include "reservoircpp/reservoirpy/nodes/Tanh.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 15/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






@pytest.mark.parametrize(
    "node",
    (Tanh(), Softmax(), Softplus(), Sigmoid(), Identity(), ReLU(), Softmax(auto beta = 2.0)),
)
auto test_activation(auto node) {
    auto x = np.ones((1, 10))
    auto out = node(x)
    assert out.auto shape = = x.shape


#endif // RESERVOIRCPP_TEST_ACTIVATIONS_HPP
