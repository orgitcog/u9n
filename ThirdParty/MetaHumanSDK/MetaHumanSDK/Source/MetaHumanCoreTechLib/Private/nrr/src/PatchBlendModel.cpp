// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/PatchBlendModel.h>

#include <carbon/io/Utils.h>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/utils/Timer.h>
#include <nls/VectorVariable.h>
#include <nls/functions/ColwiseAddFunction.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionAverage.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/serialization/EigenSerialization.h>
#include <nrr/LinearVertexModel.h>
#include <nls/geometry/QRigidMotion.h>

#include <carbon/io/JsonIO.h>

#include <algorithm>
#include <vector>
#include <filesystem>


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
PatchBlendModel<T>::State::State(int numPatches)
{
    regionScales = std::vector<T>(numPatches, T(1));
    regionRotations = std::vector<Eigen::Quaternion<T>>(numPatches, Eigen::Quaternion<T>::Identity());
    regionPcaWeights.resize(numPatches);
    regionTranslations.resize(numPatches, Eigen::Vector3<T>::Zero());
    regionVertexDeltas.resize(numPatches);
}

template <class T>
void PatchBlendModel<T>::State::Reset(const PatchBlendModel<T>& patchBlendModel)
{
    regionScales = std::vector<T>(patchBlendModel.NumPatches(), T(1));
    regionRotations = std::vector<Eigen::Quaternion<T>>(patchBlendModel.NumPatches(), Eigen::Quaternion<T>::Identity());
    regionPcaWeights.resize(patchBlendModel.NumPatches());
    regionTranslations.resize(patchBlendModel.NumPatches());
    for (int id = 0; id < patchBlendModel.NumPatches(); ++id)
    {
        regionTranslations[id] = patchBlendModel.PatchCenterOfGravity(id);
        regionPcaWeights[id] = Eigen::VectorX<T>::Zero(patchBlendModel.NumPcaModesForPatch(id));
    }
    regionVertexDeltas.clear();
    regionVertexDeltas.resize(patchBlendModel.NumPatches());
}

template <class T>
void PatchBlendModel<T>::State::CopyTransforms(const State& other)
{
    if (other.NumPatches() != NumPatches())
    {
        CARBON_CRITICAL("number of patches do not match: {} vs {}", other.NumPatches(), NumPatches());
    }
    regionScales = other.regionScales;
    regionRotations = other.regionRotations;
    regionTranslations = other.regionTranslations;
}

template <class T>
void PatchBlendModel<T>::State::CopyPcaWeights(const State& other)
{
    if (other.NumPatches() != NumPatches())
    {
        CARBON_CRITICAL("number of patches do not match: {} vs {}", other.NumPatches(), NumPatches());
    }
    regionPcaWeights = other.regionPcaWeights;
}

template <class T>
void PatchBlendModel<T>::State::ResetPatchVertexDeltas()
{
    for (int id = 0; id < NumPatches(); ++id)
    {
        ResetPatchVertexDeltas(id);
    }
}

template <class T>
void PatchBlendModel<T>::State::ResetPatchVertexDeltas(int id)
{
    regionVertexDeltas[id].resize(3, 0);
}

template <class T>
bool PatchBlendModel<T>::State::HasPatchVertexDeltas() const
{
    for (int id = 0; id < NumPatches(); ++id)
    {
        if (HasPatchVertexDeltas(id)) return true;
    }
    return false;
}

template <class T>
bool PatchBlendModel<T>::State::HasPatchVertexDeltas(int id) const
{
    return (regionVertexDeltas[id].cols() > 0);
}

template <class T>
void PatchBlendModel<T>::State::BakeVertexDeltas(const Eigen::Matrix<T, 3, -1>& vertexDeltas, const PatchBlendModel<T>& patchBlendModel)
{
    if (vertexDeltas.cols() != patchBlendModel.NumVertices())
    {
        ResetPatchVertexDeltas();
        return;
    }

    std::vector<Eigen::Matrix<T, 3, 3>> scaledRotations(NumPatches());
    
    for (int id = 0; id < NumPatches(); ++id)
    {
        regionVertexDeltas[id].resize(3, patchBlendModel.NumVerticesForPatch(id));
        scaledRotations[id] = PatchRotation(id).toRotationMatrix().transpose() / PatchScale(id);
    }
    for (int vID = 0; vID < patchBlendModel.NumVertices(); ++vID)
    {
        Eigen::Vector3<T> vertexDelta = vertexDeltas.col(vID);
        for (const auto& [regionIndex, regionVID, weight] : patchBlendModel.m_globalBlendMatrix[vID])
        {
            regionVertexDeltas[regionIndex].col(regionVID) = scaledRotations[regionIndex] * vertexDelta;
        }
    }
}

template <class T>
Eigen::Matrix<T, 3, -1> PatchBlendModel<T>::State::EvaluateVertexDeltas(const PatchBlendModel<T>& patchBlendModel) const
{
    bool hasVertexDeltas = false;
    for (int id = 0; id < NumPatches(); ++id)
    {
        hasVertexDeltas |= (regionVertexDeltas[id].cols() > 0);
    }
    if (!hasVertexDeltas) return Eigen::Matrix<T, 3, -1>();

    std::vector<Eigen::Matrix<T, 3, 3>> scaledRotations(NumPatches());
    for (int id = 0; id < NumPatches(); ++id)
    {
        scaledRotations[id] = PatchScale(id) * PatchRotation(id).toRotationMatrix();
    }

    Eigen::Matrix<T, 3, -1> vertexDeltas = Eigen::Matrix<T, 3, -1>::Zero(3, patchBlendModel.NumVertices());

    auto evaluateVertexDeltas = [&](int start, int end) {
        for (int vID = start; vID < end; ++vID)
        {
            Eigen::Vector3<T> vertexDelta = vertexDeltas.col(vID);
            for (const auto& [regionIndex, regionVID, weight] : patchBlendModel.m_globalBlendMatrix[vID])
            {
                if (regionVertexDeltas[regionIndex].cols() > 0)
                {
                    vertexDelta += weight * (scaledRotations[regionIndex] * regionVertexDeltas[regionIndex].col(regionVID));
                }
            }
            vertexDeltas.col(vID) = vertexDelta;
        }
    };
    if (patchBlendModel.m_threadPool) patchBlendModel.m_threadPool->AddTaskRangeAndWait((int)vertexDeltas.cols(), evaluateVertexDeltas);
    else evaluateVertexDeltas(0, (int)vertexDeltas.cols());

    return vertexDeltas;
}

