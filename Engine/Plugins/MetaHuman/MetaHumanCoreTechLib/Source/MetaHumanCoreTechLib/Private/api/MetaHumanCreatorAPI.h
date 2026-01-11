// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <carbon/Math.h>

#include <map>
#include <memory>
#include <stdint.h>
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

enum class AlignmentOptions {
	None,
	Translation,
	RotationTranslation,
	ScalingTranslation,
	ScalingRotationTranslation
};

class MetaHumanCreatorAPI : public std::enable_shared_from_this<MetaHumanCreatorAPI>
{

public:
	class State;
	class Settings;

public:
	~MetaHumanCreatorAPI();
	MetaHumanCreatorAPI(MetaHumanCreatorAPI&&) = delete;
	MetaHumanCreatorAPI(const MetaHumanCreatorAPI&) = delete;
	MetaHumanCreatorAPI& operator=(MetaHumanCreatorAPI&&) = delete;
	MetaHumanCreatorAPI& operator=(const MetaHumanCreatorAPI&) = delete;

	static std::shared_ptr<MetaHumanCreatorAPI> TITAN_API CreateMHCApi(
		dna::Reader* InDnaReader,
		const char* InMhcDataPath,
		int numThreads = -1,
		dna::Reader* InBodyDnaReader = nullptr);

	void TITAN_API SetNumThreads(int numThreads);
	int TITAN_API GetNumThreads() const;

	std::shared_ptr<State> TITAN_API CreateState() const;

	const std::vector<int> TITAN_API& GetVertexSymmetries() const;

	const std::vector<std::string> TITAN_API& GetExpressionNames() const;

	//! @returns all region names
	TITAN_API const std::vector<std::string>& GetRegionNames() const;

	int TITAN_API NumVertices() const;

	bool TITAN_API Evaluate(const State& State, float* OutVertices) const;

	//! Evaluates the state and sets all joints and vertices that are defined by the dna
	bool TITAN_API Evaluate(const State& State, Eigen::Matrix<float, 3, -1>& OutVertices) const;

	//! Computes the normals for all meshes and stores it to @p OutNormals
	bool TITAN_API EvaluateNormals(const State& State, const Eigen::Matrix<float, 3, -1>& InVertices, Eigen::Matrix<float, 3, -1>& OutNormals, const std::vector<Eigen::Ref<const Eigen::Matrix<float, 3, -1>>>& InBodyNormals) const;

	bool TITAN_API GetVertex(const float* InVertices, int DNAMeshIndex, int DNAVertexIndex, float OutVertexXYZ[3]) const;

	//! Gets the mesh vertices @p OutVertices based on @p InVertices (as calculated by @see Evaluate())
	bool TITAN_API GetMeshVertices(const float* InVertices, int DNAMeshIndex, Eigen::Matrix<float, 3, -1>& OutVertices) const;

	//! Gets the joint positions @p OutBindPose based on @p InVertices (as calculated by @see Evaluate())
	bool TITAN_API GetBindPose(const float* InVertices, Eigen::Matrix<float, 3, -1>& OutBindPose) const;

	//! Gets the state parameters @p OutParameters based on @p State
	bool TITAN_API GetParameters(const State& State, Eigen::VectorXf& OutParameters) const;

	//! Gets the model identifier
	bool TITAN_API GetModelIdentifier(const State& State, std::string& OutIdentifier) const;

	//! @returns all available presets
	std::vector<std::string> TITAN_API GetPresetNames() const;

	//! adds state @p State as preset with name @p PresetName
	bool TITAN_API AddPreset(const std::string& PresetName, std::shared_ptr<const State>& State);

	//! remove preset @p PresetName from the list
	bool TITAN_API RemovePreset(const std::string& PresetName);

	int TITAN_API NumHFVariants() const;

	//! @return types of variants e.g. eyelashes or teeth
	std::vector<std::string> TITAN_API GetVariantTypes() const;

	//! @return get name of variants for a certain variant type
	std::vector<std::string> TITAN_API GetVariantNames(const std::string& variantType) const;

