// Copyright Epic Games, Inc. All Rights Reserved.

#include "rig/BodyLogic.h"

#include <riglogic/RigLogic.h>

#include <algorithm>
#include <numeric>
#include <vector>
#include <cstring>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
BodyLogic<T>::BodyLogic()
{
}

template <class T>
BodyLogic<T>::BodyLogic(int _numLods)
{
    numLODs = _numLods;
    jointMatrix.resize(numLODs);
    rbfJointMatrix.resize(numLODs);
}

template <class T>
std::shared_ptr<BodyLogic<T>> BodyLogic<T>::Clone() const
{
    std::shared_ptr<BodyLogic<T>> clone = std::make_shared<BodyLogic>();
    *clone = *this;
    return clone;
}

template <class T>
bool BodyLogic<T>::Init(const dna::Reader* reader)
{
    numLODs = reader->getLODCount();

    const int guiControlCount = reader->getGUIControlCount();

         // this only works as our quaternion raw controls for rbf are at the end
    std::uint16_t indexOfFirstQuaternionRawControl = reader->getRawControlCount();
    for(std::uint16_t si = 0u; si < reader->getRBFSolverCount(); ++si)
    {
        for (const auto ri : reader->getRBFSolverRawControlIndices(si)) {
            indexOfFirstQuaternionRawControl = std::min(indexOfFirstQuaternionRawControl, ri);
        }
    }

    const int rawControlCount = indexOfFirstQuaternionRawControl;
    const int psdControlCount = reader->getPSDCount();
    const std::uint16_t rbfPoseControlsOffset = static_cast<std::uint16_t>(
        reader->getRawControlCount() +
        reader->getPSDCount() +
        reader->getMLControlCount());

    // bodies should not have psd controls
    if (psdControlCount > 0) {
        CARBON_CRITICAL("body models should not have psd controls");
    }

    guiControlNames.clear();
    for (int i = 0; i < guiControlCount; ++i) {
        guiControlNames.push_back(reader->getGUIControlName(std::uint16_t(i)).c_str());
    }

    rawControlNames.clear();
    for (int i = 0; i < rawControlCount; ++i) {
        rawControlNames.push_back(reader->getRawControlName(std::uint16_t(i)).c_str());
    }

    // setup gui to raw calculation
    const int numGuiToRawAssignments = int(reader->getGUIToRawInputIndices().size());
    guiToRawMapping.resize(numGuiToRawAssignments);
    guiControlRanges = Eigen::Matrix<T, 2, -1>(2, guiControlCount);
    guiControlRanges.row(0).setConstant(1e6f);
    guiControlRanges.row(1).setConstant(-1e6f);

    for (int i = 0; i < numGuiToRawAssignments; i++) {
        const int inputIndex = reader->getGUIToRawInputIndices()[i];
        const int outputIndex = reader->getGUIToRawOutputIndices()[i];
        if (inputIndex < 0 || inputIndex >= guiControlCount) {
            CARBON_CRITICAL("gui control input index is invalid");
        }
        if (outputIndex < 0 || outputIndex >= rawControlCount) {
            CARBON_CRITICAL("gui control output index is invalid");
        }
        T from = reader->getGUIToRawFromValues()[i];
        T to = reader->getGUIToRawToValues()[i];
        if (from > to) std::swap(from, to);
        guiToRawMapping[i].inputIndex = inputIndex;
        guiToRawMapping[i].outputIndex = outputIndex;
        guiToRawMapping[i].from = from;
        guiToRawMapping[i].to = to;
        guiToRawMapping[i].slope = reader->getGUIToRawSlopeValues()[i];
        guiToRawMapping[i].cut = reader->getGUIToRawCutValues()[i];
        guiControlRanges(0, inputIndex) = std::min<T>(guiControlRanges(0, inputIndex), from);
        guiControlRanges(1, inputIndex) = std::max<T>(guiControlRanges(1, inputIndex), to);
    }

    // sort gui to raw control mapping
    SortGuiControlMapping();

    // setup joints
    const int numJoints = reader->getJointCount();
    if (reader->getJointCount() * 9 != reader->getJointRowCount()) {
        CARBON_CRITICAL("number of joints and joint rows not matching");
    }

    //if (numJoints > 0 && rawControlCount != reader->getJointColumnCount()) {
    //    CARBON_CRITICAL("number of total controls and joint columns not matching");
    //}

    jointMatrix.resize(numLODs);
    rbfJointMatrix.resize(numLODs);
    for (int lod = 0; lod < numLODs; lod++) {
        std::vector<Eigen::Triplet<T>> jointMatrixTriplets;
        std::vector<Eigen::Triplet<T>> rbfJointMatrixTriplets;
        for (std::uint16_t i = 0; i < reader->getJointGroupCount(); i++) {
            rl4::ConstArrayView<std::uint16_t> rowsPerLOD = reader->getJointGroupLODs(i);
            rl4::ConstArrayView<std::uint16_t> jointGroupInputIndices = reader->getJointGroupInputIndices(i);
            rl4::ConstArrayView<std::uint16_t> jointGroupOutputIndices = reader->getJointGroupOutputIndices(i);
            rl4::ConstArrayView<float> jointGroupValues = reader->getJointGroupValues(i);
            for (int j = 0; j < rowsPerLOD[lod]; j++) {
                std::uint16_t jointIndexAndDof = jointGroupOutputIndices[j];
                // const std::uint16_t jointIndex = jointIndexAndDof / 9;
                const std::uint16_t dof = jointIndexAndDof % 9;
                constexpr T deg2rad = T(CARBON_PI / 180.0);
                const T scaling = (dof >= 3 && dof < 6) ? deg2rad : T(1);
                for (int k = 0; k < int(jointGroupInputIndices.size()); k++) {
                    const int& parameterIndex = jointGroupInputIndices[k];
                    const int valueIndex = j * int(jointGroupInputIndices.size()) + k;
                    const T value = scaling * T(jointGroupValues[valueIndex]);
                    if (fabs(value) > 1e-20) {
                        if (parameterIndex < rbfPoseControlsOffset) {
                            jointMatrixTriplets.push_back(Eigen::Triplet<T>(jointIndexAndDof, parameterIndex, value));
                        }
                        else {
                            rbfJointMatrixTriplets.push_back(Eigen::Triplet<T>(jointIndexAndDof, parameterIndex - rbfPoseControlsOffset, value));
                        }
                    }
                }
            }
        }

        jointMatrix[lod].resize(numJoints * 9, rawControlCount);
        jointMatrix[lod].setFromTriplets(jointMatrixTriplets.begin(), jointMatrixTriplets.end());
        rbfJointMatrix[lod].resize(numJoints * 9, reader->getRBFPoseControlCount());
        rbfJointMatrix[lod].setFromTriplets(rbfJointMatrixTriplets.begin(), rbfJointMatrixTriplets.end());
    }

    return true;
}

