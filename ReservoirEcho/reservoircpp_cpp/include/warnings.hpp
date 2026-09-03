#ifndef RESERVOIRCPP_WARNINGS_HPP
#define RESERVOIRCPP_WARNINGS_HPP

#include <iostream>
#include <string>

namespace reservoircpp {

inline void warn(const std::string& message) {
    std::cerr << "Warning: " << message << std::endl;
}

} // namespace reservoircpp

#endif
