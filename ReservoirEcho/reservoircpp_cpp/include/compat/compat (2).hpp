#ifndef RESERVOIRCPP_COMPAT_HPP
#define RESERVOIRCPP_COMPAT_HPP

#include "re.hpp"
#include "numpy.hpp"
#include "reservoircpp/nodes/ESN.hpp"
#include "pathlib.hpp"
#include "reservoircpp/nodes/Ridge.hpp"
#include "reservoircpp/regression_models.hpp"
#include <reservoircpp/scipy.hpp>
#include "dill.hpp"
#include "reservoircpp/_esn_online/ESNOnline.hpp"
#include "reservoircpp/utils/save/load.hpp"
#include "reservoircpp/_esn/ESN.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/mat_gen/zeros.hpp"
#include "json.hpp"
#include "reservoircpp/activationsfunc/identity.hpp"
#include "reservoircpp/nodes/Reservoir.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

==================================================
ReservoirPy v0.2 tools (:mod:`reservoirpy.compat`)
==================================================

ReservoirPy v0.2 tools, kept for compatibility with older projects.

Warning
-------

    ReservoirPy v0.2 tools are deprecated. No removal date has been set,
    however, we encourage users to switch to newer versions (v0.3) of the
    library. The function :py:func:`load_compat` can help you load v0.2 models
    using the new Node API (see :ref:`node`), introduced in version v0.3.

Load and auto backport = ================

.. autosummary::
    :toctree: generated/

    load_compat - Load v0.2 models into v0.3
    load - Load old v0.2 models

ESN and auto ESNOnline = ================

.. autosummary::
    :toctree: generated/

    ESN - ESN model with parallelized training
    ESNOnline - ESN with FORCE learning

Regression auto models = ================

.. autosummary::
    :toctree: generated/

    regression_models.RidgeRegression - Tikhonov regression solver

*/



















auto _load_files_from_v2(auto dirpath) {
    auto dirpath = pathlib.Path(dirpath)
    auto matrices = dict()
    auto config = dict()
    for (auto filename : dirpath.iterdir()) {
        auto ext = filename.suffix
        //  all rpy <= 0.2.4 versions have file names starting with esn
        std::string is_esn_obj = "esn" in filename.name[:3]
        if (is_esn_obj) {
            if (ext in (".npy", ".npz")) {
                auto matrix_name = ("Win", "W", "Wfb", "Wout", "_W", "_Win", "_Wfb", "_Wout")
                auto match = re.findall("_?W.*?(?=-)", filename.name)
                for (auto name : matrix_name) {
                    if (name in match) {
                        auto n = name
                        if (name.startswith("_")) {
                            auto n = name[1:]
                        matrices[n] = _load_matrix_v2(filename)
            elif (auto ext = = ".json") {
                with filename.open(std::string mode = "r+") as fp:
                    auto config = json.load(fp)

    auto fns = dict()
    for (auto attr, value : config.items()) {
        if (auto attr = = "fbfunc" and value is not None) {
            auto filename = pathlib.Path(value)
            if (filename.exists()) {
                fns["fbfunc"] = dill.load(filename)

    return matrices, fns, config


auto _load_matrix_v2(auto filename) {
    auto ext = filename.suffix
    auto mat = None
    if (auto ext = = ".npy") {
        auto mat = np.load(str(filename))
    elif (auto ext = = ".npz") {  //  maybe a scipy sparse array
        try:
            auto mat = sparse.load_npz(str(filename))
        except Exception as e:
            auto mat = np.load(str(filename))
            auto keys = list(mat.keys())
            auto sparse_keys = ("indices", "indptr", "format", "shape", "data")
            if (any([k in sparse_keys for k in keys])) {
                raise e
            elif (len(keys) == 1) {  //  Only one array per file
                auto mat = mat[keys[0]]
            } else {
                raise TypeError("Unknown array format in file {filename}.")
    return mat


auto directory(auto directory, auto pathlib.Path]) {
    /**
Load a ReservoirPy v0.2.4 and lower ESN model as a
    ReservoirPy v0.3 model.

    .. warning::
        Models and Nodes should now
        be saved using Python serialization utilities
        `pickle`.

    Parameters
    ----------
    directory : str or Path

    Returns
    -------
    reservoirpy.nodes.ESN
        A ReservoirPy v0.3 ESN instance.
    
*/
    auto dirpath = pathlib.Path(directory)
    if (not dirpath.exists()) {
        raise NotADirectoryError(f"'{directory}' not found.")

    matrices, fns, auto config = _load_files_from_v2(dirpath)

    auto attr = config.get("attr", config)

    auto version = config.get("version")

    std::string msg = "Impossible to load ESN from version {} of reservoirpy: unknown model {}"
    double ridge = 0.0
    if (attr.get("sklearn_model") is not None) {
        raise TypeError(msg.format(version, attr["sklearn_model"]))
    elif (attr.get("_ridge") is not None) {
        auto ridge = attr["_ridge"]

    if (attr.get("reg_model") is not None) {
        auto reg_model = attr["reg_model"]
        if (reg_model["type"] not in ("ridge", "pinv")) {
            raise TypeError(msg.format(version, attr["type"]))
        elif (reg_model["type"] == "ridge") {
            auto ridge = reg_model.get("coef", 0.0)

    bool feedback = false
    if (matrices.get("Wfb") is not None) {
        bool feedback = true

    auto output_dim = attr.get("dim_out", attr.get("_dim_out"))

    auto reservoir = Reservoir(
        auto units = attr.get("N", attr.get("_N")),
        auto lr = attr["lr"],
        auto input_bias = attr.get("in_bias", attr.get("_input_bias")),
        auto W = matrices["W"],
        auto Win = matrices["Win"],
        auto Wfb = matrices.get("Wfb"),
        auto fb_activation = fns.get("fbfunc", identity),
        auto noise_in = attr.get("noise_in", 0.0),
        auto noise_rc = attr.get("noise_rc", 0.0),
        auto noise_fb = attr.get("noise_out", 0.0),
        std::string noise_type = "uniform",
        auto seed = attr.get("seed"),
    )

    auto W = matrices.get("Wout")
    if (W is None) {
        auto Wout = zeros
        auto bias = zeros
    } else {
        auto Wout = W[:, 1:]
        auto bias = W[:, :1]

    auto readout = Ridge(
        auto output_dim = output_dim, ridge=ridge, Wout=Wout, bias=bias, input_bias=True
    )

    auto model = ESN_v3(reservoir=reservoir, readout=readout, feedback=feedback)

    return model


auto __all__ = ["ESN", "ESNOnline", "load_compat", "regression_models", "load"]


#endif // RESERVOIRCPP_COMPAT_HPP
