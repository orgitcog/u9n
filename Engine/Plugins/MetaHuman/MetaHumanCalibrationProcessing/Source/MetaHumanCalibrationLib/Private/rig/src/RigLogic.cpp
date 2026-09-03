// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RigLogic.h>

#include <nls/DiffData.h>
#include <nls/math/Math.h>
#include <rig/RBFLogic.h>
#include <rig/rbfs/RBFSolver.h>

#include <carbon/Algorithm.h>
#include <carbon/utils/Timer.h>
#include <nls/VectorVariable.h>
#include <nls/geometry/EulerAngles.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/StringUtils.h>

#include <dna/Reader.h>
#include <dna/layers/Descriptor.h>
#include <riglogic/RigLogic.h>
#include <tdm/Quat.h>
#include <tdm/Transforms.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <limits>
#include <set>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct RigLogicMLNetwork
{
    std::vector<Eigen::MatrixX<T>> m_layerWeights;
    std::vector<Eigen::VectorX<T>> m_layerBiases;
    std::vector<dna::ActivationFunction> m_layerActivationFunctions;
    std::vector<Eigen::VectorX<T>> m_layerActivationFunctionParameters;
    std::vector<int> m_inputIndices;
    std::vector<int> m_outputIndices;
};


template <class T>
struct RigLogic<T>::Private
{
    int numLODs = 0;

    int guiControlCount = 0;
    int rawControlCount = 0;
    int psdControlCount = 0;
    int mlControlCount = 0;
    int rbfPoseControlCount = 0;
    int totalControlCount = 0;

    std::vector<std::string> guiControlNames;
    std::vector<std::string> rawControlNames;
    std::vector<std::string> mlControlNames;
    std::vector<std::string> mlNetworkNames;

    // ### GUI to Raw mapping ###
    struct GuiToRawInfo
    {
        int inputIndex; // gui control (col index)
        int outputIndex; // raw control (row index)
        T from;
        T to;
        T slope;
        T cut;
    };
    std::vector<GuiToRawInfo> guiToRawMapping;
    // the ranges for each gui control
    Eigen::Matrix<T, 2, -1> guiControlRanges;
    //! number of times the gui control is used as a mapping (0: unused, 1: control from 0 to 1, 2: control from -1 to 1, >2: complex controls)
    std::vector<int> guiControlUseCount;

    // ### psd matrix ###
    SparseMatrix<T> psdToRawMap;
    bool psdDependsOnMLorRBF{};

    // ### blendshapes ###
    // Eigen::VectorX<int> blendshapesPerLOD;
    // Eigen::Matrix<int, 2, -1> blendshapeMapping;

    // ### joints ###
    int numJoints = 0;
    bool withJointScaling = false;
    std::vector<SparseMatrix<T>> jointMatrixPerLOD;
    std::vector<int> jointGroupIndexPerJoint;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupOutputRowsPerLOD;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupInputIndices;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupOutputIndices;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupJointIndices;

    // ### aniated maps ###
    int numAnimatedMaps = 0;
    Eigen::VectorX<int> animatedMapsPerLOD;
    // mapping from input (rowIndex=0) to output (rowIndex=1) for each animated map (columns)
    Eigen::Matrix<int, 2, -1> animatedMapsMapping;
    // from(rowIndex=0), to(rowIndex=1), slope(rowIndex=2), cut(rowIndex=3), for each animated map (columns)
    Eigen::Matrix<T, 4, -1> animatedMapsValues;

    // ### neural nets ###
    std::vector<RigLogicMLNetwork<T>> neuralNets;
    //! sorted vector containing control output index, network index, and network output index
    std::vector<std::tuple<int, int, int>> neuralNetOutputOrderedByOutputIndex;
    //! for each net sorted vector of pairs of control input index and network input index
    std::vector<std::vector<std::pair<int, int>>> neuralNetControlInputIndexAndNetInputIndex;

    RBFLogic<T> rbfLogic;
};


template <class T>
RigLogic<T>::RigLogic() : m(new Private)
{}

template <class T> RigLogic<T>::~RigLogic() = default;
template <class T> RigLogic<T>::RigLogic(RigLogic&&) = default;
template <class T> RigLogic<T>& RigLogic<T>::operator=(RigLogic&&) = default;

template <class T>
std::shared_ptr<RigLogic<T>> RigLogic<T>::Clone() const
{
    std::shared_ptr<RigLogic<T>> clone = std::make_shared<RigLogic>();
    *clone->m = *m;
    return clone;
}

template <class T>
bool RigLogic<T>::WithJointScaling() const
{
    return m->withJointScaling;
}

template <class T>
std::vector<T> rlToVec(const rl4::ConstArrayView<T>& view)
{
    return std::vector<T>(view.begin(), view.end());
}

template <class T>
Eigen::VectorX<T> rlToEigen(const rl4::ConstArrayView<T>& view)
{
    return Eigen::Map<const Eigen::VectorX<T>>(view.data(), view.size());
}

