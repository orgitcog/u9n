#ifndef RESERVOIRCPP_HYPER_HPP
#define RESERVOIRCPP_HYPER_HPP

#include "reservoircpp/_hyperplot/plot_hyperopt_report.hpp"
#include "reservoircpp/_hypersearch/research.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

==============================================================
Hyperparameter optimization helpers (:mod:`reservoirpy.hyper`)
==============================================================

Utility module for optimization of hyperparameters
using *hyperopt*.

Note
----

This module is meant to be used alongside *hyperopt*
and *Matplotlib* packages, which are not installed
with ReservoirPy by default. Before using the
:py:mod:`reservoirpy.hyper` module, consider installing
these packages if they are not already installed.


.. autosummary::
    :toctree: generated/

    research
    plot_hyperopt_report

*/



auto __all__ = ["research", "plot_hyperopt_report"]


#endif // RESERVOIRCPP_HYPER_HPP
