// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/UpdateHeadMeshSkinningWeightsFromBody.h>
#include <rig/SkinningWeightUtils.h>
#include <carbon/utils/Timer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void UpdateHeadMeshSkinningWeightsFromBody(const SparseMatrix<T>& skinningWeightsHead, const SparseMatrix<T>& skinningWeightsCombinedBody,
    const SkinningWeightUpdateParams<T>& skinningWeightUpdateParams, Eigen::Matrix<T, -1, -1>& updatedHeadSkinningWeightsDense,
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool)
{
    //Timer timer;

    // convert to dense matrix
    updatedHeadSkinningWeightsDense = Eigen::Matrix<T, -1, -1>(skinningWeightsHead);

    // iterate over each vertex, only considering those where the combined body weight > 0
    auto updateSkinningWeightsForVertices = [&](int start, int end)
    {
        for (int nzWeightInd = start; nzWeightInd < end; ++nzWeightInd)
        {
            const auto [v, weight] = skinningWeightUpdateParams.headVertexSkinningWeightsMask->NonzeroVerticesAndWeights()[nzWeightInd];

            //LOG_INFO("Processing vertex {}", v);
            Eigen::Matrix<T, 1, -1> curWeightsFromBody = Eigen::Matrix<T, 1, -1>::Zero(updatedHeadSkinningWeightsDense.cols());

            // do each joint at a time
            for (typename SparseMatrix<T>::InnerIterator it(skinningWeightsCombinedBody, v); it; ++it)
            {
                if (std::fabs(it.value()) > 0)
                {
                    size_t j = size_t(it.col());
                    std::vector<int> jointsToRedistributeTo;

                    auto [faceJointIndex, isFaceJointIndex] = skinningWeightUpdateParams.bodyFaceJointMapping[j];
                    if (!isFaceJointIndex)
                    {
                        LOG_WARNING("Combined body joint {} for vertex {} has no matching joint in the face", j, v);
                        continue;
                    }

                    // if non zero in the head, no need to redistribute; just copy it
                    if (std::fabs(updatedHeadSkinningWeightsDense(v, faceJointIndex)) > std::numeric_limits<T>::epsilon())
                    {
                        jointsToRedistributeTo.push_back(faceJointIndex);
                    }
                    else
                    {
                        // go through the any descendant joint(s) already has a weight in the head, add to list of joints to redistribute to
                        bool bSplitAmongstChildren = false;
                        if (skinningWeightUpdateParams.faceRigChildrenMap.find(faceJointIndex) != skinningWeightUpdateParams.faceRigChildrenMap.end())
                        {
                            const auto& children = skinningWeightUpdateParams.faceRigChildrenMap.at(faceJointIndex);
                            for (const auto& childJoint : children)
                            {
                                if (std::fabs(updatedHeadSkinningWeightsDense(v, childJoint) > std::numeric_limits<T>::min()))
                                {
                                    jointsToRedistributeTo.push_back(childJoint);
                                    bSplitAmongstChildren = true;
                                }
                            }
                        }

                        // if either we are at a leaf node or no descendants, just split amongst any non-zero joints in the head for this vertex
                        if (!bSplitAmongstChildren)
                        {
                            for (typename SparseMatrix<T>::InnerIterator it2(skinningWeightsHead, v); it2; ++it2)
                            {
                                if (std::fabs(it2.value()) > std::numeric_limits<T>::epsilon())
                                {
                                    jointsToRedistributeTo.push_back(int(it2.col()));
                                }
                            }
                        }
                    }

                    T splitValue = it.value() / jointsToRedistributeTo.size();
                    for (const auto& joint : jointsToRedistributeTo)
                    {
                        curWeightsFromBody(joint) += splitValue;
                    }
                }
            }

            // add the weighted sum together
            for (int j = 0; j < updatedHeadSkinningWeightsDense.cols(); ++j)
            {
                updatedHeadSkinningWeightsDense(v, j) = (T(1.0f) - weight) * updatedHeadSkinningWeightsDense(v, j) + weight * curWeightsFromBody(j);
            }

            // renormalize the weights; shouldn't need pruning as we are only using weights present in the head already
            T sum = 0;
            for (int j = 0; j < updatedHeadSkinningWeightsDense.cols(); ++j)
            {
                sum += updatedHeadSkinningWeightsDense(v, j);
            }
            if (std::fabs(sum) > std::numeric_limits<T>::min())
            {
                for (int j = 0; j < updatedHeadSkinningWeightsDense.cols(); j++)
                {
                    updatedHeadSkinningWeightsDense(v, j) /= sum;
                }
            }
        }
    };

    taskThreadPool->AddTaskRangeAndWait(int(skinningWeightUpdateParams.headVertexSkinningWeightsMask->NonzeroVerticesAndWeights().size()), updateSkinningWeightsForVertices);

    // finally snap the neck seam vertices to match exactly
    skinningweightutils::SnapNeckSeamSkinningWeightsToBodySkinningWeights<T>(skinningWeightsCombinedBody, skinningWeightUpdateParams.faceBodyJointMapping, skinningWeightUpdateParams.neckBodySnapConfig, 
        updatedHeadSkinningWeightsDense);


    //LOG_INFO("time to update skinning weights {}", timer.Current());
}