template <class T>
bool RigLogic<T>::Init(const dna::Reader* reader, bool withJointScaling)
{
    m->numLODs = reader->getLODCount();
    m->withJointScaling = withJointScaling;

    m->guiControlCount = reader->getGUIControlCount();
    m->rawControlCount = reader->getRawControlCount();
    m->psdControlCount = reader->getPSDCount();
    m->rbfPoseControlCount = reader->getRBFPoseControlCount();
    m->mlControlCount = reader->getMLControlCount();

    m->totalControlCount = m->rawControlCount + m->psdControlCount + m->mlControlCount + m->rbfPoseControlCount;
    LOG_VERBOSE("num controls: [gui {}] [raw {}] [psd {}] [ml {}] [rbf {}] => {}", m->guiControlCount, m->rawControlCount, m->psdControlCount, m->mlControlCount,  m->rbfPoseControlCount, m->totalControlCount);

    m->guiControlNames.clear();
    for (int i = 0; i < m->guiControlCount; ++i)
    {
        m->guiControlNames.push_back(reader->getGUIControlName(std::uint16_t(i)).c_str());
    }

    m->rawControlNames.clear();
    for (int i = 0; i < m->rawControlCount; ++i)
    {
        m->rawControlNames.push_back(reader->getRawControlName(std::uint16_t(i)).c_str());
    }

    m->mlControlNames.clear();
    for (int i = 0; i < m->mlControlCount; ++i)
    {
        m->mlControlNames.push_back(reader->getMLControlName(std::uint16_t(i)).c_str());
    }
    m->rbfLogic.Init(reader);

    // setup gui to raw calculation
    const int numGuiToRawAssignments = int(reader->getGUIToRawInputIndices().size());
    m->guiToRawMapping.resize(numGuiToRawAssignments);
    m->guiControlRanges = Eigen::Matrix<T, 2, -1>(2, m->guiControlCount);
    m->guiControlRanges.row(0).setConstant(1e6f);
    m->guiControlRanges.row(1).setConstant(-1e6f);

    m->guiControlUseCount = std::vector<int>(m->guiControlCount, 0);
    std::vector<int> rawControlUseCount(m->rawControlCount, 0);

    for (int i = 0; i < numGuiToRawAssignments; i++)
    {
        const int inputIndex = reader->getGUIToRawInputIndices()[i];
        const int outputIndex = reader->getGUIToRawOutputIndices()[i];
        if ((inputIndex < 0) || (inputIndex >= m->guiControlCount))
        {
            CARBON_CRITICAL("gui control input index is invalid");
        }
        if ((outputIndex < 0) || (outputIndex >= m->rawControlCount))
        {
            CARBON_CRITICAL("gui control output index is invalid");
        }
        T from = reader->getGUIToRawFromValues()[i];
        T to = reader->getGUIToRawToValues()[i];
        if (from > to) { std::swap(from, to); }
        m->guiToRawMapping[i].inputIndex = inputIndex;
        m->guiToRawMapping[i].outputIndex = outputIndex;
        m->guiToRawMapping[i].from = from;
        m->guiToRawMapping[i].to = to;
        m->guiToRawMapping[i].slope = reader->getGUIToRawSlopeValues()[i];
        m->guiToRawMapping[i].cut = reader->getGUIToRawCutValues()[i];
        m->guiControlRanges(0, inputIndex) = std::min<T>(m->guiControlRanges(0, inputIndex), from);
        m->guiControlRanges(1, inputIndex) = std::max<T>(m->guiControlRanges(1, inputIndex), to);
        m->guiControlUseCount[inputIndex]++;
        rawControlUseCount[outputIndex]++;
    }

    // sort gui to raw control mapping
    SortGuiControlMapping();

    for (int i = 0; i < m->guiControlCount; ++i)
    {
        if (!m->guiControlUseCount[i])
        {
            CARBON_CRITICAL("not all gui controls are being used");
        }
    }
    if (m->guiControlCount > 0)
    {
        int numUnusedRawControls = 0;
        for (int i = 0; i < m->rawControlCount; ++i)
        {
            if (!rawControlUseCount[i])
            {
                numUnusedRawControls++;
                LOG_VERBOSE("raw control {} {} is not mapped by gui controls", RawControlNames()[i], i);
            }
        }
        if (numUnusedRawControls > 0)
        {
            LOG_VERBOSE("{} out of {} raw controls are not used", numUnusedRawControls, m->rawControlCount);
        }
    }

    // setup psd calculation
    m->psdToRawMap = SparseMatrix<T>(m->rawControlCount + m->psdControlCount, m->totalControlCount);
    m->psdDependsOnMLorRBF = false;

    std::vector<Eigen::Triplet<T>> psdToRawMapTriplets;
    for (int i = 0; i < m->rawControlCount; ++i)
    {
        psdToRawMapTriplets.push_back(Eigen::Triplet<T>(i, i, T(1)));
    }
    for (int j = 0; j < int(reader->getPSDColumnIndices().size()); j++)
    {
        int row = reader->getPSDRowIndices()[j];
        if ((row < m->rawControlCount) || (row >= m->psdControlCount + m->rawControlCount))
        {
            CARBON_CRITICAL("psd control mapping invalid");
        }
        int col = reader->getPSDColumnIndices()[j];
        const bool psdDependsRaw = (col >= 0) && (col < m->rawControlCount);
        const bool psdDependsOnMLorRBF = (col >= (m->rawControlCount + m->psdControlCount)) && (col < m->totalControlCount);
        m->psdDependsOnMLorRBF |= psdDependsOnMLorRBF;
        if (!psdDependsRaw && !psdDependsOnMLorRBF)
        {
            CARBON_CRITICAL("psd control mapping invalid: psd {} uses {} as input, but max {} raw controls", row, col, m->rawControlCount);
        }

        psdToRawMapTriplets.push_back(Eigen::Triplet<T>(row, col, reader->getPSDValues()[j]));
    }
    m->psdToRawMap.setFromTriplets(psdToRawMapTriplets.begin(), psdToRawMapTriplets.end());


    //// setup blendshape mapping
    // m->blendshapesPerLOD.resize(reader->getLODCount());
    // for (int lod = 0; lod < reader->getLODCount(); lod++)
    // {
    // m->blendshapesPerLOD[lod] = reader->getBlendShapeChannelLODs()[lod];
    // }
    // m->blendshapeMapping.resize(2, reader->getBlendShapeChannelCount());
    // for (int i = 0; i < reader->getBlendShapeChannelCount(); i++)
    // {
    // m->blendshapeMapping(0, i) = reader->getBlendShapeChannelInputIndices()[i];
    // m->blendshapeMapping(1, i) = reader->getBlendShapeChannelOutputIndices()[i];
    // }

    // setup joints
    m->numJoints = reader->getJointCount();
    if (reader->getJointCount() * 9 != reader->getJointRowCount())
    {
        LOG_WARNING("number of joints and joint rows not matching: {} vs {}", reader->getJointCount() * 9, reader->getJointRowCount());
    }
    if ((m->numJoints > 0) && (m->totalControlCount != reader->getJointColumnCount()))
    {
        if (reader->getJointColumnCount() == (m->rawControlCount + m->psdControlCount + m->mlControlCount +  m->rbfPoseControlCount))
        {
            // ml rigs may only map to blendshapes and hence the joint column count matches the raw control and psd control count
        }
        else
        {
            LOG_WARNING("number of total controls and joint columns not matching: {} vs {}", m->totalControlCount, reader->getJointColumnCount());
        }
    }

    int numScalingDiscarded = 0;
    m->jointMatrixPerLOD.resize(m->numLODs);
    m->jointGroupIndexPerJoint = std::vector<int>(m->numJoints, -1);
    const std::uint16_t numJointGroups = reader->getJointGroupCount();
    m->jointGroupOutputRowsPerLOD.resize(numJointGroups);
    m->jointGroupInputIndices.resize(numJointGroups);
    m->jointGroupOutputIndices.resize(numJointGroups);
    m->jointGroupJointIndices.resize(numJointGroups);
    for (int lod = 0; lod < m->numLODs; lod++)
    {
        std::vector<Eigen::Triplet<T>> jointMatrixPerLODTriplets;
        for (std::uint16_t jointGroupIndex = 0; jointGroupIndex < numJointGroups; ++jointGroupIndex)
        {
            m->jointGroupOutputRowsPerLOD[jointGroupIndex] = rlToEigen(reader->getJointGroupLODs(jointGroupIndex));
            m->jointGroupInputIndices[jointGroupIndex] = rlToEigen(reader->getJointGroupInputIndices(jointGroupIndex));
            m->jointGroupOutputIndices[jointGroupIndex] = rlToEigen(reader->getJointGroupOutputIndices(jointGroupIndex));
            m->jointGroupJointIndices[jointGroupIndex] = rlToEigen(reader->getJointGroupJointIndices(jointGroupIndex));

            const int numInputIndices = (int)m->jointGroupInputIndices[jointGroupIndex].size();
            rl4::ConstArrayView<float> jointGroupValues = reader->getJointGroupValues(jointGroupIndex);
            for (int j = 0; j < m->jointGroupOutputRowsPerLOD[jointGroupIndex][lod]; j++)
            {
                if (j >= m->jointGroupOutputIndices[jointGroupIndex].size())
                {
                    CARBON_CRITICAL("invalid rows per lod value");
                }
                std::uint16_t jointIndexAndDof = m->jointGroupOutputIndices[jointGroupIndex][j];
                const std::uint16_t jointIndex = jointIndexAndDof / 9;
                const std::uint16_t dof = jointIndexAndDof % 9;
                if (m->jointGroupIndexPerJoint[jointIndex] < 0)
                {
                    m->jointGroupIndexPerJoint[jointIndex] = jointGroupIndex;
                }
                else if (m->jointGroupIndexPerJoint[jointIndex] != jointGroupIndex)
                {
                    LOG_WARNING("joint \"{}\" is part of more than one joint group ({} vs {})", reader->getJointName(jointIndex).c_str(), m->jointGroupIndexPerJoint[jointIndex], jointGroupIndex);
                }
                if (!m->withJointScaling)
                {
                    if (dof >= 6)
                    {
                        // LOG_VERBOSE("discarding scaling {} for joint {}", dof - 6, reader->getJointName(jointIndex));
                        numScalingDiscarded++;
                        continue;
                    }
                    jointIndexAndDof = 6 * jointIndex + dof;
                }
                const T scaling = (dof >= 3 && dof < 6) ? degree2radScale<T>() : T(1);
                for (int k = 0; k < numInputIndices; k++)
                {
                    const int valueIndex = j * numInputIndices + k;
                    const T value = scaling * T(jointGroupValues[valueIndex]);
                    if (fabs(value) > 1e-20)
                    {
                        jointMatrixPerLODTriplets.push_back(Eigen::Triplet<T>(jointIndexAndDof, m->jointGroupInputIndices[jointGroupIndex][k], value));
                    }
                }
            }
        }

        m->jointMatrixPerLOD[lod].resize(m->numJoints * (m->withJointScaling ? 9 : 6), m->totalControlCount);
        m->jointMatrixPerLOD[lod].setFromTriplets(jointMatrixPerLODTriplets.begin(), jointMatrixPerLODTriplets.end());
    }
    if (numScalingDiscarded > 0)
    {
        LOG_VERBOSE("discarding scaling for {} parameters", numScalingDiscarded);
    }

    // setup animated maps
    m->numAnimatedMaps = reader->getAnimatedMapCount();

    m->animatedMapsPerLOD = Eigen::VectorX<int>::Zero(m->numLODs);
    if (m->numAnimatedMaps > 0)
    {
        rl4::ConstArrayView<std::uint16_t> animatedMapLODs = reader->getAnimatedMapLODs();
        if (int(animatedMapLODs.size()) != int(m->animatedMapsPerLOD.size()))
        {
            CARBON_CRITICAL("animated map lods incorrect");
        }
        for (int i = 0; i < m->numLODs; i++)
        {
            m->animatedMapsPerLOD[i] = animatedMapLODs[i];
        }
        const int numAnimatedMapAssignments = int(reader->getAnimatedMapInputIndices().size());
        m->animatedMapsMapping.resize(2, numAnimatedMapAssignments);
        m->animatedMapsValues.resize(4, numAnimatedMapAssignments);

        for (int i = 0; i < numAnimatedMapAssignments; i++)
        {
            const int inputIndex = reader->getAnimatedMapInputIndices()[i];
            const int outputIndex = reader->getAnimatedMapOutputIndices()[i];
            if ((inputIndex < 0) || (inputIndex >= m->totalControlCount))
            {
                CARBON_CRITICAL("animated map input index is invalid");
            }
            if ((outputIndex < 0) || (outputIndex >= m->numAnimatedMaps))
            {
                CARBON_CRITICAL("animated map output index is invalid");
            }
            m->animatedMapsMapping(0, i) = inputIndex;
            m->animatedMapsMapping(1, i) = outputIndex;
            m->animatedMapsValues(0, i) = reader->getAnimatedMapFromValues()[i];
            m->animatedMapsValues(1, i) = reader->getAnimatedMapToValues()[i];
            m->animatedMapsValues(2, i) = reader->getAnimatedMapSlopeValues()[i];
            m->animatedMapsValues(3, i) = reader->getAnimatedMapCutValues()[i];
            if (m->animatedMapsValues(0, i) > m->animatedMapsValues(1, i))
            {
                CARBON_CRITICAL("animated maps mapping needs to have smaller from-value than to-value");
            }
        }
    }

    m->neuralNets.clear();
    m->neuralNetOutputOrderedByOutputIndex.clear();
    m->neuralNetControlInputIndexAndNetInputIndex.clear();
    LOG_VERBOSE("Number of neural networks in rig: {}", reader->getNeuralNetworkCount());
    for (std::uint16_t netIndex = 0; netIndex < reader->getNeuralNetworkCount(); ++netIndex)
    {
        RigLogicMLNetwork<T> net;
        for (std::uint16_t layerIndex = 0; layerIndex < reader->getNeuralNetworkLayerCount(netIndex); ++layerIndex)
        {
            dna::ConstArrayView<float> biases = reader->getNeuralNetworkLayerBiases(netIndex, layerIndex);
            dna::ConstArrayView<float> weights = reader->getNeuralNetworkLayerWeights(netIndex, layerIndex);
            dna::ConstArrayView<float> activationFunctionParameters = reader->getNeuralNetworkLayerActivationFunctionParameters(netIndex, layerIndex);
            const int outputSize = (int)biases.size();
            const int inputSize = (int)weights.size() / outputSize;
            auto weightsMap = Eigen::Map<const Eigen::MatrixXf>(weights.data(), inputSize, outputSize).transpose();
            auto biasesMap = Eigen::Map<const Eigen::VectorXf>(biases.data(), outputSize);
            auto paramsMap = Eigen::Map<const Eigen::VectorXf>(activationFunctionParameters.data(), activationFunctionParameters.size());
            net.m_layerWeights.push_back(weightsMap.template cast<T>());
            net.m_layerBiases.push_back(biasesMap.template cast<T>());
            net.m_layerActivationFunctionParameters.push_back(paramsMap.template cast<T>());
            net.m_layerActivationFunctions.push_back(reader->getNeuralNetworkLayerActivationFunction(netIndex, layerIndex));
        }
        const std::vector<std::uint16_t> inputIndices = rlToVec(reader->getNeuralNetworkInputIndices(netIndex));
        const std::vector<std::uint16_t> outputIndices = rlToVec(reader->getNeuralNetworkOutputIndices(netIndex));
        net.m_inputIndices = std::vector<int>(inputIndices.begin(), inputIndices.end());
        net.m_outputIndices = std::vector<int>(outputIndices.begin(), outputIndices.end());

        std::vector<std::pair<int, int>> vecOfControlInputAndNetInput;
        for (int netInputIndex = 0; netInputIndex < (int)net.m_inputIndices.size(); ++netInputIndex)
        {
            vecOfControlInputAndNetInput.push_back({net.m_inputIndices[netInputIndex], netInputIndex});
        }
        // sort the neural net inputs to make sure insertion into the jacobian is in order
        std::sort(vecOfControlInputAndNetInput.begin(), vecOfControlInputAndNetInput.end());

        for (int netOutputIndex = 0; netOutputIndex < (int)net.m_outputIndices.size(); ++netOutputIndex)
        {
            m->neuralNetOutputOrderedByOutputIndex.push_back({net.m_outputIndices[netOutputIndex], (int)netIndex, netOutputIndex});
        }

        m->neuralNets.emplace_back(std::move(net));
        m->neuralNetControlInputIndexAndNetInputIndex.push_back(vecOfControlInputAndNetInput);
    }
    // sort the neural net outputs by output index to ensure that the jacobian is filled in the right order
    std::sort(m->neuralNetOutputOrderedByOutputIndex.begin(), m->neuralNetOutputOrderedByOutputIndex.end());

    m->mlNetworkNames = std::vector<std::string>(m->neuralNets.size(), "Unknown");
    for (uint16_t meshIndex = 0; meshIndex <reader->getMeshCount(); ++meshIndex)
    {
        std::uint16_t meshRegionCount = reader->getMeshRegionCount(meshIndex);
        if (meshRegionCount > 0)
        {
            // LOG_INFO("mesh region count: {} => {}", reader->getMeshName(meshIndex), meshRegionCount);
            for (std::uint16_t regionIndex = 0; regionIndex < meshRegionCount; ++regionIndex)
            {
                // LOG_INFO("region {} : {}", regionIndex, reader->getMeshRegionName(meshIndex, regionIndex).c_str());
                auto view = reader->getNeuralNetworkIndicesForMeshRegion(meshIndex, regionIndex);
                for (int i = 0; i < (int)view.size(); ++i)
                {
                    // LOG_INFO("neural index {}: {}", i, view[i]);
                    if (view[i] != regionIndex)
                    {
                        LOG_WARNING("ml network for mesh {}, region {} points to neural net {}", reader->getMeshName(meshIndex).c_str(), reader->getMeshRegionName(meshIndex, regionIndex).c_str(), view[i]);
                    }
                    m->mlNetworkNames[view[i]] = reader->getMeshRegionName(meshIndex, regionIndex).c_str();
                }
            }
        }
    }

    return true;
}

