// Copyright Epic Games, Inc. All Rights Reserved.

#include <rigcalibration/ModelData.h>

#include <nrr/RigFitting.h>
#include <nls/geometry/GeometryHelpers.h>
#include <nls/serialization/BinarySerialization.h>

#include <rig/RigUtils.h>

#include <carbon/io/Utils.h>
#include <carbon/common/Log.h>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/utils/Timer.h>

#include <filesystem>
#include <mutex>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

std::shared_ptr<IdentityBlendModel<float, -1>> ConcatenateModels(const std::shared_ptr<IdentityBlendModel<float, -1>>& a, const std::shared_ptr<IdentityBlendModel<float, -1>>& b)
{
    const int newModelDim = a->NumVertices() + b->NumVertices();
    const int numRegions = a->NumRegions();

    Eigen::Matrix<float, -1, -1> newMean = Eigen::Matrix<float, -1, -1>::Zero(a->Base().rows(), newModelDim);
    std::vector<typename IdentityBlendModel<float, -1>::RegionData> newRegionData(numRegions);

    newMean.leftCols(a->NumVertices()) = a->Base();
    newMean.rightCols(b->NumVertices()) = b->Base();

    for (int r = 0; r < numRegions; ++r)
    {
        Eigen::Matrix<float, -1, -1> concatenatedModes = Eigen::Matrix<float, -1, -1>::Zero(a->RegionModes(r).rows() + b->RegionModes(r).rows(), a->RegionModes(r).cols());
        concatenatedModes.topRows(a->RegionModes(r).rows()) = a->RegionModes(r);
        concatenatedModes.bottomRows(b->RegionModes(r).rows()) = b->RegionModes(r);

        auto fixedVtxIds = a->RegionVertexIds(r);
        auto updatedVtxIds = b->RegionVertexIds(r).array() + a->NumVertices();

        Eigen::VectorXi concatenatedIds(fixedVtxIds.size() + updatedVtxIds.size());
        concatenatedIds << fixedVtxIds, updatedVtxIds;

        Eigen::VectorXf concatenatedWeights(a->RegionWeights(r).size() + b->RegionWeights(r).size());
        concatenatedWeights << a->RegionWeights(r), b->RegionWeights(r);

        typename IdentityBlendModel<float, -1>::RegionData concatenatedRegion;
        concatenatedRegion.modeNames = a->ModeNames(r);
        concatenatedRegion.regionName = a->RegionName(r);
        concatenatedRegion.modes = std::move(concatenatedModes);
        concatenatedRegion.vertexIDs = std::move(concatenatedIds);
        concatenatedRegion.weights = std::move(concatenatedWeights);
        newRegionData[r] = std::move(concatenatedRegion);
    }
    auto outputModel = std::make_shared<IdentityBlendModel<float, -1>>();
    outputModel->SetModel(std::move(newMean), std::move(newRegionData));
    return outputModel;
}

bool LoadSkinningModelBinary(const std::string& filename,
                             std::map<std::string, SkinningRegionData>& skinningModel,
                             std::vector<std::string>& regionsIncluded)
{
    if (filename.empty())
    {
        CARBON_CRITICAL("Missing file {}. Check your database.", filename);
    }
    skinningModel.clear();
    regionsIncluded.clear();

    FILE* pFile = nullptr;
    #ifdef _MSC_VER
    fopen_s(&pFile, filename.c_str(), "rb");
    #else
    pFile = fopen(filename.c_str(), "rb");
    #endif
    if (pFile)
    {
        int readNumOfRegions = 0;
        size_t itemsRead = fread(&readNumOfRegions, sizeof(readNumOfRegions), 1, pFile);
        if (itemsRead == 0)
        {
            return false;
        }

        bool fileRead = true;
        for (int r = 0; r < readNumOfRegions; r++)
        {
            SkinningRegionData regionData;
            fileRead &= io::FromBinaryFile(pFile, regionData.name);
            fileRead &= io::FromBinaryFile(pFile, regionData.mean);
            fileRead &= io::FromBinaryFile(pFile, regionData.modes);
            fileRead &= io::FromBinaryFile(pFile, regionData.jointMapping);
            fileRead &= io::FromBinaryFile(pFile, regionData.combinedVertexMapping);

            skinningModel[regionData.name] = regionData;
            regionsIncluded.push_back(regionData.name);
        }

        return fileRead;
    }
    else
    {
        return false;
    }
}

