// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/SkinningWeightUtils.h>
#include <rig/BodyGeometry.h>
#include <carbon/Algorithm.h>
#include <numeric>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::skinningweightutils)

template <class T>
void SnapNeckSeamSkinningWeightsToBodySkinningWeights(const SparseMatrix<T>& skinningWeightsCombinedBody, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const SnapConfig<T>& snapConfig, Eigen::Matrix<T, -1, -1>& updatedHeadSkinningWeightsDense)
{
    if (int(faceBodyJointMapping.size()) != updatedHeadSkinningWeightsDense.cols())
    {
        CARBON_CRITICAL("Face body joint mapping contains the incorrect number of joints");
    }

    Eigen::Matrix<T, -1, -1> skinningWeightsCombinedBodyDense = Eigen::Matrix<T, -1, -1>(skinningWeightsCombinedBody);

    for (size_t v = 0; v < snapConfig.sourceVertexIndices.size(); ++v)
    {
        std::set<int> nonZeroBodyJoints;

        for (typename SparseMatrix<T>::InnerIterator it(skinningWeightsCombinedBody, snapConfig.sourceVertexIndices[v]); it; ++it)
        {
            if (std::fabs(it.value()) > 0)
            {
                nonZeroBodyJoints.insert(int(it.col()));
            }
        }

        for (int j = 0; j < updatedHeadSkinningWeightsDense.cols(); ++j)
        {
            auto [bodyOrParentIndex, isBodyJointIndex] = faceBodyJointMapping[j];
            updatedHeadSkinningWeightsDense(snapConfig.targetVertexIndices[v], j) = 0;

            if (isBodyJointIndex)
            {
                // copy the weight from the body
                updatedHeadSkinningWeightsDense(snapConfig.targetVertexIndices[v], j) = skinningWeightsCombinedBodyDense(snapConfig.sourceVertexIndices[v], bodyOrParentIndex);
                nonZeroBodyJoints.erase(bodyOrParentIndex);
            }
        }

        if (!nonZeroBodyJoints.empty())
        {
            LOG_WARNING("Vertex {} on combined body neck seam is influenced by joints not present in the face rig", snapConfig.sourceVertexIndices[v]);
        }
    }
}


template void SnapNeckSeamSkinningWeightsToBodySkinningWeights(const SparseMatrix<float>& skinningWeightsCombinedBody, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const SnapConfig<float>& snapConfig, Eigen::Matrix<float, -1, -1>& updatedHeadSkinningWeightsDense);
template void SnapNeckSeamSkinningWeightsToBodySkinningWeights(const SparseMatrix<double>& skinningWeightsCombinedBody, const std::vector<std::pair<int, bool>>& faceBodyJointMapping,
    const SnapConfig<double>& snapConfig, Eigen::Matrix<double, -1, -1>& updatedHeadSkinningWeightsDense);


template <class T>
void GetAllDescendants(int parent, const std::vector<int>& parentIndices, const JointRig2<T>& jointRig, std::vector<int>& descendants, const std::string& mustContainStr = {})
{
    for (size_t i = 0; i < parentIndices.size(); ++i)
    {

        const std::string jointName = jointRig.GetJointNames()[i];

        if ((mustContainStr.empty() || jointName.find(mustContainStr) != std::string::npos) && parentIndices[i] == parent)
        {
            // Direct child
            descendants.push_back(int(i));
            GetAllDescendants(int(i), parentIndices, jointRig, descendants, mustContainStr); // Recurse for deeper descendants
        }
    }
}

template <class T>
std::map<int, std::vector<int>> GetJointChildrenRecursive(const JointRig2<T>& jointRig, const std::string& mustContainStr)
{
    std::map<int, std::vector<int>> children;
    std::vector<int> parentIndices(jointRig.NumJoints());

    for (int jointIndex = 0; jointIndex < jointRig.NumJoints(); ++jointIndex)
    {
        parentIndices[jointIndex] = jointRig.GetParentIndex(jointIndex);
    }

    for (int jointIndex = 0; jointIndex < jointRig.NumJoints(); ++jointIndex)
    {
        std::vector<int> descendants;
        GetAllDescendants(jointIndex, parentIndices, jointRig, descendants, mustContainStr);
        children[jointIndex] = descendants;
    }

    return children;
}