	//! Convert the current face state to DNA. Note that this does NOT currently include the updated skinning weights which should be generated post auto-rigging using func UpdateFaceSkinningWeightsFromBody
	bool TITAN_API StateToDna(const State& State, dna::Writer* InOutDnaWriter) const;

	bool TITAN_API CopyBodyJointsToFace(const dna::Reader* InBodyDnaReader, const dna::Reader* InFaceDnaReader, dna::Writer* InOutDnaWriter) const;

	bool TITAN_API AddRbfControlsFromReference(dna::Reader* InReferenceDnaReader, dna::Reader* InTargetDnaReader, dna::Writer* InOutDnaWriter) const;

	//! Update the face skinning weights from the combined body, such that the skinning weights exactly match those of the body at the neck seam and are blended between body and head in the neck region;
	//! weights should be provided for each lod as a pair of number of combined model vertices at that lod and triplets from a sparse joint influence matrix
	bool TITAN_API UpdateFaceSkinWeightsFromBody(const std::vector<std::pair<int, std::vector<Eigen::Triplet<float>>>>& InCombinedBodySkinWeights, const dna::Reader* InFaceDnaReader, dna::Writer* InOutDnaWriter) const;

	//! Selects a vertex from @p Vertices based on the ray defined by @p Origin and @p Direction.
	int TITAN_API SelectVertex(const Eigen::Matrix<float, 3, -1>& Vertices, const Eigen::Vector3f& Origin, const Eigen::Vector3f& Direction) const;

	//! @returns the neck region index
	int TITAN_API GetNeckRegionIndex() const;

private:
	MetaHumanCreatorAPI();

	struct Private;
	Private* m{};
};

class TITAN_API MetaHumanCreatorAPI::State
{
public:
	enum class FaceAttribute
	{
		Proportions,
		Features,
		Both
	};

public:
	~State();
	State(State&&) = delete;
	State& operator=(State&&) = delete;
	State& operator=(const State&) = delete;

	std::shared_ptr<State> Clone() const;

	int NumGizmos() const;
	bool EvaluateGizmos(const float* InVertices, float* OutGizmos) const;

	int NumLandmarks() const;
	bool EvaluateLandmarks(const float* InVertices, float* OutLandmarks) const;

	//! Update the underlying dmt model
	bool UpdateDmt();

	//! Reset the model to the mean
	bool Reset(bool resetBody = false);

	struct BlendOptions {
		FaceAttribute Type = FaceAttribute::Both;
		bool bBlendSymmetrically{ true };
		bool bBlendRelativeTranslation{ false };
	};

	//! Reset a region using @p alpha as a blend factor (1.0 will do a full reset). @p Type defines whether proportions, features, or both are reset.
	bool ResetRegion(int GizmoIndex, float Alpha, const BlendOptions& InBlendOptions);

	/**
	 *  Blend region @p GizmoIndex (all regions besides neck if -1) to @p States.
	 * @p Type defines whether proportions, features, or both are blended.
	 */
	bool Blend(int GizmoIndex, const std::vector<std::pair<float, const State*>>& States, const BlendOptions& InBlendOptions);

	bool Randomize(float magnitude);

	bool SelectPreset(int GizmoIndex, const std::string& PresetName, const BlendOptions& InBlendOptions);

	bool BlendPresets(int GizmoIndex, const std::vector<std::pair<float, std::string>>& alphaAndPresetNames, const BlendOptions& InBlendOptions);

	struct FitToTargetOptions {
		//! alignment when fitting meshes or a dna file
		AlignmentOptions alignmentOptions{ AlignmentOptions::ScalingRotationTranslation };
		//! adapt the neck region to blend to the body shape
		bool adaptNeck{ true };
	};


#ifdef _MSC_VER
	// disabling warning about padded structure
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

	struct FitToTargetResult {
		Eigen::Matrix4f transform;
		float scale;
	};


#ifdef _MSC_VER
#pragma warning(pop)
#endif


	bool FitToTarget(const float* InVertices, int NumVertices);

	bool FitToTarget(const std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>>& InVertices, const FitToTargetOptions& options, FitToTargetResult* result = nullptr, bool useStabModel = true);