template <class T>
void RigLogic<T>::SortGuiControlMapping()
{
    std::vector<int> order(m->guiToRawMapping.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
            if (m->guiToRawMapping[a].outputIndex < m->guiToRawMapping[b].outputIndex) { return true; }
            if (m->guiToRawMapping[a].outputIndex > m->guiToRawMapping[b].outputIndex) { return false; }
            return (m->guiToRawMapping[a].inputIndex < m->guiToRawMapping[b].inputIndex);
        });
    std::vector<typename Private::GuiToRawInfo> guiToRawMapping = m->guiToRawMapping;
    for (size_t i = 0; i < order.size(); ++i)
    {
        guiToRawMapping[i] = m->guiToRawMapping[order[i]];
    }
    m->guiToRawMapping = std::move(guiToRawMapping);
}

template <class T>
int RigLogic<T>::NumGUIControls() const
{
    return m->guiControlCount;
}

template <class T>
int RigLogic<T>::NumRawControls() const
{
    return m->rawControlCount;
}

template <class T>
int RigLogic<T>::NumPsdControls() const
{
    return m->psdControlCount;
}

template <class T>
int RigLogic<T>::NumMLControls() const
{
    return m->mlControlCount;
}

template <class T>
int RigLogic<T>::NumRBFControls() const
{
    return m->rbfPoseControlCount;
}

template <class T>
int RigLogic<T>::NumTotalControls() const
{
    return m->totalControlCount;
}

template <class T>
int RigLogic<T>::NumNeuralNetworks() const
{
    return (int)m->neuralNets.size();
}

template <class T>
const std::vector<std::string>& RigLogic<T>::GuiControlNames() const
{
    return m->guiControlNames;
}

template <class T>
const std::vector<std::string>& RigLogic<T>::RawControlNames() const
{
    return m->rawControlNames;
}

template <class T>
const std::vector<std::string>& RigLogic<T>::MLControlNames() const
{
    return m->mlControlNames;
}

template <class T>
const std::vector<std::string>& RigLogic<T>::RBFEulerControlNames() const {
     return m->rbfLogic.EulerControlNames();
}

template <class T>
const std::vector<std::string>& RigLogic<T>::RBFPoseNames() const
{
    return m->rbfLogic.PoseNames();
}

template <class T>
const std::vector<std::string>& RigLogic<T>::RBFPoseControlNames() const
{
    return m->rbfLogic.PoseControlNames();
}

template <class T>
const std::vector<std::string>& RigLogic<T>::MLNetworkNames() const
{
    return m->mlNetworkNames;
}


template <class T>
const Eigen::Matrix<T, 2, -1>& RigLogic<T>::GuiControlRanges() const
{
    return m->guiControlRanges;
}

