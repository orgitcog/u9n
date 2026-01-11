// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>
#include <nls/geometry/SnapConfig.h>
#include <nls/geometry/BarycentricCoordinates.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::skinningweightutils)


/**
* Take skinning weights for a combined body mesh, a mapping between face joint ids and body joint ids, and a snap config to map from source vertex indices (on the body)
* to target vertex indices (on the face). The results are updated in updatedHeadSkinningWeightsDense.
* @pre The function makes the assumption that the first updatedHeadSkinningWeightsDense.rows() rows of skinningWeightsCombinedBody contain head vertices ie 1 to 1 correspondence.
* @pre faceBodyJointMapping must contain the same number of elements as updatedHeadSkinningWeightsDense has rows.
* @param[in] skinningWeightsCombinedBody: the skinning weights for a combined body rig mesh
* @param[in] faceBodyJointMapping: the mapping for each face joint id to each body joint id (for each face joint id the corresponding body joint or body joint parent id is given, and will be set to true
* if a direct corresponding and false if the joint id is a parent joint)
* @param[in] snapConfig: a snap config for the neck seam defining mapping between source vertices in the body to target vertices in the head mesh
* @param[out] updatedHeadSkinningWeightsDense: will contain the output (dense) updated head skinning weights after copying the neck seam vertex skinning weights from the body to those in the head mesh
*/
template <class T>
void SnapNeckSeamSkinningWeightsToBodySkinningWeights(const SparseMatrix<T>& skinningWeightsCombinedBody, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const SnapConfig<T>& snapConfig, Eigen::Matrix<T, -1, -1>& updatedHeadSkinningWeightsDense);


/**
* For each joint in a joint rig, return a vector of all descendant joint indices. Optionally mass in a string (mustContainStr) which the joint name must contain to be considered for
* recursion (for example if you just want to consider FACIAL joints).
* @param[in] jointRig: the rig which we are analyzing
* @param[in] mustContainStr: an optional string which allows us to only consider joints which contain the supplied string (for example if we just want to consider FACIAL joints)
* @returns a map of joint index to a vector containing the joint indices of all the descendants of that joint
*/
template <class T>
std::map<int, std::vector<int>> GetJointChildrenRecursive(const JointRig2<T>& jointRig, const std::string& mustContainStr = {});

/**
 *Calculate the mapping for each body rig joint to corresponding face rig joint. If there is a match, result will contain the matching joint index and true; if
 * not, it will be - 1 and false 
 * @param[in]: faceJointRig the face joint rig
 * @param[in]: bodyJointRig the body joint rig
 * @returns a vector containing for each body joint index, a pair which contains either the matching face joint index and true, or -1 and false if no match
 */
template <class T>
std::vector<std::pair<int, bool>> CalculateBodyFaceJointMapping(const JointRig2<T>& faceJointRig, const JointRig2<T>& bodyJointRig);

/**
 *Calculate the mapping for each face rig joint to corresponding body rig joint. If there is a match, result will contain the matching joint index and true; if
 * not, it will contain the facial parent joint which matches the body and false
 * @param[in]: faceJointRig the face joint rig
 * @param[in]: bodyJointRig the body joint rig
 * @returns a vector containing for each face joint index, a pair which contains either the matching body joint index and true, or the corresponding body rig parent joint and false
 */
template <class T>
std::vector<std::pair<int, bool>> CalculateFaceBodyJointMapping(const JointRig2<T>& faceJointRig, const JointRig2<T>& bodyJointRig);