	bool FitToTarget(const dna::Reader* InDnaReader, const FitToTargetOptions& option, FitToTargetResult* result = nullptr);

	//! Adapt the neck by blending the vertex delta to zero. Does not do anything if the vertex delta of the neck seam is alerady zero.
	bool AdaptNeck();

	bool HasGizmo(int GizmoIndex) const;

	struct TranslateGizmoOptions {
		bool bSymmetric{ true };
		bool bEnforceBounds{ true };
		float BBoxSoftBound{ 0.2f };
		float BBoxReduction{ 0.2f };
	};
	bool TranslateGizmo(int GizmoIndex, const float DeltaXYZ[3], bool bInSymmetric);
	bool TranslateGizmo(int GizmoIndex, const float DeltaXYZ[3], const TranslateGizmoOptions& InTranslateGizmoOptions);

	struct GizmoPositionOptions {
		bool bSymmetric{ true };
		bool bEnforceBounds{ true };
		float BBoxReduction{ 0.2f };
	};
	bool SetGizmoPosition(int InGizmoIndex, const float InPosition[3], const GizmoPositionOptions& InGizmoPositionOptions);
	bool GetGizmoPosition(int InGizmoIndex, float OutPosition[3]) const;
	/**
		* Get the bounds of the gizmo
		* @param [in] InBBoxReduction      How much to reduce the bounding box ratio.
		* @param [in] bInExpandToCurrent   Whether to expand the bounds to include the current gizmo position
		*/
	bool GetGizmoPositionBounds(int InGizmoIndex, float OutMinPosition[3], float OutMaxPosition[3], float InBBoxReduction, bool bInExpandToCurrent) const;

	struct GizmoRotationOptions {
		bool bSymmetric{ true };
		bool bEnforceBounds{ true };
	};
	bool SetGizmoRotation(int GizmoIndex, const float Eulers[3], bool bInSymmetric);
	bool SetGizmoRotation(int GizmoIndex, const float Eulers[3], const GizmoRotationOptions& InGizmoRotationOptions);
	bool GetGizmoRotation(int GizmoIndex, float Euler[3]) const;
	bool GetGizmoRotationBounds(int InGizmoIndex, float OutMinEuler[3], float OutMaxEuler[3], bool bInExpandToCurrent) const;

	struct GizmoScalingOptions {
		bool bSymmetric{ true };
		bool bEnforceBounds{ true };
	};
	bool SetGizmoScale(int GizmoIndex, float scale, bool bInSymmetric);
	bool SetGizmoScale(int GizmoIndex, float scale, const GizmoScalingOptions& InGizmoScalingOptions);
	bool GetGizmoScale(int GizmoIndex, float& scale) const;
	bool GetGizmoScaleBounds(int GizmoIndex, float& OutMinScale, float& OutMaxScale, bool bInExpandToCurrent) const;

	bool SetGlobalScale(float scale);

	bool GetGlobalScale(float& scale) const;

	bool SetFaceScale(float scale);

	bool GetFaceScale(float& scale) const;

	bool TranslateLandmark(int LandmarkIndex, const float DeltaXYZ[3], bool bInSymmetric);

	bool AddLandmark(int VertexIndex);

	bool HasLandmark(int VertexIndex) const;

	bool RemoveLandmark(int LandmarkIndex);

	int SelectFaceVertex(const Eigen::Vector3f& InOriginXYZ, const Eigen::Vector3f& InDirectionXYZ, Eigen::Vector3f& OutVertex, Eigen::Vector3f& OutNormal) const;

	bool Serialize(std::string& OutArchive) const;
	bool Serialize(trio::BoundedIOStream* OutputStream) const;

	bool Deserialize(const std::string& InArchive);
	bool Deserialize(trio::BoundedIOStream* InputStream);

	const std::shared_ptr<const Settings>& GetSettings() const;

	void SetSettings(const std::shared_ptr<Settings>& settings);

	bool SetExpressionActivations(const std::map<std::string, float>& expressionActivations);

	bool ResetNeckExclusionMask();

