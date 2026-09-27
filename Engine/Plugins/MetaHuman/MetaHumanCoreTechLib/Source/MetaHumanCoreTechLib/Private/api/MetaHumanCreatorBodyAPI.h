// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <arrayview/ArrayView.h>
#include <arrayview/StringView.h>
#include <nls/math/Math.h>

#include <memory>
#include <string>
#include <vector>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

namespace trio
{

class BoundedIOStream;

} // namespace trio

namespace TITAN_API_NAMESPACE
{

class MetaHumanCreatorBodyAPI : public std::enable_shared_from_this<MetaHumanCreatorBodyAPI>
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
    ~MetaHumanCreatorBodyAPI();
    MetaHumanCreatorBodyAPI(MetaHumanCreatorBodyAPI&&) = delete;
    MetaHumanCreatorBodyAPI(const MetaHumanCreatorBodyAPI&) = delete;
    MetaHumanCreatorBodyAPI& operator=(MetaHumanCreatorBodyAPI&&) = delete;
    MetaHumanCreatorBodyAPI& operator=(const MetaHumanCreatorBodyAPI&) = delete;

    static std::shared_ptr<MetaHumanCreatorBodyAPI> TITAN_API CreateMHCBodyApi(const dna::Reader* PCABodyModel,
                                                                               dna::Reader* InCombinedBodyArchetypeDnaReader,
                                                                               const std::string& CombinedSkinningWeightGenerationConfigPath,
                                                                               const std::string& CombinedLodGenerationConfigPath = {},
                                                                               const std::string& PhysicsBodiesConfigPath = {},
                                                                               const std::string& BodyMasksPath = {},
                                                                               const std::string& RegionLandmarksPath = {},
                                                                               int numThreads = -1);

    void TITAN_API SetNumThreads(int numThreads);
    int TITAN_API GetNumThreads() const;

    TITAN_API std::shared_ptr<State> CreateState() const;

    TITAN_API bool GetVertex(int lod, const float* InVertices, int DNAVertexIndex, float OutVertexXYZ[3]) const;

    TITAN_API void Evaluate(State& State) const;
    TITAN_API void EvaluateConstraintRange(const State& state, av::ArrayView<float> MinValues, av::ArrayView<float> MaxValues) const;
    TITAN_API void StateToDna(const State& State, dna::Writer* InOutDnaWriter, bool combinedBodyAndFace = false) const;

    TITAN_API void DumpState(const State& State, trio::BoundedIOStream* Stream) const;
    TITAN_API bool RestoreState(trio::BoundedIOStream* Stream, std::shared_ptr<State> OutState) const;

    //! Add Legacy body - dna needs to contine the combined body/face model
    TITAN_API void AddLegacyBody(const dna::Reader* LegacyBody, const av::StringView& LegacyBodyName);

    //! @returns the number of legacy bodies
    TITAN_API int NumLegacyBodies() const;

    //! @returns the name of legacy body @p LegacyBodyIndex
    TITAN_API const std::string& LegacyBodyName(int LegacyBodyIndex) const;

    //! Update @p State using legacy body at index @p LegacyBodyIndex
    TITAN_API void SelectLegacyBody(State& State, int LegacyBodyIndex, bool Fit = false) const;

    //! @returns the number of LODs supported by the API
    TITAN_API int NumLODs() const;

    //! @returns the number of preset bodies
    TITAN_API int NumPresetBodies() const;

    //! @returns all preset names
    TITAN_API const std::vector<std::string>& GetPresetNames() const;

    //! @returns the name of preset body @p PresetBodyIndex
    TITAN_API const std::string& PresetBodyName(int PresetBodyIndex) const;

    //! Update @p State using preset body at index @p PresetBodyIndex
    TITAN_API void SelectPresetBody(State& State, int PresetBodyIndex) const;

    //! Calculates the combined body vertex influence weights for the supplied body state at each lod; the body must have a pca skinning model
    TITAN_API void GetVertexInfluenceWeights(const State& State, std::vector<TITAN_NAMESPACE::SparseMatrix<float>>& vertexInfluenceWeights) const;