template <class T>
void PatchBlendModel<T>::State::SymmetrizeRegions(
    const std::vector<int>& patchSymmetries,
    bool symmetrizeTransform,
    bool symmetrizePca,
    const PatchBlendModel<T>& patchBlendModel,
    const std::vector<int>& vertexSymmetries)
{
    if ((int)patchSymmetries.size() != NumPatches()) return;

    auto symmetricInterpolate = [](const Eigen::Vector3<T>& p1, const Eigen::Vector3<T>& p2) -> Eigen::Vector3<T> {
        return 0.5f * Eigen::Vector3<T>(p1[0] - p2[0], p1[1] + p2[1], p1[2] + p2[2]);
    };
    auto symmetricRotInterpolate = [](const Eigen::Vector3<T>& p1, const Eigen::Vector3<T>& p2) -> Eigen::Vector3<T> {
        return 0.5f * Eigen::Vector3<T>(p1[0] + p2[0], p1[1] - p2[1], p1[2] - p2[2]);
    };

    State newState = *this;
    for (int i = 0; i < NumPatches(); ++i)
    {
        const int symmetricIndex = patchSymmetries[i];
        if (symmetricIndex >= 0)
        {
            if (symmetrizeTransform)
            {
                newState.SetPatchRotationEulerDegrees(i, symmetricRotInterpolate(PatchRotationEulerDegrees(i), PatchRotationEulerDegrees(symmetricIndex)));
                newState.SetPatchTranslation(i, symmetricInterpolate(PatchTranslation(i), PatchTranslation(symmetricIndex)));
                newState.SetPatchScale(i, 0.5f * (PatchScale(i) + PatchScale(symmetricIndex)));
            }
            if (symmetrizePca && (int)vertexSymmetries.size() == patchBlendModel.NumVertices())
            {
                std::vector<int> regionIndices1;
                std::vector<int> regionIndices2;
                for (int vID = 0; vID < (int)vertexSymmetries.size(); ++vID)
                {
                    const int symmetricVertexIndex = vertexSymmetries[vID];
                    if (symmetricVertexIndex < 0) continue;

                    int patchVertexIndex1 = -1;
                    int patchVertexIndex2 = -1;

                    for (const auto& [patchId, patchVID, weight] : patchBlendModel.BlendMatrix()[vID])
                    {
                        if (patchId == i)
                        {
                            patchVertexIndex1 = patchVID;
                        }
                    }
                    for (const auto& [patchId, patchVID, weight] : patchBlendModel.BlendMatrix()[symmetricVertexIndex])
                    {
                        if (patchId == symmetricIndex)
                        {
                            patchVertexIndex2 = patchVID;
                        }
                    }
                    if (patchVertexIndex1 >= 0 && patchVertexIndex2 >= 0)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            regionIndices1.push_back(3 * patchVertexIndex1 + k);
                            regionIndices2.push_back(3 * patchVertexIndex2 + k);
                        }
                    }
                }

                if (regionIndices1.size() > 0)
                {
                    Eigen::VectorX<T> delta1 = patchBlendModel.PatchModels()[i].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC)(regionIndices1, Eigen::all) * PatchPcaWeights(i);
                    Eigen::VectorX<T> delta2 = patchBlendModel.PatchModels()[symmetricIndex].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC)(regionIndices2, Eigen::all) * PatchPcaWeights(symmetricIndex);
                    Eigen::VectorX<T> newDelta = (delta1 + delta2) * T(0.5);
                    newDelta(Eigen::seq(0, Eigen::last, 3)) = T(0.5) * (delta1(Eigen::seq(0, Eigen::last, 3)) - delta2(Eigen::seq(0, Eigen::last, 3)));
                    Eigen::Matrix<T, -1, -1> A = patchBlendModel.PatchModels()[i].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC)(regionIndices1, Eigen::all);
                    Eigen::VectorX<T> newWeights = (A.transpose() * A).inverse() * A.transpose() * newDelta;
                    newState.SetPatchPcaWeights(i, newWeights);
                }
            }
        }
    }
    *this = newState;
}

template <class T>
void PatchBlendModel<T>::State::SymmetricRegionCopy(const std::vector<int>& patchSymmetries, int regionId, bool includingPcaWeights)
{
    if ((int)patchSymmetries.size() != NumPatches()) return;

    auto symmetricSwap = [](const Eigen::Vector3<T>& p) -> Eigen::Vector3<T> {
        return Eigen::Vector3<T>(-p[0], p[1], p[2]);
    };

    auto newState = *this;
    const int symmetricIndex = patchSymmetries[regionId];
    if (symmetricIndex >= 0)
    {
        if (symmetricIndex != regionId)
        {
            newState.SetPatchRotationEulerDegrees(symmetricIndex, -symmetricSwap(PatchRotationEulerDegrees(regionId)));
            newState.SetPatchTranslation(symmetricIndex, symmetricSwap(PatchTranslation(regionId)));
            newState.SetPatchScale(symmetricIndex, PatchScale(regionId));
            if (includingPcaWeights)
            {
                newState.SetPatchPcaWeights(symmetricIndex, PatchPcaWeights(regionId));
            }
        }
        else
        {
            // make sure region is self symmetric in rigid
            Eigen::Vector3<T> rot = newState.PatchRotationEulerDegrees(regionId);
            rot[1] = 0.0f;
            rot[2] = 0.0f;
            newState.SetPatchRotationEulerDegrees(regionId, rot);
            Eigen::Vector3<T> t = newState.PatchTranslation(regionId);
            t[0] = 0;
            newState.SetPatchTranslation(regionId, t);
            // no mode to symmetrize pca weights for self symmetric regions
        }
    }
    *this = newState;
}

template <class T>
Eigen::VectorX<T> PatchBlendModel<T>::State::ConcatenatePatchPcaWeights() const
{
    return geoutils::CombinePoints(regionPcaWeights);
}

template <class T>
void PatchBlendModel<T>::State::SetConcatenatedPatchPcaWeights(const Eigen::VectorX<T>& w)
{
    int idx = 0;
    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        const int size = (int)regionPcaWeights[regionIndex].size();
        if (idx + size > (int)w.size())
        {
            CARBON_CRITICAL("invalid size of concatenated pca weights");
        }
        regionPcaWeights[regionIndex] = w.segment(idx, size);
        idx += size;
    }
    if (idx != (int)w.size())
    {
        CARBON_CRITICAL("invalid size of concatenated pca weights: expected {}, but got {}", idx, w.size());
    }
}


template <class T>
JsonElement PatchBlendModel<T>::State::ToJson() const
{
    JsonElement json(JsonElement::JsonType::Array);
    for (int i = 0; i < NumPatches(); ++i)
    {
        JsonElement jsonInner(JsonElement::JsonType::Array);
        jsonInner.Append(JsonElement(regionScales[i]));
        jsonInner.Append(io::ToJson(regionRotations[i].coeffs()));
        jsonInner.Append(io::ToJson(regionTranslations[i]));
        jsonInner.Append(io::ToJson(regionPcaWeights[i]));
        json.Append(std::move(jsonInner));
    }
    return json;
}

template <class T>
void PatchBlendModel<T>::State::FromJson(const JsonElement& json)
{
    const int numPatches = (int)json.Size();
    regionScales.resize(numPatches);
    regionRotations.resize(numPatches);
    regionTranslations.resize(numPatches);
    regionPcaWeights.resize(numPatches);
    regionVertexDeltas.clear();
    regionVertexDeltas.resize(numPatches);
    for (int i = 0; i < numPatches; ++i)
    {
        regionScales[i] = json[i][0].Get<T>();
        regionRotations[i] = Eigen::Quaternion<T>(io::FromJson<Eigen::Vector4<T>>(json[i][1]));
        regionTranslations[i] = io::FromJson<Eigen::Vector3<T>>(json[i][2]);
        regionPcaWeights[i] = io::FromJson<Eigen::VectorX<T>>(json[i][3]);
    }
}

