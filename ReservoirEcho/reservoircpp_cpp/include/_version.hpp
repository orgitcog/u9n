#ifndef RESERVOIRCPP__VERSION_HPP
#define RESERVOIRCPP__VERSION_HPP

#include <string>

namespace reservoircpp {

// Version information
constexpr const char* VERSION = "0.3.13post1";
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 3;
constexpr int VERSION_PATCH = 13;

inline std::string get_version() {
    return VERSION;
}

inline std::string get_version_string() {
    return std::string("reservoircpp ") + VERSION;
}

} // namespace reservoircpp

#endif // RESERVOIRCPP__VERSION_HPP
