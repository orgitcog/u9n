// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <carbon/io/JsonIO.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nrr/deformation_models/DeformationModel.h>
#include <nrr/IdentityBlendModel.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/EulerAngles.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/GeometryHelpers.h>
#include <nrr/LinearVertexModel.h>
#include <dna/Reader.h>

#include <memory>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class PatchBlendModel
{
public:
    class State;
    class OptimizationState;

public:
    PatchBlendModel();
    PatchBlendModel(const std::shared_ptr<TaskThreadPool>& threadPool);
    ~PatchBlendModel();
    PatchBlendModel(PatchBlendModel&&);
    PatchBlendModel(const PatchBlendModel&) = delete;
    PatchBlendModel& operator=(PatchBlendModel&&);
    PatchBlendModel& operator=(const PatchBlendModel&) = delete;

    std::shared_ptr<PatchBlendModel> Reduce(const std::vector<int>& vertexIds) const;

    std::pair<DiffDataMatrix<T, 3, -1>, Cost<T>> EvaluateVerticesAndConstraints(Context<T>* context, OptimizationState& state, T modelRegularization, T patchSmoothness) const;

    //! Load the patch model from filename or data as Json string
    void LoadModel(const std::string& identityModelFileOrData);

    //! Load the patch model from binary file (by first loading an IdentityBlendModel)
    void LoadModelBinary(const std::string& filename);

    //! Load the patch model from identity model
    void LoadFromIdentityModel(const std::shared_ptr<const IdentityBlendModel<T>>& identityModel);

    //! Load the patch model from identity model
    void LoadFromIdentityModel(const std::shared_ptr<const IdentityBlendModel<T, -1>>& identityModel);

    //! Get patch center of gravity for specified patch id
    Eigen::Vector3<T> PatchCenterOfGravity(int id) const;

    //! @returns the number of vertices
    int NumVertices() const;

    //! @returns the number of patches
    int NumPatches() const;

    //! Get number of modes for patch at specified patch id
    int NumPcaModesForPatch(int id) const;

    //! Get number of vertices for patch
    int NumVerticesForPatch(int id) const;

    //! Get the patch names
    const std::vector<std::string>& PatchNames() const { return m_patchNames; }

    //! Get the name for patch @p id
    const std::string& PatchName(int id) const;

    //! @returns the deformed vertex positions
    Eigen::Matrix<T, 3, -1> DeformedVertices(const State& state, const Eigen::VectorX<T>& vertexDeltaScale = Eigen::VectorX<T>(0)) const;

    //! @returns the deformed vertex positions
    Eigen::Matrix<T, 3, -1> DeformedVertices(OptimizationState& state, bool updateModes = false) const;

    //! Create a new state
    State CreateState() const;

    //! Create a new optimization state
    OptimizationState CreateOptimizationState() const;

    //! @return the blend matrix
    const std::vector<std::vector<std::tuple<int, int, T>>>& BlendMatrix() const;

    //! @returns the base vertex positions
    const Eigen::Matrix<T, 3, -1>& BaseVertices() const;

    //! get the base region models
    const std::vector<rt::LinearVertexModel<T>>& PatchModels() const;

    //! estimate the approximation rotation and scale for vertex @p vID based on state @p state (estimates the transform as weighted sum)
    std::pair<Eigen::Quaternion<T>, T> EstimateRotationAndScale(int vertex, const State& state) const;

private:
    void UpdateBaseVertices();
    void UpdateRegionModels(OptimizationState& state, bool withModes) const;

    DiffDataMatrix<T, 3, -1> EvaluateVertices(OptimizationState& state, const DiffData<T>& values) const;
    DiffData<T> EvaluateRegularization(OptimizationState& state, const DiffData<T>& values) const;
    DiffData<T> EvaluatePatchSmoothness(OptimizationState& state, const DiffData<T>& values) const;

private:
    //! the base vertices
    Eigen::Matrix<T, 3, -1> m_baseVertices;

    //! the linear model per region: dR * (1 + dscale) * (base + modes * params) + dt
    std::vector<rt::LinearVertexModel<T>> m_regionModels;

    //! gravity center of each region
    std::vector<Eigen::Vector3<T>> m_centerOfGravityPerRegion;

    //! names of regions
    std::vector<std::string> m_patchNames;

    //! for each vertex, this points to all regions with region index, vertex within region, and weight.
    std::vector<std::vector<std::tuple<int, int, T>>> m_globalBlendMatrix;

    //! thread pool for parallelization
    std::shared_ptr<TaskThreadPool> m_threadPool;
};