template <class T>
void PatchBlendModel<T>::State::DeserializeFromVector(const Eigen::VectorX<T>& vector)
{
    auto verifySize = [](const Eigen::VectorX<T>& vec, int offset, int size) {
        if (offset < 0 || size <= 0)
        {
          CARBON_CRITICAL("Deserialize from vector failed. invalid offset or size: {} {}", offset, size);
        }
        if (offset + size > (int) vec.size())
        {
          CARBON_CRITICAL("Deserialize from vector failed. input vector not of correct size: {} {} ({})", offset, size, (int) vec.size());
        }
    };

    verifySize(vector, 0, 1);

    const int numPatches = (int)vector[0];
    verifySize(vector, 1, numPatches * 8);

    regionScales.resize(numPatches);
    regionRotations.resize(numPatches);
    regionTranslations.resize(numPatches);
    regionPcaWeights.resize(numPatches);

    int inputVectorIterator = 1;
    for (int p = 0; p < numPatches; ++p)
    {
        verifySize(vector, inputVectorIterator, /*scale (1) + rotation(4) + translation(3)*/ 8);

        regionScales[p] = vector[inputVectorIterator];
        inputVectorIterator++;

        regionRotations[p] = Eigen::Quaternion<T>(Eigen::Vector4<T>(vector.segment(inputVectorIterator, 4)));
        inputVectorIterator += 4;

        regionTranslations[p] = vector.segment(inputVectorIterator, 3);
        inputVectorIterator += 3;

        verifySize(vector, inputVectorIterator, 1);

        const int pcaWeightsCount = (int)vector[inputVectorIterator];
        inputVectorIterator++;

        verifySize(vector, inputVectorIterator, pcaWeightsCount);

        regionPcaWeights[p] = vector.segment(inputVectorIterator, (size_t)pcaWeightsCount);
        inputVectorIterator += pcaWeightsCount;
    }
}

template <class T>
Eigen::VectorX<T> PatchBlendModel<T>::State::SerializeToVector() const
{
    const int numPatches = NumPatches();
    const int patchDoF = 8;
    const int patchMetadataCount = 1;
    const int patchesPcaParametersSize = (int)ConcatenatePatchPcaWeights().size();
    const int totalSerializedVectorSize = /*numRegionsMetadata */1 + patchesPcaParametersSize + numPatches * (patchDoF + patchMetadataCount);

    Eigen::VectorX<T> output = Eigen::VectorX<T>::Zero(totalSerializedVectorSize);

    output[0] = (T)numPatches;
    int outputVectorIterartor = 1;

    for (int p = 0; p < numPatches; ++p)
    {
        // add scale - one parameter
        output[outputVectorIterartor] =  regionScales[p];
        outputVectorIterartor++;

        // add rotation - quaternion
        output.segment(outputVectorIterartor, 4) = regionRotations[p].coeffs();
        outputVectorIterartor += 4;

        // add translation vector
        output.segment(outputVectorIterartor, 3) = regionTranslations[p];
        outputVectorIterartor += 3;

        // region modes count so we can reconstruct
        output[outputVectorIterartor] = (T)regionPcaWeights[p].size();
        outputVectorIterartor++;

        output.segment(outputVectorIterartor, regionPcaWeights[p].size()) = regionPcaWeights[p];
        outputVectorIterartor+= (int)regionPcaWeights[p].size();
    }

    return output;
}

template <class T>
PatchBlendModel<T>::State PatchBlendModel<T>::OptimizationState::CreateState() const
{
    PatchBlendModel<T>::State state(NumPatches());
    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        state.regionPcaWeights[regionIndex].resize(NumPcaModesForPatch(regionIndex));
    }
    CopyToState(state);
    return state;

}

template <class T>
void PatchBlendModel<T>::OptimizationState::CopyToState(PatchBlendModel<T>::State& state) const
{
    if (state.NumPatches() != NumPatches())
    {
        CARBON_CRITICAL("number of patches is not matching: {} vs {}", NumPatches(), state.NumPatches());
    }
    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        const int offset = regionVariableOffsets[regionIndex];
        const int numModes = NumPcaModesForPatch(regionIndex);
        if (numModes != state.PatchPcaWeights(regionIndex).size())
        {
            CARBON_CRITICAL("inconsistent modes size: {} vs {}", numModes, state.PatchPcaWeights(regionIndex).size());
        }
        state.SetPatchPcaWeights(regionIndex, allVariables.Value().segment(offset, numModes));
        const Eigen::Vector3<T> dR = allVariables.Value().segment(offset + numModes + 0, 3);
        state.SetPatchRotation(regionIndex, (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * regionRotations[regionIndex]).normalized());
        state.SetPatchTranslation(regionIndex, allVariables.Value().segment(offset + numModes + 3, 3));
        state.SetPatchScale(regionIndex, allVariables.Value().segment(offset + numModes + 6, 1)[0]);
    }
}

template <class T>
void PatchBlendModel<T>::OptimizationState::CopyFromState(const PatchBlendModel<T>::State& state)
{
    regionRotations = state.regionRotations;
    Eigen::VectorX<T> values(allVariables.Size());

    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        const int offset = regionVariableOffsets[regionIndex];
        const int numModes = NumPcaModesForPatch(regionIndex);
        values.segment(offset, numModes) = state.PatchPcaWeights(regionIndex);
        values.segment(offset + numModes + 0, 3).setZero();
        values.segment(offset + numModes + 3, 3) = state.PatchTranslation(regionIndex);
        values.segment(offset + numModes + 6, 1)[0] = state.PatchScale(regionIndex);
    }
    allVariables.Set(values);
}


template <class T>
int PatchBlendModel<T>::OptimizationState::NumParameters() const
{
    return allVariables.Size();
}

template <typename T>
void PatchBlendModel<T>::OptimizationState::ResetParameters(const PatchBlendModel<T>& patchBlendModel)
{
    if (allVariables.Size() == 0)
    {
        return;
    }

    Eigen::VectorX<T> values = allVariables.Value();
    values.setZero();

    // set scale to 1 and translation to origin center of gravity per region
    for (size_t k = 0; k < patchBlendModel.m_regionModels.size(); ++k)
    {
        const int rigidOffset = regionVariableOffsets[k] + patchBlendModel.m_regionModels[k].NumPCAModes();
        values[rigidOffset + 6] = T(1);
        values.segment(rigidOffset + 3, 3) = patchBlendModel.m_centerOfGravityPerRegion[k];
    }

    allVariables.Set(values);

    for (size_t i = 0; i < regionRotations.size(); ++i)
    {
        regionRotations[i] = Eigen::Quaternion<T>::Identity();
    }
}

template <class T>
void PatchBlendModel<T>::OptimizationState::ClearFixedPatch()
{
    fixedRegion = -1;
}

template <class T>
int PatchBlendModel<T>::OptimizationState::FixedPatch() const
{
    return fixedRegion;
}

template <class T>
void PatchBlendModel<T>::OptimizationState::SetFixedPatch(int patchId)
{
    fixedRegion = patchId;
}

template <class T>
void PatchBlendModel<T>::OptimizationState::SetModelParameters(const Eigen::VectorX<T>& parameters)
{
    if ((int)parameters.size() != allVariables.Size())
    {
        CARBON_CRITICAL("model parameters size does not match: {} vs {}", parameters.size() != allVariables.Size());
    }
    allVariables.Set(parameters);
}

template <class T>
const Eigen::VectorX<T>& PatchBlendModel<T>::OptimizationState::GetModelParameters() const
{
    return allVariables.Value();
}

template <class T>
int PatchBlendModel<T>::OptimizationState::NumPatches() const
{
    return (int)transformedRegionModels.size();
}

