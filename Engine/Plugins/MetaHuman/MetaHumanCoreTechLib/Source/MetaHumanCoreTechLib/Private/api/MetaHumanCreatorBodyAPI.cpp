// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCreatorBodyAPI.h"
#include <Common.h>

#include <bodyshapeeditor/BodyShapeEditor.h>
#include <rig/BodyGeometry.h>
#include <rig/RigGeometry.h>
#include <rig/CombinedBodyJointLodMapping.h>
#include <dna/BinaryStreamWriter.h>
#include <nrr/MeshLandmarks.h>
#include <nrr/VertexWeights.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <trio/Stream.h>

#include <algorithm>
#include <memory>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

struct PhysicsVolumeDefinition
{
    std::vector<int> vertexIndices;
    std::vector<std::pair<std::string, float>> extentJointsAndScale;
};

struct MetaHumanCreatorBodyAPI::Private
{
    std::shared_ptr<TaskThreadPool> threadPool;
    BodyShapeEditor ptr;
    std::vector<std::shared_ptr<const BodyGeometry<float>>> legacyBodies;
    std::vector<std::string> legacyBodiesNames;
    std::vector<int> regionVertexIndices;
    std::vector<std::string> presetNames;
    std::map<std::string, std::shared_ptr<const State>> presetStates;

    std::map<std::string, std::vector<PhysicsVolumeDefinition>> physicsBodiesVolumes;
};

static std::map<std::string, std::vector<PhysicsVolumeDefinition>> LoadPhysicsVolumeDefinitions(const std::string& physicsVolumeDefinitionsJsonPath, const std::string& physicsVertexMasksJsonPath, int topologyNumVertices)
{
    std::map<std::string, std::vector<PhysicsVolumeDefinition>> physicsVolumeDefinitions;

    const JsonElement bodiesJson = ReadJson(ReadFile(physicsVolumeDefinitionsJsonPath));
    std::map<std::string, VertexWeights<float>> physicsVertexMasks = VertexWeights<float>::LoadAllVertexWeights(physicsVertexMasksJsonPath, topologyNumVertices);


    if (bodiesJson.Contains("physics_body_volumes"))
    {
        for (const JsonElement& bodyDef : bodiesJson["physics_body_volumes"].Array())
        {
            PhysicsVolumeDefinition volumeDefinition;

            const std::string jointName = bodyDef["joint_name"].String();

            const std::string vertexMaskName = bodyDef["vertex_mask"].String();
            if (auto vertexWeightItr = physicsVertexMasks.find(vertexMaskName); vertexWeightItr != physicsVertexMasks.end())
            {
                volumeDefinition.vertexIndices = vertexWeightItr->second.NonzeroVertices();
            }

            if (bodyDef.Contains("extent_joints"))
            {
                for (const JsonElement& extentJoints : bodyDef["extent_joints"].Array())
                {
                    const std::string extentJointName = extentJoints["extent_joint"].String();
                    float extentJointScale = extentJoints["scale"].Get<float>();
                    volumeDefinition.extentJointsAndScale.push_back({extentJointName, extentJointScale});
                }
            }

            physicsVolumeDefinitions[jointName].push_back(volumeDefinition);
        }
    }

    return physicsVolumeDefinitions;
}
    
static std::vector<int> LoadRegionVertexIndices(const std::string& RegionLandmarksPath, dna::Reader* CombinedBodyArchetypeDnaReader)
{
    std::vector<int> vertexIndices;

    if (std::filesystem::exists(RegionLandmarksPath))
    {
		const int meshIndex = 0;
		auto rigGeometry = std::make_shared<RigGeometry<float>>();
		TITAN_CHECK_OR_RETURN(rigGeometry->Init(CombinedBodyArchetypeDnaReader, true), {}, "cannot load rig geometry");
		const std::string meshName = rigGeometry->GetMeshName(meshIndex);

		auto meshLandmarks = std::make_shared<MeshLandmarks<float>>();
		meshLandmarks->Load(RegionLandmarksPath, rigGeometry->GetMesh(meshName), meshName);

		for (const auto& landmark : meshLandmarks->LandmarksBarycentricCoordinates())
		{
			vertexIndices.push_back(landmark.second.Index(0));
		}
    }

    return vertexIndices;
}

