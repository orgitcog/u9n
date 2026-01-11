// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/JointRig2.h>

#include <carbon/Algorithm.h>
#include <carbon/utils/StringUtils.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void JointRig2<T>::Clear()
{
    m_jointNames.clear();
    m_jointParentIndices.clear();
    m_skinningWeights.clear();
}

template <class T>
void JointRig2<T>::AddJoint(const std::string& newJointName)
{
    for (const std::string& jointName : m_jointNames)
    {
        if (jointName == newJointName)
        {
            CARBON_CRITICAL("joint with name \"{}\" already exists", newJointName);
        }
    }
    m_jointNames.push_back(newJointName);
    m_jointParentIndices.push_back(-1);
}

template <class T>
int JointRig2<T>::GetJointIndex(const std::string& jointName) const
{
    for (int i = 0; i < int(m_jointNames.size()); ++i)
    {
        if (jointName == m_jointNames[i])
        {
            return i;
        }
    }
    return -1;
}

template <class T>
void JointRig2<T>::AttachJointToParent(const std::string& childJointName, const std::string& parentJointName)
{
    const int childIndex = GetJointIndex(childJointName);
    const int parentIndex = GetJointIndex(parentJointName);
    if (childIndex < 0)
    {
        CARBON_CRITICAL("joint \"{}\" does not exist", childJointName);
    }
    if (parentIndex < 0)
    {
        CARBON_CRITICAL("joint \"{}\" does not exist", parentJointName);
    }
    if (m_jointParentIndices[childIndex] >= 0)
    {
        CARBON_CRITICAL("joint \"{}\" already has a parent", childJointName);
    }
    if (IsChildOf(parentJointName, childJointName))
    {
        CARBON_CRITICAL("joint \"{}\" is a child of \"{}\"", parentJointName, childJointName);
    }
    if (childIndex == parentIndex)
    {
        CARBON_CRITICAL("cannot parent joint to itself");
    }
    if (childIndex < parentIndex)
    {
        CARBON_CRITICAL("cannot have child with lower index compared to parent");
    }
    m_jointParentIndices[childIndex] = parentIndex;
}

template <class T>
bool JointRig2<T>::IsChildOf(const std::string& jointName, const std::string& parentJointName) const
{
    const int jointIndex = GetJointIndex(jointName);
    const int parentIndex = GetJointIndex(parentJointName);
    if (jointIndex < 0)
    {
        CARBON_CRITICAL("joint \"{}\" does not exist", jointName);
    }
    if (parentIndex < 0)
    {
        CARBON_CRITICAL("joint \"{}\" does not exist", parentJointName);
    }
    int currentIndex = jointIndex;
    while (m_jointParentIndices[currentIndex] >= 0)
    {
        if (m_jointParentIndices[currentIndex] == parentIndex)
        {
            return true;
        }
        currentIndex = m_jointParentIndices[currentIndex];
    }
    return false;
}

template <class T>
int JointRig2<T>::HierarchyLevel(const std::string& jointName) const
{
    const int jointIndex = GetJointIndex(jointName);
    if (jointIndex < 0)
    {
        CARBON_CRITICAL("joint \"{}\" does not exist", jointName);
    }
    return HierarchyLevel(jointIndex);
}

template <class T>
int JointRig2<T>::HierarchyLevel(int jointIndex) const
{
    if ((jointIndex < 0) || (jointIndex >= NumJoints()))
    {
        CARBON_CRITICAL("invalid joint index {}", jointIndex);
    }
    int level = 0;
    int currentIndex = jointIndex;
    while (m_jointParentIndices[currentIndex] >= 0)
    {
        level++;
        currentIndex = m_jointParentIndices[currentIndex];
    }
    return level;
}