template <class T>
DiffData<T> RigLogic<T>::EvaluateRawControls(const DiffData<T>& guiControls) const
{
    if (guiControls.Size() != m->guiControlCount)
    {
        CARBON_CRITICAL("RigLogic::EvaluateRawControls(): guiControls control count incorrect: {} instead of {}", guiControls.Size(), m->guiControlCount);
    }

    Vector<T> output = Vector<T>::Zero(m->rawControlCount);

    // evaluate GUI controls
    const int numGuiToRawMappings = int(m->guiToRawMapping.size());
    for (int i = 0; i < numGuiToRawMappings; i++)
    {
        const int inputIndex = m->guiToRawMapping[i].inputIndex;
        const int outputIndex = m->guiToRawMapping[i].outputIndex;
        const T from = m->guiToRawMapping[i].from;
        const T to = m->guiToRawMapping[i].to;
        const T slope = m->guiToRawMapping[i].slope;
        const T cut = m->guiToRawMapping[i].cut;
        const T value = guiControls.Value()[inputIndex];
        const T rangeStart = m->guiControlRanges(0, inputIndex);
        const T rangeEnd = m->guiControlRanges(1, inputIndex);
        const bool belowRange = (from == rangeStart && value < from);
        const bool aboveRange = (to == rangeEnd && value >= to);
        if ((from <= value) && (value < to))
        {
            // note that the evaluation here is slightly different compared to the original RigLogic implementation:
            // there the condition is (from < value && value <= to). The reason
            // to use this condition here is so that the base analytical Jacobian at "from" is matching forward differentiation
            output[outputIndex] += slope * value + cut;
        }
        else if (belowRange)
        {
            output[outputIndex] += slope * from + cut; // clamp to minimum range value
        }
        else if (aboveRange)
        {
            output[outputIndex] += slope * to + cut; // clamp to maximum range value
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (guiControls.HasJacobian())
    {
        // fill jacobian matrix directly as gui to raw mappings are ordered
        SparseMatrix<T> localJacobian(m->rawControlCount, guiControls.Size());
        localJacobian.reserve(numGuiToRawMappings);
        int prevRowIndex = -1;
        for (int i = 0; i < numGuiToRawMappings; i++)
        {
            const int inputIndex = m->guiToRawMapping[i].inputIndex;
            const int outputIndex = m->guiToRawMapping[i].outputIndex;
            const T from = m->guiToRawMapping[i].from;
            const T to = m->guiToRawMapping[i].to;
            const T slope = m->guiToRawMapping[i].slope;
            const T value = guiControls.Value()[inputIndex];
            const T rangeStart = m->guiControlRanges(0, inputIndex);
            const T rangeEnd = m->guiControlRanges(1, inputIndex);
            const bool belowRange = (from == rangeStart && value < from);
            const bool aboveRange = (to == rangeEnd && value >= to);
            while (prevRowIndex < outputIndex)
            {
                localJacobian.startVec(++prevRowIndex);
            }
            if ((from <= value) && (value < to))
            {
                localJacobian.insertBackByOuterInner(outputIndex, inputIndex) = slope;
            }
            else if (belowRange || aboveRange)
            {
                // When the GUI control is out of bounds then the raw control is clamped and technically
                // the Jacobian would be zero. However this would mean that any optimization that uses
                // the control would not have an "incentive" to move the control back inside the bounds.
                // Therefore we keep the Jacobian for these bounds, but any optimization needs
                // to enforce that the GUI controls stay within the bounds.
                // triplets.push_back(Eigen::Triplet<T>(outputIndex, inputIndex, slope));
                localJacobian.insertBackByOuterInner(outputIndex, inputIndex) = slope;
            }
        }
        localJacobian.finalize();
        Jacobian = guiControls.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(output), std::move(Jacobian));
}

template <class T>
DiffData<T> RigLogic<T>::EvaluateRawRBFControls(const DiffData<T>& eulerRBFControls) const
{
    return m->rbfLogic.EvaluateRawControlsFromEuler(eulerRBFControls);
}

template <class T>
DiffData<T> RigLogic<T>::EvaluatePSD(const DiffData<T>& rawControls, const Eigen::VectorX<T>& maskWeights) const
{
    if (rawControls.Size() != m->rawControlCount)
    {
        CARBON_CRITICAL("raw control count incorrect {} instead of {}", rawControls.Size(), m->rawControlCount);
    }

    Vector<T> output(m->totalControlCount);

    JacobianConstPtr<T> Jacobian;
    const bool hasJacobian = rawControls.HasJacobian();

    // copy raw controls
    output.head(m->rawControlCount) = rawControls.Value().head(m->rawControlCount);

    // evaluate rbf pose controls
    const auto rbfPoseControls = m->rbfLogic.EvaluatePoseControlsFromRawControls(rawControls);
    if (rbfPoseControls.Size() == m->rbfPoseControlCount)
    {
        output.segment(m->totalControlCount - m->rbfPoseControlCount, m->rbfPoseControlCount) = rbfPoseControls.Value();
    }
    else
    {
        if (rbfPoseControls.Size() != 0)
        {
            LOG_ERROR("invalid rbf pose controls size: {}, expected {}", rbfPoseControls.Size(), m->rbfPoseControlCount);
        }
        output.segment(m->totalControlCount - m->rbfPoseControlCount, m->rbfPoseControlCount).setZero();
    }

    // evaluate ml
    std::vector<Eigen::Matrix<T, -1, -1>> neuralNetJacobians(m->neuralNets.size());
    output.segment(m->totalControlCount - m->mlControlCount - m->rbfPoseControlCount, m->mlControlCount).setZero();
    for (int netIndex = 0; netIndex < (int)m->neuralNets.size(); ++netIndex)
    {
        Eigen::Matrix<T, -1, -1> neuralNetJacobian;
        const T maskWeight = (netIndex < (int)maskWeights.size()) ? maskWeights[netIndex] : T(1);
        if (maskWeight > 0)
        {
            const auto& net = m->neuralNets[netIndex];
            const Eigen::VectorX<T> input = output(net.m_inputIndices);
            Eigen::VectorX<T> layerInput = input;
            Eigen::VectorX<T> layerOutput;
            if (hasJacobian)
            {
                neuralNetJacobian = Eigen::Matrix<T, -1, -1>::Identity(layerInput.size(), layerInput.size());
            }
            for (int layerIndex = 0; layerIndex < (int)net.m_layerWeights.size(); ++layerIndex)
            {
                layerOutput = net.m_layerWeights[layerIndex] * layerInput + net.m_layerBiases[layerIndex];
                if (hasJacobian)
                {
                    neuralNetJacobian = net.m_layerWeights[layerIndex] * neuralNetJacobian;
                }
                if (net.m_layerActivationFunctions[layerIndex] == dna::ActivationFunction::linear)
                {
                }
                else if (net.m_layerActivationFunctions[layerIndex] == dna::ActivationFunction::relu)
                {
                    if (hasJacobian)
                    {
                        for (int k = 0; k < layerOutput.size(); ++k)
                        {
                            if (layerOutput[k] < 0)
                            {
                                neuralNetJacobian.row(k).setZero();
                            }
                        }
                    }
                    layerOutput = layerOutput.array().max(T(0));
                }
                else
                {
                    CARBON_CRITICAL("unsupported activation net {} and layer {}: {}", netIndex, layerIndex, net.m_layerActivationFunctions[layerIndex]);
                }
                std::swap(layerInput, layerOutput);
            }

            output(net.m_outputIndices) = maskWeight * layerInput;
            if (hasJacobian)
            {
                neuralNetJacobians[netIndex] = neuralNetJacobian * maskWeight;
            }
        }
    }

    // evaluate psd controls (can use ml or rbf controls as input)
    for (int k = m->rawControlCount; k < int(m->psdToRawMap.outerSize()); ++k)
    {
        if (NumNonzerosForRow(m->psdToRawMap, k) > 0)
        {
            T weight = T(1);
            for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, k); it; ++it)
            {
                // this can be an index into the raw, rbf, or ml controls
                weight *= std::clamp<T>(output[it.col()], 0, T(1)) * it.value();
            }
            output[k] = weight;
        }
        else
        {
            output[k] = T(0);
        }
    }

    // get jacobian
    if (hasJacobian)
    {
        SparseMatrix<T> localJacobian(m->totalControlCount, m->rawControlCount);
        localJacobian.reserve(m->psdToRawMap.nonZeros());

        int rowIndex = 0;
        for (int k = 0; k < int(m->psdToRawMap.outerSize()); ++k)
        {
            localJacobian.startVec(rowIndex);
            const T weight = output[k];
            if (NumNonzerosForRow(m->psdToRawMap, k) > 0 && (weight >= 0) && (weight <= T(1)))
            {
                // Note that even if the corrective value is zero, the Jacobian can be valid. For example a corrective
                // corr(A, B) = A * B. If A is 1, and B is 0, then the derivative of corr(A, B) with respect to B is A.
                for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, k); it; ++it)
                {
                    T accValue = T(1);
                    for (typename SparseMatrix<T>::InnerIterator it2(m->psdToRawMap, k); it2; ++it2)
                    {
                        if (it.col() != it2.col())
                        {
                            accValue *= std::clamp<T>(output[it2.col()], 0, T(1)) * it2.value();
                        }
                        else
                        {
                            accValue *= it2.value();
                        }
                    }
                    if (accValue > 0) // we can discard 0 values as it is a sparse matrix, and accValue is never negative as all controls are >= 0
                    {
                        localJacobian.insertBackByOuterInner(rowIndex, it.col()) = accValue;
                    }
                }
            }
            else
            {
                //CARBON_ASSERT(weight >= 0, "raw control outputs can only be 0 or positive, never negative");
                // If we are outside of bounds then correctives do not have an impact on the Jacobian
                // as a tiny delta on any of the values will not move the corrective output to within bounds.
                // For the value of 1 we keep the Jacobian valid as a tiny negative delta will change the corrective
            }
            rowIndex++;
        }

        // insert neural net jacobian values in the right order (sorted by outputIndex and inputIndex)
        for (const auto& [outputIndex, netIndex, indexOfNetOutput] : m->neuralNetOutputOrderedByOutputIndex)
        {
            if (outputIndex < rowIndex)
            {
                CARBON_CRITICAL("invalid output index!");
            }
            while (rowIndex < outputIndex)
            {
                localJacobian.startVec(rowIndex);
                rowIndex++;
            }
            localJacobian.startVec(rowIndex);
            if (neuralNetJacobians[netIndex].size() > 0)
            {
                for (auto&& [controlInputIndex, indexOfNetInput] : m->neuralNetControlInputIndexAndNetInputIndex[netIndex])
                {
                    localJacobian.insertBackByOuterInner(rowIndex, controlInputIndex) = neuralNetJacobians[netIndex](indexOfNetOutput, indexOfNetInput);
                }
            }
            rowIndex++;
        }
        localJacobian.finalize();
        Jacobian = std::move(rawControls.Jacobian().Premultiply(localJacobian));
    }

    // clamp correctives
    for (int k = m->rawControlCount; k < int(m->psdToRawMap.outerSize()); ++k)
    {
        output[k] = clamp<T>(output[k], 0, 1);
    }

    return DiffData<T>(std::move(output), std::move(Jacobian));
}

// template <class T>
// DiffData<T> RigLogic<T>::EvaluateBlendshapes(const DiffData<T>& psdControls, int lod) const
// {
// if (psdControls.Size() != m->totalControlCount) {
// CARBON_CRITICAL("RigLogic::EvaluateBlendshapes(): psd control count incorrect");
// }
// if (lod < 0 || lod >= m->numLODs) {
// CARBON_CRITICAL("RigLogic::EvaluateBlendshapes(): invalid lod");
// }

// Vector<T> output = Vector<T>::Zero(m->blendshapeMapping.cols());
// output->setZero();

// for (int i = 0; i < m->blendshapesPerLOD[lod]; i++) {
// output[m->blendshapeMapping(1, i)] = psdControls.Value()[m->blendshapeMapping(0, i)];
// }

// JacobianConstPtr<T> Jacobian;
// if (psdControls.HasJacobian()) {
// SparseMatrix<T> localJacobian(m->blendshapeMapping.cols(), psdControls.Size());
// std::vector<Eigen::Triplet<T>> triplets;
// for (int i = 0; i < m->blendshapesPerLOD[lod]; i++) {
// triplets.push_back(Eigen::Triplet<T>(m->blendshapeMapping(1, i), m->blendshapeMapping(0, i), T(1)));
// }
// localJacobian.setFromTriplets(triplets.begin(), triplets.end());
// Jacobian = psdControls.Jacobian().Premultiply(localJacobian);
// }

// return DiffData<T>(std::move(output), Jacobian);
// }

