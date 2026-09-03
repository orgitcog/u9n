// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <carbon/utils/Base64.h>
#include <carbon/io/Utils.h>
#include <nls/math/Math.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/geometry/WrapDeformer.h>
#include <nls/geometry/SnapConfig.h>
#include <nls/serialization/ObjFileFormat.h>
#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// forward declarations
template<class T>
class LodGeneration;

/**
 * Representation of a configuration representing how Lod Generation is performed
 */
template <class T>
class LodGenerationConfiguration
{
public:
    struct LodData
    {
        std::string meshName;
        std::string driverMesh;
        std::shared_ptr<const Mesh<T>> mesh;
        SnapConfig<T> snapConfig;
        bool ReadJson(const JsonElement& element, const std::string& baseDir, bool isFileBased = true);
    };

    struct LodGenerationData
    {
        std::vector<LodData> lodData;
        WrapDeformerParams<T> params;
        bool ReadJson(const JsonElement& element, const std::string& baseDir, bool isFileBased = true);
    };

    LodGenerationConfiguration() = default;

    //! Load data from either file or the data directly.
    bool Load(const std::string& filenameOrData);

    const std::map<std::string, LodGenerationData>& RigPartLodGenerationData() const
    {
        return m_rigPartLodGenerationData;
    }

    friend class LodGeneration<T>;

private:
    bool LoadJson(const std::string& jsonString, const std::string& baseDir, bool isFileBased);

    // a map of part name to lod generation data for each rig part
    std::map<std::string, LodGenerationData> m_rigPartLodGenerationData;
};

/**
 * Representation of a class for performing Lod generation using wrap deformers
 */
template <class T>
class LodGeneration
{
public:
    LodGeneration();

    //! Set a threadpool for parallelization of lod generation tasks (if not set, the default global threadpool will be used)
    void SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool);

    //! initialize the lod generation object from a config
    bool Init(const LodGenerationConfiguration<T>& config);

    //! apply lod generation to a map of mesh name to vertices, once the object has been initialized, and return the results for the higher lods as a map of mesh name to vertices
    bool Apply(std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0MeshVertices, std::map<std::string, Eigen::Matrix<T, 3, -1>>& higherLodMeshVertices, bool bAllowMissingMeshes = false) const;

    //! once the object has been initialized, for each higher lod mesh, get the barycentric coordinates for the closest vertices on the driver mesh, returning the results as a map of mesh names to vector of barycemtric coordinates
    void GetDriverMeshClosestPointBarycentricCoordinates(std::map<std::string, std::vector<BarycentricCoordinates<T>>>& driverMeshClosestPointBarycentricCoordinates) const;
    
    //! load from a binary lod generation model filename
    bool LoadModelBinary(const std::string& lodGenerationModelFile);

    //! save to a binary lod generation model filename
    bool SaveModelBinary(const std::string& lodGenerationModelFile) const;

    const std::vector<std::string>& Lod0MeshNames() const { return m_baseMeshes; }
    std::vector<std::string> HigherLodMeshNames() const;
    int LodForMesh(const std::string& meshName) const;

private:
    static constexpr int32_t m_version = 2;
    std::vector<std::string> m_baseMeshes;
    std::map<std::string, std::string> m_driverMeshNames;
    std::map<std::string, WrapDeformer<T>> m_wrapDeformers;
    std::map<std::string, SnapConfig<T>> m_snapConfigs;
    std::map<std::string, int> m_meshLods;
    std::map<std::string, std::shared_ptr<const Mesh<T>>> m_allMeshes;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> m_taskThreadPool;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
