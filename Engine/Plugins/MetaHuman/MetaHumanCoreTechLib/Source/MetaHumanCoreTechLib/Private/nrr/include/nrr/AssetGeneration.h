// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <nls/math/Math.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/geometry/WrapDeformer.h>
#include <nls/geometry/SnapConfig.h>
#include <nls/serialization/ObjFileFormat.h>
#include <nrr/EyelashesGenerator.h>
#include <nrr/EyeAssetGenerator.h>
#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// forward declarations
template<class T>
class AssetGeneration;

/**
 * Representation of a configuration representing how Asset Generation is performed
 */
template <class T>
class AssetGenerationConfiguration
{
public:

    AssetGenerationConfiguration() = default;

    //! Load data from either file or the data directly.
    bool Load(const std::string& filenameOrData);

    friend class AssetGeneration<T>;

    template <class U>
    friend bool ToBinaryFile(FILE* pFile, const AssetGenerationConfiguration<U>& config);
    template <class U>
    friend bool FromBinaryFile(FILE* pFile, AssetGenerationConfiguration<U>& config);


private:
    bool LoadJson(const std::string& jsonString, const std::string& baseDir);

    static constexpr int32_t m_version = 1;

    std::string m_archetypePath;

    std::string m_salivaDriverMeshName;
    std::string m_salivaMeshName;
    WrapDeformerParams<T> m_salivaWrapDeformerParams;
    
    std::string m_cartilageDriverMeshName;
    std::string m_cartilageMeshName;
    WrapDeformerParams<T> m_cartilageWrapDeformerParams;

    std::string m_eyelashesDriverMeshName;
    std::string m_eyelashesMeshName;
    EyelashesGeneratorParams<T> m_eyelashesGeneratorParams;

    std::string m_headMeshName;
    std::string m_eyeLeftMeshName;
    std::string m_eyeRightMeshName;
    std::string m_eyeEdgeMeshName;
    std::string m_eyeshellMeshName;
    EyeAssetGeneratorParams<T> m_eyeEdgeGeneratorParams;
    EyeAssetGeneratorParams<T> m_eyeshellGeneratorParams;
};


template <class T>
bool ToBinaryFile(FILE* pFile, const AssetGenerationConfiguration<T>& config);

template <class T>
bool FromBinaryFile(FILE* pFile, AssetGenerationConfiguration<T>& config);


/**
 * Representation of a class for performing Asset generation using a mixture of vertex 'snapping' wrap deformers and optimization
 */
template <class T>
class AssetGeneration
{
public:
    AssetGeneration();
    
    //! Set a threadpool for parallelization of  asset generation tasks (if not set, the default global threadpool will be used)
    void SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool);

    //! Initialize the asset generation object from a configuration
    bool Init(const AssetGenerationConfiguration<T>& config);

    //! Perform asset generation once initialized given head, teeth and eye meshes (which must be triangulated) and return the results as a map of mesh name to asset vertices
    bool Apply(const std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0HeadEyeTeethMeshVertices, std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0AssetVertices) const;

    //! load from a binary asset generation model filename
    bool LoadModelBinary(const std::string& assetGenerationModelFile);

    //! save to a binary asset generation model filename
    bool SaveModelBinary(const std::string& assetGenerationModelFile) const;

private:
    static constexpr int32_t m_version = 1;

    AssetGenerationConfiguration<T> m_config;
    WrapDeformer<T> m_salivaWrapDeformer;
    WrapDeformer<T> m_cartilageWrapDeformer;
    EyelashesGenerator<T> m_eyelashesGenerator; 
    EyeAssetGenerator<T> m_eyeEdgeGenerator;
    EyeAssetGenerator<T> m_eyeshellGenerator;
    std::map<std::string, std::shared_ptr<const Mesh<T>>> m_allMeshes;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> m_taskThreadPool;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