bool LoadJsonMask(VertexWeights<float>& output, const std::string& filepath, const std::string& maskName, int numVertices)
{
    const JsonElement masksJson = ReadJson(ReadFile(filepath));
    if (masksJson.Contains(maskName))
    {
        output.Load(masksJson, maskName, numVertices);
        return true;
    }

    return false;
}

MetaHumanCreatorBodyAPI::MetaHumanCreatorBodyAPI()
    : m(new Private())
{}

MetaHumanCreatorBodyAPI::~MetaHumanCreatorBodyAPI()
{
    delete m;
}

struct MetaHumanCreatorBodyAPI::State::Private
{
    std::shared_ptr<BodyShapeEditor::State> ptr;
    int legacyBodyIndex{-1};
};

MetaHumanCreatorBodyAPI::State::State()
    : m(new Private())
{}

MetaHumanCreatorBodyAPI::State::~State()
{
    delete m;
}

MetaHumanCreatorBodyAPI::State::State(const State& other)
    : m(new Private(*other.m))
{}

std::shared_ptr<MetaHumanCreatorBodyAPI> MetaHumanCreatorBodyAPI::CreateMHCBodyApi(const dna::Reader* PCABodyModel,
                                                                                   dna::Reader* InCombinedBodyArchetypeDnaReader,
                                                                                   const std::string& CombinedSkinningWeightGenerationConfigPath,
                                                                                   const std::string& CombinedLodGenerationConfigPath,
                                                                                   const std::string& PhysicsBodiesConfigPath,
                                                                                   const std::string& BodyMasksPath,
                                                                                   const std::string& RegionLandmarksPath,
                                                                                   int numThreads)
{
    try
    {
        TITAN_RESET_ERROR;
        std::shared_ptr<MetaHumanCreatorBodyAPI> APIInstance(new MetaHumanCreatorBodyAPI);
        if (numThreads != 0)
        {
            APIInstance->m->threadPool = std::make_shared<TaskThreadPool>(numThreads);
        }
        APIInstance->m->ptr.SetThreadPool(APIInstance->m->threadPool);
        std::shared_ptr<LodGeneration<float>> CombinedLodGenerationData;
        if (std::filesystem::exists(CombinedLodGenerationConfigPath))
        {
            CombinedLodGenerationData = std::make_shared<LodGeneration<float>>();
            if (!CombinedLodGenerationData->LoadModelBinary(CombinedLodGenerationConfigPath))
            {
                LOG_ERROR("Failed to load combined body model lod generation data");
                return nullptr;
            }
        }
        else
        {
            LOG_WARNING("No lod generation data supplied; only lod 0 will be available");
        }

        CombinedLodGenerationData->SetThreadPool(APIInstance->m->threadPool);

        // load in the configuration which defines which how joint weights are distributed to higher lods
        JsonElement json;
        json = ReadJson(ReadFile(CombinedSkinningWeightGenerationConfigPath));
        CombinedBodyJointLodMapping<float> jointMapping;
        bool bLoadMapping = jointMapping.ReadJson(json);
        if (!bLoadMapping)
        {
            LOG_ERROR("Failed to parse skinning weight generation config for body model");
            return nullptr;
        }

        const std::vector<int> MaxSkinWeightsPerLod = { 12, 8, 8, 4 };
        APIInstance->m->ptr.Init(PCABodyModel, InCombinedBodyArchetypeDnaReader, jointMapping.GetJointMapping(), MaxSkinWeightsPerLod, CombinedLodGenerationData);

        APIInstance->m->physicsBodiesVolumes = LoadPhysicsVolumeDefinitions(PhysicsBodiesConfigPath, BodyMasksPath, InCombinedBodyArchetypeDnaReader->getVertexPositionCount(0));
        APIInstance->m->regionVertexIndices = LoadRegionVertexIndices(RegionLandmarksPath, InCombinedBodyArchetypeDnaReader);

        VertexWeights<float> weights;
        if(!LoadJsonMask(weights, BodyMasksPath, "FitToTarget", InCombinedBodyArchetypeDnaReader->getVertexPositionCount(0)))
        {
            weights = VertexWeights<float>(Eigen::VectorXf::Ones(InCombinedBodyArchetypeDnaReader->getVertexPositionCount(0)));
        }
        APIInstance->m->ptr.SetFittingVertexIDs(weights.NonzeroVertices());

        const int neckSeamLoopsCount = 3;
        std::vector<std::vector<int>> neckSeamLoops;
        for (int i = 0; i < neckSeamLoopsCount; ++i)
        {
            if(LoadJsonMask(weights, BodyMasksPath, "neck_seam_" + std::to_string(i), InCombinedBodyArchetypeDnaReader->getVertexPositionCount(0)))
            {
                neckSeamLoops.push_back(weights.NonzeroVertices());
            }
        }
        APIInstance->m->ptr.SetNeckSeamVertexIDs(neckSeamLoops);

        APIInstance->m->presetNames.clear();
        APIInstance->m->presetStates.clear();
        std::sort(APIInstance->m->presetNames.begin(), APIInstance->m->presetNames.end());

        return APIInstance;
    }
    catch (const std::exception& e)
    {
        TITAN_SET_ERROR(-1, TITAN_NAMESPACE::fmt::format("failure to initialize body: {}", e.what()).c_str());
        return nullptr;
    }
}

