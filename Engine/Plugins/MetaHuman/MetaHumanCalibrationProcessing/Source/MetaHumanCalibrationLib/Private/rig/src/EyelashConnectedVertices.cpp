// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/EyelashConnectedVertices.h>
#include <carbon/geometry/KdTree.h>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool EyelashConnectedVertices<T>::InitializeEyelashMapping(const Mesh<T>& headMesh, const Mesh<T>& eyelashesMesh,
    const std::vector<std::pair<int, T>>& eyelashRoots,
    std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices)
{
    eyelashConnectedVertices = std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>(eyelashesMesh.NumVertices());
    const std::vector<std::pair<int, int>> headEdges = headMesh.GetEdges({});
    TITAN_NAMESPACE::KdTree<T> headKdTree(headMesh.Vertices().transpose());

    const std::vector<std::pair<int, int>> edges = eyelashesMesh.GetEdges({});
    std::vector<std::vector<int>> neighbors(eyelashesMesh.NumVertices());
    for (const auto& [vID0, vID1] : edges)
    {
        neighbors[vID0].push_back(vID1);
        neighbors[vID1].push_back(vID0);
    }
    std::set<int> toProcess;
    for (const auto& [vID, weight] : eyelashRoots)
    {
        if (vID >= eyelashesMesh.NumVertices())
        {
            return false;
        }
        if (weight > T(0.5))
        {
            eyelashConnectedVertices[vID] = std::make_shared<EyelashConnectedVertices<T>>();
            eyelashConnectedVertices[vID]->indices.push_back(vID);
            toProcess.insert(vID);
        }
    }
    while (toProcess.size() > 0)
    {
        const int vID = *toProcess.begin();
        toProcess.erase(toProcess.begin());
        for (int othervID : neighbors[vID])
        {
            // both nullptr or both pointing to the same, then do nothing
            if (eyelashConnectedVertices[vID] == eyelashConnectedVertices[othervID])
            {
                continue;
            }

            if (eyelashConnectedVertices[vID])
            {
                if (eyelashConnectedVertices[othervID])
                {
                    for (int k : eyelashConnectedVertices[othervID]->indices)
                    {
                        eyelashConnectedVertices[vID]->indices.push_back(k);
                    }
                    eyelashConnectedVertices[othervID] = eyelashConnectedVertices[vID];
                    toProcess.insert(othervID);
                }
                else
                {
                    eyelashConnectedVertices[othervID] = eyelashConnectedVertices[vID];
                    eyelashConnectedVertices[vID]->indices.push_back(othervID);
                    toProcess.insert(othervID);
                }
            }
            else if (eyelashConnectedVertices[othervID])
            {
                eyelashConnectedVertices[vID] = eyelashConnectedVertices[othervID];
                eyelashConnectedVertices[othervID]->indices.push_back(vID);
                toProcess.insert(vID);
            }
        }
    }
    for (int i = 0; i < eyelashesMesh.NumVertices(); ++i)
    {
        if (!eyelashConnectedVertices[i]->valid)
        {
            std::set<int> headvIDs;
            for (int vID : eyelashConnectedVertices[i]->indices)
            {
                // for each root vertex, find closest head vertex and also any edge-connected vertices to the closest head vertex
                const int headvID = (int)headKdTree.getClosestPoint(eyelashesMesh.Vertices().col(vID).transpose(), T(1e9)).first;
                headvIDs.insert(headvID);
                for (const auto& [vID0, vID1] : headEdges)
                {
                    if ((headvID == vID0) || (headvID == vID1))
                    {
                        headvIDs.insert(vID0);
                        headvIDs.insert(vID1);
                    }
                }
            }
            eyelashConnectedVertices[i]->headvIDs = std::vector<int>(headvIDs.begin(), headvIDs.end());
            std::set<int> uniqueIndices(eyelashConnectedVertices[i]->indices.begin(), eyelashConnectedVertices[i]->indices.end());
            eyelashConnectedVertices[i]->indices = std::vector(uniqueIndices.begin(), uniqueIndices.end());
            eyelashConnectedVertices[i]->valid = true;
        }
    }

    Reduce(eyelashConnectedVertices);

    return true;
}