template std::map<int, std::vector<int>> GetJointChildrenRecursive(const JointRig2<float>& jointRig, const std::string& mustContainStr);
template std::map<int, std::vector<int>> GetJointChildrenRecursive(const JointRig2<double>& jointRig, const std::string& mustContainStr);

template<class T>
std::vector<std::pair<int, bool>> CalculateBodyFaceJointMapping(const JointRig2<T>& faceJointRig, const JointRig2<T>& bodyJointRig)
{
    std::vector<std::pair<int, bool>> bodyFaceJointMapping;

    // for each body joint find corresponding face joint
    for (int jointIndex = 0; jointIndex < bodyJointRig.NumJoints(); ++jointIndex)
    {
        const std::string jointName = bodyJointRig.GetJointNames()[jointIndex];
        const int faceJointIndex = GetItemIndex(faceJointRig.GetJointNames(), jointName);
        if (faceJointIndex >= 0)
        {
            bodyFaceJointMapping.push_back({ faceJointIndex, true });
        }
        else
        {
            bodyFaceJointMapping.push_back({ -1, false });
        }
    }

    return bodyFaceJointMapping;
}

template std::vector<std::pair<int, bool>> CalculateBodyFaceJointMapping(const JointRig2<float>& faceJointRig, const JointRig2<float>& bodyJointRig);
template std::vector<std::pair<int, bool>> CalculateBodyFaceJointMapping(const JointRig2<double>& faceJointRig, const JointRig2<double>& bodyJointRig);


template <class T>
std::vector<std::pair<int, bool>> CalculateFaceBodyJointMapping(const JointRig2<T>& faceJointRig, const JointRig2<T>& bodyJointRig)
{
    std::vector<std::pair<int, bool>> faceBodyJointMapping;

    // for each face joint find corresponding body joint
    for (int jointIndex = 0; jointIndex < faceJointRig.NumJoints(); ++jointIndex)
    {
        const std::string jointName = faceJointRig.GetJointNames()[jointIndex];
        const int bodyJointIndex = GetItemIndex(bodyJointRig.GetJointNames(), jointName);
        if (bodyJointIndex >= 0)
        {
            faceBodyJointMapping.push_back({ bodyJointIndex, true });
        }
        else
        {
            faceBodyJointMapping.push_back({ -1, false });
        }
    }

    // for joints that do not map to the body, find the parent joint that maps to the body
    for (int jointIndex = 0; jointIndex < faceJointRig.NumJoints(); ++jointIndex)
    {
        if (!faceBodyJointMapping[jointIndex].second)
        {
            int parentIndex = faceJointRig.GetParentIndex(jointIndex);
            while (parentIndex >= 0)
            {
                if (faceBodyJointMapping[parentIndex].second)
                {
                    faceBodyJointMapping[jointIndex].first = parentIndex;
                    break;
                }
                parentIndex = faceJointRig.GetParentIndex(parentIndex);
            }
            if (parentIndex < 0)
            {
                LOG_ERROR("face joint does not have a valid parent joint that has a mapping to the body");
            }
        }
    }

    return faceBodyJointMapping;
}

template std::vector<std::pair<int, bool>> CalculateFaceBodyJointMapping(const JointRig2<float>& faceJointRig, const JointRig2<float>& bodyJointRig);
template std::vector<std::pair<int, bool>> CalculateFaceBodyJointMapping(const JointRig2<double>& faceJointRig, const JointRig2<double>& bodyJointRig);


