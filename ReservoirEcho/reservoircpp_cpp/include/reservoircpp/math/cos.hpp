#ifndef RESERVOIRCPP_MATH_COS_HPP
#define RESERVOIRCPP_MATH_COS_HPP

#include <cmath>
#include <Eigen/Dense>

namespace reservoircpp {
namespace math {

inline Eigen::ArrayXd cos(const Eigen::ArrayXd& x) {
    return x.cos();
}

inline double cos(double x) {
    return std::cos(x);
}

} // namespace math
} // namespace reservoircpp

#endif