template <class T>
void EyelashConnectedVertices<T>::ApplyEyelashMapping(const Mesh<T>& srcHeadMesh, const Eigen::Matrix<T, 3, -1>& targetHeadMeshVertices, const Mesh<T>& srcEyelashesMesh, 
    const std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices, Eigen::Matrix<T, 3, -1>& updatedEyelashVertices)
{
    if (srcHeadMesh.NumVertices() != targetHeadMeshVertices.cols())
    {
        CARBON_CRITICAL("src head mesh and target head mesh number of vertices do not match");
    }
    updatedEyelashVertices = srcEyelashesMesh.Vertices();

    for (int i = 0; i < (int)eyelashConnectedVertices.size(); ++i)
    {
        // Eigen::Matrix<T, 3, -1> src = srcHeadMesh.Vertices()(Eigen::all, eyelashConnectedVertices[i]->headvIDs);
        // Eigen::Matrix<T, 3, -1> target = targetHeadMeshVertices(Eigen::all, eyelashConnectedVertices[i]->headvIDs);
        // align roots using affine transformation
        // Eigen::Transform<T, 3, Eigen::Affine> affine(Procrustes<T, 3>::AlignRigid(src, target).Matrix());
        // updatedEyelashVertices(Eigen::all, eyelashConnectedVertices[i]->indices).noalias() = affine * srcEyelashesMesh.Vertices()(Eigen::all, eyelashConnectedVertices[i]->indices);

        // align roots using translation
        Eigen::Vector3<T> src = Eigen::Vector3<T>::Zero();
        Eigen::Vector3<T> target = Eigen::Vector3<T>::Zero();
        for (int vID : eyelashConnectedVertices[i]->headvIDs)
        {
            src += srcHeadMesh.Vertices().col(vID);
            target += targetHeadMeshVertices.col(vID);
        }
        Eigen::Vector3<T> offset = (target - src) / T(eyelashConnectedVertices[i]->headvIDs.size());
        updatedEyelashVertices(Eigen::all, eyelashConnectedVertices[i]->indices).colwise() += offset;
    }
}

template <class T>
void EyelashConnectedVertices<T>::Reduce(std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices)
{
    // remove duplicates
    bool hasDuplicates = false;
    std::vector<bool> duplicate(eyelashConnectedVertices.size(), false);
    for (size_t i = 0; i < eyelashConnectedVertices.size(); ++i)
    {
        for (size_t j = 0; j < i; ++j)
        {
            if (eyelashConnectedVertices[i]->indices == eyelashConnectedVertices[j]->indices)
            {
                eyelashConnectedVertices[i] = eyelashConnectedVertices[j];
                duplicate[i] = true;
                hasDuplicates = true;
                break;
            }
        }
    }

    if (hasDuplicates)
    {
        // old format with eyelashConnectedVertices per eyelash vertex
        // add vertices that are missing
        for (size_t i = 0; i < eyelashConnectedVertices.size(); ++i)
        {
            eyelashConnectedVertices[i]->indices.push_back((int)i);
        }

        for (size_t i = 0; i < eyelashConnectedVertices.size(); ++i)
        {
            std::set<int> indices(eyelashConnectedVertices[i]->indices.begin(), eyelashConnectedVertices[i]->indices.end());
            eyelashConnectedVertices[i]->indices = std::vector<int>(indices.begin(), indices.end());
        }

        std::vector<std::shared_ptr<EyelashConnectedVertices<T>>> newEyelashConnectedVertices;
        for (size_t i = 0; i < duplicate.size(); ++i)
        {
            if (!duplicate[i])
            {
                newEyelashConnectedVertices.push_back(eyelashConnectedVertices[i]);
            }
        }
        eyelashConnectedVertices = newEyelashConnectedVertices;
    }
}

template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashConnectedVertices<T>& eyelashConnectedVertices)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, eyelashConnectedVertices.version);
    success &= io::ToBinaryFile(pFile, eyelashConnectedVertices.valid);
    success &= io::ToBinaryFile(pFile, eyelashConnectedVertices.indices);
    success &= ToBinaryFile(pFile, eyelashConnectedVertices.affine);
    success &= io::ToBinaryFile(pFile, eyelashConnectedVertices.headvIDs);
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashConnectedVertices<T>& eyelashConnectedVertices)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 1)
    {
        success &= io::FromBinaryFile(pFile, eyelashConnectedVertices.valid);
        success &= io::FromBinaryFile(pFile, eyelashConnectedVertices.indices);
        success &= FromBinaryFile(pFile, eyelashConnectedVertices.affine);
        success &= io::FromBinaryFile(pFile, eyelashConnectedVertices.headvIDs);
    }
    else
    {
        success = false;
    }
    return success;
}


// explicitly instantiate the struct and functions
template struct EyelashConnectedVertices<float>;
template struct EyelashConnectedVertices<double>;

template bool ToBinaryFile(FILE* pFile, const EyelashConnectedVertices<float>& eyelashConnectedVertices);
template bool ToBinaryFile(FILE* pFile, const EyelashConnectedVertices<double>& eyelashConnectedVertices);

template bool FromBinaryFile(FILE* pFile, EyelashConnectedVertices<float>& eyelashConnectedVertices);
template bool FromBinaryFile(FILE* pFile, EyelashConnectedVertices<double>& eyelashConnectedVertices);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