template <class T>
int PatchBlendModel<T>::OptimizationState::NumPcaModesForPatch(int id) const
{
    return transformedRegionModels[id].NumPCAModes();
}


template <class T>
void PatchBlendModel<T>::OptimizationState::SetPatchPcaWeights(int id, const Eigen::VectorX<T>& weights)
{
    if (allVariables.Size() == 0)
    {
        return;
    }

    Eigen::VectorX<T> values = allVariables.Value();
    values.segment(regionVariableOffsets[id], transformedRegionModels[id].NumPCAModes()) = weights;
    allVariables.Set(values);
}

template <class T>
T PatchBlendModel<T>::OptimizationState::PatchScale(int id) const
{
    Eigen::VectorX<T> allParameters = allVariables.Value();
    const int rigidOffset = regionVariableOffsets[id] + transformedRegionModels[id].NumPCAModes();
    return allParameters[rigidOffset + 6];
}

template <class T>
void PatchBlendModel<T>::OptimizationState::SetPatchScale(int id, T scale)
{
    Eigen::VectorX<T> allParameters = allVariables.Value();
    const int rigidOffset = regionVariableOffsets[id] + transformedRegionModels[id].NumPCAModes();
    allParameters[rigidOffset + 6] = scale;
    allVariables.Set(allParameters);
}

template <class T>
Eigen::Vector3<T> PatchBlendModel<T>::OptimizationState::PatchTranslation(int id) const
{
    Eigen::VectorX<T> values = allVariables.Value();
    const int offset = regionVariableOffsets[id];
    const int numModes = transformedRegionModels[id].NumPCAModes();
    const int rigidOffset = offset + numModes;
    const Eigen::Vector3<T> dt = values.segment(rigidOffset + 3, 3);
    return dt;
}

template <class T>
void PatchBlendModel<T>::OptimizationState::SetPatchTranslation(int id, const Eigen::Vector3<T>& translation)
{
    Eigen::VectorX<T> values = allVariables.Value();
    const int offset = regionVariableOffsets[id];
    const int numModes = transformedRegionModels[id].NumPCAModes();
    const int rigidOffset = offset + numModes;
    values.segment(rigidOffset + 3, 3) = translation;
    allVariables.Set(values);
}

template <class T>
Eigen::Vector3<T> PatchBlendModel<T>::OptimizationState::PatchRotation(int id) const
{
    Eigen::VectorX<T> values = allVariables.Value();
    const int offset = regionVariableOffsets[id];
    const int numModes = transformedRegionModels[id].NumPCAModes();
    const int rigidOffset = offset + numModes;
    const Eigen::Vector3<T> dR = values.segment(rigidOffset, 3);
    return dR;
}

template <class T>
void PatchBlendModel<T>::OptimizationState::SetPatchRotation(int id, const Eigen::Vector3<T>& rotation)
{
    Eigen::VectorX<T> values = allVariables.Value();
    const int offset = regionVariableOffsets[id];
    const int numModes = transformedRegionModels[id].NumPCAModes();
    const int rigidOffset = offset + numModes;
    values.segment(rigidOffset, 3) = rotation;
    allVariables.Set(values);
}

template <class T>
void PatchBlendModel<T>::OptimizationState::MakeRotationConstantForPatch(int patchId)
{
    const int offset = regionVariableOffsets[patchId];
    const int numModes = transformedRegionModels[patchId].NumPCAModes();
    const int rigidOffset = offset + numModes;
    for (int i = 0; i < 3; i++)
    {
        constantIndices.insert(rigidOffset + i);
    }
    allVariables.MakeIndividualIndicesConstant(std::vector<int>(constantIndices.begin(), constantIndices.end()));
}

template <class T>
void PatchBlendModel<T>::OptimizationState::MakeTranslationConstantForPatch(int patchId)
{
    const int offset = regionVariableOffsets[patchId];
    const int numModes = transformedRegionModels[patchId].NumPCAModes();
    const int rigidOffset = offset + numModes + /*rotationOffset=*/3;
    for (int i = 0; i < 3; i++)
    {
        constantIndices.insert(rigidOffset + i);
    }
    allVariables.MakeIndividualIndicesConstant(std::vector<int>(constantIndices.begin(), constantIndices.end()));
}

template <class T>
void PatchBlendModel<T>::OptimizationState::MakeScaleConstantForPatch(int patchId)
{
    const int offset = regionVariableOffsets[patchId];
    const int numModes = transformedRegionModels[patchId].NumPCAModes();
    const int rigidOffset = offset + numModes + /*rigidOffset=*/6;
    constantIndices.insert(rigidOffset);
    allVariables.MakeIndividualIndicesConstant(std::vector<int>(constantIndices.begin(), constantIndices.end()));
}

template <class T>
void PatchBlendModel<T>::OptimizationState::RemoveConstraintsOnVariables()
{
    constantIndices.clear();
    allVariables.MakeIndividualIndicesConstant({});
}

template <class T>
Eigen::VectorX<T> PatchBlendModel<T>::OptimizationState::PatchPcaWeights(int id) const
{
    const Eigen::VectorX<T> allParams = allVariables.Value();
    const int regionOffset = regionVariableOffsets[id];
    const int regionPcaModesNum = transformedRegionModels[id].NumPCAModes();
    return allParams.segment(regionOffset, regionPcaModesNum);
}

template <class T>
void PatchBlendModel<T>::OptimizationState::BakeRotationLinearization()
{
    Eigen::VectorX<T> values = allVariables.Value();

    // copy rigid transforms
    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        const int rigidOffset = regionVariableOffsets[regionIndex] + transformedRegionModels[regionIndex].NumPCAModes();
        const Eigen::Vector3<T> dR = allVariables.Value().segment(rigidOffset + 0, 3);
        regionRotations[regionIndex] =
            (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * regionRotations[regionIndex]).normalized();
        // set linearized rotation to zero
        values.segment(rigidOffset, 3).setZero();
    }

    allVariables.Set(values);
}

template <class T>
void PatchBlendModel<T>::OptimizationState::TransformPatch(const Affine<T, 3, 3>& aff, int patchId)
{
    // transform the patches
    Eigen::VectorX<T> values = allVariables.Value();
    QRigidMotion<T> deltaTransform(aff.Matrix());
    const int offset = regionVariableOffsets[patchId];
    const int numModes = transformedRegionModels[patchId].NumPCAModes();
    const int rigidOffset = offset + numModes;

    const Eigen::Vector3<T> dR = values.segment(rigidOffset + 0, 3);
    const Eigen::Vector3<T> dt = values.segment(rigidOffset + 3, 3);

    QRigidMotion<T> currTransform;
    currTransform.q = (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * regionRotations[patchId]).normalized();
    currTransform.t = dt;
    QRigidMotion<T> newTransform = deltaTransform * currTransform;

    regionRotations[patchId] = newTransform.q;

    values.segment(rigidOffset + 0, 3).setZero();
    values.segment(rigidOffset + 3, 3) = newTransform.t;

    allVariables.Set(values);
}