template <class T>
class PatchBlendModel<T>::State
{
public:
    State() = default;
    State(int numPatches);
    ~State() = default;
    State(const State&) = default;
    State(State&&) = default;
    State& operator=(const State&) = default;
    State& operator=(State&&) = default;

    void Reset(const PatchBlendModel<T>& patchBlendModel);
    int NumPatches() const { return (int)regionScales.size(); }

    void CopyTransforms(const State& other);
    void CopyPcaWeights(const State& other);

    void SetPatchScale(int id, T scale) { regionScales[id] = scale; }
    T PatchScale(int id) const { return regionScales[id]; }
    const std::vector<T>& PatchScales() const { return regionScales; }

    void SetPatchTranslation(int id, const Eigen::Vector3<T>& t) { regionTranslations[id] = t; }
    Eigen::Vector3<T> PatchTranslation(int id) const { return regionTranslations[id]; }

    void SetPatchRotation(int id, const Eigen::Quaternion<T>& q) { regionRotations[id] = q; }
    Eigen::Quaternion<T> PatchRotation(int id) const { return regionRotations[id]; }

    void SetPatchRotationEulerDegrees(int id, const Eigen::Vector<T, 3>& eulerXYZ) { regionRotations[id] = Eigen::Quaternion<T>(EulerXYZ<T>(eulerXYZ * degree2radScale<T>())).normalized(); }
    Eigen::Vector<T, 3> PatchRotationEulerDegrees(int id) const { return RotationMatrixToEulerXYZ<T>(regionRotations[id].toRotationMatrix()) * rad2degreeScale<T>(); }

    void SetPatchPcaWeights(int id, const Eigen::VectorX<T>& w) { regionPcaWeights[id] = w; }
    const Eigen::VectorX<T>& PatchPcaWeights(int id) const { return regionPcaWeights[id]; }

    void SetPatchVertexDeltas(int id, const Eigen::Matrix<T, 3, -1>& vertexDeltas) { regionVertexDeltas[id] = vertexDeltas; }
    const Eigen::Matrix<T, 3, -1>& PatchVertexDeltas(int id) const { return regionVertexDeltas[id]; }
    void ResetPatchVertexDeltas();
    void ResetPatchVertexDeltas(int id);
    bool HasPatchVertexDeltas() const;
    bool HasPatchVertexDeltas(int id) const;
    //! bake the global vertex deltas to each patch
    void BakeVertexDeltas(const Eigen::Matrix<T, 3, -1>& vertexDeltas, const PatchBlendModel<T>& patchBlendModel);
    //! evaluate the global vertex deltas
    Eigen::Matrix<T, 3, -1> EvaluateVertexDeltas(const PatchBlendModel<T>& patchBlendModel) const;


    //! symmetrize all regions as defined by @p patchSymmetries.
    void SymmetrizeRegions(const std::vector<int>& patchSymmetries, bool symmetrizeTransform, bool symmetrizePca, const PatchBlendModel<T>& patchBlendModel, const std::vector<int>& vertexSymmetries);

    //! copy rotation/translation/scale (and optionally pca weights) from @p regionId to its symmetric counterpart as defined by @p patchSymmetries
    void SymmetricRegionCopy(const std::vector<int>& patchSymmetries, int regionId, bool includingPcaWeights);

