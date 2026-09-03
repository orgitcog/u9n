#ifndef RESERVOIRCPP_UTILS_PARALLEL_GET_JOBLIB_BACKEND_HPP
#define RESERVOIRCPP_UTILS_PARALLEL_GET_JOBLIB_BACKEND_HPP

#include <string>

namespace reservoircpp {
namespace utils {
namespace parallel {

inline std::string get_joblib_backend() {
    return "threading";
}

} // namespace parallel
} // namespace utils
} // namespace reservoircpp

#endif