template <class T>
void PatchBlendModel<T>::OptimizationState::TransformPatches(const Affine<T, 3, 3>& aff)
{
    // transform the patches
    Eigen::VectorX<T> values = allVariables.Value();

    QRigidMotion<T> deltaTransform(aff.Matrix());
    for (size_t k = 0; k < regionRotations.size(); ++k)
    {
        const int offset = regionVariableOffsets[k];
        const int numModes = transformedRegionModels[k].NumPCAModes();
        const int rigidOffset = offset + numModes;

        const Eigen::Vector3<T> dR = values.segment(rigidOffset + 0, 3);
        const Eigen::Vector3<T> dt = values.segment(rigidOffset + 3, 3);

        QRigidMotion<T> currTransform;
        currTransform.q = (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * regionRotations[k]).normalized();
        currTransform.t = dt;
        QRigidMotion<T> newTransform = deltaTransform * currTransform;

        regionRotations[k] = newTransform.q;

        values.segment(rigidOffset + 0, 3).setZero();
        values.segment(rigidOffset + 3, 3) = newTransform.t;
    }

    allVariables.Set(values);
}


template <class T>
PatchBlendModel<T>::PatchBlendModel()
    : m_threadPool(TaskThreadPool::GlobalInstance(true))
{
}

template <class T>
PatchBlendModel<T>::PatchBlendModel(const std::shared_ptr<TaskThreadPool>& threadPool)
    : m_threadPool(threadPool)
{
}

template <class T> PatchBlendModel<T>::~PatchBlendModel() = default;
template <class T> PatchBlendModel<T>::PatchBlendModel(PatchBlendModel&&) = default;
template <class T> PatchBlendModel<T>& PatchBlendModel<T>::operator=(PatchBlendModel&&) = default;

template <class T>
const std::string& PatchBlendModel<T>::PatchName(int id) const
{
    return m_patchNames[id];
}

template <class T>
PatchBlendModel<T>::State PatchBlendModel<T>::CreateState() const
{
    State state;
    state.Reset(*this);
    return state;
}

template <class T>
PatchBlendModel<T>::OptimizationState PatchBlendModel<T>::CreateOptimizationState() const
{
    OptimizationState state;
    // TODO: hardcoded to use the nose region as fixed region (previous implementation of LoadModel()), otherwise use 10 (previous implementation of LoadModelFromIdentity())
    state.SetFixedPatch(10);
    for (int i = 0; i < NumPatches(); ++i)
    {
        if (PatchName(i) == "nose")
        {
            state.SetFixedPatch(i);
        }
    }
    state.regionRotations = std::vector<Eigen::Quaternion<T>>(m_regionModels.size(), Eigen::Quaternion<T>::Identity());
    state.transformedRegionModels = m_regionModels;

    int accumulatedParameters = 0;
    state.regionVariableOffsets.clear();
    for (int i = 0; i < (int)m_regionModels.size(); i++)
    {
        state.regionVariableOffsets.push_back(accumulatedParameters);
        accumulatedParameters += m_regionModels[i].NumTotalModes();
    }
    state.regionVariableOffsets.push_back(accumulatedParameters);

    state.allVariables = VectorVariable<T>(state.regionVariableOffsets.back());
    state.ResetParameters(*this);
    return state;
}


template <class T>
std::pair<DiffDataMatrix<T, 3, -1>, Cost<T>> PatchBlendModel<T>::EvaluateVerticesAndConstraints(Context<T>* context, OptimizationState& state, T modelRegularization, T patchSmoothness) const
{
    // evaluate so that the variables are being used
    const DiffData<T> values = state.allVariables.Evaluate(context);

    // update the vertices per region (and the modes if context/Jacobian is used)
    UpdateRegionModels(state, /*withModes=*/values.HasJacobian());

    DiffDataMatrix<T, 3, -1> diffGlobalVertices(3, 0, DiffData<T>(nullptr, 0));
    DiffData<T> regDiffData(nullptr, 0);
    DiffData<T> patchDiffData(nullptr, 0);
    if (m_threadPool)
    {
        TaskFutures taskFutures;

        taskFutures.Add(m_threadPool->AddTask([&]() { diffGlobalVertices = EvaluateVertices(state, values); }));

        // evaluate model regularization
        if (modelRegularization > 0)
        {
            taskFutures.Add(m_threadPool->AddTask([&]() { regDiffData = EvaluateRegularization(state, values); }));
        }

        // evaluate patch regularization
        if (patchSmoothness > 0)
        {
            taskFutures.Add(m_threadPool->AddTask([&]() { patchDiffData = EvaluatePatchSmoothness(state, values); }));
        }

        taskFutures.Wait();
    }
    else
    {
        diffGlobalVertices = EvaluateVertices(state, values);

        if (modelRegularization > 0)
        {
            regDiffData = EvaluateRegularization(state, values);
        }

        // evaluate patch regularization
        if (patchSmoothness > 0)
        {
            patchDiffData = EvaluatePatchSmoothness(state, values);
        }
    }

    Cost<T> cost;
    if (patchSmoothness > 0)
    {
        cost.Add(std::move(patchDiffData), patchSmoothness);
    }
    if (modelRegularization > 0)
    {
        cost.Add(std::move(regDiffData), modelRegularization);
    }

    return { std::move(diffGlobalVertices), std::move(cost) };
}

template <class T>
DiffDataMatrix<T, 3, -1> PatchBlendModel<T>::EvaluateVertices(OptimizationState& state, const DiffData<T>& values) const
{
    Eigen::Matrix<T, 3, -1> output = Eigen::Matrix<T, 3, -1>(3, NumVertices());
    for (int vID = 0; vID < NumVertices(); ++vID)
    {
        Eigen::Vector3<T> v = Eigen::Vector3<T>::Zero();
        for (const auto& [regionIndex, regionVID, weight] : m_globalBlendMatrix[vID])
        {
            v += state.transformedRegionModels[regionIndex].Base().col(regionVID) * weight;
        }
        output.col(vID) = v;
    }

    // set up the jacobian for the vertex evaluation
    std::shared_ptr<const SparseJacobian<T>> jacobian;
    if (values.HasJacobian())
    {
        SparseMatrixPtr<T> smat = std::make_shared<SparseMatrix<T>>(3 * NumVertices(), values.Jacobian().Cols());
        smat->reserve(state.numNonZerosVertexJacobian);
        const int startCol = values.Jacobian().StartCol();

        for (int vID = 0; vID < NumVertices(); ++vID)
        {
            for (int k = 0; k < 3; ++k)
            {
                smat->startVec(3 * vID + k);
                for (const auto& [regionIndex, regionVID, weight] : m_globalBlendMatrix[vID])
                {
                    const int offset = state.regionVariableOffsets[regionIndex];
                    for (Eigen::Index c = 0; c < state.transformedRegionModels[regionIndex].MutableModes().cols(); ++c)
                    {
                        smat->insertBackByOuterInner(3 * vID + k,
                                                     startCol + offset + c) = weight *
                            state.transformedRegionModels[regionIndex].MutableModes()(3 * regionVID + k, c);
                    }
                }
            }
        }
        smat->finalize();
        state.numNonZerosVertexJacobian = int(smat->nonZeros());

        jacobian = std::make_shared<SparseJacobian<T>>(smat, startCol);
    }

    return DiffDataMatrix<T, 3, -1>(output, jacobian);
}

template <class T>
int PatchBlendModel<T>::NumPcaModesForPatch(int id) const { return m_regionModels[id].NumPCAModes(); }

