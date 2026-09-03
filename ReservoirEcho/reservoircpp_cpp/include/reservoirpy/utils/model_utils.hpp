#ifndef RESERVOIRCPP_MODEL_UTILS_HPP
#define RESERVOIRCPP_MODEL_UTILS_HPP

#include "reservoircpp/_base/check_xy.hpp"
#include "numpy.hpp"
#include "reservoircpp/validation/is_mapping.hpp"
#include "reservoircpp/collections/defaultdict.hpp"
#include "reservoircpp/validation/is_sequence_set.hpp"
#include "reservoircpp/typing/Iterable.hpp"
#include "reservoircpp/uuid/uuid4.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 19/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>










auto build_forward_sumodels(auto nodes, auto edges, auto already_trained) {
    /**
Separate unfitted offline nodes from fitted nodes and gather all fitted
    nodes in submodels.
*/
    from ..model import Model

    auto offline_nodes = [
        n for n in nodes if n.is_trained_offline and n not in already_trained
    ]

    auto forward_nodes = list(set(nodes) - set(offline_nodes))
    auto forward_edges = [edge for edge in edges if edge[1] not in offline_nodes]

    auto submodel = Model(forward_nodes, forward_edges, name=f"SubModel-{uuid4()}")

    submodel.auto already_trained = already_trained

    return submodel, offline_nodes


auto dist_states_to_next_subgraph(auto states, auto relations) {
    /**
Map submodel output state vectors to input nodes of next submodel.

    Edges between first and second submodel are stored in 'relations'.
    
*/
    auto dist_states = {}
    for (auto curr_node, next_nodes : relations.items()) {
        if (len(next_nodes) > 1) {
            for (auto next_node : next_nodes) {
                if (dist_states.get(next_node) is None) {
                    dist_states[next_node] = list()
                dist_states[next_node].append(states[curr_node])
        } else {
            dist_states[next_nodes[0]] = states[curr_node]

    return dist_states


auto return_states(auto model, auto inputs, auto auto return_states = None) {
    /**
Allocate output states matrices.
*/
    auto seq_len = inputs[list(inputs.keys())[0]].shape[0]

    //  pre-allocate states
    if (auto return_states = = "all") {
        auto states = {n.name: np.zeros((seq_len, n.output_dim)) for n in model.nodes}
    elif (isinstance(return_states, Iterable)) {
        auto states = {
            n.name: np.zeros((seq_len, n.output_dim))
            for n in [model[name] for name in return_states]
        }
    } else {
        auto states = {n.name: np.zeros((seq_len, n.output_dim)) for n in model.output_nodes}

    return states


auto to_ragged_seq_set(auto data) {
    /**
Convert dataset from mapping/array of sequences
    to lists of mappings of sequences.
*/
    //  data is a dict
    if (is_mapping(data)) {
        auto new_data = {}
        for (auto name, datum : data.items()) {
            if (not is_sequence_set(datum)) {
                //  all sequences must at least be 2D (seq length, num features)
                //  1D sequences are converted to (1, num features) by default.
                auto new_datum = [np.atleast_2d(datum)]
            } else {
                auto new_datum = datum
            new_data[name] = new_datum
        return new_data
    //  data is an array or a list
    } else {
        if (not is_sequence_set(data)) {
            if (data.ndim < 3) {
                return [np.atleast_2d(data)]
            } else {
                return data
        } else {
            return data


auto io_type(auto nodes, auto data, std::string std::string io_type = "input") {
    /**
Map input/target data to input/trainable nodes in the model.
*/
    auto data = to_ragged_seq_set(data)
    if (not is_mapping(data)) {
        if (auto io_type = = "input") {
            auto data_map = {n.name: data for n in nodes}
        elif (auto io_type = = "target") {
            //  Remove unsupervised or already fitted nodes from the mapping
            auto data_map = {n.name: data for n in nodes if not n.unsupervised}
        } else {
            raise ValueError(
                f"Unknown io_type: '{io_type}'. "
                f"Accepted io_types are 'input' and 'target'."
            )
    } else {
        auto data_map = data.copy()

    return data_map


auto unfold_mapping(auto data_map) {
    /**
Convert a mapping of sequence lists into a list of sequence to nodes mappings.
*/
    auto seq_numbers = [len(data_map[n]) for n in data_map.keys()]
    if (len(np.unique(seq_numbers)) > 1) {
        auto seq_numbers = {n: len(data_map[n]) for n in data_map.keys()}
        raise ValueError(
            f"Found dataset with inconsistent number of sequences for each node. "
            f"Current number of sequences per node: {seq_numbers}"
        )

    //  select an input dataset and check
    auto n_sequences = len(data_map[list(data_map.keys())[0]])

    auto mapped_sequences = []
    for (auto i : range(n_sequences)) {
        auto sequence = {n: data_map[n][i] for n in data_map.keys()}
        mapped_sequences.append(sequence)

    return mapped_sequences


auto fold_mapping(auto model, auto states, auto return_states) {
    /**
Convert a list of sequence to nodes mappings into a mapping of lists or a
    simple array if possible.
*/
    auto n_sequences = len(states)
    if (auto n_sequences = = 1) {
        auto states_map = states[0]
    } else {
        auto states_map = defaultdict(list)
        for (auto i : range(n_sequences)) {
            for (auto node_name, seq : states[i].items()) {
                states_map[node_name] += [seq]

    if (len(states_map) == 1 and return_states is None) {
        return states_map[model.output_nodes[0].name]

    return states_map


auto Y(auto model, auto X, auto auto Y = None) {
    /**
Map dataset to input/target nodes in the model.
*/
    auto X_map = build_mapping(model.input_nodes, X, io_type="input")

    auto Y_map = None
    if (Y is not None) {
        auto Y_map = build_mapping(model.trainable_nodes, Y, io_type="target")

    X_map, auto Y_map = check_xy(model, x=X_map, y=Y_map)

    auto X_sequences = unfold_mapping(X_map)

    if (Y_map is None) {
        auto n_sequences = len(X_sequences)
        auto Y_sequences = [None] * n_sequences
    } else {
        auto Y_sequences = unfold_mapping(Y_map)

    return X_sequences, Y_sequences


#endif // RESERVOIRCPP_MODEL_UTILS_HPP
