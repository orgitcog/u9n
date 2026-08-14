// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/AssetGeneration.h>
#include <rig/Rig.h>
#include <dna/BinaryStreamReader.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool AssetGenerationConfiguration<T>::Load(const std::string& filenameOrData)
{
    const bool isValidFile = std::filesystem::exists(filenameOrData);
    if (isValidFile)
    {
        // filenameOrData points to a file
        const std::string filedata = ReadFile(filenameOrData);
        const std::string baseDir = std::filesystem::absolute(std::filesystem::path(filenameOrData)).parent_path().string();
        return LoadJson(filedata, baseDir);
    }
    else
    {
        // assume filenameOrData is the data directly
        return LoadJson(filenameOrData, "");
    }
}

template <class T>
bool AssetGenerationConfiguration<T>::LoadJson(const std::string& jsonString, const std::string& baseDir) 
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

    try
    {
        const JsonElement j = ReadJson(jsonString);

        if (j.Contains("asset_generation"))
        {
            const auto& config = j["asset_generation"];
            if (config.Contains("archetype") && config["archetype"].IsString())
            {
                m_archetypePath = makeAbsolute(config["archetype"].String());
            }
            else
            {
                LOG_ERROR("failed to find \"archetype\" field in the asset generation configuration");
                return false;
            }

            if (config.Contains("saliva") && config["saliva"].IsObject())
            {
                if (config["saliva"].Contains("mesh") && config["saliva"]["mesh"].IsString())
                {
                    m_salivaMeshName = config["saliva"]["mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"saliva\" \"mesh\" parameter in the asset generation configuration");
                    return false;
                }
                                
                if (config["saliva"].Contains("driver_mesh") && config["saliva"]["driver_mesh"].IsString())
                {
                    m_salivaDriverMeshName = config["saliva"]["driver_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"saliva\" \"driver_mesh\" parameter in the asset generation configuration");
                    return false;
                }
                  
                if (config["saliva"].Contains("wrap_deformer_params") && config["saliva"]["wrap_deformer_params"].IsObject())
                {
                    const bool bLoadedWrapDeformerParams = m_salivaWrapDeformerParams.ReadJson(config["saliva"]["wrap_deformer_params"]);
                    if (!bLoadedWrapDeformerParams)
                    {
                        LOG_ERROR("failed to load \"saliva\" \"wrap_deformer_params\" section in the asset generation configuration");
                        return false;
                    }
                }
                else
                {
                    LOG_ERROR("failed to find \"saliva\" \"wrap_deformer_params\" section in the asset generation configuration");
                    return false;
                }

            }
            else
            {
                LOG_ERROR("failed to find \"saliva\" section in the asset generation configuration");
                return false;
            }
            
            if (config.Contains("cartilage") && config["cartilage"].IsObject())
            {
                if (config["cartilage"].Contains("mesh") && config["cartilage"]["mesh"].IsString())
                {
                    m_cartilageMeshName = config["cartilage"]["mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"cartilage\" \"mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["cartilage"].Contains("driver_mesh") && config["cartilage"]["driver_mesh"].IsString())
                {
                    m_cartilageDriverMeshName = config["cartilage"]["driver_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"cartilage\" \"driver_mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["cartilage"].Contains("wrap_deformer_params") && config["cartilage"]["wrap_deformer_params"].IsObject())
                {
                    const bool bLoadedWrapDeformerParams = m_cartilageWrapDeformerParams.ReadJson(config["cartilage"]["wrap_deformer_params"]);
                    if (!bLoadedWrapDeformerParams)
                    {
                        LOG_ERROR("failed to load \"cartilage\" \"wrap_deformer_params\" section in the asset generation configuration");
                        return false;
                    }
                }
                else
                {
                    LOG_ERROR("failed to find \"cartilage\" \"wrap_deformer_params\" section in the asset generation configuration");
                    return false;
                }
            }
            else
            {
                LOG_ERROR("failed to find \"cartilage\" section in the asset generation configuration");
                return false;
            }

            
            // eyelashes
            if (config.Contains("eyelashes") && config["eyelashes"].IsObject())
            {
                if (config["eyelashes"].Contains("mesh") && config["eyelashes"]["mesh"].IsString())
                {
                    m_eyelashesMeshName = config["eyelashes"]["mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"eyelashes\" \"mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["eyelashes"].Contains("driver_mesh") && config["eyelashes"]["driver_mesh"].IsString())
                {
                    m_eyelashesDriverMeshName = config["eyelashes"]["driver_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"eyelashes\" \"driver_mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["eyelashes"].Contains("eyelashes_generator_params") && config["eyelashes"]["eyelashes_generator_params"].IsObject())
                {
                    const bool bLoadedEyelashesParams = m_eyelashesGeneratorParams.ReadJson(config["eyelashes"]["eyelashes_generator_params"]);
                    if (!bLoadedEyelashesParams)
                    {
                        LOG_ERROR("failed to load \"eyelashes\" \"eyelashes_generator_params\" section in the asset generation configuration");
                        return false;
                    }
                }
                else
                {
                    LOG_ERROR("failed to find \"eyelashes\" \"eyelashes_generator_params\" section in the asset generation configuration");
                    return false;
                }
            }
            else
            {
                LOG_ERROR("failed to find \"eyelashes\" section in the asset generation configuration");
                return false;
            }

            // eye assets
            if (config.Contains("eye_assets") && config["eye_assets"].IsObject())
            {

                if (config["eye_assets"].Contains("head_mesh") && config["eye_assets"]["head_mesh"].IsString())
                {
                    m_headMeshName = config["eye_assets"]["head_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"eye_assets\" \"head_mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["eye_assets"].Contains("eye_left_mesh") && config["eye_assets"]["eye_left_mesh"].IsString())
                {
                    m_eyeLeftMeshName = config["eye_assets"]["eye_left_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"eye_assets\" \"eye_left_mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["eye_assets"].Contains("eye_right_mesh") && config["eye_assets"]["eye_right_mesh"].IsString())
                {
                    m_eyeRightMeshName = config["eye_assets"]["eye_right_mesh"].String();
                }
                else
                {
                    LOG_ERROR("failed to find \"eye_assets\" \"eye_right_mesh\" parameter in the asset generation configuration");
                    return false;
                }

                if (config["eye_assets"].Contains("eyeEdge") && config["eye_assets"]["eyeEdge"].IsObject())
                {
                    if (config["eye_assets"]["eyeEdge"].Contains("mesh") && config["eye_assets"]["eyeEdge"]["mesh"].IsString())
                    {
                        m_eyeEdgeMeshName = config["eye_assets"]["eyeEdge"]["mesh"].String();
                    }
                    else
                    {
                        LOG_ERROR("failed to find \"eye_assets\" \"eyeEdge\" \"mesh\" parameter in the asset generation configuration");
                        return false;
                    }
                    
                    if (config["eye_assets"]["eyeEdge"].Contains("params") && config["eye_assets"]["eyeEdge"]["params"].IsObject())
                    {
                        const bool bLoadedEyeEdgeParams = m_eyeEdgeGeneratorParams.ReadJson(config["eye_assets"]["eyeEdge"]["params"]);
                        if (!bLoadedEyeEdgeParams)
                        {
                            LOG_ERROR("failed to load \"eye_assets\" \"eyeEdge\" \"params\" section in the asset generation configuration");
                            return false;
                        }
                    }
                    else
                    {
                        LOG_ERROR("failed to find \"eye_assets\" \"eyeEdge\" \"params\" section in the asset generation configuration");
                        return false;
                    }
                }
                else
                {
                    LOG_ERROR("failed to find \"eye_assets\" \"eyeEdge\" section in the asset generation configuration");
                    return false;
                }

                if (config["eye_assets"].Contains("eyeshell") && config["eye_assets"]["eyeshell"].IsObject())
                {
                    if (config["eye_assets"]["eyeshell"].Contains("mesh") && config["eye_assets"]["eyeshell"]["mesh"].IsString())
                    {
                        m_eyeshellMeshName = config["eye_assets"]["eyeshell"]["mesh"].String();
                    }
                    else
                    {
                        LOG_ERROR("failed to find \"eye_assets\" \"eyeshell\" \"mesh\" parameter in the asset generation configuration");
                        return false;
                    }

                    if (config["eye_assets"]["eyeshell"].Contains("params") && config["eye_assets"]["eyeshell"]["params"].IsObject())
                    {
                        const bool bLoadedEyeshellParams = m_eyeshellGeneratorParams.ReadJson(config["eye_assets"]["eyeshell"]["params"]);
                        if (!bLoadedEyeshellParams)
                        {
                            LOG_ERROR("failed to load \"eye_assets\" \"eyeshell\" \"params\" section in the asset generation configuration");
                            return false;
                        }
                    }
                    else
                    {
                        LOG_ERROR("failed to find \"eye_assets\" \"eyeshell\" \"params\" section in the asset generation configuration");
                        return false;
                    }
                }
                else
                {
                    LOG_ERROR("failed to find \"eye_assets\" \"eyeshell\" section in the asset generation configuration");
                    return false;
                }
            }
            else
            {
                LOG_ERROR("failed to find \"eye_assets\" section in the asset generation configuration");
                return false;
            }
        }
        else
        {
            LOG_ERROR("failed to find \"asset_generation\" field in the asset generation configuration");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("failure to load asset generation configuration: {}", e.what());
        return false;
    }

    return true;
}

template <class T>
void TriangulateIfNeeded(Mesh<T> & mesh)
{
    if (mesh.NumQuads() > 0)
    {
        mesh.Triangulate();
    }
}


template <class T>
AssetGeneration<T>::AssetGeneration()
{
    m_taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
}

template <class T>
void AssetGeneration<T>::SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool)
{
    m_taskThreadPool = taskThreadPool;
}


template <class T>
bool AssetGeneration<T>::Init(const AssetGenerationConfiguration<T>& config)
{
    m_config = config;

    // load in the archetype
    auto archetypeDnaStream = pma::makeScoped<dna::FileStream>(config.m_archetypePath.c_str(), dna::FileStream::AccessMode::Read, dna::FileStream::OpenMode::Binary);
    auto archetypeDnaReader = pma::makeScoped<dna::BinaryStreamReader>(archetypeDnaStream.get(), dna::DataLayer::All);
    archetypeDnaReader->read();

    Rig<T> archetypeRig;
    if (!archetypeRig.LoadRig(archetypeDnaReader.get(), /*withJointScaling=*/true))
    {
        LOG_ERROR("could not read archetype dna: ", config.m_archetypePath);
        return false;
    }

    const std::shared_ptr<const RigGeometry<T>>& archetypeRigGeometry = archetypeRig.GetRigGeometry();

    // get the driver and wrap mesh for cartilage and saliva 
    int cartilageDriverMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_cartilageDriverMeshName);
    if (cartilageDriverMeshIndex == -1)
    {
        LOG_ERROR("cartilage driver mesh {} does not exist in the archetype ", config.m_cartilageDriverMeshName);
        return false;
    }

    int cartilageMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_cartilageMeshName);
    if (cartilageMeshIndex == -1)
    {
        LOG_ERROR("cartilage mesh {} does not exist in the archetype ", config.m_cartilageMeshName);
        return false;
    }

    std::shared_ptr<Mesh<T>> cartilageDriverMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(cartilageDriverMeshIndex));
    m_allMeshes[config.m_cartilageDriverMeshName] = cartilageDriverMesh;
    TriangulateIfNeeded(*cartilageDriverMesh);
    std::shared_ptr<Mesh<T>> cartilageMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(cartilageMeshIndex));
    m_allMeshes[config.m_cartilageMeshName] = cartilageMesh;
    TriangulateIfNeeded(*cartilageMesh);
    m_cartilageWrapDeformer.Init(cartilageDriverMesh, cartilageMesh, config.m_cartilageWrapDeformerParams);

    int salivaDriverMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_salivaDriverMeshName);
    if (salivaDriverMeshIndex == -1)
    {
        LOG_ERROR("saliva driver mesh {} does not exist in the archetype ", config.m_salivaDriverMeshName);
        return false;
    }

    int salivaMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_salivaMeshName);
    if (salivaMeshIndex == -1)
    {
        LOG_ERROR("saliva mesh {} does not exist in the archetype ", config.m_salivaMeshName);
        return false;
    }

    std::shared_ptr<const Mesh<T>> salivaDriverMesh;
    if ( m_allMeshes.find(config.m_salivaDriverMeshName) != m_allMeshes.end())
    {
        salivaDriverMesh = m_allMeshes.at(config.m_salivaDriverMeshName);
    }
    else
    {
        std::shared_ptr< Mesh<T>> newSalivaDriverMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(salivaDriverMeshIndex));
        m_allMeshes[config.m_salivaDriverMeshName] = newSalivaDriverMesh;
        TriangulateIfNeeded(*newSalivaDriverMesh);
        salivaDriverMesh = newSalivaDriverMesh;
    }

    std::shared_ptr<Mesh<T>> salivaMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(salivaMeshIndex));
    m_allMeshes[config.m_salivaMeshName] = salivaMesh;
    TriangulateIfNeeded(*salivaMesh);
    m_salivaWrapDeformer.Init(salivaDriverMesh, salivaMesh, config.m_salivaWrapDeformerParams);

    // eyelashes
    // get the driver and wrap mesh for eyelashes
    int eyelashesDriverMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyelashesDriverMeshName);
    if (eyelashesDriverMeshIndex == -1)
    {
        LOG_ERROR("eyelashes driver mesh {} does not exist in the archetype ", config.m_eyelashesDriverMeshName);
        return false;
    }

    int eyelashesMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyelashesMeshName);
    if (eyelashesMeshIndex == -1)
    {
        LOG_ERROR("eyelashes mesh {} does not exist in the archetype ", config.m_eyelashesMeshName);
        return false;
    }

    std::shared_ptr<const Mesh<T>> eyelashesDriverMesh;
    if (m_allMeshes.find(config.m_eyelashesDriverMeshName) != m_allMeshes.end())
    {
        eyelashesDriverMesh = m_allMeshes.at(config.m_eyelashesDriverMeshName);
    }
    else
    {
        eyelashesDriverMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyelashesDriverMeshIndex));
        m_allMeshes[config.m_eyelashesDriverMeshName] = eyelashesDriverMesh;
    }

    std::shared_ptr<Mesh<T>> eyelashesMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyelashesMeshIndex));
    m_allMeshes[config.m_eyelashesMeshName] = eyelashesMesh;


    // initialize the eyelashes generator
    bool bInitializedEyelashesGenerator = m_eyelashesGenerator.Init(eyelashesDriverMesh, eyelashesMesh, config.m_eyelashesGeneratorParams);
    if (!bInitializedEyelashesGenerator)
    {
        LOG_ERROR("failed to initialize eyelashes generator");
        return false;
    }

    // eye assets ie eyeEdge and eyeshell
    // get all the meshes needed for eyeEdge and eyeshell generation
    int headMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_headMeshName);
    if (headMeshIndex == -1)
    {
        LOG_ERROR("head mesh {} does not exist in the archetype ", config.m_headMeshName);
        return false;
    }

    int eyeLeftMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyeLeftMeshName);
    if (eyeLeftMeshIndex == -1)
    {
        LOG_ERROR("left eye mesh {} does not exist in the archetype ", config.m_eyeLeftMeshName);
        return false;
    }

    int eyeRightMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyeRightMeshName);
    if (eyeRightMeshIndex == -1)
    {
        LOG_ERROR("right eye mesh {} does not exist in the archetype ", config.m_eyeRightMeshName);
        return false;
    }

    int eyeEdgeMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyeEdgeMeshName);
    if (eyeEdgeMeshIndex == -1)
    {
        LOG_ERROR("eyeEdge mesh {} does not exist in the archetype ", config.m_eyeEdgeMeshName);
        return false;
    }

    int eyeshellMeshIndex = archetypeRigGeometry->GetMeshIndex(config.m_eyeshellMeshName);
    if (eyeshellMeshIndex == -1)
    {
        LOG_ERROR("eyeshell mesh {} does not exist in the archetype ", config.m_eyeshellMeshName);
        return false;
    }

    std::shared_ptr<const Mesh<T>> headMesh;
    if (m_allMeshes.find(config.m_headMeshName) != m_allMeshes.end())
    {
        headMesh = m_allMeshes.at(config.m_headMeshName);
    }
    else
    {
        std::shared_ptr<Mesh<T>> newHeadMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(headMeshIndex));
        TriangulateIfNeeded(*newHeadMesh);
        m_allMeshes[config.m_headMeshName] = newHeadMesh;
        headMesh = newHeadMesh;
    }

    std::shared_ptr<const Mesh<T>> eyeLeftMesh;
    if (m_allMeshes.find(config.m_eyeLeftMeshName) != m_allMeshes.end())
    {
        eyeLeftMesh = m_allMeshes.at(config.m_eyeLeftMeshName);
    }
    else
    {
        std::shared_ptr<Mesh<T>> newEyeLeftMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyeLeftMeshIndex));
        m_allMeshes[config.m_eyeLeftMeshName] = newEyeLeftMesh;
        eyeLeftMesh = newEyeLeftMesh;
    }

    std::shared_ptr<const Mesh<T>> eyeRightMesh;
    if (m_allMeshes.find(config.m_eyeRightMeshName) != m_allMeshes.end())
    {
        eyeRightMesh = m_allMeshes.at(config.m_eyeRightMeshName);
    }
    else
    {
        std::shared_ptr<Mesh<T>> newEyeRightMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyeRightMeshIndex));
        m_allMeshes[config.m_eyeRightMeshName] = newEyeRightMesh;
        eyeRightMesh = newEyeRightMesh;
    }

    std::shared_ptr<Mesh<T>> eyeEdgeMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyeEdgeMeshIndex));
    m_allMeshes[config.m_eyeEdgeMeshName] = eyeEdgeMesh;
    TriangulateIfNeeded(*eyeEdgeMesh);
    std::shared_ptr<Mesh<T>> eyeshellMesh = std::make_shared<Mesh<T>>(archetypeRigGeometry->GetMesh(eyeshellMeshIndex));
    m_allMeshes[config.m_eyeshellMeshName] = eyeshellMesh;
    TriangulateIfNeeded(*eyeshellMesh);

    // initialize the eye assets generators
    m_eyeEdgeGenerator.Init(headMesh, eyeLeftMesh, eyeRightMesh, eyeEdgeMesh, config.m_eyeEdgeGeneratorParams);
    m_eyeshellGenerator.Init(headMesh, eyeLeftMesh, eyeRightMesh, eyeshellMesh, config.m_eyeshellGeneratorParams);
 
    return true;
}

