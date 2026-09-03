// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/TemplateDescription.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/Base64.h>
#include <nls/serialization/EigenSerialization.h>
#include <nls/serialization/ObjFileFormat.h>

#include <sstream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

bool TemplateDescription::Load(const std::string& filenameOrData)
{
    const bool isValidFile = std::filesystem::exists(filenameOrData);
    if (isValidFile)
    {
        // filenameOrData points to a file
        const std::string filedata = ReadFile(filenameOrData);
        const std::string baseDir = std::filesystem::absolute(std::filesystem::path(filenameOrData)).parent_path().string();
        return LoadJson(filedata, baseDir, /*isFileBased=*/true);
    }
    else
    {
        // assume filenameOrData is the data directly
        return LoadJson(filenameOrData, "", /*isFileBased=*/false);
    }
}

bool TemplateDescription::LoadJson(const std::string& jsonString, const std::string& baseDir, bool isFileBased)
{
    try
    {
        const JsonElement j = ReadJson(jsonString);

        auto makeAbsolute = [&](const std::string& filename) {
                if (std::filesystem::path(filename).is_relative())
                {
                    return baseDir + "/" + filename;
                }
                else
                {
                    return filename;
                }
            };

        auto readJsonFromFile = [&](const std::string& filename) {
                const std::string absoluteFilename = makeAbsolute(filename);
                const std::string fileData = ReadFile(absoluteFilename);
                return ReadJson(fileData);
            };

        auto loadMasks = [&](const JsonElement& json, int numVertices) {
                if (isFileBased && json.IsString())
                {
                    return VertexWeights<float>::LoadAllVertexWeights(readJsonFromFile(json.String()), numVertices);
                }
                else
                {
                    return VertexWeights<float>::LoadAllVertexWeights(json, numVertices);
                }
            };

        auto loadMeshLandmarks =
            [&](const JsonElement& json, MeshLandmarks<float>& meshLandmarks, const Mesh<float>& mesh, const std::string& meshName) {
                if (isFileBased && json.IsString())
                {
                    const std::string meshLandmarksFile = makeAbsolute(json.String());
                    if (!meshLandmarks.Load(meshLandmarksFile, mesh, meshName))
                    {
                        CARBON_CRITICAL("failure to load mesh landmarks from {}", meshLandmarksFile);
                    }
                }
                else
                {
                    if (!meshLandmarks.DeserializeJson(json, mesh, meshName))
                    {
                        CARBON_CRITICAL("failure to load mesh landmarks json data");
                    }
                }
            };

        auto loadSymmetry = [&](const JsonElement& json, SymmetryMapping& symmetryMapping, int numVertices) {
                if (!symmetryMapping.Load(json))
                {
                    CARBON_CRITICAL("failure to load symmetry information");
                }
                if (symmetryMapping.NumSymmetries() != numVertices)
                {
                    LOG_INFO("symmetry size: {}", symmetryMapping.NumSymmetries());
                    LOG_INFO("topology size: {}", numVertices);
                    CARBON_CRITICAL("symmetry mapping does not have the same vertex count as the topology/asset");
                }
            };

        auto loadObj = [&](const std::string& str, Mesh<float>& mesh) {
                if (isFileBased)
                {
                    const std::string objFilename = makeAbsolute(str);
                    if (std::filesystem::exists(objFilename))
                    {
                        if (!ObjFileReader<float>::readObj(objFilename, mesh))
                        {
                            CARBON_CRITICAL("failed to load mesh from {}", objFilename);
                        }
                        return;
                    }
                }
                std::vector<unsigned char> decodedData;
                if (!Base64Decode(str, decodedData))
                {
                    CARBON_CRITICAL("failed to decode mesh data");
                }
                const std::string decodedStr(decodedData.begin(), decodedData.end());
                if (!ObjFileReader<float>::readObjFromString(decodedStr, mesh))
                {
                    CARBON_CRITICAL("failed to load mesh from string");
                }
            };

        std::function<std::map<std::string, std::vector<std::string>>(const std::string &filename)> readPredefinedInfluencedExpressions =
            [&](const std::string &filename)
        {
            auto dataPerExpressionData = readJsonFromFile(filename);
            const auto &dataPerExpression = dataPerExpressionData.Map();

            std::map<std::string, std::vector<std::string>> predefinedInfluences;

            for (auto it = dataPerExpression.begin(); it != dataPerExpression.end(); ++it)
            {
                const auto &exprName = it->first;
                const auto &dependents = it->second.Array();

                predefinedInfluences[exprName] = std::vector<std::string>();

                for (int i = 0; i < (int)dependents.size(); ++i)
                {
                    predefinedInfluences[exprName].push_back(dependents[i].String());
                }
            }

            return predefinedInfluences;
        };

        // parse topology
        if (j.Contains("topology"))
        {
            loadObj(j["topology"].String(), m_topology);
        }
        else
        {
            CARBON_CRITICAL("topology missing from template description");
        }

        if (j.Contains("triangulated_topology"))
        {
            loadObj(j["triangulated_topology"].String(), m_triangulatedTopology);
            if ((m_triangulatedTopology.NumVertices() != m_topology.NumVertices()) ||
                (m_triangulatedTopology.NumTexcoords() != m_topology.NumTexcoords()))
            {
                CARBON_CRITICAL("triangulated topology does not match the mesh topology");
            }
            if (m_triangulatedTopology.NumQuads() > 0)
            {
                CARBON_CRITICAL("triangulated topology should only contain triangles");
            }
        }
        else
        {
            m_triangulatedTopology = m_topology;
            m_triangulatedTopology.Triangulate();
        }

        // parse symmetry information
        if (j.Contains("symmetry"))
        {
            if (isFileBased && j["symmetry"].IsString())
            {
                loadSymmetry(readJsonFromFile(j["symmetry"].String()), m_symmetryMapping, m_topology.NumVertices());
            }
            else
            {
                loadSymmetry(j["symmetry"], m_symmetryMapping, m_topology.NumVertices());
            }
        }

        // parse mask information
        if (j.Contains("masks"))
        {
            m_vertexWeights = loadMasks(j["masks"], m_topology.NumVertices());
        }

        // parse expressions fitting masks
        if (j.Contains("rig_calib_expressions_fitting_masks"))
        {
            m_rigCalibExpressionsFittingMasks = loadMasks(j["rig_calib_expressions_fitting_masks"], m_topology.NumVertices());
        }

        // read influenced expressions
        if (j.Contains("rig_calib_predefined_influenced_expr_upstream"))
        {
            m_predefinedInfluencedExpressionsUpstream = readPredefinedInfluencedExpressions(
                j["rig_calib_predefined_influenced_expr_upstream"].String());
        }

        // read influenced expressions
        if (j.Contains("rig_calib_predefined_influenced_expr_downstream"))
        {
            m_predefinedInfluencedExpressionsDownstream = readPredefinedInfluencedExpressions(
                j["rig_calib_predefined_influenced_expr_downstream"].String());
        }

        // parse edge information
        if (j.Contains("edges"))
        {
            auto loadEdges = [&](const JsonElement& jEdges) {
                    for (const auto& [edgeName, jMap] : jEdges.Map())
                    {
                        // support both weighted and unweighted maps
                        if (jMap.IsArray() && (jMap.Size() > 0))
                        {
                            if (jMap[0].IsArray() && (jMap[0].Size() == 2))
                            {
                                auto edges = jMap.Get<std::vector<std::tuple<int, int>>>();
                                std::vector<std::tuple<int, int, float>> weightedEdges;
                                for (const auto& [vID0, vID1] : edges)
                                {
                                    weightedEdges.push_back({ vID0, vID1, 1.0f });
                                }
                                m_edgeWeights[edgeName] = weightedEdges;
                            }
                            else
                            {
                                m_edgeWeights[edgeName] = jMap.Get<std::vector<std::tuple<int, int, float>>>();
                            }
                        }
                    }
                };
            if (isFileBased && j["edges"].IsString())
            {
                loadEdges(readJsonFromFile(j["edges"].String()));
            }
            else
            {
                loadEdges(j["edges"]);
            }
        }

        if (j.Contains("assets"))
        {
            for (auto&& [assetName, elem] : j["assets"].Map())
            {
                const auto& jAsset = j["assets"][assetName];

                if (jAsset.Contains("topology"))
                {
                    loadObj(jAsset["topology"].String(), m_assetTopologies[assetName]);
                }
                else
                {
                    CARBON_CRITICAL("asset {} is missing the topology", assetName);
                }

                if (jAsset.Contains("masks"))
                {
                    m_assetVertexWeights[assetName] = loadMasks(jAsset["masks"], m_assetTopologies[assetName].NumVertices());
                }

                if (jAsset.Contains("texture"))
                {
                    if (!isFileBased)
                    {
                        CARBON_CRITICAL("texture filename is only supported for file-based template descriptions");
                    }
                    m_assetTextureFilename[assetName] = makeAbsolute(jAsset["texture"].String());
                }

                if (jAsset.Contains("mesh_landmarks"))
                {
                    MeshLandmarks<float> assetMeshLandmarks;
                    loadMeshLandmarks(jAsset["mesh_landmarks"], assetMeshLandmarks, m_assetTopologies[assetName], assetName);
                    m_assetMeshLandmarks.emplace(assetName, assetMeshLandmarks);
                }

                if (jAsset.Contains("symmetry"))
                {
                    if (isFileBased && jAsset["symmetry"].IsString())
                    {
                        loadSymmetry(readJsonFromFile(jAsset["symmetry"].String()),
                                     m_assetSymmetryMappings[assetName],
                                     m_assetTopologies[assetName].NumVertices());
                    }
                    else
                    {
                        loadSymmetry(jAsset["symmetry"], m_assetSymmetryMappings[assetName], m_assetTopologies[assetName].NumVertices());
                    }
                }
            }
        }

        // parse mesh landmarks
        if (j.Contains("mesh_landmarks"))
        {
            loadMeshLandmarks(j["mesh_landmarks"], m_meshLandmarks, m_topology, MeshLandmarks<float>::DEFAULT_MESH_NAME);

            Mesh<float> eyeTemplateMesh = (m_assetTopologies.find("eye") != m_assetTopologies.end()) ? m_assetTopologies["eye"] : Mesh<float>();
            Mesh<float> teethTemplateMesh =
                (m_assetTopologies.find("teeth") != m_assetTopologies.end()) ? m_assetTopologies["teeth"] : Mesh<float>();

            loadMeshLandmarks(j["mesh_landmarks"], m_eyeLeftMeshLandmarks, eyeTemplateMesh, "eyeLeft_lod0_mesh");
            loadMeshLandmarks(j["mesh_landmarks"], m_eyeRightMeshLandmarks, eyeTemplateMesh, "eyeRight_lod0_mesh");
            loadMeshLandmarks(j["mesh_landmarks"], m_teethMeshLandmarks, teethTemplateMesh, "teeth_lod0_mesh");
        }

        if (j.Contains("texture"))
        {
            if (!isFileBased)
            {
                CARBON_CRITICAL("texture filename is only supported for file-based template descriptions");
            }
            m_textureFilename = makeAbsolute(j["texture"].String());
        }

        // parse subdiv
        if (j.Contains("subdiv"))
        {
            if (j["subdiv"].Contains("mesh"))
            {
                loadObj(j["subdiv"]["mesh"].String(), m_subdivMesh);
            }
            else
            {
                CARBON_CRITICAL("mesh missing in the subdivision part of the template description");
            }
            if (j["subdiv"].Contains("stencils"))
            {
                const std::string subdivStencilsFilename = makeAbsolute(j["subdiv"]["stencils"].String());
                m_subdivStencilWeights = ReadJson(ReadFile(subdivStencilsFilename)).Get<std::vector<std::tuple<int, int, float>>>();
            }
            else
            {
                CARBON_CRITICAL("stencil weights missing in the subdivision part of the template description");
            }
        }

        if (j.Contains("volumetric_model"))
        {
            if (!isFileBased)
            {
                CARBON_CRITICAL("volumetric model is only supported for file-based template descriptions");
            }
            m_volumetricModelDirname = makeAbsolute(j["volumetric_model"].String());
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("failure to load template description: {}", e.what());
        return false;
    }

    return true;
}

template <typename T>
const T& GetMapItemOrThrowCriticalError(const std::map<std::string, T>& map, const std::string& name, const std::string& msg)
{
    auto it = map.find(name);
    if (it != map.end())
    {
        return it->second;
    }
    else
    {
        CARBON_CRITICAL(msg, name);
    }
}

const VertexWeights<float>& TemplateDescription::GetVertexWeights(const std::string& maskName) const
{
    return GetMapItemOrThrowCriticalError(m_vertexWeights, maskName, "no vertex weights of name {}");
}

const std::map<std::string, VertexWeights<float>>& TemplateDescription::GetAssetVertexWeights(const std::string& assetName) const
{
    return GetMapItemOrThrowCriticalError(m_assetVertexWeights, assetName, "no vertex weights for asset {}");
}

const std::vector<std::tuple<int, int, float>>& TemplateDescription::GetEdgeWeights(const std::string& edgeMapName) const
{
    return GetMapItemOrThrowCriticalError(m_edgeWeights, edgeMapName, "no edge weights of name {}");
}

bool TemplateDescription::HasAssetVertexWeights(const std::string& assetName, const std::string& maskName) const
{
    auto it = m_assetVertexWeights.find(assetName);
    if (it != m_assetVertexWeights.end())
    {
        return (it->second.find(maskName) != it->second.end());
    }
    return false;
}

const VertexWeights<float>& TemplateDescription::GetAssetVertexWeights(const std::string& assetName, const std::string maskName) const
{
    const auto& vertexWeights = GetMapItemOrThrowCriticalError(m_assetVertexWeights, assetName, "no masks for asset {}");
    return GetMapItemOrThrowCriticalError(vertexWeights, maskName, "no vertex weights of name {}");
}

const Mesh<float>& TemplateDescription::GetAssetTopology(const std::string& assetName) const
{
    return GetMapItemOrThrowCriticalError(m_assetTopologies, assetName, "no topology for asset {}");
}

const std::string& TemplateDescription::GetAssetTextureFilename(const std::string& assetName) const
{
    return GetMapItemOrThrowCriticalError(m_assetTextureFilename, assetName, "no texture filename for asset {}");
}

const MeshLandmarks<float>& TemplateDescription::GetAssetMeshLandmarks(const std::string& assetName) const
{
    return GetMapItemOrThrowCriticalError(m_assetMeshLandmarks, assetName, "no mesh landmarks for asset {}");
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