bool LoadGeneCodeBinary(const std::string& filename,
                        std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>>& geneCodeMatrix,
                        std::map<std::string,
                        std::map<std::string,
                        std::pair<int,
                        int>>>& expressionRanges)
{
    expressionRanges.clear();
    if (filename.empty())
    {
        CARBON_CRITICAL("Missing file {}. Check your database.", filename);
    }

    FILE* pFile = nullptr;
    #ifdef _MSC_VER
    fopen_s(&pFile, filename.c_str(), "rb");
    #else
    pFile = fopen(filename.c_str(), "rb");
    #endif
    if (pFile)
    {
        int readNumOfRegions = 0;
        size_t itemsRead = fread(&readNumOfRegions, sizeof(readNumOfRegions), 1, pFile);
        if (itemsRead == 0)
        {
            CARBON_CRITICAL("Failed to read binary file {}", filename);
        }

        bool fileRead = true;

        for (int r = 0; r < readNumOfRegions; r++)
        {
            int readNumOfExpressions = 0;
            itemsRead = fread(&readNumOfExpressions, sizeof(readNumOfExpressions), 1, pFile);
            if (itemsRead == 0)
            {
                CARBON_CRITICAL("Failed to read binary file {}", filename);
            }
            std::string regionName;
            fileRead &= io::FromBinaryFile(pFile, regionName);
            fileRead &= io::FromBinaryFile(pFile, geneCodeMatrix[regionName].first);
            fileRead &= io::FromBinaryFile(pFile, geneCodeMatrix[regionName].second);
            for (int e = 0; e < readNumOfExpressions; e++)
            {
                std::string expressionName;
                fileRead &= io::FromBinaryFile(pFile, expressionName);
                Eigen::Vector2i range;
                fileRead &= io::FromBinaryFile(pFile, range);
                expressionRanges[regionName][expressionName] = std::pair<int, int>(range[0], range[1]);
            }
        }

        return fileRead;
    }
    else
    {
        return false;
    }
}

struct ModelData::Private
{
    std::map<std::string, std::shared_ptr<IdentityBlendModel<float, -1>>> models;
    std::shared_ptr<IdentityBlendModel<float>> stabilizationModel;
    std::string neutralName;
    std::string skinningName;
    std::vector<std::string> expressionNames;
    std::string modelVersionIdentifier;

    std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>> geneCodeMatrix;
    std::map<std::string, std::map<std::string, std::pair<int, int>>> geneCodeExprRanges;
    std::map<std::string, SkinningRegionData> skinningModel;
    std::vector<std::string> skinningRegions;

    //! flag whether to cancel initialization
    bool cancelInitialization = false;

    bool initializationFirstPhaseCompleted = false;

    //! future for asynchronous initialization
    mutable TITAN_NAMESPACE::TaskFutures initializationFutures;
    mutable std::mutex mutex;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(true);

    //! wait for initalization to finish i.e. all dnas have been loaded (can be slow)
    void WaitForInitialization() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        initializationFutures.Wait();
    }
};

ModelData::ModelData()
    : m(new Private)
{}

ModelData::~ModelData()
{
    m->cancelInitialization = true;
    m->WaitForInitialization();
}

void ModelData::Set(const std::map<std::string, std::shared_ptr<IdentityBlendModel<float, -1>>>& models,
                    const std::string& neutralName,
                    const std::string& skinningName,
                    const std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>>& geneCode,
                    const std::map<std::string, std::map<std::string, std::pair<int, int>>>& geneCodeExprRange,
                    const std::map<std::string, SkinningRegionData>& skinningModel,
                    const std::shared_ptr<IdentityBlendModel<float>>& stabilizationModel)
{
    m->models = models;
    m->neutralName = neutralName;
    m->skinningName = skinningName;

    if (stabilizationModel)
    {
        m->stabilizationModel = stabilizationModel;
    }

    for (const auto &[name, model] : models)
    {
        m->expressionNames.push_back(name);
    }

    m->geneCodeExprRanges = geneCodeExprRange;
    m->geneCodeMatrix = geneCode;
    m->skinningModel = skinningModel;

    m->initializationFirstPhaseCompleted = true;
}