template <class T>
void BodyLogic<T>::SetNumLODs(const int l) {
    numLODs = l;
    jointMatrix.resize(l);
    rbfJointMatrix.resize(l);
}


template <class T>
void BodyLogic<T>::SortGuiControlMapping() {
    std::vector<int> order(guiToRawMapping.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (guiToRawMapping[a].outputIndex < guiToRawMapping[b].outputIndex) return true;
        if (guiToRawMapping[a].outputIndex > guiToRawMapping[b].outputIndex) return false;
        return (guiToRawMapping[a].inputIndex < guiToRawMapping[b].inputIndex);
        });
    std::vector<GuiToRawInfo> newGuiToRawMapping = guiToRawMapping;
    for (size_t i = 0; i < order.size(); ++i) {
        newGuiToRawMapping[i] = guiToRawMapping[order[i]];
    }
    guiToRawMapping = newGuiToRawMapping;
}

template <class T>
DiffData<T> BodyLogic<T>::EvaluateRawControls(const DiffData<T>& guiControls) const
{
    if (guiControls.Size() != NumGUIControls()) {
        CARBON_CRITICAL("BodyLogic::EvaluateRawControls(): guiControls control count incorrect: {} instead of {}", guiControls.Size(), NumGUIControls());
    }

    Vector<T> output = Vector<T>::Zero(NumRawControls());

    // evaluate GUI controls
    const int numGuiToRawMappings = int(guiToRawMapping.size());
    for (int i = 0; i < numGuiToRawMappings; i++) {
        const int inputIndex = guiToRawMapping[i].inputIndex;
        const int outputIndex = guiToRawMapping[i].outputIndex;
        // const T from = guiToRawMapping[i].from;
        // const T to = guiToRawMapping[i].to;
        const T slope = guiToRawMapping[i].slope;
        const T cut = guiToRawMapping[i].cut;
        const T value = guiControls.Value()[inputIndex];
        // bodies don't really do explicit clamping, we do this in the UI or with soft loss functions
        output[outputIndex] += slope * value + cut;
    }

    JacobianConstPtr<T> Jacobian;

    if (guiControls.HasJacobian()) {
        // fill jacobian matrix directly as gui to raw mappings are ordered
        SparseMatrix<T> localJacobian(NumRawControls(), guiControls.Size());
        localJacobian.reserve(numGuiToRawMappings);
        int prevRowIndex = -1;
        for (int i = 0; i < numGuiToRawMappings; i++) {
            const int inputIndex = guiToRawMapping[i].inputIndex;
            const int outputIndex = guiToRawMapping[i].outputIndex;
            // const T from = guiToRawMapping[i].from;
            // const T to = guiToRawMapping[i].to;
            const T slope = guiToRawMapping[i].slope;
            // const T value = guiControls.Value()[inputIndex];
            // const T rangeStart = guiControlRanges(0, inputIndex);
            // const T rangeEnd = guiControlRanges(1, inputIndex);
            // const bool belowRange = (from == rangeStart && value < from);
            // const bool aboveRange = (to == rangeEnd && value >= to);
            while (prevRowIndex < outputIndex) {
                localJacobian.startVec(++prevRowIndex);
            }
            localJacobian.insertBackByOuterInner(outputIndex, inputIndex) = slope;
        }
        localJacobian.finalize();
        Jacobian = guiControls.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(output), std::move(Jacobian));
}

