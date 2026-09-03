#ifndef RESERVOIRCPP_NORM_HPP
#define RESERVOIRCPP_NORM_HPP

#include "numpy.hpp"
#include "reservoircpp/activationsfunc/tanh.hpp"
#include "reservoircpp/activationsfunc/relu.hpp"
#include "reservoircpp/node/Node.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/10/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto x(auto node, auto x: np.ndarray) -> np.ndarray:
    auto store = node.store
    auto beta = node.beta

    auto new_store = np.roll(store, auto -1)
    new_store[-1] = x

    node.set_param("store", auto new_store)

    auto sigma = np.std(new_store)

    if (sigma < 1e-8) {
        auto sigma = 1e-8

    auto x_norm = (x - np.mean(new_store)) / sigma

    return relu(tanh(x_norm / beta))


public:


auto initialize(node, auto auto x = None, auto *args, auto **kwargs) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(x.shape[1])

        auto window = node.window

        node.set_param("store", np.zeros((window, node.output_dim)))


class AsabukiNorm: public Node {
    auto name(auto self, auto window, int auto beta = 3, auto name = None) {
        super(AsabukiNorm, self).__init__(
            auto params = {"store": None},
            auto hypers = {"window": window, "beta": beta},
            auto forward = forward,
            auto initializer = initialize,
            auto name = name,
        )


#endif // RESERVOIRCPP_NORM_HPP