bool ModelData::Load(const RigCalibrationDatabaseDescription &pcaDatabaseModelHandler, bool loadBlendshapes)
{
    std::vector<std::string> expressionNames = pcaDatabaseModelHandler.GetExpressionModelNames();

    m->modelVersionIdentifier = pcaDatabaseModelHandler.GetModelVersionIdentifer();

    std::lock_guard<std::mutex> lock(m->mutex);
    m->cancelInitialization = true;
    m->initializationFutures.Wait();

    if (pcaDatabaseModelHandler.GetIdentityModelName().empty())
    {
        CARBON_CRITICAL("Model Handler not initialized.");
    }
    LOG_INFO("RigCalibrationData::Load() started.");

    if (!pcaDatabaseModelHandler.GetGeneCodeMatrixFilePath().empty())
    {
        std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>> geneCodeMatrix;
        std::map<std::string, std::map<std::string, std::pair<int, int>>> geneCodeRanges;
        bool genecodeLoaded = LoadGeneCodeBinary(pcaDatabaseModelHandler.GetGeneCodeMatrixFilePath(), geneCodeMatrix, geneCodeRanges);
        if (genecodeLoaded)
        {
            m->geneCodeMatrix = geneCodeMatrix;
            m->geneCodeExprRanges = geneCodeRanges;
        }
        else
        {
            LOG_ERROR("GeneCode model {} failed to load", pcaDatabaseModelHandler.GetGeneCodeMatrixFilePath());
        }
    }

    m->neutralName = pcaDatabaseModelHandler.GetIdentityModelName();

    if (!pcaDatabaseModelHandler.GetSkinningModelName().empty())
    {
        std::map<std::string, SkinningRegionData> skinningModel;
        std::vector<std::string> skinningRegions;

        bool skinningLoaded = LoadSkinningModelBinary(pcaDatabaseModelHandler.GetSkinningModelFilePath(), skinningModel, skinningRegions);
        if (skinningLoaded)
        {
            m->skinningName = pcaDatabaseModelHandler.GetSkinningModelName();
            m->skinningModel = skinningModel;
            m->skinningRegions = skinningRegions;
        }
        else
        {
            LOG_ERROR("Skinning model {} failed to load", pcaDatabaseModelHandler.GetSkinningModelFilePath());
        }
    }

    if (!pcaDatabaseModelHandler.GetStabilizationModelFilePath().empty())
    {
        m->stabilizationModel = std::make_shared<IdentityBlendModel<float>>();
        m->stabilizationModel->LoadModelBinary(pcaDatabaseModelHandler.GetStabilizationModelFilePath());
    }

    const int expressionModelsCount = (int)expressionNames.size();
    m->expressionNames.resize(expressionModelsCount);

    for (int i = 0; i < expressionModelsCount; ++i)
    {
        const std::string modelName = expressionNames[i];
        m->expressionNames[i] = modelName;
        m->models[modelName] = std::make_shared<IdentityBlendModel<float, -1>>();
    }

    m->initializationFirstPhaseCompleted = true;
    m->cancelInitialization = false;

    // load all dna data asynchronously
    for (int i = 0; i < expressionModelsCount; ++i)
    {
        const std::string modelPath = pcaDatabaseModelHandler.GetExpressionModelPath(i);
        const std::string modelName = m->expressionNames[i];

        if (m->models[modelName] == nullptr)
        {
            CARBON_CRITICAL("Allocation failed for model {}", modelName);
        }

        m->initializationFutures.Add(m->taskThreadPool->AddTask(
            [&, modelName, modelPath]()
            {
                if (m->cancelInitialization)
                    return;

                if (m->models[modelName] == nullptr)
                {
                    CARBON_CRITICAL("Allocation failed for model {}", modelName);
                }
                if (!m->models[modelName]->LoadModelBinary(modelPath))
                {
                    CARBON_CRITICAL("Failed to load file {}", modelPath);
                }
            }));
    }

    if (loadBlendshapes)
    {
        for (int i = 0; i < expressionModelsCount; ++i)
        {
            const std::string modelName = m->expressionNames[i];
            const std::string modelPath = pcaDatabaseModelHandler.GetExpressionBlendshapeModelPath(i);
            if (!modelPath.empty())
            {
                m->initializationFutures.Add(m->taskThreadPool->AddTask(
                    [&, modelName, modelPath]()
                    {
                        if (m->cancelInitialization)
                            return;
                        auto blendshapeModel = std::make_shared<IdentityBlendModel<float, -1>>();

                        if (!blendshapeModel->LoadModelBinary(modelPath))
                        {
                            CARBON_CRITICAL("Failed to load file {}", modelPath);
                        }
                        m->models[modelName] = ConcatenateModels(m->models[modelName], blendshapeModel);
                    }));
            }
        }
    }

    m->initializationFutures.Wait();

    // check if region count is the same between expressions
    for (int i = 1; i < expressionModelsCount; ++i)
    {
        if (m->models[m->expressionNames[i]]->NumRegions() != m->models[m->expressionNames[0]]->NumRegions())
        {
            CARBON_CRITICAL("Expression \"{}\" has different number of regions: {} vs {}",
                            m->expressionNames[i],
                            m->models[m->expressionNames[i]]->NumRegions(),
                            m->models[m->expressionNames[0]]->NumRegions());
        }
    }

    LOG_INFO("RigCalibrationData::Load() finished.");
    return true;
}