template <class T>
DiffData<T> BodyLogic<T>::EvaluateJoints(const int lod, const DiffData<T>& rawControls) const
{
    if (rawControls.Size() != NumRawControls()) {
        CARBON_CRITICAL("BodyLogic::EvaluateJoints(): psd control count incorrect");
    }
    Vector<T> output = jointMatrix[lod] * rawControls.Value();

    JacobianConstPtr<T> jacobian;
    if (rawControls.HasJacobian()) {
        jacobian = rawControls.Jacobian().Premultiply(jointMatrix[lod]);
    }

    return DiffData<T>(std::move(output), jacobian);
}

template <class T>
DiffData<T> BodyLogic<T>::EvaluateRbfJoints(const int lod, const DiffData<T>& rbfControls) const {
    if (rbfControls.Size() != rbfJointMatrix[lod].cols()) {
        CARBON_CRITICAL("BodyLogic::EvaluateJoints(): rbf control count incorrect");
    }
    Vector<T> output = rbfJointMatrix[lod] * rbfControls.Value();

    JacobianConstPtr<T> jacobian;
    if (rbfControls.HasJacobian()) {
        jacobian = rbfControls.Jacobian().Premultiply(rbfJointMatrix[lod]);
    }

    return DiffData<T>(std::move(output), jacobian);
}

// explicitly instantiate the rig logic classes
template class BodyLogic<float>;
template class BodyLogic<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)