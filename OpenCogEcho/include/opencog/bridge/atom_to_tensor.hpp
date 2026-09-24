#pragma once
/**
 * @file atom_to_tensor.hpp
 * @brief Convert OpenCog AtomSpace atoms to Eigen tensors
 *
 * Feature F1.1.3: Type Conversion Layer
 *
 * Conversion mapping:
 *   ConceptNode set   → VectorXf activation
 *   EvaluationLinks   → MatrixXf weight matrix
 *   StateLink         → VectorXf ESN state
 *   Attentional focus → VectorXf sparse activation
 */

#include <opencog/bridge/tensor_atom_types.hpp>
#include <opencog/atomspace/atomspace.hpp>
#include <opencog/attention/attention_bank.hpp>

#include <Eigen/Core>

#include <string>
#include <vector>

namespace opencog::bridge {

/**
 * @brief Convert a set of ConceptNodes to an activation vector.
 *
 * Each node's TruthValue strength becomes the corresponding vector element.
 * The result vector has one element per handle (in order).
 *
 * @param space   Source AtomSpace
 * @param nodes   Handles to ConceptNodes (order determines vector indices)
 * @return Activation vector (size = nodes.size())
 */
[[nodiscard]] ConversionResult<Eigen::VectorXf>
nodes_to_activation(const AtomSpace& space,
                    const std::vector<Handle>& nodes);

/**
 * @brief Convert EvaluationLinks (with ListLink(src, tgt)) to a weight matrix.
 *
 * Discovers source and target node sets from the provided EvaluationLinks.
 * Returns a matrix M where M(src_idx, tgt_idx) = link's TruthValue strength.
 *
 * @param space   Source AtomSpace
 * @param links   Handles to EvaluationLinks
 * @return Weight matrix (rows = unique sources, cols = unique targets)
 */
[[nodiscard]] ConversionResult<Eigen::MatrixXf>
links_to_matrix(const AtomSpace& space,
                const std::vector<Handle>& links);

/**
 * @brief Convert a StateLink atom back to an ESN state vector.
 *
 * Expects the StateLink structure:
 *   (StateLink (AnchorNode label) (ListLink (NumberNode v0) (NumberNode v1) ...))
 * Parses each NumberNode name as a float.
 *
 * @param space       Source AtomSpace
 * @param state_link  Handle to the StateLink
 * @return State vector
 */
[[nodiscard]] ConversionResult<Eigen::VectorXf>
atom_to_state(const AtomSpace& space,
              Handle state_link);

/**
 * @brief Convert the attentional focus set to an activation vector.
 *
 * Atoms in the attentional focus are ordered by STI (descending).
 * Each element is the atom's STI value.
 *
 * @param space Source AtomSpace
 * @param bank  AttentionBank that tracks the focus set
 * @param max_size Maximum vector size (0 = unlimited)
 * @return Activation vector (size = min(AF_size, max_size))
 */
[[nodiscard]] ConversionResult<Eigen::VectorXf>
attention_focus_to_vector(const AtomSpace& space,
                          const AttentionBank& bank,
                          size_t max_size = 0);

} // namespace opencog::bridge