    //! @returns the numbers of physics body volumes
    TITAN_API int NumPhysicsBodyVolumes(const::std::string& JointName) const;

    //! Calculates the physics bounding box for the joint and volume index
    TITAN_API bool GetPhysicsBodyBoundingBox(const State& State, const::std::string& JointName, int BodyVolumeIndex, Eigen::Vector3f& OutCenter, Eigen::Vector3f& OutExtents) const;

	TITAN_API int NumJoints() const;
	TITAN_API void GetNeutralJointTransform(const State& State, std::uint16_t JointIndex, Eigen::Vector3f& OutJointTranslation, Eigen::Vector3f& OutJointRotation) const;

    //! @returns all number of gizmos used for region blending
    TITAN_API int NumGizmos() const;

    //! Gets the positions of the gizmos used for region blending
    TITAN_API bool EvaluateGizmos(const State& State, float* OutGizmos) const;

    //! @returns all region names
    TITAN_API const std::vector<std::string>& GetRegionNames() const;

    /**
     * Blend region @p RegionIndex (all regions if -1) to @p States.
     * @p Type defines whether skeleton, shaping, or both are blended.
     */
    TITAN_API bool Blend(State& state, int RegionIndex, const std::vector<std::pair<float, const State*>>& States, BodyAttribute Type) const;

    TITAN_API bool SelectPreset(State& state, int RegionIndex, const std::string& PresetName, BodyAttribute Type) const;

    TITAN_API bool BlendPresets(State& state, int RegionIndex, const std::vector<std::pair<float, std::string>>& alphaAndPresetNames, BodyAttribute Type) const;

    TITAN_API bool SetVertexDeltaScale(State& state, float VertexDeltaScale) const;

    struct FitToTargetOptions {
        //! whether to keep the skeleton fixed
        bool fitSkeleton{false};
        //! whether to fit the scale
        bool fitRigidAndScale{false};
        //! option to snap result to floor plane
        bool snapToFloor{false};
    };

    TITAN_API bool FitToTarget(State& state,
                               const FitToTargetOptions& options,
                               const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices,
                               const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints) const;

    TITAN_API bool FitToTarget(State& state,
                               const FitToTargetOptions& options,
                               bool fitFromJoints,
                               const dna::Reader* InDnaReader) const;

    //! Calculate measurements on the combined body vertices
    TITAN_API bool GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> combinedBodyAndFaceVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const;

    //! Calculate measurements on the body and face vertices
    TITAN_API bool GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceVertices, Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const;

private:
    MetaHumanCreatorBodyAPI();

    struct Private;
    Private* m{};
};

class TITAN_API MetaHumanCreatorBodyAPI::State
{
public:
    ~State();
    State(State&&) = delete;
    State& operator=(State&&) = delete;
    State& operator=(const State&) = delete;

    std::shared_ptr<State> Clone() const;

    bool Reset();

    av::ConstArrayView<float> GetMesh(int lod) const;
    av::ConstArrayView<float> GetMeshNormals(int lod) const;
    av::ConstArrayView<float> GetBindPose() const;
    av::ConstArrayView<float> GetMeasurements() const;
    av::ConstArrayView<float> GetPose() const;

    Eigen::Matrix3Xf GetContourVertices(int ConstraintIndex) const;
    Eigen::Matrix3Xf GetContourDebugVertices(int ConstraintIndex) const;

    int GetConstraintNum() const;
    const std::string& GetConstraintName(int ConstraintIndex) const;
    bool GetConstraintTarget(int ConstraintIndex, float& OutTarget) const;
    bool SetConstraintTarget(int ConstraintIndex, float Target);
    bool RemoveConstraintTarget(int ConstraintIndex);

    float VertexDeltaScale() const;

    bool ShowLegacy() const;
    void SetShowLegacy(bool bInShowLegacy);

private:
    State();
    State(const State&);

    struct Private;
    Private* m{};

private:
    friend class MetaHumanCreatorBodyAPI;
};

} // namespace TITAN_API_NAMESPACE
