// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/BarycentricCoordinatesForOddLods.h>
#include <carbon/io/Utils.h>
#include <filesystem>
#include <carbon/geometry/AABBTree.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void BarycentricCoordinatesForOddLods<T>::Init(const RigGeometry<T>& headRigGeometry)
{
    m_barycentricCoordinatesForOddLods.clear();

    int prevHeadMeshIndex = -1;
    for (int lod = 0; lod < headRigGeometry.NumLODs(); ++lod)
    {
        const int headMeshIndex = headRigGeometry.HeadMeshIndex(lod);

        if (lod % 2 == 1)
        {
            // for each point on the current mesh, we find the corresponding vertex on the previous lod mesh (which will match the combined body model)
            Mesh<T> prevMesh = headRigGeometry.GetMesh(prevHeadMeshIndex);
            prevMesh.Triangulate();
            AABBTree<T> aabbTree(prevMesh.Vertices().transpose(), prevMesh.Triangles().transpose());

            std::vector<std::pair<bool, BarycentricCoordinates<T>>> curBarycentricCoords(headRigGeometry.GetMesh(headMeshIndex).NumVertices(), std::make_pair(false, BarycentricCoordinates<T>()));

            for (int vID = 0; vID < headRigGeometry.GetMesh(headMeshIndex).Vertices().cols(); ++vID)
            {
                auto [tID, bcWeights, dist] = aabbTree.getClosestPoint(headRigGeometry.GetMesh(headMeshIndex).Vertices().col(vID).transpose(), std::numeric_limits<float>::max());

                if (tID != -1)
                {
                    BarycentricCoordinates<T> bc(prevMesh.Triangles().col(tID), bcWeights.transpose());
                    // force all weights to be in range 0 to 1 otherwise we can get negative skinning weights which causes warnings for the DNA
                    // they can occasionally be slightly out of range
                    auto weights = bc.Weights();
                    for (size_t i = 0; i < 3; ++i)
                    {
                        weights[i] = std::clamp(weights[i], T(0.0), T(1.0));
                    }
                    bc = BarycentricCoordinates<T>(bc.Indices(), weights);
                    curBarycentricCoords[size_t(vID)] = std::make_pair(true, bc);
                }
            }

            m_barycentricCoordinatesForOddLods[lod] = curBarycentricCoords;
        }

        prevHeadMeshIndex = headMeshIndex;
    }
}

template <class T>
bool BarycentricCoordinatesForOddLods<T>::IsValidForRig(const RigGeometry<T>& faceRigGeometry) const
{
    int prevHeadMeshIndex = -1;

    for (int lod = 0; lod < faceRigGeometry.NumLODs(); ++lod)
    {
        const int headMeshIndex = faceRigGeometry.HeadMeshIndex(lod);
        if (lod % 2 == 1)
        {
            const Mesh<T>& prevMesh = faceRigGeometry.GetMesh(prevHeadMeshIndex);

            if (m_barycentricCoordinatesForOddLods.find(lod) == m_barycentricCoordinatesForOddLods.end())
            {
                return false;
            }

            const auto& curBcs = m_barycentricCoordinatesForOddLods.at(lod);
            if (int(curBcs.size()) != faceRigGeometry.GetMesh(headMeshIndex).NumVertices())
            {
                return false;
            }

            // check indices are valid for those from previous mesh 
            for (size_t v = 0; v < curBcs.size(); v++)
            {
                if (curBcs[v].first)
                {
                    for (size_t i = 0; i < 3; ++i)
                    {
                        if (curBcs[v].second.Index(int(i)) < 0 || curBcs[v].second.Index(int(i)) > prevMesh.NumVertices())
                        {
                            return false;
                        }
                    }
                }
            }
        }
       
        prevHeadMeshIndex = headMeshIndex;
    }

    return true;
}

