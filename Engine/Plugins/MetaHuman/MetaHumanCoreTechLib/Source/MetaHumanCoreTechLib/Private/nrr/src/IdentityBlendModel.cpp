// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/IdentityBlendModel.h>

#include <carbon/io/Utils.h>
#include <carbon/io/JsonIO.h>
#include <nls/math/Math.h>
#include <nls/geometry/Mesh.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/serialization/EigenSerialization.h>

#include <carbon/io/JsonIO.h>
#include <carbon/utils/Timer.h>
#include <type_traits>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

static constexpr uint32_t IdentityBlendModelMagicNumber = 0x88770001;
static constexpr int32_t IdentityBlendModelVersion = 1;

template <class T, int D>
std::shared_ptr<IdentityBlendModel<T, D>> IdentityBlendModel<T, D>::Reduce(const std::vector<int>& vertexIds) const
{
    const int newModelDim = (int)vertexIds.size();
    const int numRegions = this->NumRegions();

    Eigen::Matrix<T, D, -1> newMean = Eigen::Matrix<T, D, -1>::Zero(m->numDimensions, newModelDim);
    std::vector<typename IdentityBlendModel<T, D>::RegionData> newRegionData(numRegions);

    for (int i = 0; i < newModelDim; ++i)
    {
        newMean.col(i) = this->Base().col(vertexIds[i]);
    }

    for (int r = 0; r < numRegions; ++r)
    {
        // modes from the input model. Size: m->numDimensions * numInputModelRegionVertices / numModes
        const auto& modes = this->RegionModes(r);
        // weights from the input model. Size: numInputModelRegionVertices
        const auto& weights = this->RegionWeights(r);
        // region vertex ids from the input model. Size: numInputModelRegionVertices
        const auto& regionVertexIds = this->RegionVertexIds(r);

        const int numRegionModes = int(modes.cols());

        std::vector<int> indicesInVerticesIds;
        indicesInVerticesIds.reserve(vertexIds.size());
        std::vector<int> indicesInRegionVertexIds;
        indicesInRegionVertexIds.reserve(vertexIds.size());

        Eigen::VectorXi globalToRegionVertexIdMap = Eigen::VectorXi::Constant(this->NumVertices(), -1);
        for (int i = 0; i < (int)regionVertexIds.size(); ++i)
        {
            globalToRegionVertexIdMap[regionVertexIds[i]] = i;
        }

        for (int globVtxIter = 0; globVtxIter < (int)vertexIds.size(); globVtxIter++)
        {
            int regVertexIdx = globalToRegionVertexIdMap[vertexIds[globVtxIter]];
            if (regVertexIdx >= 0)
            {
                indicesInVerticesIds.push_back(globVtxIter);
                indicesInRegionVertexIds.push_back(regVertexIdx);
            }
        }

        int newRegionSize;
        Eigen::VectorXi newRegionVtxIds;
        Eigen::VectorX<T> newWeights;
        Eigen::MatrixX<T> newRegionModes;

        if (indicesInVerticesIds.size() > 0)
        {
            newRegionSize = (int)indicesInVerticesIds.size();
            newRegionVtxIds = Eigen::VectorXi::Zero(newRegionSize);
            newWeights = Eigen::VectorX<T>::Zero(newRegionSize);
            newRegionModes = Eigen::MatrixX<T>::Zero(newRegionSize * m->numDimensions, numRegionModes);

            for (int i = 0; i < newRegionSize; ++i)
            {
                newRegionVtxIds[i] = indicesInVerticesIds[i];
                newWeights[i] = weights[indicesInRegionVertexIds[i]];

                for (int j = 0; j < m->numDimensions; ++j)
                {
                    newRegionModes.row(m->numDimensions * i + j) = modes.row(m->numDimensions * indicesInRegionVertexIds[i] + j);
                }
            }
        }
        else
        {
            newRegionSize = (int)0;
            newRegionVtxIds = Eigen::VectorXi::Zero(newRegionSize);
            newWeights = Eigen::VectorX<T>::Zero(newRegionSize);
            newRegionModes = Eigen::MatrixX<T>::Zero(newRegionSize * m->numDimensions, numRegionModes);
        }

        typename IdentityBlendModel<T, D>::RegionData reducedRegion;
        reducedRegion.modeNames = this->ModeNames(r);
        reducedRegion.regionName = this->RegionName(r);
        reducedRegion.modes = std::move(newRegionModes);
        reducedRegion.vertexIDs = std::move(newRegionVtxIds);
        reducedRegion.weights = std::move(newWeights);
        newRegionData[r] = std::move(reducedRegion);
    }
    auto outputModel = std::make_shared<IdentityBlendModel<T, D>>();
    outputModel->SetModel(std::move(newMean), std::move(newRegionData));
    return outputModel;
}

