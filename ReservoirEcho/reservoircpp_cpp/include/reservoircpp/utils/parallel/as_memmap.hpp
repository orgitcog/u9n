#ifndef RESERVOIRCPP_UTILS_PARALLEL_AS_MEMMAP_HPP
#define RESERVOIRCPP_UTILS_PARALLEL_AS_MEMMAP_HPP

#include <Eigen/Dense>

namespace reservoircpp {
namespace utils {
namespace parallel {

template<typename T>
T& as_memmap(T& data) {
    return data;
}

} // namespace parallel
} // namespace utils
} // namespace reservoircpp

#endif