template <class T>
bool AssetGeneration<T>::Apply(const std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0HeadEyeTeethMeshVertices, std::map<std::string, Eigen::Matrix<T, 3, -1>>& lod0AssetVertices) const
{
    lod0AssetVertices.clear();

    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_salivaDriverMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_salivaDriverMeshName);
        return false;
    }

    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_cartilageDriverMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_cartilageDriverMeshName);
        return false;
    }
 
    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_eyelashesDriverMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_eyelashesDriverMeshName);
        return false;
    }

    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_headMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_headMeshName);
        return false;
    }
    
    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_eyeLeftMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_eyeLeftMeshName);
        return false;
    }
    if (lod0HeadEyeTeethMeshVertices.find(m_config.m_eyeRightMeshName) == lod0HeadEyeTeethMeshVertices.end())
    {
        LOG_ERROR("input data for asset generation does not contain required mesh {}", m_config.m_eyeRightMeshName);
        return false;
    }

    // apply the assets in parallel

    // first make sure the outputs exist
    lod0AssetVertices[m_config.m_salivaMeshName] = {};
    lod0AssetVertices[m_config.m_cartilageMeshName] = {};
    lod0AssetVertices[m_config.m_eyelashesMeshName] = {};
    lod0AssetVertices[m_config.m_eyeEdgeMeshName] = {};
    lod0AssetVertices[m_config.m_eyeshellMeshName] = {};

    TITAN_NAMESPACE::TaskFutures futures;

    futures.Add(m_taskThreadPool->AddTask([&]()
        { 
            m_salivaWrapDeformer.Deform(lod0HeadEyeTeethMeshVertices.at(m_config.m_salivaDriverMeshName), lod0AssetVertices.at(m_config.m_salivaMeshName)); 
            m_cartilageWrapDeformer.Deform(lod0HeadEyeTeethMeshVertices.at(m_config.m_cartilageDriverMeshName), lod0AssetVertices.at(m_config.m_cartilageMeshName));
            m_eyelashesGenerator.Apply(lod0HeadEyeTeethMeshVertices.at(m_config.m_eyelashesDriverMeshName), lod0AssetVertices.at(m_config.m_eyelashesMeshName));
        }));

    futures.Add(m_taskThreadPool->AddTask([&]()
        {
            m_eyeEdgeGenerator.Apply(lod0HeadEyeTeethMeshVertices.at(m_config.m_headMeshName), lod0HeadEyeTeethMeshVertices.at(m_config.m_eyeLeftMeshName), lod0HeadEyeTeethMeshVertices.at(m_config.m_eyeRightMeshName),
              lod0AssetVertices.at(m_config.m_eyeEdgeMeshName));
        }));

    futures.Add(m_taskThreadPool->AddTask([&]()
        {
            m_eyeshellGenerator.Apply(lod0HeadEyeTeethMeshVertices.at(m_config.m_headMeshName), lod0HeadEyeTeethMeshVertices.at(m_config.m_eyeLeftMeshName), lod0HeadEyeTeethMeshVertices.at(m_config.m_eyeRightMeshName),
              lod0AssetVertices.at(m_config.m_eyeshellMeshName));
            }));

    futures.Wait();

    // fast heuristic to fix potential intersections between eyeshell and eyeEdge caruncle region
    EyeAssetGenerator<T>::FixCaruncleIntersection(m_eyeshellGenerator, m_eyeEdgeGenerator, lod0AssetVertices.at(m_config.m_eyeshellMeshName), lod0AssetVertices.at(m_config.m_eyeEdgeMeshName));

    return true;
}

