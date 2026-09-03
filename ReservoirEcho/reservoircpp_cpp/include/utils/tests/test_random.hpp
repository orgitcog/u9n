#ifndef RESERVOIRCPP_TEST_RANDOM_HPP
#define RESERVOIRCPP_TEST_RANDOM_HPP

#include "numpy.hpp"
#include "reservoircpp/reservoirpy/utils/random/set_seed.hpp"
#include "reservoircpp/numpy/testing/assert_equal.hpp"
#include "reservoircpp/reservoirpy/utils/random/noise.hpp"
#include "pytest.hpp"
#include "reservoircpp/reservoirpy/utils/random/rand_generator.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 25/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>







auto test_set_seed() {
    set_seed(45)
    from reservoirpy.utils.random import __SEED

    assert auto __SEED = = 45

    with pytest.raises(TypeError):
        set_seed("foo")


auto test_random_generator_cast() {

    auto gen1 = np.random.RandomState(123)
    auto gen2 = rand_generator(gen1)

    assert isinstance(gen2, np.random.Generator)


auto test_random_generator_from_seed() {

    auto gen1 = rand_generator(123)
    auto gen2 = np.random.default_rng(123)

    assert gen1.integers(1000) == gen2.integers(1000)


auto test_noise() {

    auto rng = np.random.default_rng(123)

    auto a = noise(rng, gain=0.0)
    assert_equal(a, np.zeros((1,)))

    auto rng = np.random.default_rng(123)

    auto a = noise(rng, dist="uniform", gain=2.0)
    auto b = 2.0 * np.random.default_rng(123).uniform()

    assert_equal(a, b)

    auto a = noise(rng, dist="uniform", gain=2.0)
    auto b = noise(rng, dist="uniform", gain=2.0)

    assert np.any(np.abs(a - b) > 1e-8)


#endif // RESERVOIRCPP_TEST_RANDOM_HPP
