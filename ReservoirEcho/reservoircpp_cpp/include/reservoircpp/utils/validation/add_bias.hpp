#ifndef RESERVOIRCPP_UTILS_VALIDATION_ADD_BIAS_HPP
#define RESERVOIRCPP_UTILS_VALIDATION_ADD_BIAS_HPP

#include <Eigen/Dense>

namespace reservoircpp {
namespace utils {
namespace validation {

inline Eigen::MatrixXd add_bias(const Eigen::MatrixXd& X) {
    Eigen::MatrixXd result(X.rows(), X.cols() + 1);
    result.leftCols(X.cols()) = X;
    result.rightCols(1).setOnes();
    return result;
}

} // namespace validation
} // namespace utils
} // namespace reservoircpp

#endif
