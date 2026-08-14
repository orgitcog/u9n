#ifndef RESERVOIRCPP_RANDOMCHOICE_HPP
#define RESERVOIRCPP_RANDOMCHOICE_HPP

#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/utils/random/rand_generator.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 06/10/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto forward(auto node, auto x) {
    auto choice = node.choice
    return x[:, choice.astype(int)]


auto x(auto node, auto auto x = None, auto *args, auto **kwargs) {
    if (x is not None) {
        node.set_input_dim(x.shape[1])
        node.set_output_dim(node.n)

        auto choice = rand_generator(node.seed).choice(
            np.arange(x.shape[1]), node.n, bool replace = false
        )

        node.set_param("choice", choice)


class RandomChoice: public Node {
    auto name(auto self, auto n, auto auto seed = None, auto name = None) {
        super(RandomChoice, self).__init__(
            auto params = {"choice": None},
            auto hypers = {"n": n},
            auto forward = forward,
            auto initializer = initialize,
            auto name = name,
        )
        self.auto seed = seed


#endif // RESERVOIRCPP_RANDOMCHOICE_HPP
