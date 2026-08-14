#ifndef RESERVOIRCPP_RESERVOIRS_HPP
#define RESERVOIRCPP_RESERVOIRS_HPP

#include "reservoircpp/reservoir/Reservoir.hpp"
#include "reservoircpp/intrinsic_plasticity/IPReservoir.hpp"
#include "reservoircpp/nvar/NVAR.hpp"
#include "reservoircpp/local_plasticity_reservoir/LocalPlasticityReservoir.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 08/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>





auto __all__ = ["Reservoir", "IPReservoir", "NVAR", "LocalPlasticityReservoir"]


#endif // RESERVOIRCPP_RESERVOIRS_HPP
