// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RBFLogic.h>
#include <nls/math/Math.h>
#include <nls/DiffData.h>
#include <nls/geometry/EulerAngles.h>
#include <carbon/common/Defs.h>
#include <rig/rbfs/RBFSolver.h>

#include <dna/layers/JointBehaviorMetadata.h>
#include <dna/layers/RBFBehavior.h>
#include <dna/Reader.h>
#include <dna/Writer.h>
#include <pma/resources/DefaultMemoryResource.h>
#include <tdm/TDM.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <set>
#include <vector>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace {
    using SolverPtr = pma::ScopedPtr<rbfs::RBFSolver>;
    using dna::TranslationRepresentation;
    using dna::RotationRepresentation;
    using dna::ScaleRepresentation;
}

struct JointRepresentation
{
    TranslationRepresentation translation;
    RotationRepresentation rotation;
    ScaleRepresentation scale;
};

template<typename T>
struct RBFLogic<T>::Private
{
    Private() = default;
    Private(Private&&) = default;
    Private& operator=(Private&&) = default;

    Private(const Private& other) :
        eulerControlNames{ other.eulerControlNames },
        controlMappings{ other.controlMappings },
        solvers{},
        solverToControlMapping{ other.solverToControlMapping },
        poseInputControlIndices{ other.poseInputControlIndices },
        poseOutputControlIndices{ other.poseOutputControlIndices },
        poseOutputControlWeights{ other.poseOutputControlWeights },
        solverPoseIndices{ other.solverPoseIndices },
        solverNames{ other.solverNames },
        poseNames{ other.poseNames },
        poseControlNames{ other.poseControlNames },
        poseScales{ other.poseScales },
        isAutomaticRadius{ other.isAutomaticRadius },
        quaternionRawControlIndices{ other.quaternionRawControlIndices },
        rawTargetValues{ other.rawTargetValues },
        jointRepresentations{ other.jointRepresentations },
        rawControlCount{ other.rawControlCount },
        poseControlOffset{ other.poseControlOffset },
        poseControlCount{ other.poseControlCount },
        maxDrivingJointsCount{ other.maxDrivingJointsCount }
    {
        for (const auto& solver : other.solvers) {
            solvers.push_back(pma::makeScoped<rbfs::RBFSolver>(solver.get()));
        }
    }

    Private& operator=(const Private& rhs) {
        Private tmp{ rhs };
        *this = std::move(tmp);
        return *this;
    }
    pma::DefaultMemoryResource dmr{};
    std::vector<std::string> eulerControlNames;
    std::vector<EulerToRawMapping> controlMappings;
    std::vector<SolverPtr> solvers;
    std::vector<std::vector<std::uint16_t>> solverToControlMapping;
    std::vector<std::vector<std::uint16_t>> poseInputControlIndices;
    std::vector<std::vector<std::uint16_t>> poseOutputControlIndices;
    std::vector<std::vector<float>> poseOutputControlWeights;
    std::vector<std::vector<std::uint16_t>> solverPoseIndices;
    std::vector<std::string> solverNames;
    std::vector<std::string> poseNames;
    std::vector<std::string> poseControlNames;
    std::vector<float> poseScales;
    std::vector<bool> isAutomaticRadius;
    std::set<std::uint16_t> quaternionRawControlIndices;
    std::vector<std::vector<float>> rawTargetValues;
    std::vector<JointRepresentation> jointRepresentations;
    std::uint16_t rawControlCount;
    std::uint16_t poseControlOffset;
    std::uint16_t poseControlCount;
    std::uint16_t maxDrivingJointsCount;
};

template<typename T>
RBFLogic<T>::RBFLogic() : m{ new Private() } {
}
template<typename T> RBFLogic<T>::~RBFLogic() = default;
template<typename T> RBFLogic<T>::RBFLogic(const RBFLogic<T>& other) : m{ new Private(*other.m) } {
}
template<typename T> RBFLogic<T>& RBFLogic<T>::operator=(const RBFLogic<T>& other) {
    RBFLogic<T> temp(other);
    std::swap(*this, temp);
    return *this;
}
template<typename T> RBFLogic<T>::RBFLogic(RBFLogic&&) = default;
template<typename T> RBFLogic<T>& RBFLogic<T>::operator=(RBFLogic&&) = default;