/**
* Take a vector of barycentric coordinates, a corresponding matrix of vertex positions, and skinning weights for those vertices. 
* Evaluate the barycentric coordinates and return a matrix of skinning weights for each joint, for each element of the vector of barycentric coordinates.
* No pruning or re-normalization is performed by this function currently, so output skinning weights will not necessarily be normalize to sum to 1 per vertex, or have the right number of influences per vertex.
* @param[in] barycentricCoordinates: the barycentric coordinates for each vertex for which we want to create skinning weights; for each vertex, this is the closest point on the mesh of vertices contained in vertices (typically a lower lod)
* @param[in] vertices: a 3 x n vertices matrix containing the coordinates of the vertices at the lower lod; this would typically be lod 0
* @param[in] skinningWeights: a n vertices x n joints sparse matrix containing the skinning weights at the lower lod (typically lod 0)
* @param[out] outputSkinningWeights: on output will contain a matrix of barycentricCoordinates.size() x skinningWeights.cols() skinning weights which are generated by calculating a weighted sum of the source 
* skinningWeights for each set of barycentric coordinates. The output weights are not normalized or pruned.
* @pre skinningWeights must contain the same number of rows as the number of columns in vertices
* @pre barycentric coordinates indices must be consistent with the vertices ie all indices in the range 0 to vertices.cols() - 1
*/
template <class T>
void CalculateSkinningWeightsForBarycentricCoordinates(const std::vector<BarycentricCoordinates<T>>& barycentricCoordinates, const Eigen::Matrix<T, 3, -1>& vertices,
    const SparseMatrix<T>& skinningWeights, SparseMatrix<T>& outputSkinningWeights);


/**
* Take an input sparse matrix of skinning weights which have been generated, and a 'template' matrix of skinning weights for the archetype at a (higher) required lod level, plus a corresponding joint rig, and 
* modify the input skinning weights so that any joint influences not present in the archetype are . Finally, sort, prune and re-normalize the weights for each vertex to the required maximum number per vertex for the lod.
* This function assumes that the joints in different lods follow a simple parent child relationship ie at each level joint influences can simply be summed up and pushed up to their parents (as required).
* The function is templated on the scalar type T and also on the rig type R. This is so that it can work with both a JointRig2 and and a BodyGeometry type (which supports the required functionality)
* @param[in] higherLodBarycentricCoordinates: the barycentric coordinates for each vertex in the higher lod which we want to create skinning weights; for each higher lod vertex, this is the closest point on the lower lod mesh
* @param[in] lowerLodVertices: a 3 x n vertices matrix containing the coordinates of the vertices at the lower lod; this would typically be lod 0
* @param[in] lowerLodSkinningWeights: a n vertices x n joints sparse matrix containing the skinning weights at the lower lod (typically lod 0)
* @params[in] jointMappingFromLod0: a map which for each joint in the rig used in lod0 defines how joint influences should be redistributed to joints in the higher lod
* @params[in] lowerLodSnapConfig: a snap config (which may be empty) which defines any vertices for which the skinning weights should be identical to those in the lower lod (for example the neck seam)
* @params[in] jointRig: the jointRig2 or BodyGeometry for which we are propagating the skinning weights (both support the required functionality)
* @params[in] maxNumWeightsForLod: the maximum number of joint influences per vertex in the output skinning weights
* @params[out] outputHighLodSkinningWeights: on output will contain a matrix of higherLodBarycentricCoordinates.size() x jointRig.NumJoints() skinning weights which are the skinning weights for the higher lod, normalized and 
* pruned to a maximum of maxNumWeightsForLod per vertex
* @pre higherLodBarycentricCoordinates coordinates indices must be consistent with the lowerLodVertices ie all indices in the range 0 to lowerLodVertices.cols() - 1
* @pre lowerLodSkinningWeights.rows() must be equal to lowerLodVertices.cols()
* @pre each joint name in jointMappingFromLod0 must be a valid joint name in jointRig
*/
template <class T, class R>
void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<T>>& higherLodBarycentricCoordinates, const Eigen::Matrix<T, 3, -1>& lowerLodVertices, const SparseMatrix<T>& lowerLodSkinningWeights,
    const std::map<std::string, std::map<std::string, T>>& jointMappingFromLod0, const SnapConfig<T>& lowerLodSnapConfig, const R& jointRig, int maxNumWeightsForLod, SparseMatrix<T>& outputHighLodSkinningWeights);


/*
* sort, prune and renormalize the weights in the skinning weight matrix, limiting the influence for each vertex to maxSkinWeights joints. skin is n vertices rows x n joints columns matrix
* @param[out] skin: the input skinning weight matrix (n vertices rows x n joints columns) which on output will be pruned and normalized such that the joint weights for each vertex sum to 1 and no more than maxSkinWeights are > 0
* @param[in] maxSkinWeights: the maximum number of joint weights for each vertex which may have a non-zero value
*/
template <class T>
void SortPruneAndRenormalizeSkinningWeights(SparseMatrix<T>& skin, int maxSkinWeights);


CARBON_NAMESPACE_END(TITAN_NAMESPACE::skinningweightutils)
