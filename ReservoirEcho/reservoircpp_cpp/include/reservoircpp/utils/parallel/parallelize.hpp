#ifndef RESERVOIRCPP_UTILS_PARALLEL_PARALLELIZE_HPP
#define RESERVOIRCPP_UTILS_PARALLEL_PARALLELIZE_HPP

#include <vector>
#include <functional>

namespace reservoircpp {
namespace utils {
namespace parallel {

template<typename Func, typename T>
auto parallelize(Func&& func, const std::vector<T>& items) {
    std::vector<decltype(func(items[0]))> results;
    results.reserve(items.size());
    for (const auto& item : items) {
        results.push_back(func(item));
    }
    return results;
}

} // namespace parallel
} // namespace utils
} // namespace reservoircpp

#endif
