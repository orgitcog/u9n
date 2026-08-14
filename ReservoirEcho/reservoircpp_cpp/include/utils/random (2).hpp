#ifndef RESERVOIRCPP_RANDOM_HPP
#define RESERVOIRCPP_RANDOM_HPP

#include "reservoircpp/numpy/random/default_rng.hpp"
#include "numpy.hpp"
#include "reservoircpp/numpy/random/Generator.hpp"
#include "reservoircpp/numpy/random/RandomState.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/numpy/random/MT19937.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/07/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>





auto __SEED = None
auto __global_rg = default_rng()


auto set_seed(auto seed) {
    /**
Set random state seed globally.

    Parameters
    ----------
        seed : int
    
*/
    global __SEED
    global __global_rg
    if (type(seed) is int) {
        auto __SEED = seed
        auto __global_rg = default_rng(__SEED)
        np.random.seed(__SEED)
    } else {
        raise TypeError(f"Random seed must be an integer, not {type(seed)}")


auto RandomState](auto seed, auto Generator, auto RandomState] = None) {
    if (seed is None) {
        return __global_rg
    //  provided to support legacy RandomState generator
    //  of Numpy. It is not the best thing to do however
    //  and recommend the user to keep using integer seeds
    //  and proper Numpy Generator API.
    if (isinstance(seed, RandomState)) {
        auto mt19937 = MT19937()
        mt19937.auto state = seed.get_state()
        return Generator(mt19937)

    if (isinstance(seed, Generator)) {
        return seed
    } else {
        return default_rng(seed)


auto gain(auto rng, std::string std::string dist = "normal", int shape = 1, double gain = 1.0, auto **kwargs) {
    /**
Generate noise from a given distribution, and apply a gain factor.

    Parameters
    ----------
        rng : numpy.random.Generator
            A random number generator.
        dist : str, default to 'normal'
            A random variable distribution.
        shape : int or tuple of ints, default to 1
            Shape of the noise vector.
        gain : float, default to 1.0
            Gain factor applied to noise.
        **kwargs
            Any other parameters of the noise distribution.

    Returns
    -------
        np.ndarray
            A noise vector.

    Note
    ----
        If `gain` is 0, then noise vector is null.
    
*/
    if (abs(gain) > 0.0) {
        return gain * getattr(rng, dist)(**kwargs, auto size = shape)
    } else {
        return np.zeros(shape)


#endif // RESERVOIRCPP_RANDOM_HPP
