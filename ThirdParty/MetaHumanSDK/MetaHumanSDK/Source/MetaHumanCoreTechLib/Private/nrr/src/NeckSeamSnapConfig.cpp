// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/NeckSeamSnapConfig.h>
#include <carbon/io/Utils.h>
#include <nrr/VertexWeights.h>
#include <filesystem>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool NeckSeamSnapConfig<T>::Init(const std::string neckSeamPath, const RigGeometry<T>& rigGeometry)
{
    m_neckBodySnapConfig.clear();
    try
    {  
        JsonElement json = TITAN_NAMESPACE::ReadJson(ReadFile(neckSeamPath));

        std::vector<int> meshLods(rigGeometry.NumMeshes(), -1);
        for (int lod = 0; lod < rigGeometry.NumLODs(); ++lod)
        {
            for (int meshIndex : rigGeometry.GetMeshIndicesForLOD(lod))
            {
                meshLods[meshIndex] = lod;
            }
        }
        for (int meshIndex = 0; meshIndex < rigGeometry.NumMeshes(); ++meshIndex)
        {
            const std::string meshName = rigGeometry.GetMeshName(meshIndex);
            if (json.Contains(meshName))
            {
                VertexWeights<T> weights;
                weights.Load(json[meshName], "neck_seam", rigGeometry.GetMesh(meshIndex).NumVertices());

                SnapConfig<T> snapConfig;
                // body lods match every second face lod with the body mesh vertices starting with the corresponding face vertices
                const int bodyLod = meshLods[meshIndex] / 2;
                if (meshLods[meshIndex] % 2)
                {
                    continue;
                }
                snapConfig.sourceMesh = std::to_string(bodyLod);
                snapConfig.sourceVertexIndices = weights.NonzeroVertices();
                snapConfig.targetVertexIndices = weights.NonzeroVertices();
                m_neckBodySnapConfig[meshName] = { bodyLod, snapConfig };

                // special handling where the next face lod maps to the same body lod
                const int nextMeshIndex = rigGeometry.CorrespondingMeshIndexAtLod(meshIndex, meshLods[meshIndex] + 1);
                if (nextMeshIndex >= 0)
                {
                    const std::string nextMeshName = rigGeometry.GetMeshName(nextMeshIndex);
                    if (json.Contains(nextMeshName))
                    {
                        VertexWeights<T> nextWeights;
                        nextWeights.Load(json[nextMeshName], "neck_seam", rigGeometry.GetMesh(nextMeshIndex).NumVertices());
                        if (nextWeights.NonzeroVertices().size() == weights.NonzeroVertices().size())
                        {
                            SnapConfig<T> nextSnapConfig = snapConfig;
                            const Mesh<T>& mesh = rigGeometry.GetMesh(meshIndex);
                            const Mesh<T>& nextMesh = rigGeometry.GetMesh(nextMeshIndex);
                            nextSnapConfig.targetVertexIndices.clear();
                            for (int idx = 0; idx < (int)snapConfig.targetVertexIndices.size(); ++idx)
                            {
                                Eigen::Vector<T, 3> vertex = mesh.Vertices().col(snapConfig.targetVertexIndices[idx]);
                                int bestvID = nextWeights.NonzeroVertices().front();
                                for (int nextvID : nextWeights.NonzeroVertices())
                                {
                                    if ((nextMesh.Vertices().col(nextvID) - vertex).squaredNorm() <= (nextMesh.Vertices().col(bestvID) - vertex).squaredNorm())
                                    {
                                        bestvID = nextvID;
                                    }
                                }
                                nextSnapConfig.targetVertexIndices.push_back(bestvID);
                            }
                            m_neckBodySnapConfig[nextMeshName] = { bodyLod, nextSnapConfig };
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to parse neck seam snap config with error: {}", e.what());
        return false;
    }
    return true;
}

template <class T>
bool NeckSeamSnapConfig<T>::IsValidForCombinedBodyAndFaceRigs(const RigGeometry<T>& combinedBodyRigGeometry, const RigGeometry<T>& faceRigGeometry) const
{
    std::vector<std::string> headMeshNames(faceRigGeometry.NumLODs());
    for (int lod = 0; lod < faceRigGeometry.NumLODs(); ++lod)
    {
        headMeshNames[size_t(lod)] = faceRigGeometry.HeadMeshName(lod);
    }
    
    if (m_neckBodySnapConfig.size() != headMeshNames.size())
    {
        return false;
    }

    for (const auto & name : headMeshNames)
    {
        if (faceRigGeometry.GetMeshIndex(name) == -1)
        {
            return false;
        }
    }

    // check all lods of body are covered and that each snap config is valid
    std::set<int> lods;
    for (const auto& pair : m_neckBodySnapConfig)
    {
        lods.insert(pair.second.first);
        if (!pair.second.second.IsValid(combinedBodyRigGeometry.GetMesh(pair.second.first).Vertices(), faceRigGeometry.GetMesh(pair.first).Vertices()))
        {
            return false;
        }
    }

    if (*lods.begin() != 0 || *lods.rbegin() != combinedBodyRigGeometry.NumLODs() - 1 || int(lods.size()) != combinedBodyRigGeometry.NumLODs())
    {
        return false;
    }

    return true;
}


template <class T>
bool NeckSeamSnapConfig<T>::ReadJson(const JsonElement& json)
{
    const auto& snapConfigsMapJson = json.Object();

    m_neckBodySnapConfig.clear();

    for (const auto & lodMappingJson : snapConfigsMapJson)
    {
        std::pair<int, SnapConfig<T>> curMapping;

        if (lodMappingJson.second.IsObject())
        {
            if (lodMappingJson.second.Object().size() != 1)
            {
                LOG_ERROR("Failure parsing SnapConfig for head mesh {}, lod mapping should contain only one lod identifier per neck seam", lodMappingJson.first);
                return false;
            }
            else
            {
                // get first (and only) element of map
                for (const auto& pair : lodMappingJson.second.Object())
                {
                    if (pair.second.Contains("snap_config") && pair.second["snap_config"].IsObject())
                    {
                        bool bReadSnapConfig = curMapping.second.ReadJson(pair.second["snap_config"]);
                        if (!bReadSnapConfig)
                        {
                            LOG_ERROR("Failure parsing SnapConfig for head mesh {}, failed to parse SnapConfig", pair.first);
                            return false;
                        }

                        try
                        {
                            curMapping.first = std::stoi(pair.first);
                        }
                        catch (const std::invalid_argument&)
                        {
                            LOG_ERROR("Failure parsing SnapConfig for head mesh {}, body lod is not an integer", pair.first);
                            return false;
                        }
                    }
                    else
                    {
                        LOG_ERROR("Failure parsing SnapConfig for head mesh {}, snap_config field does not exist or is not an object", lodMappingJson.first);
                        return false;
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("Failure parsing SnapConfig for head mesh {}", lodMappingJson.first);
            return false;
        }
        m_neckBodySnapConfig[lodMappingJson.first] = curMapping;
    }

    return true;
}

template <class T>
void NeckSeamSnapConfig<T>::WriteJson(JsonElement& json) const
{
    JsonElement snapConfigsMapJson(JsonElement::JsonType::Object);

    for (const auto & lodMapping : m_neckBodySnapConfig)
    {
        JsonElement curLodMappingJson(JsonElement::JsonType::Object);
        JsonElement snapConfigJson(JsonElement::JsonType::Object);
        lodMapping.second.second.WriteJson(snapConfigJson);
        curLodMappingJson.Insert(std::to_string(lodMapping.second.first), std::move(snapConfigJson));
        snapConfigsMapJson.Insert(lodMapping.first, std::move(curLodMappingJson));
    }

    json.Insert("neck_seam_snap_config", std::move(snapConfigsMapJson));
}


template <class T>
const std::map<std::string, std::pair<int, SnapConfig<T>>>& NeckSeamSnapConfig<T>::GetLodNeckSeamSnapConfigs() const
{
    return m_neckBodySnapConfig;
}


template class NeckSeamSnapConfig<float>;
template class NeckSeamSnapConfig<double>;


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