template <class T>
DiffData<T> RigLogic<T>::EvaluateJoints(const DiffData<T>& psdControls, int lod) const
{
    if (psdControls.Size() != m->totalControlCount)
    {
        CARBON_CRITICAL("RigLogic::EvaluateJoints(): psd control count incorrect");
    }
    if ((lod < 0) || (lod >= m->numLODs))
    {
        CARBON_CRITICAL("RigLogic::EvaluateJoints(): invalid lod");
    }

    Eigen::VectorX<T> output = m->jointMatrixPerLOD[lod] * psdControls.Value();

    for (std::uint16_t si = 0u; si < m->rbfLogic.EulerToRaw().size(); ++si)
    {
        const auto& mapping =  m->rbfLogic.EulerToRaw()[si];
        const float x = static_cast<float>(psdControls.Value()[mapping.rawX]);
        const float y = static_cast<float>(psdControls.Value()[mapping.rawY]);
        const float z = static_cast<float>(psdControls.Value()[mapping.rawZ]);
        const float w = static_cast<float>(psdControls.Value()[mapping.rawW]);
        // Since joints are directly driven by euler controls we need to update them as well
        const auto euler =  tdm::quat<float>(x,y,z,w).euler<tdm::rot_seq::xyz>();
        const std::uint16_t drivingJointIndex = mapping.jointIndex;
        output[drivingJointIndex * (m->withJointScaling ? 9 : 6) + 3] = euler[0].value;
        output[drivingJointIndex * (m->withJointScaling ? 9 : 6) + 4] = euler[1].value;
        output[drivingJointIndex * (m->withJointScaling ? 9 : 6) + 5] = euler[2].value;
    }

    JacobianConstPtr<T> Jacobian;
    if (psdControls.HasJacobian())
    {
        Jacobian = psdControls.Jacobian().Premultiply(m->jointMatrixPerLOD[lod]);
    }

    return DiffData<T>(std::move(output), Jacobian);
}

template <class T>
DiffData<T> RigLogic<T>::EvaluateJoints(const DiffData<T>& psdControls, const DiffDataSparseMatrix<T>& jointMatrix) const
{
    if (psdControls.Size() != jointMatrix.Cols())
    {
        CARBON_CRITICAL("RigLogic::EvaluateJoints(): psd control count incorrect");
    }
    if (jointMatrix.Cols() != psdControls.Size())
    {
        CARBON_CRITICAL("RigLogic::EvaluateJoints(): jointMatrix cols != psdControls size");
    }
    return jointMatrix.Multiply(psdControls);
}

template <class T>
DiffData<T> RigLogic<T>::EvaluateAnimatedMaps(const DiffData<T>& psdControls, int lod) const
{
    if (psdControls.Size() != m->totalControlCount)
    {
        CARBON_CRITICAL("RigLogic::EvaluateAnimatedMaps(): psd control count incorrect");
    }
    if ((lod < 0) || (lod >= m->numLODs))
    {
        CARBON_CRITICAL("RigLogic::EvaluateAnimatedMaps(): invalid lod");
    }

    Vector<T> output = Vector<T>::Zero(m->numAnimatedMaps);

    // evaluate animated maps
    for (int i = 0; i < m->animatedMapsPerLOD[lod]; i++)
    {
        const int inputIndex = m->animatedMapsMapping(0, i);
        const int outputIndex = m->animatedMapsMapping(1, i);
        const T from = m->animatedMapsValues(0, i);
        const T to = m->animatedMapsValues(1, i);
        const T slope = m->animatedMapsValues(2, i);
        const T cut = m->animatedMapsValues(3, i);
        const T value = psdControls.Value()[inputIndex];
        if ((from < value) && (value <= to))
        {
            output[outputIndex] = output[outputIndex] + slope * value + cut;
        }
    }
    // replace the clamp within the loop above once it has been fixed in RigLogic
    for (int i = 0; i < m->numAnimatedMaps; i++)
    {
        output[i] = clamp<T>(output[i], T(0), T(1));
    }

    JacobianConstPtr<T> Jacobian;
    if (psdControls.HasJacobian())
    {
        SparseMatrix<T> localJacobian(m->numAnimatedMaps, psdControls.Size());
        std::vector<Eigen::Triplet<T>> triplets;
        for (int i = 0; i < m->animatedMapsPerLOD[lod]; i++)
        {
            const int inputIndex = m->animatedMapsMapping(0, i);
            const int outputIndex = m->animatedMapsMapping(1, i);
            const T from = m->animatedMapsValues(0, i);
            const T to = m->animatedMapsValues(1, i);
            const T slope = m->animatedMapsValues(2, i);
            const T value = psdControls.Value()[inputIndex];
            if ((from < value) && (value <= to))
            {
                // note that at this point the value may be clamped and therefore the Jacobian is techincally 0. However this would
                // mean that the value could never be pulled back within bounds
                // const T outputValue = (*output)[outputIndex];
                // if (outputValue >= 0 && outputValue < 1) {
                triplets.push_back(Eigen::Triplet<T>(outputIndex, inputIndex, slope));
                // }
            }
        }
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = psdControls.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(output), Jacobian);
}

template <class T>
int RigLogic<T>::NumLODs() const
{
    return m->numLODs;
}

template <class T>
int RigLogic<T>::NumJoints() const
{
    return m->numJoints;
}

template <class T>
const SparseMatrix<T>& RigLogic<T>::PsdToRawMap() const
{
    return m->psdToRawMap;
}

template <class T>
const SparseMatrix<T>& RigLogic<T>::JointMatrix(int lod) const
{
    return m->jointMatrixPerLOD[lod];
}

template <class T>
void RigLogic<T>::SetJointMatrix(int lod, const SparseMatrix<T>& mat)
{
    m->jointMatrixPerLOD[lod] = mat;
}

template <class T>
void RigLogic<T>::ReduceToLOD0Only()
{
    m->numLODs = 1;
    // m->blendshapesPerLOD.conservativeResize(1);
    m->jointMatrixPerLOD.resize(1);
    m->animatedMapsPerLOD.conservativeResize(1);
}

template <class T>
std::vector<std::tuple<int, int, Eigen::VectorX<T>>> RigLogic<T>::GetAllExpressions() const
{
    std::vector<std::tuple<int, int, Eigen::VectorX<T>>> psds;

    // push all combinations (excluding those that depend expressions that depend on rgb/ml controls)
    for (int k = 0; k < int(m->psdToRawMap.outerSize()); ++k)
    {
        Eigen::VectorX<T> rawControls = Eigen::Vector<T, -1>::Zero(m->rawControlCount);
        int controlCount = 0;
        bool valid = true;
        for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, k); it; ++it)
        {
            if (it.col() < (int)rawControls.size())
            {
                rawControls[it.col()] = T(1) / it.value();
                controlCount++;
            }
            else
            {
                valid = false;
            }
        }
        if (valid)
        {
            psds.push_back({ controlCount, k, rawControls });
        }
    }

    // sort the expressions by the number of raw controls that are affecting the expression
    std::sort(psds.begin(), psds.end(), [](const auto& a, const auto& b) {
            return (std::get<0>(a) < std::get<0>(b)) || (std::get<0>(a) == std::get<0>(b) && (std::get<1>(a) < std::get<1>(b)));
        });

    return psds;
}

template <class T>
void RigLogic<T>::RemoveJoints(const std::vector<int>& newToOldJointMapping)
{
    LOG_VERBOSE("remove {} out of {} joints", m->numJoints - int(newToOldJointMapping.size()), m->numJoints);
    m->numJoints = int(newToOldJointMapping.size());
    const int dofPerJoint = (m->withJointScaling ? 9 : 6);
    for (auto& smat : m->jointMatrixPerLOD)
    {
        std::vector<Eigen::Triplet<T>> triplets;
        for (int newIdx = 0; newIdx < int(newToOldJointMapping.size()); ++newIdx)
        {
            const int oldIdx = newToOldJointMapping[newIdx];
            for (int k = 0; k < dofPerJoint; ++k)
            {
                for (typename SparseMatrix<T>::InnerIterator it(smat, oldIdx * dofPerJoint + k); it; ++it)
                {
                    triplets.push_back(Eigen::Triplet<T>(newIdx * dofPerJoint + k, int(it.col()), it.value()));
                }
            }
        }
        smat.resize(m->numJoints * dofPerJoint, smat.cols());
        smat.setFromTriplets(triplets.begin(), triplets.end());
    }
    m->rbfLogic.RemoveJoints(newToOldJointMapping);
}