template<typename T>
bool RBFLogic<T>::Init(const dna::Reader* reader) {
    const auto jointCount = reader->getJointCount();
    std::map<std::string, std::uint16_t> jointNameToIdx{};
    m->jointRepresentations.clear();
    m->jointRepresentations.reserve(jointCount);
    m->poseControlOffset = reader->getRawControlCount() + reader->getPSDCount() + reader->getMLControlCount();

    for (std::uint16_t i = 0; i < jointCount; ++i) {
        std::string jointName = reader->getJointName(i).c_str();
        jointNameToIdx.insert({ jointName, i });
        m->jointRepresentations.push_back(JointRepresentation{ reader->getJointTranslationRepresentation(i),
                                                                reader->getJointRotationRepresentation(i),
                                                                reader->getJointScaleRepresentation(i) });
    }

    for (int i = 0; i < reader->getRBFPoseCount(); ++i) {
        m->poseNames.push_back(reader->getRBFPoseName(std::uint16_t(i)).c_str());
    }

    //this is to silence unreachable code warning treated as error
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702)
#endif
    const auto getJointIndex = [jointNameToIdx](const std::string& jointName) {
        auto it = jointNameToIdx.find(jointName);
        if (it != jointNameToIdx.end()) {
            return it->second;
        }

        CARBON_CRITICAL("Joint with name {} doesn't exist in dna file.", jointName);
        return std::numeric_limits<std::uint16_t>::max();
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    m->eulerControlNames.clear();
    m->controlMappings.clear();
    m->solvers.clear();
    m->solverToControlMapping.clear();
    m->poseInputControlIndices.clear();
    m->poseOutputControlIndices.clear();
    m->poseOutputControlWeights.clear();
    m->solverPoseIndices.clear();
    m->solverNames.clear();
    m->poseControlNames.clear();
    m->poseNames.clear();
    m->poseScales.clear();
    m->isAutomaticRadius.clear();
    m->quaternionRawControlIndices.clear();
    m->rawTargetValues.clear();
    m->jointRepresentations.clear();
    const auto solverCount = reader->getRBFSolverCount();
    const auto poseCount = reader->getRBFPoseCount();

    m->solverToControlMapping.resize(solverCount);
    m->solverPoseIndices.resize(solverCount);
    m->poseInputControlIndices.resize(poseCount);
    m->poseOutputControlIndices.resize(poseCount);
    m->poseOutputControlWeights.resize(poseCount);
    m->solverNames.reserve(solverCount);
    m->poseNames.reserve(poseCount);
    m->poseScales.reserve(poseCount);

    m->poseControlCount = reader->getRBFPoseControlCount();
    m->poseControlNames.reserve(m->poseControlNames.size());
    m->rawControlCount = reader->getRawControlCount();

    for (std::uint16_t pi = 0u; pi < poseCount; ++pi) {
        m->poseNames.push_back(reader->getRBFPoseName(pi).c_str());
        m->poseScales.push_back(reader->getRBFPoseScale(pi));
        const auto inputIndices = reader->getRBFPoseInputControlIndices(pi);
        const auto poseWeights = reader->getRBFPoseOutputControlWeights(pi);
        const auto outputIndices = reader->getRBFPoseOutputControlIndices(pi);
        m->poseInputControlIndices[pi].assign(inputIndices.begin(), inputIndices.end());
        m->poseOutputControlWeights[pi].assign(poseWeights.begin(), poseWeights.end());
        m->poseOutputControlIndices[pi].assign(outputIndices.begin(), outputIndices.end());
    }

    for (std::uint16_t pci = 0u; pci < m->poseControlCount; pci++) {
        m->poseControlNames.push_back(reader->getRBFPoseControlName(pci).c_str());
    }

    for (std::uint16_t si = 0u; si < solverCount; ++si) {
        m->solverNames.push_back(reader->getRBFSolverName(si).c_str());
        if (reader->getRBFSolverDistanceMethod(si) == static_cast<dna::RBFDistanceMethod>(rbfs::RBFDistanceMethod::Euclidean)) {
            CARBON_CRITICAL("RBFLogic.cpp supports only quaternion based rbf solvers.");
        }

        auto solverRawControlIndices = reader->getRBFSolverRawControlIndices(si);
        for (std::uint16_t ci = 0u; ci < solverRawControlIndices.size(); ci += 4u) {
            // Control names are in form   {jointName}.{quaternionAttribute}
            // e.g. "calf_l.x", "calf_l.y",  "calf_l.z", " calf_l.w"
            const std::string fullControlName = reader->getRawControlName(solverRawControlIndices[ci]).c_str();
            const size_t dotPos = fullControlName.find('.');
            const std::string jointName = fullControlName.substr(0, dotPos);
            const auto drivingJointIndex = getJointIndex(jointName);

            auto mappingIt = std::find_if(m->controlMappings.begin(), m->controlMappings.end(),
                [drivingJointIndex](const EulerToRawMapping& mapping) {
                    return mapping.jointIndex == drivingJointIndex;
                });

            if (mappingIt == m->controlMappings.end()) {
                // we need to add mapping for this joint
                EulerToRawMapping mapping{};
                mapping.jointIndex = drivingJointIndex;

                mapping.rawX = solverRawControlIndices[ci + 0];
                mapping.rawY = solverRawControlIndices[ci + 1];
                mapping.rawZ = solverRawControlIndices[ci + 2];
                mapping.rawW = solverRawControlIndices[ci + 3];

                mapping.eulerX = static_cast<std::uint16_t>(m->eulerControlNames.size());
                mapping.eulerY = static_cast<std::uint16_t>(m->eulerControlNames.size()) + 1u;
                mapping.eulerZ = static_cast<std::uint16_t>(m->eulerControlNames.size()) + 2u;
                m->eulerControlNames.push_back(reader->getRawControlName(solverRawControlIndices[ci + 0]).c_str());
                m->eulerControlNames.push_back(reader->getRawControlName(solverRawControlIndices[ci + 1]).c_str());
                m->eulerControlNames.push_back(reader->getRawControlName(solverRawControlIndices[ci + 2]).c_str());
                mappingIt = m->controlMappings.insert(mappingIt, mapping);
            }
            m->solverToControlMapping[si].push_back(static_cast<std::uint16_t>(std::distance(m->controlMappings.begin(), mappingIt)));
            m->quaternionRawControlIndices.insert(solverRawControlIndices.begin(), solverRawControlIndices.end());
        }
        const std::uint16_t drivingJointCount = static_cast<std::uint16_t>(solverRawControlIndices.size() / 4u);

        if (drivingJointCount > m->maxDrivingJointsCount) {
            m->maxDrivingJointsCount = drivingJointCount;
        }

        std::vector<float> targetScales;
        auto poseIndices = reader->getRBFSolverPoseIndices(si);
        m->solverPoseIndices[si].assign(poseIndices.data(), poseIndices.data() + poseIndices.size());
        for (std::uint16_t pi : m->solverPoseIndices[si]) {
            targetScales.push_back(m->poseScales[pi]);
        }

        rbfs::RBFSolverRecipe recipe;
        recipe.solverType = static_cast<rbfs::RBFSolverType>(reader->getRBFSolverType(si));
        recipe.distanceMethod = static_cast<rbfs::RBFDistanceMethod>(reader->getRBFSolverDistanceMethod(si));
        recipe.normalizeMethod = static_cast<rbfs::RBFNormalizeMethod>(reader->getRBFSolverNormalizeMethod(si));
        recipe.isAutomaticRadius = static_cast<rbfs::AutomaticRadius>(reader->getRBFSolverAutomaticRadius(si)) == rbfs::AutomaticRadius::On;
        m->isAutomaticRadius.push_back(recipe.isAutomaticRadius);
        recipe.twistAxis = static_cast<rbfs::TwistAxis>(reader->getRBFSolverTwistAxis(si));
        recipe.weightFunction = static_cast<rbfs::RBFFunctionType>(reader->getRBFSolverFunctionType(si));
        const auto radius = reader->getRBFSolverRadius(si);
        recipe.radius = reader->getRotationUnit() == dna::RotationUnit::degrees ? tdm::radians(radius) : radius;
        recipe.rawControlCount = static_cast<std::uint16_t>(solverRawControlIndices.size());
        recipe.weightThreshold = reader->getRBFSolverWeightThreshold(si);
        recipe.targetValues = reader->getRBFSolverRawControlValues(si);
        m->rawTargetValues.push_back(std::vector<float>{recipe.targetValues.begin(), recipe.targetValues.end()});
        recipe.targetScales = targetScales;
        m->solvers.push_back(pma::makeScoped<rbfs::RBFSolver>(recipe, &m->dmr));
    }

    return true;
}

