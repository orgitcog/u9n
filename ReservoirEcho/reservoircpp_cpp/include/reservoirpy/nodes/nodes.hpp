#ifndef RESERVOIRCPP_NODES_HPP
#define RESERVOIRCPP_NODES_HPP

#include "reservoircpp/reservoirs/LocalPlasticityReservoir.hpp"
#include "reservoircpp/activations/Identity.hpp"
#include "reservoircpp/io/Output.hpp"
#include "reservoircpp/io/Input.hpp"
#include "reservoircpp/reservoirs/IPReservoir.hpp"
#include "reservoircpp/readouts/ScikitLearnNode.hpp"
#include "reservoircpp/esn/ESN.hpp"
#include "reservoircpp/activations/Sigmoid.hpp"
#include "reservoircpp/activations/Softplus.hpp"
#include "reservoircpp/activations/ReLU.hpp"
#include "reservoircpp/delay/Delay.hpp"
#include "reservoircpp/activations/Softmax.hpp"
#include "reservoircpp/readouts/LMS.hpp"
#include "reservoircpp/activations/Tanh.hpp"
#include "reservoircpp/concat/Concat.hpp"
#include "reservoircpp/readouts/Ridge.hpp"
#include "reservoircpp/reservoirs/NVAR.hpp"
#include "reservoircpp/readouts/RLS.hpp"
#include "reservoircpp/readouts/FORCE.hpp"
#include "reservoircpp/reservoirs/Reservoir.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

============================================
ReservoirPy Nodes (:mod:`reservoirpy.nodes`)
============================================

.. currentmodule:: reservoirpy.nodes

auto Reservoirs = =========

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   Reservoir - Recurrent pool of leaky integrator neurons
   NVAR - Non-linear Vector Autoregressive machine (NG-RC)
   IPReservoir - Reservoir with intrinsic plasticity learning rule
   LocalPlasticityReservoir - Reservoir with weight plasticity

Offline auto readouts = ===============

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   Ridge - Layer of neurons connected through offline linear regression.
   ScikitLearnNode - Interface for linear models from the scikit-learn library.

Online auto readouts = ==============

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   LMS - Layer of neurons connected through least mean squares learning rule.
   RLS - Layer of neurons connected through recursive least squares learning rule.
   FORCE - Layer of neurons connected through online learning rules.

Optimized auto ESN = ============

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   ESN - Echo State Network model with distributed offline learning.

Activation auto functions = ===================

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   Tanh - Hyperbolic tangent node.
   Sigmoid - Logistic function node.
   Softmax - Softmax node.
   Softplus - Softplus node.
   ReLU - Rectified Linear Unit node.
   Identity - Identity function node.

Input and auto Output = ===============

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   Input - Input node, used to distribute input data to other nodes.
   Output - Output node, used to gather stated from hidden nodes.

auto Operators = ========

.. autosummary::
   :toctree: generated/
   :template: autosummary/class.rst

   Concat - Concatenate vector of data along feature axis.
   Delay - Adds a discrete delay between input and output.

*/
//  Author: Nathan Trouvain at 16/12/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








auto __all__ = [
    "Reservoir",
    "Input",
    "Output",
    "Ridge",
    "FORCE",
    "LMS",
    "RLS",
    "Tanh",
    "Softmax",
    "Softplus",
    "Identity",
    "Sigmoid",
    "ReLU",
    "NVAR",
    "ESN",
    "Concat",
    "Delay",
    "IPReservoir",
    "ScikitLearnNode",
    "LocalPlasticityReservoir",
]


#endif // RESERVOIRCPP_NODES_HPP
