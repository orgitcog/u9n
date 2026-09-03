#ifndef RESERVOIRCPP_NUMPY_RANDOM_SEEDSEQUENCE_HPP
#define RESERVOIRCPP_NUMPY_RANDOM_SEEDSEQUENCE_HPP

#include <vector>
#include <cstdint>

namespace reservoircpp {
namespace numpy {
namespace random {

class SeedSequence {
public:
    SeedSequence(uint64_t seed = 0) : seed_(seed) {}
    
    std::vector<uint32_t> generate_state(std::size_t n) {
        return std::vector<uint32_t>(n, static_cast<uint32_t>(seed_));
    }
    
private:
    uint64_t seed_;
};

} // namespace random
} // namespace numpy
} // namespace reservoircpp

#endif