template <class T, int D>
struct IdentityBlendModel<T, D>::Private
{
    Eigen::Matrix<T, D, -1> base;
    std::vector<RegionData> regionsData;
    SparseMatrixConstPtr<T> identityBlendModelMatrix;
    std::vector<std::pair<int, int>> regionRanges;
    int numDimensions;
};


template <class T, int D>
IdentityBlendModel<T, D>::IdentityBlendModel()
    : m(std::make_unique<Private>())
{}

template <class T, int D> IdentityBlendModel<T, D>::~IdentityBlendModel() = default;
template <class T, int D> IdentityBlendModel<T, D>::IdentityBlendModel(IdentityBlendModel&&) = default;
template <class T, int D>
IdentityBlendModel<T, D>& IdentityBlendModel<T, D>::operator=(IdentityBlendModel&&) = default;

template <class T, int D>
void IdentityBlendModel<T, D>::SetModel(Eigen::Matrix<T, D, -1>&& mean, std::vector<RegionData>&& regionsData)
{
    m->base = std::move(mean);
    m->regionsData = std::move(regionsData);
    m->numDimensions = (int)m->base.rows();
    UpdateBlendModelMatrix();
}

template <class T, int D>
void IdentityBlendModel<T, D>::UpdateBlendModelMatrix()
{
    const int numVertices = int(m->base.cols());

    std::vector<std::pair<int, int>> regionRanges;
    int totalModes = 0;

    Eigen::VectorXi entriesPerVertex = Eigen::VectorXi::Zero(numVertices);
    for (auto& regionData : m->regionsData)
    {
        const int numRegionModes = int(regionData.modes.cols());
        totalModes += numRegionModes;
        for (int i = 0; i < int(regionData.vertexIDs.size()); i++)
        {
            const int vID = regionData.vertexIDs[i];
            entriesPerVertex[vID] += numRegionModes;
        }
    }

    SparseMatrixPtr<T> identityBlendModelMatrix = std::make_shared<SparseMatrix<T>>(m->numDimensions * numVertices, totalModes);
    identityBlendModelMatrix->resizeNonZeros(entriesPerVertex.sum() * m->numDimensions);

    int* rowIndices = identityBlendModelMatrix->outerIndexPtr();
    int* colIndices = identityBlendModelMatrix->innerIndexPtr();
    T* values = identityBlendModelMatrix->valuePtr();
    Eigen::VectorXi currentInnerIndex = Eigen::VectorXi::Zero(numVertices * m->numDimensions);
    {
        int total = 0;
        for (int vID = 0; vID < numVertices; ++vID)
        {
            for (int k = 0; k < m->numDimensions; ++k)
            {
                rowIndices[m->numDimensions * vID + k] = total;
                currentInnerIndex[m->numDimensions * vID + k] = total;
                total += entriesPerVertex[vID];
            }
        }
        rowIndices[m->numDimensions * numVertices] = total;
    }

    totalModes = 0;
    for (auto& regionData : m->regionsData)
    {
        const int numRegionModes = int(regionData.modes.cols());

        if (regionData.vertexIDs.size() != int(regionData.modes.rows()) / m->numDimensions)
        {
            CARBON_CRITICAL("vertex_ids and modes matrix for region {} do not match", regionData.regionName);
        }
        if (regionData.weights.size() != int(regionData.modes.rows()) / m->numDimensions)
        {
            CARBON_CRITICAL("weights and modes matrix for region {} do not match", regionData.regionName);
        }

        for (int i = 0; i < int(regionData.vertexIDs.size()); i++)
        {
            const int vID = regionData.vertexIDs[i];
            for (int j = 0; j < numRegionModes; j++)
            {
                for (int k = 0; k < m->numDimensions; k++)
                {
                    const int idx = m->numDimensions * vID + k;
                    values[currentInnerIndex[idx]] = regionData.weights[i] * regionData.modes(m->numDimensions * i + k, j);
                    colIndices[currentInnerIndex[idx]] = totalModes + j;
                    currentInnerIndex[idx]++;
                }
            }
        }

        regionRanges.push_back({ totalModes, totalModes + numRegionModes });
        totalModes += numRegionModes;

        // verify that we have names for each mode
        if (int(regionData.modeNames.size()) != numRegionModes)
        {
            regionData.modeNames.clear();
            for (int k = 0; k < numRegionModes; ++k)
            {
                regionData.modeNames.push_back("mode " + std::to_string(k));
            }
        }
    }

    m->regionRanges = std::move(regionRanges);
    m->identityBlendModelMatrix = identityBlendModelMatrix;
}

