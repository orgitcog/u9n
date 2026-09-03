// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/SnapConfig.h>
#include <nrr/VertexWeights.h>
#include <rig/RigGeometry.h>
#include <nls/geometry/BarycentricCoordinates.h> 

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! a struct for passing parameters to the function below
template <class T>
struct SkinningWeightUpdateParams
{
    //! the snap config for the neck seam for the current lod which maps between src vertex indices 
    //! (on the combined body) to target vertex indices (on the head). 
    SnapConfig<T> neckBodySnapConfig;

    //! a shared pointer to vertex weights which defines a weight for each vertex on head_lod0_mesh and how much weight (0-1) should be given to the body skinning weights for each vertex
    std::shared_ptr<VertexWeights<T>> headVertexSkinningWeightsMask;

    //! a vector for each combined body rig joint which gives the corresponding face rig joint index (or -1 if not present), and a flag set to true if there is a correspondence or false if not
    std::vector<std::pair<int, bool>> bodyFaceJointMapping;

    //! a vector for each face rig joint which gives the corresponding combined body rig joint index (or if no direct correspondence, contains the face rig parent joint id which maps directly to the body), and a flag set to true if there is a direct correspondence or false if not
    std::vector<std::pair<int, bool>> faceBodyJointMapping;

    //! a map of joint index to a vector of joint indices which contains every descendant joint index for the original joint index
    std::map<int, std::vector<int>> faceRigChildrenMap;
};


/**
* Take the supplied skinning weights for head and body rigs, and update the head from the body for the specified mesh name to give updatedHeadSkinningWeightsDense as the output.
* This gives the results for a single mesh / lod.
* A neck blend mask is used to blend skinning weights from the neck (on the body) with those on the head, and the neck seam is snapped to exactly match the skinning weights on the 
* body. Output weights are in a dense matrix. We assume that the first n rows of the combined body skinning weights correspond to identical vertices in the head mesh; for 
* even head lods this will be the case, but for odd head lods this will not be the case, and special handling is needed to prepare the data for this function. Only the skinning weights
* corresponding to head vertices are used from skinningWeightsCombinedBody.
* @param[in] skinningWeightsHead: a sparse matrix of n head mesh vertices rows x n head mesh joints columns containing the original skinning weights for the head mesh
* @param[in] skinningWeightsCombinedBody: a sparse matrix of n combined body vertices rows x n combined body joints containing the skinning weights for the combined body
* @param[in] skinningWeightUpdateParams: a struct containing the parameters used to perform the calculation (see above)
* @param[out] updatedHeadSkinningWeightsDense: on completion contains a dense matrix containing the updated skinning weights for the head mesh for each vertex for each joint. Weights are normalized such that 
* they sum to 1 for each vertex and no more than the original non-zero skinning weights should have non-zero values
*/
template <class T>
void UpdateHeadMeshSkinningWeightsFromBody(const SparseMatrix<T>& skinningWeightsHead, const SparseMatrix<T>& skinningWeightsCombinedBody,
    const SkinningWeightUpdateParams<T>& skinningWeightUpdateParams, Eigen::Matrix<T, -1, -1>& updatedHeadSkinningWeightsDense, std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true));

/**
* Take the supplied head and combined body rigs, and update the head from the body to give updatedHeadSkinningWeightsMap as the output. Results are provided for the head mesh at each lod.
* A set of neck blend masks is used to blend skinning weights from the neck (on the body) with those on the head, and in each case the neck seam is snapped to exactly match the skinning weights on the 
* body. Output weights are in a dense matrix for each lod. 
* @param[in] headRigGeometry: the rig geometry of the head rig which we want the updated skinning weights for
* @param[in] combinedBodySkinningWeights: a vector of combined body skinning weights at each lod
* @param[in] neckBodySnapConfig: a set of snap configs defining the mapping between body and head for the neck seam at each lod
* @param[in] bodyFaceJointMapping: a vector for each combined body rig joint which gives the corresponding face rig joint index (or -1 if not present), and a flag set to true if there is a correspondence or false if not
* @param[in] faceBodyJointMapping: a vector for each face rig joint which gives the corresponding combined body rig joint index (or if no direct correspondence, contains the face rig parent joint id which maps directly to the body), 
* and a flag set to true if there is a direct correspondence or false if not
* @params[in] barycentricCoordinatesForOddLods: a map of (face) lod number to a vector which must contain, for each ODD numbered lod, for each vertex in the previous face lod, i) a flag saying whether the barycentric
* coordinates are valid and ii) barycentric coordinates for how to map the vertex from the previous lod onto the mesh for the current lod. 
* @param[out] updatedHeadSkinningWeights: on completion contains a vector of skinning weight matrices at each lod. Each skinning matrix is represented as a dense matrix containing the updated skinning weights for the 
* head mesh for each vertex for each joint. Weights are normalized such that they sum to 1 for each vertex and no more than the original non-zero skinning weights should have non-zero values
* @param[in] taskThreadPool: a thread pool which operations will be parallelized over
*/
template <class T>
void UpdateHeadMeshSkinningWeightsFromBody(const RigGeometry<T>& headRigGeometry, const std::vector<SparseMatrix<T>> & combinedBodySkinningWeights, const std::map<std::string, std::pair<int, SnapConfig<T>>>& neckBodySnapConfig,
    const std::vector<std::shared_ptr<VertexWeights<T>>>& headVertexSkinningWeightsMasks, const std::vector<std::pair<int, bool>>& bodyFaceJointMapping, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<T>>>>& barycentricCoordinatesForOddLods, std::vector<Eigen::Matrix<T, -1, -1>>& updatedHeadSkinningWeights,
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true));


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
