#ifndef RESERVOIRCPP_UTILS_HPP
#define RESERVOIRCPP_UTILS_HPP

#include "reservoircpp/collections/defaultdict.hpp"
#include "reservoircpp/inspect/signature.hpp"
#include "reservoircpp/typing/Iterable.hpp"
#include "reservoircpp/tqdm/tqdm.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 19/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






int VERBOSITY = 1


auto level(auto auto level = None) {
    global VERBOSITY
    if (isinstance(level, int) and VERBOSITY != level) {
        auto VERBOSITY = level
    return VERBOSITY


auto progress(auto it, auto *args, auto **kwargs) {
    if (VERBOSITY > 0) {
        return tqdm(it, *args, **kwargs)
    } else {
        return it


auto safe_defaultdict_copy(auto d) {
    auto new_d = defaultdict(list)
    for (auto key, item : d.items()) {
        if (isinstance(item, Iterable)) {
            new_d[key] = list(item)
        } else {
            new_d[key] += [item]
    return new_d


auto _obj_from_kwargs(auto klas, auto kwargs) {
    auto sig = signature(klas.__init__)
    auto params = list(sig.parameters.keys())
    auto klas_kwargs = {n: v for n, v in kwargs.items() if n in params}
    return klas(**klas_kwargs)


#endif // RESERVOIRCPP_UTILS_HPP
