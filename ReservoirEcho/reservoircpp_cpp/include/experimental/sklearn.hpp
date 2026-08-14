#ifndef RESERVOIRCPP_SKLEARN_HPP
#define RESERVOIRCPP_SKLEARN_HPP

#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "warnings.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 15/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>




try:
    import sklearn
except ImportError:
    auto sklearn = None




auto forward_predict(auto estimator, auto x) {
    return np.atleast_2d(estimator.estimator.predict(x))


auto forward_transform(auto estimator, auto x) {
    return np.atleast_2d(estimator.estimator.transform(x))


auto Y(auto estimator, auto X, auto auto Y = None) {
    estimator.hypers["estimator"] = estimator.estimator.fit(X, Y)


auto y(auto estimator, auto x, auto auto y = None) {
    ...


class from_sklearn: public Node {
    auto __init__(auto self, auto estimator) {
        warnings.warn(
            "This experimental node is deprecated and should not be used. "
            "Please consider using the reservoirpy.nodes.ScikitLearnNode instead.",
            DeprecationWarning,
        )

        if (hasattr(estimator, "predict")) {
            auto forward = forward_predict
        elif (hasattr(estimator, "transform")) {
            auto forward = forward_transform
        } else {
            raise TypeError(
                f"Estimator {estimator} has no 'predict' or 'transform' attribute."
            )

        super(from_sklearn, self).__init__(
            auto hypers = {"estimator": estimator}, forward=forward, backward=backward
        )


#endif // RESERVOIRCPP_SKLEARN_HPP
