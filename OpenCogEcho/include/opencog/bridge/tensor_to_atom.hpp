#pragma once
/**
 * @file tensor_to_atom.hpp
 * @brief Convert Eigen tensors to OpenCog AtomSpace atoms
 *
 * Feature F1.1.3: Type Conversion Layer
 *
 * Conversion mapping:
 *   VectorXf activation  → ConceptNode set with TruthValues
 *   MatrixXf weights     → EvaluationLink graph
 *   VectorXf ESN state   → StateLink atom
 *   MatrixXf time-series → AtTimeLink chain
 */

#include <opencog/bridge/tensor_atom_types.hpp>
#include <opencog/atomspace/atomspace.hpp>

#include <Eigen/Core>

#include <string>
#include <vector>

namespace opencog::bridge {

/**
 * @brief Convert an activation vector to a set of ConceptNodes.
 *
 * Each element i with value v (above threshold) creates:
 *   (ConceptNode "{prefix}_{i}")
 * with TruthValue{v, confidence} and AttentionValue{v * sti_scale, 0, 0}.
 *
 * @param space   Target AtomSpace (atoms are created/updated in place)
 * @param vec     Activation vector (1-D Eigen column vector)
 * @param policy  Conversion policy (threshold, naming, confidence)
 * @return Handles of created/updated ConceptNodes
 */
[[nodiscard]] ConversionResult<std::vector<Handle>>
activation_to_nodes(AtomSpace& space,
                    const Eigen::VectorXf& vec,
                    const ConversionPolicy& policy = {});

/**
 * @brief Convert a weight matrix to an EvaluationLink graph.
 *
 * For each non-negligible entry M(i,j) the function creates:
 *   (EvaluationLink
 *       (PredicateNode "{prefix}_weight")
 *       (ListLink
 *           (ConceptNode "{prefix}_src_{i}")
 *           (ConceptNode "{prefix}_tgt_{j}")))
 * with TruthValue{|M(i,j)|, confidence}.
 *
 * Source/target nodes are created if they don't already exist.
 *
 * @param space   Target AtomSpace
 * @param mat     Weight matrix (rows = source neurons, cols = target neurons)
 * @param policy  Conversion policy
 * @return Handles of created EvaluationLinks
 */
[[nodiscard]] ConversionResult<std::vector<Handle>>
matrix_to_links(AtomSpace& space,
                const Eigen::MatrixXf& mat,
                const ConversionPolicy& policy = {});

/**
 * @brief Convert an ESN reservoir state vector to a StateLink atom.
 *
 * Creates:
 *   (StateLink
 *       (AnchorNode "{label}")
 *       (ConceptNode "{label}_state"))
 * The ConceptNode gets a TruthValue encoding the L2 norm of the state,
 * and an AttentionValue proportional to the vector's energy.
 * Individual state elements are stored as NumberNode children in a ListLink.
 *
 * @param space  Target AtomSpace
 * @param state  Reservoir state vector
 * @param label  Semantic label (e.g., "stream1_reservoir")
 * @param policy Conversion policy
 * @return Handle of the created StateLink
 */
[[nodiscard]] ConversionResult<Handle>
state_to_atom(AtomSpace& space,
              const Eigen::VectorXf& state,
              const std::string& label = "reservoir",
              const ConversionPolicy& policy = {});

/**
 * @brief Convert a temporal pattern matrix to AtTimeLink atoms.
 *
 * Each row t of the matrix represents a timestep. The function creates:
 *   (AtTimeLink
 *       (NumberNode "t")
 *       (ListLink <ConceptNodes for timestep t>))
 *
 * @param space   Target AtomSpace
 * @param pattern Matrix (rows = timesteps, cols = features)
 * @param label   Semantic label
 * @param policy  Conversion policy
 * @return Handles of created AtTimeLinks (one per timestep)
 */
[[nodiscard]] ConversionResult<std::vector<Handle>>
temporal_pattern_to_atoms(AtomSpace& space,
                          const Eigen::MatrixXf& pattern,
                          const std::string& label = "temporal",
                          const ConversionPolicy& policy = {});

} // namespace opencog::bridge