template <class T, int D>
void IdentityBlendModel<T, D>::LoadModelJson(const std::string& identityModelFile) { LoadModelJson(ReadJson(ReadFile(identityModelFile))); }

template <class T, int D>
bool IdentityBlendModel<T, D>::LoadModelBinary(const std::string& identityModelFile)
{
    try
    {
        FILE *pFile = nullptr;
        #ifdef _MSC_VER
        fopen_s(&pFile, identityModelFile.c_str(), "rb");
        #else
        pFile = fopen(identityModelFile.c_str(), "rb");
        #endif
        if (pFile)
        {
            bool success = true;
            int readNumOfRegions = 0;

            const bool hasVersion = io::ReadAndCheckOrRevertFromBinaryFile(pFile, IdentityBlendModelMagicNumber);
            int32_t currVersion = -1;
            if (hasVersion)
            {
                success = success && io::FromBinaryFile<int32_t>(pFile, currVersion);
                if (currVersion != IdentityBlendModelVersion)
                {
                    CARBON_CRITICAL("Version not supported {}", currVersion);
                }
            }
            if (!io::FromBinaryFile<int>(pFile, readNumOfRegions))
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            Eigen::Matrix<T, D, -1> mean;
            if (!io::FromBinaryFile(pFile, mean))
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            std::vector<RegionData> regionsData(readNumOfRegions);
            for (int i = 0; i < readNumOfRegions; i++)
            {
                RegionData regionData;
                success &= io::FromBinaryFile(pFile, regionData.regionName);
                success &= io::FromBinaryFile(pFile, regionData.modes);
                success &= io::FromBinaryFile(pFile, regionData.vertexIDs);
                success &= io::FromBinaryFile(pFile, regionData.weights);
                if (currVersion >= 1)
                {
                    io::FromBinaryFile(pFile, regionData.modeNames);
                }

                if (!success)
                {
                    CARBON_CRITICAL("Failed to load {}", identityModelFile);
                }
                regionsData[i] = std::move(regionData);
            }
            SetModel(std::move(mean), std::move(regionsData));
            fclose(pFile);
            return success;
        }
    }
    catch (const std::exception&)
    {}

    return false;
}

template <class T, int D>
void IdentityBlendModel<T, D>::LoadModelJson(const TITAN_NAMESPACE::JsonElement& identityJson)
{
    Eigen::Matrix<T, D, -1> mean;
    io::FromJson(identityJson["mean"], mean);

    std::vector<RegionData> regionsData;
    const JsonElement& jRegions = identityJson["regions"];
    for (auto&& [regionName, regionJsonData] : jRegions.Map())
    {
        RegionData regionData;
        regionData.regionName = regionName;

        io::FromJson(regionJsonData["vertex_ids"], regionData.vertexIDs);
        io::FromJson(regionJsonData["weights"], regionData.weights);
        io::FromJson(regionJsonData["modes"], regionData.modes);

        if (regionJsonData.Contains("mode names") && regionJsonData["mode names"].IsArray())
        {
            regionData.modeNames = regionJsonData["mode names"].template Get<std::vector<std::string>>();
        }

        regionsData.emplace_back(std::move(regionData));
    }

    SetModel(std::move(mean), std::move(regionsData));
}