template <class T>
void CalculateSkinningWeightsForBarycentricCoordinates(const std::vector<BarycentricCoordinates<T>>& barycentricCoordinates, const Eigen::Matrix<T, 3, -1>& vertices,
    const SparseMatrix<T>& skinningWeights, SparseMatrix<T>& outputSkinningWeights)
{
    if (skinningWeights.rows() != vertices.cols())
    {
        CARBON_CRITICAL("Skinning weights matrix must contain the same number of rows as the number of columns in vertices");
    }

    outputSkinningWeights = SparseMatrix<T>(int(barycentricCoordinates.size()), skinningWeights.cols());
    
    // make a dense version of the input skinning weights
    Eigen::Matrix<T, -1, -1> denseSkinningWeights = Eigen::Matrix<T, -1, -1>(skinningWeights);
    std::vector<Eigen::Triplet<T>> triplets;

    for (size_t v = 0; v < barycentricCoordinates.size(); ++v)
    {
        if (barycentricCoordinates[v].Index(0) < 0 || barycentricCoordinates[v].Index(0) >= denseSkinningWeights.rows() 
            || barycentricCoordinates[v].Index(1) < 0 || barycentricCoordinates[v].Index(1) >= denseSkinningWeights.rows() 
            || barycentricCoordinates[v].Index(2) < 0 || barycentricCoordinates[v].Index(2) >= denseSkinningWeights.rows() )
        {
            CARBON_CRITICAL("barycentric coordinate index is out of range for supplied skinning weights");
        }
        Eigen::Vector<T, -1> weights = denseSkinningWeights.row(barycentricCoordinates[v].Index(0)) * barycentricCoordinates[v].Weight(0)
            + denseSkinningWeights.row(barycentricCoordinates[v].Index(1)) * barycentricCoordinates[v].Weight(1)
            + denseSkinningWeights.row(barycentricCoordinates[v].Index(2)) * barycentricCoordinates[v].Weight(2);

        // set non-zero values in the output skinning weights
        for (int j = 0; j < weights.size(); ++j)
        {
            if (fabs(weights(j)) > std::numeric_limits<T>::min())
            {
                triplets.emplace_back(Eigen::Triplet<T>(int(v), j, weights(j)));
            }
        }
    }

    outputSkinningWeights.setFromTriplets(triplets.begin(), triplets.end());
}

template void CalculateSkinningWeightsForBarycentricCoordinates(const std::vector<BarycentricCoordinates<float>>& barycentricCoordinates, const Eigen::Matrix<float, 3, -1>& vertices,
    const SparseMatrix<float>& skinningWeights, SparseMatrix<float>& outputSkinningWeights);
template void CalculateSkinningWeightsForBarycentricCoordinates(const std::vector<BarycentricCoordinates<double>>& barycentricCoordinates, const Eigen::Matrix<double, 3, -1>& vertices,
    const SparseMatrix<double>& skinningWeights, SparseMatrix<double>& outputSkinningWeights);

template <class R>
std::vector<std::vector<int>> CalculateAncestorsForAllJoints(const R & jointRig)
{
    std::vector<std::vector<int>> ancestors(jointRig.NumJoints());

    for (int j = 0; j < jointRig.NumJoints(); ++j)
    {
        bool bHasParent = true;
        int curJoint = j;
        while (bHasParent)
        {
            int parent = jointRig.GetParentIndex(curJoint);
            if (parent == -1)
            {
                bHasParent = false;
            }
            else
            {
                ancestors[j].push_back(parent);
                curJoint = parent;
            }
        }
    }

    return ancestors;
}

template std::vector<std::vector<int>> CalculateAncestorsForAllJoints(const JointRig2<float>& jointRig);
template std::vector<std::vector<int>> CalculateAncestorsForAllJoints(const JointRig2<double>& jointRig);
template std::vector<std::vector<int>> CalculateAncestorsForAllJoints(const BodyGeometry<float>& bodyGeometry);
template std::vector<std::vector<int>> CalculateAncestorsForAllJoints(const BodyGeometry<double>& bodyGeometry);


