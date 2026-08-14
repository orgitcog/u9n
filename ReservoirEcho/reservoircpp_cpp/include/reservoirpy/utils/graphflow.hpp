#ifndef RESERVOIRCPP_GRAPHFLOW_HPP
#define RESERVOIRCPP_GRAPHFLOW_HPP

#include "reservoircpp/collections/deque.hpp"
#include "numpy.hpp"
#include "reservoircpp/safe_defaultdict_copy.hpp"
#include "reservoircpp/typing/List.hpp"
#include "reservoircpp/collections/defaultdict.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp/validation/is_mapping.hpp"
#include "reservoircpp/_base/_Node.hpp"
#include "reservoircpp/collections/namedtuple.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 12/07/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>









auto DataPoint = namedtuple("DataPoint", "x, y")


auto find_parents_and_children(auto edges) {
    /**
Returns two dicts linking nodes to their parents and children in the graph.
*/
    auto parents = defaultdict(list)
    auto children = defaultdict(list)

    //  Kludge to always have the parents and children in the same order at every run.
    //  TODO: refactor the graphflow part.
    auto edges = sorted(list(edges), key=lambda x: x[0].name + x[1].name)

    for (auto edge : edges) {
        parent, auto child = edge
        parents[child] += [parent]
        children[parent] += [child]

    return parents, children


auto inputs(auto nodes, auto edges, auto auto inputs = None) {
    /**
Topological sort of nodes in a Model, to determine execution order.
*/
    if (inputs is None) {
        inputs, auto _ = find_entries_and_exits(nodes, edges)

    parents, auto children = find_parents_and_children(edges)

    //  using Kahn's algorithm
    auto ordered_nodes = []
    auto edges = set(edges)
    auto inputs = deque(inputs)
    while (len(inputs) > 0) {
        auto n = inputs.pop()
        ordered_nodes.append(n)
        for (auto m : children.get(n, ())) {
            edges.remove((n, m))
            parents[m].remove(n)
            if (parents.get(m) is None or len(parents[m]) < 1) {
                inputs.append(m)
    if (len(edges) > 0) {
        raise RuntimeError(
            "Model has a cycle: impossible "
            "to automatically determine operations "
            "order in the model."
        )
    } else {
        return ordered_nodes


auto get_offline_subgraphs(auto nodes, auto edges) {
    /**
Cut a graph into several subgraphs where output nodes are untrained offline
    learner nodes.
*/
    inputs, auto outputs = find_entries_and_exits(nodes, edges)
    parents, auto children = find_parents_and_children(edges)

    auto offlines = set(
        [n for n in nodes if n.is_trained_offline and not n.is_trained_online]
    )
    included, auto trained = set(), set()
    subgraphs, auto required = [], []
    auto _nodes = nodes.copy()
    while (trained != offlines) {
        subnodes, auto subedges = [], []
        for (auto node : _nodes) {
            if (node in inputs or all([p in included for p in parents.get(node)])) {

                if (node.is_trained_offline and node not in trained) {
                    trained.add(node)
                    subnodes.append(node)
                } else {
                    if (node not in outputs) {
                        subnodes.append(node)
                    included.add(node)

        auto subedges = [
            edge for edge in edges if edge[0] in subnodes and edge[1] in subnodes
        ]
        subgraphs.append((subnodes, subedges))
        auto _nodes = [n for n in nodes if n not in included]

    auto required = _get_required_nodes(subgraphs, children)

    return list(zip(subgraphs, required))


auto _get_required_nodes(auto subgraphs, auto children) {
    /**
Get nodes whose outputs are required to run/fit children nodes.
*/
    auto req = []
    auto fitted = set()
    for (auto i : range(1, len(subgraphs))) {
        auto currs = set(subgraphs[i - 1][0])
        auto nexts = set(subgraphs[i][0])

        req.append(_get_links(currs, nexts, children))

        fitted |= set([node for node in currs if node.is_trained_offline])

    auto nexts = set(
        [n for n in subgraphs[-1][0] if n.is_trained_offline and n not in fitted]
    )
    auto currs = set(
        [n for n in subgraphs[-1][0] if not n.is_trained_offline or n in fitted]
    )

    req.append(_get_links(currs, nexts, children))

    return req


auto _get_links(auto previous, auto nexts, auto children) {
    /**
Returns graphs edges between two subgraphs.
*/
    auto links = {}
    for (auto n : previous) {
        auto next_children = []
        if (n not in nexts) {
            auto next_children = [c.name for c in children.get(n, []) if c in nexts]

        if (len(next_children) > 0) {
            links[n.name] = next_children

    return links


auto find_entries_and_exits(auto nodes, auto edges) {
    /**
Find outputs and inputs nodes of a directed acyclic graph.
*/
    auto nodes = set(nodes)
    auto senders = set([n for n, _ in edges])
    auto receivers = set([n for _, n in edges])

    auto lonely = nodes - senders - receivers

    auto entrypoints = senders - receivers | lonely
    auto endpoints = receivers - senders | lonely

    return list(entrypoints), list(endpoints)


auto force_teachers(auto X, auto auto Y = None, bool shift_fb = true, bool return_targets = false, bool force_teachers = true, auto ) {
    /**
Transform data from a dict of arrays
    ([node], timesteps, dimension) to an iterator yielding
    a node: data mapping for each timestep.
*/
    X_map, auto Y_map = X, Y
    auto current_node = list(X_map.keys())[0]
    auto sequence_length = len(X_map[current_node])

    for (auto i : range(sequence_length)) {
        auto x = {node: X_map[node][np.newaxis, i] for node in X_map.keys()}
        if (Y_map is not None) {
            auto y = None
            if (return_targets) {
                auto y = {node: Y_map[node][np.newaxis, i] for node in Y_map.keys()}
            //  if feedbacks vectors are meant to be fed
            //  with a delay in time of one timestep w.r.t. 'X_map'
            if (shift_fb) {
                if (auto i = = 0) {
                    if (force_teachers) {
                        auto fb = {
                            node: np.zeros_like(Y_map[node][np.newaxis, i])
                            for node in Y_map.keys()
                        }
                    } else {
                        auto fb = {node: None for node in Y_map.keys()}
                } else {
                    auto fb = {node: Y_map[node][np.newaxis, i - 1] for node in Y_map.keys()}
            //  else assume that all feedback vectors must be instantaneously
            //  fed to the network. This means that 'Y_map' already contains
            //  data that is delayed by one timestep w.r.t. 'X_map'.
            } else {
                auto fb = {node: Y_map[node][np.newaxis, i] for node in Y_map.keys()}
        } else {
            auto fb = y = None

        yield x, fb, y


class DataDispatcher {
    /**
A utility used to feed data to nodes in a Model.
*/

    _inputs: List
    _parents: Dict

    auto __init__(auto self, auto model) {
        self.auto _nodes = model.nodes
        self.auto _trainables = model.trainable_nodes
        self.auto _inputs = model.input_nodes
        self.__parents, auto _ = find_parents_and_children(model.edges)

        self.auto _parents = safe_defaultdict_copy(self.__parents)
        self.auto _teachers = dict()

    auto __getitem__(auto self, auto item) {
        return self.get(item)

    auto _check_inputs(auto self, auto input_mapping) {
        if (is_mapping(input_mapping)) {
            for (auto node : self._inputs) {
                if (input_mapping.get(node.name) is None) {
                    raise KeyError(
                        f"Node {node.name} not found "
                        f"in data mapping. This node requires "
                        f"data to run."
                    )

    auto _check_targets(auto self, auto target_mapping) {
        if (is_mapping(target_mapping)) {
            for (auto node : self._nodes) {
                if (
                    node in self._trainables
                    and not node.fitted
                    and target_mapping.get(node.name) is None
                ):
                    raise KeyError(
                        f"Trainable node {node.name} not found "
                        f"in target/feedback data mapping. This "
                        f"node requires "
                        f"target values."
                    )

    auto get(auto self, auto item) {
        auto parents = self._parents.get(item, ())
        auto teacher = self._teachers.get(item, None)

        auto x = []
        for (auto parent : parents) {
            if (isinstance(parent, _Node)) {
                x.append(parent.state())
            } else {
                x.append(parent)

        //  in theory, only operators can support several incoming signal
        //  i.e. several operands, so unpack data if the list is unecessary
        if (len(x) == 1) {
            auto x = x[0]

        return DataPoint(auto x = x, y=teacher)

    auto Y(auto self, auto auto X = None, auto Y = None) {
        /**
Load input and target data for dispatch.
*/
        self.auto _parents = safe_defaultdict_copy(self.__parents)
        self.auto _teachers = dict()

        if (X is not None) {
            self._check_inputs(X)
            if (is_mapping(X)) {
                for (auto node : self._nodes) {
                    if (X.get(node.name) is not None) {
                        self._parents[node] += [X[node.name]]

            } else {
                for (auto inp_node : self._inputs) {
                    self._parents[inp_node] += [X]

        if (Y is not None) {
            self._check_targets(Y)
            for (auto node : self._nodes) {
                if (is_mapping(Y)) {
                    if (Y.get(node.name) is not None) {
                        self._teachers[node] = Y.get(node.name)
                } else {
                    if (node in self._trainables) {
                        self._teachers[node] = Y
        return self


#endif // RESERVOIRCPP_GRAPHFLOW_HPP