template <class T>
Eigen::VectorX<T> RigLogic<T>::GuiControlsFromRawControls(const Eigen::VectorX<T>& rawControls, std::vector<int>& inconsistentGuiControls) const
{
    inconsistentGuiControls.clear();

    std::vector<std::vector<T>> candidatesPerControl(m->guiControlCount);
    std::vector<std::vector<Eigen::Vector2<T>>> candidateRangesPerControl(m->guiControlCount);
    std::vector<bool> usedGuiControl(m->guiControlCount, false);
    for (int i = 0; i < int(m->guiToRawMapping.size()); i++)
    {
        const int inputIndex = m->guiToRawMapping[i].inputIndex;
        usedGuiControl[inputIndex] = true;
        const int outputIndex = m->guiToRawMapping[i].outputIndex;
        const T from = m->guiToRawMapping[i].from;
        const T to = m->guiToRawMapping[i].to;
        const T slope = m->guiToRawMapping[i].slope;
        const T cut = m->guiToRawMapping[i].cut;
        const T outputValue = rawControls[outputIndex];
        const T outputValueFrom = slope * from + cut;
        const T outputValueTo = slope * to + cut;
        const T outputValueMin = std::min<T>(outputValueFrom, outputValueTo);
        const T outputValueMax = std::max<T>(outputValueFrom, outputValueTo);
        if (slope != 0)
        {
            if ((outputValueMin < outputValue) && (outputValue < outputValueMax))
            {
                const T inputValue = clamp((outputValue - cut) / slope, from, to);
                candidatesPerControl[inputIndex].push_back(inputValue);
            }
            else if (fabs(outputValueFrom - outputValue) < T(1e-6))
            {
                candidateRangesPerControl[inputIndex].push_back(Eigen::Vector2<T>(m->guiControlRanges(0, inputIndex), from));
            }
            else if (fabs(outputValueTo - outputValue) < T(1e-6))
            {
                candidateRangesPerControl[inputIndex].push_back(Eigen::Vector2<T>(to, m->guiControlRanges(1, inputIndex)));
            }
        }
    }

    Eigen::VectorX<T> guiControls = Eigen::VectorX<T>::Zero(m->guiControlCount);
    for (int i = 0; i < m->guiControlCount; ++i)
    {
        const int guiControlIndex = int(i);
        const std::vector<T>& candidates = candidatesPerControl[i];
        const std::vector<Eigen::Vector2<T>>& candidateRanges = candidateRangesPerControl[i];

        if (candidates.empty() && candidateRanges.empty())
        {
            if (usedGuiControl[i])
            {
                inconsistentGuiControls.push_back(guiControlIndex);
                LOG_WARNING("gui control {} ({}) is not mapped by any raw control", m->guiControlNames[i], i);
            }
        }
        else if ((candidates.size() == 1) && candidateRanges.empty())
        {
            guiControls[guiControlIndex] = candidates.front();
        }
        else if (candidates.empty() && (candidateRanges.size() == 1))
        {
            if (candidateRanges.front()[1] - candidateRanges.front()[0] > T(1e-6))
            {
                inconsistentGuiControls.push_back(guiControlIndex);
                LOG_WARNING("gui control {} ({}) is not mapped uniquely", m->guiControlNames[i], i);
            }
            guiControls[guiControlIndex] = candidateRanges.front().mean();
        }
        else
        {
            // generate candidate points
            std::vector<T> candidateValues;
            std::vector<int> scores;

            auto indexForCandidateValue = [&](T value) {
                    for (size_t j = 0; j < candidateValues.size(); ++j)
                    {
                        if (candidateValues[j] == value) { return (int)j; }
                    }
                    return -1;
                };

            // add candidate points
            for (size_t k = 0; k < candidates.size(); ++k)
            {
                const T candidateValue = candidates[k];
                const int currIndex = indexForCandidateValue(candidateValue);
                if (currIndex >= 0)
                {
                    scores[currIndex]++;
                }
                else
                {
                    candidateValues.push_back(candidateValue);
                    scores.push_back(1);
                }
            }

            // create candidate points for start and end of the ranges
            for (size_t k = 0; k < candidateRanges.size(); ++k)
            {
                for (int j = 0; j < 2; ++j)
                {
                    if (indexForCandidateValue(candidateRanges[k][j]) < 0)
                    {
                        candidateValues.push_back(candidateRanges[k][j]);
                        scores.push_back(0);
                    }
                }
            }

            // score all candidate points based on ranges
            for (size_t j = 0; j < candidateValues.size(); ++j)
            {
                for (size_t k = 0; k < candidateRanges.size(); ++k)
                {
                    if ((candidateValues[j] >= candidateRanges[k][0]) && (candidateValues[j] <= candidateRanges[k][1]))
                    {
                        scores[j]++;
                    }
                }
            }

            if (candidateValues.size() > 1)
            {
                // use the candidate with the most votes
                std::vector<int> indices(scores.size());
                std::iota(indices.begin(), indices.end(), 0);
                std::sort(indices.begin(), indices.end(), [&](int i1, int i2) {
                        if (scores[i1] == scores[i2]) { return fabs(candidateValues[i1]) < fabs(candidateValues[i2]); }
                        else { return scores[i1] > scores[i2]; }
                    });
                guiControls[i] = candidateValues[indices[0]];
                if (scores[indices[0]] == scores[indices[1]])
                {
                    if ((m->guiControlUseCount[guiControlIndex] < 2) || (candidateValues[indices[0]] != 0))
                    {
                        // only warn complex controls if they are non-zero
                        inconsistentGuiControls.push_back(guiControlIndex);
                        LOG_WARNING("gui control {} ({}) is not uniquely determined ({} score, values: {} {})",
                                    m->guiControlNames[i],
                                    i,
                                    scores[indices[0]],
                                    candidateValues[indices[0]],
                                    candidateValues[indices[1]]);
                    }
                }
            }
            else if (candidateValues.size() == 1)
            {
                LOG_WARNING("single candidate for control {} ({})", m->guiControlNames[i], i);
                guiControls[guiControlIndex] = candidateValues.front();
            }
            else
            {
                inconsistentGuiControls.push_back(guiControlIndex);
                LOG_WARNING("no candidate for control {} ({})", m->guiControlNames[i], i);
            }
        }
    }
    return guiControls;
}

template <class T>
std::vector<int> RigLogic<T>::UnusedGuiControls() const
{
    std::vector<bool> guiControlUsed(m->guiControlCount, false);

    for (int i = 0; i < int(m->guiToRawMapping.size()); i++)
    {
        guiControlUsed[m->guiToRawMapping[i].inputIndex] = true;
    }
    std::vector<int> controls;
    for (int i = 0; i < m->guiControlCount; ++i)
    {
        if (!guiControlUsed[i])
        {
            controls.push_back(i);
        }
    }
    return controls;
}

template <class T>
std::vector<int> RigLogic<T>::UnusedRawControls() const
{
    std::vector<bool> rawControlUsed(m->rawControlCount, false);

    for (int i = 0; i < int(m->guiToRawMapping.size()); i++)
    {
        rawControlUsed[m->guiToRawMapping[i].outputIndex] = true;
    }

    std::vector<int> controls;
    for (int i = 0; i < m->rawControlCount; ++i)
    {
        if (!rawControlUsed[i])
        {
            controls.push_back(i);
        }
    }
    return controls;
}

template <class T>
void RigLogic<T>::ReduceToGuiControls(const std::vector<int>& guiControls)
{
    std::vector<typename Private::GuiToRawInfo> guiToRawMapping;
    std::set<int> guiControlsToKeep(guiControls.begin(), guiControls.end());
    for (const typename Private::GuiToRawInfo& guiToRawInfo : m->guiToRawMapping)
    {
        if (guiControlsToKeep.find(guiToRawInfo.inputIndex) != guiControlsToKeep.end())
        {
            guiToRawMapping.push_back(guiToRawInfo);
        }
    }
    LOG_VERBOSE("reducing gui to raw control mapping from {} to {} mappings", m->guiToRawMapping.size(), guiToRawMapping.size());
    m->guiToRawMapping = std::move(guiToRawMapping);

    m->guiControlUseCount = std::vector<int>(m->guiControlCount, 0);
    std::vector<int> rawControlUseCount(m->rawControlCount, 0);

    for (int i = 0; i < int(m->guiToRawMapping.size()); i++)
    {
        m->guiControlUseCount[m->guiToRawMapping[i].inputIndex]++;
        rawControlUseCount[m->guiToRawMapping[i].outputIndex]++;
    }

    // update psd matrix
    std::vector<bool> psdsUsed(m->psdToRawMap.outerSize(), false);
    {
        std::vector<Eigen::Triplet<T>> triplets;
        int unusedPsd = 0;
        for (int k = 0; k < int(m->psdToRawMap.outerSize()); ++k)
        {
            bool psdUsed = true;
            for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, k); it; ++it)
            {
                // remove unused raw controls, and controls that are part of ml or rbf
                if (it.col() >= (int)rawControlUseCount.size() || !rawControlUseCount[it.col()]) { psdUsed = false; }
            }
            if (psdUsed)
            {
                psdsUsed[k] = true;
                for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, k); it; ++it)
                {
                    triplets.push_back(Eigen::Triplet<T>(k, int(it.col()), it.value()));
                }
            }
            else
            {
                unusedPsd++;
            }
        }
        SparseMatrix<T> psdToRawMap(m->psdToRawMap.rows(), m->psdToRawMap.cols());
        psdToRawMap.setFromTriplets(triplets.begin(), triplets.end());
        LOG_VERBOSE("reduced psd matrix from {} to {} non-zero rows", psdToRawMap.rows(), int(psdToRawMap.rows()) - unusedPsd);
        m->psdToRawMap = std::move(psdToRawMap);
    }

    // update joint matrix (remove unsed PSD values)
    {
        for (size_t lod = 0; lod < m->jointMatrixPerLOD.size(); ++lod)
        {
            if (m->jointMatrixPerLOD[lod].nonZeros() > 0)
            {
                std::vector<Eigen::Triplet<T>> triplets;
                for (int row = 0; row < int(m->jointMatrixPerLOD[lod].rows()); ++row)
                {
                    for (typename SparseMatrix<T>::InnerIterator it(m->jointMatrixPerLOD[lod], row); it; ++it)
                    {
                        if (psdsUsed[it.col()])
                        {
                            triplets.push_back(Eigen::Triplet<T>(row, int(it.col()), it.value()));
                        }
                    }
                }
                SparseMatrix<T> smat(m->jointMatrixPerLOD[lod].rows(), m->jointMatrixPerLOD[lod].cols());
                smat.setFromTriplets(triplets.begin(), triplets.end());
                LOG_VERBOSE("reduced number of nonzeros for lod{} joint matrix matrix from {} to {} entries",
                            lod,
                            m->jointMatrixPerLOD[lod].nonZeros(),
                            smat.nonZeros());
                m->jointMatrixPerLOD[lod] = std::move(smat);
            }
        }
    }

    // remove rbf logic
    m->rbfLogic = RBFLogic<T>();
}

template <class T>
std::vector<int> RigLogic<T>::UnmappedJoints() const
{
    std::vector<bool> jointUsed(m->numJoints, false);
    for (size_t lod = 0; lod < m->jointMatrixPerLOD.size(); ++lod)
    {
        for (int row = 0; row < int(m->jointMatrixPerLOD[lod].rows()); ++row)
        {
            for (typename SparseMatrix<T>::InnerIterator it(m->jointMatrixPerLOD[lod], row); it; ++it)
            {
                jointUsed[row / (m->withJointScaling ? 9 : 6)] = true;
            }
        }
    }

    std::vector<int> unmappedJoints;
    for (int jointIndex = 0; jointIndex < (int)jointUsed.size(); ++jointIndex)
    {
        if (!jointUsed[jointIndex])
        {
            unmappedJoints.push_back(jointIndex);
        }
    }
    return unmappedJoints;
}

