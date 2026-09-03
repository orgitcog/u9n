// Copyright Epic Games, Inc. All Rights Reserved.

#include <MetaHumanCreatorAPI.h>
#include <Common.h>

#include <bodyshapeeditor/SerializationHelper.h>
#include <carbon/Algorithm.h>
#include <carbon/geometry/AABBTree.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/NpyFileFormat.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/StringUtils.h>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/utils/Timer.h>
#include <nls/geometry/MeshSmoothing.h>
#include <nls/geometry/Procrustes.h>
#include <nls/geometry/QuaternionAverage.h>
#include <nrr/AssetGeneration.h>
#include <nls/geometry/LodGeneration.h>
#include <nls/geometry/SnapConfig.h>
#include <nls/math/PCG.h>
#include <nls/serialization/ObjFileFormat.h>
#include <rig/BindPoseJointsCalculation.h>
#include <rig/Rig.h>
#include <rig/BarycentricCoordinatesForOddLods.h>
#include <rig/RigUtils.h>
#include <nrr/DmtModel.h>
#include <nrr/IdentityBlendModel.h>
#include <nrr/MeshLandmarks.h>
#include <nrr/PatchBlendModel.h>
#include <nrr/VertexWeights.h>
#include <nrr/RigFitting.h>
#include <rigcalibration/ModelData.h>
#include <rigcalibration/RigCalibrationCore.h>
#include <rig/SkinningWeightUtils.h>
#include <nrr/NeckSeamSnapConfig.h>
#include <nrr/LoadNeckFalloffMasks.h>
#include <nrr/UpdateHeadMeshSkinningWeightsFromBody.h>

#include <filesystem>
#include <map>
#include <numeric>
#include <mutex>
#include <random>

#include <dna/Reader.h>
#include <dna/BinaryStreamReader.h>
#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <trio/Stream.h>

#include <nls/serialization/EigenSerialization.h>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

namespace
{

std::vector<int> GetSymmetricIndices(const std::vector<int>& vertexIndices, const std::vector<int>& symmetry)
{
	std::vector<int> out(vertexIndices.size(), -1);
	if (symmetry.size() > 0)
	{
		for (size_t j = 0; j < vertexIndices.size(); ++j)
		{
			const int vID = vertexIndices[j];
			if (vID < 0) { continue; }
			const int sym_vID = symmetry[vID];
			if (sym_vID == vID)
			{
				out[j] = (int)j;
			}
			else
			{
				const int sym_j = GetItemIndex(vertexIndices, sym_vID);
				out[j] = sym_j;
				if (sym_j < 0)
				{
					CARBON_CRITICAL("symmetries are defined but landmarks are not symmetric: {} vs {}", vID, sym_vID);
				}
			}
		}
	}
	return out;
}

static constexpr const char* FaceFitMaskName = "dna_estimator";
static constexpr const char* BodyBlendMaskName = "body_blend";
static constexpr const char* BodyFitMaskName = "body_fit";
static constexpr const char* DeltaMushMaskName = "delta_mush";
// static constexpr const char* NeckSeamMaskName = "neck_seam";
static constexpr const char* HeadJointName = "head";

struct DmtLandmarkData
{
	std::vector<int> vertexIndices;
	std::vector<int> symmetries; //!< symmetry mapping with indices into @see vertexIndices
	int selectedIndex{ -1 };

	int GetSymmetricIndex(int idx) const
	{
		return (idx >= 0 && idx < (int)vertexIndices.size() && symmetries.size() == vertexIndices.size()) ? symmetries[idx] : -1;
	}

	bool IsSelfSymmetric(int idx) const
	{
		return (GetSymmetricIndex(idx) == idx);
	}
};

struct DmtGizmoData
{
	std::vector<int> vertexIndices;
	std::vector<int> symmetries; //!< symmetry mapping with indices into @see vertexIndices
	int selectedIndex{ -1 };

	int GetSymmetricIndex(int idx) const
	{
		return (idx >= 0 && idx < (int)vertexIndices.size() && symmetries.size() == vertexIndices.size()) ? symmetries[idx] : -1;
	}

	bool HasSymmetry() const { return vertexIndices.size() == symmetries.size(); }
};

struct DmtSettings
{
	//! whether to use symmetric modeling
	bool symmetricDmt = true;
	//! whether to use a single region per landmark
	bool singleRegionPerLandmark = true;
	//! regularization parameter for dmt
	float dmtRegularization = 0.03f;
	//! min/max value for dmt
	float dmtPcaThreshold = 3.0f;
	//! whether to compensate for landmark delta of fixed landmarks (the landmarks that should not move)
	bool dmtStabilizeFixLandmarks = true;
};

struct FittingSettings
{
	//! region/patch that is fixed rigidly when evaluating PatchBlendModel
	int fixedRegion = 19;
	//! number of iterations
	int numIterations = 3;
};

struct EvaluationSettings
{
	//! global scale applying per-vertex delta to evaluated data
	float globalVertexDeltaScale = 1.0f;

	//! per region scale vertex delta
	Eigen::VectorXf regionVertexDeltaScales;

	//! number of iterations for smoothing
	int hfIterations = 10;

	//! global scaling of hf delta
	float globalHfScale = 1.0f;

	//! per region hf scaling
	Eigen::VectorXf regionHfScales;

	//! whether to generate assets and evaluate all LODs
	bool generateAssetsAndEvaluateAllLODs = true;

	//! whether to update body surface joints (should not be done for legacy bodies)
	bool updateBodySurfaceJoints = false;

	bool combineFaceAndBody = true;
	bool updateBodyJoints = true;
	bool useBodyDelta = true;
	bool useCanonicalBodyInEvaluation = false;
	bool updateFaceSurfaceJoints = true;
	bool updateFaceVolumetricJoints = true;

	bool lockBodyFaceState = false;
	bool lockFaceScale = false;
};

bool LoadLandmarksAndGizmos(const std::string& filename,
	const RigGeometry<float>& rigGeometry,
	const PatchBlendModel<float>& patchBlendModel,
	const std::vector<int>& symmetries,
	DmtGizmoData& dmtGizmoData,
	DmtLandmarkData& dmtLandmarkData)
{
	if (!std::filesystem::exists(filename))
	{
		LOG_ERROR("Failed to load mesh landmarks. File not existing on specified path.");
		return false;
	}

	const int meshIndex = 0;
	const std::string meshName = rigGeometry.GetMeshName(meshIndex);

	auto meshLandmarks = std::make_shared<MeshLandmarks<float>>();
	meshLandmarks->Load(filename, rigGeometry.GetMesh(meshName), meshName);

	const auto landmarkIndicesSet = meshLandmarks->GetAllVertexIndices();
	const auto landmarksByName = meshLandmarks->LandmarksBarycentricCoordinates();

	if (landmarkIndicesSet.size() != landmarksByName.size())
	{
		CARBON_CRITICAL("LoadLandmarks failed. Landmarks for DMT file currently only supports basic landmarks.");
	}

	const int numRegions = patchBlendModel.NumPatches();


	dmtGizmoData = DmtGizmoData();
	dmtLandmarkData = DmtLandmarkData();

	dmtGizmoData.vertexIndices = std::vector<int>(numRegions, -1);
	dmtLandmarkData.vertexIndices.clear();

	for (auto [name, baryCoord] : landmarksByName)
	{
		if (name.find("landmark") != std::string::npos)
		{
			dmtLandmarkData.vertexIndices.push_back(baryCoord.Index(0));
		}
		if (name.find("gizmo") != std::string::npos)
		{
			for (int r = 0; r < numRegions; ++r)
			{
				auto regionName = patchBlendModel.PatchName(r);
				std::string jsonRegionName = name.substr(6, name.size());
				if (jsonRegionName == regionName)
				{
					dmtGizmoData.vertexIndices[r] = baryCoord.Index(0);
				}
			}
		}
	}

	dmtLandmarkData.symmetries = GetSymmetricIndices(dmtLandmarkData.vertexIndices, symmetries);
	dmtGizmoData.symmetries = GetSymmetricIndices(dmtGizmoData.vertexIndices, symmetries);

	return true;
}

//! class supporting to transform the face rig based on the body joints
class FaceToBodySkinning
{
public:
	/**
		* Initialize the the face to body skinning object recording the common joints between the body and face rig and creating
		* the appropriate skinning matrices for the joints as well as all meshes.
		* @param bodyRigGeometry  Specifies the body joint rig.
		* @param faceRigGeometry  Specifies the face joint rig as well as the skinning weights for all meshes.
		*/
	void Init(const RigGeometry<float>& bodyRigGeometry, const RigGeometry<float>& faceRigGeometry, const std::string& faceJointName);

	//! Extracts the common joints of face and body based on input body joints.
	Eigen::Matrix<float, 3, -1> ExtractCommonJointsFromBodyJoints(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyJointPositions) const;

	Eigen::Matrix<float, 3, -1> ExtractCommonJointsFromFaceJoints(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceJointPositions) const;

	void UpdateJoints(Eigen::Ref<Eigen::Matrix<float, 3, -1>> faceJoints, const Eigen::Matrix<float, 3, -1>& jointDeltas) const;

	void UpdateVertices(int meshIndex, Eigen::Ref<Eigen::Matrix<float, 3, -1>> faceVertices, const Eigen::Matrix<float, 3, -1>& jointDeltas) const;

	int MainFaceJointIndex() const { return m_faceJointIndex; }

private:
	std::vector<int> m_faceJointIndices;
	std::vector<int> m_bodyJointIndices;
	Eigen::SparseMatrix<float> m_jointOffsetMatrix;
	std::vector<Eigen::SparseMatrix<float>> m_jointOffsetSkinningMatrices;
	//! main face joint index mapping into @see m_bodyJointIndices and @see m_faceJointIndices
	int m_faceJointIndex{ -1 };
};

void FaceToBodySkinning::Init(const RigGeometry<float>& bodyRigGeometry, const RigGeometry<float>& faceRigGeometry, const std::string& faceJointName)
{
	m_bodyJointIndices.clear();
	m_faceJointIndices.clear();

	const int numFaceJoints = faceRigGeometry.GetJointRig().NumJoints();

	// for each face joint find corresponding body joint
	std::vector<int> selectionIndices(numFaceJoints, -1);
	for (int jointIndex = 0; jointIndex < numFaceJoints; ++jointIndex)
	{
		const std::string jointName = faceRigGeometry.GetJointRig().GetJointNames()[jointIndex];
		const int bodyJointIndex = GetItemIndex(bodyRigGeometry.GetJointRig().GetJointNames(), jointName);
		if (bodyJointIndex >= 0)
		{
			selectionIndices[jointIndex] = (int)m_faceJointIndices.size();
			if (jointName == faceJointName)
			{
				m_faceJointIndex = (int)m_faceJointIndices.size();
			}
			m_faceJointIndices.push_back(jointIndex);
			m_bodyJointIndices.push_back(bodyJointIndex);
		}
	}

	// for joints that do not map to the body, find the parent joint that maps to the body
	std::vector<Eigen::Triplet<float>> triplets;
	for (int jointIndex = 0; jointIndex < numFaceJoints; ++jointIndex)
	{
		if (selectionIndices[jointIndex] >= 0)
		{
			triplets.push_back(Eigen::Triplet<float>(jointIndex, selectionIndices[jointIndex], 1.0f));
		}
		else
		{
			int parentIndex = faceRigGeometry.GetJointRig().GetParentIndex(jointIndex);
			while (parentIndex >= 0)
			{
				if (selectionIndices[parentIndex] >= 0)
				{
					triplets.push_back(Eigen::Triplet<float>(jointIndex, selectionIndices[parentIndex], 1.0f));
					break;
				}
				parentIndex = faceRigGeometry.GetJointRig().GetParentIndex(parentIndex);
			}
			if (parentIndex < 0)
			{
				LOG_ERROR("face joint does not have a valid parent joint that has a mapping to the body");
			}
		}
	}

	m_jointOffsetMatrix = Eigen::SparseMatrix<float>(numFaceJoints, m_faceJointIndices.size());
	m_jointOffsetMatrix.setFromTriplets(triplets.begin(), triplets.end());

	m_jointOffsetSkinningMatrices.clear();
	for (int meshIndex = 0; meshIndex < faceRigGeometry.NumMeshes(); ++meshIndex)
	{
		m_jointOffsetSkinningMatrices.push_back(faceRigGeometry.GetJointRig().GetSkinningWeights(faceRigGeometry.GetMeshName(meshIndex)) * m_jointOffsetMatrix);
	}
}

Eigen::Matrix<float, 3, -1> FaceToBodySkinning::ExtractCommonJointsFromBodyJoints(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyJointPositions) const
{
	return bodyJointPositions(Eigen::all, m_bodyJointIndices);
}

Eigen::Matrix<float, 3, -1> FaceToBodySkinning::ExtractCommonJointsFromFaceJoints(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceJointPositions) const
{
	return faceJointPositions(Eigen::all, m_faceJointIndices);
}

void FaceToBodySkinning::UpdateJoints(Eigen::Ref<Eigen::Matrix<float, 3, -1>> faceJoints, const Eigen::Matrix<float, 3, -1>& jointDeltas) const
{
	if (jointDeltas.cols() != m_jointOffsetMatrix.cols())
	{
		CARBON_CRITICAL("joint deltas size is invalid");
	}
	if (faceJoints.cols() != m_jointOffsetMatrix.rows())
	{
		CARBON_CRITICAL("face joints size is invalid: {} vs {}", faceJoints.cols(), m_jointOffsetMatrix.rows());
	}
	faceJoints += jointDeltas * m_jointOffsetMatrix.transpose();
}

void FaceToBodySkinning::UpdateVertices(int meshIndex, Eigen::Ref<Eigen::Matrix<float, 3, -1>> faceVertices, const Eigen::Matrix<float, 3, -1>& jointDeltas) const
{
	if (jointDeltas.cols() != m_jointOffsetSkinningMatrices[meshIndex].cols())
	{
		CARBON_CRITICAL("joint deltas size is invalid");
	}
	if (faceVertices.cols() != m_jointOffsetSkinningMatrices[meshIndex].rows())
	{
		CARBON_CRITICAL("face vertices size is invalid");
	}
	faceVertices += jointDeltas * m_jointOffsetSkinningMatrices[meshIndex].transpose();
}

} // namespace

struct MetaHumanCreatorAPI::Settings::Private
{
	DmtSettings dmtSettings;
	FittingSettings fittingSettings;
	EvaluationSettings evaluationSettings;
	FastPatchModelFitting<float>::Settings bodyFitSettings;
};

struct MetaHumanCreatorAPI::State::Private
{
public:
	std::shared_ptr<const MetaHumanCreatorAPI> mhcApi;

	//! current combined sculpting state
	std::shared_ptr<const PatchBlendModel<float>::State> combinedState;

	//! current face sculpting state
	std::shared_ptr<const PatchBlendModel<float>::State> faceState;

	//! (optional) body sculping state (only required when the body is updated)
	std::shared_ptr<const PatchBlendModel<float>::State> bodyState;

	std::shared_ptr<const MetaHumanCreatorAPI::Settings> settings;

	std::shared_ptr<const DmtGizmoData> dmtGizmoData;
	std::shared_ptr<const DmtLandmarkData> dmtLandmarkData;
	std::shared_ptr<const DmtModel<float>> dmtModel;

	//! global scale
	float combinedScale{ 1.0f };
	float faceScale{ 1.0f };
	float bodyScale{ 1.0f };

	//! high frequency variant selection
	int hfVariant{ -1 };

	//! activation for different variants
	std::map<std::string, std::shared_ptr<const Eigen::VectorXf>> variantValues;

	//! bind poses for the body
	std::shared_ptr<const Eigen::Matrix<float, 3, -1>> bodyJointPositions;

	//! face vertices as defined by the body
	std::shared_ptr<const Eigen::Matrix<float, 3, -1>> bodyFaceVertices;

	//! body deltas
	std::shared_ptr<const Eigen::Matrix<float, 3, -1>> bodyDeltas;

	//! canoncial body vertices (debug)
	std::shared_ptr<const Eigen::Matrix<float, 3, -1>> canoncialBodyVertices;

	std::shared_ptr<const std::map<std::string, Eigen::VectorXf>> calibratedModelParameters;

	std::shared_ptr<const std::map<std::string, float>> expressionActivations;

public:
	//! @return current state, either combined or face
	const std::shared_ptr<const PatchBlendModel<float>::State>& State() const;

	//! Update the face state from the combined and body state
	void UpdateFaceState();

	//! Update the combined state from face and body state
	void UpdateCombinedState();

	//! Update the body delta based on the current scale
	void UpdateBodyDeltas();

	//! @return the body model vertices (scaled and transformed to the body position
	Eigen::Matrix<float, 3, -1> GetBodyModelVertices() const;

	enum class VertexDeltaCalculationMethod
	{
		None,  //!< set vertex deltas to zero
		Adapt, //!< update vertex deltas, but
		Exact  //!< update vertex deltas to perfectly fit the input
	};

	//! Update the vertex deltas
	void UpdateVertexDeltas(const std::shared_ptr<PatchBlendModel<float>::State>& state, const std::map<int, Eigen::Matrix3Xf>& canonicalMeshVertices, VertexDeltaCalculationMethod method);

public:
	static constexpr int32_t MagicNumber = 0x8c3b5f6e;
};


struct MetaHumanCreatorAPI::Private
{
	std::shared_ptr<TaskThreadPool> threadPool;

	std::shared_ptr<RigGeometry<float>> archetypeFaceGeometry;
	std::vector<Mesh<float>> archetypeTriangulatedMeshes;
	std::shared_ptr<RigGeometry<float>> archetypeBodyGeometry;
	std::shared_ptr<PatchBlendModel<float>> patchBlendModel;
	std::shared_ptr<PatchBlendModel<float>> facePatchBlendModel;
	std::shared_ptr<PatchBlendModel<float>> faceTeethEyesPatchBlendModel;
	std::shared_ptr<PatchBlendModelDataManipulator<float>> patchBlendModelDataManipulator;
	std::shared_ptr<FastPatchModelFitting<float>> fastPatchModelFitting;
	std::shared_ptr<MeshSmoothing<float>> meshSmoothing;

	std::shared_ptr<ModelData> rigCalibrationModelData;
	std::shared_ptr<LodGeneration<float>> lodGenerationData;
	std::shared_ptr<AssetGeneration<float>> assetGenerationData;
	std::vector<std::shared_ptr<VertexWeights<float>>> headVertexSkinningWeightsMasks;

	std::map<std::string, std::shared_ptr<const State>> presets;

	std::shared_ptr<const FaceToBodySkinning> faceToBodySkinning;

	std::shared_ptr<BindPoseJointsCalculation> bindPoseJointsCalculation;

	std::vector<int> symmetries;
	
	std::vector<int> modelMeshIds;

	std::map<std::string, int> jointNameToIndex;

	std::map<std::string, VertexWeights<float>> masks;
	std::map<std::string, std::pair<int, SnapConfig<float>>> neckBodySnapConfig;
	std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<float>>>> barycentricCoordinatesForOddLods;

	std::shared_ptr<const State> defaultState;

	//! body surface joint map (only joints that are part of head mesh, but are driven by the body rig) - indices in the full patch blend model
	std::vector<std::pair<int, int>> bodySurfaceJointMap;

	//! for each face joint the equivalent body joint index (if true), or the mapping to the parent face joint that has an equivalent body joint (if false)
	std::vector<std::pair<int, bool>> faceBodyJointMapping;

	//! for each body joint the equivalent face joint index (if true) or -1 if not
	std::vector<std::pair<int, bool>> bodyFaceJointMapping;

	//! High frequency variants
	Eigen::MatrixXf hfVariants;

	//! variants
	std::map<std::string, std::shared_ptr<const IdentityBlendModel<float>>> variants;

	//! archetype face mesh
	std::shared_ptr<const Mesh<float>> faceArchetypeMesh;

	//! ranges for the regions
	std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> regionTranslationRanges;
	std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> regionRotationRanges;
	std::vector<std::pair<float, float>> regionScaleRanges;

	//! the neck region index
	int neckRegionIndex{ -1 };

	//! region neighbourhood
	std::vector<std::vector<bool>> isRegionNeighbor;

	//! mesh search struct
	mutable struct MeshQuery {
		std::mutex mutex;
		std::shared_ptr<AABBTree<float>> aabbTree;
		Eigen::Matrix<int, 3, -1> triangles;
	} meshQuery;
};


const std::shared_ptr<const PatchBlendModel<float>::State>& MetaHumanCreatorAPI::State::Private::State() const
{
	if (combinedState) return combinedState;
	return faceState;
}