	//! Update the face state base on the body bind pose and body "face" vertices (face vertices in the combined body/face model)
	bool SetBodyJointsAndBodyFaceVertices(const float* InBodyBindPoses, const float* InBodyVertices);

	//! Set high frequency variant @p HFIndex. Disable HF if @p HFIndex < 0
	bool SetHFVariant(int HFIndex);
	int GetHFVariant() const;

	//! Set values for variant @p VariantType. Set VariantValues to nullptr to clear the variants.
	bool SetVariant(const std::string& VariantType, const float* VariantValues);

	bool GetVariant(const std::string& VariantType, float* VariantValues) const;

	//! Calibrate expressions based on the Neutral state
	bool Calibrate();

	//! Dump all data required for auto rigging to local directory @p directoryName
	bool DumpDataForAR(const std::string& directoryName) const;

private:
	State();
	State(const State&);

	struct Private;
	Private* m{};

private:
	friend class MetaHumanCreatorAPI;
};

class TITAN_API MetaHumanCreatorAPI::Settings
{
public:
	Settings();
	~Settings();
	Settings(Settings&&) = delete;
	Settings& operator=(Settings&&) = delete;
	Settings& operator=(const Settings&) = delete;

	std::shared_ptr<Settings> Clone() const;

	float GlobalVertexDeltaScale() const;
	void SetGlobalVertexDeltaScale(float globalVertexDeltaScale);

	float RegionVertexDeltaScale(int patchId) const;
	void SetRegionVertexDeltaScale(int patchId, float vertexDeltaScale);

	bool GenerateAssetsAndEvaluateAllLODs() const;
	void SetGenerateAssetsAndEvaluateAllLODs(bool bGenerateAssetsAndEvaluateAllLODs);

	bool DmtWithSymmetry() const;
	void SetDmtWithSymmetry(bool isDmtWithSymmetry);

	float DmtPcaThreshold() const;
	void SetDmtPcaThreshold(float DmtPcaThreshold);

	float DmtRegularization() const;
	void SetDmtRegularization(float bInDmtRegularization);

	bool DmtStabilizeFixLandmarks() const;
	void SetDmtStabilizeFixLandmarks(bool bInDmtStabilizeFixLandmarks);

	bool LockFaceScale() const;
	void SetLockFaceScale(bool lockFaceScale);

	bool LockBodyFaceState() const;
	void SetLockBodyFaceState(bool lockBodyState);

	bool CombineFaceAndBodyInEvaluation() const;
	void SetCombineFaceAndBodyInEvaluation(bool combineFaceAndBody);

	bool UpdateBodyJointsInEvaluation() const;
	void SetUpdateBodyJointsInEvaluation(bool updateBodyJoints);

	bool UpdateFaceSurfaceJointsInEvaluation() const;
	void SetUpdateFaceSurfaceJointsInEvaluation(bool updateFaceSurfaceJoints);

	bool UpdateFaceVolumetricJointsInEvaluation() const;
	void SetUpdateFaceVolumetricJointsInEvaluation(bool updateFaceVolumetricJoints);

	bool UpdateBodySurfaceJointsInEvaluation() const;
	void SetUpdateBodySurfaceJointsInEvaluation(bool updateFaceSurfaceJoints);

	bool UseBodyDeltaInEvaluation() const;
	void SetUseBodyDeltaInEvaluation(bool useBodyDelta);

	bool UseScaleInBodyFit() const;
	void SetUseScaleInBodyFit(bool useScaleInBodyFit);

	float BodyFitRegularization() const;
	void SetBodyFitRegularization(float bodyFitRegularization);

	bool UseCanoncialBodyInEvaluation() const;
	void SetUseCanonicalBodyInEvaluation(bool useCanconicalBodyInEvaluation);

	float GlobalHFScale() const;
	void SetGlobalHFScale(float Scale);

	float RegionHfScale(int patchId) const;
	void SetRegionHfScale(int patchId, float vertexDeltaScale);

	int HFIterations() const;
	void SetHFIterations(int Iterations);

private:

	Settings(const Settings&);

	struct Private;
	Private* m{};

private:
	friend class MetaHumanCreatorAPI;
};

} // namespace TITAN_API_NAMESPACE