template <class T>
std::vector<std::vector<int>> JointRig2<T>::GetJointsPerHierarchyLevel() const
{
    int maxLevel = 0;
    std::vector<std::vector<int>> jointsPerHierarchyLevel(NumJoints());
    for (int jointIndex = 0; jointIndex < NumJoints(); ++jointIndex)
    {
        const int hierarchyLevel = HierarchyLevel(jointIndex);
        jointsPerHierarchyLevel[hierarchyLevel].push_back(jointIndex);
        maxLevel = std::max<int>(hierarchyLevel, maxLevel);
    }
    jointsPerHierarchyLevel.resize(maxLevel + 1);
    return jointsPerHierarchyLevel;
}

template <class T>
void JointRig2<T>::SetSkinningWeights(const std::string& geometryName,
                                      const std::map<std::string, InfluenceWeights<T>>& jointSkinningWeights,
                                      int numVertices,
                                      bool normalizeWeights,
                                      bool allowNegativeWeights,
                                      ErrorInfo* errorInfo)
{
    if (errorInfo)
    {
        errorInfo->unskinnedVertices.clear();
        errorInfo->unnormalizedVertices.clear();
        errorInfo->negativeWeightsVertices.clear();
    }

    if (m_skinningWeights.find(geometryName) != m_skinningWeights.end())
    {
        CARBON_CRITICAL("influence weights for geometry \"{}\" have already been defined", geometryName);
    }

    for (const auto& [jointName, _] : jointSkinningWeights)
    {
        if (GetJointIndex(jointName) < 0)
        {
            CARBON_CRITICAL("no joint {} in joint rig", jointName);
        }
    }

    int maxInfluenceVertex = std::max<int>(numVertices - 1, -1);
    if (numVertices <= 0)
    {
        for (auto&& [_, influenceWeights] : jointSkinningWeights)
        {
            maxInfluenceVertex = std::max<int>(maxInfluenceVertex, influenceWeights.indices.maxCoeff());
        }
    }

    // record if all vertices are influenced
    std::vector<T> totalVertexInfluence(maxInfluenceVertex + 1, 0);
    std::set<int> negativeWeightsVertices;
    for (const auto& [_, influenceWeights] : jointSkinningWeights)
    {
        for (int k = 0; k < influenceWeights.indices.size(); ++k)
        {
            if (influenceWeights.weights[k] < 0)
            {
                negativeWeightsVertices.insert(influenceWeights.indices[k]);
            }
            totalVertexInfluence[influenceWeights.indices[k]] += allowNegativeWeights ? influenceWeights.weights[k] : std::max(T(
                                                                                                                                   0),
                                                                                                                               influenceWeights.weights[
                                                                                                                                   k]);
        }
    }

    int numVerticesNotFullyInfluenced = 0;
    std::vector<int> verticesToConnectToRoot;
	const T totalVertInfluenceTolerance = T(2e-3);
    for (int vID = 0; vID < static_cast<int>(totalVertexInfluence.size()); ++vID)
    {
        if (totalVertexInfluence[vID] == T(0))
        {
            LOG_WARNING("Vertex {} is not influenced by any rig joint. Connecting vertex to the root.", vID);
            verticesToConnectToRoot.push_back(vID);
        }
		else if (fabs(totalVertexInfluence[vID] - T(1)) > totalVertInfluenceTolerance)
		{
            LOG_WARNING("Vertex {} is not fully influenced, total weight is {}.", vID, totalVertexInfluence[vID]);
            numVerticesNotFullyInfluenced++;
            if (errorInfo) errorInfo->unnormalizedVertices.push_back(vID);
        }
    }
    if (negativeWeightsVertices.size() > 0)
    {
        LOG_WARNING("Geometry \"{}\" has {} negative weights.", geometryName, negativeWeightsVertices.size());
    }
    if (numVerticesNotFullyInfluenced > 0)
    {
        LOG_WARNING("Geometry \"{}\" has {} out of {} vertices where the total weights of all joints do not add up to 1.0",
                    geometryName,
                    numVerticesNotFullyInfluenced,
                    numVertices);
    }
    if (verticesToConnectToRoot.size() > 0)
    {
        LOG_WARNING(
            "Geometry \"{}\" has {} vertices that are not influenced by any rig joint and are therefore connected to the root.",
            geometryName,
            verticesToConnectToRoot.size());
    }

    if (errorInfo)
    {
        errorInfo->negativeWeightsVertices = std::vector<int>(negativeWeightsVertices.begin(), negativeWeightsVertices.end());
        errorInfo->unskinnedVertices = verticesToConnectToRoot;
    }


    SparseMatrix<T> mat(NumJoints(), maxInfluenceVertex + 1);
    for (int jointIndex = 0; jointIndex < NumJoints(); jointIndex++)
    {
        mat.startVec(jointIndex);
        auto it = jointSkinningWeights.find(m_jointNames[jointIndex]);
        if (it != jointSkinningWeights.end())
        {
            for (int k = 0; k < int(it->second.indices.size()); k++)
            {
                const int vID = it->second.indices[k];
                T weight = it->second.weights[k];
                if (!allowNegativeWeights)
                {
                    weight = std::max(T(0), weight);
                }
                if (normalizeWeights && (totalVertexInfluence[vID] != 0))
                {
                    weight = weight / totalVertexInfluence[vID];
                }
                mat.insertBackByOuterInner(jointIndex, vID) = weight;
            }
        }
        if (m_jointParentIndices[jointIndex] < 0)
        {
            for (int vID : verticesToConnectToRoot)
            {
                mat.insertBackByOuterInner(jointIndex, vID) = T(1);
            }
        }
    }
    mat.finalize();
    // transpose does also an ordering
    m_skinningWeights[geometryName] = mat.transpose();
}