template<typename T>
void RBFLogic<T>::Write(dna::Writer* writer) {
    for (std::uint16_t pci = 0u; pci < m->poseControlNames.size(); ++pci) {
        writer->setRBFPoseControlName(pci, m->poseControlNames[pci].c_str());
    }
    for (std::uint16_t pi = 0u; pi < m->poseNames.size(); ++pi) {
        writer->setRBFPoseName(pi, m->poseNames[pi].c_str());
        writer->setRBFPoseScale(pi, m->poseScales[pi]);
        writer->setRBFPoseInputControlIndices(pi, m->poseInputControlIndices[pi].data(), static_cast<std::uint16_t>(m->poseInputControlIndices[pi].size()));
        writer->setRBFPoseOutputControlIndices(pi, m->poseOutputControlIndices[pi].data(), static_cast<std::uint16_t>(m->poseOutputControlIndices[pi].size()));
        writer->setRBFPoseOutputControlWeights(pi, m->poseOutputControlWeights[pi].data(), static_cast<std::uint16_t>(m->poseOutputControlWeights[pi].size()));
    }
    for (std::uint16_t si = 0u; si < m->solverNames.size(); ++si) {
        writer->setRBFSolverName(si, m->solverNames[si].c_str());
        std::vector<std::uint16_t> solverRawControlIndices{};
        for (const auto mappingIndex : m->solverToControlMapping[si]) {
            const EulerToRawMapping& mapping = m->controlMappings[mappingIndex];
            solverRawControlIndices.push_back(mapping.rawX);
            solverRawControlIndices.push_back(mapping.rawY);
            solverRawControlIndices.push_back(mapping.rawZ);
            solverRawControlIndices.push_back(mapping.rawW);
        }
        writer->setRBFSolverRawControlIndices(si, solverRawControlIndices.data(), static_cast<std::uint16_t>(solverRawControlIndices.size()));
        writer->setRBFSolverPoseIndices(si, m->solverPoseIndices[si].data(), static_cast<std::uint16_t>(m->solverPoseIndices[si].size()));
        writer->setRBFSolverRawControlValues(si, m->rawTargetValues[si].data(), static_cast<std::uint16_t>(m->rawTargetValues[si].size()));
        writer->setRBFSolverType(si, static_cast<dna::RBFSolverType>(m->solvers[si]->getSolverType()));
        writer->setRBFSolverRadius(si, m->solvers[si]->getRadius());
        dna::AutomaticRadius automaticRadius = m->isAutomaticRadius[si] ? dna::AutomaticRadius::On : dna::AutomaticRadius::Off;
        writer->setRBFSolverAutomaticRadius(si, automaticRadius);
        writer->setRBFSolverWeightThreshold(si, m->solvers[si]->getWeightThreshold());
        writer->setRBFSolverDistanceMethod(si, static_cast<dna::RBFDistanceMethod>(m->solvers[si]->getDistanceMethod()));
        writer->setRBFSolverNormalizeMethod(si, static_cast<dna::RBFNormalizeMethod>(m->solvers[si]->getNormalizeMethod()));
        writer->setRBFSolverFunctionType(si, static_cast<dna::RBFFunctionType>(m->solvers[si]->getWeightFunction()));
        writer->setRBFSolverTwistAxis(si, static_cast<dna::TwistAxis>(m->solvers[si]->getTwistAxis()));
    }
}