template <class T>
bool AssetGeneration<T>::SaveModelBinary(const std::string& AssetGenerationModelFile) const
{
    FILE* pFile = nullptr;
#ifdef _MSC_VER
    fopen_s(&pFile, AssetGenerationModelFile.c_str(), "wb");
#else
    pFile = fopen(AssetGenerationModelFile.c_str(), "wb");
#endif
    bool bSuccess = true;
    if (pFile)
    {
        bSuccess &= io::ToBinaryFile(pFile, m_version);
        bSuccess &= ToBinaryFile(pFile, m_config);

        // set all the meshes to nullptrs in the asset generation objects to save space
        auto salivaWrapDeformer = m_salivaWrapDeformer;
        salivaWrapDeformer.SetMeshes(nullptr, nullptr);
        auto cartilageWrapDeformer = m_cartilageWrapDeformer;
        cartilageWrapDeformer.SetMeshes(nullptr, nullptr);
        auto eyelashesGenerator = m_eyelashesGenerator;
        eyelashesGenerator.SetMeshes(nullptr, nullptr);
        auto eyeEdgeGenerator = m_eyeEdgeGenerator;
        eyeEdgeGenerator.SetMeshes(nullptr, nullptr, nullptr, nullptr);
        auto eyeshellGenerator = m_eyeshellGenerator;
        eyeshellGenerator.SetMeshes(nullptr, nullptr, nullptr, nullptr);

        bSuccess &= ToBinaryFile(pFile, salivaWrapDeformer);
        bSuccess &= ToBinaryFile(pFile, cartilageWrapDeformer);
        bSuccess &= ToBinaryFile(pFile, eyelashesGenerator);
        bSuccess &= ToBinaryFile(pFile, eyeEdgeGenerator);
        bSuccess &= ToBinaryFile(pFile, eyeshellGenerator);
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
bool AssetGeneration<T>::LoadModelBinary(const std::string& AssetGenerationModelFile)
{
    FILE* pFile = nullptr;
#ifdef _MSC_VER
    fopen_s(&pFile, AssetGenerationModelFile.c_str(), "rb");
#else
    pFile = fopen(AssetGenerationModelFile.c_str(), "rb");
#endif
    bool bSuccess = true;
    if (pFile)
    {
        int32_t version;
        bSuccess &= io::FromBinaryFile(pFile, version);
        if (bSuccess && version == 1)
        {
            bSuccess &= FromBinaryFile(pFile, m_config);
            bSuccess &= FromBinaryFile(pFile, m_salivaWrapDeformer);
            bSuccess &= FromBinaryFile(pFile, m_cartilageWrapDeformer);
            bSuccess &= FromBinaryFile(pFile, m_eyelashesGenerator);
            bSuccess &= FromBinaryFile(pFile, m_eyeEdgeGenerator);
            bSuccess &= FromBinaryFile(pFile, m_eyeshellGenerator);
            std::map<std::string, std::shared_ptr<Mesh<T>>> allMeshes;
            bSuccess &= io::FromBinaryFile(pFile, allMeshes);
            m_allMeshes.clear();
            for (const auto& [key, value] : allMeshes)
            {
                m_allMeshes[key] = std::static_pointer_cast<const Mesh<T>>(value);
            }

            // set the meshes in the asset generation objects to those in m_allMeshes
            m_salivaWrapDeformer.SetMeshes(m_allMeshes[m_config.m_salivaDriverMeshName], m_allMeshes[m_config.m_salivaMeshName]);
            m_cartilageWrapDeformer.SetMeshes(m_allMeshes[m_config.m_cartilageDriverMeshName], m_allMeshes[m_config.m_cartilageMeshName]);
            m_eyelashesGenerator.SetMeshes(m_allMeshes[m_config.m_eyelashesDriverMeshName], m_allMeshes[m_config.m_eyelashesMeshName]);
            m_eyeEdgeGenerator.SetMeshes(m_allMeshes[m_config.m_headMeshName], m_allMeshes[m_config.m_eyeLeftMeshName], m_allMeshes[m_config.m_eyeRightMeshName], m_allMeshes[m_config.m_eyeEdgeMeshName]);
            m_eyeshellGenerator.SetMeshes(m_allMeshes[m_config.m_headMeshName], m_allMeshes[m_config.m_eyeLeftMeshName], m_allMeshes[m_config.m_eyeRightMeshName], m_allMeshes[m_config.m_eyeshellMeshName]);

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

template <class T>
bool ToBinaryFile(FILE* pFile, const AssetGenerationConfiguration<T> & config)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, config.m_version);
    success &= io::ToBinaryFile(pFile, config.m_archetypePath);
    success &= io::ToBinaryFile(pFile, config.m_salivaDriverMeshName);
    success &= io::ToBinaryFile(pFile, config.m_salivaMeshName);
    success &= ToBinaryFile(pFile, config.m_salivaWrapDeformerParams);
    success &= io::ToBinaryFile(pFile, config.m_cartilageDriverMeshName);
    success &= io::ToBinaryFile(pFile, config.m_cartilageMeshName);
    success &= ToBinaryFile(pFile, config.m_cartilageWrapDeformerParams);
    success &= io::ToBinaryFile(pFile, config.m_eyelashesDriverMeshName);
    success &= io::ToBinaryFile(pFile, config.m_eyelashesMeshName);
    success &= ToBinaryFile(pFile, config.m_eyelashesGeneratorParams);
    success &= io::ToBinaryFile(pFile, config.m_headMeshName);
    success &= io::ToBinaryFile(pFile, config.m_eyeLeftMeshName);
    success &= io::ToBinaryFile(pFile, config.m_eyeRightMeshName);
    success &= io::ToBinaryFile(pFile, config.m_eyeEdgeMeshName);
    success &= io::ToBinaryFile(pFile, config.m_eyeshellMeshName);
    success &= ToBinaryFile(pFile, config.m_eyeEdgeGeneratorParams);
    success &= ToBinaryFile(pFile, config.m_eyeshellGeneratorParams);

    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, AssetGenerationConfiguration<T>& config)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile<int32_t>(pFile, version);
    if (success && version == 1)
    {
        success &= io::FromBinaryFile(pFile, config.m_archetypePath);
        success &= io::FromBinaryFile(pFile, config.m_salivaDriverMeshName);
        success &= io::FromBinaryFile(pFile, config.m_salivaMeshName);
        success &= FromBinaryFile(pFile, config.m_salivaWrapDeformerParams);
        success &= io::FromBinaryFile(pFile, config.m_cartilageDriverMeshName);
        success &= io::FromBinaryFile(pFile, config.m_cartilageMeshName);
        success &= FromBinaryFile(pFile, config.m_cartilageWrapDeformerParams);
        success &= io::FromBinaryFile(pFile, config.m_eyelashesDriverMeshName);
        success &= io::FromBinaryFile(pFile, config.m_eyelashesMeshName);
        success &= FromBinaryFile(pFile, config.m_eyelashesGeneratorParams);
        success &= io::FromBinaryFile(pFile, config.m_headMeshName);
        success &= io::FromBinaryFile(pFile, config.m_eyeLeftMeshName);
        success &= io::FromBinaryFile(pFile, config.m_eyeRightMeshName);
        success &= io::FromBinaryFile(pFile, config.m_eyeEdgeMeshName);
        success &= io::FromBinaryFile(pFile, config.m_eyeshellMeshName);
        success &= FromBinaryFile(pFile, config.m_eyeEdgeGeneratorParams);
        success &= FromBinaryFile(pFile, config.m_eyeshellGeneratorParams);
    }
    else
    {
        success = false;
    }
    return success;
}


template class AssetGeneration<float>;
template class AssetGeneration<double>;

template class AssetGenerationConfiguration<float>;
template class AssetGenerationConfiguration<double>;

template bool FromBinaryFile(FILE* pFile, AssetGenerationConfiguration<float>& config);
template bool FromBinaryFile(FILE* pFile, AssetGenerationConfiguration<double>& config);
template bool ToBinaryFile(FILE* pFile, const AssetGenerationConfiguration<float>& config);
template bool ToBinaryFile(FILE* pFile, const AssetGenerationConfiguration<double>& config);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)

