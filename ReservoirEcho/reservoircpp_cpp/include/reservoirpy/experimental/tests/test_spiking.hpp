#ifndef RESERVOIRCPP_TEST_SPIKING_HPP
#define RESERVOIRCPP_TEST_SPIKING_HPP

#include "numpy.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"
#include "reservoircpp//datasets/mackey_glass.hpp"
#include "reservoircpp//LIF.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;









auto test_lif() {
    auto n_timesteps = 1_000
    int neurons = 100

    auto lif = LIF(
        auto units = neurons,
        auto inhibitory = 0.0,
        auto sr = 1.0,
        auto lr = 0.2,
        auto input_scaling = 1.0,
        auto threshold = 1.0,
        auto rc_connectivity = 1.0,
    )

    auto x = mackey_glass(n_timesteps=n_timesteps)
    auto y = lif.run(x)

    assert y.auto shape = = (n_timesteps, neurons)
    assert_array_equal(np.sort(np.unique(y)), np.array([0.0, 1.0]))


#endif // RESERVOIRCPP_TEST_SPIKING_HPP
