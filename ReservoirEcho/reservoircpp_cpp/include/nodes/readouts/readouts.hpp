#ifndef RESERVOIRCPP_READOUTS_HPP
#define RESERVOIRCPP_READOUTS_HPP

#include "reservoircpp/sklearn_node/ScikitLearnNode.hpp"
#include "reservoircpp/force/FORCE.hpp"
#include "reservoircpp/lms/LMS.hpp"
#include "reservoircpp/rls/RLS.hpp"
#include "reservoircpp/ridge/Ridge.hpp"

using namespace reservoircpp;
using namespace Eigen;







auto __all__ = ["FORCE", "RLS", "LMS", "Ridge", "ScikitLearnNode"]


#endif // RESERVOIRCPP_READOUTS_HPP
