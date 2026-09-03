// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/CombinedBodyJointLodMapping.h>
#include <rig/SkinningWeightUtils.h>
#include <rig/BodyGeometry.h>
#include <numeric>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool CombinedBodyJointLodMapping<T>::ReadJson(const JsonElement& jointPropagationMapJson)
{
    m_combinedBodyJointLodMapping.clear();

    if (!jointPropagationMapJson.Contains("JointPropagationMap") || !jointPropagationMapJson["JointPropagationMap"].IsArray())
    {
        LOG_ERROR("CombinedBodyJointLodMapping does not contain field JointPropagationMap containing an array");
        return false;
    }

    const std::vector<JsonElement>& arr = jointPropagationMapJson["JointPropagationMap"].Array();
    m_combinedBodyJointLodMapping.resize(arr.size());
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (!arr[i].IsObject())
        {
            LOG_ERROR("Each element in the JointPropagationMap must be an object");
            return false;
        }

        const auto& jointNameMappingJson = arr[i].Object();
        std::map<std::string, std::map<std::string, T>> curLodMap;
        for (const auto& mapping : jointNameMappingJson)
        {
            std::string curJointName = mapping.first;
            std::map<std::string, T> curJointMap;
            for (const auto& curMapPair : mapping.second.Object())
            {
                curJointMap[curMapPair.first] = curMapPair.second.Get<T>();
            }

            curLodMap[curJointName] = curJointMap;
        }
        m_combinedBodyJointLodMapping[i] = curLodMap;
    }

    // add parameters
    if (!jointPropagationMapJson.Contains("JointsToIncludeSiblingsInPropagation") || !jointPropagationMapJson["JointsToIncludeSiblingsInPropagation"].IsArray())
    {
        LOG_ERROR("CombinedBodyJointLodMapping does not contain field JointsToIncludeSiblingsInPropagationcontaining an array");
        return false;
    }
    m_jointsToIncludeSiblingsInPropagation = jointPropagationMapJson["JointsToIncludeSiblingsInPropagation"].Get<std::vector<std::string>>();

    if (!jointPropagationMapJson.Contains("ParentWeightForSiblingPropagation") || !jointPropagationMapJson["ParentWeightForSiblingPropagation"].IsDouble())
    {
        LOG_ERROR("CombinedBodyJointLodMapping does not contain field ParentWeightForSiblingPropagation containing a double");
        return false;
    }
    m_parentWeightForSiblingPropagation = jointPropagationMapJson["ParentWeightForSiblingPropagation"].Get<T>();

    if (!jointPropagationMapJson.Contains("UseDistanceWeightingForSiblingPropagation"))
    {
        LOG_ERROR("CombinedBodyJointLodMapping does not contain field UseDistanceWeightingForSiblingPropagation");
        return false;
    }
    m_bUseDistanceWeightingForSiblingPropagation = jointPropagationMapJson["UseDistanceWeightingForSiblingPropagation"].IsTrue();

    return true;
}

template<class T>
JsonElement CombinedBodyJointLodMapping<T>::ToJson() const
{
    // add the map
    JsonElement jointPropagationMapJson(TITAN_NAMESPACE::JsonElement::JsonType::Object);
    JsonElement arr(JsonElement::JsonType::Array);
    for (size_t lod = 0; lod < m_combinedBodyJointLodMapping.size(); ++lod)
    {
        JsonElement jointNameMappingJson(JsonElement::JsonType::Object);
        for (const auto& mapping : m_combinedBodyJointLodMapping[lod])
        {
            JsonElement curMapJson(JsonElement::JsonType::Object);
            std::string curJointName = mapping.first;
            for (const auto& curMapping : mapping.second)
            {
                curMapJson.Insert(curMapping.first, JsonElement(curMapping.second));
            }
            jointNameMappingJson.Insert(curJointName, std::move(curMapJson));
        }
        arr.Append(std::move(jointNameMappingJson));
    }
    jointPropagationMapJson.Insert("JointPropagationMap", std::move(arr));

    // add the parameters
    JsonElement sibJointNameArr(JsonElement::JsonType::Array);
    for (const auto& jointName : m_jointsToIncludeSiblingsInPropagation)
    {
        sibJointNameArr.Append(JsonElement(jointName));
    }

    jointPropagationMapJson.Insert("JointsToIncludeSiblingsInPropagation", std::move(sibJointNameArr));
    jointPropagationMapJson.Insert("ParentWeightForSiblingPropagation", JsonElement(m_parentWeightForSiblingPropagation));
    jointPropagationMapJson.Insert("UseDistanceWeightingForSiblingPropagation", JsonElement(m_bUseDistanceWeightingForSiblingPropagation));

    return jointPropagationMapJson;
}


