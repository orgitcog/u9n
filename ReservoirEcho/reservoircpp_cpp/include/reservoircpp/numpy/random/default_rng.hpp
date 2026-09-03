#ifndef RESERVOIRCPP_NUMPY_RANDOM_DEFAULT_RNG_HPP
#define RESERVOIRCPP_NUMPY_RANDOM_DEFAULT_RNG_HPP

#include <random>

namespace reservoircpp {
namespace numpy {
namespace random {

class Generator {
public:
    Generator(unsigned int seed = std::random_device{}()) : gen_(seed) {}
    
    double uniform(double low = 0.0, double high = 1.0) {
        std::uniform_real_distribution<double> dist(low, high);
        return dist(gen_);
    }
    
    template<typename T>
    T uniform(T low, T high, std::size_t size) {
        T result(size);
        std::uniform_real_distribution<double> dist(low, high);
        for (std::size_t i = 0; i < size; ++i) {
            result[i] = dist(gen_);
        }
        return result;
    }
    
private:
    std::mt19937 gen_;
};

inline Generator default_rng(unsigned int seed = 0) {
    return Generator(seed == 0 ? std::random_device{}() : seed);
}

} // namespace random
} // namespace numpy
} // namespace reservoircpp

#endif
