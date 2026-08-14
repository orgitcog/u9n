#ifndef RESERVOIRCPP_MAIN_HPP
#define RESERVOIRCPP_MAIN_HPP

#include "reservoircpp/observables.hpp"
#include "reservoircpp/ops/link.hpp"
#include "tempfile.hpp"
#include "reservoircpp/utils/verbosity.hpp"
#include "reservoircpp/compat/load_compat.hpp"
#include "os.hpp"
#include "reservoircpp/nodes.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/hyper.hpp"
#include "reservoircpp/compat/utils/save/load.hpp"
#include "reservoircpp/mat_gen.hpp"
#include "reservoircpp/ops/link_feedback.hpp"
#include "reservoircpp/compat.hpp"
#include "reservoircpp/_version/__version__.hpp"
#include "reservoircpp/ops/merge.hpp"
#include "logging.hpp"
#include "reservoircpp/type.hpp"
#include "reservoircpp/model/Model.hpp"
#include "reservoircpp/utils/random/set_seed.hpp"
#include "reservoircpp/activationsfunc.hpp"

using namespace reservoircpp;
using namespace Eigen;















auto logger = logging.getLogger(__name__)

auto _TEMPDIR = os.path.join(tempfile.gettempdir(), "reservoirpy-temp")
if (not os.path.exists(_TEMPDIR)) {  //  pragma: no cover
    try:
        os.mkdir(_TEMPDIR)
    except OSError:
        auto _TEMPDIR = tempfile.gettempdir()

auto __all__ = [
    "__version__",
    "mat_gen",
    "activationsfunc",
    "observables",
    "hyper",
    "nodes",
    "load",
    "Node",
    "Model",
    "link",
    "link_feedback",
    "merge",
    "set_seed",
    "verbosity",
    "load_compat",
]


#endif // RESERVOIRCPP_MAIN_HPP