void MetaHumanCreatorBodyAPI::SetNumThreads(int numThreads) { m->threadPool->SetNumThreads(numThreads); }

int MetaHumanCreatorBodyAPI::GetNumThreads() const
{
    return (int)m->threadPool->NumThreads();
}

void MetaHumanCreatorBodyAPI::AddLegacyBody(const dna::Reader* LegacyBody, const av::StringView& LegacyBodyName)
{
    m->legacyBodiesNames.push_back(LegacyBodyName.c_str());
    std::shared_ptr<BodyGeometry<float>> body = std::make_shared<BodyGeometry<float>>(m->threadPool);
    body->Init(LegacyBody, /*computeMeshNormals=*/ false);
    m->legacyBodies.push_back(body);
}

std::shared_ptr<MetaHumanCreatorBodyAPI::State> MetaHumanCreatorBodyAPI::CreateState() const
{
    try
    {
        TITAN_RESET_ERROR;
        std::shared_ptr<MetaHumanCreatorBodyAPI::State> StateInstance { new MetaHumanCreatorBodyAPI::State() };
        StateInstance->m->ptr = m->ptr.CreateState();
        return StateInstance;
    }
    catch (const std::exception& e)
    {
        TITAN_SET_ERROR(-1, TITAN_NAMESPACE::fmt::format("failure to create state: {}", e.what()).c_str());
        return nullptr;
    }
}

std::shared_ptr<MetaHumanCreatorBodyAPI::State> MetaHumanCreatorBodyAPI::State::Clone() const
{
    try
    {
        TITAN_RESET_ERROR;
        return std::shared_ptr<MetaHumanCreatorBodyAPI::State>(new MetaHumanCreatorBodyAPI::State(*this));
    }
    catch (const std::exception& e)
    {
        TITAN_SET_ERROR(-1, TITAN_NAMESPACE::fmt::format("failure to clone state: {}", e.what()).c_str());
        return nullptr;
    }
}