template <class T>
DiffData<T> RBFLogic<T>::EvaluateRawControlsFromEuler(const DiffData<T>& eulerControls) const {
    if (eulerControls.Size() != static_cast<int>(m->eulerControlNames.size())) {
        CARBON_CRITICAL("RbfLogic::EvaluateRawControlsFromEuler(): eulerControls count incorrect: {} instead of {}", eulerControls.Size(), m->eulerControlNames.size());
    }

    Vector<T> rawControls = Vector<T>::Zero(m->rawControlCount);
    for (const auto& mapping : m->controlMappings) {
        using tdm::frad;

        const auto euler = tdm::frad3{
            frad{static_cast<float>(eulerControls.Value()[mapping.eulerX])},
            frad{static_cast<float>(eulerControls.Value()[mapping.eulerY])},
            frad{static_cast<float>(eulerControls.Value()[mapping.eulerZ])} };

        auto quaternion = tdm::quat<float>::fromEuler<tdm::rot_seq::xyz>(euler);

        rawControls[mapping.rawX] = quaternion.x;
        rawControls[mapping.rawY] = quaternion.y;
        rawControls[mapping.rawZ] = quaternion.z;
        rawControls[mapping.rawW] = quaternion.w;
    }

    return DiffData<T>{std::move(rawControls)};
}

