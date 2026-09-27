// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <nrr/MeshLandmarks.h>
#include <nrr/SymmetryMapping.h>
#include <nrr/VertexWeights.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Class containing all template face specific data such as mesh symmetry information, landmark positions on the mesh, weight masks etc.
 * The class does not contain any geometry/texture information for an individual.
 */
class TemplateDescription
{
public:
    TemplateDescription() = default;

    //! Load data from either file or the data directly.
    bool Load(const std::string& filenameOrData);

    const Mesh<float>& Topology() const { return m_topology; }

    const Mesh<float>& TriangulatedTopology() const { return m_triangulatedTopology; }

    const SymmetryMapping& GetSymmetryMapping() const { return m_symmetryMapping; }

    bool HasVertexWeights(const std::string& maskName) const { return m_vertexWeights.find(maskName) != m_vertexWeights.end(); }
    const VertexWeights<float>& GetVertexWeights(const std::string& maskName) const;
    const std::map<std::string, VertexWeights<float>>& GetVertexWeights() const { return m_vertexWeights; }
    const std::map<std::string, VertexWeights<float>>& GetExpressionsFittingMasks() const { return m_rigCalibExpressionsFittingMasks; }
    const VertexWeights<float>& GetAssetVertexWeights(const std::string& assetName, const std::string maskName) const;
    const std::map<std::string, VertexWeights<float>>& GetAssetVertexWeights(const std::string& assetName) const;

    bool HasEdgeWeights(const std::string& edgeMapName) const { return m_edgeWeights.find(edgeMapName) != m_edgeWeights.end(); }
    const std::vector<std::tuple<int, int, float>>& GetEdgeWeights(const std::string& edgeMapName) const;

    const Mesh<float>& GetAssetTopology(const std::string& assetName) const;
    bool HasAssetTopology(const std::string& assetName) const { return m_assetTopologies.find(assetName) != m_assetTopologies.end(); }
    bool HasAssetVertexWeights(const std::string& assetName) const { return m_assetVertexWeights.find(assetName) != m_assetVertexWeights.end(); }
    bool HasAssetVertexWeights(const std::string& assetName, const std::string& maskName) const;

    bool HasAssetSymmetries(const std::string& assetName) const { return m_assetSymmetryMappings.find(assetName) != m_assetSymmetryMappings.end(); }
    const SymmetryMapping& GetAssetSymmetryMapping(const std::string& assetName) const { return m_assetSymmetryMappings.find(assetName)->second; }

    const MeshLandmarks<float>& GetMeshLandmarks() const { return m_meshLandmarks; }
    const MeshLandmarks<float>& GetEyeLeftMeshLandmarks() const { return m_eyeLeftMeshLandmarks; }
    const MeshLandmarks<float>& GetEyeRightMeshLandmarks() const { return m_eyeRightMeshLandmarks; }
    const MeshLandmarks<float>& GetTeethMeshLandmarks() const { return m_teethMeshLandmarks; }

    const MeshLandmarks<float>& GetAssetMeshLandmarks(const std::string& assetName) const;

    bool HasAssetTexture(const std::string& assetName) const { return m_assetTextureFilename.find(assetName) != m_assetTextureFilename.end(); }
    const std::string& GetAssetTextureFilename(const std::string& assetName) const;

    const std::string& TextureFilename() const { return m_textureFilename; }

    bool HasSubdivMesh() const { return (m_subdivMesh.NumVertices() > 0); }
    const Mesh<float>& SubdivMesh() const { return m_subdivMesh; }
    const std::vector<std::tuple<int, int, float>>& SubdivStencilWeights() const { return m_subdivStencilWeights; }

    const std::string& VolumetricModelDirname() const { return m_volumetricModelDirname; }

    std::map<std::string, std::vector<std::string>> GetPredefinedInfluencesUpstream() const
    {
        return m_predefinedInfluencedExpressionsUpstream;
    }

    std::map<std::string, std::vector<std::string>> GetPredefinedInfluencesDownstream() const
    {
        return m_predefinedInfluencedExpressionsDownstream;
    }

private:
    bool LoadJson(const std::string& jsonString, const std::string& baseDir, bool isFileBased);

private:
    //! topology
    Mesh<float> m_topology;

    //! the triangulated topology
    Mesh<float> m_triangulatedTopology;

    //! symmetry mapping
    SymmetryMapping m_symmetryMapping;

    //! named weight maps
    std::map<std::string, VertexWeights<float>> m_vertexWeights;

    //! named weight maps for expressions fitting
    std::map<std::string, VertexWeights<float>> m_rigCalibExpressionsFittingMasks;

    //! named edge maps where an edge map is a vector of tuples of form [vID0, vID1, edgeWeight]
    std::map<std::string, std::vector<std::tuple<int, int, float>>> m_edgeWeights;

    //! mesh landmark information
    MeshLandmarks<float> m_meshLandmarks;

    //! mesh landmark information for the left eye ball
    MeshLandmarks<float> m_eyeLeftMeshLandmarks;

    //! mesh landmark information for the right eye ball
    MeshLandmarks<float> m_eyeRightMeshLandmarks;

    //! mesh landmark information for the teeth
    MeshLandmarks<float> m_teethMeshLandmarks;

    //! asset meshes
    std::map<std::string, Mesh<float>> m_assetTopologies;

    //! asset weight maps
    std::map<std::string, std::map<std::string, VertexWeights<float>>> m_assetVertexWeights;

    //! per-asset texture filename
    std::map<std::string, std::string> m_assetTextureFilename;

    //! asset mesh landmarks
    std::map<std::string, MeshLandmarks<float>> m_assetMeshLandmarks;

    //! asset symmetry mapping
    std::map<std::string, SymmetryMapping> m_assetSymmetryMappings;

    //! texture with the guide lines
    std::string m_textureFilename;

    //! subdiv mesh
    Mesh<float> m_subdivMesh;

    /**
     * Subdiv stencil weights mapping from subdiv vID to topology vID and weight. Format: [subdiv_vID, topology_vID, weight].
     * subdiv_vertices[subdiv_vID] = sum(topology_vertices[topology_vID] * weight)
     */
    std::vector<std::tuple<int, int, float>> m_subdivStencilWeights;

    //! path to the directory containing the volumetric model of the template
    std::string m_volumetricModelDirname;

    //! Expressions influenced by other expressions (predefined by hand)
    std::map<std::string, std::vector<std::string>> m_predefinedInfluencedExpressionsUpstream;
    std::map<std::string, std::vector<std::string>> m_predefinedInfluencedExpressionsDownstream;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