void MetaHumanCreatorAPI::State::Private::UpdateFaceState()
{
	if (!combinedState || !bodyState) return;

	auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*faceState);
	for (int regionId = 0; regionId < combinedState->NumPatches(); ++regionId)
	{
		newFaceState->SetPatchScale(regionId, combinedState->PatchScale(regionId) / bodyState->PatchScale(regionId));
		newFaceState->SetPatchPcaWeights(regionId, combinedState->PatchPcaWeights(regionId) - bodyState->PatchPcaWeights(regionId));
		newFaceState->SetPatchTranslation(regionId, combinedState->PatchTranslation(regionId) - bodyState->PatchTranslation(regionId) + mhcApi->m->patchBlendModel->PatchCenterOfGravity(regionId));
		newFaceState->SetPatchRotation(regionId, combinedState->PatchRotation(regionId) * bodyState->PatchRotation(regionId).inverse());
		const auto& combinedVertexDeltas = combinedState->PatchVertexDeltas(regionId);
		const auto& bodyVertexDeltas = bodyState->PatchVertexDeltas(regionId);
		if (combinedVertexDeltas.cols() > 0 && bodyVertexDeltas.cols() > 0) newFaceState->SetPatchVertexDeltas(regionId, combinedVertexDeltas - bodyVertexDeltas);
		else if (combinedVertexDeltas.cols() > 0) newFaceState->SetPatchVertexDeltas(regionId, combinedVertexDeltas);
		else if (bodyVertexDeltas.cols() > 0) newFaceState->SetPatchVertexDeltas(regionId, bodyVertexDeltas);
	}
	faceState = newFaceState;
}

void MetaHumanCreatorAPI::State::Private::UpdateCombinedState()
{
	// relative transformation of body (on top of face, and compensating for previous body state)
	if (bodyState)
	{
		auto newCombinedState = std::make_shared<PatchBlendModel<float>::State>(*faceState);
		for (int regionId = 0; regionId < newCombinedState->NumPatches(); ++regionId)
		{
			newCombinedState->SetPatchScale(regionId, faceState->PatchScale(regionId) * bodyState->PatchScale(regionId));
			newCombinedState->SetPatchPcaWeights(regionId, faceState->PatchPcaWeights(regionId) + bodyState->PatchPcaWeights(regionId));
			newCombinedState->SetPatchTranslation(regionId, faceState->PatchTranslation(regionId) + bodyState->PatchTranslation(regionId) - mhcApi->m->patchBlendModel->PatchCenterOfGravity(regionId));
			newCombinedState->SetPatchRotation(regionId, faceState->PatchRotation(regionId) * bodyState->PatchRotation(regionId));
			const auto& faceVertexDeltas = faceState->PatchVertexDeltas(regionId);
			const auto& bodyVertexDeltas = bodyState->PatchVertexDeltas(regionId);
			if (faceVertexDeltas.cols() > 0 && bodyVertexDeltas.cols() > 0) newCombinedState->SetPatchVertexDeltas(regionId, faceVertexDeltas + bodyVertexDeltas);
			else if (faceVertexDeltas.cols() > 0) newCombinedState->SetPatchVertexDeltas(regionId, faceVertexDeltas);
			else if (bodyVertexDeltas.cols() > 0) newCombinedState->SetPatchVertexDeltas(regionId, bodyVertexDeltas);
		}
		combinedState = newCombinedState;
		combinedScale = faceScale * bodyScale;
	}
	else
	{
		combinedState.reset();
		combinedScale = faceScale;
	}
}

Eigen::Matrix<float, 3, -1> MetaHumanCreatorAPI::State::Private::GetBodyModelVertices() const
{
	if (bodyJointPositions && bodyState)
	{
		// evaluate vertices using body model parameters
		Eigen::Matrix3Xf vertices = mhcApi->m->patchBlendModel->DeformedVertices(*bodyState);

		// scale vertices
		vertices *= combinedScale;

		// get delta of common face/body joints (head, neck_01, neck_02) between scaled face archetype and body joints
		int numFacesJoints = mhcApi->m->archetypeFaceGeometry ? mhcApi->m->archetypeFaceGeometry->GetJointRig().NumJoints() : 0;
		Eigen::Matrix3Xf commonFaceJoints = mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromFaceJoints(vertices.leftCols(numFacesJoints));
		Eigen::Matrix3Xf commonBodyJoints = mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromBodyJoints(*bodyJointPositions);
		Eigen::Matrix3Xf faceToBodyJointDeltas = commonBodyJoints - commonFaceJoints;

		// move scaled vertices vertices to the body joint positions
		const int meshIndex = 0;
		auto faceRange = mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
		mhcApi->m->faceToBodySkinning->UpdateVertices(meshIndex, vertices.block(0, faceRange.first, 3, faceRange.second - faceRange.first), faceToBodyJointDeltas);

		return vertices.block(0, faceRange.first, 3, faceRange.second - faceRange.first);
	}
	else
	{
		return Eigen::Matrix<float, 3, -1>();
	}
}

void MetaHumanCreatorAPI::State::Private::UpdateBodyDeltas()
{
	if (bodyJointPositions && bodyFaceVertices && bodyState)
	{
		Eigen::Matrix<float, 3, -1> bodyModelVertices = GetBodyModelVertices();

		// get the delta between the body vertices and the transformed scaled vertices
		auto newBodyDeltas = std::make_shared<Eigen::Matrix<float, 3, -1>>(3, bodyFaceVertices->cols());
		*newBodyDeltas = *bodyFaceVertices - bodyModelVertices;
		bodyDeltas = newBodyDeltas;
	}
	else
	{
		bodyDeltas.reset();
	}
}

void MetaHumanCreatorAPI::State::Private::UpdateVertexDeltas(const std::shared_ptr<PatchBlendModel<float>::State>& state,
	const std::map<int, Eigen::Matrix3Xf>& canonicalMeshVertices,
	VertexDeltaCalculationMethod method)
{
	for (int id = 0; id < mhcApi->m->patchBlendModel->NumPatches(); ++id)
	{
		state->SetPatchVertexDeltas(id, Eigen::Matrix<float, 3, -1>());
	}

	if (method == VertexDeltaCalculationMethod::None)
	{
		return;
	}

	const Eigen::Matrix3Xf modelVertices = mhcApi->m->patchBlendModel->DeformedVertices(*state);
	Eigen::Matrix3Xf vertexDeltas = Eigen::Matrix3Xf::Zero(3, modelVertices.cols());
	int currentPos = mhcApi->m->patchBlendModelDataManipulator->NumJoints();
	for (int i = 0; i < (int)mhcApi->m->modelMeshIds.size(); ++i)
	{
		int meshIndex = mhcApi->m->modelMeshIds[i];
		auto range = mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
		auto size = range.second - range.first;
		auto it = canonicalMeshVertices.find(meshIndex);
		if (it != canonicalMeshVertices.end())
		{
			const auto& assetVertices = it->second;
			if (size != (int)assetVertices.cols())
			{
				CARBON_CRITICAL("asset vertices size is not correct");
			}
			vertexDeltas.block(0, currentPos, 3, size) = assetVertices - modelVertices.block(0, currentPos, 3, size);
		}
		currentPos += size;
	}

	switch (method)
	{
	default:
	case VertexDeltaCalculationMethod::Adapt:
	{
		// blend the vertex delta to zero in the neck region
		const int offset = mhcApi->m->patchBlendModelDataManipulator->NumJoints();
		auto it = mhcApi->m->masks.find(BodyBlendMaskName);
		if (it != mhcApi->m->masks.end())
		{
			for (int vID = 0; vID < it->second.NumVertices(); ++vID)
			{
				vertexDeltas.col(offset + vID) *= 1.0f - it->second.Weights()[vID];
			}
		}
	}
	break;
	case VertexDeltaCalculationMethod::Exact:
	{
		// make sure vertex delta perfectly fits the input when evaluated
		if (bodyDeltas)
		{
			const int offset = mhcApi->m->patchBlendModelDataManipulator->NumJoints();
			auto it = mhcApi->m->masks.find(BodyBlendMaskName);
			if (it != mhcApi->m->masks.end())
			{
				for (const auto& [vID, alpha] : it->second.NonzeroVerticesAndWeights())
				{
					vertexDeltas.col(offset + vID) -= alpha * bodyDeltas->col(vID) / combinedScale;
				}
			}
		}
	}
	break;
	}

	state->BakeVertexDeltas(vertexDeltas, *mhcApi->m->patchBlendModel);
}

MetaHumanCreatorAPI::MetaHumanCreatorAPI()
	: m(new Private())
{}

MetaHumanCreatorAPI::~MetaHumanCreatorAPI()
{
	delete m;
}

