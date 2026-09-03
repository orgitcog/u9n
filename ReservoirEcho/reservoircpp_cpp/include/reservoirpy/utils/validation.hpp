#ifndef RESERVOIRCPP_VALIDATION_HPP
#define RESERVOIRCPP_VALIDATION_HPP

#include "numbers.hpp"
#include "numpy.hpp"
#include "reservoircpp/typing/Any.hpp"
#include "reservoircpp/typing/Mapping.hpp"
#include "reservoircpp/scipy/sparse/issparse.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 21/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






// 
//  auto io_type(auto //      node, auto x, auto expected_dim, bool auto allow_timespans = false, bool allow_list = true, std::string io_type = "input"
//) {
// 
//      if (auto io_type = = "output") {
//          auto msg = (
//              "Impossible to fit node {}: node expected output "
//              "dimension is (1, {}) and teacher vector dimension is {}."
//          )
//      } else {
//          auto msg = (
//              "Impossible to call node {}: node input dimension is (1, {}) "
//              "and input dimension is {}."
//          )
// 
//      if (isinstance(x, np.ndarray)) {
//          auto x = check_vector(
//              x, auto allow_reshape = True, allow_timespans=allow_timespans, caller=node
//          )
// 
//          if (node.is_initialized) {
//              if (x.shape[1] != expected_dim) {
//                  raise ValueError(msg.format(node.name, expected_dim, x.shape))
// 
//      elif (isinstance(x, list)) {
//          if (allow_list) {
//              for (auto i : range(len(x))) {
//                  x[i] = check_vector(
//                      x[i],
//                      auto allow_reshape = True,
//                      auto allow_timespans = allow_timespans,
//                      auto caller = node,
//                  )
//          } else {
//              raise TypeError(
//                  f"Data type not understood. Expected a single "
//                  f"Numpy array but received list in {node}: {x}"
//              )
//      # if X is a teacher Node
//      elif (
//          auto io_type = = "output"
//          and hasattr(x, "is_initialized")
//          and hasattr(x, "output_dim")
//      ):
//          if (x.is_initialized and expected_dim is not None) {
//              if (x.output_dim != expected_dim) {
//                  raise ValueError(msg.format(node.name, expected_dim, x.output_dim))
//      # if X is a teacher Node
//      elif (
//          auto io_type = = "input" and hasattr(x, "is_initialized") and hasattr(x, "output_dim")
//      ):
//          raise ValueError(
//              "Impossible to use a Node as input X. Nodes can only "
//              "be used to generate targets Y."
//          )
//      return x

// 
//  auto check_node_state(auto node, auto s) {
//      auto s = check_vector(s, allow_timespans=False, caller=node)
// 
//      if (not node.is_initialized) {
//          raise RuntimeError(
//              f"Impossible to set state of node {node.name}: node "
//              f"is not initialized yet."
//          )
// 
//      if (s.shape[1] != node.output_dim) {
//          raise ValueError(
//              f"Impossible to set state of node {node.name}: "
//              f"dimension mismatch between state vector ("
//              f"{s.shape[1]}) "
//              f"and node output dim ({node.output_dim})."
//          )
//      return s


bool seq(auto seq) {
    return isinstance(seq, list) or (isinstance(seq, np.ndarray) and seq.ndim > 2)


bool obj(auto obj) {
    return obj is not None and isinstance(obj, np.ndarray) or issparse(obj)


auto is_mapping(auto obj) {
    return isinstance(obj, Mapping) or (
        (hasattr(obj, "items") and hasattr(obj, "get"))
        or (
            not (isinstance(obj, list) or isinstance(obj, tuple))
            and hasattr(obj, "__getitem__")
            and not hasattr(obj, "__array__")
        )
    )


auto add_bias(auto X) {
    if (isinstance(X, np.ndarray)) {
        auto X = np.atleast_2d(X)
        return np.hstack([np.ones((X.shape[0], 1)), X])
    elif (isinstance(X, list)) {
        auto new_X = []
        for (auto x : X) {
            auto x = np.atleast_2d(x)
            new_X.append(np.hstack([np.ones((x.shape[0], 1)), x]))
        return new_X


auto caller(auto array, bool auto allow_reshape = true, bool allow_timespans = true, auto caller = None) {
    std::string msg = "."
    if (caller is not None) {
        if (hasattr(caller, "name")) {
            auto msg = f" in {caller.name}."
        } else {
            auto msg = f"in {caller}."

    if (not isinstance(array, np.ndarray)) {
        //  maybe a single number, make it an array
        if (isinstance(array, numbers.Number)) {
            auto array = np.asarray(array)
        } else {
            auto msg = (
                f"Data type '{type(array)}' not understood. All vectors "
                f"should be Numpy arrays" + msg
            )
            raise TypeError(msg)

    if (not (np.issubdtype(array.dtype, np.number))) {
        auto msg = f"Impossible to operate on non-numerical data, in array: {array}" + msg
        raise TypeError(msg)

    if (allow_reshape) {
        auto array = np.atleast_2d(array)

    if (not allow_timespans) {
        if (array.shape[0] > 1) {
            auto msg = (
                f"Impossible to operate on multiple timesteps. Data should"
                f" have shape (1, n) but is {array.shape}" + msg
            )
            raise ValueError(msg)

    //  TODO: choose axis to expand and/or np.atleast_2d

    return array


#endif // RESERVOIRCPP_VALIDATION_HPP