template <class T>
DiffData<T> PatchBlendModel<T>::EvaluateRegularization(OptimizationState& state, const DiffData<T>& values) const
{
    int numValues = 0;
    for (int k = 0; k < NumPatches(); ++k)
    {
        numValues += m_regionModels[k].NumPCAModes();
    }
    Eigen::VectorX<T> regValues(numValues);

    std::shared_ptr<const SparseJacobian<T>> regJacobian;
    std::vector<Eigen::Triplet<T>> triplets;

    int offset = 0;
    for (int k = 0; k < NumPatches(); ++k)
    {
        const int numModes = m_regionModels[k].NumPCAModes();
        const Eigen::VectorX<T> coeffs = values.Value().segment(state.regionVariableOffsets[k], numModes);
        regValues.segment(offset, numModes) = coeffs;
        if (values.HasJacobian())
        {
            const int startCol = values.Jacobian().StartCol();
            for (int j = 0; j < numModes; ++j)
            {
                triplets.push_back(Eigen::Triplet<T>(offset + j, startCol + state.regionVariableOffsets[k] + j, T(1)));
            }
        }
        offset += numModes;
    }

    if (values.HasJacobian())
    {
        const int startCol = values.Jacobian().StartCol();
        SparseMatrixPtr<T> smat = std::make_shared<SparseMatrix<T>>(numValues, values.Jacobian().Cols());
        smat->setFromTriplets(triplets.begin(), triplets.end());
        regJacobian = std::make_shared<SparseJacobian<T>>(smat, startCol);
    }

    return DiffData<T>(regValues, regJacobian);
}

template <class T>
DiffData<T> PatchBlendModel<T>::EvaluatePatchSmoothness(OptimizationState& state, const DiffData<T>& values) const
{
    std::vector<Eigen::Vector3<T>> regCosts;
    for (int vID = 0; vID < NumVertices(); ++vID)
    {
        for (size_t j1 = 0; j1 < m_globalBlendMatrix[vID].size(); ++j1)
        {
            for (size_t j2 = j1 + 1; j2 < m_globalBlendMatrix[vID].size(); ++j2)
            {
                const int regionIndex1 = std::get<0>(m_globalBlendMatrix[vID][j1]);
                const int regionvID1 = std::get<1>(m_globalBlendMatrix[vID][j1]);
                const int regionIndex2 = std::get<0>(m_globalBlendMatrix[vID][j2]);
                const int regionvID2 = std::get<1>(m_globalBlendMatrix[vID][j2]);
                const Eigen::Vector3<T> vertexDiff = state.transformedRegionModels[regionIndex1].Base().col(regionvID1) -
                    state.transformedRegionModels[regionIndex2].Base().col(regionvID2);
                regCosts.push_back(vertexDiff);
            }
        }
    }

    Eigen::VectorX<T> regCostsVec = Eigen::Map<const Eigen::VectorX<T>>((const T*)regCosts.data(), regCosts.size() * 3);

    // evaluate smoothness of patches i.e. the same vertex for two different patches
    // should evaluate to a similar position
    JacobianConstPtr<T> patchJacobian;
    if (values.HasJacobian())
    {
        SparseMatrixPtr<T> smat = std::make_shared<SparseMatrix<T>>(regCostsVec.size(), values.Jacobian().Cols());
        smat->reserve(state.numNonZerosSmoothnessJacobian);
        const int startCol = values.Jacobian().StartCol();

        int rowIndex = 0;
        for (int vID = 0; vID < NumVertices(); ++vID)
        {
            for (size_t j1 = 0; j1 < m_globalBlendMatrix[vID].size(); ++j1)
            {
                for (size_t j2 = j1 + 1; j2 < m_globalBlendMatrix[vID].size(); ++j2)
                {
                    const int regionIndex1 = std::get<0>(m_globalBlendMatrix[vID][j1]);
                    const int regionVID1 = std::get<1>(m_globalBlendMatrix[vID][j1]);
                    const int regionIndex2 = std::get<0>(m_globalBlendMatrix[vID][j2]);
                    const int regionVID2 = std::get<1>(m_globalBlendMatrix[vID][j2]);
                    const int offset1 = state.regionVariableOffsets[regionIndex1];
                    const int offset2 = state.regionVariableOffsets[regionIndex2];
                    for (int k = 0; k < 3; ++k)
                    {
                        smat->startVec(rowIndex + k);
                        for (Eigen::Index c = 0; c < state.transformedRegionModels[regionIndex1].MutableModes().cols(); ++c)
                        {
                            smat->insertBackByOuterInner(rowIndex + k,
                                                         startCol + offset1 +
                                                         c) =
                                state.transformedRegionModels[regionIndex1].MutableModes()(3 * regionVID1 + k, c);
                        }
                        for (Eigen::Index c = 0; c < state.transformedRegionModels[regionIndex2].MutableModes().cols(); ++c)
                        {
                            smat->insertBackByOuterInner(rowIndex + k,
                                                         startCol + offset2 +
                                                         c) =
                                -state.transformedRegionModels[regionIndex2].MutableModes()(3 * regionVID2 + k, c);
                        }
                    }
                    rowIndex += 3;
                }
            }
        }
        smat->finalize();

        state.numNonZerosSmoothnessJacobian = int(smat->nonZeros());
        patchJacobian = std::make_shared<SparseJacobian<T>>(smat, startCol);
    }

    return DiffData<T>(std::move(regCostsVec), patchJacobian);
}

