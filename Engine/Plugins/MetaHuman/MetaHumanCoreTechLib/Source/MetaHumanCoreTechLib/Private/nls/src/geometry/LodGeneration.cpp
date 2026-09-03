// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/LodGeneration.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool LodGenerationConfiguration<T>::LodData::ReadJson(const JsonElement& element, const std::string& baseDir, bool isFileBased)
{
    auto makeAbsolute = [&](const std::string& filename)
    {
        if (std::filesystem::path(filename).is_relative())
        {
            return baseDir + "/" + filename;
        }
        else
        {
            return filename;
        }
    };

    auto loadObj = [&](const std::string& str, Mesh<T>& mesh_)
    {
        if (isFileBased)
        {
            const std::string objFilename = makeAbsolute(str);
            if (std::filesystem::exists(objFilename))
            {
                if (!ObjFileReader<T>::readObj(objFilename, mesh_))
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
        if (!ObjFileReader<T>::readObjFromString(decodedStr, mesh_))
        {
            CARBON_CRITICAL("failed to load mesh from string");
        }
    };

    if (element.Contains("name") && element["name"].IsString())
    {
        meshName = element["name"].String();
    }
    else
    {
        LOG_ERROR("Failed to find load name for lod");
        return false;
    }

    if (element.Contains("driver_mesh") && element["driver_mesh"].IsString())
    {
        driverMesh = element["driver_mesh"].String();
    }

    if (element.Contains("mesh") && element["mesh"].IsString())
    {
        std::shared_ptr<Mesh<T>> curMesh = std::make_shared<Mesh<T>>();
        loadObj(element["mesh"].String(), *curMesh);
        // triangulate the mesh if needed as lod generation only works with triangulated meshes
        if (curMesh->NumQuads() > 0)
        {
            curMesh->Triangulate();
        }
        mesh = curMesh;
    }
    else
    {
        LOG_ERROR("Failed to find load mesh for lod");
        return false;
    }

    snapConfig = {};
    // snapConfig is optional
    if (element.Contains("snap_config") && element["snap_config"].IsObject())
    {
        const auto& snapConfigJson = element["snap_config"];
        const bool bLoadedSnapConfig = snapConfig.ReadJson(snapConfigJson);

        if (!bLoadedSnapConfig)
        {
            LOG_ERROR("failed to load snap config for lod");
            return false;
        }
    }

    return true;
}

template <class T>
bool LodGenerationConfiguration<T>::LodGenerationData::ReadJson(const JsonElement& element, const std::string& baseDir, bool isFileBased)
{
    if (element.Contains("lods") && element["lods"].IsArray())
    {
        lodData.resize(element["lods"].Array().size());
        size_t i = 0;
        for (const auto& lod : element["lods"].Array())
        {
            const bool bLoadedLod = lodData[i].ReadJson(lod, baseDir, isFileBased);
            if (!bLoadedLod)
            {
                LOG_ERROR("failed to load lod {}", i);
                return false;
            }
            ++i;
        }

        // check that the snap_config data is valid ie all vertex ids are in range for target mesh. source mesh will need checking at the higher level
        for (i = 0; i < lodData.size(); ++i)
        {
            if (!lodData[i].snapConfig.sourceMesh.empty())
            {
                // check that the target vertex indices are in range
                for (size_t v = 0; v < lodData[i].snapConfig.targetVertexIndices.size(); ++v)
                {
                    if (lodData[i].snapConfig.targetVertexIndices[v] >= lodData[i].mesh->NumVertices())
                    {
                        LOG_ERROR("vertex {} specified in snap_config target_vertex_indices for mesh {} is out of range", lodData[i].snapConfig.targetVertexIndices[v], lodData[i].meshName);
                        return false;
                    }
                }
            }
        }
    }
    else
    {
        LOG_ERROR("array of lods missing from lod generation configuration or not an array");
        return false;
    }

    if (element.Contains("params") && element["params"].IsObject())
    {
        const auto& paramsJson = element["params"];
        const bool bLoadedParams = params.ReadJson(paramsJson);

        if (!bLoadedParams)
        {
            LOG_ERROR("failed to load params from lod generation configuration");
            return false;
        }
    }
    else
    {
        LOG_ERROR("params missing from lod generation configuration");
        return false;
    }

    return true;
}

template <class T>
bool LodGenerationConfiguration<T>::Load(const std::string& filenameOrData)
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

template <class T>
bool LodGenerationConfiguration<T>::LoadJson(const std::string& jsonString, const std::string& baseDir, bool isFileBased)
{
    try
    {
        m_rigPartLodGenerationData.clear();
        const JsonElement j = ReadJson(jsonString);

        if (j.IsObject())
        {
            for (const auto& [partName, partConfig] : j.Object())
            {
                LodGenerationData partLodGenerationData;

                bool bLoadedLodGenerationData = partLodGenerationData.ReadJson(partConfig, baseDir, isFileBased);
                if (bLoadedLodGenerationData)
                {
                    m_rigPartLodGenerationData[partName] = partLodGenerationData;
                }
                else
                {
                    LOG_ERROR("failed to load lod generation data for part {}", partName);
                    return false;
                }
            }

            // go make through and check the snap_config data is valid ie all vertex ids are in range for source mesh
            for (const auto& [partName, part] : m_rigPartLodGenerationData)
            {
                for (size_t i = 0; i < part.lodData.size(); ++i)
                {
                    if (!part.lodData[i].snapConfig.sourceMesh.empty())
                    {
                        // check that the source vertex indices are for a mesh which exists and that they are in range
                        bool bFound = false;
                        size_t sourceMeshIndex = 0;
                        std::string sourcePartName;
                        for (const auto& [partName2, part2] : m_rigPartLodGenerationData)
                        {
                            for (size_t k = 0; k < part2.lodData.size(); ++k)
                            {
                                if (part2.lodData[k].meshName == part.lodData[i].snapConfig.sourceMesh)
                                {
                                    bFound = true;
                                    sourceMeshIndex = k;
                                    sourcePartName = partName2;
                                    break;
                                }
                            }
                        }

                        if (!bFound)
                        {
                            LOG_ERROR("failed to find source_mesh {} for mesh {} snap_config", part.lodData[i].snapConfig.sourceMesh, part.lodData[i].meshName);
                            return false;
                        }

                        for (size_t v = 0; v < part.lodData[i].snapConfig.sourceVertexIndices.size(); ++v)
                        {
                            if (part.lodData[i].snapConfig.sourceVertexIndices[v] >= m_rigPartLodGenerationData.at(sourcePartName).lodData[sourceMeshIndex].mesh->NumVertices())
                            {
                                LOG_ERROR("vertex {} specified in snap_config source_vertex_indices for mesh {} is out of range", part.lodData[i].snapConfig.sourceVertexIndices[v],
                                    part.lodData[i].meshName);
                                return false;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("failed to find any parts in the lod generation configuration");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("failure to load lod generation configuration: {}", e.what());
        return false;
    }

    return true;
}

template <class T>
LodGeneration<T>::LodGeneration()
{
    m_taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
}

template <class T>
void LodGeneration<T>::SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool)
{
    m_taskThreadPool = taskThreadPool;
}


template <class T>
bool LodGeneration<T>::Init(const LodGenerationConfiguration<T>& config)
{
    m_wrapDeformers.clear();
    m_baseMeshes.clear();
    m_driverMeshNames.clear();

    // for each part, and each lod, create a wrap deformer
    for (const auto& partLodGenerationData : config.m_rigPartLodGenerationData)
    {
        // add the meshes from the lod data for each part into the set of all meshes, so we don't have any duplication; do this first as we need all the meshes for various checks
        for (const auto& lodData : partLodGenerationData.second.lodData)
        {
            m_allMeshes[lodData.meshName] = lodData.mesh;
        }
    }

    for (const auto& partLodGenerationData : config.m_rigPartLodGenerationData)
    {
        for (size_t lod = 0; lod < partLodGenerationData.second.lodData.size(); ++lod)
        {
            const std::string curMeshName = partLodGenerationData.second.lodData[lod].meshName;
            m_meshLods[curMeshName] = static_cast<int>(lod);
            if (partLodGenerationData.second.lodData[lod].driverMesh.empty())
            {
                m_baseMeshes.emplace_back(curMeshName);
            }
            else
            {
                WrapDeformer<T> curWrapDeformer;
                std::shared_ptr<const Mesh<T>> driverMesh;
                const std::string driverMeshName = partLodGenerationData.second.lodData[lod].driverMesh;

                auto it = std::find_if(partLodGenerationData.second.lodData.begin(), partLodGenerationData.second.lodData.end(),
                    [&driverMeshName](const typename LodGenerationConfiguration<T>::LodData& lodData)
                    {
                        return lodData.meshName == driverMeshName;
                    });

                if (it != partLodGenerationData.second.lodData.end())
                {
                    driverMesh = it->mesh;
                    m_driverMeshNames[curMeshName] = driverMeshName;
                }
                else
                {
                    LOG_ERROR("failed to find matching driver mesh {}", driverMeshName);
                    return false;
                }

                curWrapDeformer.Init(driverMesh, partLodGenerationData.second.lodData[lod].mesh, partLodGenerationData.second.params);
                m_wrapDeformers[curMeshName] = curWrapDeformer;
            }
            if (!partLodGenerationData.second.lodData[lod].snapConfig.sourceMesh.empty())
            {
                std::string snapConfigSourceMeshName = partLodGenerationData.second.lodData[lod].snapConfig.sourceMesh;
                bool bFound = false;
                for (const auto& partLodGenerationDataInner : config.RigPartLodGenerationData())
                {
                    for (size_t lodInner = 0; lodInner < partLodGenerationDataInner.second.lodData.size(); ++lodInner)
                    {
                        if (partLodGenerationDataInner.second.lodData[lodInner].meshName == snapConfigSourceMeshName)
                        {
                            bFound = true;
                            break;
                        }
                        if (bFound)
                        {
                            break;
                        }
                    }
                }

                if (bFound)
                {
                    if (partLodGenerationData.second.lodData[lod].snapConfig.sourceVertexIndices.size() != partLodGenerationData.second.lodData[lod].snapConfig.targetVertexIndices.size())
                    {
                        LOG_ERROR("source and target vertices for snap config for mesh {} contain different numbers of indices", m_allMeshes.at(partLodGenerationData.second.lodData[lod].meshName));
                        return false;
                    }

                    // check vertex indices are in range for source and target meshes
                    const auto& srcMesh = m_allMeshes.at(snapConfigSourceMeshName);
                    const auto& targetMesh = m_allMeshes.at(partLodGenerationData.second.lodData[lod].meshName);
                    for (size_t i = 0; i < partLodGenerationData.second.lodData[lod].snapConfig.sourceVertexIndices.size(); ++i)
                    {
                        if (partLodGenerationData.second.lodData[lod].snapConfig.sourceVertexIndices[i] >= srcMesh->NumVertices())
                        {
                            LOG_ERROR("snap config for mesh {} contains an index {} which is out of range for the {} src mesh", partLodGenerationData.second.lodData[lod].meshName, i, snapConfigSourceMeshName);
                            return false;
                        }
                        if (partLodGenerationData.second.lodData[lod].snapConfig.targetVertexIndices[i] >= targetMesh->NumVertices())
                        {
                            LOG_ERROR("snap config for mesh {} contains an index {} which is out of range for the target mesh", snapConfigSourceMeshName, i);
                            return false;
                        }
                    }

                    m_snapConfigs[curMeshName] = partLodGenerationData.second.lodData[lod].snapConfig;
                }
                else
                {
                    LOG_ERROR("failed to find matching snap config mesh {}", snapConfigSourceMeshName);
                    return false;
                }
            }
        };
    }

    return true;
}

template <class T>
void LodGeneration<T>::GetDriverMeshClosestPointBarycentricCoordinates(std::map<std::string, std::vector<BarycentricCoordinates<T>>>& driverMeshClosestPointBarycentricCoordinates) const
{
    driverMeshClosestPointBarycentricCoordinates.clear();
    for (const auto& deformer : m_wrapDeformers)
    {
        deformer.second.GetDriverMeshClosestPointBarycentricCoordinates(driverMeshClosestPointBarycentricCoordinates[deformer.first]);
    }
}



template <class T>
bool LodGeneration<T>::Apply(std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0MeshVertices, std::map<std::string, Eigen::Matrix<T, 3, -1>>& higherLodMeshVertices, bool bAllowMissingMeshes) const
{
    // go through all lod0 meshes and generate the lower lod results from them
    if (!bAllowMissingMeshes)
    {
        if (m_baseMeshes.size() != lod0MeshVertices.size())
        {
            LOG_ERROR("lod0 meshes do not match lod generation config meshes");
            return false;
        }

        for (size_t i = 0; i < m_baseMeshes.size(); ++i)
        {
            if (lod0MeshVertices.find(m_baseMeshes[i]) == lod0MeshVertices.end())
            {
                LOG_ERROR("input data to applying lods is missing mesh {}", m_baseMeshes[i]);
                return false;
            }
        }
    }

    // initialize the results vertices and iterators to the wrap deformers so they exist before we run the deformers in parallel
    std::vector<const WrapDeformer<T>*> wrapDeformerPtrs;
    std::vector<std::string> deformerNames;
    for (const auto& deformer : m_wrapDeformers)
    {
        higherLodMeshVertices[deformer.first] = {};
        wrapDeformerPtrs.push_back(&deformer.second);
        deformerNames.push_back(deformer.first);
    }

    auto runDeformers = [&](int start, int end)
    {
        for (int deformerIndex = start; deformerIndex < end; ++deformerIndex)
        {
            wrapDeformerPtrs[deformerIndex]->Deform(lod0MeshVertices.at(m_driverMeshNames.at(deformerNames[deformerIndex])), higherLodMeshVertices.at(deformerNames[deformerIndex]));
        }
    };

    m_taskThreadPool->AddTaskRangeAndWait((int)m_wrapDeformers.size(), runDeformers);


    // go through again and apply the snap configs
    std::vector<const SnapConfig<T>*> snapConfigPtrs;
    std::vector<std::string> snapConfigNames;
    for (const auto& snapConfig : m_snapConfigs)
    {
        snapConfigPtrs.push_back(&snapConfig.second);
        snapConfigNames.push_back(snapConfig.first);
    }

    auto runSnapConfigs = [&](int start, int end) 
    {
        for (int snapConfigIndex = start; snapConfigIndex < end; ++snapConfigIndex)
        {
            Eigen::Matrix<T, 3, -1> sourceVertices;
            if (std::find(m_baseMeshes.begin(), m_baseMeshes.end(), snapConfigPtrs[snapConfigIndex]->sourceMesh) != m_baseMeshes.end())
            {
                sourceVertices = lod0MeshVertices.at(snapConfigPtrs[snapConfigIndex]->sourceMesh);
            }
            else
            {
                sourceVertices = higherLodMeshVertices.at(snapConfigPtrs[snapConfigIndex]->sourceMesh);
            }

            if (std::find(m_baseMeshes.begin(), m_baseMeshes.end(), snapConfigNames[snapConfigIndex]) != m_baseMeshes.end())
            {
                snapConfigPtrs[snapConfigIndex]->Apply(sourceVertices, lod0MeshVertices.at(snapConfigNames[snapConfigIndex]));
            }
            else
            {
                snapConfigPtrs[snapConfigIndex]->Apply(sourceVertices, higherLodMeshVertices.at(snapConfigNames[snapConfigIndex]));
            }
        }
    };

    m_taskThreadPool->AddTaskRangeAndWait((int)m_snapConfigs.size(), runSnapConfigs);


    return true;
}

template <class T>
std::vector<std::string> LodGeneration<T>::HigherLodMeshNames() const
{
    std::vector<std::string> higherLodMeshNames;
    higherLodMeshNames.reserve(m_wrapDeformers.size());
    for (const auto& pair : m_wrapDeformers)
    {
        higherLodMeshNames.push_back(pair.first);
    }
    return higherLodMeshNames;
}

template <class T>
int LodGeneration<T>::LodForMesh(const std::string& meshName) const
{
    return m_meshLods.at(meshName);
}



template <class T>
bool LodGeneration<T>::SaveModelBinary(const std::string& lodGenerationModelFile) const
{
    FILE* pFile = nullptr;
#ifdef _MSC_VER
    fopen_s(&pFile, lodGenerationModelFile.c_str(), "wb");
#else
    pFile = fopen(lodGenerationModelFile.c_str(), "wb");
#endif
    bool bSuccess = true;
    if (pFile)
    {
        bSuccess &= io::ToBinaryFile(pFile, m_version);
        bSuccess &= io::ToBinaryFile(pFile, m_baseMeshes);
        bSuccess &= io::ToBinaryFile(pFile, m_driverMeshNames);
        // reset all the meshes in the wrapDeformers before saving
        auto wrapDeformers = m_wrapDeformers;
        for (auto& deformer : wrapDeformers)
        {
            deformer.second.SetMeshes(nullptr, nullptr);
        }
        bSuccess &= io::ToBinaryFile(pFile, wrapDeformers);
        bSuccess &= io::ToBinaryFile(pFile, m_snapConfigs);
        bSuccess &= io::ToBinaryFile(pFile, m_meshLods);
        bSuccess &= io::ToBinaryFile(pFile, m_allMeshes);
        fclose(pFile);
    }
    else
    {
        bSuccess = false;
    }

    return bSuccess;
}

template <class T>
bool LodGeneration<T>::LoadModelBinary(const std::string& lodGenerationModelFile)
{
    FILE* pFile = nullptr;
#ifdef _MSC_VER
    fopen_s(&pFile, lodGenerationModelFile.c_str(), "rb");
#else
    pFile = fopen(lodGenerationModelFile.c_str(), "rb");
#endif
    bool bSuccess = true;
    if (pFile)
    {
        int32_t version;
        bSuccess &= io::FromBinaryFile(pFile, version);
        if (bSuccess && version == 2)
        {
            bSuccess &= io::FromBinaryFile(pFile, m_baseMeshes);
            bSuccess &= io::FromBinaryFile(pFile, m_driverMeshNames);
            bSuccess &= io::FromBinaryFile(pFile, m_wrapDeformers);
            bSuccess &= io::FromBinaryFile(pFile, m_snapConfigs);
            bSuccess &= io::FromBinaryFile(pFile, m_meshLods);
            std::map<std::string, std::shared_ptr<Mesh<T>>> allMeshes;
            bSuccess &= io::FromBinaryFile(pFile, allMeshes); 
            m_allMeshes.clear();
            for (const auto& [key, value] : allMeshes)
            {
                m_allMeshes[key] = std::static_pointer_cast<const Mesh<T>>(value);
            }
 
            // set the meshes in the wrapDeformers
            for (auto& deformer : m_wrapDeformers)
            {
                std::shared_ptr<const Mesh<T>> driverMesh = m_allMeshes[m_driverMeshNames.at(deformer.first)];
                std::shared_ptr<const Mesh<T>> wrappedMesh = m_allMeshes[deformer.first];
                deformer.second.SetMeshes(driverMesh, wrappedMesh);
            }
        }
        else if (bSuccess && version == 1)
        {
            bSuccess = false;
            LOG_ERROR("not supporting back-compatible IO for version 1 of LodGeneration object as not released to end users");
        }
        else
        {
            bSuccess = false;
        }
        fclose(pFile);
    }
    else
    {
        bSuccess = false;
    }

    return bSuccess;
}

template class LodGeneration<float>;
template class LodGeneration<double>;

template class LodGenerationConfiguration<float>;
template class LodGenerationConfiguration<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
