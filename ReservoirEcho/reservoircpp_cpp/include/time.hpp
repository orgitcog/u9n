#ifndef RESERVOIRCPP_TIME_HPP
#define RESERVOIRCPP_TIME_HPP

#include <chrono>

namespace reservoircpp {
namespace time {

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

inline time_point now() {
    return clock::now();
}

} // namespace time
} // namespace reservoircpp

#endif