int ModelData::NumModels() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return (int)m->models.size();
}

const std::shared_ptr<IdentityBlendModel<float, -1>> ModelData::GetModel(const std::string& modelName) const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }

    if (modelName != m->neutralName)
    {
        m->WaitForInitialization();
    }

    auto result = m->models.find(modelName);
    if (result != m->models.end())
    {
        return result->second;
    }
    else
    {
        return nullptr;
    }
}

const std::string& ModelData::GetNeutralName() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->neutralName;
}

const std::string& ModelData::GetSkinningName() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->skinningName;
}

const std::string& ModelData::GetModelVersionIdentifier() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->modelVersionIdentifier;
}

const std::vector<std::string>& ModelData::GetModelNames() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->expressionNames;
}

const std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>>& ModelData::GetRegionGeneCodeMatrices() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->geneCodeMatrix;
}

const std::map<std::string, std::map<std::string, std::pair<int, int>>>& ModelData::GetRegionGeneCodeExprRanges() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->geneCodeExprRanges;
}

const std::map<std::string, SkinningRegionData>& ModelData::GetSkinningModel() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->skinningModel;
}

SkinningRegionData ModelData::GetRegionSkinningModel(const std::string& region) const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }

    auto it = m->skinningModel.find(region);
    if (it != m->skinningModel.end())
    {
        return it->second;
    }

    return {};
}

const std::vector<std::string>& ModelData::GetSkinningModelRegions() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    return m->skinningRegions;
}

const std::shared_ptr<IdentityBlendModel<float>>& ModelData::GetStabilizationModel() const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }

    return m->stabilizationModel;
}

std::vector<std::pair<Eigen::VectorXf, Eigen::MatrixXf>> ModelData::GetExpressionGeneCodeModes(const std::string expressionName) const
{
    if (!m->initializationFirstPhaseCompleted)
    {
        CARBON_CRITICAL("RigCalibrationData not initialized");
    }
    auto geneCodeMatrices = GetRegionGeneCodeMatrices();
    auto geneCodeRanges = GetRegionGeneCodeExprRanges();

    const auto& neutralModel = m->models.at(m->neutralName);

    int numRegions = (int)geneCodeMatrices.size();

    std::vector<std::pair<Eigen::VectorXf, Eigen::MatrixXf>> perRegionMeansAndModes;

    for (int r = 0; r < numRegions; ++r)
    {
        const auto &regionName = neutralModel->RegionName(r);
        const auto regionGeneCodeMatrixIt = geneCodeMatrices.find(regionName);
        if (regionGeneCodeMatrixIt == geneCodeMatrices.end())
        {
            CARBON_CRITICAL("No region {} in the character code matrix.", regionName);
        }

        const auto expressionRegionRangeIt = geneCodeRanges.find(regionName);
        if (expressionRegionRangeIt == geneCodeRanges.end())
        {
            CARBON_CRITICAL("No region {} in the character code matrix.", regionName);
        }

        const auto &expressionRangesForRegion = expressionRegionRangeIt->second;
        const auto expressionRangeForRegionIt = expressionRangesForRegion.find(expressionName);
        if (expressionRangeForRegionIt == expressionRangesForRegion.end())
        {
            CARBON_CRITICAL("No model {} for region.", expressionName);
        }

        const auto [start, end] = expressionRangeForRegionIt->second;
        const auto &[regionMean, regionModes] = regionGeneCodeMatrixIt->second;

        // extract modes and mean from gene code that corresponds to the expressions
        const int numRows = end - start;
        const int numCols = (int)regionModes.rows();
        Eigen::MatrixXf expressionModes = Eigen::MatrixXf::Zero(numRows, numCols);
        Eigen::VectorXf expressionMean = Eigen::VectorXf::Zero(numRows);

        for (int j = start, rowIter = 0; j < end; ++j, ++rowIter)
        {
            expressionModes.row(rowIter) = regionModes.col(j);
            expressionMean[rowIter] = regionMean[j];
        }

        perRegionMeansAndModes.push_back({ expressionMean, expressionModes });
    }

    return perRegionMeansAndModes;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
