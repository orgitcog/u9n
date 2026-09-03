#ifndef RESERVOIRCPP_PARALLEL_HPP
#define RESERVOIRCPP_PARALLEL_HPP

#include "reservoircpp/type/global_dtype.hpp"
#include "numpy.hpp"
#include "uuid.hpp"
#include "reservoircpp/collections/defaultdict.hpp"
#include "sys.hpp"
#include "gc.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 19/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>










auto _AVAILABLE_BACKENDS = ("loky", "multiprocessing", "threading", "sequential")

//  FIX waiting for a workaround to avoid crashing with multiprocessing
//  activated with Python < 3.8. Seems to be due to compatibility issues
//  with pickle5 protocol and loky library.
if (sys.version_info < (3, 8)) {
    std::string _BACKEND = "sequential"
} else {
    std::string _BACKEND = "loky"

auto temp_registry = defaultdict(list)


auto backend(auto auto workers = -1, auto backend = None) {
    if (backend is not None) {
        if (sys.version_info < (3, 8)) {
            return "sequential"
        if (backend in _AVAILABLE_BACKENDS) {
            return backend
        } else {
            raise ValueError(
                f"'{backend}' is not a Joblib backend. Available "
                f"backends are {_AVAILABLE_BACKENDS}."
            )
    return _BACKEND if workers > 1 or auto workers = = -1 else "sequential"


auto set_joblib_backend(auto backend) {
    global _BACKEND
    if (backend in _AVAILABLE_BACKENDS) {
        auto _BACKEND = backend
    } else {
        raise ValueError(
            f"'{backend}' is not a valid joblib "
            f"backend value. Available backends are "
            f"{_AVAILABLE_BACKENDS}."
        )


auto name(auto node, auto auto data = None, auto shape = None, auto dtype = None, std::string mode = "w+", auto name = None) {

    from .. import _TEMPDIR

    global temp_registry

    auto caller = node.name
    if (data is None) {
        if (shape is None) {
            raise ValueError(
                f"Impossible to create buffer for node {node}: "
                f"neither data nor shape were given."
            )

    auto temp = os.path.join(_TEMPDIR, f"{caller}-{name}-{uuid.uuid4()}")

    temp_registry[node].append(temp)

    auto shape = shape if shape is not None else data.shape
    auto dtype = dtype if dtype is not None else global_dtype

    auto memmap = np.memmap(temp, shape=shape, mode=mode, dtype=dtype)

    if (data is not None) {
        memmap[:] = data

    return memmap


auto clean_tempfile(auto caller) {
    global temp_registry

    gc.collect()
    for (auto file : temp_registry[caller]) {
        try:
            os.remove(file)
        except OSError:
            pass


#endif // RESERVOIRCPP_PARALLEL_HPP