template <class T, int D>
void IdentityBlendModel<T, D>::SaveModelBinary(const std::string& filename) const
{
    FILE* pFile = nullptr;
    #ifdef _MSC_VER
    fopen_s(&pFile, filename.c_str(), "wb");
    #else
    pFile = fopen(filename.c_str(), "wb");
    #endif

    if (pFile)
    {
        io::ToBinaryFile<uint32_t>(pFile, IdentityBlendModelMagicNumber);
        io::ToBinaryFile<int32_t>(pFile, IdentityBlendModelVersion);
        const int numOfRegions = (int)m->regionsData.size();
        io::ToBinaryFile<int>(pFile, numOfRegions);
        io::ToBinaryFile(pFile, m->base);

        for (const auto& regionData : m->regionsData)
        {
            io::ToBinaryFile(pFile, regionData.regionName);
            io::ToBinaryFile(pFile, regionData.modes);
            io::ToBinaryFile(pFile, regionData.vertexIDs);
            io::ToBinaryFile(pFile, regionData.weights);
            io::ToBinaryFile(pFile, regionData.modeNames);
        }

        fclose(pFile);
    }
}

template <class T, int D>
TITAN_NAMESPACE::JsonElement IdentityBlendModel<T, D>::SaveModelJson() const
{
    using namespace TITAN_NAMESPACE;

    JsonElement regionsJson(JsonElement::JsonType::Object);

    for (const auto& regionData : m->regionsData)
    {
        JsonElement regionJson(JsonElement::JsonType::Object);
        regionJson.Insert("modes", io::ToJson(regionData.modes));
        regionJson.Insert("vertex_ids", io::ToJson(regionData.vertexIDs));
        regionJson.Insert("weights", io::ToJson(regionData.weights));
        regionJson.Insert("mode names", JsonElement(regionData.modeNames));
        regionsJson.Insert(regionData.regionName, std::move(regionJson));
    }

    JsonElement json(JsonElement::JsonType::Object);
    json.Insert("mean", io::ToJson(m->base));
    json.Insert("regions", std::move(regionsJson));

    return json;
}

template <class T, int D>
void IdentityBlendModel<T, D>::SaveModelJson(const std::string& identityModelFile) const
{
    TITAN_NAMESPACE::WriteFile(identityModelFile, TITAN_NAMESPACE::WriteJson(SaveModelJson(), -1));
}

template <class T, int D>
int IdentityBlendModel<T, D>::NumParameters() const
{
    if (m->identityBlendModelMatrix)
    {
        return int(m->identityBlendModelMatrix->cols());
    }
    else
    {
        return 0;
    }
}

template <class T, int D>
int IdentityBlendModel<T, D>::NumRegions() const
{
    return int(m->regionsData.size());
}

template <class T, int D>
int IdentityBlendModel<T, D>::NumVertices() const
{
    return int(m->identityBlendModelMatrix->rows() / m->numDimensions);
}

template <class T, int D>
Vector<T> IdentityBlendModel<T, D>::DefaultParameters() const
{
    return Vector<T>::Zero(NumParameters());
}

template <class T, int D>
const Eigen::Matrix<T, D, -1>& IdentityBlendModel<T, D>::Base() const
{
    return m->base;
}

template <class T, int D>
SparseMatrixConstPtr<T> IdentityBlendModel<T, D>::ModelMatrix() const
{
    return m->identityBlendModelMatrix;
}

template <class T, int D>
Eigen::Matrix<T, D, -1> IdentityBlendModel<T, D>::Evaluate(const Vector<T>& parameters) const
{
    Eigen::Matrix<T, D, -1> mat = m->base;
    Eigen::Map<Eigen::VectorX<T>>(mat.data(), mat.size()) += *(m->identityBlendModelMatrix) * parameters;
    return mat;
}

template <class T, int D>
DiffDataMatrix<T, D, -1> IdentityBlendModel<T, D>::Evaluate(const DiffData<T>& parameters) const
{
    if (parameters.Size() != NumParameters())
    {
        throw std::runtime_error("parameters have incorrect size");
    }

    Vector<T> mat =
        Eigen::Map<const Eigen::VectorX<T>>(m->base.data(),
                                            m->base.size()) + *(m->identityBlendModelMatrix) * parameters.Value();

    JacobianConstPtr<T> Jacobian;
    if (parameters.HasJacobian())
    {
        Jacobian = parameters.Jacobian().Premultiply(*m->identityBlendModelMatrix);
    }
    return DiffDataMatrix<T, D, -1>(m->numDimensions, NumVertices(), DiffData<T>(std::move(mat), Jacobian));
}