template void UpdateHeadMeshSkinningWeightsFromBody(const SparseMatrix<float>& skinningWeightsHead, const SparseMatrix<float>& skinningWeightsCombinedBody,
    const SkinningWeightUpdateParams<float>& skinningWeightUpdateParams, Eigen::Matrix<float, -1, -1>& updatedHeadSkinningWeightsDense,
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool);

template void UpdateHeadMeshSkinningWeightsFromBody(const SparseMatrix<double>& skinningWeightsHead, const SparseMatrix<double>& skinningWeightsCombinedBody,
    const SkinningWeightUpdateParams<double>& skinningWeightUpdateParams, Eigen::Matrix<double, -1, -1>& updatedHeadSkinningWeightsDense,
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool);

template<class T>
void UpdateHeadMeshSkinningWeightsFromBody(const RigGeometry<T>& headRigGeometry, const std::vector<SparseMatrix<T>>& combinedBodySkinningWeights, const std::map<std::string, std::pair<int, SnapConfig<T>>>& neckBodySnapConfig,
    const std::vector<std::shared_ptr<VertexWeights<T>>>& headVertexSkinningWeightsMasks, const std::vector<std::pair<int, bool>>& bodyFaceJointMapping, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<T>>>>& barycentricCoordinatesForOddLods, std::vector<Eigen::Matrix<T, -1, -1>>& updatedHeadSkinningWeights, std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool)
{
    if (int(combinedBodySkinningWeights.size()) != headRigGeometry.NumLODs() / 2)
    {
        CARBON_CRITICAL("We are expecting half as many combined body rig skinning weight matrices as lods in the head rig");
    }

    for (int lod = 0; lod < headRigGeometry.NumLODs(); ++lod)
    {
        const int headMeshIndex = headRigGeometry.HeadMeshIndex(lod);
        if (lod % 2 == 1)
        {
            if (barycentricCoordinatesForOddLods.find(lod) == barycentricCoordinatesForOddLods.end())
            {
                CARBON_CRITICAL("barycentricCoordinatesForOddLods does not contain values for lod {}", lod);
            }

            if (int(barycentricCoordinatesForOddLods.at(lod).size()) != headRigGeometry.GetMesh(headMeshIndex).NumVertices())
            {
                CARBON_CRITICAL("barycentricCoordinatesForOddLods does not contain the right number of bc coords for lod {}", lod);
            }
        }
    }

    Timer timer;

    // generate the map of children for each joint, only including FACIAL joints (so we don't propagate weights up the spine/neck)
    auto headRigChildrenMap = skinningweightutils::GetJointChildrenRecursive(headRigGeometry.GetJointRig(), "FACIAL");
    updatedHeadSkinningWeights.resize(size_t(headRigGeometry.NumLODs()));

    for (int lod = 0; lod < headRigGeometry.NumLODs(); ++lod)
    {
        const int headMeshIndex = headRigGeometry.HeadMeshIndex(lod);
        const int combinedBodyMeshIndex = lod / 2;

        const SparseMatrix<T>& skinningWeightsHead = headRigGeometry.GetJointRig().GetSkinningWeights(headRigGeometry.GetMeshName(headMeshIndex));
        const SparseMatrix<T>& skinningWeightsCombinedBody = combinedBodySkinningWeights[size_t(combinedBodyMeshIndex)];
        SparseMatrix<T> skinningWeightsCombinedBodyOddLodHeadVerticesOnly;

        if (lod % 2 == 1)
        {
            // for odd lods, the combined body mesh vertices (and skinning weights) do not match those in the head, so we must remap the skinning weights from the lod below
            // this is pretty simple as we will assume we are using the same joints; just use a weighted sum of the lod below based upon barycentric coords of closest points
            skinningWeightsCombinedBodyOddLodHeadVerticesOnly = SparseMatrix<T>(headRigGeometry.GetMesh(headMeshIndex).NumVertices(), skinningWeightsCombinedBody.cols());

            std::vector<Eigen::Triplet<T>> triplets;

            Eigen::Matrix<T, -1, -1> skinningWeightsCombinedBodyDense = Eigen::Matrix<T, -1, -1>(skinningWeightsCombinedBody);

            for (int vID = 0; vID < headRigGeometry.GetMesh(headMeshIndex).Vertices().cols(); ++vID)
            {
                if (barycentricCoordinatesForOddLods.at(lod)[size_t(vID)].first)
                {
                    const auto& bc = barycentricCoordinatesForOddLods.at(lod)[size_t(vID)].second;
                    // do a weighted sum of the combined body weights
                    Eigen::Vector<T, -1> weights = skinningWeightsCombinedBodyDense.row(bc.Index(0)) * bc.Weight(0)
                        + skinningWeightsCombinedBodyDense.row(bc.Index(1)) * bc.Weight(1)
                        + skinningWeightsCombinedBodyDense.row(bc.Index(2)) * bc.Weight(2);

                    // set non-zero values in the output skinning weights
                    for (int j = 0; j < weights.size(); ++j)
                    {
                        if (std::fabs(weights(j)) > std::numeric_limits<T>::min())
                        {
                            triplets.emplace_back(Eigen::Triplet<T>(int(vID), j, weights(j)));
                        }
                    }
                }
                else
                {
                    LOG_WARNING("Failed to find closest point for vertex {}, lod {}", vID, lod);
                }
            }

            skinningWeightsCombinedBodyOddLodHeadVerticesOnly.setFromTriplets(triplets.begin(), triplets.end());
        }

        Eigen::Matrix<T, -1, -1> updatedHeadSkinningWeightsDense;

        std::string headMeshName = headRigGeometry.GetMeshName(headMeshIndex);
        auto it = neckBodySnapConfig.find(headMeshName);
        if (it == neckBodySnapConfig.end())
        {
            CARBON_CRITICAL("failed to find head neck snap config for mesh {}", headMeshName);
        }

        
		auto snapConfig = it->second.second;
        if (lod % 2 == 1)
        {
            // we have a special case for the odd numbered lods. The snap config has been calculated correctly to take acount of odd lods, but
            // because we are using weights calculated for barycentric coords each head mesh vertex, we don't need the 'proper' mapping here and
            // the sourceVertexIndices are actually head mesh indices.
            snapConfig.sourceVertexIndices = snapConfig.targetVertexIndices;
        }

        SkinningWeightUpdateParams<T> params { snapConfig, headVertexSkinningWeightsMasks[size_t(lod)], bodyFaceJointMapping, faceBodyJointMapping, headRigChildrenMap };

        if (lod % 2 == 1)
        {
            UpdateHeadMeshSkinningWeightsFromBody<T>(skinningWeightsHead, skinningWeightsCombinedBodyOddLodHeadVerticesOnly, params, updatedHeadSkinningWeightsDense, taskThreadPool);
        }
        else
        {
            UpdateHeadMeshSkinningWeightsFromBody<T>(skinningWeightsHead, skinningWeightsCombinedBody, params, updatedHeadSkinningWeightsDense, taskThreadPool);
        }

        updatedHeadSkinningWeights[size_t(lod)] = updatedHeadSkinningWeightsDense;
    }

    LOG_INFO("time to update skinning weights for all lods {}", timer.Current());
}

