#ifndef RESERVOIRCPP_TYPE_WEIGHTS_HPP
#define RESERVOIRCPP_TYPE_WEIGHTS_HPP

#include <Eigen/Dense>
#include <optional>

namespace reservoircpp {
namespace type {

using Weights = std::optional<Eigen::MatrixXd>;

} // namespace type
} // namespace reservoircpp

#endif