    //! concatenate all pca weights of all patches into vector.
    Eigen::VectorX<T> ConcatenatePatchPcaWeights() const;

    //! set the concatenated pca weights.
    void SetConcatenatedPatchPcaWeights(const Eigen::VectorX<T>& w);

    //! serialize to json
    JsonElement ToJson() const;

    //! deserialize from json
    void FromJson(const JsonElement& json);

    //! number of regions, regionsScale, regionRotations, regionTranslation, numberOfModesPerRegion, regionPcaWeights
    Eigen::VectorX<T> SerializeToVector() const;

    void DeserializeFromVector(const Eigen::VectorX<T>& vector);

private:
    std::vector<T> regionScales;
    std::vector<Eigen::Quaternion<T>> regionRotations;
    std::vector<Eigen::Vector3<T>> regionTranslations;
    std::vector<Eigen::VectorX<T>> regionPcaWeights;
    std::vector<Eigen::Matrix3X<T>> regionVertexDeltas;

private:
    friend class PatchBlendModel<T>;
};

template <class T>
class PatchBlendModel<T>::OptimizationState
{
public:
    OptimizationState() = default;
    ~OptimizationState() = default;
    OptimizationState(OptimizationState&&) = default;
    OptimizationState& operator=(OptimizationState&&) = default;

    void CopyFromState(const PatchBlendModel<T>::State& state);
    void CopyToState(PatchBlendModel<T>::State& state) const;

    PatchBlendModel<T>::State CreateState() const;

    int NumParameters() const;
    void ResetParameters(const PatchBlendModel<T>& patchBlendModel);

    int NumPatches() const;

    //! Set current model parameters
    void SetModelParameters(const Eigen::VectorX<T>& parameters);

    //! Get patch scale for specified patch id
    T PatchScale(int id) const;

    //! Get patch translation for specified patch id
    Eigen::Vector3<T> PatchTranslation(int id) const;

    //! Get patch rotation for specified patch id
    Eigen::Vector3<T> PatchRotation(int id) const;

    //! Get patch pca weights for specified patch id
    Eigen::VectorX<T> PatchPcaWeights(int id) const;

    const Eigen::VectorX<T>& GetModelParameters() const;

    //! Set patch scale for specified patch id
    void SetPatchScale(int id, T scale);

    //! Set patch translation for specified patch id
    void SetPatchTranslation(int id, const Eigen::Vector3<T>& translation);

    //! Set patch center of gravity for specified patch id
    void SetPatchRotation(int id, const Eigen::Vector3<T>& rotation);

    //! Get number of modes for patch at specified patch id
    int NumPcaModesForPatch(int id) const;

    //! Set patch pca weights for specified patch id
    void SetPatchPcaWeights(int id, const Eigen::VectorX<T>& weights);

    void ClearFixedPatch();

    int FixedPatch() const;

    void SetFixedPatch(int patchId);

    bool OptimizeScale() const { return optimizeScale; }
    void SetOptimizeScale(bool _optimizeScale) { optimizeScale = _optimizeScale; }

    void MakeRotationConstantForPatch(int patchId);

    void MakeTranslationConstantForPatch(int patchId);

    void MakeScaleConstantForPatch(int patchId);

    void RemoveConstraintsOnVariables();

    //! Transform all patches
    void TransformPatches(const Affine<T, 3, 3>& aff);

    //! Transform patch
    void TransformPatch(const Affine<T, 3, 3>& aff, int patchId);

    //! Bake the rotation linearization
    void BakeRotationLinearization();

    const std::vector<rt::LinearVertexModel<T>>& TransformedRegionModels() const { return transformedRegionModels; }

    int PatchVariableOffset(int patchId) const { return regionVariableOffsets[patchId]; }

private:
    //! variable containing all variables: coefficients for region PCA models, rotation, translation, and scale
    VectorVariable<T> allVariables = VectorVariable<T>(0);