template <class T>
int RigLogic<T>::GuiControlIndex(const char* name) const
{
    for (int i = 0; i < (int)NumGUIControls(); ++i)
    {
        if (GuiControlNames()[i] == name) { return i; }
    }
    return -1;
}

template <class T>
int RigLogic<T>::RawControlIndex(const char* name) const
{
    for (int i = 0; i < (int)NumRawControls(); ++i)
    {
        if (RawControlNames()[i] == name) { return i; }
    }
    return -1;
}

template <class T>
void RigLogic<T>::MirrorJoints(const std::vector<int>& symmetricJointIndices)
{
    const int dofPerJoint = (m->withJointScaling ? 9 : 6);
    const std::vector<int> symmetricPsdIndices = GetSymmetricPsdIndices();

    for (auto& smat : m->jointMatrixPerLOD)
    {
        std::vector<Eigen::Triplet<T>> triplets;
        for (int jointIndex = 0; jointIndex < m->numJoints; ++jointIndex)
        {
            const int mirroredJointIndex = symmetricJointIndices[jointIndex];
            for (int dof = 0; dof < dofPerJoint; ++dof)
            {
                if (dof == 0)
                {
                    // mirror x translation
                    for (typename SparseMatrix<T>::InnerIterator it(smat, jointIndex * dofPerJoint + dof); it; ++it)
                    {
                        const int symmetricIndex = symmetricPsdIndices[it.col()];
                        triplets.push_back(Eigen::Triplet<T>(mirroredJointIndex * dofPerJoint + dof, symmetricIndex, -it.value()));
                    }
                }
                else if (dof == 4)
                {
                    // mirror y rotation
                    for (typename SparseMatrix<T>::InnerIterator it(smat, jointIndex * dofPerJoint + dof); it; ++it)
                    {
                        const int symmetricIndex = symmetricPsdIndices[it.col()];
                        triplets.push_back(Eigen::Triplet<T>(mirroredJointIndex * dofPerJoint + dof, symmetricIndex, -it.value()));
                    }
                }
                else if (dof == 5)
                {
                    // mirror z rotation
                    for (typename SparseMatrix<T>::InnerIterator it(smat, jointIndex * dofPerJoint + dof); it; ++it)
                    {
                        const int symmetricIndex = symmetricPsdIndices[it.col()];
                        triplets.push_back(Eigen::Triplet<T>(mirroredJointIndex * dofPerJoint + dof, symmetricIndex, -it.value()));
                    }
                }
                else
                {
                    for (typename SparseMatrix<T>::InnerIterator it(smat, jointIndex * dofPerJoint + dof); it; ++it)
                    {
                        const int symmetricIndex = symmetricPsdIndices[it.col()];
                        triplets.push_back(Eigen::Triplet<T>(mirroredJointIndex * dofPerJoint + dof, symmetricIndex, it.value()));
                    }
                }
            }
        }
        smat.resize(m->numJoints * dofPerJoint, smat.cols());
        smat.setFromTriplets(triplets.begin(), triplets.end());
    }
}

template <class T>
std::vector<std::pair<int, T>> RigLogic<T>::GetSymmetricGuiControlIndices() const
{
    const std::vector<int> symmetricRawControls = GetSymmetricRawControlIndices();
    const std::vector<std::vector<int>> usedRawControls = GetUsedRawControls();

    std::vector<std::pair<int, T>> symmetricIndicesAndMultipliers;
    for (int i = 0; i < NumGUIControls(); ++i)
    {
        const std::string name = GuiControlNames()[i];
        const std::vector<std::string> tokens = TITAN_NAMESPACE::Split(name, "_");
        std::vector<std::string> mirroredTokens;
        for (const std::string& token : tokens)
        {
            if (token == "R") { mirroredTokens.push_back("L"); }
            else if (token == "r") { mirroredTokens.push_back("l"); }
            else if (token == "L") { mirroredTokens.push_back("R"); }
            else if (token == "l") { mirroredTokens.push_back("r"); }
            else { mirroredTokens.push_back(token); }
        }
        std::string mirrorName;
        for (size_t k = 0; k < mirroredTokens.size(); ++k)
        {
            if (k > 0) { mirrorName.append("_"); }
            mirrorName.append(mirroredTokens[k]);
        }
        if (mirrorName != name)
        {
            const int mirrorIndex = TITAN_NAMESPACE::GetItemIndex<std::string>(GuiControlNames(), mirrorName);
            if (mirrorIndex >= 0)
            {
                T multiplier = T(1);
                if ((name == "CTRL_L_eye.tx") || (name == "CTRL_R_eye.tx"))
                {
                    // special handling for left/right eye control
                    multiplier = T(-1);
                }
                symmetricIndicesAndMultipliers.push_back({ mirrorIndex, multiplier });
                // LOG_VERBOSE("\"{}\" is symmetric to \"{}\" (using multiplier {})", name, mirrorName, multiplier);
                // check if all used raw controls are also symmetric
                for (int rawControlIdx : usedRawControls[i])
                {
                    if (symmetricRawControls[rawControlIdx] == rawControlIdx)
                    {
                        CARBON_CRITICAL("symmetric \"{}\" uses raw control \"{}\" that is not symmetric", name, RawControlNames()[rawControlIdx]);
                    }
                }
            }
            else
            {
                CARBON_CRITICAL("no symmetry for \"{}\" (searched \"{}\")", name, mirrorName);
            }
        }
        else
        {
            // check if all used raw controls are also self symmetric
            std::set<int> usedSymmetricRawControls;
            size_t count = 0;
            for (int rawControlIdx : usedRawControls[i])
            {
                if (symmetricRawControls[rawControlIdx] != rawControlIdx)
                {
                    count++;
                    usedSymmetricRawControls.insert(rawControlIdx);
                    usedSymmetricRawControls.insert(symmetricRawControls[rawControlIdx]);
                }
            }
            if (count > 0)
            {
                if (usedSymmetricRawControls.size() == count)
                {
                    if ((m->guiControlRanges(0, i) < 0) && (m->guiControlRanges(1, i) > 0))
                    {
                        // LOG_VERBOSE("\"{}\" is self-symmetric, but using symmetric raw controls", name);
                        symmetricIndicesAndMultipliers.push_back({ i, T(-1) });
                    }
                    else
                    {
                        CARBON_CRITICAL("self-symmetric \"{}\" uses raw control \"{1}\" that is not self symmetric", name);
                    }
                }
                else
                {
                    CARBON_CRITICAL("self-symmetric \"{}\" uses raw controls that are not self symmetric", name);
                }
            }
            else
            {
                symmetricIndicesAndMultipliers.push_back({ i, T(1) });
            }
            // LOG_VERBOSE("\"{}\" is self-symmetric (using multiplier {})", name, symmetricIndicesAndMultipliers.back().second);
        }
    }
    if ((int)symmetricIndicesAndMultipliers.size() != NumGUIControls())
    {
        CARBON_CRITICAL("logical error");
    }
    return symmetricIndicesAndMultipliers;
}

template <class T>
std::vector<int> RigLogic<T>::GetSymmetricRawControlIndices() const
{
    std::vector<int> symmetricIndices(NumRawControls(), -1);
    for (int i = 0; i < NumRawControls(); ++i)
    {
        const std::string name = RawControlNames()[i];
        const std::vector<std::pair<std::string, std::string>> suffixPairs = {
            { "L", "R" },
            { "Left", "Right" },
            { "LeftU", "RightU" },
            { "LeftD", "RightD" },
            { "LPh1", "RPh1" },
            { "LPh2", "RPh2" },
            { "LPh3", "RPh3" },
            { "R", "L" },
            { "RPh1", "LPh1" },
            { "RPh2", "LPh2" },
            { "RPh3", "LPh3" },
            { "Right", "Left" },
            { "RightU", "LeftU" },
            { "RightD", "LeftD" },
            // special handling for eye left/right
            { "LookLeftL", "LookRightR" },
            { "LookRightR", "LookLeftL" },
            { "LookLeftR", "LookRightL" },
            { "LookRightL", "LookLeftR" }
        };
        int mirrorIndex = -1;
        for (const auto& [suffix1, suffix2] : suffixPairs)
        {
            if (TITAN_NAMESPACE::StringEndsWith(name, suffix1))
            {
                const std::string mirrorName = name.substr(0, name.size() - suffix1.size()) + suffix2;
                mirrorIndex = TITAN_NAMESPACE::GetItemIndex<std::string>(RawControlNames(), mirrorName);
                if (mirrorIndex >= 0)
                {
                    symmetricIndices[i] = mirrorIndex;
                    // LOG_VERBOSE("\"{}\" is symmetric to \"{}\"", name, mirrorName);
                }
                else
                {
                    CARBON_CRITICAL("no symmetry for {} (searched {})", name, mirrorName);
                }
            }
        }
        if (mirrorIndex < 0)
        {
            symmetricIndices[i] = (int)i;
            // LOG_VERBOSE("\"{}\" is self-symmetric", name);
        }
    }
    return symmetricIndices;
}

template <class T>
std::vector<std::vector<int>> RigLogic<T>::GetUsedRawControls() const
{
    std::vector<std::set<int>> usedRawControls(NumGUIControls());
    for (const auto& info : m->guiToRawMapping)
    {
        usedRawControls[info.inputIndex].insert(info.outputIndex);
    }
    std::vector<std::vector<int>> usedRawControlsVec(NumGUIControls());
    for (int i = 0; i < NumGUIControls(); ++i)
    {
        usedRawControlsVec[i] = std::vector<int>(usedRawControls[i].begin(), usedRawControls[i].end());
    }
    return usedRawControlsVec;
}