template <class T>
Eigen::VectorX<T> JointRig2<T>::GetSkinningWeights(const std::string& jointName, const std::string& geometryName, bool includingChildren) const
{
    if (m_skinningWeights.find(geometryName) == m_skinningWeights.end())
    {
        CARBON_CRITICAL("no geometry {} is influenced by the jointrig", geometryName);
    }

    const int jointIndex = GetJointIndex(jointName);
    if (jointIndex < 0)
    {
        CARBON_CRITICAL("no joint with name {}", jointName);
    }

    const SparseMatrix<T>& skinningWeights = m_skinningWeights.find(geometryName)->second;
    if (includingChildren)
    {
        Eigen::VectorX<T> weights = Eigen::VectorX<T>::Zero(skinningWeights.rows());
        for (int i = 0; i < NumJoints(); ++i) {
            if (i == jointIndex || IsChildOf(GetJointNames()[i], jointName)) {
                weights += skinningWeights.col(i);
            }
        }
        return weights;
    }
    else
    {
        return skinningWeights.col(jointIndex);
    }
}


template <class T>
InfluenceWeights<T> JointRig2<T>::GetSkinningWeightsAsStruct(const std::string& jointName, const std::string& geometryName) const
{
    if (m_skinningWeights.find(geometryName) == m_skinningWeights.end())
    {
        CARBON_CRITICAL("no geometry {} is influenced by the jointrig", geometryName);
    }

    const int jointIndex = GetJointIndex(jointName);
    if (jointIndex < 0)
    {
        CARBON_CRITICAL("no joint with name {}", jointName);
    }

    // assemble the weights for the joint
    std::vector<std::pair<int, T>> weights;
    const SparseMatrix<T>& skinningWeights = m_skinningWeights.find(geometryName)->second;
    for (int vID = 0; vID < int(skinningWeights.rows()); vID++)
    {
        for (typename SparseMatrix<T>::InnerIterator it(skinningWeights, vID); it; ++it)
        {
            if (jointIndex == it.col())
            {
                weights.push_back(std::pair<int, T>(vID, it.value()));
            }
        }
    }

    InfluenceWeights<T> influenceWeights;
    influenceWeights.indices.resize(weights.size());
    influenceWeights.weights.resize(weights.size());
    for (int k = 0; k < int(weights.size()); k++)
    {
        influenceWeights.indices[k] = weights[k].first;
        influenceWeights.weights[k] = weights[k].second;
    }

    return influenceWeights;
}