template <class T>
DiffData<T> RBFLogic<T>::EvaluatePoseControlsFromRawControls(const DiffData<T>& rawControls) const {
    std::vector<float> rbfInput(m->maxDrivingJointsCount * 4u);
    const auto rbfSolverCount = m->poseNames.size();
    std::vector<float> solverIntermediateBuffer(rbfSolverCount);
    std::vector<float> solverOutputBuffer(rbfSolverCount);

    Vector<T> poseControls = Vector<T>::Zero(PoseControlCount());
    for (std::uint16_t si = 0u; si < m->solvers.size(); ++si) {
        const auto& solver = m->solvers[si];
        const auto solverPoseCount = solver->getTargetCount();
        std::uint16_t inputOffset = 0u;
        for (std::uint16_t mi : m->solverToControlMapping[si]) {
            const auto& mapping = m->controlMappings[mi];
            rbfInput[inputOffset + 0] = static_cast<float>(rawControls.Value()[mapping.rawX]);
            rbfInput[inputOffset + 1] = static_cast<float>(rawControls.Value()[mapping.rawY]);
            rbfInput[inputOffset + 2] = static_cast<float>(rawControls.Value()[mapping.rawZ]);
            rbfInput[inputOffset + 3] = static_cast<float>(rawControls.Value()[mapping.rawW]);
            inputOffset += 4u;
        }

        const std::size_t drivingJointCount = m->solverToControlMapping[si].size();
        solver->solve(av::ArrayView<float>{rbfInput.data(), drivingJointCount * 4u},
            av::ArrayView<float>{solverIntermediateBuffer.data(), solverPoseCount},
            av::ArrayView<float>{solverOutputBuffer.data(), solverPoseCount});

        for (std::size_t i = 0u; i < m->solverPoseIndices[si].size(); ++i) {
            const float poseWeight = solverOutputBuffer[i];
            const std::uint16_t pi = m->solverPoseIndices[si][i];
            const auto& controlOutputIndices = m->poseOutputControlIndices[pi];
            const auto& controlOutputWeights = m->poseOutputControlWeights[pi];
            const auto& controlInputIndices = m->poseInputControlIndices[pi];
            float inputWeight = 1.0f;
            for (std::uint16_t inputIndex : controlInputIndices) {
                inputWeight *= static_cast<float>(rawControls.Value()[inputIndex]);
            }
            for (std::size_t pci = 0u; pci < controlOutputWeights.size(); ++pci) {
                //we need to take into account offset as poseControlIndices are indices into total controls and we want only rbf controls to be returned
                const std::uint16_t controlIndex = controlOutputIndices[pci] - m->poseControlOffset;
                poseControls[controlIndex] += poseWeight * controlOutputWeights[pci] * inputWeight;
            }
        }
    }

    return DiffData<T>(std::move(poseControls));
}

template <class T>
DiffData<T> RBFLogic<T>::EvaluatePoseControlsFromJoints(const DiffData<T>& jointDiff) const {
    Vector<T> qControls = Vector<T>::Zero(m->rawControlCount);
    for (const auto& mapping : m->controlMappings) {
        using tdm::frad;
        const auto euler = tdm::frad3{
            frad{static_cast<float>(jointDiff.Value()[mapping.jointIndex * 9 + 3])},
            frad{static_cast<float>(jointDiff.Value()[mapping.jointIndex * 9 + 4])},
            frad{static_cast<float>(jointDiff.Value()[mapping.jointIndex * 9 + 5])} };

        auto quaternion = tdm::quat<float>::fromEuler<tdm::rot_seq::xyz>(euler);
        qControls[mapping.rawX] = quaternion.x;
        qControls[mapping.rawY] = quaternion.y;
        qControls[mapping.rawZ] = quaternion.z;
        qControls[mapping.rawW] = quaternion.w;
    }
    return EvaluatePoseControlsFromRawControls(std::move(qControls));
}

template <class T>
const SolverPtr& RBFLogic<T>::GetSolver(int solverIndex) const {
    return m->solvers[solverIndex];
}

template <typename T>
const std::vector<SolverPtr>& RBFLogic<T>::RBFSolvers() const {
    return m->solvers;
}

template <class T>
const std::vector<std::string>& RBFLogic<T>::SolverNames() const {
    return m->solverNames;
}

template <class T>
const std::vector<std::uint16_t>& RBFLogic<T>::SolverPoseIndices(int solverIndex) const {
    return m->solverPoseIndices[solverIndex];
}