template <class T, class R>
void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<T>>& higherLodBarycentricCoordinates, const Eigen::Matrix<T, 3, -1>& lowerLodVertices, const SparseMatrix<T>& lowerLodSkinningWeights,
    const std::map<std::string, std::map<std::string, T>>& jointMappingFromLod0, const SnapConfig<T> & lowerLodSnapConfig, const R& jointRig, int maxNumWeightsForLod, SparseMatrix<T>& outputHighLodSkinningWeights)
 {
    if (lowerLodSkinningWeights.rows() != lowerLodVertices.cols())
    {
        CARBON_CRITICAL("Lower lod skinning weights matrix must contain the same number of rows as the number of columns in lowerLodvertices");
    }

    if (jointRig.NumJoints() != lowerLodSkinningWeights.cols())
    {
        CARBON_CRITICAL("jointRig must contain the same number of joints as the number of columns in lowerLodSkinningWeights");
    }
    
    for (const auto& map : jointMappingFromLod0)
    {
        if (jointRig.GetJointIndex(map.first) == -1)
        {
            LOG_ERROR("joint {} is not present in jointRig", map.first);
            for (const auto & mapInner : map.second)
            {
                if (jointRig.GetJointIndex(mapInner.first) == -1)
                {
                    CARBON_CRITICAL("joint {} is not present in jointRig", mapInner.first);
                }
            }
        }
    }

    SparseMatrix<T> initialOutputSkinningWeights;
    CalculateSkinningWeightsForBarycentricCoordinates(higherLodBarycentricCoordinates, lowerLodVertices, lowerLodSkinningWeights, initialOutputSkinningWeights);

    // convert the skinning weight matrices to dense matrices for faster access
    Eigen::Matrix<T, -1, -1> initialOutputSkinningWeightsDense(initialOutputSkinningWeights);

    outputHighLodSkinningWeights = SparseMatrix<T>(initialOutputSkinningWeights.rows(), initialOutputSkinningWeights.cols());
    for (int v = 0; v < initialOutputSkinningWeights.rows(); ++v)
    {
        for (typename SparseMatrix<T>::InnerIterator it(initialOutputSkinningWeights, v); it; ++it)
        {
            T curWeight = it.value();
            const std::string jointName = jointRig.GetJointNames()[size_t(it.col())];
            if (jointMappingFromLod0.contains(jointName))
            {
                for (const auto& mapPair : jointMappingFromLod0.at(jointName))
                {
                    int newJointIndex = jointRig.GetJointIndex(mapPair.first);
                    outputHighLodSkinningWeights.coeffRef(v, newJointIndex) = outputHighLodSkinningWeights.coeff(v, newJointIndex) + curWeight * mapPair.second;
                }
            }
            else
            {
                LOG_ERROR("Joint {} was not expected to have an influence in LOD0 for vertex {}", jointName, v);
            }
        }
    }

    // perform any 'snapping' for vertices which need to match skinning weights from lower Lod exactly
    // we perform the snap and then remap
    for (size_t vInd = 0; vInd < lowerLodSnapConfig.targetVertexIndices.size(); ++vInd)
    {
        int targetV = lowerLodSnapConfig.targetVertexIndices[vInd];
        int srcLowerLodV = lowerLodSnapConfig.sourceVertexIndices[vInd];
        // first set all weights to 0 for the target vertex as they will have been set already
        for (typename SparseMatrix<T>::InnerIterator it(outputHighLodSkinningWeights, targetV); it; ++it)
        {
            it.valueRef() = 0;
        }
        for (typename SparseMatrix<T>::InnerIterator it(lowerLodSkinningWeights, srcLowerLodV); it; ++it)
        {
            T curWeight = it.value();
            const std::string jointName = jointRig.GetJointNames()[size_t(it.col())];
            if (jointMappingFromLod0.contains(jointName))
            {
                for (const auto& mapPair : jointMappingFromLod0.at(jointName))
                {
                    int newJointIndex = jointRig.GetJointIndex(mapPair.first);
                    outputHighLodSkinningWeights.coeffRef(targetV, newJointIndex) = outputHighLodSkinningWeights.coeff(targetV, newJointIndex) + curWeight * mapPair.second;
                }
            }
            else
            {
                LOG_ERROR("Joint {} was not expected to have an influence in LOD0 for vertex {}", jointName, srcLowerLodV);
            }
        }
    }

    // finally, sort prune and renormalize the weights
    SortPruneAndRenormalizeSkinningWeights(outputHighLodSkinningWeights, maxNumWeightsForLod);
}