template <class T>
bool CombinedBodyJointLodMapping<T>::UseDistanceWeightingForSiblingPropagation() const
{
    return m_bUseDistanceWeightingForSiblingPropagation;
}

template <class T>
void CombinedBodyJointLodMapping<T>::SetUseDistanceWeightingForSiblingPropagation(bool bUseDistanceWeighting)
{
    m_bUseDistanceWeightingForSiblingPropagation = bUseDistanceWeighting;
}

template <class T>
T CombinedBodyJointLodMapping<T>::ParentWeightForSiblingPropagation() const
{
    return m_parentWeightForSiblingPropagation;
}

template <class T>
void CombinedBodyJointLodMapping<T>::SetParentWeightForSiblingPropagation(const T& parentWeighting)
{
    m_parentWeightForSiblingPropagation = parentWeighting;
}

template <class T>
const std::vector<std::string>& CombinedBodyJointLodMapping<T>::JointsToIncludeSiblingsInPropagation() const
{
    return m_jointsToIncludeSiblingsInPropagation;
}

template <class T>
void CombinedBodyJointLodMapping<T>::SetJointsToIncludeSiblingsInPropagation(const std::vector<std::string>& jointsToIncludeInSiblingPropagation)
{
    m_jointsToIncludeSiblingsInPropagation = jointsToIncludeInSiblingPropagation;
}

template <class T>
const std::vector<std::map<std::string, std::map<std::string, T>>>& CombinedBodyJointLodMapping<T>::GetJointMapping() const
{
    return m_combinedBodyJointLodMapping;
}


