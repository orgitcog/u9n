#ifndef RESERVOIRCPP_MATH_SIN_HPP
#define RESERVOIRCPP_MATH_SIN_HPP

#include <cmath>
#include <Eigen/Dense>

namespace reservoircpp {
namespace math {

inline Eigen::ArrayXd sin(const Eigen::ArrayXd& x) {
    return x.sin();
}

inline double sin(double x) {
    return std::sin(x);
}

} // namespace math
} // namespace reservoircpp

#endif // RESERVOIRCPP_MATH_SIN_HPP
