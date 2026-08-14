#ifndef RESERVOIRCPP_UTILS_VALIDATION_CHECK_RESERVOIR_MATRICES_HPP
#define RESERVOIRCPP_UTILS_VALIDATION_CHECK_RESERVOIR_MATRICES_HPP

#include <Eigen/Dense>
#include <tuple>
#include <stdexcept>

namespace reservoircpp {
namespace utils {
namespace validation {

template<typename Caller>
auto check_reservoir_matrices(
    const Eigen::MatrixXd& W,
    const Eigen::MatrixXd& Win,
    const Eigen::MatrixXd& Wout = Eigen::MatrixXd(),
    const Eigen::MatrixXd& Wfb = Eigen::MatrixXd(),
    Caller* caller = nullptr
) {
    return std::make_tuple(W, Win, Wout, Wfb);
}

} // namespace validation
} // namespace utils
} // namespace reservoircpp

#endif
