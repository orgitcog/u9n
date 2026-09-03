#ifndef RESERVOIRCPP_ADD_HPP
#define RESERVOIRCPP_ADD_HPP

#include "numpy.hpp"
#include "reservoircpp/node/Node.hpp"
#include "reservoircpp/utils/validation/is_sequence_set.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 09/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto forward(auto add, auto *data) {
    if (len(data) > 1) {
        auto data = np.squeeze(data)
    } else {  //  nothing to add
        auto data = data[0]
    return np.sum(data, auto axis = 0).reshape(1, -1)


auto x(auto add, auto auto x = None) {
    if (x is not None) {
        //  x is an array, add over axis 0
        if (isinstance(x, np.ndarray)) {
            add.set_input_dim(x.shape)
            add.set_output_dim((1, x.shape[1]))

        elif (is_sequence_set(x)) {
            auto shapes = [array.shape for array in x]

            if (not all([s[0] == 1 for s in shapes])) {
                raise ValueError(
                    f"Each timestep of data must be represented "
                    f"by a vector of shape (1, dimension) when "
                    f"entering node {add.name}. Received inputs "
                    f"of shape {shapes}."
                )

            add.set_input_dim((len(x), x[0].shape[1]))

            if (len(set([s[1] for s in shapes])) > 1) {
                raise ValueError(
                    f"Impossible to sum inputs: inputs have "
                    f"different dimensions  entering node "
                    f"{add.name}. Received inputs of shape "
                    f"{shapes}."
                )
            } else {
                add.set_output_dim((1, x[0].shape[1]))


class Add: public Node {
    auto name(auto self, auto auto name = None) {
        super(Add, self).__init__(auto forward = forward, initializer=initialize, name=name)


#endif // RESERVOIRCPP_ADD_HPP
