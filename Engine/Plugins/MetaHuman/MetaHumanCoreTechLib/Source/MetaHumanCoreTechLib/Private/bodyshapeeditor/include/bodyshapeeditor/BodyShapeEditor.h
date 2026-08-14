// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/utils/TaskThreadPool.h>
#include <bodyshapeeditor/BodyMeasurement.h>
#include <trio/Stream.h>
#include <carbon/common/Defs.h>
#include <arrayview/ArrayView.h>
#include <arrayview/StringView.h>
#include <rig/BodyLogic.h>
#include <rig/BodyGeometry.h>
#include <nls/geometry/Affine.h>
#include <rig/RigLogic.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/LodGeneration.h>
#include <nrr/VertexWeights.h>
#include <dna/Reader.h>
#include <dna/Writer.h>

#include <memory>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class BodyShapeEditor
{
public:
    class State;

    enum class BodyAttribute
    {
        Skeleton,
        Shape,
        Both
    };

public:
    ~BodyShapeEditor();
    BodyShapeEditor();

    void SetThreadPool(const std::shared_ptr<TaskThreadPool>& threadPool);

    void Init(const dna::Reader* reader,
              dna::Reader* InCombinedBodyArchetypeDnaReader,
              const std::vector<std::map<std::string, std::map<std::string, float>>>& JointSkinningWeightLodPropagationMap,
              const std::vector<int>& maxSkinWeightsPerVertexForEachLod = { 12, 8, 8, 4},
              std::shared_ptr<const LodGeneration<float>> combinedLodGenerationData = nullptr);
    void Init(std::shared_ptr<BodyLogic<float>> BodyLogic,
              std::shared_ptr<BodyGeometry<float>> CombinedBodyArchetypeGeometry,
              std::shared_ptr<RigLogic<float>> CombinedBodyRigLogic,
              std::shared_ptr<BodyGeometry<float>> BodyGeometry,
              av::ConstArrayView<BodyMeasurement> contours,
              const std::vector<std::map<std::string, std::map<std::string, float>>>& JointSkinningWeightLodPropagationMap,
              const std::vector<int>& maxSkinWeightsPerVertexForEachLod = { 12, 8, 8, 4 },
              std::shared_ptr<const LodGeneration<float>> combinedLodGenerationData = nullptr,
              const std::map<std::string, std::pair<int, int>>& helperJointMap = {},
              const std::map<std::string, VertexWeights<float>>& partWeights = {});

    void SetFittingVertexIDs(const std::vector<int>& vertexIds);

    void SetNeckSeamVertexIDs(const std::vector<std::vector<int>>& vertexIds);

    void SetBodyToCombinedMapping(int lod, const std::vector<int>& bodyToCombinedMapping);

    const std::vector<int>& GetBodyToCombinedMapping(int lod = 0) const;

    int NumLODs() const;

    void EvaluateConstraintRange(const State& state, av::ArrayView<float> MinValues, av::ArrayView<float> MaxValues) const;

    std::shared_ptr<State> CreateState() const;

    void UpdateState(State& State, const Eigen::VectorXf& Pose) const;
    void UpdateState(State& State) const;

    //! Evaluate the state and update the meshes
    void EvaluateState(State& State, bool applyFloorOffset) const;

    //! Estimate Gui from Raw controls
    void UpdateGuiFromRawControls(State& state) const;

    std::shared_ptr<State> RestoreState(trio::BoundedIOStream* InputStream);
    void DumpState(const State& State, trio::BoundedIOStream* OutputStream);

    void Solve(State& State, float priorWeight = 1.0f, const int iterations = 2) const;

    struct FitToTargetOptions {
        float regularization{0.5};
        bool optimizeEdges{false};
        bool fitRigidAndScale{false};
        bool fitSkeleton{true};
        bool fitShape{true};
        bool snapToFloor{false};
    };

    //! Template2MH - @returns raw controls
    void SolveForTemplateMesh(Eigen::VectorXf& InOutResult,
                              float& scale,
                              Affine<float, 3, 3>& transform,
                              Eigen::Vector3<float>& modelTranslation,
                              const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices,
                              const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints,
                              const FitToTargetOptions& fitToTargetOptions,
                              const std::vector<int>& vertexMapping);

    //! Template2MH including - @returns guis controls
    Eigen::VectorXf SolveForTemplateMesh(av::ConstArrayView<int> indices, const Eigen::Matrix<float, 3, -1>& targets, float regularization, int iterations);

    void UpdateMeasurementPoints(State& State) const;

    void StateToDna(const State& State, dna::Writer* InOutDnaWriter, bool combinedBodyAndFace = false) const;

	int NumJoints() const;
	void GetNeutralJointTransform(const State& State, std::uint16_t JointIndex, Eigen::Vector3f& OutJointTranslation, Eigen::Vector3f& OutJointRotation) const;

    void SetCustomGeometryToState(State& state, std::shared_ptr<const BodyGeometry<float>> Geometry, bool Fit);

    void FitToTarget(State& state,
                     const FitToTargetOptions& options,
                     const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices,
                     const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints);

    //! calculate the skinning weights for the supplied body state at each lod; the body must have a skin weights pca present for this to work
    void GetVertexInfluenceWeights(const State& state, std::vector<SparseMatrix<float>>& vertexInfluenceWeights) const;

    //! get the maximum number of skin weights for each joint at each LOD of the combined body model (by default this is set to 12, 8, 8, 4)
    const std::vector<int>& GetMaxSkinWeights() const;
    //! set the maximum number of skin weights for each joint at each LOD of the combined body model
    void SetMaxSkinWeights(const std::vector<int>& MaxSkinWeights);

    int GetJointIndex(const std::string& JointName) const;

    //! @returns the region names
    const std::vector<std::string>& GetRegionNames() const;

    //! Blends the states
    bool Blend(State& state, int RegionIndex, const std::vector<std::pair<float, const State*>>& States, BodyAttribute Type);

    //! Calculate measurements on the combined body vertices
    bool GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> combinedBodyAndFaceVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const;

    //! Calculate measurements on the body and face vertices
    bool GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceVertices, Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const;

private:
    void UpdateHelperJoints(const Eigen::Matrix<float, 3, -1>& vertices, std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& bindMatrices) const;

private:
    struct Private;
    Private* m;
};


class BodyShapeEditor::State
{
private:
    State();

public:
    ~State();
    State(const State&);

    const Mesh<float>& GetMesh(int lod) const;
    const std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& GetJointBindMatrices() const;
    const Eigen::VectorX<float>& GetNamedConstraintMeasurements() const;
    const Eigen::VectorX<float>& GetPose() const;

    Eigen::Matrix3Xf GetContourVertices(int ConstraintIndex) const;
    Eigen::Matrix3Xf GetContourDebugVertices(int ConstraintIndex) const;

    void Reset();

    int GetConstraintNum() const;
    const std::string& GetConstraintName(int ConstraintIndex) const;

    bool GetConstraintTarget(int ConstraintIndex, float& OutTarget) const;
    void SetConstraintTarget(int ConstraintIndex, float Target);
    void RemoveConstraintTarget(int ConstraintIndex);

    float VertexDeltaScale() const;
    void SetVertexDeltaScale(float VertexDeltaScale);

    void SetSymmetry(const bool sym);
    bool GetSymmetric() const;
    void SetSemanticWeight(float weight);
    float GetSemanticWeight();

    bool GetApplyFloorOffset() const;

public:
    State(State&&) = delete;
    State& operator=(State&&) = delete;
    State& operator=(const State&) = delete;

private:
    friend BodyShapeEditor;
    struct Private;
    std::unique_ptr<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