template <class T>
std::uint16_t RBFLogic<T>::PoseControlOffset() const {
    return m->poseControlOffset;
}

template <class T>
const std::vector<std::uint16_t>& RBFLogic<T>::PoseOutputControlIndices(int poseIndex) const {
    return m->poseOutputControlIndices[poseIndex];
}

template <class T>
const std::string& RBFLogic<T>::PoseOutputControlName(int poseOutputControlIndex) const {
    int actualIndex = poseOutputControlIndex - m->poseControlOffset;
    if (actualIndex < 0) {
        CARBON_CRITICAL("RBFLogic poseOutputControl index out of bound of actual indices {}, index should be in range of {}-{}", poseOutputControlIndex, m->poseControlOffset, m->poseControlOffset + m->poseControlNames.size());
    }
    return m->poseControlNames[actualIndex];
}

template <class T>
const std::vector<std::string>& RBFLogic<T>::PoseNames() const {
    return m->poseNames;
}

template <class T>
const std::vector<std::string>& RBFLogic<T>::PoseControlNames() const
{
    return m->poseControlNames;
}

template <class T>
const std::vector<std::string>& RBFLogic<T>::EulerControlNames() const {
    return m->eulerControlNames;
}

template <class T>
const std::set<std::uint16_t>& RBFLogic<T>::RBFRawControls() const {
    return m->quaternionRawControlIndices;
}

template <class T>
const std::vector<typename RBFLogic<T>::EulerToRawMapping>& RBFLogic<T>::EulerToRaw() const {
    return m->controlMappings;
}

template <class T>
const std::vector<float>& RBFLogic<T>::SolverRawTargetValues(int solverIndex) const {
    return m->rawTargetValues[solverIndex];
}

template <class T>
bool RBFLogic<T>::SolverAutomaticRadius(int solverIndex) const {
    return m->isAutomaticRadius[solverIndex];
}

template <class T>
void RBFLogic<T>::SetSolver(int solverIndex, const std::string& solverName, const std::vector<std::uint16_t>& poseIndices, rbfs::RBFSolverRecipe recipe) {
    if (solverIndex > static_cast<int>(m->solvers.size())) {
        CARBON_CRITICAL("RBFLogic::SetSolver solverIndex={} out of bounds, solvers.size()={} ", solverIndex, m->solvers.size());
    }
    m->solverNames[solverIndex] = solverName;
    m->solverPoseIndices[solverIndex].assign(poseIndices.data(), poseIndices.data() + poseIndices.size());
    m->isAutomaticRadius[solverIndex] = recipe.isAutomaticRadius;
    m->rawTargetValues[solverIndex] = std::vector<float>{ recipe.targetValues.begin(), recipe.targetValues.end() };
    m->solvers[solverIndex] = pma::makeScoped<rbfs::RBFSolver>(recipe);
}

template <class T>
std::uint16_t RBFLogic<T>::PoseControlCount() const {
    return m->poseControlCount;
}

template <class T>
void RBFLogic<T>::RemoveJoints(const std::vector<int>& newToOldJointMapping)
{
    std::vector<EulerToRawMapping> controlMappings;
    std::vector<JointRepresentation> jointRepresentations;
    
    for (int newIdx = 0; newIdx < int(newToOldJointMapping.size()); ++newIdx)
    {
        const int oldIdx = newToOldJointMapping[newIdx];
        if (oldIdx < (int)m->jointRepresentations.size())
        {
            jointRepresentations.push_back(m->jointRepresentations[oldIdx]);
        }
    }

    for (int k = 0; k < (int)m->controlMappings.size(); ++k)
    {
        const int jointIndex = m->controlMappings[k].jointIndex;
        int newJointIndex = -1;
        for (int newIdx = 0; newIdx < int(newToOldJointMapping.size()); ++newIdx)
        {
            if (newToOldJointMapping[newIdx] == jointIndex)
            {
                newJointIndex = newIdx;
            }
        }
        if (newJointIndex >= 0)
        {
            controlMappings.push_back(m->controlMappings[k]);
            controlMappings.back().jointIndex = (std::uint16_t)newJointIndex;
        }
    }

    LOG_VERBOSE("remove {} out of {} mappings", m->controlMappings.size() - controlMappings.size(), m->controlMappings.size());
    m->jointRepresentations = jointRepresentations;
    m->controlMappings = controlMappings;
}

template class RBFLogic<float>;
template class RBFLogic<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
