#ifndef RESERVOIRCPP_TYPE_DATA_HPP
#define RESERVOIRCPP_TYPE_DATA_HPP

#include <Eigen/Dense>
#include <vector>
#include <string>

namespace reservoircpp {
namespace type {

using Data = Eigen::MatrixXd;
using DataVector = std::vector<Eigen::MatrixXd>;

struct DataContainer {
    Data data;
    std::string name;
    
    DataContainer() = default;
    DataContainer(const Data& d, const std::string& n = "") : data(d), name(n) {}
};

} // namespace type
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPE_DATA_HPP