bool MetaHumanCreatorBodyAPI::State::Reset()
{
    try
    {
        TITAN_RESET_ERROR;
        auto newState = std::make_shared<BodyShapeEditor::State>(*m->ptr);
        newState->Reset();
        m->ptr = newState;
        m->legacyBodyIndex = -1;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to reset: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::GetVertex(int lod, const float* InVertices, int DNAVertexIndex, float OutVertexXYZ[3]) const
{
    try
    {
        TITAN_RESET_ERROR;
        const std::vector<int>& BodyToCombinedMapping = m->ptr.GetBodyToCombinedMapping(lod);
        if (BodyToCombinedMapping.size() == 0) { return false; }
        int CombinedIndex = BodyToCombinedMapping[DNAVertexIndex];
        for (int k = 0; k < 3; ++k)
        {
            OutVertexXYZ[k] = InVertices[3 * CombinedIndex + k];
        }
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to get vertex: {}", e.what());
    }
}


void MetaHumanCreatorBodyAPI::GetVertexInfluenceWeights(const State& State, std::vector<TITAN_NAMESPACE::SparseMatrix<float>>& vertexInfluenceWeights) const
{
    m->ptr.GetVertexInfluenceWeights(*State.m->ptr, vertexInfluenceWeights);
}

void MetaHumanCreatorBodyAPI::Evaluate(State& State) const
{
    m->ptr.Solve(*State.m->ptr);
    State.m->legacyBodyIndex = -1;
}

void MetaHumanCreatorBodyAPI::EvaluateConstraintRange(const State& State, av::ArrayView<float> MinValues, av::ArrayView<float> MaxValues) const
{
    m->ptr.EvaluateConstraintRange(*State.m->ptr, MinValues, MaxValues);
}

void MetaHumanCreatorBodyAPI::StateToDna(const State& State, dna::Writer* InOutDnaWriter, bool combinedBodyAndFace) const
{
    m->ptr.StateToDna(*State.m->ptr, InOutDnaWriter, combinedBodyAndFace);
}

void MetaHumanCreatorBodyAPI::DumpState(const State& State, trio::BoundedIOStream* Stream) const
{
    m->ptr.DumpState(*State.m->ptr, Stream);
}

bool MetaHumanCreatorBodyAPI::RestoreState(trio::BoundedIOStream* Stream, std::shared_ptr<MetaHumanCreatorBodyAPI::State> OutState) const
{
    try
    {
        TITAN_RESET_ERROR;
        OutState->m->ptr = m->ptr.RestoreState(Stream);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to restore body state: {}", e.what());
    }

    return true;
}

int MetaHumanCreatorBodyAPI::NumLegacyBodies() const
{
    return (int)m->legacyBodies.size();
}

int MetaHumanCreatorBodyAPI::NumLODs() const
{
    return m->ptr.NumLODs();
}

int MetaHumanCreatorBodyAPI::NumPhysicsBodyVolumes(const::std::string& JointName) const
{
    return static_cast<int>(m->physicsBodiesVolumes[JointName].size());
}

bool MetaHumanCreatorBodyAPI::GetPhysicsBodyBoundingBox(const State& State, const::std::string& JointName, int BodyVolumeIndex, Eigen::Vector3f& OutCenter, Eigen::Vector3f& OutExtents) const
{
    try
    {
        TITAN_RESET_ERROR;
        PhysicsVolumeDefinition physicsVolumeDefinition = m->physicsBodiesVolumes[JointName][BodyVolumeIndex];

        int jointIndex = m->ptr.GetJointIndex(JointName);
        const Eigen::Transform<float, 3, Eigen::Affine>& jointTransform = State.m->ptr->GetJointBindMatrices().at(jointIndex);

        const size_t numVertexExtents = physicsVolumeDefinition.vertexIndices.size();
        const size_t extentSize = numVertexExtents + physicsVolumeDefinition.extentJointsAndScale.size();
        Eigen::Matrix<float, 3, -1> BodyVertexExtents(3, extentSize);

        constexpr int lodIndex = 0;
        float* VertexDataPtr = (float*)State.GetMesh(lodIndex).data();
        Eigen::Matrix<float, 3, -1> Vertices = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(VertexDataPtr, 3, State.GetMesh(lodIndex).size() / 3);
        for (size_t i = 0; i < numVertexExtents; i++)
        {
            int VertexIndex = physicsVolumeDefinition.vertexIndices[i];
            BodyVertexExtents.col(i) = jointTransform.inverse() * Vertices.col(VertexIndex);
        }

        auto GetJointExtentLocalPos = [&State, &jointTransform] (const int extentJointIndex, const float scaleFactor)
        {
            const Eigen::Transform<float, 3, Eigen::Affine>& extentJointTransform = State.m->ptr->GetJointBindMatrices()[extentJointIndex];
            Eigen::Vector3f extentLocalPos = (jointTransform.inverse() * extentJointTransform.translation()) * scaleFactor;
            return extentLocalPos;
        };

        for (size_t i = 0; i < physicsVolumeDefinition.extentJointsAndScale.size(); i++)
        {
            const int extentJointIndex = m->ptr.GetJointIndex(physicsVolumeDefinition.extentJointsAndScale[i].first);
            const float scaleFactor = physicsVolumeDefinition.extentJointsAndScale[i].second;
            BodyVertexExtents.col(numVertexExtents + i) = GetJointExtentLocalPos(extentJointIndex, scaleFactor);
        }

        Eigen::Vector3f boxMin = BodyVertexExtents.rowwise().minCoeff();
        Eigen::Vector3f boxMax = BodyVertexExtents.rowwise().maxCoeff();

        OutCenter = (boxMax + boxMin) * 0.5f;
        OutExtents = boxMax - boxMin;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to bounding box: {}", e.what());
    }

    return true;
}

int MetaHumanCreatorBodyAPI::NumJoints() const
{
	return m->ptr.NumJoints();
}

void MetaHumanCreatorBodyAPI::GetNeutralJointTransform(const State& State, std::uint16_t JointIndex, Eigen::Vector3f& OutJointTranslation, Eigen::Vector3f& OutJointRotation) const
{
	m->ptr.GetNeutralJointTransform(*State.m->ptr, JointIndex, OutJointTranslation, OutJointRotation);
}

const std::string& MetaHumanCreatorBodyAPI::LegacyBodyName(int LegacyBodyIndex) const
{
    if ((LegacyBodyIndex >= 0) && (LegacyBodyIndex < NumLegacyBodies()))
    {
        return m->legacyBodiesNames[LegacyBodyIndex];
    }
    static const std::string tmp = "";
    return tmp;
}

void MetaHumanCreatorBodyAPI::SelectLegacyBody(State& State, int LegacyBodyIndex, bool Fit) const
{
    if ((LegacyBodyIndex >= 0) && (LegacyBodyIndex < NumLegacyBodies()))
    {
        auto newBodyShapeState = std::make_shared<BodyShapeEditor::State>(*State.m->ptr);
        m->ptr.SetCustomGeometryToState(*newBodyShapeState, m->legacyBodies[LegacyBodyIndex], Fit);

        for (int constraintIndex = 0; constraintIndex < newBodyShapeState->GetConstraintNum(); constraintIndex++)
        {
            newBodyShapeState->RemoveConstraintTarget(constraintIndex);
        }

        State.m->ptr = newBodyShapeState;
    }
}

int MetaHumanCreatorBodyAPI::NumPresetBodies() const
{
    return (int)m->presetNames.size();
}

const std::vector<std::string>& MetaHumanCreatorBodyAPI::GetPresetNames() const
{
    return m->presetNames;
}

const std::string& MetaHumanCreatorBodyAPI::PresetBodyName(int PresetBodyIndex) const
{
    return m->presetNames[PresetBodyIndex];
}

int MetaHumanCreatorBodyAPI::NumGizmos() const
{
    return static_cast<int>(m->regionVertexIndices.size());
}

bool MetaHumanCreatorBodyAPI::EvaluateGizmos(const State& State, float* OutGizmos) const
{
    try
    {
        TITAN_RESET_ERROR;
        Eigen::Map<const Eigen::Matrix3Xf> vertices(State.GetMesh(0).data(), 3, State.GetMesh(0).size());    	
        Eigen::Map<Eigen::Matrix3Xf> outGizmos(OutGizmos, 3, NumGizmos());
        for (int gizmoIndex = 0; gizmoIndex < (int)m->regionVertexIndices.size(); ++gizmoIndex)
        {
            const int vID = m->regionVertexIndices[gizmoIndex];
            if (vID >= 0)
            {
                outGizmos.col(gizmoIndex) = vertices.col(vID);
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

const std::vector<std::string>& MetaHumanCreatorBodyAPI::GetRegionNames() const
{
    return m->ptr.GetRegionNames();
}


bool MetaHumanCreatorBodyAPI::SelectPreset(State& state, int RegionIndex, const std::string& PresetName, BodyAttribute Type) const
{
    try
    {
        TITAN_RESET_ERROR;
        return BlendPresets(state, RegionIndex, { { 1.0f, PresetName } }, Type);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to select preset: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::BlendPresets(State& state, int RegionIndex, const std::vector<std::pair<float, std::string>>& alphaAndPresetNames, BodyAttribute Type) const
{
    try
    {
        TITAN_RESET_ERROR;
        std::vector<std::pair<float, const State*>> alphaAndStates;
        for (const auto& [alpha, name] : alphaAndPresetNames)
        {
            auto it = m->presetStates.find(name);
            if (it != m->presetStates.end())
            {
                alphaAndStates.push_back({ alpha, it->second.get() });
            }
        }
        return Blend(state, RegionIndex, alphaAndStates, Type);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to reset state: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::Blend(State& state, int RegionIndex, const std::vector<std::pair<float, const State*>>& states, BodyAttribute Type) const
{
    try
    {
        TITAN_RESET_ERROR;

        auto newState = std::make_shared<BodyShapeEditor::State>(*state.m->ptr);
        std::vector<std::pair<float, const BodyShapeEditor::State*>> bodyStates;
        for (const auto& [alpha, s] : states)
        {
            bodyStates.push_back({alpha, s->m->ptr.get()});
        }

        if (m->ptr.Blend(*newState, RegionIndex, bodyStates, (BodyShapeEditor::BodyAttribute)Type))
        {
            state.m->ptr = newState;
            return true;
        }
        return false;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to blend body states: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::SetVertexDeltaScale(State& state, float VertexDeltaScale) const
{
    try
    {
        TITAN_RESET_ERROR;
        auto newState = std::make_shared<BodyShapeEditor::State>(*state.m->ptr);
        newState->SetVertexDeltaScale(VertexDeltaScale);
        m->ptr.EvaluateState(*newState, state.m->ptr->GetApplyFloorOffset());
        state.m->ptr = newState;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set vertex delta scale: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::FitToTarget(State& state, const FitToTargetOptions& options, const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices, const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints) const
{
    try
    {
        TITAN_RESET_ERROR;

        auto newState = std::make_shared<BodyShapeEditor::State>(*state.m->ptr);
        BodyShapeEditor::FitToTargetOptions bseOptions;
        bseOptions.fitSkeleton = options.fitSkeleton;
        bseOptions.fitRigidAndScale = options.fitRigidAndScale;
        bseOptions.optimizeEdges = false;
        bseOptions.snapToFloor = options.snapToFloor;

        m->ptr.FitToTarget(*newState, bseOptions, InVertices, InJoints);

        for (int constraintIndex = 0; constraintIndex < newState->GetConstraintNum(); constraintIndex++)
        {
            newState->RemoveConstraintTarget(constraintIndex);
        }
        
        state.m->ptr = newState;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit body to target: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::FitToTarget(State& state, const FitToTargetOptions& options, bool fitFromJoints, const dna::Reader* InDnaReader) const
{
    try
    {
        TITAN_RESET_ERROR;

        RigGeometry<float> rigGeometry;
        TITAN_CHECK_OR_RETURN(rigGeometry.Init(InDnaReader, true), false, "cannot load rig geometry");
        Eigen::Ref<const Eigen::Matrix<float, 3, -1>> vertices = rigGeometry.GetMesh(0).Vertices();

        Eigen::Matrix3Xf joints = Eigen::Matrix3Xf::Zero(3, 0);
        if (fitFromJoints)
        {
            joints = Eigen::Matrix3Xf::Zero(3, rigGeometry.GetJointRig().NumJoints());
            for (int i = 0; i < rigGeometry.GetJointRig().NumJoints(); ++i)
            {
                joints.col(i) = Affine<float,3,3>(rigGeometry.GetBindMatrix(i)).Translation();
            }
        }

        return FitToTarget(state, options, vertices, joints);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit to target: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> combinedBodyAndFaceVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const
{
    try
    {
        return m->ptr.GetMeasurements(combinedBodyAndFaceVertices, measurements, measurementNames);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to get measurements: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceVertices, Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyVertices, Eigen::VectorXf& measurements, std::vector<std::string>& measurementNames) const
{
    try
    {
        TITAN_RESET_ERROR;
        return m->ptr.GetMeasurements(faceVertices, bodyVertices, measurements, measurementNames);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to get measurements: {}", e.what());
    }
}

av::ConstArrayView<float> MetaHumanCreatorBodyAPI::State::GetMesh(int lod) const
{
    return m->ptr->GetMesh(lod).Vertices();
}

av::ConstArrayView<float> MetaHumanCreatorBodyAPI::State::GetMeshNormals(int lod) const
{
    return m->ptr->GetMesh(lod).VertexNormals();
}

av::ConstArrayView<float> MetaHumanCreatorBodyAPI::State::GetBindPose() const
{
    av::ConstArrayView<float> view((const float*)m->ptr->GetJointBindMatrices().data(),
                                   sizeof(Eigen::Transform<float, 3, Eigen::Affine>) / sizeof(float) * m->ptr->GetJointBindMatrices().size());
    return view;
}

av::ConstArrayView<float> MetaHumanCreatorBodyAPI::State::GetMeasurements() const
{
    return m->ptr->GetNamedConstraintMeasurements();
}

av::ConstArrayView<float> MetaHumanCreatorBodyAPI::State::GetPose() const
{
    return m->ptr->GetPose();
}

Eigen::Matrix3Xf MetaHumanCreatorBodyAPI::State::GetContourVertices(int ConstraintIndex) const
{
    return m->ptr->GetContourVertices(ConstraintIndex);
}

Eigen::Matrix3Xf MetaHumanCreatorBodyAPI::State::GetContourDebugVertices(int ConstraintIndex) const
{
    return m->ptr->GetContourDebugVertices(ConstraintIndex);
}

int MetaHumanCreatorBodyAPI::State::GetConstraintNum() const
{
    return m->ptr->GetConstraintNum();
}

const std::string& MetaHumanCreatorBodyAPI::State::GetConstraintName(int ConstraintIndex) const
{
    return m->ptr->GetConstraintName(ConstraintIndex);
}

bool MetaHumanCreatorBodyAPI::State::GetConstraintTarget(int ConstraintIndex, float& OutTarget) const
{
    return m->ptr->GetConstraintTarget(ConstraintIndex, OutTarget);
}

bool MetaHumanCreatorBodyAPI::State::SetConstraintTarget(int ConstraintIndex, float Target)
{
    try
    {
        TITAN_RESET_ERROR;
        auto newBodyShapeState = std::make_shared<BodyShapeEditor::State>(*m->ptr);

        newBodyShapeState->SetConstraintTarget(ConstraintIndex, Target);

        m->ptr = newBodyShapeState;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set target constraint: {}", e.what());
    }
}

bool MetaHumanCreatorBodyAPI::State::RemoveConstraintTarget(int ConstraintIndex)
{
    try
    {
        TITAN_RESET_ERROR;
        auto newBodyShapeState = std::make_shared<BodyShapeEditor::State>(*m->ptr);

        newBodyShapeState->RemoveConstraintTarget(ConstraintIndex);

        m->ptr = newBodyShapeState;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to remove target constraint: {}", e.what());
    }
}

float MetaHumanCreatorBodyAPI::State::VertexDeltaScale() const
{
    return m->ptr->VertexDeltaScale();
}

} // namespace TITAN_API_NAMESPACE