template <class T>
void PatchBlendModel<T>::LoadFromIdentityModel(const std::shared_ptr<const IdentityBlendModel<T, -1>>& identityModel)
{
    if (identityModel->Base().rows() != 3)
    {
        CARBON_CRITICAL("Could not load from IdentityBlendModel. PatchBlendModel only supports 3D data.");
    }

    m_centerOfGravityPerRegion.clear();
    m_regionModels.clear();
    m_patchNames.clear();

    int readNumOfRegions = identityModel->NumRegions();
    Eigen::Matrix<T, 3, -1> mean = identityModel->Base();

    const int numVertices = int(mean.cols());
    m_globalBlendMatrix = std::vector<std::vector<std::tuple<int, int, T>>>(numVertices);

    for (int i = 0; i < readNumOfRegions; i++)
    {
        const Eigen::VectorXi& vertexIDs = identityModel->RegionVertexIds(i);
        const Eigen::VectorX<T>& weights = identityModel->RegionWeights(i);
        const Eigen::Matrix<T, -1, -1>& modes = identityModel->RegionModes(i);

        const int numRegionVertices = int(modes.rows() / 3);
        if (int(vertexIDs.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("vertex_ids and modes matrix for region {} do not match", i);
        }
        if (int(weights.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("weights and modes matrix for region {} do not match", i);
        }

        Eigen::Matrix<T, 3, -1> regionMeanVertices = mean(Eigen::all, vertexIDs);
        Eigen::Vector3<T> centerOfGravityOfRegion = regionMeanVertices.rowwise().mean();
        regionMeanVertices.colwise() -= centerOfGravityOfRegion;
        m_centerOfGravityPerRegion.push_back(centerOfGravityOfRegion);
        m_regionModels.push_back(rt::LinearVertexModel<T>(regionMeanVertices, modes));
        m_patchNames.push_back(identityModel->RegionName(i));

        for (int j = 0; j < numRegionVertices; ++j)
        {
            m_globalBlendMatrix[vertexIDs[j]].push_back({ i, j, weights[j] });
        }
    }

    UpdateBaseVertices();
}

template <class T>
void PatchBlendModel<T>::LoadFromIdentityModel(const std::shared_ptr<const IdentityBlendModel<T>>& identityModel)
{
    m_centerOfGravityPerRegion.clear();
    m_regionModels.clear();
    m_patchNames.clear();

    int readNumOfRegions = identityModel->NumRegions();
    Eigen::Matrix<T, 3, -1> mean = identityModel->Base();

    const int numVertices = int(mean.cols());
    m_globalBlendMatrix = std::vector<std::vector<std::tuple<int, int, T>>>(numVertices);

    for (int i = 0; i < readNumOfRegions; i++)
    {
        const Eigen::VectorXi& vertexIDs = identityModel->RegionVertexIds(i);
        const Eigen::VectorX<T>& weights = identityModel->RegionWeights(i);
        const Eigen::Matrix<T, -1, -1>& modes = identityModel->RegionModes(i);

        const int numRegionVertices = int(modes.rows() / 3);
        if (int(vertexIDs.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("vertex_ids and modes matrix for region {} do not match", i);
        }
        if (int(weights.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("weights and modes matrix for region {} do not match", i);
        }

        Eigen::Matrix<T, 3, -1> regionMeanVertices = mean(Eigen::all, vertexIDs);
        Eigen::Vector3<T> centerOfGravityOfRegion = regionMeanVertices.rowwise().mean();
        regionMeanVertices.colwise() -= centerOfGravityOfRegion;
        m_centerOfGravityPerRegion.push_back(centerOfGravityOfRegion);
        m_regionModels.push_back(rt::LinearVertexModel<T>(regionMeanVertices, modes));
        m_patchNames.push_back(identityModel->RegionName(i));

        for (int j = 0; j < numRegionVertices; ++j)
        {
            m_globalBlendMatrix[vertexIDs[j]].push_back({ i, j, weights[j] });
        }
    }

    UpdateBaseVertices();
}

template <class T>
void PatchBlendModel<T>::LoadModelBinary(const std::string& filename)
{
    auto identityBlendModel = std::make_shared<IdentityBlendModel<T>>();
    identityBlendModel->LoadModelBinary(filename);
    LoadFromIdentityModel(identityBlendModel);
}



template <class T>
void PatchBlendModel<T>::LoadModel(const std::string& identityBlendModelFileOrData)
{
    m_centerOfGravityPerRegion.clear();
    m_regionModels.clear();
    m_patchNames.clear();

    const bool isValidFile = std::filesystem::exists(identityBlendModelFileOrData);
    std::string jsonString;
    if (isValidFile)
    {
        jsonString = ReadFile(identityBlendModelFileOrData);
    }
    else
    {
        jsonString = identityBlendModelFileOrData;
    }

    // see format in IdentityBlendModel.h
    const JsonElement json = ReadJson(jsonString);

    Eigen::Matrix<T, 3, -1> mean;
    io::FromJson(json["mean"], mean);
    const int numVertices = int(mean.cols());
    m_globalBlendMatrix = std::vector<std::vector<std::tuple<int, int, T>>>(numVertices);

    const JsonElement& jRegions = json["regions"];

    int regionIndex = 0;
    for (auto&& [regionName, regionData] : jRegions.Map())
    {
        Eigen::VectorXi vertexIDs;
        Eigen::VectorX<T> weights;
        Eigen::Matrix<T, -1, -1> modes;
        io::FromJson(regionData["vertex_ids"], vertexIDs);
        io::FromJson(regionData["weights"], weights);
        io::FromJson(regionData["modes"], modes);
        // const int numRegionModes = int(modes.cols());
        const int numRegionVertices = int(modes.rows() / 3);
        if (int(vertexIDs.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("vertex_ids and modes matrix for region {} do not match", regionName);
        }
        if (int(weights.size()) != numRegionVertices)
        {
            CARBON_CRITICAL("weights and modes matrix for region {} do not match", regionName);
        }

        Eigen::Matrix<T, 3, -1> regionMeanVertices = mean(Eigen::all, vertexIDs);
        Eigen::Vector3<T> centerOfGravityOfRegion = regionMeanVertices.rowwise().mean();
        regionMeanVertices.colwise() -= centerOfGravityOfRegion;
        m_centerOfGravityPerRegion.push_back(centerOfGravityOfRegion);
        m_regionModels.push_back(rt::LinearVertexModel<T>(regionMeanVertices, modes));
        m_patchNames.push_back(regionName);

        for (int j = 0; j < numRegionVertices; ++j)
        {
            const int vID = vertexIDs[j];
            m_globalBlendMatrix[vID].push_back({ regionIndex, j, weights[j] });
        }
        regionIndex++;
    }

    UpdateBaseVertices();
}


template <class T>
int PatchBlendModel<T>::NumPatches() const
{
    return (int)m_regionModels.size();
}

template <class T>
int PatchBlendModel<T>::NumVerticesForPatch(int id) const
{
    return m_regionModels[id].NumVertices();
}

template <class T>
int PatchBlendModel<T>::NumVertices() const
{
    return (int)m_globalBlendMatrix.size();
}


template <class T>
Eigen::Vector3<T> PatchBlendModel<T>::PatchCenterOfGravity(int id) const
{
    return m_centerOfGravityPerRegion[id];
}

template <class T>
const Eigen::Matrix<T, 3, -1>& PatchBlendModel<T>::BaseVertices() const
{
    return m_baseVertices;
}

template <class T>
void PatchBlendModel<T>::UpdateBaseVertices()
{
    m_baseVertices.resize(3, NumVertices());
    for (int vID = 0; vID < NumVertices(); ++vID)
    {
        Eigen::Vector3<T> v = Eigen::Vector3<T>::Zero();
        for (const auto& [regionIndex, regionVID, weight] : m_globalBlendMatrix[vID])
        {
            v += (m_regionModels[regionIndex].Base().col(regionVID) + m_centerOfGravityPerRegion[regionIndex]) * weight;
        }
        m_baseVertices.col(vID) = v;
    }
}

template <class T>
const std::vector<std::vector<std::tuple<int, int, T>>>& PatchBlendModel<T>::BlendMatrix() const
{
    return m_globalBlendMatrix;
}

template <class T>
const std::vector<rt::LinearVertexModel<T>>& PatchBlendModel<T>::PatchModels() const
{
    return m_regionModels;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
template <class T>
std::pair<Eigen::Quaternion<T>, T> PatchBlendModel<T>::EstimateRotationAndScale(int vID, const PatchBlendModel<T>::State& state) const
{
    std::vector<Eigen::Quaternion<T>> qs;
    std::vector<T> weights;
    T scale = 0;
    for (size_t i = 0; i < m_globalBlendMatrix[vID].size(); ++i)
    {
        const auto& [regionIndex, _, w] = m_globalBlendMatrix[vID][i];
        qs.push_back(state.PatchRotation(regionIndex));
        weights.push_back(w);
        scale += state.PatchScale(regionIndex) * w;
    }
    return { WeightedQuaternionAverage(qs, weights), scale };

}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <class T>
Eigen::Matrix<T, 3, -1> PatchBlendModel<T>::DeformedVertices(const State& state, const Eigen::VectorX<T>& vertexDeltaScales) const
{
    std::vector<Eigen::Matrix<T, 3, -1>> patchVertices(NumPatches());
    auto evaluatePatchVertices = [&](int start, int end) {
        for (int k = start; k < end; ++k)
        {
            // base + modes * params
            patchVertices[k] = m_regionModels[k].EvaluateLinearized(state.PatchPcaWeights(k), rt::LinearVertexModel<T>::EvaluationMode::STATIC, m_threadPool.get());

            if (k < (int)state.regionVertexDeltas.size() && state.regionVertexDeltas[k].cols() == patchVertices[k].cols())
            {
                T vertexDeltaScale = (k < (int)vertexDeltaScales.size()) ? vertexDeltaScales[k] : T(1);
                patchVertices[k] += vertexDeltaScale * state.regionVertexDeltas[k];
            }

            const Eigen::Matrix<T, 3, 3> R = state.PatchRotation(k).toRotationMatrix();
            const Eigen::Vector3<T> t = state.PatchTranslation(k);

            patchVertices[k] = (state.PatchScale(k) * R) * patchVertices[k];
            patchVertices[k].colwise() += t;
        }
    };
    if (m_threadPool) m_threadPool->AddTaskRangeAndWait((int)patchVertices.size(), evaluatePatchVertices);
    else evaluatePatchVertices(0, (int)patchVertices.size());

    Eigen::Matrix<T, 3, -1> output = Eigen::Matrix<T, 3, -1>(3, NumVertices());
    for (int vID = 0; vID < NumVertices(); ++vID)
    {
        Eigen::Vector3<T> v = Eigen::Vector3<T>::Zero();
        for (const auto& [regionIndex, regionVID, weight] : m_globalBlendMatrix[vID])
        {
            v += patchVertices[regionIndex].col(regionVID) * weight;
        }
        output.col(vID) = v;
    }

    return output;
}

template <class T>
Eigen::Matrix<T, 3, -1> PatchBlendModel<T>::DeformedVertices(OptimizationState& state, bool updateModes) const
{
    UpdateRegionModels(state, updateModes);

    Eigen::Matrix<T, 3, -1> output = Eigen::Matrix<T, 3, -1>(3, NumVertices());
    for (int vID = 0; vID < NumVertices(); ++vID)
    {
        Eigen::Vector3<T> v = Eigen::Vector3<T>::Zero();
        for (const auto& [regionIndex, regionVID, weight] : m_globalBlendMatrix[vID])
        {
            v += state.transformedRegionModels[regionIndex].Base().col(regionVID) * weight;
        }
        output.col(vID) = v;
    }

    return output;
}

template <class T>
void PatchBlendModel<T>::UpdateRegionModels(OptimizationState& state, bool withModes) const
{
    auto updateRegionModel = [&](int start, int end) {
        for (int k = start; k < end; ++k)
        {
            // base + modes * params
            const int offset = state.regionVariableOffsets[k];
            const int numModes = m_regionModels[k].NumPCAModes();
            const int numRegionVertices = m_regionModels[k].NumVertices();

            m_regionModels[k].EvaluateLinearized(state.allVariables.Value().segment(offset, numModes),
                                                    rt::LinearVertexModel<T>::EvaluationMode::STATIC,
                                                    state.transformedRegionModels[k]);

            // rotated = (scale + dscale) * (dR * R) * (base + modes * params) + (T + dt)

            const int rigidOffset = offset + numModes;

            const Eigen::Vector3<T> dR = state.allVariables.Value().segment(rigidOffset + 0, 3);
            const Eigen::Vector3<T> dt = state.allVariables.Value().segment(rigidOffset + 3, 3);
            const T scale = state.allVariables.Value()[rigidOffset + 6];

            const Eigen::Matrix<T, 3,
                                3> R =
                (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * state.regionRotations[k]).normalized().toRotationMatrix();
            const Eigen::Vector3<T> t = dt;

            // R * (base + modes * params)
            state.transformedRegionModels[k].MutableBase() = (R * state.transformedRegionModels[k].Base()).eval();

            if (withModes)
            {
                if (state.OptimizeScale())
                {
                    // scale mode is the rotated vertices
                    state.transformedRegionModels[k].SetScaleMode(state.transformedRegionModels[k].Base());
                }
                else
                {
                    // no scale, so set mode to zero
                    state.transformedRegionModels[k].MutableModes().col(numModes + 6).setZero();
                }
            }

            // scale * R * (base + modes * params)
            state.transformedRegionModels[k].MutableBase() *= scale;

            if (withModes)
            {
                if (k == int(state.fixedRegion))
                {
                    state.transformedRegionModels[k].MutableModes().block(0, numModes, numRegionVertices * 3, 3).setZero();
                }
                else
                {
                    // rotation mode depends on rotated and scaled vertices (no translation)
                    state.transformedRegionModels[k].SetRotationModes(state.transformedRegionModels[k].Base());
                }
            }

            // add translation: R * scale * (base + modes * params) + T
            state.transformedRegionModels[k].MutableBase().colwise() += t;

            if (withModes)
            {
                // translation mode is always identity (i.e. no change) besides the first region which is fixed.
                if (k == int(state.fixedRegion))
                {
                    state.transformedRegionModels[k].MutableModes().block(0, numModes + 3, numRegionVertices * 3, 3).setZero();
                }
                else
                {
                    state.transformedRegionModels[k].SetTranslationModes();
                }
            }

            if (withModes)
            {
                // scale and rotate the modes
                for (int j = 0; j < m_regionModels[k].NumPCAModes(); ++j)
                {
                    Eigen::VectorX<T> mode =
                        m_regionModels[k].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC).col(j);
                    Eigen::Matrix<T, 3, -1> scaledRotatedMode = (scale * R) * Eigen::Map<const Eigen::Matrix<T, 3, -1>>(
                        mode.data(),
                        3,
                        mode.size() / 3);
                    state.transformedRegionModels[k].MutableModes().col(j).noalias() = scaledRotatedMode.reshaped();
                }
            }
        }
    };
    if (m_threadPool) m_threadPool->AddTaskRangeAndWait((int)m_regionModels.size(), updateRegionModel);
    else updateRegionModel(0, (int)m_regionModels.size());
}

template <class T>
std::shared_ptr<PatchBlendModel<T>> PatchBlendModel<T>::Reduce(const std::vector<int>& vertexIds) const
{
    std::shared_ptr<PatchBlendModel<T>> out = std::make_shared<PatchBlendModel<T>>();
    out->m_threadPool = m_threadPool;
    out->m_patchNames = m_patchNames;
    out->m_centerOfGravityPerRegion = m_centerOfGravityPerRegion;
    out->m_regionModels = m_regionModels;

    out->m_globalBlendMatrix.resize((int)vertexIds.size());
    out->m_baseVertices.resize(3, (int)vertexIds.size());
    std::vector<std::vector<int>> regionVertexIds(NumPatches());
    for (int i = 0; i < (int)vertexIds.size(); ++i)
    {
        out->m_globalBlendMatrix[i].clear();
        const int vID = vertexIds[i];
        out->m_baseVertices.col(i) = m_baseVertices.col(vID);
        for (const auto& [regionIndex, region_vID, weight] : m_globalBlendMatrix[vID])
        {
            const int newRegion_vID = (int)regionVertexIds[regionIndex].size();
            out->m_globalBlendMatrix[i].push_back({regionIndex, newRegion_vID, weight});
            regionVertexIds[regionIndex].push_back(region_vID);
        }
    }

    for (int regionIndex = 0; regionIndex < NumPatches(); ++regionIndex)
    {
        out->m_regionModels[regionIndex].Resample(regionVertexIds[regionIndex]);
    }

    return out;
}

// explicitly instantiate the PatchBlendModel classes
template class PatchBlendModel<float>;
template class PatchBlendModel<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