template <class T>
bool BarycentricCoordinatesForOddLods<T>::ReadJson(const JsonElement& json)
{
    const auto& barycentricCoordinatesForOddLods = json.Object();

    m_barycentricCoordinatesForOddLods.clear();

    for (const auto& lodBcsJson : barycentricCoordinatesForOddLods)
    {
        std::vector<std::pair<bool, BarycentricCoordinates<T>>> curLodBcs;
        int lod;
        try
        {
            lod = std::stoi(lodBcsJson.first);
        }
        catch (std::exception &)
        {
            LOG_ERROR("Failed to parse lod {} as an int", lodBcsJson.first);
            return false;
        }
        if (lodBcsJson.second.IsArray())
        {
            for (size_t i = 0; i < lodBcsJson.second.Array().size(); ++i)
            {
                if (lodBcsJson.second.Array()[i].IsObject())
                {
                    std::pair<bool, BarycentricCoordinates<T>> curBcsPair;

                    if (lodBcsJson.second.Array()[i].Contains("valid"))
                    {
                        curBcsPair.first = lodBcsJson.second.Array()[i]["valid"].Boolean();
                    }
                    else
                    {
                        LOG_ERROR("No 'valid' field found for barycentric coord {}, lod {}", i, lod);
                        return false;
                    }

                    Eigen::Vector<int, -1> indices;
                    Eigen::Vector<T, -1> weights;
                    if (lodBcsJson.second.Array()[i].Contains("bcs") && lodBcsJson.second.Array()[i]["bcs"].IsObject())
                    {
                        if (lodBcsJson.second.Array()[i]["bcs"].Contains("indices") && lodBcsJson.second.Array()[i]["bcs"]["indices"].IsArray())
                        {
                            const std::vector<int> indicesVec = lodBcsJson.second.Array()[i]["bcs"]["indices"].Get<std::vector<int>>();
                            indices.resize(int(indicesVec.size()));
                            for (int j = 0; j < indices.size(); ++j)
                            {
                                indices(j) = indicesVec[size_t(j)];
                            }
                        }
                        else
                        {
                            LOG_ERROR("No 'indices' field found for barycentric coord {}, lod {}", i, lod);
                            return false;
                        }

                        if (lodBcsJson.second.Array()[i]["bcs"].Contains("weights") && lodBcsJson.second.Array()[i]["bcs"]["weights"].IsArray())
                        {
                            const std::vector<T> weightsVec = lodBcsJson.second.Array()[i]["bcs"]["weights"].Get<std::vector<T>>();
                            weights.resize(int(weightsVec.size()));
                            for (int j = 0; j < weights.size(); ++j)
                            {
                                weights(j) = weightsVec[size_t(j)];
                            }
                        }
                        else
                        {
                            LOG_ERROR("No 'weights' field found for barycentric coord {}, lod {}", i, lod);
                            return false;
                        }
                    }
                    else
                    {
                        LOG_ERROR("No 'bcs' field found for barycentric coord {}, lod {}", i, lod);
                        return false;
                    }
                    BarycentricCoordinates<T> curBcs(indices, weights);
                    curBcsPair.second = curBcs;
                    curLodBcs.push_back(curBcsPair);
                }
                else
                {
                    LOG_ERROR("Element {} of barycentric coords for lod {} is not an object", i, lod);
                    return false;
                }
            }
        }
        else
        {
            LOG_ERROR("Failure parsing BarycentricCoordinatesForOddLods for lod {}", lodBcsJson.first);
            return false;
        }
        m_barycentricCoordinatesForOddLods[lod] = curLodBcs; 
    }
    
    return true;
}

template <class T>
void BarycentricCoordinatesForOddLods<T>::WriteJson(JsonElement& json) const
{
    JsonElement barycentricCoordinatesForOddLodsJson(JsonElement::JsonType::Object);

    for (const auto& lodBarycentricCoordinates : m_barycentricCoordinatesForOddLods)
    {
        JsonElement curLodBarycentricCoordinates(JsonElement::JsonType::Array);
        for (size_t i = 0; i < lodBarycentricCoordinates.second.size(); ++i)
        {
            JsonElement curBcsPair(JsonElement::JsonType::Object);
            JsonElement curBcs(JsonElement::JsonType::Object); 
            const std::vector<int> indices(lodBarycentricCoordinates.second[i].second.Indices().data(), lodBarycentricCoordinates.second[i].second.Indices().data() + lodBarycentricCoordinates.second[i].second.Indices().size());
            const std::vector<T> weights(lodBarycentricCoordinates.second[i].second.Weights().data(), lodBarycentricCoordinates.second[i].second.Weights().data() + lodBarycentricCoordinates.second[i].second.Weights().size());
            curBcs.Insert("indices", JsonElement(indices));
            curBcs.Insert("weights", JsonElement(weights));
            curBcsPair.Insert("valid", JsonElement(lodBarycentricCoordinates.second[i].first));
            curBcsPair.Insert("bcs", std::move(curBcs)); 
            curLodBarycentricCoordinates.Append(std::move(curBcsPair));
        }

        barycentricCoordinatesForOddLodsJson.Insert(std::to_string(lodBarycentricCoordinates.first), std::move(curLodBarycentricCoordinates));
    }

    json.Insert("barycentric_coordinates_for_odd_lods", std::move(barycentricCoordinatesForOddLodsJson));
}


template <class T>
const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<T>>>>& BarycentricCoordinatesForOddLods<T>::GetBarycentricCoordinatesForOddLods() const
{
    return m_barycentricCoordinatesForOddLods;
}



template class BarycentricCoordinatesForOddLods<float>;
template class BarycentricCoordinatesForOddLods<double>;


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