template <class T, int D>
DiffData<T> IdentityBlendModel<T, D>::EvaluateRegularization(const DiffData<T>& parameters) const
{
    if (parameters.Size() != NumParameters())
    {
        CARBON_CRITICAL("parameters have incorrect size");
    }

    // regularization is simply the L2 norm of the parameters, so we can just return the input
    return parameters.Clone();
}

template <class T, int D>
const std::string& IdentityBlendModel<T, D>::RegionName(int regionIndex) const
{
    return m->regionsData[regionIndex].regionName;
}

template <class T, int D>
const std::vector<std::string>& IdentityBlendModel<T, D>::ModeNames(int regionIndex) const
{
    return m->regionsData[regionIndex].modeNames;
}

template <class T, int D>
const Eigen::VectorXi& IdentityBlendModel<T, D>::RegionVertexIds(int regionIndex) const
{
    return m->regionsData[regionIndex].vertexIDs;
}

template <class T, int D>
const Eigen::VectorX<T>& IdentityBlendModel<T, D>::RegionWeights(int regionIndex) const
{
    return m->regionsData[regionIndex].weights;
}

template <class T, int D>
const Eigen::Matrix<T, -1, -1>& IdentityBlendModel<T, D>::RegionModes(int regionIndex) const
{
    return m->regionsData[regionIndex].modes;
}

template <class T, int D>
const std::vector<std::pair<int, int>>& IdentityBlendModel<T, D>::RegionRanges() const
{
    return m->regionRanges;
}

// explicitly instantiate the IdentityBlendModel classes
template class IdentityBlendModel<float, 3>;
template class IdentityBlendModel<double, 3>;
template class IdentityBlendModel<float, -1>;
template class IdentityBlendModel<double, -1>;

/**
 * @brief Load joints and blendshapes models from binary file and create IdentityBlendModel that unifies the two models
 *
 * @param[in] jointsPCAModelFile - joints pca model path
 * @param[in] blendshapesPCAModelFile - blendshapes pca model path
 */
template<class T, int D>
void GlobalExpressionPCAModel<T, D>::LoadModelBinary(const std::string &jointsPCAModelFile, const std::string &blendshapesPCAModelFile)
{
    // helper function that reads the regions data and the mean of the model
    auto readRegionDataAndMean = [&](const std::string &identityModelFile)
    {
        FILE *pFile = nullptr;
#ifdef _MSC_VER
        fopen_s(&pFile, identityModelFile.c_str(), "rb");
#else
        pFile = fopen(identityModelFile.c_str(), "rb");
#endif
        if (pFile)
        {
            int readNumOfRegions = 0;
            size_t itemsRead = fread(&readNumOfRegions, sizeof(readNumOfRegions), 1, pFile);
            if (itemsRead == 0)
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            Eigen::Matrix<T, D, -1> mean;
            if (!io::FromBinaryFile(pFile, mean))
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            std::vector<typename IdentityBlendModel<T>::RegionData> regionsData(readNumOfRegions);
            for (int i = 0; i < readNumOfRegions; i++)
            {
                typename IdentityBlendModel<T>::RegionData regionData;
                io::FromBinaryFile(pFile, regionData.regionName);
                io::FromBinaryFile(pFile, regionData.modes);
                io::FromBinaryFile(pFile, regionData.vertexIDs);
                io::FromBinaryFile(pFile, regionData.weights);
                regionsData[i] = std::move(regionData);
            }

            return std::tuple<Eigen::Matrix<T, D, -1>, std::vector<typename IdentityBlendModel<T>::RegionData>>(mean, regionsData);
        }

        return std::tuple<Eigen::Matrix<T, D, -1>, std::vector<typename IdentityBlendModel<T>::RegionData>>();
    };

    // read the models
    auto [jointsMean, jointsRegionData] = readRegionDataAndMean(jointsPCAModelFile);
    auto [blendshapesMean, blendshapesRegionData] = readRegionDataAndMean(blendshapesPCAModelFile);

    // Merge two models by extending the regions data of the joints model with blendshapes model for the given region
    std::vector<typename IdentityBlendModel<T>::RegionData> mergedRegionsData(jointsRegionData.size());


    const int numJointIDs = (int)jointsMean.cols();
    const int numBlendIDs = (int)blendshapesMean.cols();
    const int numMergedIDs = numJointIDs + numBlendIDs;

    for (int i = 0; i < (int)jointsRegionData.size(); i++)
    {
        typename IdentityBlendModel<T>::RegionData regionData;

        // Name is the same in both of the models
        regionData.regionName = jointsRegionData[i].regionName;

        int numJointWeights = (int)jointsRegionData[i].vertexIDs.size();
        int numBlendWeights = (int)blendshapesRegionData[i].vertexIDs.size();

        regionData.vertexIDs = Eigen::VectorXi(numJointWeights + numBlendWeights);
        regionData.vertexIDs.head(numJointWeights) = jointsRegionData[i].vertexIDs;
        regionData.vertexIDs.segment(numJointWeights, numBlendWeights) = blendshapesRegionData[i].vertexIDs.array() + numJointIDs;


        regionData.weights = Eigen::VectorX<T>(numJointWeights + numBlendWeights);
        regionData.weights.head(numJointWeights) = jointsRegionData[i].weights;
        regionData.weights.segment(numJointWeights, numBlendWeights) = blendshapesRegionData[i].weights;


        int jointRows = (int)jointsRegionData[i].modes.rows();
        int jointCols = (int)jointsRegionData[i].modes.cols();

        int blendRows = (int)blendshapesRegionData[i].modes.rows();
        int blendCols = (int)blendshapesRegionData[i].modes.cols();

        regionData.modes = Eigen::Matrix<T, -1, -1>(jointRows + blendRows, jointCols + blendCols);
        regionData.modes.block(0, 0, jointRows, jointCols) = jointsRegionData[i].modes;
        regionData.modes.block(jointRows, jointCols, blendRows, blendCols) = blendshapesRegionData[i].modes;

        mergedRegionsData[i] = std::move(regionData);
    }

    // concatenate the means of the two models
    Eigen::Matrix<T, -1, -1> mergedMean(D, numMergedIDs);
    mergedMean << jointsMean, blendshapesMean;

    // call the SetModel method to finish the setup
    IdentityBlendModel<T>::SetModel(std::move(mergedMean), std::move(mergedRegionsData));
}