template <class T>
bool JointRig2<T>::HasSkinningWeights(const std::string& geometryName) const
{
    return (m_skinningWeights.find(geometryName) != m_skinningWeights.end());
}

template <class T>
void JointRig2<T>::RemoveSkinningWeights(const std::string& geometryName)
{
    auto it = m_skinningWeights.find(geometryName);
    if (it != m_skinningWeights.end())
    {
        m_skinningWeights.erase(it);
    }
}

template <class T>
std::vector<std::string> JointRig2<T>::GetGeometryNames() const
{
    std::vector<std::string> names;
    for (auto&& [name, _] : m_skinningWeights)
    {
        names.push_back(name);
    }
    return names;
}

template <class T>
void JointRig2<T>::CheckValidity() { CheckSingleRoot(); }

template <class T>
void JointRig2<T>::CheckSingleRoot()
{
    int numRootJoints = 0;
    for (int parentIndex : m_jointParentIndices)
    {
        if (parentIndex < 0)
        {
            numRootJoints++;
        }
    }
    if (numRootJoints != 1)
    {
        CARBON_CRITICAL("The rig containts {} root joints, but only a single one is supported", numRootJoints);
    }
}

template <class T>
const SparseMatrix<T>& JointRig2<T>::GetSkinningWeights(const std::string& geometryName) const
{
    auto it = m_skinningWeights.find(geometryName);
    if (it != m_skinningWeights.end())
    {
        return it->second;
    }
    CARBON_CRITICAL("no geometry skin cluster for geometry \"{}\"", geometryName);
}

template <class T>
void JointRig2<T>::SetSkinningWeights(const std::string& geometryName, const SparseMatrix<T>& smat)
{
    m_skinningWeights[geometryName] = smat;
}

template <class T>
void JointRig2<T>::Resample(const std::string& geometryName, const std::vector<int>& newToOldMap)
{
    auto it = m_skinningWeights.find(geometryName);
    if (it != m_skinningWeights.end())
    {
        const SparseMatrix<T>& oldInfluenceVertices = it->second;
        SparseMatrix<T> newInfluenceVertices(newToOldMap.size(), oldInfluenceVertices.cols());
        for (size_t i = 0; i < newToOldMap.size(); ++i)
        {
            newInfluenceVertices.row(i) = oldInfluenceVertices.row(newToOldMap[i]);
        }
        newInfluenceVertices.makeCompressed();
        it->second = newInfluenceVertices;
    }
}