template <class T>
void CombinedBodyJointLodMapping<T>::CalculateMapping(const RigGeometry<T>& rigGeometry)
{
    m_combinedBodyJointLodMapping.resize(size_t(rigGeometry.NumLODs() - 1));

    // which joints are in use at each lod?
    std::vector<std::vector<bool>> isJointUsedInLod(rigGeometry.NumLODs(), std::vector<bool>(rigGeometry.GetJointRig().NumJoints(), false));
    for (size_t lod = 0; lod < rigGeometry.NumLODs(); ++lod)
    {
        const std::string meshName = rigGeometry.GetMeshName(int(lod));
        const SparseMatrix<T>& skinningWeights = rigGeometry.GetJointRig().GetSkinningWeights(meshName);

        for (int v = 0; v < skinningWeights.rows(); ++v)
        {
            for (typename SparseMatrix<T>::InnerIterator it(skinningWeights, v); it; ++it)
            {
                isJointUsedInLod[lod][size_t(it.col())] = true;
            }
        }
    }

    // check that m_jointsToIncludeSiblingsInPropagation contains valid joints
    for (const auto & jointName : m_jointsToIncludeSiblingsInPropagation)
    {
        if (rigGeometry.GetJointRig().GetJointIndex(jointName) == -1)
        {
            CARBON_CRITICAL("Supplied rigGeometry does not contain joint {} which is has been set in the list of joints to include for sibling propagation", jointName);
        }
    }


    // for each joint used in Lod0, go up the hierarchy until we hit a parent still in use
    std::map<int, std::vector<int>> allDescendants = skinningweightutils::GetJointChildrenRecursive<T>(rigGeometry.GetJointRig());

    for (size_t lod = 1; lod < rigGeometry.NumLODs(); ++lod)
    {
        std::map<std::string, std::map<std::string, T>> jointPropagationMap;

        for (int j = 0; j < rigGeometry.GetJointRig().NumJoints(); ++j)
        {
            if (isJointUsedInLod[0][size_t(j)])
            {
                bool bFoundParentInUse = false;
                bool bNoMoreParents = false;
                int curJoint = j;
                std::vector<int> candidateJoints;
                bool bFoundCurJoint = false;

                if (isJointUsedInLod[lod][size_t(curJoint)])
                {
                    bFoundCurJoint = true;
                    candidateJoints = {};
                }
                else
                {
                    // traverse up the hierarchy until we find the first non-zero parent in the current lod
                    int parentJoint = -1;
                    while (!bFoundParentInUse && !bNoMoreParents)
                    {
                        parentJoint = rigGeometry.GetJointRig().GetParentIndex(curJoint);

                        if (parentJoint == -1)
                        {
                            bNoMoreParents = true;
                        }
                        else
                        {
                            if (isJointUsedInLod[lod][size_t(parentJoint)])
                            {
                                bFoundParentInUse = true;
                                candidateJoints = { parentJoint };

                                // any sibling propagation to consider?
                                if (std::find(m_jointsToIncludeSiblingsInPropagation.begin(), m_jointsToIncludeSiblingsInPropagation.end(), rigGeometry.GetJointRig().GetJointNames()[size_t(parentJoint)]) != m_jointsToIncludeSiblingsInPropagation.end())
                                {
                                    auto curDescendants = allDescendants.at(parentJoint);

                                    for (const auto& descendant : curDescendants)
                                    {
                                        if (descendant != j && isJointUsedInLod[lod][size_t(descendant)])
                                        {
                                            candidateJoints.push_back(descendant);
                                        }
                                    }
                                }
                            }
                        }

                        curJoint = parentJoint;
                    }
                }
                const std::string curJointName = rigGeometry.GetJointRig().GetJointNames()[size_t(j)];
                std::map<std::string, T> curMap;

                // now weight the candidate joints by their inverse distance from the original joint and normalize
                if (bFoundCurJoint)
                {
                    // the joint is present in the current lod
                    curMap[curJointName] = T(1.0);
                }
                else
                {
                    // first the closest child candidate joint
                    Eigen::Matrix<T, 3, 1> origJointTrans = rigGeometry.GetBindMatrix(j).template block<3, 1>(0, 3);
                    Eigen::Matrix<T, 3, 1> parentJointTrans = rigGeometry.GetBindMatrix(candidateJoints[0]).template block<3, 1>(0, 3);
                    T parentDist = (parentJointTrans - origJointTrans).norm();
                    T total = 0;
                    if (parentDist == T(0))
                    {
                        curMap[rigGeometry.GetJointRig().GetJointNames()[size_t(candidateJoints[0])]] = m_parentWeightForSiblingPropagation;
                        total = m_parentWeightForSiblingPropagation;
                    }
                    else
                    {
                        curMap[rigGeometry.GetJointRig().GetJointNames()[size_t(candidateJoints[0])]] = m_parentWeightForSiblingPropagation / parentDist;
                        if (m_bUseDistanceWeightingForSiblingPropagation)
                        {
                            total = m_parentWeightForSiblingPropagation / parentDist;
                        }
                        else
                        {
                            total = m_parentWeightForSiblingPropagation;
                        }


                        if (candidateJoints.size() > 1)
                        {
                            // find the closest sibling candidate joint
                            int closestInd = -1;
                            T closestDist = 99999;

                            for (int candidateJointInd = 0; candidateJointInd < int(candidateJoints.size()); ++candidateJointInd)
                            {
                                Eigen::Matrix<T, 3, 1> curJointTrans = rigGeometry.GetBindMatrix(candidateJoints[candidateJointInd]).template block<3, 1>(0, 3);
                                T dist = (curJointTrans - origJointTrans).norm();
                                if (dist < closestDist)
                                {
                                    closestDist = dist;
                                    closestInd = candidateJointInd;
                                }
                            }

                            T curFactor = T(1.0) - m_parentWeightForSiblingPropagation;
                            T curWeight = T(curFactor);
                            if (m_bUseDistanceWeightingForSiblingPropagation)
                            {
                                curWeight /= closestDist;
                            }

                            if (closestInd != -1)
                            {
                                curMap[rigGeometry.GetJointRig().GetJointNames()[size_t(candidateJoints[size_t(closestInd)])]] = curWeight;
                                total += curWeight;
                            }
                        }
                    }


                    // normalize the current map
                    for (auto& el : curMap)
                    {
                        el.second /= total;
                    }
                }


                jointPropagationMap[curJointName] = curMap;
            }
        }

        m_combinedBodyJointLodMapping[size_t(lod) - 1] = jointPropagationMap;
    }
}


template class CombinedBodyJointLodMapping<float>;
template class CombinedBodyJointLodMapping<double>;


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
