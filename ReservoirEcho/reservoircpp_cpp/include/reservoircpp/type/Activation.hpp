#ifndef RESERVOIRCPP_TYPE_ACTIVATION_HPP
#define RESERVOIRCPP_TYPE_ACTIVATION_HPP

#include <functional>
#include <Eigen/Dense>

namespace reservoircpp {
namespace type {

using Activation = std::function<Eigen::ArrayXd(const Eigen::ArrayXd&)>;

} // namespace type
} // namespace reservoircpp

#endif