template <class T>
std::vector<int> RigLogic<T>::GetSymmetricPsdIndices() const
{
    if (m->psdDependsOnMLorRBF)
    {
        CARBON_CRITICAL("symmetric psd indices for psds that depend on ML or RBF controls has not been implemented yet");
    }

    const std::vector<int> symmetricRawControls = GetSymmetricRawControlIndices();
    if ((int)symmetricRawControls.size() != NumRawControls())
    {
        CARBON_CRITICAL("invalid size of vector");
    }
    Eigen::Matrix<bool, -1, -1> psdToRawOccupancy = Eigen::Matrix<bool, -1, -1>::Constant(m->psdToRawMap.rows(), m->psdToRawMap.cols(), false);
    std::vector<int> symmetricPsdIndices(m->psdToRawMap.rows(), -1);
    for (int r = 0; r < m->psdToRawMap.rows(); ++r)
    {
        for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, r); it; ++it)
        {
            psdToRawOccupancy(r, it.col()) = true;
        }
    }

    for (int r = 0; r < psdToRawOccupancy.rows(); ++r)
    {
        Eigen::RowVector<bool, -1> symmetricOccupancy = Eigen::RowVector<bool, -1>::Constant(psdToRawOccupancy.cols(), false);
        for (typename SparseMatrix<T>::InnerIterator it(m->psdToRawMap, r); it; ++it)
        {
            symmetricOccupancy[symmetricRawControls[it.col()]] = true;
        }
        for (int k = 0; k < psdToRawOccupancy.rows(); ++k)
        {
            if (symmetricOccupancy == psdToRawOccupancy.row(k))
            {
                symmetricPsdIndices[r] = k;
            }
        }
    }

    for (int r = 0; r < psdToRawOccupancy.rows(); ++r)
    {
        if (symmetricPsdIndices[r] == r)
        {
            // LOG_VERBOSE("psd control {} is self-symmetric", r);
        }
        else if (symmetricPsdIndices[r] < 0)
        {
            CARBON_CRITICAL("psd control {} is neither self-symmetric and does have a symmetric match", r);
        }
        else
        {
            if (symmetricPsdIndices[symmetricPsdIndices[r]] != r)
            {
                CARBON_CRITICAL("inconsistent symmetry for psd control {}", r);
            }
            // LOG_VERBOSE("psd control {} is symmetric", r);
        }
    }

    return symmetricPsdIndices;
}

template <typename T>
const std::vector<int>& RigLogic<T>::GetJointGroupIndices() const
{
    return m->jointGroupIndexPerJoint;
}

template <typename T>
const std::vector<Eigen::VectorX<std::uint16_t>>& RigLogic<T>::GetJointGroupJointIndices() const
{
    return m->jointGroupJointIndices;
}

template <typename T>
const std::vector<Eigen::VectorX<std::uint16_t>>& RigLogic<T>::GetJointGroupInputIndices() const
{
    return m->jointGroupInputIndices;

}

template <typename T>
const std::vector<Eigen::VectorX<std::uint16_t>>& RigLogic<T>::GetJointGroupOutputIndices() const
{
    return m->jointGroupOutputIndices;
}


template <typename T>
void RigLogic<T>::SaveJointDeltas(dna::Writer* writer) const
{
    if (!writer) return;
    if (!m->withJointScaling)
    {
        CARBON_CRITICAL("only rigs with joint scaling can be saved");
    }

    const int entriesPerJoint = m->withJointScaling ? 9 : 6;
    auto GetItemIndex = [](const Eigen::VectorX<std::uint16_t>& vec, int item) {
        for (int i = 0; i < (int)vec.size(); ++i)
        {
            if ((int)vec[i] == item) return i;
        }
        return -1;
    };

    // collect output indices for each joint group and each lod
    // compatible dnas should have same joints in the same joint groups
    // joint input indices should also match, however joint output indices can be different
    std::vector<std::vector<std::uint16_t>> jointGroupOutputIndices(m->jointGroupInputIndices.size());
    std::vector<std::set<std::uint16_t>> jointGroupOutputIndicesSets(m->jointGroupInputIndices.size());
    std::vector<std::vector<std::uint16_t>> jointGroupOutputRowsPerLOD(m->jointGroupInputIndices.size());
    for (int lod = m->numLODs - 1; lod >= 0; --lod)
    {
        for (int jointIndexAndDof = 0; jointIndexAndDof < (int)m->jointMatrixPerLOD[lod].rows(); ++jointIndexAndDof)
        {
            if (m->jointMatrixPerLOD[lod].row(jointIndexAndDof).nonZeros() == 0) continue;

            const int jointIndex = jointIndexAndDof / entriesPerJoint;
            const int jointGroupIndex = m->jointGroupIndexPerJoint[jointIndex];
            if (jointGroupIndex < 0)
            {
                CARBON_CRITICAL("joint {} is not part of any joint group", jointIndex);
            }
            const std::uint16_t outputIndex = (std::uint16_t)(jointIndexAndDof * 9 / entriesPerJoint);
            if (jointGroupOutputIndicesSets[jointGroupIndex].find(outputIndex) == jointGroupOutputIndicesSets[jointGroupIndex].end())
            {
                jointGroupOutputIndicesSets[jointGroupIndex].insert(outputIndex);
                jointGroupOutputIndices[jointGroupIndex].push_back(outputIndex);
            }
        }
        for (int jointGroupIndex = 0; jointGroupIndex < (int)m->jointGroupInputIndices.size(); ++jointGroupIndex)
        {
            jointGroupOutputRowsPerLOD[jointGroupIndex].push_back((std::uint16_t)jointGroupOutputIndices[jointGroupIndex].size());
        }
    }
    for (int jointGroupIndex = 0; jointGroupIndex < (int)jointGroupOutputRowsPerLOD.size(); ++jointGroupIndex)
    {
        std::reverse(jointGroupOutputRowsPerLOD[jointGroupIndex].begin(), jointGroupOutputRowsPerLOD[jointGroupIndex].end());
    }

    // copy the sparse joint matrix to block-wise joint matrices as saved in the dna file
    std::vector<Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> jointGroupValueBlocks;
    for (size_t i = 0; i < m->jointGroupInputIndices.size(); ++i)
    {
        const int inputSize = (int)m->jointGroupInputIndices[i].size();
        const int outputSize = (int)jointGroupOutputIndices[i].size();
        jointGroupValueBlocks.push_back(Eigen::Matrix<float, -1, -1, Eigen::RowMajor>::Zero(outputSize, inputSize));
    }
    for (int jointIndexAndDof = 0; jointIndexAndDof < (int)m->jointMatrixPerLOD[0].rows(); ++jointIndexAndDof)
    {
        if (m->jointMatrixPerLOD[0].row(jointIndexAndDof).nonZeros() == 0) continue;

        const int jointIndex = jointIndexAndDof / entriesPerJoint;
        const int dof = jointIndexAndDof % entriesPerJoint;
        const int jointGroupIndex = m->jointGroupIndexPerJoint[jointIndex];
        if (jointGroupIndex < 0)
        {
            CARBON_CRITICAL("joint {} is not part of any joint group", jointIndex);
        }
        const int tmpIdx = GetItemIndex(m->jointGroupJointIndices[jointGroupIndex], jointIndex);
        if (tmpIdx < 0)
        {
            CARBON_CRITICAL("joint group {} does not contain joint {}", jointGroupIndex, jointIndex);
        }
        const int blockOutputIndex = TITAN_NAMESPACE::GetItemIndex(jointGroupOutputIndices[jointGroupIndex], (std::uint16_t)jointIndexAndDof);
        if (blockOutputIndex < 0)
        {
            CARBON_CRITICAL("joint group {} does not contain joint/dof {}/{}", jointGroupIndex, jointIndex, dof);
        }
        for (typename SparseMatrix<T>::InnerIterator it(m->jointMatrixPerLOD[0], jointIndexAndDof); it; ++it)
        {
            const int blockInputIndex = GetItemIndex(m->jointGroupInputIndices[jointGroupIndex], (int)it.col());
            if (blockInputIndex < 0)
            {
                CARBON_CRITICAL("joint group {} does not contain input index {}",  jointGroupIndex, it.col());
            }
            const T scaling = (dof >= 3 && dof < 6) ? rad2degreeScale<T>() : T(1);
            const float output = (float)(scaling * it.value());
            jointGroupValueBlocks[jointGroupIndex](blockOutputIndex, blockInputIndex) = output;
        }
    }

    writer->setJointRowCount((std::uint16_t)m->numJoints * 9);
    writer->setJointColumnCount((std::uint16_t)m->jointMatrixPerLOD[0].cols());
    writer->clearJointGroups();
    for (std::uint16_t jointGroupIndex = 0; jointGroupIndex < (std::uint16_t)jointGroupValueBlocks.size(); ++jointGroupIndex)
    {
        writer->setJointGroupJointIndices(jointGroupIndex, m->jointGroupJointIndices[jointGroupIndex].data(), (std::uint16_t)m->jointGroupJointIndices[jointGroupIndex].size());
        writer->setJointGroupLODs(jointGroupIndex, jointGroupOutputRowsPerLOD[jointGroupIndex].data(), (std::uint16_t)jointGroupOutputRowsPerLOD[jointGroupIndex].size());
        writer->setJointGroupInputIndices(jointGroupIndex, m->jointGroupInputIndices[jointGroupIndex].data(), (std::uint16_t)m->jointGroupInputIndices[jointGroupIndex].size());
        writer->setJointGroupOutputIndices(jointGroupIndex, jointGroupOutputIndices[jointGroupIndex].data(), (std::uint16_t)jointGroupOutputIndices[jointGroupIndex].size());
        writer->setJointGroupValues(jointGroupIndex, jointGroupValueBlocks[jointGroupIndex].data(), (std::uint16_t)jointGroupValueBlocks[jointGroupIndex].size());
    }
}

// explicitly instantiate the rig logic classes
template class RigLogic<float>;
template class RigLogic<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