    //! which indices are constant
    std::set<int> constantIndices;

    //! the offset for each region into allVariables
    std::vector<int> regionVariableOffsets;

    //! the rotation per region
    std::vector<Eigen::Quaternion<T>> regionRotations;

    /**
     * The linear model after rotation and scale have been applied.
     * rotated = (dR * R) * (scale + dscale) * (base + modes * params) + (T + dt)
     */
    std::vector<rt::LinearVertexModel<T>> transformedRegionModels;

    //! cache size of sparse matrices
    int numNonZerosVertexJacobian = 0;
    int numNonZerosSmoothnessJacobian = 0;

    //! which region should stay "fixed"
    int fixedRegion = -1;

    //! whether to optimize scale
    bool optimizeScale = false;

private:
    friend class PatchBlendModel<T>;
};

//! Class made to extract joint deltas and mesh vertices from the patch blend model output.
template<class T>
class PatchBlendModelDataManipulator
{

public:

    PatchBlendModelDataManipulator(const dna::Reader* dnaReader)
    {
        // get the mesh starting indices

        // joint deltas are packed before the meshes
        m_startingIndices.push_back((int)0);
        int startingIndex = (int)dnaReader->getJointCount();

        for (int i = 0; i < (int)dnaReader->getMeshCount(); ++i)
        {
            m_meshNameToIdxInRigGeometry[std::string(dnaReader->getMeshName((uint16_t)i))] = i;
            m_startingIndices.push_back(startingIndex);
            startingIndex += (int)dnaReader->getVertexPositionCount((uint16_t)i);
        }

        m_startingIndices.push_back(startingIndex);
    }

    //! @return the number of meshes for the patch blend model
    int GetNumMeshes() const { return (int)m_startingIndices.size() - 2; }

    //! Get the joint detlas
    Eigen::Matrix3X<T> GetJointDeltas(const Eigen::Matrix3X<T> &patchModelData) const
    {
        return patchModelData(Eigen::all, Eigen::seqN(m_startingIndices[0], m_startingIndices[1] - m_startingIndices[0]));
    }

    //! Get the mesh vertices for mesh @p meshName
    Eigen::Matrix3X<T> GetMeshVertices(const Eigen::Matrix3X<T> &patchModelData, int meshIndex) const
    {
        const int idx = meshIndex + 1;
        return patchModelData(Eigen::all, Eigen::seqN(m_startingIndices[idx], m_startingIndices[idx + 1] - m_startingIndices[idx]));
    }

    //! Get the mesh vertices for mesh @p meshName
    Eigen::Matrix3X<T> GetMeshVertices(const Eigen::Matrix3X<T> &patchModelData, const std::string& meshName) const
    {
        return GetMeshVertices(patchModelData, m_meshNameToIdxInRigGeometry[meshName]);
    }

    std::pair<int, int> GetRangeForMeshIndex(int meshIndex) const
    {
        const int idx = meshIndex + 1;
        return {m_startingIndices[idx], m_startingIndices[idx + 1]};
    }

    std::pair<int, int> GetRangeForMesh(const std::string& meshName) const
    {
        return GetRangeForMeshIndex(m_meshNameToIdxInRigGeometry[meshName]);
    }

    const std::map<std::string, int>& GetMeshNameToIdxMapping() const
    {
        return m_meshNameToIdxInRigGeometry;
    }

    bool HasMesh(const std::string& meshName) const { return m_meshNameToIdxInRigGeometry.find(meshName) != m_meshNameToIdxInRigGeometry.end(); }

    //! @returns the number of joints + vertices
    int Size() const { return m_startingIndices.back(); }

    //! @returns the number of joints
    int NumJoints() const { return m_startingIndices[1]; }

private:
    std::vector<int> m_startingIndices; //!< Indices that define the start vertex id for each data asset
    mutable std::map<std::string, int> m_meshNameToIdxInRigGeometry; //!< Mapping of mesh names and their indices in rig geometry
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