template<class T, int D>
void GlobalExpressionPCAModel<T, D>::LoadModelBinary(const std::string &jointsPCAModelFile)
{
    // helper function that reads the regions data and the mean of the model
    auto readRegionDataAndMean = [&](const std::string &identityModelFile)
    {
        FILE *pFile = nullptr;
#ifdef _MSC_VER
        fopen_s(&pFile, identityModelFile.c_str(), "rb");
#else
        pFile = fopen(identityModelFile.c_str(), "rb");
#endif
        if (pFile)
        {
            int readNumOfRegions = 0;
            size_t itemsRead = fread(&readNumOfRegions, sizeof(readNumOfRegions), 1, pFile);
            if (itemsRead == 0)
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            Eigen::Matrix<T, D, -1> mean;
            if (!io::FromBinaryFile(pFile, mean))
            {
                CARBON_CRITICAL("Failed to read binary file {}", identityModelFile);
            }
            std::vector<typename IdentityBlendModel<T>::RegionData> regionsData(readNumOfRegions);
            for (int i = 0; i < readNumOfRegions; i++)
            {
                typename IdentityBlendModel<T>::RegionData regionData;
                io::FromBinaryFile(pFile, regionData.regionName);
                io::FromBinaryFile(pFile, regionData.modes);
                io::FromBinaryFile(pFile, regionData.vertexIDs);
                io::FromBinaryFile(pFile, regionData.weights);
                regionsData[i] = std::move(regionData);
            }

            return std::tuple<Eigen::Matrix<T, D, -1>, std::vector<typename IdentityBlendModel<T>::RegionData>>(mean, regionsData);
        }

        return std::tuple<Eigen::Matrix<T, D, -1>, std::vector<typename IdentityBlendModel<T>::RegionData>>();
    };

    // read the model
    auto [jointsMean, jointsRegionData] = readRegionDataAndMean(jointsPCAModelFile);

    // call the SetModel method to finish the setup
    IdentityBlendModel<T>::SetModel(std::move(jointsMean), std::move(jointsRegionData));
}

template class GlobalExpressionPCAModel<float, 3>;
template class GlobalExpressionPCAModel<double, 3>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
