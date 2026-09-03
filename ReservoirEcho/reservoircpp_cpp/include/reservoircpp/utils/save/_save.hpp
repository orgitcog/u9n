#ifndef RESERVOIRCPP_UTILS_SAVE_SAVE_HPP
#define RESERVOIRCPP_UTILS_SAVE_SAVE_HPP

#include <string>
#include <fstream>

namespace reservoircpp {
namespace utils {
namespace save {

template<typename T>
void save(const T& obj, const std::string& path) {
    // Stub implementation
}

template<typename T>
T load(const std::string& path) {
    return T();
}

} // namespace save
} // namespace utils
} // namespace reservoircpp

#endif