std::shared_ptr<MetaHumanCreatorAPI> MetaHumanCreatorAPI::CreateMHCApi(dna::Reader* InDnaReader,
	const char* InMhcDataPath,
	int numThreads,
	dna::Reader* InBodyDnaReader)
{
	try
	{
		TITAN_RESET_ERROR;
		TITAN_CHECK_OR_RETURN(InDnaReader, nullptr, "dna archetype is not valid");
		dna::BinaryStreamReader* Impl = static_cast<dna::BinaryStreamReader*>(InDnaReader);
		TITAN_CHECK_OR_RETURN(Impl, nullptr, "dna archetype implementation is not valid");
		TITAN_CHECK_OR_RETURN(InMhcDataPath, nullptr, "model path is not valid");

		const std::string PCAModelDescriptionPath = (std::filesystem::path(InMhcDataPath) / "uemhc_rig_calibration_data.json").string();
		const std::string PresetsPath = (std::filesystem::path(InMhcDataPath) / "presets.json").string();
		const std::string LandmarksPath = (std::filesystem::path(InMhcDataPath) / "landmarks_config.json").string();
		const std::string SymmetryPath = (std::filesystem::path(InMhcDataPath) / "symmetry.json").string();
		const std::string MasksPath = (std::filesystem::path(InMhcDataPath) / "masks_face.json").string();
		const std::string FaceSurfaceJointsMappingPath = (std::filesystem::path(InMhcDataPath) / "surface_joints_face.json").string();
		const std::string BodyJointsMappingPath = (std::filesystem::path(InMhcDataPath) / "surface_joints_body.json").string();
		const std::string EyelashesVariantsPath = (std::filesystem::path(InMhcDataPath) / "eyelashes_variants.binary").string();
		const std::string TeethVariantsPath = (std::filesystem::path(InMhcDataPath) / "teeth_variants.binary").string();
		const std::string SpeciesVariantsPath = (std::filesystem::path(InMhcDataPath) / "species_variants.binary").string();
		const std::string HFVariantsPath = (std::filesystem::path(InMhcDataPath) / "hf_variants.binary").string();
		const std::string FaceLodGenerationConfigPath = (std::filesystem::path(InMhcDataPath) / "face_lod_generation.binary").string();
		const std::string VolumetricJointsConfigPath = (std::filesystem::path(InMhcDataPath) / "volumetric_joints.json").string();
		const std::string AssetGenerationConfigPath = (std::filesystem::path(InMhcDataPath) / "asset_generation.binary").string();
		const std::string inputSkinningWeightsConfigJsonFilename = (std::filesystem::path(InMhcDataPath) / "skinningWeightsConfig.json").string();
		const std::string boundsConfigFilename = (std::filesystem::path(InMhcDataPath) / "bounds_config.json").string();

		std::shared_ptr<MetaHumanCreatorAPI> mhcApi(new MetaHumanCreatorAPI);

		if (numThreads != 0)
		{
			mhcApi->m->threadPool = std::make_shared<TaskThreadPool>(numThreads);
		}

		mhcApi->m->patchBlendModel = std::make_shared<PatchBlendModel<float>>(mhcApi->m->threadPool);
		mhcApi->m->rigCalibrationModelData = std::make_shared<ModelData>();
		mhcApi->m->lodGenerationData = nullptr;

		if (std::filesystem::exists(PCAModelDescriptionPath))
		{
			RigCalibrationDatabaseDescription databaseDescriptionLoader;
			databaseDescriptionLoader.Load(PCAModelDescriptionPath);
			mhcApi->m->rigCalibrationModelData->Load(databaseDescriptionLoader, /*loadBlendshapes=*/false);
			if (databaseDescriptionLoader.GetModelMeshIds().empty())
			{
				mhcApi->m->modelMeshIds.resize(Impl->getMeshIndicesForLOD(0).size());
				std::iota(mhcApi->m->modelMeshIds.begin(), mhcApi->m->modelMeshIds.end(), 0);
			}
			else
			{
				mhcApi->m->modelMeshIds = databaseDescriptionLoader.GetModelMeshIds();
			}
		}
		else
		{
			const std::string neutralName = "Neutral";
			const std::string neutralModelFilename = (std::filesystem::path(InMhcDataPath) / "geo_and_bindpose.pca").string();
			std::map<std::string, std::shared_ptr<IdentityBlendModel<float, -1>>> models;
			models[neutralName] = std::make_shared<IdentityBlendModel<float, -1>>();
			models[neutralName]->LoadModelBinary(neutralModelFilename);
			mhcApi->m->rigCalibrationModelData->Set(models, neutralName, {}, {}, {}, {}, {});
		}
		const auto& neutralIdentityModel = mhcApi->m->rigCalibrationModelData->GetModel(mhcApi->m->rigCalibrationModelData->GetNeutralName());
		mhcApi->m->patchBlendModel->LoadFromIdentityModel(neutralIdentityModel);
		std::vector<int> patchModelSymmetries(mhcApi->m->patchBlendModel->NumVertices(), -1);
		mhcApi->m->patchBlendModelDataManipulator = std::make_shared<PatchBlendModelDataManipulator<float>>(Impl);

		auto baseVert = mhcApi->m->patchBlendModel->BaseVertices();

		const auto& meshIds = mhcApi->m->modelMeshIds;
		int numFaceTeethEyeVerts = 0;
		for (size_t meshIdx = 0; meshIdx < meshIds.size(); ++meshIdx)
		{
			const auto& curMeshRange = mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIds[meshIdx]);
			numFaceTeethEyeVerts += curMeshRange.second - curMeshRange.first;
		}
		auto faceMeshRange = mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);
		const size_t numFaceVert = faceMeshRange.second - faceMeshRange.first;
		std::vector<int> faceIndices(numFaceVert);
		std::iota(faceIndices.begin(), faceIndices.end(), faceMeshRange.first);
		std::vector<int> faceTeethEyeIndices(numFaceTeethEyeVerts);
		std::iota(faceTeethEyeIndices.begin(), faceTeethEyeIndices.end(), faceMeshRange.first);
		mhcApi->m->facePatchBlendModel = mhcApi->m->patchBlendModel->Reduce(faceIndices);
		mhcApi->m->faceTeethEyesPatchBlendModel = mhcApi->m->patchBlendModel->Reduce(faceTeethEyeIndices);

		const Eigen::Matrix<float, 3, -1> baseVertices = mhcApi->m->patchBlendModel->BaseVertices();

		auto rigGeometry = std::make_shared<RigGeometry<float>>();
		TITAN_CHECK_OR_RETURN(rigGeometry->Init(Impl, true), nullptr, "cannot load rig geometry");

		JsonElement skinningWeightsConfigJson;
		try
		{
			const std::string skinningWeightsConfigData = ReadFile(inputSkinningWeightsConfigJsonFilename);
			skinningWeightsConfigJson = ReadJson(skinningWeightsConfigData);
		}
		catch (const std::exception& e)
		{
			CARBON_CRITICAL("Failed to load skinning weights config file: {}, error: {}", inputSkinningWeightsConfigJsonFilename, e.what());
		}

		if (skinningWeightsConfigJson.Contains("body_falloff_weights_map") && skinningWeightsConfigJson["body_falloff_weights_map"].IsObject())
		{
			bool bLoadedMasks = LoadNeckFalloffMasks<float>(skinningWeightsConfigJson["body_falloff_weights_map"], *rigGeometry, mhcApi->m->headVertexSkinningWeightsMasks);
			if (!bLoadedMasks)
			{
				CARBON_CRITICAL("Failed to parse neck falloff masks from skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
			}
		}
		else
		{
			CARBON_CRITICAL("Failed to find element body_falloff_weights_map in skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
		}

		mhcApi->m->archetypeFaceGeometry = rigGeometry;
		mhcApi->m->archetypeTriangulatedMeshes.resize(rigGeometry->NumMeshes());
		for (int meshIndex = 0; meshIndex < rigGeometry->NumMeshes(); ++meshIndex)
		{
			mhcApi->m->archetypeTriangulatedMeshes[meshIndex] = mhcApi->m->archetypeFaceGeometry->GetMesh(meshIndex);
			mhcApi->m->archetypeTriangulatedMeshes[meshIndex].Triangulate();
		}

		mhcApi->m->jointNameToIndex = rigutils::JointNameToIndexMap(*rigGeometry);

		const int faceMeshIndex = 0;
		const std::string faceMeshName = rigGeometry->GetMeshName(faceMeshIndex);
		mhcApi->m->faceArchetypeMesh = std::make_shared<Mesh<float>>(rigGeometry->GetMesh(faceMeshIndex));

		if (std::filesystem::exists(MasksPath))
		{
			mhcApi->m->masks = VertexWeights<float>::LoadAllVertexWeights(MasksPath, rigGeometry->GetMesh(faceMeshIndex).NumVertices());
		}
		{
			auto it = mhcApi->m->masks.find(FaceFitMaskName);
			if (it == mhcApi->m->masks.end())
			{
				mhcApi->m->masks[FaceFitMaskName] = VertexWeights<float>(faceMeshRange.second - faceMeshRange.first, 1);
			}
		}

		if (std::filesystem::exists(VolumetricJointsConfigPath) || std::filesystem::exists(FaceSurfaceJointsMappingPath))
		{
			mhcApi->m->bindPoseJointsCalculation = std::make_shared<BindPoseJointsCalculation>();
		}

		if (std::filesystem::exists(VolumetricJointsConfigPath))
		{
			mhcApi->m->bindPoseJointsCalculation->LoadVolumetricConfig(VolumetricJointsConfigPath);
		}
		if (std::filesystem::exists(FaceSurfaceJointsMappingPath))
		{
			mhcApi->m->bindPoseJointsCalculation->LoadSurfaceConfig(FaceSurfaceJointsMappingPath);
		}


		// create model to fit the face mesh efficiently
		auto fastPatchModelFitting = std::make_shared<FastPatchModelFitting<float>>();
		fastPatchModelFitting->Init(mhcApi->m->facePatchBlendModel, mhcApi->m->threadPool);
		{
			// add fitting mask
			auto it = mhcApi->m->masks.find(BodyFitMaskName);
			if (it != mhcApi->m->masks.end())
			{
				fastPatchModelFitting->UpdateMask(it->second);
			}
		}
		mhcApi->m->fastPatchModelFitting = fastPatchModelFitting;

		auto meshSmoothing = std::make_shared<MeshSmoothing<float>>();
		meshSmoothing->SetTopology(rigGeometry->GetMesh(faceMeshIndex), /*identityWeight=*/0.44f, /*step=*/1.0f);
		{
			// add delta mush mask
			auto it = mhcApi->m->masks.find(DeltaMushMaskName);
			if (it != mhcApi->m->masks.end())
			{
				meshSmoothing->SetWeights(it->second.Weights());
			}
		}
		mhcApi->m->meshSmoothing = meshSmoothing;

		if (std::filesystem::exists(PresetsPath))
		{
			const auto json = ReadJson(ReadFile(PresetsPath));
			mhcApi->m->presets.clear();
			for (const auto& [name, obj] : json.Object())
			{
				auto state = std::shared_ptr<State>(new State);
				auto pmState = std::make_shared<PatchBlendModel<float>::State>();
				pmState->FromJson(obj);
				if (pmState->NumPatches() != mhcApi->m->patchBlendModel->NumPatches()) {
					LOG_WARNING("invalid preset {}", name);
					continue;
				}
				bool valid = true;
				for (int regionIndex = 0; regionIndex < pmState->NumPatches(); ++regionIndex)
				{
					valid &= (pmState->PatchPcaWeights(regionIndex).size() == mhcApi->m->patchBlendModel->NumPcaModesForPatch(regionIndex));
				}
				if (!valid)
				{
					LOG_WARNING("invalid preset {}", name);
					continue;
				}
				state->m->faceState = pmState;
				mhcApi->m->presets[name] = state;
			}
		}

		const auto symmetryJson = ReadJson(ReadFile(SymmetryPath));
		if (symmetryJson.Contains("symmetry"))
		{
			if (symmetryJson["symmetry"].IsObject())
			{
				mhcApi->m->symmetries = symmetryJson["symmetry"].Get<std::map<std::string, std::vector<int>>>()[faceMeshName];
			}
			else
			{
				mhcApi->m->symmetries = symmetryJson["symmetry"].Get<std::vector<int>>();
			}
		}
		if (mhcApi->m->symmetries.size() > 0)
		{
			const int vertexOffset = mhcApi->m->patchBlendModelDataManipulator->NumJoints();
			for (size_t k = 0; k < mhcApi->m->symmetries.size(); ++k)
			{
				patchModelSymmetries[k + vertexOffset] = mhcApi->m->symmetries[k] + vertexOffset;
			}
		}

		mhcApi->m->bodySurfaceJointMap.clear();
		if (std::filesystem::exists(BodyJointsMappingPath))
		{
			const JsonElement json = ReadJson(ReadFile(BodyJointsMappingPath));
			if (json.Contains("joint_correspondence"))
			{
				for (const auto& element : json["joint_correspondence"].Array())
				{
					const std::string jointName = element["joint_name"].String();
					const int jointIndex = rigGeometry->GetJointRig().GetJointIndex(jointName);
					const int vID = element["vID"].Get<int>();
					if (vID < rigGeometry->GetMesh(0).NumVertices())
					{
						mhcApi->m->bodySurfaceJointMap.push_back({ jointIndex, rigGeometry->GetJointRig().NumJoints() + vID });
					}
				}
			}
		}

		auto defaultSettings = std::make_shared<MetaHumanCreatorAPI::Settings>();
		defaultSettings->m->evaluationSettings.regionHfScales = Eigen::VectorXf::Ones(mhcApi->m->patchBlendModel->NumPatches());
		defaultSettings->m->evaluationSettings.regionVertexDeltaScales = Eigen::VectorXf::Ones(mhcApi->m->patchBlendModel->NumPatches());
		auto defaultDmtGizmoData = std::make_shared<DmtGizmoData>();
		auto defaultDmtLandmarkData = std::make_shared<DmtLandmarkData>();

		LoadLandmarksAndGizmos(LandmarksPath, *rigGeometry, *mhcApi->m->patchBlendModel, mhcApi->m->symmetries, *defaultDmtGizmoData, *defaultDmtLandmarkData);

		defaultSettings->m->dmtSettings.symmetricDmt = defaultDmtGizmoData->HasSymmetry();
		for (int idx = 0; idx < mhcApi->m->patchBlendModel->NumPatches(); ++idx)
		{
			if (StringToLower(mhcApi->m->patchBlendModel->PatchName(idx)) == "neck")
			{
				defaultSettings->m->fittingSettings.fixedRegion = idx;
				defaultSettings->m->bodyFitSettings.fixedRegion = idx;
			}
		}

		auto defaultState = std::shared_ptr<MetaHumanCreatorAPI::State>(new MetaHumanCreatorAPI::State());

		defaultState->m->faceState = std::make_shared<PatchBlendModel<float>::State>(mhcApi->m->patchBlendModel->CreateState());
		auto dmtModel = std::make_shared<DmtModel<float>>(mhcApi->m->patchBlendModel, patchModelSymmetries, mhcApi->m->threadPool);
		dmtModel->Init(defaultDmtLandmarkData->vertexIndices,
			mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
			defaultSettings->m->dmtSettings.singleRegionPerLandmark,
			defaultSettings->m->dmtSettings.dmtRegularization);

		defaultState->m->dmtModel = dmtModel;
		defaultState->m->settings = defaultSettings;
		defaultState->m->dmtGizmoData = defaultDmtGizmoData;
		defaultState->m->dmtLandmarkData = defaultDmtLandmarkData;
		mhcApi->m->defaultState = defaultState;

		auto loadBlendModel = [](const std::string& filename)
			{
				if (std::filesystem::exists(filename))
				{
					auto model = std::make_shared<IdentityBlendModel<float>>();
					if (model->LoadModelBinary(filename))
					{
						return model;
					}
					else
					{
						CARBON_CRITICAL("failed to load model \"{}\"", filename);
					}
				}
				return std::shared_ptr<IdentityBlendModel<float>>();
			};
		auto eyelashesVariants = loadBlendModel(EyelashesVariantsPath);
		auto teethVariants = loadBlendModel(TeethVariantsPath);
		auto speciesVariants = loadBlendModel(SpeciesVariantsPath);
		if (eyelashesVariants) { mhcApi->m->variants["eyelashes"] = eyelashesVariants; }
		if (teethVariants) { mhcApi->m->variants["teeth"] = teethVariants; }
		if (speciesVariants) { mhcApi->m->variants["species"] = speciesVariants; }

		auto hfVariants = loadBlendModel(HFVariantsPath);
		if (hfVariants)
		{
			auto faceRange = mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(faceMeshIndex);
			Eigen::MatrixXf hfMatrix = hfVariants->ModelMatrix()->toDense();
			mhcApi->m->hfVariants = hfMatrix.block(3 * faceRange.first, 0, 3 * (faceRange.second - faceRange.first), hfMatrix.cols());
		}

		if (std::filesystem::exists(FaceLodGenerationConfigPath))
		{
			mhcApi->m->lodGenerationData = std::make_shared<LodGeneration<float>>();
			if (!mhcApi->m->lodGenerationData->LoadModelBinary(FaceLodGenerationConfigPath))
			{
				CARBON_CRITICAL("failed to load lod generation model \"{}\"", FaceLodGenerationConfigPath);
			}
			mhcApi->m->lodGenerationData->SetThreadPool(mhcApi->m->threadPool);
		}
		else
		{
			LOG_WARNING("No face lod generation config found; only lod 0 will be available");
		}

		auto assetGenerationData = std::make_shared<AssetGeneration<float>>();
		if (assetGenerationData->LoadModelBinary(AssetGenerationConfigPath))
		{
			assetGenerationData->SetThreadPool(mhcApi->m->threadPool);
			mhcApi->m->assetGenerationData = assetGenerationData;
		}
		else
		{
			LOG_WARNING("failed to load asset generation model \"{}\"", AssetGenerationConfigPath);
		}

		if (InBodyDnaReader)
		{
			dna::BinaryStreamReader* bodyStream = static_cast<dna::BinaryStreamReader*>(InBodyDnaReader);
			auto bodyRigGeometry = std::make_shared<RigGeometry<float>>();
			TITAN_CHECK_OR_RETURN(bodyRigGeometry->Init(bodyStream, true), nullptr, "cannot load body rig geometry");
			mhcApi->m->archetypeBodyGeometry = bodyRigGeometry;

			mhcApi->m->faceBodyJointMapping = skinningweightutils::CalculateFaceBodyJointMapping(rigGeometry->GetJointRig(), bodyRigGeometry->GetJointRig());
			mhcApi->m->bodyFaceJointMapping = skinningweightutils::CalculateBodyFaceJointMapping(rigGeometry->GetJointRig(), bodyRigGeometry->GetJointRig());
		}

		if (skinningWeightsConfigJson.Contains("neck_seam_snap_config") && skinningWeightsConfigJson["neck_seam_snap_config"].IsObject())
		{
			NeckSeamSnapConfig<float> snapConfig;

			bool bLoaded = snapConfig.ReadJson(skinningWeightsConfigJson["neck_seam_snap_config"]);
			if (!bLoaded)
			{
				LOG_ERROR("Failed to read neck_seam_snap_config from skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
			}

			// check validity of snap config, but only if we have the body archetype
			if (mhcApi->m->archetypeBodyGeometry)
			{
				if (!snapConfig.IsValidForCombinedBodyAndFaceRigs(*mhcApi->m->archetypeBodyGeometry, *rigGeometry))
				{
					CARBON_CRITICAL("neck_seam_snap_config is not valid for supplied body and face rig geometry");
				}
			}

			mhcApi->m->neckBodySnapConfig = snapConfig.GetLodNeckSeamSnapConfigs();
		}
		else
		{
			CARBON_CRITICAL("Failed to find element neck_seam_snap_config in skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
		}

		if (skinningWeightsConfigJson.Contains("barycentric_coordinates_for_odd_lods") && skinningWeightsConfigJson["barycentric_coordinates_for_odd_lods"].IsObject())
		{
			BarycentricCoordinatesForOddLods<float> bcs;
			bool bLoaded = bcs.ReadJson(skinningWeightsConfigJson["barycentric_coordinates_for_odd_lods"]);
			if (!bLoaded)
			{
				LOG_ERROR("Failed to read barycentric_coordinates_for_odd_lods from skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
			}

			mhcApi->m->barycentricCoordinatesForOddLods = bcs.GetBarycentricCoordinatesForOddLods();
		}
		else
		{
			CARBON_CRITICAL("Failed to find element barycentric_coordinates_for_odd_lods in skinning weight config: {}", inputSkinningWeightsConfigJsonFilename);
		}

		if (mhcApi->m->archetypeBodyGeometry)
		{
			std::shared_ptr<FaceToBodySkinning> faceToBodySkinning = std::make_shared<FaceToBodySkinning>();
			faceToBodySkinning->Init(*mhcApi->m->archetypeBodyGeometry, *mhcApi->m->archetypeFaceGeometry, HeadJointName);
			mhcApi->m->faceToBodySkinning = faceToBodySkinning;
		}

		mhcApi->m->regionTranslationRanges.clear();
		mhcApi->m->regionRotationRanges.clear();
		mhcApi->m->regionScaleRanges.clear();
		for (int i = 0; i < mhcApi->m->defaultState->m->faceState->NumPatches(); ++i)
		{
			Eigen::Vector3f pos = mhcApi->m->defaultState->m->faceState->PatchTranslation(i);
			Eigen::Vector3f euler = mhcApi->m->defaultState->m->faceState->PatchRotationEulerDegrees(i);
			const float defaultTranslationRange = 0.1f;
			const float defaultRotationRange = 1.0f;
			mhcApi->m->regionTranslationRanges.push_back({ pos - Eigen::Vector3f::Constant(defaultTranslationRange), pos + Eigen::Vector3f::Constant(defaultTranslationRange) });
			mhcApi->m->regionRotationRanges.push_back({ euler - Eigen::Vector3f::Constant(defaultRotationRange), euler + Eigen::Vector3f::Constant(defaultRotationRange) });
			mhcApi->m->regionScaleRanges.push_back({ 0.85f, 1.3f });
		}
		if (std::filesystem::exists(boundsConfigFilename))
		{
			const auto json = ReadJson(ReadFile(boundsConfigFilename));
			if (json.IsArray() && (int)json.Size() == mhcApi->m->patchBlendModel->NumPatches())
			{
				for (int i = 0; i < mhcApi->m->patchBlendModel->NumPatches(); ++i)
				{
					Eigen::Vector3f minRange, maxRange;
					io::FromJson(json[i][0], minRange);
					io::FromJson(json[i][1], maxRange);
					mhcApi->m->regionTranslationRanges[i].first = minRange;
					mhcApi->m->regionTranslationRanges[i].second = maxRange;
					if (json[i].Size() > 2)
					{
						io::FromJson(json[i][2], minRange);
						io::FromJson(json[i][3], maxRange);
						mhcApi->m->regionRotationRanges[i].first = minRange;
						mhcApi->m->regionRotationRanges[i].second = maxRange;
						mhcApi->m->regionScaleRanges[i].first = json[i][4].Get<float>();
						mhcApi->m->regionScaleRanges[i].second = json[i][5].Get<float>();
					}
				}
			}
		}
		else if (mhcApi->m->presets.size() > 0)
		{
			for (const auto& [name, state] : mhcApi->m->presets)
			{
				for (int i = 0; i < mhcApi->m->defaultState->m->faceState->NumPatches(); ++i)
				{
					Eigen::Vector3f pos = state->m->faceState->PatchTranslation(i);
					float scale = state->m->faceState->PatchScale(i);
					Eigen::Vector3f euler = state->m->faceState->PatchRotationEulerDegrees(i);

					mhcApi->m->regionTranslationRanges[i].first = mhcApi->m->regionTranslationRanges[i].first.array().min(pos.array());
					mhcApi->m->regionTranslationRanges[i].second = mhcApi->m->regionTranslationRanges[i].second.array().max(pos.array());
					mhcApi->m->regionRotationRanges[i].first = mhcApi->m->regionRotationRanges[i].first.array().min(euler.array());
					mhcApi->m->regionRotationRanges[i].second = mhcApi->m->regionRotationRanges[i].second.array().max(euler.array());
					mhcApi->m->regionScaleRanges[i].first = std::min(mhcApi->m->regionScaleRanges[i].first, scale);
					mhcApi->m->regionScaleRanges[i].second = std::max(mhcApi->m->regionScaleRanges[i].second, scale);
				}
			}
			/*
			if (!std::filesystem::exists(boundsConfigFilename))
			{
				// save bound configs
				JsonElement json(JsonElement::JsonType::Array);
				for (int i = 0; i < mhcApi->m->patchBlendModel->NumPatches(); ++i)
				{
					JsonElement jsonElement(JsonElement::JsonType::Array);
					jsonElement.Append(io::ToJson(mhcApi->m->regionTranslationRanges[i].first));
					jsonElement.Append(io::ToJson(mhcApi->m->regionTranslationRanges[i].second));
					jsonElement.Append(io::ToJson(mhcApi->m->regionRotationRanges[i].first));
					jsonElement.Append(io::ToJson(mhcApi->m->regionRotationRanges[i].second));
					jsonElement.Append(JsonElement(mhcApi->m->regionScaleRanges[i].first));
					jsonElement.Append(JsonElement(mhcApi->m->regionScaleRanges[i].second));
					json.Append(std::move(jsonElement));
				}
				WriteFile(boundsConfigFilename, WriteJson(json));
			}*/
		}
		// symmetrize bounds
		if (defaultDmtGizmoData->HasSymmetry())
		{
			std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> newRegionTranslationRanges = mhcApi->m->regionTranslationRanges;
			std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> newRegionRotationRanges = mhcApi->m->regionRotationRanges;
			std::vector<std::pair<float, float>> newRegionScaleRanges = mhcApi->m->regionScaleRanges;
			for (int i = 0; i < mhcApi->m->defaultState->m->faceState->NumPatches(); ++i)
			{
				const int symmetricIndex = defaultDmtGizmoData->GetSymmetricIndex(i);

				auto symmetricSwap = [](const Eigen::Vector3f& p) -> Eigen::Vector3f {
					return Eigen::Vector3f(-p[0], p[1], p[2]);
					};

				const Eigen::Vector3f symmetricMinTranslation = symmetricSwap(mhcApi->m->regionTranslationRanges[symmetricIndex].first);
				const Eigen::Vector3f symmetricMaxTranslation = symmetricSwap(mhcApi->m->regionTranslationRanges[symmetricIndex].second);
				const Eigen::Vector3f symmetricMinRotation = -symmetricSwap(mhcApi->m->regionRotationRanges[symmetricIndex].first);
				const Eigen::Vector3f symmetricMaxRotation = -symmetricSwap(mhcApi->m->regionRotationRanges[symmetricIndex].second);

				newRegionTranslationRanges[i].first = 0.5f * (mhcApi->m->regionTranslationRanges[i].first.array() + symmetricMinTranslation.array().min(symmetricMaxTranslation.array()));
				newRegionTranslationRanges[i].second = 0.5f * (mhcApi->m->regionTranslationRanges[i].second.array() + symmetricMinTranslation.array().max(symmetricMaxTranslation.array()));
				newRegionRotationRanges[i].first = 0.5f * (mhcApi->m->regionRotationRanges[i].first.array() + symmetricMinRotation.array().min(symmetricMaxRotation.array()));
				newRegionRotationRanges[i].second = 0.5f * (mhcApi->m->regionRotationRanges[i].second.array() + symmetricMinRotation.array().max(symmetricMaxRotation.array()));
				newRegionScaleRanges[i].first = 0.5f * (mhcApi->m->regionScaleRanges[i].first + mhcApi->m->regionScaleRanges[symmetricIndex].first);
				newRegionScaleRanges[i].second = 0.5f * (mhcApi->m->regionScaleRanges[i].second + mhcApi->m->regionScaleRanges[symmetricIndex].second);
			}
			mhcApi->m->regionTranslationRanges = newRegionTranslationRanges;
			mhcApi->m->regionRotationRanges = newRegionRotationRanges;
			mhcApi->m->regionScaleRanges = newRegionScaleRanges;
		}

		mhcApi->m->neckRegionIndex = -1;
		for (int i = 0; i < (int)mhcApi->m->patchBlendModel->PatchNames().size(); ++i)
		{
			if (mhcApi->m->patchBlendModel->PatchNames()[i] == "neck")
			{
				mhcApi->m->neckRegionIndex = i;
			}
		}

		const int numRegions = mhcApi->m->patchBlendModel->NumPatches();
		mhcApi->m->isRegionNeighbor.resize(numRegions);
		for (int i = 0; i < numRegions; ++i)
		{
			mhcApi->m->isRegionNeighbor[i] = std::vector<bool>(numRegions, false);
		}
		for (const std::vector<std::tuple<int, int, float>>& vertexBlendWeights : mhcApi->m->patchBlendModel->BlendMatrix())
		{
			for (int i = 0; i < (int)vertexBlendWeights.size(); ++i)
			{
				const int regionIndex1 = std::get<0>(vertexBlendWeights[i]);
				for (int j = i + 1; j < (int)vertexBlendWeights.size(); ++j)
				{
					const int regionIndex2 = std::get<0>(vertexBlendWeights[j]);
					mhcApi->m->isRegionNeighbor[regionIndex1][regionIndex2] = true;
					mhcApi->m->isRegionNeighbor[regionIndex2][regionIndex1] = true;
				}
			}
		}

		return mhcApi;
	}
	catch (const std::exception& e)
	{
		TITAN_SET_ERROR(-1, fmt::format("failure to initialize: {}", e.what()).c_str());
		return nullptr;
	}
}

void TITAN_API MetaHumanCreatorAPI::SetNumThreads(int numThreads) { m->threadPool->SetNumThreads(numThreads); }

int TITAN_API MetaHumanCreatorAPI::GetNumThreads() const
{
	return (int)m->threadPool->NumThreads();
}

std::shared_ptr<MetaHumanCreatorAPI::State> MetaHumanCreatorAPI::CreateState() const
{
	try
	{
		TITAN_RESET_ERROR;
		auto state = std::shared_ptr<MetaHumanCreatorAPI::State>(new MetaHumanCreatorAPI::State());
		state->m->mhcApi = shared_from_this();
		state->m->faceState = m->defaultState->m->faceState;
		state->m->settings = m->defaultState->m->settings;
		state->m->dmtModel = m->defaultState->m->dmtModel;
		state->m->dmtGizmoData = m->defaultState->m->dmtGizmoData;
		state->m->dmtLandmarkData = m->defaultState->m->dmtLandmarkData;
		state->m->expressionActivations.reset();
		return state;
	}
	catch (const std::exception& e)
	{
		TITAN_SET_ERROR(-1, fmt::format("failure to create state: {}", e.what()).c_str());
		return nullptr;
	}
}

int MetaHumanCreatorAPI::NumVertices() const
{
	return m->patchBlendModelDataManipulator ? m->patchBlendModelDataManipulator->Size() : 0;
}

int MetaHumanCreatorAPI::GetNeckRegionIndex() const
{
	return m->neckRegionIndex;
}

const std::vector<int>& MetaHumanCreatorAPI::GetVertexSymmetries() const
{
	return m->symmetries;
}

Eigen::VectorXf DecodeExpressionModelData(const Eigen::Matrix3Xf& evaluatedModelData)
{
	const int dof = 9;
	const int numTransformations = (int)evaluatedModelData.cols() / 3;
	Eigen::VectorXf flattened(numTransformations * dof);

	const Eigen::Matrix3Xf translation = evaluatedModelData.block<3, -1>(0, 0, 3, numTransformations);
	Eigen::Matrix3Xf rotation = evaluatedModelData.block<3, -1>(0, numTransformations, 3, numTransformations);
	const Eigen::Matrix3Xf scale = evaluatedModelData.block<3, -1>(0, 2 * numTransformations, 3, numTransformations);

	rotation = rotation.array() * (CARBON_PI / 180.0f);

	for (int i = 0; i < numTransformations; ++i)
	{
		flattened.segment<3>(i * dof) = translation.col(i);
		flattened.segment<3>(i * dof + 3) = rotation.col(i);
		flattened.segment<3>(i * dof + 6) = scale.col(i);
	}

	return flattened;
}

bool MetaHumanCreatorAPI::Evaluate(const State& State, float* OutVertices) const
{
	if (!OutVertices) { return false; }

	try
	{
		TITAN_RESET_ERROR;
		Eigen::Matrix3Xf Vertices = Eigen::Matrix3Xf::Zero(3, m->patchBlendModelDataManipulator->Size());

		// evaluate the face model (only joints + specified model mesh IDs)
		auto evaluationState = (State.m->combinedState && State.m->settings->m->evaluationSettings.combineFaceAndBody) ? State.m->combinedState : State.m->faceState;
		if (evaluationState)
		{
			Eigen::VectorXf vertexDeltaScales;
			if ((int)State.m->settings->m->evaluationSettings.regionVertexDeltaScales.size() == m->patchBlendModel->NumPatches())
			{
				vertexDeltaScales = State.m->settings->m->evaluationSettings.regionVertexDeltaScales * State.m->settings->m->evaluationSettings.globalVertexDeltaScale;
			}
			else
			{
				vertexDeltaScales = Eigen::VectorXf::Constant(m->patchBlendModel->NumPatches(), State.m->settings->m->evaluationSettings.globalVertexDeltaScale);
			}
			auto modelOutput = m->patchBlendModel->DeformedVertices(*evaluationState, vertexDeltaScales);
			Vertices.leftCols(m->patchBlendModelDataManipulator->NumJoints()) = modelOutput.leftCols(m->patchBlendModelDataManipulator->NumJoints());
			int currentPos = m->patchBlendModelDataManipulator->NumJoints();
			for (int meshIndex : m->modelMeshIds)
			{
				auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
				auto size = range.second - range.first;
				Vertices.block(0, range.first, 3, size) = modelOutput.block(0, currentPos, 3, size);
				currentPos += size;
			}
		}

		// add smoothing and selection of high frequency delta
		if ((m->hfVariants.size() > 0) && (State.m->hfVariant >= 0) && (State.m->hfVariant < m->hfVariants.cols()) && m->meshSmoothing)
		{
			// smooth
			auto faceRange = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);
			Eigen::Matrix<float, 3, -1> faceVertices = Vertices(Eigen::all, Eigen::seq(faceRange.first, faceRange.second - 1));
			m->meshSmoothing->Apply(faceVertices, State.m->settings->m->evaluationSettings.hfIterations);
			// add HF data (scaled based on global scale)
			float globalScale = State.m->settings->m->evaluationSettings.globalHfScale;
			faceVertices += m->hfVariants.col(State.m->hfVariant).reshaped(3, m->hfVariants.rows() / 3);
			for (int vID = 0; vID < (int)faceVertices.cols(); ++vID)
			{
				float weight = 0;
				for (const auto& [regionId, _, regionWeight] : m->facePatchBlendModel->BlendMatrix()[vID])
				{
					weight += regionWeight * State.m->settings->m->evaluationSettings.regionHfScales[regionId];
				}
				float perVertexScale = globalScale * weight;
				Vertices.col(vID + faceRange.first) = (1.0f - perVertexScale) * Vertices.col(vID + faceRange.first) + perVertexScale * faceVertices.col(vID);
			}
		}

		// add scaling
		if (State.m->combinedScale != 1.0f)
		{
			Vertices *= State.m->combinedScale;
		}

		// update joints and meshes based on the body joints delta
		if (State.m->settings->m->evaluationSettings.updateBodyJoints && State.m->bodyJointPositions)
		{
			Eigen::Matrix<float, 3, -1> commonBodyJoints = m->faceToBodySkinning->ExtractCommonJointsFromBodyJoints(*State.m->bodyJointPositions);
			Eigen::Matrix<float, 3, -1> commonFaceJoints = m->faceToBodySkinning->ExtractCommonJointsFromFaceJoints(Vertices);
			Eigen::Matrix3Xf faceToBodyJointDeltas = commonBodyJoints - commonFaceJoints;
			m->faceToBodySkinning->UpdateJoints(Vertices.leftCols(m->patchBlendModelDataManipulator->NumJoints()), faceToBodyJointDeltas);
			for (int meshIndex : m->modelMeshIds)
			{
				auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
				m->faceToBodySkinning->UpdateVertices(meshIndex, Vertices.block(0, range.first, 3, range.second - range.first), faceToBodyJointDeltas);
			}
		}

		// add body delta
		if (State.m->settings->m->evaluationSettings.useBodyDelta && State.m->bodyDeltas)
		{
			const int offset = m->patchBlendModelDataManipulator->NumJoints();
			auto it = m->masks.find(BodyBlendMaskName);
			if (it != m->masks.end())
			{
				for (const auto& [vID, alpha] : it->second.NonzeroVerticesAndWeights())
				{
					Vertices.col(offset + vID) += alpha * (*State.m->bodyDeltas).col(vID);
					// Vertices.col(offset + vID) = alpha * (*State.m->bodyFaceVertices).col(vID) + (1.0f - alpha) * Vertices.col(offset + vID);
				}
			}
		}

		// optionally use canconical body mesh (debug purposes)
		if (State.m->settings->m->evaluationSettings.useCanonicalBodyInEvaluation && State.m->canoncialBodyVertices)
		{
			const auto faceRange = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);
			Vertices.block(0, faceRange.first, 3, faceRange.second - faceRange.first) = *State.m->canoncialBodyVertices;
		}

		std::map<std::string, Eigen::Matrix<float, 3, -1>> lod0MeshVertices;
		// evaluate the LOD0 assets
		if (m->lodGenerationData && State.m->settings->m->evaluationSettings.generateAssetsAndEvaluateAllLODs)
		{

			for (size_t i = 0; i < m->modelMeshIds.size(); ++i)
			{
				int curMeshIndex = m->modelMeshIds[i];
				const auto& [start, end] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(curMeshIndex);
				// inverse scale to generate assets in canonical space
				lod0MeshVertices[m->archetypeFaceGeometry->GetMeshName(curMeshIndex)] = (1.0 / State.m->combinedScale) * Vertices.block(0, start, 3, end - start);
			}

			if (m->assetGenerationData)
			{
				// generate the additional assets and add them to the lod0 mesh vertices
				std::map<std::string, Eigen::Matrix<float, 3, -1>> assetVertices;
				bool bGeneratedAssets = m->assetGenerationData->Apply(lod0MeshVertices, assetVertices);
				if (!bGeneratedAssets)
				{
					LOG_ERROR("Failed to generate assets");
					return false;
				}

				// add the assets into the right block of Vertices data
				for (const auto& asset : assetVertices)
				{
					lod0MeshVertices[asset.first] = asset.second;
					int curMeshIndex = m->archetypeFaceGeometry->GetMeshIndex(asset.first);
					const auto& [start, end] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(curMeshIndex);
					// apply scale since assets are generated in canonical space
					Vertices.block(0, start, 3, end - start) = State.m->combinedScale * asset.second;
				}
			}
			else
			{
				LOG_ERROR("No asset generation data present");
				return false;
			}
		}

		// add variants (TODO: make much more efficient, this is very expensive)
		if (State.m->variantValues.size() > 0)
		{
			for (const auto& [variantType, variantValues] : State.m->variantValues)
			{
				const int numVertices = m->variants[variantType]->NumVertices();
				Vertices.leftCols(numVertices) += m->variants[variantType]->Evaluate(*variantValues);
			}
		}

		// evaluate LOD > 0 vertices
		if (m->lodGenerationData && State.m->settings->m->evaluationSettings.generateAssetsAndEvaluateAllLODs)
		{
			for (size_t i = 0; i < m->lodGenerationData->Lod0MeshNames().size(); ++i)
			{
				int curMeshIndex = m->archetypeFaceGeometry->GetMeshIndex(m->lodGenerationData->Lod0MeshNames()[i]);
				const auto& [start, end] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(curMeshIndex);
				lod0MeshVertices[m->lodGenerationData->Lod0MeshNames()[i]] = (1.0 / State.m->combinedScale) * Vertices.block(0, start, 3, end - start);
			}

			std::map<std::string, Eigen::Matrix<float, 3, -1>> higherLodMeshVertices;
			bool bGeneratedLods = m->lodGenerationData->Apply(lod0MeshVertices, higherLodMeshVertices);
			if (!bGeneratedLods)
			{
				LOG_ERROR("Failed to generate LODs");
				return false;
			}

			// go through and stick the higher lod results into the right block of Vertices data
			for (const auto& lodMeshVertices : higherLodMeshVertices)
			{
				int curMeshIndex = m->archetypeFaceGeometry->GetMeshIndex(lodMeshVertices.first);
				const auto& [start, end] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(curMeshIndex);
				Vertices.block(0, start, 3, end - start) = State.m->combinedScale * lodMeshVertices.second;
			}
		}

		// update volumetric joints
		if (State.m->settings->m->evaluationSettings.updateFaceVolumetricJoints && (State.m->mhcApi->m->bindPoseJointsCalculation->VolumetricDataLoaded()))
		{
			std::map<std::string, std::pair<int, int>> meshRanges;
			for (const auto& meshId : m->archetypeFaceGeometry->GetMeshIndicesForLOD(0))
			{
				meshRanges[m->archetypeFaceGeometry->GetMeshName(meshId)] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshId);
			}
			State.m->mhcApi->m->bindPoseJointsCalculation->UpdateVolumetric(Vertices, meshRanges, State.m->mhcApi->m->jointNameToIndex);
		}

		// update surface joints
		if (State.m->settings->m->evaluationSettings.updateFaceSurfaceJoints && (State.m->mhcApi->m->bindPoseJointsCalculation->SurfaceDataLoaded()))
		{
			State.m->mhcApi->m->bindPoseJointsCalculation->UpdateSurface(Vertices, m->patchBlendModelDataManipulator->NumJoints(), State.m->mhcApi->m->jointNameToIndex);
		}

		if (State.m->settings->m->evaluationSettings.updateBodySurfaceJoints && (State.m->mhcApi->m->bodySurfaceJointMap.size() > 0))
		{
			for (const auto& [jointIndex, vertexIdx] : State.m->mhcApi->m->bodySurfaceJointMap)
			{
				Vertices.col(jointIndex) = Vertices.col(vertexIdx);
			}
		}

		if (State.m->expressionActivations && !State.m->expressionActivations->empty())
		{
			std::vector<Eigen::Ref<const Eigen::Matrix3Xf>> neutralMeshVertices;
			for (int i = 0; i < m->patchBlendModelDataManipulator->GetNumMeshes(); ++i)
			{
				auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(i);
				neutralMeshVertices.push_back(Vertices(Eigen::all, Eigen::seq(range.first, range.second - 1)));
			}

			const auto numMeshes = m->patchBlendModelDataManipulator->GetNumMeshes();
			std::vector<int> meshIndices(numMeshes);
			std::iota(meshIndices.begin(), meshIndices.end(), 0);

			m->archetypeFaceGeometry->SetRestPose(m->patchBlendModelDataManipulator->GetJointDeltas(Vertices), CoordinateSystem::World);
			Eigen::VectorXf combinedJointDeltas = Eigen::VectorXf::Zero(m->patchBlendModelDataManipulator->NumJoints() * 9);
			for (const auto& [name, activation] : *State.m->expressionActivations)
			{
				auto it = State.m->calibratedModelParameters->find(name);
				if (it != State.m->calibratedModelParameters->end())
				{
					const auto& expressionModel = m->rigCalibrationModelData->GetModel(name);
					const auto jointDeltas = expressionModel->Evaluate(it->second);
					const auto flattenedJointDeltas = DecodeExpressionModelData(jointDeltas);
					combinedJointDeltas += activation * flattenedJointDeltas;
				}
			}

			RigGeometry<float>::State state;
			m->archetypeFaceGeometry->EvaluateWithPerMeshBlendshapes(DiffDataAffine<float, 3, 3> {},
				DiffData<float>(combinedJointDeltas),
				std::vector<DiffDataMatrix<float, 3, -1>>(),
				meshIndices,
				neutralMeshVertices,
				state);

			const auto stateMeshIndices = state.MeshIndices();

			for (int i = 0; i < m->archetypeFaceGeometry->GetJointRig().NumJoints(); ++i)
			{
				Vertices.col(i) = state.GetWorldMatrix(i).block<3, 1>(0, 3);
			}

			const auto resultVertices = state.MoveVertices();
			for (int i = 0; i < (int)stateMeshIndices.size(); ++i)
			{
				const int meshIndex = stateMeshIndices[i];
				const auto& [start, end] = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
				Vertices.block(0, start, 3, end - start).noalias() = resultVertices[i].Matrix();
			}
		}

		std::memcpy(OutVertices, Vertices.data(), sizeof(float) * Vertices.size());

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to evaluate state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::EvaluateNormals(const State& State, const Eigen::Matrix<float, 3, -1>& InVertices, Eigen::Matrix<float, 3, -1>& OutNormals, const std::vector<Eigen::Ref<const Eigen::Matrix<float, 3, -1>>>& InBodyNormals) const
{
	try
	{
		TITAN_RESET_ERROR;
		OutNormals.resize(3, m->patchBlendModelDataManipulator->Size());
		OutNormals.setZero();

		std::vector<int> meshIndices;
		if (State.m->settings->m->evaluationSettings.generateAssetsAndEvaluateAllLODs)
		{
			meshIndices.resize(m->archetypeFaceGeometry->NumMeshes());
			std::iota(meshIndices.begin(), meshIndices.end(), 0);
		}
		else
		{
			// only evaluate lod0
			meshIndices = m->archetypeFaceGeometry->GetMeshIndicesForLOD(/*lod=*/0);
		}

		auto computeNormals = [&](int start, int end) {
			for (int meshIndex = start; meshIndex < end; ++meshIndex)
			{
				auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
				m->archetypeTriangulatedMeshes[meshIndex].CalculateVertexNormalsRef(
					InVertices(Eigen::all, Eigen::seq(range.first, range.second - 1)),
					OutNormals(Eigen::all, Eigen::seq(range.first, range.second - 1)),
					VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/false, m->threadPool.get());

				if ((int)InBodyNormals.size() > 0)
				{
					auto it = m->neckBodySnapConfig.find(m->archetypeFaceGeometry->GetMeshName(meshIndex));
					if (it != m->neckBodySnapConfig.end())
					{
						const auto& [bodyLod, snapConfig] = it->second;
						if (bodyLod < (int)InBodyNormals.size())
						{
							// copy normals of neck seam
							for (size_t v = 0; v < snapConfig.sourceVertexIndices.size(); ++v)
							{
								OutNormals.col(range.first + snapConfig.targetVertexIndices[v]) = InBodyNormals[bodyLod].col(snapConfig.sourceVertexIndices[v]);
							}
						}
					}
				}
			}
			};
		if (m->threadPool) m->threadPool->AddTaskRangeAndWait((int)meshIndices.size(), computeNormals);
		else computeNormals(0, (int)meshIndices.size());
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to calculate normals: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::GetVertex(const float* InVertices, int DNAMeshIndex, int DNAVertexIndex, float OutVertexXYZ[3]) const
{
	try
	{
		TITAN_RESET_ERROR;
		int startIndex = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(DNAMeshIndex).first;
		for (int k = 0; k < 3; ++k)
		{
			OutVertexXYZ[k] = InVertices[3 * (startIndex + DNAVertexIndex) + k];
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get vertex: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::Evaluate(const State& State, Eigen::Matrix<float, 3, -1>& OutVertices) const
{
	try
	{
		TITAN_RESET_ERROR;
		OutVertices.resize(3, NumVertices());
		Evaluate(State, OutVertices.data());
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to evaluate: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::GetMeshVertices(const float* InVertices, int DNAMeshIndex, Eigen::Matrix<float, 3, -1>& OutVertices) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!InVertices) return false;
		if (!m || !m->patchBlendModelDataManipulator) return false;
		if (DNAMeshIndex < 0 || DNAMeshIndex >= m->patchBlendModelDataManipulator->GetNumMeshes()) return false;
		auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(DNAMeshIndex);
		OutVertices = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(InVertices + 3 * range.first, 3, range.second - range.first);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get mesh vertices: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::GetBindPose(const float* InVertices, Eigen::Matrix<float, 3, -1>& OutBindPose) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!InVertices) return false;
		if (!m || !m->patchBlendModelDataManipulator) return false;
		OutBindPose = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(InVertices, 3, NumVertices()).leftCols(m->patchBlendModelDataManipulator->NumJoints());
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get bind pose: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::GetParameters(const State& State, Eigen::VectorXf& OutParameters) const
{
	try
	{
		TITAN_RESET_ERROR;
		OutParameters = State.m->State()->SerializeToVector();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get state parameters: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::GetModelIdentifier(const State& State, std::string& OutIdentifier) const
{
	try
	{
		TITAN_RESET_ERROR;
		OutIdentifier = State.m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get model identifier: {}", e.what());
	}
}

std::vector<std::string> MetaHumanCreatorAPI::GetPresetNames() const
{
	std::vector<std::string> names;
	for (const auto& [name, _] : m->presets)
	{
		names.push_back(name);
	}
	return names;
}

bool MetaHumanCreatorAPI::AddPreset(const std::string& PresetName, std::shared_ptr<const State>& State)
{
	try
	{
		TITAN_RESET_ERROR;
		if (PresetName.empty())
		{
			TITAN_HANDLE_EXCEPTION("Preset name cannot be empty.");
		}
		auto it = m->presets.find(PresetName);
		if (it != m->presets.end())
		{
			TITAN_HANDLE_EXCEPTION("Preset {} already exists.", PresetName);
		}
		else
		{
			m->presets[PresetName] = State;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to add preset: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::RemovePreset(const std::string& PresetName)
{
	try
	{
		TITAN_RESET_ERROR;
		auto it = m->presets.find(PresetName);
		if (it == m->presets.end())
		{
			TITAN_HANDLE_EXCEPTION("No preset {}.", PresetName);
		}
		m->presets.erase(it);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to remove preset: {}", e.what());
	}
}

int MetaHumanCreatorAPI::NumHFVariants() const
{
	return (int)m->hfVariants.cols();
}

std::vector<std::string> MetaHumanCreatorAPI::GetVariantTypes() const
{
	std::vector<std::string> names;
	for (const auto& [name, _] : m->variants)
	{
		names.push_back(name);
	}
	return names;
}

std::vector<std::string> MetaHumanCreatorAPI::GetVariantNames(const std::string& variantType) const
{
	auto it = m->variants.find(variantType);
	if (it != m->variants.end())
	{
		return it->second->ModeNames(0);
	}
	return {};
}

const std::vector<std::string>& MetaHumanCreatorAPI::GetExpressionNames() const
{
	return m->rigCalibrationModelData->GetModelNames();
}

const std::vector<std::string>& MetaHumanCreatorAPI::GetRegionNames() const
{
	return m->patchBlendModel->PatchNames();
}

void UpdateNeutralGeometry(dna::Writer* InDna, const std::vector<Eigen::Matrix3Xf>& vertices, const Eigen::Matrix3Xf& localBindPoseJointTranslation)
{
	for (int i = 0; i < (int)vertices.size(); ++i)
	{
		InDna->setVertexPositions((uint16_t)i, (dna::Position*)vertices[i].data(), (int)vertices[i].cols());
	}

	InDna->setNeutralJointTranslations((dna::Vector3*)localBindPoseJointTranslation.data(), (uint16_t)localBindPoseJointTranslation.cols());
}

bool TITAN_API MetaHumanCreatorAPI::UpdateFaceSkinWeightsFromBody(const std::vector<std::pair<int, std::vector<Eigen::Triplet<float>>>>& InCombinedBodySkinWeights, const dna::Reader* InFaceDnaReader, dna::Writer* InOutDnaWriter) const
{
	try
	{
		const dna::BinaryStreamReader* FaceImpl = static_cast<const dna::BinaryStreamReader*>(InFaceDnaReader);

		// update the skinning weights in the DNA for the head mesh

		RigGeometry<float> rigGeometry;
		bool bInitialized = rigGeometry.Init(FaceImpl, true);
		if (!bInitialized)
		{
			CARBON_CRITICAL("Failed to initialize face DNA rig geometry");
		}

		const int numBodyJoints = m->archetypeBodyGeometry->GetJointRig().NumJoints();
		// set up the skinning weight matrices from the triplets
		std::vector<SparseMatrix<float>> combinedBodySkinningWeights(InCombinedBodySkinWeights.size());
		for (size_t lod = 0; lod < combinedBodySkinningWeights.size(); ++lod)
		{
			combinedBodySkinningWeights[lod] = SparseMatrix<float>(InCombinedBodySkinWeights[lod].first, numBodyJoints);
			combinedBodySkinningWeights[lod].setFromTriplets(InCombinedBodySkinWeights[lod].second.begin(), InCombinedBodySkinWeights[lod].second.end());
		}

		std::vector<Eigen::Matrix<float, -1, -1>> updatedHeadSkinningWeights;
		UpdateHeadMeshSkinningWeightsFromBody(rigGeometry, combinedBodySkinningWeights, m->neckBodySnapConfig, m->headVertexSkinningWeightsMasks, m->bodyFaceJointMapping, m->faceBodyJointMapping,
			m->barycentricCoordinatesForOddLods, updatedHeadSkinningWeights, m->threadPool);


		int lod = 0;
		for (const auto& denseWeights : updatedHeadSkinningWeights)
		{
			uint16_t headMeshIndex = (uint16_t)rigGeometry.HeadMeshIndex(lod);

			// write updated skinning weights to DNA
			for (int v = 0; v < denseWeights.rows(); ++v) // iterate over vertices in head
			{
				std::vector<float> weights;
				std::vector<uint16_t> indices;

				for (int j = 0; j < denseWeights.cols(); ++j)
				{
					if (std::fabs(denseWeights(v, j)) > std::numeric_limits<float>::min())
					{
						weights.push_back(denseWeights(v, j));
						indices.emplace_back((uint16_t)j);
					}
				}

				InOutDnaWriter->setSkinWeightsValues(headMeshIndex, v, weights.data(), (uint16_t)weights.size());
				InOutDnaWriter->setSkinWeightsJointIndices(headMeshIndex, v, indices.data(), (uint16_t)indices.size());
			}

			lod++;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure updating head skinning weights from body: {}", e.what());
	}
}

int MetaHumanCreatorAPI::SelectVertex(const Eigen::Matrix<float, 3, -1>& Vertices, const Eigen::Vector3f& Origin, const Eigen::Vector3f& Direction) const
{
	std::lock_guard<std::mutex> lock(m->meshQuery.mutex);

	if (!m->meshQuery.aabbTree.get())
	{
		int totalTriangles = 0;
		for (int meshIndex : m->modelMeshIds)
		{
			totalTriangles += m->archetypeTriangulatedMeshes[meshIndex].NumTriangles();
		}
		m->meshQuery.triangles.resize(3, totalTriangles);
		int triOffset = 0;
		for (int meshIndex : m->modelMeshIds)
		{
			const int numTriangles = m->archetypeTriangulatedMeshes[meshIndex].NumTriangles();
			auto range = m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
			m->meshQuery.triangles(Eigen::all, Eigen::seqN(triOffset, numTriangles)) = m->archetypeTriangulatedMeshes[meshIndex].Triangles().array() + range.first;
			triOffset += numTriangles;
		}
		m->meshQuery.aabbTree = std::make_shared<AABBTree<float>>(Vertices.transpose(), m->meshQuery.triangles.transpose());
	}
	else
	{
		m->meshQuery.aabbTree->Update((const float*)Vertices.data(), m->threadPool.get());
	}
	auto [tID, bc, dist] = m->meshQuery.aabbTree->intersectRay(Origin.transpose(), Direction.transpose());
	if (tID >= 0)
	{
		int bestK = 0;
		const Eigen::Matrix3f triangleVertices = Vertices(Eigen::all, m->meshQuery.triangles.col(tID).transpose());
		const Eigen::Vector3f intersection = triangleVertices * bc.transpose();
		const Eigen::RowVector3f distPerVertex = (triangleVertices.colwise() - intersection).colwise().norm();
		distPerVertex.minCoeff(&bestK);
		return m->meshQuery.triangles.col(tID)[bestK];
	}
	return -1;
}

bool MetaHumanCreatorAPI::StateToDna(const State& State, dna::Writer* InOutDnaWriter) const
{
	try
	{
		Eigen::Matrix<float, 3, -1> Vertices(3, NumVertices());
		if (!Evaluate(State, Vertices.data()))
		{
			return false;
		}

		std::vector<Eigen::Matrix3Xf> ModelVertices;
		for (int i = 0; i < m->patchBlendModelDataManipulator->GetNumMeshes(); ++i)
		{
			ModelVertices.push_back(m->patchBlendModelDataManipulator->GetMeshVertices(Vertices, i));
		}

		auto bindPoseJointPositions = m->patchBlendModelDataManipulator->GetJointDeltas(Vertices);

		std::vector<epic::nls::Affine<float, 3, 3>> jointWorldTransforms;
		for (int i = 0; i < m->archetypeFaceGeometry->GetJointRig().NumJoints(); ++i)
		{
			epic::nls::Affine<float, 3, 3> jointWorldTransform(m->archetypeFaceGeometry->GetBindMatrix(i));
			jointWorldTransform.SetTranslation(bindPoseJointPositions.col(i));
			jointWorldTransforms.push_back(jointWorldTransform);
		}

		auto [localRotations, localTranslations] = epic::nls::rigutils::CalculateLocalJointRotationAndTranslation(*m->archetypeFaceGeometry,
			jointWorldTransforms);

		UpdateNeutralGeometry(InOutDnaWriter, ModelVertices, localTranslations);

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure serialize state to dna: {}", e.what());
	}
}

bool TITAN_API MetaHumanCreatorAPI::CopyBodyJointsToFace(const dna::Reader* InBodyDnaReader, const dna::Reader* InFaceDnaReader, dna::Writer* InOutDnaWriter) const
{
	try
	{
		const dna::BinaryStreamReader* BodyImpl = static_cast<const dna::BinaryStreamReader*>(InBodyDnaReader);
		const dna::BinaryStreamReader* FaceImpl = static_cast<const dna::BinaryStreamReader*>(InFaceDnaReader);
		RigGeometry<float> bodyRigGeometry;
		TITAN_CHECK_OR_RETURN(bodyRigGeometry.Init(BodyImpl, true), false, "cannot load rig geometry");
		RigGeometry<float> faceRigGeometry;
		TITAN_CHECK_OR_RETURN(faceRigGeometry.Init(FaceImpl, true), false, "cannot load rig geometry");

		std::vector<Affine<float, 3, 3>> faceJointWorldTransforms;
		std::map<std::string, int> faceJointIndices;
		for (int i = 0; i < faceRigGeometry.GetJointRig().NumJoints(); ++i)
		{
			faceJointWorldTransforms.push_back(Affine<float, 3, 3>(faceRigGeometry.GetBindMatrix(i)));
			faceJointIndices[faceRigGeometry.GetJointRig().GetJointNames()[i]] = i;
		}

		for (int i = 0; i < bodyRigGeometry.GetJointRig().NumJoints(); ++i)
		{
			std::string bodyJointName = bodyRigGeometry.GetJointRig().GetJointNames()[i];
			auto it = faceJointIndices.find(bodyJointName);
			if (it != faceJointIndices.end())
			{
				faceJointWorldTransforms[it->second] = bodyRigGeometry.GetBindMatrix(i);
			}
		}

		Eigen::Matrix3Xf restPose, restOrientationEuler;
		faceRigGeometry.CalculateLocalJointTransformsFromWorldTransforms(faceJointWorldTransforms, restPose, restOrientationEuler);

		faceRigGeometry.SetRestOrientationEuler(restOrientationEuler);
		faceRigGeometry.SetRestPose(restPose);
		faceRigGeometry.SaveBindPoseToDna(InOutDnaWriter);

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure serialize state to dna: {}", e.what());
	}
}

bool TITAN_API MetaHumanCreatorAPI::AddRbfControlsFromReference(dna::Reader* InReferenceDnaReader, dna::Reader* InTargetDnaReader, dna::Writer* InOutDnaWriter) const
{
	try
	{
		rigutils::AddRBFLayerToDnaStream(InTargetDnaReader, InReferenceDnaReader, InOutDnaWriter);

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure serialize state to dna: {}", e.what());
	}
}

MetaHumanCreatorAPI::State::State()
	: m(new Private())
{}

MetaHumanCreatorAPI::State::~State()
{
	delete m;
}

MetaHumanCreatorAPI::State::State(const State& other)
	: m(new Private(*other.m))
{}

std::shared_ptr<MetaHumanCreatorAPI::State> MetaHumanCreatorAPI::State::Clone() const
{
	try
	{
		TITAN_RESET_ERROR;
		return std::shared_ptr<MetaHumanCreatorAPI::State>(new MetaHumanCreatorAPI::State(*this));
	}
	catch (const std::exception& e)
	{
		TITAN_SET_ERROR(-1, fmt::format("failure to clone state: {}", e.what()).c_str());
		return nullptr;
	}
}

int MetaHumanCreatorAPI::State::NumGizmos() const
{
	return (int)m->dmtGizmoData->vertexIndices.size();
}

bool MetaHumanCreatorAPI::State::HasGizmo(int GizmoIndex) const
{
	if ((GizmoIndex < 0) || (GizmoIndex >= (int)m->dmtGizmoData->vertexIndices.size())) { return false; }
	return m->dmtGizmoData->vertexIndices[GizmoIndex] >= 0;
}

bool MetaHumanCreatorAPI::State::EvaluateGizmos(const float* InVertices, float* OutGizmos) const
{
	try
	{
		TITAN_RESET_ERROR;
		int startIndex = m->mhcApi->m->patchBlendModelDataManipulator->NumJoints();
		Eigen::Map<const Eigen::Matrix3Xf> inVertices(InVertices, 3, m->mhcApi->m->patchBlendModel->NumVertices());
		Eigen::Map<Eigen::Matrix3Xf> outGizmos(OutGizmos, 3, NumGizmos());
		for (int gizmoIndex = 0; gizmoIndex < (int)m->dmtGizmoData->vertexIndices.size(); ++gizmoIndex)
		{
			const int vID = m->dmtGizmoData->vertexIndices[gizmoIndex];
			if (vID >= 0)
			{
				outGizmos.col(gizmoIndex) = inVertices.col(startIndex + vID);
			}
			else
			{
				outGizmos.col(gizmoIndex).setZero();
			}
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to evaluate gizmos: {}", e.what());
	}
}

int MetaHumanCreatorAPI::State::NumLandmarks() const
{
	return (int)m->dmtLandmarkData->vertexIndices.size();
}

bool MetaHumanCreatorAPI::State::EvaluateLandmarks(const float* InVertices, float* OutLandmarks) const
{
	try
	{
		TITAN_RESET_ERROR;
		int startIndex = m->mhcApi->m->patchBlendModelDataManipulator->NumJoints();
		for (int landmarkIndex = 0; landmarkIndex < (int)m->dmtLandmarkData->vertexIndices.size(); ++landmarkIndex)
		{
			for (int k = 0; k < 3; ++k)
			{
				OutLandmarks[3 * landmarkIndex + k] = InVertices[3 * (startIndex + m->dmtLandmarkData->vertexIndices[landmarkIndex]) + k];
			}
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to evaluate landmarks: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::FitToTarget(const float* InVertices, int NumVertices)
{
	try
	{
		TITAN_RESET_ERROR;

		TITAN_CHECK_OR_RETURN(InVertices, false, "empty inputy");
		std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>> vertexMap;
		Eigen::Map<const Eigen::Matrix3Xf> VerticesMap(InVertices, 3, NumVertices);
		vertexMap.insert({ 0, VerticesMap });
		FitToTargetOptions options;
		return FitToTarget(vertexMap, options, nullptr);
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to fit to target: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::FitToTarget(const std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>>& InVertices,
	const FitToTargetOptions& options,
	FitToTargetResult* result,
	bool useStabModel)
{
	try
	{
		TITAN_RESET_ERROR;

		// only use vertices that are part of the model
		std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>> ValidTargetVertices;
		int totalNumVertices = 0;
		for (int meshIndex : m->mhcApi->m->modelMeshIds)
		{
			auto it = InVertices.find(meshIndex);
			if (it != InVertices.end())
			{
				auto range = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIndex);
				const int numVerticesInModel = range.second - range.first;
				if (numVerticesInModel != it->second.cols())
				{
					TITAN_SET_ERROR(-1, fmt::format("failure to fit to target. Vertex number mismatch.").c_str());
					return false;
				}
				ValidTargetVertices.insert({ meshIndex, it->second });
				totalNumVertices += numVerticesInModel;
			}
		}

		const int headLod0MeshIndex = 0;
		Eigen::Ref<const Eigen::Matrix<float, 3, -1>> headLod0Vertices = ValidTargetVertices.find(headLod0MeshIndex)->second;
		const int numFaceJoints = m->mhcApi->m->patchBlendModelDataManipulator->NumJoints();

		std::map<int, Eigen::Matrix<float, 3, -1>> canonicalMeshVertices;
		for (const auto& [meshIndex, vertices] : ValidTargetVertices)
		{
			canonicalMeshVertices.insert({ meshIndex, vertices });
		}
		const Eigen::Matrix<float, 3, -1>& archetypeFaceVertices = m->mhcApi->m->faceArchetypeMesh->Vertices();
		float inputToArchetypeScale = 1.0f;
		Affine<float, 3, 3> inputToArchetypeTransform;

		const int numHeadLod0Vertices = (int)headLod0Vertices.cols();
		VertexWeights<float> targetMask(Eigen::VectorXf::Ones(numHeadLod0Vertices));
		auto it = m->mhcApi->m->masks.find(FaceFitMaskName);
		if (it != m->mhcApi->m->masks.end())
		{
			targetMask = it->second;
		}

		if (options.alignmentOptions == AlignmentOptions::None)
		{
			// assumes input mesh is in metric scale at the right position relative to the current body
			if (m->bodyJointPositions && m->mhcApi->m->faceToBodySkinning)
			{
				// get delta of common face/body joints (head, neck_01, neck_02) between input body joints and the face archetype joints
				auto archetypeFaceJoints = m->mhcApi->m->patchBlendModel->BaseVertices().leftCols(numFaceJoints);
				Eigen::Matrix3Xf commonFaceJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromFaceJoints(archetypeFaceJoints);
				Eigen::Matrix3Xf commonBodyJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromBodyJoints(*m->bodyJointPositions);
				Eigen::Matrix3Xf bodyToFaceJointDeltas = commonFaceJoints - commonBodyJoints / m->bodyScale;

				// scale the input vertices by the body scale and move (using skinning) to the archetype
				for (auto&& [meshIndex, vertices] : canonicalMeshVertices)
				{
					vertices = vertices / m->bodyScale;
					m->mhcApi->m->faceToBodySkinning->UpdateVertices(meshIndex, vertices, bodyToFaceJointDeltas);
				}
			}
		}
		else
		{
			const std::vector<int>& vIDs = targetMask.NonzeroVertices();
			NeutralPoseFittingParams<float> stabilizationParams;
			stabilizationParams.numIterations = m->settings->m->fittingSettings.numIterations;
			switch (options.alignmentOptions)
			{
			default:
			case AlignmentOptions::Translation:
			case AlignmentOptions::RotationTranslation:
			{
				const bool withRotation = (options.alignmentOptions == AlignmentOptions::RotationTranslation);
				if (m->mhcApi->m->rigCalibrationModelData->GetStabilizationModel() && useStabModel)
				{
					stabilizationParams.rigidFitOptimizeRotation = withRotation;
					stabilizationParams.rigidFitOptimizeTranslation = true;
					stabilizationParams.rigidFitOptimizeScale = false;
				}
				else
				{
					inputToArchetypeTransform = Procrustes<float, 3>::AlignRigid(headLod0Vertices(Eigen::all, vIDs), archetypeFaceVertices(Eigen::all, vIDs), withRotation);
				}
			}
			break;
			case AlignmentOptions::ScalingTranslation:
			case AlignmentOptions::ScalingRotationTranslation:
			{
				const bool withRotation = (options.alignmentOptions == AlignmentOptions::ScalingRotationTranslation);
				if (m->mhcApi->m->rigCalibrationModelData->GetStabilizationModel() && useStabModel)
				{
					stabilizationParams.rigidFitOptimizeRotation = withRotation;
					stabilizationParams.rigidFitOptimizeTranslation = true;
					stabilizationParams.rigidFitOptimizeScale = true;
				}
				else
				{
					std::tie(inputToArchetypeScale, inputToArchetypeTransform) = Procrustes<float, 3>::AlignRigidAndScale(headLod0Vertices(Eigen::all, vIDs), archetypeFaceVertices(Eigen::all, vIDs), withRotation);
				}
			}
			break;
			}

			if (m->mhcApi->m->rigCalibrationModelData->GetStabilizationModel() && useStabModel)
			{
				Eigen::VectorXf resultParams;
				float modelToTargetScale = 1.f;
				Affine<float, 3, 3> modelToTargetTransform;
				std::tie(modelToTargetScale, modelToTargetTransform) = NeutralPoseFittingOptimization<float>::RegisterPose(headLod0Vertices,
					m->mhcApi->m->rigCalibrationModelData->GetStabilizationModel(),
					stabilizationParams,
					targetMask.Weights(),
					resultParams);
				// scale from RegisterPose is used as scale * (R * vtx + t) but the code below expects (R * (scale * vtx) + t)
				modelToTargetTransform.SetTranslation(modelToTargetTransform.Translation() * modelToTargetScale);

				// model to target
				// v' = R * (s * v) + t
				// however, we want target in model space
				// v = 1/s * R.t() * v' - 1/s * R.t() * t
				// hence the translational part of this inverse is -1/s * R.t() * t
				inputToArchetypeScale = 1.f / modelToTargetScale;
				inputToArchetypeTransform.SetLinear(modelToTargetTransform.Linear().transpose());
				inputToArchetypeTransform.SetTranslation((-1.f) * inputToArchetypeScale * (modelToTargetTransform.Linear().transpose() * modelToTargetTransform.Translation()));
			}

			for (auto&& [meshIndex, vertices] : canonicalMeshVertices)
			{
				vertices = inputToArchetypeTransform.Transform(inputToArchetypeScale * vertices);
			}
		}

		const auto& meshIds = m->mhcApi->m->modelMeshIds;
		std::vector<int> numVerticesPerMesh(meshIds.size());
		for (size_t meshIdx = 0; meshIdx < meshIds.size(); ++meshIdx)
		{
			const auto curMeshRange = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(meshIds[meshIdx]);
			numVerticesPerMesh[meshIdx] = curMeshRange.second - curMeshRange.first;
		}

		Eigen::VectorXf targetMaskWeights = Eigen::VectorXf::Ones(m->mhcApi->m->faceTeethEyesPatchBlendModel->NumVertices());
		// always add the face mask if it exists
		it = m->mhcApi->m->masks.find(FaceFitMaskName);
		if (it != m->mhcApi->m->masks.end())
		{
			const auto& mask = it->second;
			targetMaskWeights.head(mask.NumVertices()) = mask.Weights();

		}
		VertexWeights<float> targetMaskFaceTeethEyes(targetMaskWeights);

		std::vector<int> vtxIds(totalNumVertices);
		Eigen::Matrix<float, 3, -1> allVertices(3, totalNumVertices);
		int totalUsed = 0;
		int totalAll = 0;
		for (size_t meshIdx = 0; meshIdx < meshIds.size(); ++meshIdx)
		{
			auto meshVertIt = canonicalMeshVertices.find(meshIds[meshIdx]);
			if (meshVertIt != canonicalMeshVertices.end())
			{
				std::iota(vtxIds.begin() + totalUsed, vtxIds.end(), totalAll);
				allVertices.block(0, totalUsed, 3, numVerticesPerMesh[meshIdx]) = meshVertIt->second.block(0, 0, 3, numVerticesPerMesh[meshIdx]);
				totalUsed += numVerticesPerMesh[meshIdx];
			}
			totalAll += numVerticesPerMesh[meshIdx];
		}

		Eigen::VectorXi targetVtxIds = Eigen::Map<Eigen::VectorXi>(vtxIds.data(), vtxIds.size());

		NeutralPoseFittingParams<float> params;
		params.numIterations = m->settings->m->fittingSettings.numIterations;
		params.fixedRegion = m->settings->m->fittingSettings.fixedRegion;
		params.modelFitOptimizeRigid = false;

		auto patchBlendModelOptimizationState = m->mhcApi->m->faceTeethEyesPatchBlendModel->CreateOptimizationState();

		Affine<float, 3, 3> modelToTargetRigid;
		modelToTargetRigid = NeutralPoseFittingOptimization<float>::RegisterPose(allVertices,
			targetVtxIds,
			modelToTargetRigid,
			m->mhcApi->m->faceTeethEyesPatchBlendModel,
			patchBlendModelOptimizationState,
			params,
			targetMaskFaceTeethEyes.Weights());


		auto newState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		patchBlendModelOptimizationState.CopyToState(*newState);

		m->faceScale = 1.0f;
		m->combinedScale = m->bodyScale;
		m->UpdateBodyDeltas();

		// update vertex deltas
		Private::VertexDeltaCalculationMethod method = options.adaptNeck ? Private::VertexDeltaCalculationMethod::Adapt : Private::VertexDeltaCalculationMethod::Exact;
		m->UpdateVertexDeltas(newState, canonicalMeshVertices, method);

		if (m->bodyJointPositions)
		{
			m->combinedState = newState;
			m->UpdateFaceState();
		}
		else
		{
			m->faceState = newState;
		}

		// if (!options.adaptNeck && m->bodyDeltas)
		// {
		//     // verify that the input dna is perfectly reproduced
		//     Eigen::Matrix<float, 3, -1> testVertices;
		//     m->mhcApi->Evaluate(*this, testVertices);
		//     auto faceRange = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(headLod0MeshIndex);
		//     Eigen::Matrix3Xf diff = headLod0Vertices - testVertices.block(0, faceRange.first, 3, faceRange.second - faceRange.first);
		//     LOG_INFO("diff: {} {}", diff.norm(), diff.maxCoeff());
		// }

		if (result)
		{
			// record scale and translation of the target
			if (m->bodyJointPositions && m->mhcApi->m->faceToBodySkinning && m->mhcApi->m->faceToBodySkinning->MainFaceJointIndex() >= 0 && options.alignmentOptions != AlignmentOptions::None)
			{
				auto archetypeFaceJoints = m->mhcApi->m->patchBlendModel->BaseVertices().leftCols(numFaceJoints);
				Eigen::Matrix3Xf commonFaceJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromFaceJoints(archetypeFaceJoints);
				Eigen::Matrix3Xf commonBodyJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromBodyJoints(*m->bodyJointPositions);
				const int mainFaceJointIndex = m->mhcApi->m->faceToBodySkinning->MainFaceJointIndex();
				Eigen::Vector3f offset = commonBodyJoints.col(mainFaceJointIndex) - m->bodyScale * commonFaceJoints.col(mainFaceJointIndex);
				result->scale = inputToArchetypeScale * m->bodyScale;
				Affine<float, 3, 3> transform = inputToArchetypeTransform;
				transform.SetTranslation(transform.Translation() * m->bodyScale);
				result->transform = (Affine<float, 3, 3>::FromTranslation(offset) * transform).Matrix();
			}
			else
			{
				result->scale = 1.0f;
				result->transform.setIdentity();
			}
		}

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to fit to target: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::FitToTarget(const dna::Reader* reader, const FitToTargetOptions& options, FitToTargetResult* result)
{
	try
	{
		TITAN_RESET_ERROR;

		RigGeometry<float> rigGeometry;
		TITAN_CHECK_OR_RETURN(rigGeometry.Init(reader, true), false, "cannot load rig geometry");
		std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>> vertexMap;
		for (auto meshIndex : m->mhcApi->m->modelMeshIds)
		{
			vertexMap.insert({ meshIndex, rigGeometry.GetMesh(meshIndex).Vertices() });
		}

		return FitToTarget(vertexMap, options, result);
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to fit to target: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::AdaptNeck()
{
	try
	{
		TITAN_RESET_ERROR;
		auto state = m->combinedState ? m->combinedState : m->faceState;
		if (state->HasPatchVertexDeltas())
		{
			auto vertexDeltas = state->EvaluateVertexDeltas(*m->mhcApi->m->patchBlendModel);

			// blend the vertex delta to zero in the neck region
			const int offset = m->mhcApi->m->patchBlendModelDataManipulator->NumJoints();
			auto it = m->mhcApi->m->masks.find(BodyBlendMaskName);
			if (it != m->mhcApi->m->masks.end())
			{
				const auto& mask = it->second;
				bool neckSeamIsNonZero = false;
				for (int vID : mask.NonzeroVertices())
				{
					if (mask.Weights()[vID] >= 1.0f && vertexDeltas.col(offset + vID).squaredNorm() > 0)
					{
						neckSeamIsNonZero = true;
						break;
					}
				}
				if (neckSeamIsNonZero)
				{
					for (int vID : mask.NonzeroVertices())
					{
						vertexDeltas.col(offset + vID) *= 1.0f - it->second.Weights()[vID];
					}
				}
			}

			auto newState = std::make_shared<PatchBlendModel<float>::State>(*state);
			newState->BakeVertexDeltas(vertexDeltas, *m->mhcApi->m->patchBlendModel);
			if (m->combinedState)
			{
				m->combinedState = newState;
				m->UpdateFaceState();
			}
			else
			{
				m->faceState = newState;
			}
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to reset state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Reset(bool resetBody)
{
	try
	{
		TITAN_RESET_ERROR;
		m->faceState = m->mhcApi->m->defaultState->m->faceState;
		if (resetBody)
		{
			m->combinedState.reset();
			m->bodyState.reset();
			m->bodyJointPositions.reset();
			m->bodyDeltas.reset();
			m->canoncialBodyVertices.reset();
		}
		else
		{
			m->UpdateCombinedState();
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to reset state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::ResetRegion(int GizmoIndex, float alpha, const BlendOptions& InBlendOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		return Blend(GizmoIndex, { { alpha, m->mhcApi->m->defaultState.get() } }, InBlendOptions);
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to reset state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::BlendPresets(int GizmoIndex, const std::vector<std::pair<float, std::string>>& alphaAndPresetNames, const BlendOptions& InBlendOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		std::vector<std::pair<float, const State*>> alphaAndStates;
		for (const auto& [alpha, name] : alphaAndPresetNames)
		{
			auto it = m->mhcApi->m->presets.find(name);
			if (it != m->mhcApi->m->presets.end())
			{
				alphaAndStates.push_back({ alpha, it->second.get() });
			}
		}
		return Blend(GizmoIndex, alphaAndStates, InBlendOptions);
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to blend presets: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Serialize(std::string& OutArchive) const
{
	try
	{
		TITAN_RESET_ERROR;
		OutArchive.clear();
		JsonElement StateJson(JsonElement::JsonType::Object);

		int Version = 2;
		StateJson.Insert("Version", JsonElement(Version));
		StateJson.Insert("ModelVersionIdentifier", JsonElement(m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier()));

		auto PcaWeights = m->faceState->ConcatenatePatchPcaWeights();
		StateJson.Insert("PcaWeights", io::ToJson(PcaWeights));

		JsonElement PatchesJson(JsonElement::JsonType::Object);

		for (int PatchIndex = 0; PatchIndex < m->faceState->NumPatches(); ++PatchIndex)
		{
			JsonElement PatchJson(JsonElement::JsonType::Object);
			Eigen::Vector3f Position = m->faceState->PatchTranslation(PatchIndex);
			Eigen::Vector3f Rotation = m->faceState->PatchRotationEulerDegrees(PatchIndex);

			PatchJson.Insert("Position", io::ToJson(Position));
			PatchJson.Insert("Rotation", io::ToJson(Rotation));

			Eigen::Matrix3Xf VertexDeltas = m->faceState->PatchVertexDeltas(PatchIndex);
			if (VertexDeltas.size() > 0)
			{
				PatchJson.Insert("VertexDeltas", io::ToJson(VertexDeltas));
			}

			const std::string PatchName = std::to_string(PatchIndex);
			PatchesJson.Insert(PatchName, std::move(PatchJson));
		}

		StateJson.Insert("Patches", std::move(PatchesJson));

		const Eigen::Matrix3Xf ModelVertices = m->mhcApi->m->patchBlendModel->DeformedVertices(*m->faceState);
		StateJson.Insert("ModelVertices", io::ToJson(ModelVertices));

		Eigen::VectorXi LandmarkIndices = Eigen::VectorXi::Zero(m->dmtLandmarkData->vertexIndices.size());
		for (int i = 0; i < (int)LandmarkIndices.size(); ++i)
		{
			LandmarkIndices[i] = m->dmtLandmarkData->vertexIndices[i];
		}

		StateJson.Insert("LandmarkIndices", io::ToJson(LandmarkIndices));

		JsonElement SettingsJson(JsonElement::JsonType::Object);
		SettingsJson.Insert("VertexDeltaScale", JsonElement(m->settings->GlobalVertexDeltaScale()));
		SettingsJson.Insert("DmtSymmetry", JsonElement(m->settings->DmtWithSymmetry()));

		StateJson.Insert("Settings", std::move(SettingsJson));

		OutArchive = WriteJson(StateJson, -1);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to serialize state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Deserialize(const std::string& InArchive)
{
	try
	{
		TITAN_RESET_ERROR;
		if (InArchive.empty())
		{
			return false;
		}

		auto StateJson = ReadJson(InArchive);

		auto newState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		auto newDmtLandmarkData = std::make_shared<DmtLandmarkData>(*m->dmtLandmarkData);

		int Version = 0;

		if (StateJson.Contains("Version"))
		{
			Version = StateJson["Version"].template Get<int>();
		}
		else
		{
			return false;
		}

		if (Version != 2)
		{
			return false;
		}

		if (StateJson.Contains("ModelVersionIdentifier"))
		{
			const std::string& expectedModelIdentifier = m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier();
			if (StateJson["ModelVersionIdentifier"].String() != expectedModelIdentifier)
			{
				CARBON_CRITICAL("state contains wrong model version: \"{}\" instead of \"{}\"", StateJson["ModelVersionIdentifier"].String(), expectedModelIdentifier);
			}
		}
		else
		{
			CARBON_CRITICAL("state does not contain model version");
		}


		Eigen::VectorXf PcaWeights;
		if (StateJson.Contains("PcaWeights"))
		{
			io::FromJson(StateJson["PcaWeights"], PcaWeights);
		}
		else
		{
			return false;
		}
		newState->SetConcatenatedPatchPcaWeights(PcaWeights);

		if (StateJson.Contains("Patches"))
		{
			const JsonElement& PatchesJson = StateJson["Patches"];
			for (auto&& [PatchName, PatchJsonData] : PatchesJson.Map())
			{
				int PatchId = std::stoi(PatchName);

				Eigen::Vector3f Position;
				Eigen::Vector3f Rotation;

				io::FromJson(PatchJsonData["Position"], Position);
				io::FromJson(PatchJsonData["Rotation"], Rotation);

				newState->SetPatchTranslation(PatchId, Position);
				newState->SetPatchRotationEulerDegrees(PatchId, Rotation);
				if (PatchJsonData.Contains("VertexDeltas"))
				{
					Eigen::Matrix3Xf VertexDeltas;
					io::FromJson(PatchJsonData["VertexDeltas"], VertexDeltas);
					newState->SetPatchVertexDeltas(PatchId, VertexDeltas);
				}
			}
		}
		else
		{
			return false;
		}

		Eigen::VectorXi LandmarkIndicesEigen;
		if (StateJson.Contains("LandmarkIndices"))
		{
			io::FromJson(StateJson["LandmarkIndices"], LandmarkIndicesEigen);
		}
		else
		{
			return false;
		}

		std::vector<int> LandmarkIndices(LandmarkIndicesEigen.size());
		for (int i = 0; i < (int)LandmarkIndicesEigen.size(); ++i)
		{
			LandmarkIndices[i] = LandmarkIndicesEigen[i];
		}

		auto LocalSettings = m->settings->Clone();

		if (StateJson.Contains("Settings"))
		{
			if (StateJson["Settings"].Contains("VertexDeltaScale"))
			{
				LocalSettings->SetGlobalVertexDeltaScale(StateJson["Settings"]["VertexDeltaScale"].Get<float>());
			}
			if (StateJson["Settings"].Contains("DmtSymmetry"))
			{
				LocalSettings->SetDmtWithSymmetry(StateJson["Settings"]["DmtSymmetry"].Boolean());
			}
		}

		m->settings = LocalSettings;

		newDmtLandmarkData->vertexIndices = LandmarkIndices;
		newDmtLandmarkData->symmetries = GetSymmetricIndices(newDmtLandmarkData->vertexIndices, m->mhcApi->m->symmetries);
		m->faceState = newState;

		m->dmtLandmarkData = newDmtLandmarkData;
		auto newDmtModel = m->mhcApi->m->defaultState->m->dmtModel->Clone();
		newDmtModel->Init(newDmtLandmarkData->vertexIndices,
			m->mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
			m->settings->m->dmtSettings.singleRegionPerLandmark,
			m->settings->m->dmtSettings.dmtRegularization);
		m->dmtModel = newDmtModel;

		m->UpdateCombinedState();

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to de-serialize state: {}", e.what());
	}
}


bool MetaHumanCreatorAPI::State::Serialize(trio::BoundedIOStream* OutputStream) const
{
	try
	{
		TITAN_RESET_ERROR;

		MHCBinaryOutputArchive archive{ OutputStream };
		using SizeType = MHCBinaryOutputArchive::SizeType;

		int32_t Version = 1;
		archive(m->MagicNumber);
		archive(Version);
		archive(m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier());

		SizeType numPatches = m->faceState->NumPatches();
		archive(numPatches);

		for (int PatchIndex = 0; PatchIndex < m->faceState->NumPatches(); ++PatchIndex)
		{
			archive(m->faceState->PatchScale(PatchIndex));
			SerializeEigenMatrix(archive, OutputStream, m->faceState->PatchTranslation(PatchIndex));
			SerializeEigenMatrix(archive, OutputStream, m->faceState->PatchRotation(PatchIndex).coeffs());
			SerializeEigenMatrix(archive, OutputStream, m->faceState->PatchPcaWeights(PatchIndex));
			SerializeEigenMatrix(archive, OutputStream, m->faceState->PatchVertexDeltas(PatchIndex));
		}

		const Eigen::Matrix3Xf ModelVertices = m->mhcApi->m->patchBlendModel->DeformedVertices(*m->faceState);
		SerializeEigenMatrix(archive, OutputStream, ModelVertices);

		archive(m->dmtLandmarkData->vertexIndices);

		archive(m->settings->GlobalVertexDeltaScale());
		archive(m->settings->DmtWithSymmetry());

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to serialize state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Deserialize(trio::BoundedIOStream* InputStream)
{
	try
	{
		TITAN_RESET_ERROR;
		if (!InputStream)
		{
			return false;
		}

		MHCBinaryInputArchive archive{ InputStream };
		using SizeType = MHCBinaryOutputArchive::SizeType;

		int32_t Version = { -1 };
		int32_t MagicNumber = { -1 };
		archive(MagicNumber);
		if (MagicNumber != m->MagicNumber)
		{
			LOG_WARNING("stream does not contain a MHC state");
			return false;
		}
		archive(Version);
		if (Version != 1)
		{
			LOG_ERROR("version {} is not supported", Version);
			return false;
		}

		auto newState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		auto newDmtLandmarkData = std::make_shared<DmtLandmarkData>(*m->dmtLandmarkData);

		std::string modelVersionIdentifier;
		archive(modelVersionIdentifier);
		const std::string& expectedModelIdentifier = m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier();
		const bool isExpectedModelVersionIdentifier = (modelVersionIdentifier == expectedModelIdentifier);
		if (!isExpectedModelVersionIdentifier)
		{
			CARBON_CRITICAL("state contains wrong model version: \"{}\" instead of \"{}\"", modelVersionIdentifier, expectedModelIdentifier);
		}

		SizeType numPatches;
		archive(numPatches);
		if (numPatches != (SizeType)m->faceState->NumPatches())
		{
			CARBON_CRITICAL("invalid number of patches: expected {}, but got {}", m->faceState->NumPatches(), numPatches);
		}

		for (int PatchIndex = 0; PatchIndex < m->faceState->NumPatches(); ++PatchIndex)
		{
			float scale;
			Eigen::Vector3f translation;
			Eigen::Quaternionf rotation;
			Eigen::VectorXf pcaWeights;
			Eigen::Matrix3Xf vertexDeltas;
			archive(scale);
			DeserializeEigenMatrix(archive, InputStream, translation);
			DeserializeEigenMatrix(archive, InputStream, rotation.coeffs());
			DeserializeEigenMatrix(archive, InputStream, pcaWeights);
			DeserializeEigenMatrix(archive, InputStream, vertexDeltas);
			if (isExpectedModelVersionIdentifier)
			{
				newState->SetPatchScale(PatchIndex, scale);
				newState->SetPatchTranslation(PatchIndex, translation);
				newState->SetPatchRotation(PatchIndex, rotation);
				newState->SetPatchPcaWeights(PatchIndex, pcaWeights);
				newState->SetPatchVertexDeltas(PatchIndex, vertexDeltas);
			}
		}

		Eigen::Matrix3Xf ModelVertices;
		DeserializeEigenMatrix(archive, InputStream, ModelVertices);
		if (!isExpectedModelVersionIdentifier)
		{
			// serialize state does not contain data of this model
			// TODO: fit data
		}

		archive(newDmtLandmarkData->vertexIndices);
		newDmtLandmarkData->symmetries = GetSymmetricIndices(newDmtLandmarkData->vertexIndices, m->mhcApi->m->symmetries);

		auto LocalSettings = m->settings->Clone();
		float GlobalVertexDeltaScale{};
		archive(GlobalVertexDeltaScale);
		bool DmtWithSymmetry{};
		archive(DmtWithSymmetry);

		LocalSettings->SetGlobalVertexDeltaScale(GlobalVertexDeltaScale);
		LocalSettings->SetDmtWithSymmetry(DmtWithSymmetry);

		m->settings = LocalSettings;

		m->faceState = newState;
		m->dmtLandmarkData = newDmtLandmarkData;
		auto newDmtModel = m->mhcApi->m->defaultState->m->dmtModel->Clone();
		newDmtModel->Init(newDmtLandmarkData->vertexIndices,
			m->mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
			m->settings->m->dmtSettings.singleRegionPerLandmark,
			m->settings->m->dmtSettings.dmtRegularization);
		m->dmtModel = newDmtModel;

		m->UpdateCombinedState();

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to de-serialize state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Randomize(float magnitude)
{
	try
	{
		TITAN_RESET_ERROR;
		auto newState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(-magnitude, magnitude);
		for (int r = 0; r < newState->NumPatches(); ++r)
		{
			newState->SetPatchRotationEulerDegrees(r, Eigen::Vector3f((float)dis(gen), (float)dis(gen), (float)dis(gen)) / 10.0f);
			newState->SetPatchTranslation(r,
				m->mhcApi->m->patchBlendModel->PatchCenterOfGravity(r) + Eigen::Vector3f((float)dis(gen), (float)dis(gen), (float)dis(
					gen)) / 10.0f);
			auto weights = newState->PatchPcaWeights(r);
			for (int i = 0; i < weights.size(); ++i)
			{
				weights[i] = (float)dis(gen);
			}
			newState->SetPatchPcaWeights(r, weights);
		}

		m->faceState = newState;
		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to randomize state: {}", e.what());
	}
}

const std::shared_ptr<const MetaHumanCreatorAPI::Settings>& MetaHumanCreatorAPI::State::GetSettings() const
{
	return m->settings;
}

void MetaHumanCreatorAPI::State::SetSettings(const std::shared_ptr<MetaHumanCreatorAPI::Settings>& settings) { m->settings = settings; }

bool MetaHumanCreatorAPI::State::SetExpressionActivations(const std::map<std::string, float>& expressionActivations)
{
	try
	{
		TITAN_RESET_ERROR;

		if (expressionActivations.empty())
		{
			m->expressionActivations.reset();
			return true;
		}

		// if no calibrated parameters, calibrate
		if (m->calibratedModelParameters == nullptr)
		{
			if (!Calibrate())
			{
				return false;
			}
		}

		// make sure only valid expressions are used
		std::shared_ptr<std::map<std::string, float>> cleanExpressionActivations = std::make_shared<std::map<std::string, float>>();
		for (const auto& [name, activation] : expressionActivations)
		{
			if ((activation > 0) && (name != m->mhcApi->m->rigCalibrationModelData->GetNeutralName()))
			{
				auto exprIt = m->calibratedModelParameters->find(name);
				if (exprIt != m->calibratedModelParameters->end())
				{
					(*cleanExpressionActivations)[name] = activation;
				}
			}
		}
		m->expressionActivations = cleanExpressionActivations;

		if (cleanExpressionActivations->empty())
		{
			return true;
		}

		// if neutral parameters changed, calibrate
		auto neutralIt = m->calibratedModelParameters->find(m->mhcApi->m->rigCalibrationModelData->GetNeutralName());
		if (neutralIt != m->calibratedModelParameters->end())
		{
			if (neutralIt->second != m->State()->ConcatenatePatchPcaWeights())
			{
				if (!Calibrate())
				{
					return false;
				}
			}
		}
		else
		{
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set expression: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::ResetNeckExclusionMask()
{
	try
	{
		TITAN_RESET_ERROR;

		auto it = m->mhcApi->m->masks.find(FaceFitMaskName);
		if (it != m->mhcApi->m->masks.end())
		{
			it->second = VertexWeights<float>(it->second.NumVertices(), 1);
		}

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to reset neck exclusion mask: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SelectPreset(int GizmoIndex, const std::string& PresetName, const BlendOptions& InBlendOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		auto it = m->mhcApi->m->presets.find(PresetName);
		if (it == m->mhcApi->m->presets.end())
		{
			return false;
		}
		else
		{
			return Blend(GizmoIndex, { { 1.0f, it->second.get() } }, InBlendOptions);
		}
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to select preset: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Blend(int GizmoIndex, const std::vector<std::pair<float, const State*>>& states, const BlendOptions& InBlendOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		if (states.empty()) { return true; }
		if (!m->faceState) { return false; }

		float totalAlpha = 0;
		for (const auto& [alpha, _] : states)
		{
			totalAlpha += alpha;
		}
		if (totalAlpha == 0) { return true; }

		auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);

		const bool blendAll = (GizmoIndex < 0) || (GizmoIndex >= newFaceState->NumPatches());

		auto relativePatchTranslation = [&](const PatchBlendModel<float>::State& state, int regionId) -> std::pair<Eigen::Vector3f, Eigen::Vector3f>
		{
				Eigen::Vector3f Center = Eigen::Vector3f::Zero();
				int numRegions = 0;
				for (int i = 0; i < (int)m->mhcApi->m->isRegionNeighbor[regionId].size(); ++i)
				{
					if (m->mhcApi->m->isRegionNeighbor[regionId][i])
					{
						Center += state.PatchTranslation(i);
						numRegions++;
					}
				}
				Center /= (float)numRegions;
				Eigen::Vector3f Offset = state.PatchTranslation(regionId) - Center;
				return { Center, Offset };
		};


		auto updateNewState = [&](int regionId) {
			const Eigen::VectorXf faceCoeffs = m->faceState->PatchPcaWeights(regionId);
			const Eigen::Vector3f faceTranslation = m->faceState->PatchTranslation(regionId);
			const auto [relativeFaceOrigin, relativeFaceOffset] = relativePatchTranslation(*m->faceState, regionId);
			Eigen::Vector3f translation = faceTranslation;
			const float faceScale = m->faceState->PatchScale(regionId);
			Eigen::VectorXf coeffs = faceCoeffs;
			std::vector<Eigen::Quaternionf> qs;
			std::vector<float> qsWeights;
			float scale = faceScale;
			Eigen::Matrix<float, 3, -1> vertexDeltas = Eigen::Matrix<float, 3, -1>::Zero(3, m->mhcApi->m->patchBlendModel->NumVerticesForPatch(regionId));
			if (m->faceState->HasPatchVertexDeltas(regionId))
			{
				vertexDeltas = m->faceState->PatchVertexDeltas(regionId);
			}
			const Eigen::Matrix<float, 3, -1> initVertexDeltas = vertexDeltas;

			for (int k = 0; k < (int)states.size(); ++k)
			{
				const auto& [alpha, state] = states[k];
				auto delta = (state->m->faceState->PatchPcaWeights(regionId) - faceCoeffs);
				coeffs += alpha * delta;
				qs.push_back(state->m->faceState->PatchRotation(regionId));
				qsWeights.push_back(alpha);
				if (InBlendOptions.bBlendRelativeTranslation)
				{
					const auto [patchRelativeFaceOrigin, patchRelativeFaceOffset] = relativePatchTranslation(*state->m->faceState, regionId);
					translation += alpha * (relativeFaceOrigin + patchRelativeFaceOffset - faceTranslation);
				}
				else
				{
					translation += alpha * (state->m->faceState->PatchTranslation(regionId) - faceTranslation);
				}
				scale += alpha * (state->m->faceState->PatchScale(regionId) - faceScale);
				if (state->m->faceState->HasPatchVertexDeltas(regionId))
				{
					float vertexDeltaScale = state->m->settings->GlobalVertexDeltaScale();
					if (regionId < (int)state->m->settings->m->evaluationSettings.regionVertexDeltaScales.size())
					{
						vertexDeltaScale *= state->m->settings->m->evaluationSettings.regionVertexDeltaScales[regionId];
					}
					vertexDeltas += alpha * (vertexDeltaScale * state->m->faceState->PatchVertexDeltas(regionId) - initVertexDeltas);
				}
				else
				{
					vertexDeltas -= alpha * initVertexDeltas;
				}
			}

			Eigen::Quaternionf q = WeightedQuaternionAverage<float>(qs, qsWeights);
			if ((InBlendOptions.Type == FaceAttribute::Both) || (InBlendOptions.Type == FaceAttribute::Features))
			{
				newFaceState->SetPatchPcaWeights(regionId, coeffs);
				if (vertexDeltas.squaredNorm() > 0)
				{
					newFaceState->SetPatchVertexDeltas(regionId, vertexDeltas);
				}
				else
				{
					newFaceState->ResetPatchVertexDeltas(regionId);
				}
			}
			if ((InBlendOptions.Type == FaceAttribute::Both) || (InBlendOptions.Type == FaceAttribute::Proportions))
			{
				newFaceState->SetPatchRotation(regionId, newFaceState->PatchRotation(regionId).slerp(totalAlpha, q));
				newFaceState->SetPatchTranslation(regionId, translation);
				newFaceState->SetPatchScale(regionId, scale);
			}
			};

		std::vector<bool> patchesBlended(newFaceState->NumPatches(), false);

		if (blendAll)
		{
			for (int r = 0; r < newFaceState->NumPatches(); ++r)
			{
				if (r != m->mhcApi->GetNeckRegionIndex())
				{
					updateNewState(r);
					patchesBlended[r] = true;
				}
			}
		}
		else
		{
			updateNewState(GizmoIndex);
			patchesBlended[GizmoIndex] = true;
			if (m->dmtGizmoData->HasSymmetry() && InBlendOptions.bBlendSymmetrically)
			{
				const int symmetricIndex = m->dmtGizmoData->symmetries[GizmoIndex];
				if ((symmetricIndex >= 0) && (symmetricIndex != GizmoIndex))
				{
					updateNewState(symmetricIndex);
					patchesBlended[symmetricIndex] = true;
				}
			}
		}

		m->faceState = newFaceState;

		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to reset state: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::TranslateGizmo(int GizmoIndex, const float DeltaXYZ[3], bool bInSymmetric)
{
	TranslateGizmoOptions translateGizmoOptions;
	translateGizmoOptions.bSymmetric = bInSymmetric;
	return TranslateGizmo(GizmoIndex, DeltaXYZ, translateGizmoOptions);
}

bool MetaHumanCreatorAPI::State::TranslateGizmo(int InGizmoIndex, const float InDeltaXYZ[3], const TranslateGizmoOptions& InTranslateGizmoOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }

		auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);

		Eigen::Vector3f CurrentPosition = newFaceState->PatchTranslation(InGizmoIndex);
		Eigen::Vector3f DeltaXYZ = Eigen::Map<const Eigen::Vector3f>(InDeltaXYZ);
		Eigen::Vector3f NewPosition = CurrentPosition + DeltaXYZ;

		if (InTranslateGizmoOptions.bEnforceBounds)
		{
			Eigen::Vector3f MinPosition;
			Eigen::Vector3f MaxPosition;
			GetGizmoPositionBounds(InGizmoIndex, MinPosition.data(), MaxPosition.data(), InTranslateGizmoOptions.BBoxReduction, /*bInExpandToCurrent*/ true);
			Eigen::Vector3f NewBoundedPosition = NewPosition.array().max(MinPosition.array()).min(MaxPosition.array());
			Eigen::Vector3f BoundDelta = NewPosition - NewBoundedPosition;
			NewPosition = NewBoundedPosition.array() += 2.0f / (1.0f + (-2.0f * BoundDelta.array() * InTranslateGizmoOptions.BBoxSoftBound).exp()) - 1.0f;
		}

		newFaceState->SetPatchTranslation(InGizmoIndex, NewPosition);
		if (InTranslateGizmoOptions.bSymmetric && m->dmtGizmoData->HasSymmetry())
		{
			newFaceState->SymmetricRegionCopy(m->dmtGizmoData->symmetries, InGizmoIndex, /*includingPcaWeights=*/false);
		}

		m->faceState = newFaceState;
		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to translate gizmo: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetGizmoPosition(int InGizmoIndex, const float InPosition[3], const GizmoPositionOptions& InGizmoPositionOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }

		Eigen::Vector3f Position = Eigen::Map<const Eigen::Vector3f>(InPosition);
		if (InGizmoPositionOptions.bEnforceBounds)
		{
			Eigen::Vector3f minPosition;
			Eigen::Vector3f maxPosition;
			GetGizmoPositionBounds(InGizmoIndex, minPosition.data(), maxPosition.data(), InGizmoPositionOptions.BBoxReduction, /*bInExpandToCurrent*/ true);
			Position.array() = Position.array().max(minPosition.array()).min(maxPosition.array());
		}

		auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		newFaceState->SetPatchTranslation(InGizmoIndex, Position);
		if (InGizmoPositionOptions.bSymmetric && m->dmtGizmoData->HasSymmetry())
		{
			newFaceState->SymmetricRegionCopy(m->dmtGizmoData->symmetries, InGizmoIndex, /*includingPcaWeights=*/false);
		}

		m->faceState = newFaceState;
		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set gizmo rotation: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoPosition(int InGizmoIndex, float OutPosition[3]) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }
		Eigen::Map<Eigen::Vector3f> Position(OutPosition);
		Position = m->faceState->PatchTranslation(InGizmoIndex);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get gizmo translation: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoPositionBounds(int InGizmoIndex, float OutMinPosition[3], float OutMaxPosition[3], float InBBoxReduction, bool bInExpandToCurrent) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }
		Eigen::Vector3f MinPosition = m->mhcApi->m->regionTranslationRanges[InGizmoIndex].first;
		Eigen::Vector3f MaxPosition = m->mhcApi->m->regionTranslationRanges[InGizmoIndex].second;
		Eigen::Vector3f Delta = MaxPosition - MinPosition;
		float BBoxReduction = std::min(InBBoxReduction, 0.5f);
		MinPosition += Delta * BBoxReduction;
		MaxPosition -= Delta * BBoxReduction;

		Eigen::Map<Eigen::Vector3f> MapMinPosition(OutMinPosition);
		Eigen::Map<Eigen::Vector3f> MapMaxPosition(OutMaxPosition);
		if (bInExpandToCurrent)
		{
			Eigen::Vector3f CurrentPosition = m->faceState->PatchTranslation(InGizmoIndex);
			MapMinPosition = CurrentPosition.array().min(MinPosition.array());
			MapMaxPosition = CurrentPosition.array().max(MaxPosition.array());
		}
		else
		{
			MapMinPosition = MinPosition;
			MapMaxPosition = MaxPosition;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get translate gizmo bounds: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetGizmoRotation(int InGizmoIndex, const float InEulers[3], bool bInSymmetric)
{
	GizmoRotationOptions Options;
	Options.bSymmetric = bInSymmetric;
	return SetGizmoRotation(InGizmoIndex, InEulers, Options);
}

bool MetaHumanCreatorAPI::State::SetGizmoRotation(int InGizmoIndex, const float InEulers[3], const GizmoRotationOptions& InGizmoRotationOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }

		Eigen::Vector3f Eulers = Eigen::Map<const Eigen::Vector3f>(InEulers);
		if (InGizmoRotationOptions.bEnforceBounds)
		{
			Eigen::Vector3f minEulers;
			Eigen::Vector3f maxEulers;
			GetGizmoRotationBounds(InGizmoIndex, minEulers.data(), maxEulers.data(), /*bInExpandToCurrent*/ true);
			Eulers.array() = Eulers.array().max(minEulers.array()).min(maxEulers.array());
		}

		auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		newFaceState->SetPatchRotationEulerDegrees(InGizmoIndex, Eulers);
		if (InGizmoRotationOptions.bSymmetric && m->dmtGizmoData->HasSymmetry())
		{
			newFaceState->SymmetricRegionCopy(m->dmtGizmoData->symmetries, InGizmoIndex, /*includingPcaWeights=*/false);
		}

		m->faceState = newFaceState;
		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set gizmo rotation: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoRotationBounds(int InGizmoIndex, float OutMinEuler[3], float OutMaxEuler[3], bool bInExpandToCurrent) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }
		Eigen::Vector3f MinEuler = m->mhcApi->m->regionRotationRanges[InGizmoIndex].first;
		Eigen::Vector3f MaxEuler = m->mhcApi->m->regionRotationRanges[InGizmoIndex].second;

		Eigen::Map<Eigen::Vector3f> MapMinEuler(OutMinEuler);
		Eigen::Map<Eigen::Vector3f> MapMaxEuler(OutMaxEuler);
		if (bInExpandToCurrent)
		{
			Eigen::Vector3f CurrentEuler = m->faceState->PatchRotationEulerDegrees(InGizmoIndex);
			MapMinEuler = CurrentEuler.array().min(MinEuler.array());
			MapMaxEuler = CurrentEuler.array().max(MaxEuler.array());
		}
		else
		{
			MapMinEuler = MinEuler;
			MapMaxEuler = MaxEuler;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get gizmo scale bounds: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoRotation(int InGizmoIndex, float OutEuler[3]) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }
		Eigen::Map<Eigen::Vector3f> eul(OutEuler);
		eul = m->faceState->PatchRotationEulerDegrees(InGizmoIndex);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set gizmo rotation: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetGizmoScale(int InGizmoIndex, float InScale, bool bInSymmetric)
{
	GizmoScalingOptions Options;
	Options.bSymmetric = bInSymmetric;
	return SetGizmoScale(InGizmoIndex, InScale, Options);
}

bool MetaHumanCreatorAPI::State::SetGizmoScale(int InGizmoIndex, float InScale, const GizmoScalingOptions& InGizmoScalingOptions)
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }

		float scale = InScale;
		if (InGizmoScalingOptions.bEnforceBounds)
		{
			float minScale{ scale };
			float maxScale{ scale };
			GetGizmoScaleBounds(InGizmoIndex, minScale, maxScale, /*bInExpandToCurrent*/ true);
			scale = std::clamp(scale, minScale, maxScale);
		}

		auto newFaceState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
		newFaceState->SetPatchScale(InGizmoIndex, scale);
		if (InGizmoScalingOptions.bSymmetric && m->dmtGizmoData->HasSymmetry())
		{
			newFaceState->SymmetricRegionCopy(m->dmtGizmoData->symmetries, InGizmoIndex, /*includingPcaWeights=*/false);
		}
		m->faceState = newFaceState;
		m->UpdateCombinedState();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set gizmo scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoScaleBounds(int InGizmoIndex, float& OutMinScale, float& OutMaxScale, bool bInExpandToCurrent) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(InGizmoIndex)) { return false; }
		if (bInExpandToCurrent)
		{
			const float currentScale = m->faceState->PatchScale(InGizmoIndex);
			OutMinScale = std::min(currentScale, m->mhcApi->m->regionScaleRanges[InGizmoIndex].first);
			OutMaxScale = std::max(currentScale, m->mhcApi->m->regionScaleRanges[InGizmoIndex].second);
		}
		else
		{
			OutMinScale = m->mhcApi->m->regionScaleRanges[InGizmoIndex].first;
			OutMaxScale = m->mhcApi->m->regionScaleRanges[InGizmoIndex].second;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get gizmo scale bounds: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGizmoScale(int GizmoIndex, float& scale) const
{
	try
	{
		TITAN_RESET_ERROR;
		if (!HasGizmo(GizmoIndex)) { return false; }
		scale = m->faceState->PatchScale(GizmoIndex);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get gizmo scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetGlobalScale(float scale)
{
	try
	{
		TITAN_RESET_ERROR;
		m->combinedScale = scale;
		m->faceScale = m->combinedScale / m->bodyScale;
		m->UpdateCombinedState();
		m->UpdateBodyDeltas();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set global scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetGlobalScale(float& scale) const
{
	try
	{
		TITAN_RESET_ERROR;
		scale = m->combinedScale;
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get global scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetFaceScale(float scale)
{
	try
	{
		TITAN_RESET_ERROR;
		m->faceScale = scale;
		m->UpdateCombinedState();
		m->UpdateBodyDeltas();
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set global scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::GetFaceScale(float& scale) const
{
	try
	{
		TITAN_RESET_ERROR;
		scale = m->faceScale;
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get global scale: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::TranslateLandmark(int LandmarkIndex, const float DeltaXYZ[3], bool bInSymmetric)
{
	try
	{
		TITAN_RESET_ERROR;
		const Eigen::Vector3f delta = Eigen::Map<const Eigen::Vector3f>(DeltaXYZ);
		auto newState = std::make_shared<PatchBlendModel<float>::State>(*m->State());

		if (fabs(m->dmtModel->GetRegularizationWeight() - m->settings->m->dmtSettings.dmtRegularization) > 1e-7)
		{
			auto newDmtModel = m->mhcApi->m->defaultState->m->dmtModel->Clone();
			newDmtModel->Init(m->dmtLandmarkData->vertexIndices,
				m->mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
				m->settings->m->dmtSettings.singleRegionPerLandmark,
				m->settings->m->dmtSettings.dmtRegularization);
			m->dmtModel = newDmtModel;
		}
		DmtModel<float>::SolveOptions solverOptions;
		solverOptions.symmetric = bInSymmetric;
		solverOptions.pcaThreshold = m->settings->m->dmtSettings.dmtPcaThreshold;
		solverOptions.markerCompensate = m->settings->m->dmtSettings.dmtStabilizeFixLandmarks;
		m->dmtModel->ForwardDmtDelta(*newState, LandmarkIndex, delta, solverOptions);
		if (m->combinedState)
		{
			m->combinedState = newState;
			m->UpdateFaceState();
		}
		else
		{
			m->faceState = newState;
			m->UpdateCombinedState();
		}

		// // new test
// Eigen::Matrix<float, 3, -1> Vertices = m->mhcApi->m->patchBlendModel->DeformedVertices(*m->State());
// Eigen::Matrix<float, 3, -1> VerticesNew = m->mhcApi->m->patchBlendModel->DeformedVertices(*newState);
// Eigen::Matrix<float, 3, -1> Deltas = VerticesNew - Vertices;
// std::shared_ptr<Mesh<float>> mesh = std::make_shared<Mesh<float>>();
// auto faceRange = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);
// mesh->SetVertices(Vertices(Eigen::all, Eigen::seq(faceRange.first, faceRange.second - 1)));
// const int vID = m->dmtLandmarkData->vertexIndices[LandmarkIndex];
// const float radius = 3.0f;
// auto smootherstep = [](float x) {
//     x = std::clamp<float>(x, 0.0f, 1.0f);
//     return x * x * x * (x * (x * 6 - 15) + 10);
// };
// auto distancesAndVertices = m->mhcApi->m->meshTools.GeodesicDistance(m->mhcApi->m->faceArchetypeMesh, mesh, FaceCoord(vID), radius);
// auto vertexDeltas = std::make_shared<Eigen::Matrix<float, 3, -1>>();
// vertexDeltas->resize(3, m->mhcApi->m->patchBlendModel->NumVertices());
// if (m->vertexDelta) *vertexDeltas = *m->vertexDelta;
// else vertexDeltas->setZero();
// for (const auto& [dist, vID] : distancesAndVertices)
// {
//     (*vertexDeltas).col(faceRange.first + vID) += Deltas.col(faceRange.first + vID) * smootherstep(1.0f - dist/radius);
// }
// m->vertexDelta = vertexDeltas;


		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to translate landmark: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::HasLandmark(int VertexIndex) const
{
	return GetItemIndex<int>(m->dmtLandmarkData->vertexIndices, VertexIndex) >= 0;
}

bool MetaHumanCreatorAPI::State::AddLandmark(int VertexIndex)
{
	try
	{
		TITAN_RESET_ERROR;
		auto newDmtLandmarkData = std::make_shared<DmtLandmarkData>(*m->dmtLandmarkData);

		if (GetItemIndex<int>(m->dmtLandmarkData->vertexIndices, VertexIndex) >= 0)
		{
			return false;
		}

		newDmtLandmarkData->vertexIndices.push_back(VertexIndex);

		if ((m->mhcApi->m->symmetries.size() > 0) && (m->mhcApi->m->symmetries[VertexIndex] != VertexIndex))
		{
			newDmtLandmarkData->vertexIndices.push_back(m->mhcApi->m->symmetries[VertexIndex]);
		}
		newDmtLandmarkData->symmetries = GetSymmetricIndices(newDmtLandmarkData->vertexIndices, m->mhcApi->m->symmetries);
		m->dmtLandmarkData = newDmtLandmarkData;

		auto newDmtModel = m->mhcApi->m->defaultState->m->dmtModel->Clone();
		newDmtModel->Init(newDmtLandmarkData->vertexIndices,
			m->mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
			m->settings->m->dmtSettings.singleRegionPerLandmark,
			m->settings->m->dmtSettings.dmtRegularization);
		m->dmtModel = newDmtModel;

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to add landmark: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::RemoveLandmark(int LandmarkIndex)
{
	try
	{
		TITAN_RESET_ERROR;
		auto newDmtLandmarkData = std::make_shared<DmtLandmarkData>(*m->dmtLandmarkData);

		std::vector<int> newIndices;
		const int symmetricIndex = m->dmtLandmarkData->GetSymmetricIndex(LandmarkIndex);
		for (int i = 0; i < (int)m->dmtLandmarkData->vertexIndices.size(); ++i)
		{
			if ((i != LandmarkIndex) && (i != symmetricIndex))
			{
				newIndices.push_back(m->dmtLandmarkData->vertexIndices[i]);
			}
		}
		newDmtLandmarkData->vertexIndices = newIndices;
		newDmtLandmarkData->symmetries = GetSymmetricIndices(newDmtLandmarkData->vertexIndices, m->mhcApi->m->symmetries);
		m->dmtLandmarkData = newDmtLandmarkData;

		auto newDmtModel = m->mhcApi->m->defaultState->m->dmtModel->Clone();
		newDmtModel->Init(newDmtLandmarkData->vertexIndices,
			m->mhcApi->m->patchBlendModelDataManipulator->NumJoints(),
			m->settings->m->dmtSettings.singleRegionPerLandmark,
			m->settings->m->dmtSettings.dmtRegularization);
		m->dmtModel = newDmtModel;

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to remove landmark: {}", e.what());
	}
}

int MetaHumanCreatorAPI::State::SelectFaceVertex(const Eigen::Vector3f& InOriginXYZ, const Eigen::Vector3f& InDirectionXYZ, Eigen::Vector3f& OutVertex, Eigen::Vector3f& OutNormal) const
{
	try
	{
		TITAN_RESET_ERROR;
		int VertexID = -1;

		// evaluate the current state (TODO: only evaluate the core, not all assets or LODs)
		Eigen::Matrix<float, 3, -1> Vertices;
		if (!m->mhcApi->Evaluate(*this, Vertices))
		{
			return VertexID;
		}

		int vID = m->mhcApi->SelectVertex(Vertices, InOriginXYZ, InDirectionXYZ);
		auto faceRange = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);

		if (vID >= faceRange.first && vID < faceRange.second)
		{
			VertexID = vID - faceRange.first;
			Eigen::Matrix<float, 3, -1> Normals(3, faceRange.second - faceRange.first);
			// TODO: only evaluate the normal of the vertex
			m->mhcApi->m->archetypeTriangulatedMeshes[0].CalculateVertexNormalsRef(Vertices(Eigen::all, Eigen::seq(faceRange.first, faceRange.second - 1)), Normals, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/false, m->mhcApi->m->threadPool.get());
			OutVertex = Vertices.col(vID);
			OutNormal = Normals.col(vID - faceRange.first);
		}

		return VertexID;
	}
	catch (const std::exception& e)
	{
		TITAN_CHECK_OR_RETURN(false, -1, "failure to select face landmark: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetBodyJointsAndBodyFaceVertices(const float* InBodyBindPoses, const float* InBodyVertices)
{
	try
	{
		TITAN_RESET_ERROR;
		const int numBodyJoints = m->mhcApi->m->archetypeBodyGeometry ? m->mhcApi->m->archetypeBodyGeometry->GetJointRig().NumJoints() : 0;
		const int numFaceJoints = m->mhcApi->m->archetypeFaceGeometry ? m->mhcApi->m->archetypeFaceGeometry->GetJointRig().NumJoints() : 0;

		if (InBodyBindPoses && (numBodyJoints > 0))
		{
			// extract body joint positions
			Eigen::Map<const Eigen::VectorX<Eigen::Matrix4f>> bodyBindPosesMap((const Eigen::Matrix4f*)InBodyBindPoses, numBodyJoints);
			auto bodyJointPositions = std::make_shared<Eigen::Matrix<float, 3, -1>>(3, numBodyJoints);
			for (int jointIndex = 0; jointIndex < numBodyJoints; ++jointIndex)
			{
				bodyJointPositions->col(jointIndex) = bodyBindPosesMap[jointIndex].block(0, 3, 3, 1);
			}
			m->bodyJointPositions = bodyJointPositions;
		}
		else
		{
			m->bodyJointPositions.reset();
		}

		if (InBodyVertices)
		{
			// copy the body face vertices (first part of the body vertices are the face vertices)
			auto faceRange = m->mhcApi->m->patchBlendModelDataManipulator->GetRangeForMeshIndex(0);
			CARBON_ASSERT(faceRange.first == numFaceJoints, "number of face joints should match the model");
			const int numFaceVertices = faceRange.second - faceRange.first;

			auto bodyVertices = std::make_shared<Eigen::Matrix<float, 3, -1>>();
			*bodyVertices = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(InBodyVertices, 3, numFaceVertices);
			m->bodyFaceVertices = bodyVertices;

			// fit the state to the body face vertices
			if (m->bodyJointPositions)
			{
				float bodyScale = 1.0f;
				auto it = m->mhcApi->m->masks.find(BodyBlendMaskName);
				if (it != m->mhcApi->m->masks.end())
				{
					// estimate scale
					Eigen::Matrix4f AtA = Eigen::Matrix4f::Zero();
					Eigen::Vector4f Atb = Eigen::Vector4f::Zero();
					Eigen::Matrix<float, 3, 4> A = Eigen::Matrix<float, 3, 4>::Zero();
					A.rightCols(3).setIdentity();
					Eigen::Vector3f b = Eigen::Vector3f::Zero();

					for (int vID = 0; vID < it->second.NumVertices(); ++vID)
					{
						float weight = 1.0f - it->second.Weights()[vID];
						if (weight > 0)
						{
							// scale * mean + offset = target
							Eigen::Vector3f vertex = m->mhcApi->m->patchBlendModel->BaseVertices().col(numFaceJoints + vID);
							A.col(0) = vertex;
							b = bodyVertices->col(vID);
							AtA += A.transpose() * A * weight;
							Atb += A.transpose() * b * weight;
						}
					}

					bodyScale = (AtA.inverse() * Atb)[0];
				}

				// get delta of common face/body joints (head, neck_01, neck_02) between input body joints and the face archetype joints
				auto archetypeFaceJoints = m->mhcApi->m->patchBlendModel->BaseVertices().leftCols(numFaceJoints);
				Eigen::Matrix3Xf commonFaceJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromFaceJoints(archetypeFaceJoints);
				Eigen::Matrix3Xf commonBodyJoints = m->mhcApi->m->faceToBodySkinning->ExtractCommonJointsFromBodyJoints(*m->bodyJointPositions);
				Eigen::Matrix3Xf bodyToFaceJointDeltas = commonFaceJoints - commonBodyJoints / bodyScale;

				// move body vertices to the archetype joint positions (canconical body vertices)
				const int meshIndex = 0;
				auto canoncialBodyVertices = std::make_shared<Eigen::Matrix<float, 3, -1>>(*bodyVertices / bodyScale);
				m->mhcApi->m->faceToBodySkinning->UpdateVertices(meshIndex, *canoncialBodyVertices, bodyToFaceJointDeltas);
				m->canoncialBodyVertices = canoncialBodyVertices;

				if (!m->settings->m->evaluationSettings.lockBodyFaceState)
				{
					auto bodyState = std::make_shared<PatchBlendModel<float>::State>(*m->faceState);
					m->mhcApi->m->fastPatchModelFitting->Fit(*bodyState, *canoncialBodyVertices, m->settings->m->bodyFitSettings);
					m->bodyState = bodyState;
				}
				if (m->settings->m->evaluationSettings.lockFaceScale)
				{
					m->faceScale *= m->bodyScale / bodyScale;
				}
				m->bodyScale = bodyScale;
				m->UpdateCombinedState();
				m->UpdateBodyDeltas();
			}
		}
		else
		{
			m->bodyFaceVertices.reset();
			m->bodyDeltas.reset();
			m->bodyState.reset();
			m->canoncialBodyVertices.reset();
			m->bodyScale = 1.0f;
			m->UpdateCombinedState();
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set body joints and deltas: {}", e.what());
	}
}


bool MetaHumanCreatorAPI::State::GetVariant(const std::string& VariantType, float* VariantValues) const
{
	try
	{
		if (!VariantValues) { return true; }

		auto it = m->variantValues.find(VariantType);
		if (it != m->variantValues.end())
		{
			std::memcpy(VariantValues, it->second->data(), sizeof(float) * it->second->size());
			return true;
		}
		else
		{
			std::memset(VariantValues, 0, sizeof(float) * m->mhcApi->GetVariantNames(VariantType).size());
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to get variant: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::SetHFVariant(int HFIndex)
{
	try
	{
		m->hfVariant = HFIndex;
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set hf variant: {}", e.what());
	}
}

int MetaHumanCreatorAPI::State::GetHFVariant() const
{
	return m->hfVariant;
}

bool MetaHumanCreatorAPI::State::SetVariant(const std::string& VariantType, const float* VariantValues)
{
	try
	{
		auto it = m->mhcApi->m->variants.find(VariantType);
		if (it != m->mhcApi->m->variants.end())
		{
			int numParameters = it->second->NumParameters();
			auto coeffs = Eigen::Map<const Eigen::VectorXf>(VariantValues, numParameters);
			if (VariantValues && (coeffs.squaredNorm() > 0))
			{
				m->variantValues[VariantType] = std::make_shared<Eigen::VectorXf>(coeffs);
			}
			else
			{
				// clear the variants
				auto itValues = m->variantValues.find(VariantType);
				if (itValues != m->variantValues.end())
				{
					m->variantValues.erase(VariantType);
				}
			}
		}
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to set variant: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::Calibrate()
{
	try
	{
		const auto neutralParametersState = m->State()->ConcatenatePatchPcaWeights();
		std::map<std::string, Eigen::VectorXf> inputParams;
		inputParams[m->mhcApi->m->rigCalibrationModelData->GetNeutralName()] = neutralParametersState;
		float lambda = 0.01;

		const auto calibrationResult = RigCalibrationCore::CalibrateExpressionsAndSkinning(m->mhcApi->m->rigCalibrationModelData, inputParams, { lambda });
		m->calibratedModelParameters = std::make_shared<std::map<std::string, Eigen::VectorXf>>(calibrationResult);
		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to calibrate expressions: {}", e.what());
	}
}

bool MetaHumanCreatorAPI::State::DumpDataForAR(const std::string& directoryName) const
{
	try
	{
		Eigen::Matrix<float, 3, -1> vertices;
		if (!m->mhcApi->Evaluate(*this, vertices)) return false;
		const auto& rigGeometry = m->mhcApi->m->archetypeFaceGeometry;
		if (!rigGeometry) return false;

		JsonElement meshesJson(JsonElement::JsonType::Object);
		// dump meshes
		for (int i = 0; i < (int)m->mhcApi->m->modelMeshIds.size(); ++i)
		{
			int meshIndex = m->mhcApi->m->modelMeshIds[i];
			Eigen::Matrix<float, 3, -1> meshVertices;
			m->mhcApi->GetMeshVertices(vertices.data(), meshIndex, meshVertices);
			Mesh<float> mesh = rigGeometry->GetMesh(meshIndex);
			if ((int)meshVertices.cols() != mesh.NumVertices()) return false;
			mesh.SetVertices(meshVertices);
			const std::string& meshName = rigGeometry->GetMeshName(meshIndex);
			std::string filename = meshName + ".obj";
			ObjFileWriter<float>::writeObj(mesh, directoryName + "/" + filename);
			meshesJson.Insert(meshName, JsonElement("./" + filename));
		}
		JsonElement targetJson(JsonElement::JsonType::Object);
		targetJson.Insert("meshes", std::move(meshesJson));

		// dump bindpose
		Eigen::Matrix<float, 3, -1> bindPose;
		if (!m->mhcApi->GetBindPose(vertices.data(), bindPose)) return false;
		std::string bindPoseFilename = "bind_pose.npy";
		npy::SaveMatrixAsNpy(directoryName + "/" + bindPoseFilename, bindPose);
		targetJson.Insert("bind_pose", JsonElement("./" + bindPoseFilename));

		// dump state
		Eigen::VectorXf serializedState = m->State()->SerializeToVector();
		std::string paramsFilename = "params.npy";
		npy::SaveMatrixAsNpy(directoryName + "/" + paramsFilename, serializedState);
		targetJson.Insert("params", JsonElement("./" + paramsFilename));
		targetJson.Insert("model_version_identifier", JsonElement(m->mhcApi->m->rigCalibrationModelData->GetModelVersionIdentifier()));
		targetJson.Insert("hf_id", JsonElement(m->hfVariant));
		targetJson.Insert("scale", JsonElement(m->combinedScale));

		JsonElement finalJson(JsonElement::JsonType::Object);
		finalJson.Insert("target", std::move(targetJson));

		WriteFile(directoryName + "/" + "targets.json", WriteJson(finalJson, /*tabs=*/1));

		return true;
	}
	catch (const std::exception& e)
	{
		TITAN_HANDLE_EXCEPTION("failure to export state for auto rigging: {}", e.what());
	}
}

MetaHumanCreatorAPI::Settings::Settings()
	: m(new Private())
{}

MetaHumanCreatorAPI::Settings::~Settings()
{
	delete m;
}

MetaHumanCreatorAPI::Settings::Settings(const Settings& other)
	: m(new Private(*other.m))
{}

std::shared_ptr<MetaHumanCreatorAPI::Settings> MetaHumanCreatorAPI::Settings::Clone() const
{
	try
	{
		TITAN_RESET_ERROR;
		return std::shared_ptr<MetaHumanCreatorAPI::Settings>(new MetaHumanCreatorAPI::Settings(*this));
	}
	catch (const std::exception& e)
	{
		TITAN_SET_ERROR(-1, fmt::format("failure to clone settings: {}", e.what()).c_str());
		return nullptr;
	}
}

float MetaHumanCreatorAPI::Settings::GlobalVertexDeltaScale() const { return m->evaluationSettings.globalVertexDeltaScale; }
void MetaHumanCreatorAPI::Settings::SetGlobalVertexDeltaScale(float globalVertexDeltaScale) { m->evaluationSettings.globalVertexDeltaScale = globalVertexDeltaScale; }
float MetaHumanCreatorAPI::Settings::RegionVertexDeltaScale(int patchId) const { return m->evaluationSettings.regionVertexDeltaScales[patchId]; }
void MetaHumanCreatorAPI::Settings::SetRegionVertexDeltaScale(int patchId, float vertexDeltaScale) { m->evaluationSettings.regionVertexDeltaScales[patchId] = vertexDeltaScale; }
bool MetaHumanCreatorAPI::Settings::GenerateAssetsAndEvaluateAllLODs() const { return m->evaluationSettings.generateAssetsAndEvaluateAllLODs; }
void MetaHumanCreatorAPI::Settings::SetGenerateAssetsAndEvaluateAllLODs(bool bGenerateAssetsAndEvaluateAllLODs) { m->evaluationSettings.generateAssetsAndEvaluateAllLODs = bGenerateAssetsAndEvaluateAllLODs; }
bool MetaHumanCreatorAPI::Settings::DmtWithSymmetry() const { return m->dmtSettings.symmetricDmt; }
void MetaHumanCreatorAPI::Settings::SetDmtWithSymmetry(bool isDmtWithSymmetry) { m->dmtSettings.symmetricDmt = isDmtWithSymmetry; }
float MetaHumanCreatorAPI::Settings::DmtPcaThreshold() const { return m->dmtSettings.dmtPcaThreshold; }
void MetaHumanCreatorAPI::Settings::SetDmtPcaThreshold(float DmtPcaThreshold) { m->dmtSettings.dmtPcaThreshold = DmtPcaThreshold; }
float MetaHumanCreatorAPI::Settings::DmtRegularization() const { return m->dmtSettings.dmtRegularization; }
void MetaHumanCreatorAPI::Settings::SetDmtRegularization(float InDmtRegularization) { m->dmtSettings.dmtRegularization = InDmtRegularization; }
bool MetaHumanCreatorAPI::Settings::DmtStabilizeFixLandmarks() const { return m->dmtSettings.dmtStabilizeFixLandmarks; }
void MetaHumanCreatorAPI::Settings::SetDmtStabilizeFixLandmarks(bool bInDmtStabilizeFixLandmarks) { m->dmtSettings.dmtStabilizeFixLandmarks = bInDmtStabilizeFixLandmarks; }
bool MetaHumanCreatorAPI::Settings::LockBodyFaceState() const { return m->evaluationSettings.lockBodyFaceState; }
void MetaHumanCreatorAPI::Settings::SetLockBodyFaceState(bool lockBodyFaceState) { m->evaluationSettings.lockBodyFaceState = lockBodyFaceState; }
bool MetaHumanCreatorAPI::Settings::LockFaceScale() const { return m->evaluationSettings.lockFaceScale; }
void MetaHumanCreatorAPI::Settings::SetLockFaceScale(bool lockFaceScale) { m->evaluationSettings.lockFaceScale = lockFaceScale; }
bool MetaHumanCreatorAPI::Settings::CombineFaceAndBodyInEvaluation() const { return m->evaluationSettings.combineFaceAndBody; }
void MetaHumanCreatorAPI::Settings::SetCombineFaceAndBodyInEvaluation(bool combineFaceAndBody) { m->evaluationSettings.combineFaceAndBody = combineFaceAndBody; }
bool MetaHumanCreatorAPI::Settings::UpdateBodyJointsInEvaluation() const { return m->evaluationSettings.updateBodyJoints; }
void MetaHumanCreatorAPI::Settings::SetUpdateBodyJointsInEvaluation(bool updateBodyJoints) { m->evaluationSettings.updateBodyJoints = updateBodyJoints; }
bool MetaHumanCreatorAPI::Settings::UpdateFaceSurfaceJointsInEvaluation() const { return m->evaluationSettings.updateFaceSurfaceJoints; }
bool MetaHumanCreatorAPI::Settings::UpdateFaceVolumetricJointsInEvaluation() const { return m->evaluationSettings.updateFaceVolumetricJoints; }
void MetaHumanCreatorAPI::Settings::SetUpdateFaceVolumetricJointsInEvaluation(bool updateFaceVolumetricJoints) { m->evaluationSettings.updateFaceVolumetricJoints = updateFaceVolumetricJoints; }
void MetaHumanCreatorAPI::Settings::SetUpdateFaceSurfaceJointsInEvaluation(bool updateFaceSurfaceJoints) { m->evaluationSettings.updateFaceSurfaceJoints = updateFaceSurfaceJoints; }
bool MetaHumanCreatorAPI::Settings::UpdateBodySurfaceJointsInEvaluation() const { return m->evaluationSettings.updateBodySurfaceJoints; }
void MetaHumanCreatorAPI::Settings::SetUpdateBodySurfaceJointsInEvaluation(bool updateBodySurfaceJoints) { m->evaluationSettings.updateBodySurfaceJoints = updateBodySurfaceJoints; }
bool MetaHumanCreatorAPI::Settings::UseBodyDeltaInEvaluation() const { return m->evaluationSettings.useBodyDelta; }
void MetaHumanCreatorAPI::Settings::SetUseBodyDeltaInEvaluation(bool useBodyDelta) { m->evaluationSettings.useBodyDelta = useBodyDelta; }
bool MetaHumanCreatorAPI::Settings::UseScaleInBodyFit() const { return m->bodyFitSettings.withScale; }
void MetaHumanCreatorAPI::Settings::SetUseScaleInBodyFit(bool useScaleInBodyFit) { m->bodyFitSettings.withScale = useScaleInBodyFit; }
float MetaHumanCreatorAPI::Settings::BodyFitRegularization() const { return m->bodyFitSettings.regularization; }
void MetaHumanCreatorAPI::Settings::SetBodyFitRegularization(float bodyFitRegularization) { m->bodyFitSettings.regularization = bodyFitRegularization; }
bool MetaHumanCreatorAPI::Settings::UseCanoncialBodyInEvaluation() const { return m->evaluationSettings.useCanonicalBodyInEvaluation; }
void MetaHumanCreatorAPI::Settings::SetUseCanonicalBodyInEvaluation(bool useCanconicalBodyInEvaluation) { m->evaluationSettings.useCanonicalBodyInEvaluation = useCanconicalBodyInEvaluation; }
float MetaHumanCreatorAPI::Settings::GlobalHFScale() const { return m->evaluationSettings.globalHfScale; }
void MetaHumanCreatorAPI::Settings::SetGlobalHFScale(float Scale) { m->evaluationSettings.globalHfScale = Scale; }
float MetaHumanCreatorAPI::Settings::RegionHfScale(int patchId) const { return m->evaluationSettings.regionHfScales[patchId]; }
void MetaHumanCreatorAPI::Settings::SetRegionHfScale(int patchId, float vertexDeltaScale) { m->evaluationSettings.regionHfScales[patchId] = vertexDeltaScale; }
int MetaHumanCreatorAPI::Settings::HFIterations() const { return m->evaluationSettings.hfIterations; }
void MetaHumanCreatorAPI::Settings::SetHFIterations(int Iterations) { m->evaluationSettings.hfIterations = Iterations; }

} // namespace TITAN_API_NAMESPACE