template <class T>
std::vector<int> JointRig2<T>::RemoveUnusedJoints()
{
    // check which joints are influenced by any geometry
    Eigen::VectorX<T> totalInfluenceWeightsPerJoints = Eigen::VectorX<T>::Zero(NumJoints());
    for (const auto& [_, skinningWeights] : m_skinningWeights)
    {
        for (int vID = 0; vID < int(skinningWeights.rows()); vID++)
        {
            for (typename SparseMatrix<T>::InnerIterator it(skinningWeights, vID); it; ++it)
            {
                totalInfluenceWeightsPerJoints[it.col()] += it.value();
            }
        }
    }
    std::vector<bool> used(NumJoints(), false);
    for (int i = 0; i < NumJoints(); ++i)
    {
        used[i] = (totalInfluenceWeightsPerJoints[i] > 0);
        // LOG_VERBOSE("joint {} is {}", m_jointNames[i], used[i] ? "used" : "not used");
    }
    for (int i = 0; i < NumJoints(); ++i)
    {
        if (totalInfluenceWeightsPerJoints[i] > 0)
        {
            int currIdx = i;
            while (currIdx >= 0)
            {
                if (!used[currIdx])
                {
                    used[currIdx] = true;
                    // LOG_VERBOSE("joint {} is used as parent", m_jointNames[currIdx]);
                }
                currIdx = m_jointParentIndices[currIdx];
            }
        }
    }

    // remove all unused joints (update skinning weights)
    std::vector<int> oldToNew(NumJoints(), -1);
    std::vector<int> newToOld;
    for (int i = 0; i < NumJoints(); ++i)
    {
        if (used[i])
        {
            oldToNew[i] = int(newToOld.size());
            newToOld.push_back(i);
        }
    }
    const int numNewJoints = int(newToOld.size());
    LOG_VERBOSE("reducing rig from {} to {} joints", NumJoints(), numNewJoints);

    // update names
    std::vector<std::string> jointNames(numNewJoints);
    for (int i = 0; i < numNewJoints; ++i)
    {
        jointNames[i] = m_jointNames[newToOld[i]];
    }
    m_jointNames = jointNames;

    // update parents
    std::vector<int> jointParentIndices(numNewJoints, -1);
    for (int i = 0; i < numNewJoints; ++i)
    {
        const int oldIndex = newToOld[i];
        const int oldParentIndex = m_jointParentIndices[oldIndex];
        if (oldParentIndex >= 0)
        {
            const int newParentIndex = oldToNew[oldParentIndex];
            jointParentIndices[i] = newParentIndex;
        }
    }
    m_jointParentIndices = jointParentIndices;

    // update skinning weights
    for (auto& [_, skinningWeights] : m_skinningWeights)
    {
        std::vector<Eigen::Triplet<T>> triplets;
        for (int vID = 0; vID < int(skinningWeights.rows()); vID++)
        {
            for (typename SparseMatrix<T>::InnerIterator it(skinningWeights, vID); it; ++it)
            {
                const int newIdx = oldToNew[int(it.col())];
                if (newIdx >= 0)
                {
                    triplets.push_back(Eigen::Triplet<T>(vID, newIdx, it.value()));
                }
            }
        }
        skinningWeights.resize(skinningWeights.rows(), numNewJoints);
        skinningWeights.setFromTriplets(triplets.begin(), triplets.end());
    }

    return newToOld;
}

template <class T>
std::vector<int> JointRig2<T>::GetSymmetricJointIndices() const
{
    std::vector<int> symmetricJointIndices(NumJoints(), -1);
    for (int i = 0; i < NumJoints(); ++i)
    {
        const std::string& jointName = GetJointNames()[i];
        const std::vector<std::string> tokens = TITAN_NAMESPACE::Split(jointName, "_");
        std::vector<std::string> mirroredTokens;
        for (const std::string& token : tokens)
        {
            if (token == "R")
            {
                mirroredTokens.push_back("L");
            }
            else if (token == "r")
            {
                mirroredTokens.push_back("l");
            }
            else if (token == "L")
            {
                mirroredTokens.push_back("R");
            }
            else if (token == "l")
            {
                mirroredTokens.push_back("r");
            }
            else
            {
                mirroredTokens.push_back(token);
            }
        }
        std::string mirrorName;
        for (size_t k = 0; k < mirroredTokens.size(); ++k)
        {
            if (k > 0)
            {
                mirrorName.append("_");
            }
            mirrorName.append(mirroredTokens[k]);
        }
        if (mirrorName != jointName)
        {
            const int mirrorIndex = TITAN_NAMESPACE::GetItemIndex<std::string>(GetJointNames(), mirrorName);
            if (mirrorIndex >= 0)
            {
                symmetricJointIndices[i] = mirrorIndex;
            }
            else
            {
                CARBON_CRITICAL("no symmetry for {} (searched {})", jointName, mirrorName);
            }
        }
        else
        {
            symmetricJointIndices[i] = (int)i;
        }
    }
    return symmetricJointIndices;
}

// explicitly instantiate the JointRig2 classes
template class JointRig2<float>;
template class JointRig2<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
