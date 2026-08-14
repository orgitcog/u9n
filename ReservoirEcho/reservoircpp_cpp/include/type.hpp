#ifndef RESERVOIRCPP_TYPE_HPP
#define RESERVOIRCPP_TYPE_HPP

#include "numpy.hpp"
#include "reservoircpp/scipy/sparse/csr_matrix.hpp"
#include "reservoircpp/scipy/sparse/csc_matrix.hpp"
#include "sys.hpp"
#include "reservoircpp/scipy/sparse/coo_matrix.hpp"
#include "reservoircpp/typing/(.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 22/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>


    Any,
    Callable,
    Dict,
    Iterable,
    Iterator,
    Optional,
    Sequence,
    Tuple,
    TypeVar,
    Union,
)

if (sys.version_info < (3, 8)) {
    from typing_extensions import Protocol
} else {
    from typing import Protocol




auto global_dtype = np.float64
std::string global_ctype = "d"

auto Weights = TypeVar("Weights", np.ndarray, csr_matrix, csc_matrix, coo_matrix)
auto Shape = TypeVar("Shape", int, Tuple[int, ...])
auto Data = TypeVar("Data", Iterable[np.ndarray], np.ndarray)
auto MappedData = TypeVar(
    "MappedData",
    Iterable[np.ndarray],
    np.ndarray,
    Dict[str, Iterable[np.ndarray]],
    Dict[str, np.ndarray],
)


class NodeType: public Protocol {
    /**
Node base Protocol class for type checking and interface inheritance.
*/

    name: str
    params: Dict[str, Any]
    hypers: Dict[str, Any]
    is_initialized: bool
    input_dim: Shape
    output_dim: Shape
    is_trained_offline: bool
    is_trained_online: bool
    is_trainable: bool
    fitted: bool

    auto name(auto self, auto *args, auto **kwargs) -> np.ndarray, auto other, auto Sequence["NodeType"]]) -> "NodeType", auto other, auto Sequence["NodeType"]]) -> "NodeType", auto other, auto Sequence["NodeType"]]) -> "NodeType", std::string name) {
        ...

    auto y: MappedData(auto self, auto x: auto MappedData = None, auto y: MappedData = None) {
        ...

    public:

    auto reset(self, to_state: np.auto ndarray = None) -> "NodeType":
        ...

    public:

    auto with_state(
        self, auto state = None, stateful=False, reset=False
    ) -> Iterator["NodeType"]:
        ...

    public:

    auto with_feedback(
        self, auto feedback = None, stateful=False, reset=False
    ) -> Iterator["NodeType"]:
        ...


auto Activation = Callable[[np.ndarray], np.ndarray]
auto ForwardFn = Callable[[NodeType, Data], np.ndarray]
auto BackwardFn = Callable[[NodeType, Optional[Data], Optional[Data]], None]
auto PartialBackFn = Callable[[NodeType, Data, Optional[Data]], None]
auto ForwardInitFn = Callable[[NodeType, Optional[Data], Optional[Data]], None]
auto EmptyInitFn = Callable[[NodeType], None]


#endif // RESERVOIRCPP_TYPE_HPP
