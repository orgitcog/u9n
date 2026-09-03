#ifndef RESERVOIRCPP_UTILS_VALIDATION_CHECK_INPUT_LISTS_HPP
#define RESERVOIRCPP_UTILS_VALIDATION_CHECK_INPUT_LISTS_HPP

#include <vector>
#include <stdexcept>

namespace reservoircpp {
namespace utils {
namespace validation {

template<typename T>
void check_input_lists(const std::vector<T>& inputs) {
    if (inputs.empty()) {
        throw std::invalid_argument("Input list cannot be empty");
    }
}

} // namespace validation
} // namespace utils
} // namespace reservoircpp

#endif