template void UpdateHeadMeshSkinningWeightsFromBody(const RigGeometry<float>& headRigGeometry, const std::vector<SparseMatrix<float>>& combinedBodySkinningWeights, const std::map<std::string, std::pair<int, SnapConfig<float>>>& neckBodySnapConfig,
    const std::vector<std::shared_ptr<VertexWeights<float>>>& headVertexSkinningWeightsMasks, const std::vector<std::pair<int, bool>>& bodyFaceJointMapping, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<float>>>>& barycentricCoordinatesForOddLods, std::vector<Eigen::Matrix<float, -1, -1>>& updatedHeadSkinningWeights, std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool);

template void UpdateHeadMeshSkinningWeightsFromBody(const RigGeometry<double>& headRigGeometry, const std::vector<SparseMatrix<double>>& combinedBodySkinningWeights, const std::map<std::string, std::pair<int, SnapConfig<double>>>& neckBodySnapConfig,
    const std::vector<std::shared_ptr<VertexWeights<double>>>& headVertexSkinningWeightsMasks, const std::vector<std::pair<int, bool>>& bodyFaceJointMapping, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<double>>>>& barycentricCoordinatesForOddLods, std::vector<Eigen::Matrix<double, -1, -1>>& updatedHeadSkinningWeights, std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)