template void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<float>>& higherLodBarycentricCoordinates, const Eigen::Matrix<float, 3, -1>& lowerLodVertices,
    const SparseMatrix<float>& lowerLodSkinningWeights, const std::map<std::string, std::map<std::string, float>>& jointMappingFromLod0, const SnapConfig<float>& lowerLodSnapConfig,
    const JointRig2<float>& jointRig, int maxNumWeightsForLod, SparseMatrix<float>& outputHighLodSkinningWeights);
template void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<double>>& higherLodBarycentricCoordinates, const Eigen::Matrix<double, 3, -1>& lowerLodVertices,
    const SparseMatrix<double>& lowerLodSkinningWeights, const std::map<std::string, std::map<std::string, double>>& jointMappingFromLod0, const SnapConfig<double>& lowerLodSnapConfig,
    const JointRig2<double>& jointRig, int maxNumWeightsForLod, SparseMatrix<double>& outputHighLodSkinningWeights);
template void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<float>>& higherLodBarycentricCoordinates, const Eigen::Matrix<float, 3, -1>& lowerLodVertices,
    const SparseMatrix<float>& lowerLodSkinningWeights, const std::map<std::string, std::map<std::string, float>>& jointMappingFromLod0, const SnapConfig<float>& lowerLodSnapConfig,
    const BodyGeometry<float>& bodyGeometry, int maxNumWeightsForLod, SparseMatrix<float>& outputHighLodSkinningWeights);
template void PropagateSkinningWeightsToHigherLOD(const std::vector<BarycentricCoordinates<double>>& higherLodBarycentricCoordinates, const Eigen::Matrix<double, 3, -1>& lowerLodVertices,
    const SparseMatrix<double>& lowerLodSkinningWeights, const std::map<std::string, std::map<std::string, double>>& jointMappingFromLod0, const SnapConfig<double>& lowerLodSnapConfig,
    const BodyGeometry<double>& bodyGeometry, int maxNumWeightsForLod, SparseMatrix<double>& outputHighLodSkinningWeightsl);



template<class T>
void SortPruneAndRenormalizeSkinningWeights(SparseMatrix<T>& skin, int maxSkinWeights)
{
    for (int vID = 0; vID < skin.outerSize(); ++vID)
    {
        std::vector<typename SparseMatrix<T>::InnerIterator> its;
        for (typename SparseMatrix<T>::InnerIterator it(skin, vID); it; ++it)
        {
            if (it.value() > T(0))
            {
                its.push_back(it);
            } else 
            {
                it.valueRef()= T(0);
            }
        }
        // sort and prune down the weights
        if ((int)its.size() > maxSkinWeights)
        {
            std::vector<size_t> idx(its.size());
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(),
                [&its](size_t i, size_t j)
                { return its[i].value() > its[j].value(); });

            // set too big values to 0
            for (size_t j = maxSkinWeights; j < its.size(); j++)
            {
                skin.coeffRef(its[idx[j]].row(), its[idx[j]].col()) = T(0);
            }
            for (size_t j = 0; j < size_t(maxSkinWeights); j++)
            {
                skin.coeffRef(its[idx[j]].row(), its[idx[j]].col()) = its[idx[j]].value();
            }
        }

        // final renormalization (we do this at the end in case the weights are not normalized initially)
        T sum = T(0);
        for (size_t j = 0; j < its.size(); j++)
        {
            sum += its[j].value();
        }
        if (sum != T(0))
        {
            for (size_t j = 0; j < its.size(); j++)
            {
                skin.coeffRef(its[j].row(), its[j].col()) /= sum;
            }
        }
    }
}

template void SortPruneAndRenormalizeSkinningWeights(SparseMatrix<float>& skin, int maxSkinWeights);
template void SortPruneAndRenormalizeSkinningWeights(SparseMatrix<double>& skin, int maxSkinWeights);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::skinningweightutils)
