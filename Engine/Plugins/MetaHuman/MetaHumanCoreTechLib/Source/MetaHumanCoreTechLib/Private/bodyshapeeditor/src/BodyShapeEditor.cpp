// Copyright Epic Games, Inc. All Rights Reserved.

#include "bodyshapeeditor/BodyShapeEditor.h"
#include "bodyshapeeditor/BodyMeasurement.h"
#include "bodyshapeeditor/SerializationHelper.h"
#include "dna/Reader.h"
#include "rig/RigLogic.h"

#include <Eigen/src/Core/Map.h>
#include <arrayview/ArrayView.h>
#include <carbon/Algorithm.h>
#include <carbon/common/Log.h>
#include <carbon/utils/ObjectPool.h>
#include <carbon/utils/StringReplace.h>
#include <carbon/utils/StringUtils.h>
#include <carbon/utils/Timer.h>
#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <rig/BodyGeometry.h>
#include <rig/BodyLogic.h>
#include <rig/RBFLogic.h>
#include <rig/SymmetricControls.h>
#include <rig/SkinningWeightUtils.h>
#include <carbon/common/Defs.h>
#include <nls/Context.h>
#include <nls/Cost.h>
#include <nrr/deformation_models/DeformationModelVertex.h>
#include <nls/geometry/EulerAngles.h>
#include <nls/geometry/Procrustes.h>
#include <nls/geometry/Quaternion.h>
#include <nls/BoundedVectorVariable.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/DiffData.h>
#include <nls/math/Math.h>
#include <nls/math/ParallelBLAS.h>
#include <nls/VectorVariable.h>
#include <nls/functions/LimitConstraintFunction.h>
#include <nls/functions/ProjectionConstraintFunction.h>
#include <nls/functions/AxisConstraintFunction.h>
#include <nls/functions/LengthConstraintFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/geometry/Mesh.h>
#include <nls/solver/SimpleGaussNewtonSolver.h>

#include <Eigen/src/Core/Matrix.h>
#include <carbon/io/JsonIO.h>


#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace
{

struct SparseMatrixPCA
{
    SparseMatrix<float> mean;
    std::vector<Eigen::MatrixXf> mods;
    std::vector<std::vector<int>> rowsPerPart;
    std::vector<std::vector<int>> colIndicesPerRow;
    Eigen::MatrixXf globalToMods;


    int pcaModCount()
    {
        int i = 0;
        for (const auto& m : mods)
        {
            i += m.cols();
        }
        return i;
    }

    int numColsForRows(const std::vector<int>& rows)
    {
        int totalCols = 0;
        for (int ri : rows)
        {
            totalCols += static_cast<int>(colIndicesPerRow[ri].size());
        }
        return totalCols;
    }

    void ReadFromDNA(const dna::Reader* reader, std::string model_name)
    {
        auto modelStr64 = reader->getMetaDataValue(model_name.c_str());
        if (modelStr64.size() == 0u)
        {
            return;
        }
        auto modelStr = TITAN_NAMESPACE::Base64Decode(std::string{ modelStr64.begin(), modelStr64.end() });
        auto stream = pma::makeScoped<trio::MemoryStream>(modelStr.size());
        stream->open();
        stream->write(modelStr.data(), modelStr.size());
        stream->seek(0);
        modelStr.clear();
        terse::BinaryInputArchive<trio::MemoryStream> archive{ stream.get() };

        std::vector<std::uint32_t> rows;
        std::vector<std::uint32_t> cols;
        std::vector<float> values;

        SparseMatrix<float>::Index colCount;
        SparseMatrix<float>::Index rowCount;
        archive(colCount);
        archive(rowCount);
        archive(rows);
        archive(cols);
        archive(values);
        CARBON_ASSERT(rows.size() == cols.size(), "Model matrix has wrong entries");
        CARBON_ASSERT(rows.size() == values.size(), "Model matrix has wrong entries");
        std::vector<Eigen::Triplet<float>> triplets;
        triplets.reserve(values.size());
        for (size_t j = 0; j < rows.size(); j++)
        {
            triplets.push_back(Eigen::Triplet<float>(rows[j], cols[j], values[j]));
        }
        mean.resize(rowCount, colCount);
        mean.setFromTriplets(triplets.begin(), triplets.end());


        auto archiveDynMatrix = [&](Eigen::MatrixXf& matrix) {
                Eigen::MatrixXf::Index colCount;
                Eigen::MatrixXf::Index rowCount;
                archive(colCount);
                archive(rowCount);
                std::vector<float> values;
                archive(values);
                matrix = Eigen::Map<Eigen::MatrixXf>{ values.data(), rowCount, colCount };
            };

        decltype(mods)::size_type modCount;
        archive(modCount);

        mods.resize(modCount);
        for (std::size_t mi = 0u; mi < modCount; ++mi)
        {
            archiveDynMatrix(mods[mi]);
        }

        archive(rowsPerPart);
        archive(colIndicesPerRow);
        archiveDynMatrix(globalToMods);
    }

    SparseMatrix<float> calculateResult(const Eigen::VectorXf& global)
    {
        auto pcaCoeffAllRegions = globalToMods * global;
        std::size_t inputOffset = 0;
        auto result = mean;
        for (int ri = 0; ri < mods.size(); ++ri)
        {
            const auto& mod = mods[ri];
            auto pcaCoeff = pcaCoeffAllRegions.middleRows(inputOffset, mod.cols()).transpose().array();
            Eigen::VectorXf regionResult = mod.col(0) * pcaCoeff[0];

            for (int mi = 1; mi < mod.cols(); ++mi)
            {
                regionResult += mod.col(mi) * pcaCoeff[mi];
            }

            inputOffset += mod.cols();

            int jOffset = 0;
            for (int i = 0; i < rowsPerPart[ri].size(); ++i)
            {
                const auto rowIndex = rowsPerPart[ri][i];
                for (std::uint16_t j = 0; j < colIndicesPerRow[rowIndex].size(); ++j)
                {
                    const auto ji = colIndicesPerRow[rowIndex][j];
                    result.coeffRef(rowIndex, ji) += regionResult[jOffset];
                    jOffset++;
                }
            }
        }
        return result;
    }
};

} // namespace

struct BodyShapeEditor::State::Private
{
    Eigen::VectorXf RawControls;
    Eigen::Vector3f ModelTranslation = Eigen::Vector3f::Zero();
    Eigen::Matrix<float, 3, -1> VertexDeltas;
    Eigen::Matrix<float, 3, -1> JointDeltas;
    float VertexDeltaScale{ 1.0f };

    Eigen::VectorXf GuiControls;
    std::vector<Mesh<float>> RigMeshes;
    std::vector<Eigen::Transform<float, 3, Eigen::Affine>> JointBindMatrices;
    std::vector<BodyMeasurement> Constraints;
    //! evaluated measurements of the current state
    Eigen::VectorXf ConstraintMeasurements;
    //! user specified target measurements
    std::vector<std::pair<int, float>> TargetMeasurements;
    bool UseSymmetry = true;
    float SemanticWeight = 10.0f;
    bool FloorOffsetApplied = true;
	std::string ModelVersion;

    // tmp
    //! gui controls prior (e.g. from blending or from template2MH)
    Eigen::VectorXf GuiControlsPrior;
};

BodyShapeEditor::State::State() : m{new Private()}
{}

BodyShapeEditor::State::~State()
{}

BodyShapeEditor::State::State(const State& other)
    : m(new Private(*other.m))
{}

void BodyShapeEditor::State::SetSymmetry(const bool sym) { m->UseSymmetry = sym; }
bool BodyShapeEditor::State::GetSymmetric() const { return m->UseSymmetry; }
float BodyShapeEditor::State::GetSemanticWeight() { return m->SemanticWeight; }
void BodyShapeEditor::State::SetSemanticWeight(float weight) { m->SemanticWeight = weight; }

bool BodyShapeEditor::State::GetApplyFloorOffset() const { return m->FloorOffsetApplied; }

float BodyShapeEditor::State::VertexDeltaScale() const { return m->VertexDeltaScale; }
void BodyShapeEditor::State::SetVertexDeltaScale(float VertexDeltaScale) { m->VertexDeltaScale = VertexDeltaScale; }

const Eigen::VectorX<float>& BodyShapeEditor::State::GetPose() const
{ return m->GuiControls; }

const Mesh<float>& BodyShapeEditor::State::GetMesh(int lod) const
{ return m->RigMeshes[static_cast<size_t>(lod)]; }

const std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& BodyShapeEditor::State::GetJointBindMatrices() const
{ return m->JointBindMatrices; }

const Eigen::VectorXf& BodyShapeEditor::State::GetNamedConstraintMeasurements() const
{
    if (m->ConstraintMeasurements.size() == 0)
    {
        m->ConstraintMeasurements = BodyMeasurement::GetBodyMeasurements(m->Constraints, m->RigMeshes[0].Vertices(), m->RawControls);
    }
    return m->ConstraintMeasurements;
}

Eigen::Matrix3Xf BodyShapeEditor::State::GetContourVertices(int ConstraintIndex) const
{
    if (m->Constraints.size() <= ConstraintIndex)
    {
        CARBON_CRITICAL("Invalid ConstraintIndex");
    }
    return m->Constraints[ConstraintIndex].GetMeasurementPoints();
}

Eigen::Matrix3Xf BodyShapeEditor::State::GetContourDebugVertices(int ConstraintIndex) const
{
    if (m->Constraints.size() <= ConstraintIndex)
    {
        CARBON_CRITICAL("Invalid ConstraintIndex");
    }
    return m->Constraints[ConstraintIndex].GetMeasurementDebugPoints(m->RigMeshes[0].Vertices());
}

void BodyShapeEditor::State::Reset()
{
    m->RawControls.setZero();
    m->VertexDeltas.setZero();
    m->GuiControls.setZero();
    m->TargetMeasurements.clear();
    m->VertexDeltaScale = 1.0f;
    m->GuiControlsPrior.setZero();
}

int BodyShapeEditor::State::GetConstraintNum() const
{
    return static_cast<int>(m->Constraints.size());
}

const std::string& BodyShapeEditor::State::GetConstraintName(int ConstraintIndex) const
{
    if (m->Constraints.size() <= ConstraintIndex)
    {
        CARBON_CRITICAL("Invalid ConstraintIndex");
    }
    return m->Constraints[ConstraintIndex].GetName();
}

bool BodyShapeEditor::State::GetConstraintTarget(int ConstraintIndex, float& OutTarget) const
{
    auto it = std::find_if(m->TargetMeasurements.begin(), m->TargetMeasurements.end(),
                           [ConstraintIndex](const std::pair<int, float>& el) {
        return el.first == ConstraintIndex;
    });

    if (it != m->TargetMeasurements.end())
    {
        OutTarget = it->second;
        return true;
    }

    return false;
}

void BodyShapeEditor::State::SetConstraintTarget(int ConstraintIndex, float Target)
{
    if (m->Constraints.size() <= ConstraintIndex)
    {
        CARBON_CRITICAL("Invalid ConstraintIndex");
    }
    std::pair<int, float> TargetMeasurement{ ConstraintIndex, Target };
    auto it = std::lower_bound(m->TargetMeasurements.begin(), m->TargetMeasurements.end(), TargetMeasurement,
                               [](const std::pair<int, float>& elA, const std::pair<int, float>& elB) {
        return elA.first < elB.first;
    });

    if (it != m->TargetMeasurements.end())
    {
        if (it->first == ConstraintIndex)
        {
            it->second = Target;
            return;
        }
    }
    m->TargetMeasurements.insert(it, TargetMeasurement);
}

void BodyShapeEditor::State::RemoveConstraintTarget(int ConstraintIndex)
{
    if (m->Constraints.size() <= ConstraintIndex)
    {
        CARBON_CRITICAL("Invalid ConstraintIndex");
    }
    auto it = std::find_if(m->TargetMeasurements.begin(), m->TargetMeasurements.end(),
                           [ConstraintIndex](const std::pair<int, float>& el) {
        return el.first == ConstraintIndex;
    });

    if (it != m->TargetMeasurements.end())
    {
        m->TargetMeasurements.erase(it);
    }
}

struct BodyShapeEditor::Private
{
    std::unique_ptr<SymmetricControls<float>> symControls;
    std::shared_ptr<BodyLogic<float>> rigLogic;
    std::shared_ptr<BodyGeometry<float>> rigGeometry;
    std::shared_ptr<BodyGeometry<float>> combinedBodyArchetypeRigGeometry;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupInputIndices;
    std::vector<Eigen::VectorX<std::uint16_t>> jointGroupOutputIndices;
    std::string ModelVersion;
	std::vector<BodyMeasurement> Constraints;
    std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> solveSteps;
    std::vector<int> localIndices;
    std::vector<int> globalIndices;
    std::vector<int> poseIndices;
    std::vector<int> rawLocalIndices;
    std::vector<int> rawPoseIndices;
    std::vector<std::vector<int>> bodyToCombinedMapping;
    std::vector<std::map<int, int>> combinedToBodyMapping;
    std::shared_ptr<const LodGeneration<float>> combinedLodGenerationData;
    std::vector<Eigen::Matrix<int, 3, -1>> meshTriangles;
    SparseMatrix<float> gwm;
    std::map<std::string, std::pair<int, int>> helperJointMap;
    ObjectPool<BodyGeometry<float>::State> StatePool;
    ObjectPool<BodyGeometry<float>::State> StatePoolJacobian;
    std::shared_ptr<Mesh<float>> triTopology;
    std::shared_ptr<HalfEdgeMesh<float>> heTopology;
    std::shared_ptr<TaskThreadPool> threadPool;
    std::vector<float> minMeasurementInput;
    std::vector<float> maxMeasurementInput;

    std::vector<int> combinedFittingIndices;
    std::vector<std::vector<int>> neckSeamIndices;

    SparseMatrixPCA rbfPCA;
    SparseMatrixPCA skinWeightsPCA;
    //! region names
    std::vector<std::string> regionNames;

    //! map of skeleton pca region to affectedJoints
	std::map<std::string, std::set<int>> regionToJoints;
    //! map of skeleton pca region to raw controls
    std::map<std::string, std::vector<int>> skeletonPcaControls;
    //! map of shape pca region to raw controls
    std::map<std::string, std::vector<int>> shapePcaControls;
    //! symmetric mapping of pca regions
    std::map<std::string, std::string> symmetricPartMapping;
    //! mapping from raw to gui controls
    std::vector<int> rawToGuiControls;
    //! mapping from gui to raw controls
    std::vector<int> guiToRawControls;
    //! linear matrix mapping gui to raw controls: rawControls = guiToRawMapping * guiControls
    Eigen::SparseMatrix<float, Eigen::RowMajor> guiToRawMappingMatrix;
    // Eigen::MatrixXf guiToRawMappingMatrix;
    //! matrix to solve from raw to global gui controls
    Eigen::MatrixX<float> rawToGlobalGuiControlsSolveMatrix;
    //! vertex mask for each pca part
    std::map<std::string, VertexWeights<float>> partWeights;

    //! identity vertex evaluation matrix from raw controls
    Eigen::SparseMatrix<float, Eigen::RowMajor> identityVertexEvaluationMatrix;
    //! identity joint evlauation matrix from raw controls
    Eigen::SparseMatrix<float, Eigen::RowMajor> identityJointEvaluationMatrix;
    //! identity vertex evaluation matrix from symmetric controls
    Eigen::SparseMatrix<float, Eigen::RowMajor> symmetricIdentityVertexEvaluationMatrix;

    int floorIndex{ -1 };

    void CalculateCombinedLods(BodyShapeEditor::State& state) const;
    std::vector<int> maxSkinWeights = { 12, 8, 8, 4 };
    std::vector<std::map<std::string, std::map<std::string, float>>> jointSkinningWeightLodPropagationMap;
    std::vector<SnapConfig<float>> skinningWeightSnapConfigs;

    //! calculate the skinning weight snap config for the specified lod
    SnapConfig<float> CalcNeckSeamSkinningWeightsSnapConfig(int lod)const;

    public:
        static constexpr int32_t MagicNumber = 0x8c3b5f5e;
};

BodyShapeEditor::~BodyShapeEditor()
{
    delete m;
}

BodyShapeEditor::BodyShapeEditor() : m{new Private()}
{}


const std::vector<int>& BodyShapeEditor::GetMaxSkinWeights() const
{
    return m->maxSkinWeights;
}

void BodyShapeEditor::SetMaxSkinWeights(const std::vector<int>& maxSkinWeights) { m->maxSkinWeights = maxSkinWeights; }


void BodyShapeEditor::SetThreadPool(const std::shared_ptr<TaskThreadPool>& threadPool) { m->threadPool = threadPool; }

void BodyShapeEditor::Private::CalculateCombinedLods(BodyShapeEditor::State& state) const
{
    if (combinedLodGenerationData)
    {
        std::map<std::string, Eigen::Matrix<float, 3, -1>> lod0Vertices, higherLodVertices;
        const auto baseMeshes = combinedLodGenerationData->Lod0MeshNames();
        if (baseMeshes.size() != 1)
        {
            CARBON_CRITICAL("There should be 1 lod 0 mesh for the combined body model");
        }
        lod0Vertices[baseMeshes[0]] = state.m->RigMeshes[0].Vertices();

        bool bCalculatedLods = combinedLodGenerationData->Apply(lod0Vertices, higherLodVertices);
        if (!bCalculatedLods)
        {
            CARBON_CRITICAL("Failed to generate lods for the combined body model");
        }
        for (const auto& lodVertices : higherLodVertices)
        {
            int lod = combinedLodGenerationData->LodForMesh(lodVertices.first);
            state.m->RigMeshes[static_cast<size_t>(lod)].SetVertices(lodVertices.second);
            state.m->RigMeshes[static_cast<size_t>(lod)].CalculateVertexNormals(true, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/ true);
        }
    }
}

std::shared_ptr<BodyShapeEditor::State> BodyShapeEditor::CreateState() const
{
    auto state = std::shared_ptr<State>(new State());
    state->m->GuiControls = Eigen::VectorX<float>::Zero(m->rigLogic->NumGUIControls());
    state->m->Constraints = m->Constraints;
    state->m->JointBindMatrices = m->rigGeometry->GetBindMatrices();
	state->m->ModelVersion = m->ModelVersion;
    UpdateState(*state);

    return state;
}

void BodyShapeEditor::UpdateState(State& State, const Eigen::VectorXf& Pose) const
{
    if (Pose.size() != State.m->GuiControls.size())
    {
        CARBON_CRITICAL("pose has incorrect size: expected {}, but got {}", State.m->GuiControls.size(), Pose.size());
    }
    State.m->GuiControls = Pose;
    UpdateState(State);
}

void BodyShapeEditor::UpdateState(State& State) const
{
    State.m->RawControls = m->rigLogic->EvaluateRawControls(State.m->GuiControls).Value();
    EvaluateState(State, /*applyFloorOffset=*/ true);
}

void BodyShapeEditor::EvaluateState(State& State, bool applyFloorOffset) const
{
    Eigen::Matrix3Xf vertices;
    if (State.m->RawControls(m->rawPoseIndices).squaredNorm() > 0)
    {
        // evaluate using riglogic when poses are activated
        BodyGeometry<float>::State geometryState;
        const DiffData<float> joints = m->rigLogic->EvaluateJoints(0, State.m->RawControls);
        if (State.m->VertexDeltas.size() > 0)
        {
            m->rigGeometry->EvaluateBodyGeometryWithOffset(0, (State.m->VertexDeltaScale * State.m->VertexDeltas).colwise() + State.m->ModelTranslation, joints, State.m->RawControls, geometryState);
        }
        else
        {
            m->rigGeometry->EvaluateBodyGeometry(0, joints, State.m->RawControls, geometryState);
        }
        vertices = geometryState.Vertices().Matrix();

        // update bind matrices using the linear matrix as the pose does not affect the bind pose
        Eigen::VectorXf rawLocalControls = State.m->RawControls(m->rawLocalIndices);
        Eigen::VectorXf jointDeltas = m->identityJointEvaluationMatrix * rawLocalControls;
        for (int ji = 0; ji < (int)m->rigGeometry->GetBindMatrices().size(); ++ji)
        {
            State.m->JointBindMatrices[ji].translation() = jointDeltas.segment(3 * ji, 3) + m->rigGeometry->GetBindMatrices()[ji].translation() + State.m->ModelTranslation;
            if (State.m->JointDeltas.cols() > 0)
            {
                State.m->JointBindMatrices[ji].translation() += State.m->VertexDeltaScale * State.m->JointDeltas.col(ji);
            }
        }
    }
    else
    {
        // use linear matrix for activation
        const int numVertices = m->rigGeometry->GetMesh(0).NumVertices();
        Eigen::VectorXf rawLocalControls = State.m->RawControls(m->rawLocalIndices);
        if (m->threadPool)
        {
            vertices.resize(3, numVertices);
            ParallelNoAliasGEMV<float>(vertices.reshaped(), m->identityVertexEvaluationMatrix, rawLocalControls, m->threadPool.get());
            if (((int)State.m->VertexDeltas.cols() == m->rigGeometry->GetMesh(0).NumVertices()) && (State.m->VertexDeltaScale > 0))
            {
                vertices += m->rigGeometry->GetMesh(0).Vertices() + State.m->VertexDeltaScale * State.m->VertexDeltas;
            }
            else
            {
                vertices += m->rigGeometry->GetMesh(0).Vertices();
            }
            vertices.colwise() += State.m->ModelTranslation;
        }
        else
        {
            vertices = (m->identityVertexEvaluationMatrix * rawLocalControls + m->rigGeometry->GetMesh(0).Vertices().reshaped()).reshaped(3, numVertices);
            if (((int)State.m->VertexDeltas.cols() == m->rigGeometry->GetMesh(0).NumVertices()) && (State.m->VertexDeltaScale > 0))
            {
                vertices += State.m->VertexDeltaScale * State.m->VertexDeltas;
            }
            vertices.colwise() += State.m->ModelTranslation;
        }
        Eigen::VectorXf jointDeltas = m->identityJointEvaluationMatrix * rawLocalControls;
        for (int ji = 0; ji < (int)m->rigGeometry->GetBindMatrices().size(); ++ji)
        {
            State.m->JointBindMatrices[ji].translation() = jointDeltas.segment(3 * ji, 3) + m->rigGeometry->GetBindMatrices()[ji].translation() + State.m->ModelTranslation;
            if (State.m->JointDeltas.cols() > 0)
            {
                State.m->JointBindMatrices[ji].translation() += State.m->VertexDeltaScale * State.m->JointDeltas.col(ji);
            }
        }
    }

    State.m->FloorOffsetApplied = applyFloorOffset;
    if (applyFloorOffset)
    {
        // get floor position (using index or lowest vertex in the mesh) and move vertices and joints
        float floorOffset = 0;
        if (m->floorIndex >= 0)
        {
            floorOffset = vertices.row(1)[m->floorIndex];
        }
        else
        {
            floorOffset = vertices.row(1).minCoeff();
        }
        vertices.row(1).array() -= floorOffset;

        Eigen::Vector3f offsetTranslation(0.0f, floorOffset, 0.0f);
        for (int i = 1; i < (int)State.m->JointBindMatrices.size(); i++)
        {
            State.m->JointBindMatrices[i].translation() -= offsetTranslation;
        }
    }
    State.m->JointBindMatrices[0].translation() = Eigen::Vector3f::Zero();
    // make sure the rig meshes have the right triangulation
    State.m->RigMeshes.resize(m->meshTriangles.size());
    for (size_t i = 0; i < m->meshTriangles.size(); ++i)
    {
        if (State.m->RigMeshes[i].NumTriangles() != (int)m->meshTriangles[i].cols())
        {
            State.m->RigMeshes[i].SetTriangles(m->meshTriangles[i]);
        }
    }
    // update LOD0
    State.m->RigMeshes[0].SetVertices(vertices);
    State.m->RigMeshes[0].CalculateVertexNormals(true, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/ true, m->threadPool.get());

    // update other LODs
    m->CalculateCombinedLods(State);

    UpdateHelperJoints(vertices, State.m->JointBindMatrices);
    BodyMeasurement::UpdateBodyMeasurementPoints(State.m->Constraints, vertices, State.m->RigMeshes[0].VertexNormals(), *m->heTopology, nullptr);// m->threadPool.get());
    State.m->ConstraintMeasurements = BodyMeasurement::GetBodyMeasurements(State.m->Constraints, State.m->RigMeshes[0].Vertices(), State.m->RawControls);
}

void BodyShapeEditor::UpdateGuiFromRawControls(State& state) const
{
    const Eigen::VectorXf prevRawControls = state.m->RawControls;

    state.m->GuiControls = Eigen::VectorXf::Zero(state.m->GuiControls.size());
    state.m->GuiControls(m->globalIndices) = m->rawToGlobalGuiControlsSolveMatrix * prevRawControls;
    Eigen::VectorXf newRawControls = m->rigLogic->EvaluateRawControls(state.m->GuiControls).Value();
    for (int vID = 0; vID < (int)m->rawToGuiControls.size(); ++vID)
    {
        const int guiID = m->rawToGuiControls[vID];
        if (guiID >= 0)
        {
            state.m->GuiControls[guiID] += prevRawControls[vID] - newRawControls[vID];
        }
    }
}

int BodyShapeEditor::NumLODs() const
{
    if (!m->combinedLodGenerationData)
    {
        return 1;
    }
    else
    {
        return static_cast<int>(m->combinedLodGenerationData->HigherLodMeshNames().size()) + 1;
    }
}

std::vector<int> FindMissing(int totalInputs, const std::vector<int>& selected)
{
    std::vector<bool> isSelected(totalInputs, false);

    for (int control : selected)
    {
        isSelected[control] = true;
    }

    std::vector<int> missing;
    for (int i = 0; i < totalInputs; ++i)
    {
        if (!isSelected[i])
        {
            missing.push_back(i);
        }
    }

    return missing;
}

std::vector<int> NonZeroMaskVerticesIntersection(const std::vector<int>& mapping, const std::vector<int>& mask)
{
    std::unordered_set<int> maskSet(mask.begin(), mask.end());
    std::vector<int> result;

    for (int idx : mapping)
    {
        if (maskSet.contains(idx))
        {
            result.push_back(idx);
        }
    }

    return result;
}

int ClosestIndex(int queryIndex, std::vector<int>& targetIndices, const Eigen::Matrix<float, 3, -1>& vertexPositions)
{
    float distance = 1e5f;
    int resultIndex = -1;

    const Eigen::Vector3f queryVertex = vertexPositions.col(queryIndex);
    for (int i = 0; i < (int)targetIndices.size(); ++i)
    {
        const Eigen::Vector3f targetVertex = vertexPositions.col(targetIndices[i]);
        float currentDistance = (targetVertex - queryVertex).norm();
        if (currentDistance < distance)
        {
            distance = currentDistance;
            resultIndex = targetIndices[i];
        }
    }

    return resultIndex;
}

void BodyShapeEditor::SolveForTemplateMesh(Eigen::VectorXf& InOutResult,
                                           float& scale,
                                           Affine<float, 3, 3>& transform,
                                           Eigen::Vector3<float>& modelTranslation,
                                           const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices,
                                           const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints,
                                           const FitToTargetOptions& fitToTargetOptions,
                                           const std::vector<int>& vertexMapping)
{
    std::vector<int> controlsToFit;
    std::vector<int> fixedControls;

    scale = 1.0f;
    transform = Affine<float, 3, 3>();

    if (!fitToTargetOptions.fitSkeleton)
    {
        for (const auto& [name, indices] : m->skeletonPcaControls)
        {
            fixedControls.insert(fixedControls.end(), indices.begin(), indices.end());
        }
        fixedControls.insert(fixedControls.end(), m->globalIndices.begin(), m->globalIndices.end());
    }
    if (!fitToTargetOptions.fitShape)
    {
        for (const auto& [name, indices] : m->shapePcaControls)
        {
            fixedControls.insert(fixedControls.end(), indices.begin(), indices.end());
        }
        fixedControls.insert(fixedControls.end(), m->globalIndices.begin(), m->globalIndices.end());
    }

    controlsToFit = FindMissing(static_cast<int>(InOutResult.size()), fixedControls);

    if (controlsToFit.empty())
    {
        LOG_WARNING("No parameters to fit. Skipping FitToTarget.");
        return;
    }

    Eigen::VectorXf fixedResult = InOutResult(fixedControls);

    Eigen::MatrixXf identityEvaluationMatrix =
        Eigen::MatrixXf::Zero((InVertices.cols() >
                               0 ? m->identityVertexEvaluationMatrix.rows() : 0) + (InJoints.cols() > 0 ? m->identityJointEvaluationMatrix.rows() : 0),
                              m->identityVertexEvaluationMatrix.cols());

    if (InVertices.cols() > 0)
    {
        identityEvaluationMatrix.topRows(m->identityVertexEvaluationMatrix.rows()) = m->identityVertexEvaluationMatrix;
    }
    if (InJoints.cols() > 0)
    {
        identityEvaluationMatrix.bottomRows(m->identityJointEvaluationMatrix.rows()) = m->identityJointEvaluationMatrix;
    }

    std::vector<int> evaluationIndices(InVertices.size() + InJoints.size());
    for (int i = 0; i < (int)InVertices.cols(); ++i)
    {
         evaluationIndices[3 * i + 0] = 3 * vertexMapping[i] + 0;
         evaluationIndices[3 * i + 1] = 3 * vertexMapping[i] + 1;
         evaluationIndices[3 * i + 2] = 3 * vertexMapping[i] + 2;
    }

    for (int i = 0; i < (int)InJoints.cols(); ++i)
    {
        evaluationIndices[3 * (int)InVertices.cols() + 3 * i + 0] = (InVertices.cols() > 0 ? (int)m->identityVertexEvaluationMatrix.rows() : 0) + 3 * i + 0;
        evaluationIndices[3 * (int)InVertices.cols() + 3 * i + 1] = (InVertices.cols() > 0 ? (int)m->identityVertexEvaluationMatrix.rows() : 0) + 3 * i + 1;
        evaluationIndices[3 * (int)InVertices.cols() + 3 * i + 2] = (InVertices.cols() > 0 ? (int)m->identityVertexEvaluationMatrix.rows() : 0) + 3 * i + 2;
    }

    identityEvaluationMatrix *= m->guiToRawMappingMatrix;

    Eigen::MatrixXf A = identityEvaluationMatrix(evaluationIndices, controlsToFit);

    Eigen::Matrix3Xf target = Eigen::Matrix3Xf::Zero(3, InJoints.cols() + InVertices.cols());
    Eigen::Matrix3Xf meanVertices = Eigen::Matrix3Xf::Zero(3, InJoints.cols() + InVertices.cols());
    if (InVertices.cols() > 0)
    {
        target.leftCols(InVertices.cols()) = InVertices;
        meanVertices.leftCols(vertexMapping.size()) = m->rigGeometry->GetMesh(0).Vertices()(Eigen::all, vertexMapping);
    }
    if (InJoints.cols() > 0)
    {
        if (InJoints.cols() == m->rigGeometry->NumJoints())
        {
            target.rightCols(InJoints.cols()) = InJoints;
            Eigen::Matrix<float, 3, -1> jointPositions(3, (int)m->rigGeometry->GetBindMatrices().size());
            for (size_t i = 0; i < m->rigGeometry->GetBindMatrices().size(); ++i)
            {
                jointPositions.col(i) = m->rigGeometry->GetBindMatrices()[i].translation();
            }
            meanVertices.rightCols(InJoints.cols()) = jointPositions;
        }
    }

    if (fitToTargetOptions.fitRigidAndScale)
    {
        const auto& [pScale, pTransform] = Procrustes<float, 3>::AlignRigidAndScale(target, meanVertices);
        target = pTransform.Transform(pScale * target);
        scale = pScale;
        transform = pTransform;
    }

    if (fitToTargetOptions.fitSkeleton)
    {
        A.conservativeResize(A.rows(), A.cols() + 3);

        for (int vID = 0; vID < int(meanVertices.cols()); ++vID) {
            A.block(3 * vID, controlsToFit.size(), 3, 3) = Eigen::Matrix<float, 3, 3>::Identity();
        }
    }

    //if (fitToTargetOptions.optimizeEdges)
    //{
    //    // fit edges
    //    const std::vector<std::pair<int, int>> edges = m->rigGeometry->GetMesh(0).GetEdges(vertexMapping);
    //    std::vector<Eigen::Triplet<float>> triplets;
    //    for (int i = 0; i < (int)edges.size(); ++i)
    //    {
    //        for (int k = 0; k < 3; ++k)
    //        {
    //            triplets.push_back(Eigen::Triplet<float>(3 * i + k, 3 * edges[i].first + k, 1.0f));
    //            triplets.push_back(Eigen::Triplet<float>(3 * i + k, 3 * edges[i].second + k, -1.0f));
    //        }
    //    }
    //    Eigen::SparseMatrix<float> edgeMatrix(3 * edges.size(), evaluationMatrix.rows());
    //    edgeMatrix.setFromTriplets(triplets.begin(), triplets.end());
    //    Eigen::MatrixXf A = edgeMatrix * evaluationMatrix;
    //    const Eigen::VectorXf B = edgeMatrix * ((target.reshaped() - meanVertices.reshaped()) - fixedControlsMatrix * fixedResult);
    //    Eigen::MatrixXf I = Eigen::MatrixXf::Zero(A.cols(), A.cols());
    //    I.topLeftCorner(controlsToFit.size(), controlsToFit.size()) = Eigen::MatrixXf::Identity(controlsToFit.size(), controlsToFit.size());
          
    //    const Eigen::VectorXf x = (A.transpose() * A + fitToTargetOptions.regularization * I).llt().solve(A.transpose() * B);
    //    InOutResult(controlsToFit) = x.head(controlsToFit.size());
    //}
    Eigen::VectorXf B = (target.reshaped() - meanVertices.reshaped());
    if (!fixedControls.empty())
    {
        Eigen::MatrixXf fixedControlsMatrix = identityEvaluationMatrix(evaluationIndices, fixedControls);
        if (!fitToTargetOptions.fitSkeleton)
        {
            fixedControlsMatrix.conservativeResize(fixedControlsMatrix.rows(), fixedControlsMatrix.cols() + 3);

            for (int vID = 0; vID < int(meanVertices.cols()); ++vID) {
                fixedControlsMatrix.block(3 * vID, fixedControls.size(), 3, 3) = Eigen::Matrix<float, 3, 3>::Identity();
            }

            fixedResult.conservativeResize(fixedResult.size() + 3);
            fixedResult.tail(3) = modelTranslation;
        }

        B -= fixedControlsMatrix * fixedResult;
    }

    Eigen::MatrixXf I = Eigen::MatrixXf::Zero(A.cols(), A.cols());
    I.topLeftCorner(controlsToFit.size(), controlsToFit.size()) = Eigen::MatrixXf::Identity(controlsToFit.size(), controlsToFit.size());

    const Eigen::VectorXf x = (A.transpose() * A + fitToTargetOptions.regularization * I).llt().solve(A.transpose() * B);
    InOutResult(controlsToFit) = x.head(controlsToFit.size());

    if (fitToTargetOptions.fitSkeleton)
    {
        modelTranslation = x.tail(3);
    }
}

Eigen::VectorXf BodyShapeEditor::SolveForTemplateMesh(ConstArrayView<int> indices_,
                                                      const Eigen::Matrix<float, 3, -1>& targets,
                                                      float regularization,
                                                      int iterations)
{
    const Eigen::VectorXi targetIndices { Eigen::Map<const Eigen::VectorXi>{ indices_.data(), static_cast<Eigen::Index>(indices_.size()) } };
    std::shared_ptr<BodyGeometry<float>::State> geometryState = m->StatePoolJacobian.Aquire();
    std::vector<int> indices{ indices_.data(), indices_.data() + indices_.size() };
    Eigen::VectorXf m_targetWeights = Eigen::VectorXf::Ones(indices.size());
    VectorVariable<float> v{ m->rigLogic->NumGUIControls() };
    v.SetZero();

    // define cost function
    std::function<Cost<float>(Context<float>*)> costFunction = [&](Context<float>* context) {
            Cost<float> cost;

            // evaluate mesh
            DiffData<float> guiControls = v.Evaluate(context);


            const DiffData<float> rawControls = m->rigLogic->EvaluateRawControls(guiControls);
            DiffData<float> joints = m->rigLogic->EvaluateJoints(0, rawControls);
            m->rigGeometry->EvaluateIndexedBodyGeometry(0, joints, rawControls, indices, *geometryState);

            cost.Add(PointPointConstraintFunction<float, 3>::Evaluate(
                         geometryState->Vertices(),
                         targetIndices,
                         targets,
                         m_targetWeights,
                         1e-4f * 1.0f), 1.0f, "Keypoints");
            cost.Add(m->gwm * guiControls, regularization, "", false);
            return cost;
        };
    Context<float> context;
    SimpleGaussNewtonSolver<float> solver;
    solver.Solve(costFunction,
                 context,
                 iterations,
                 0.0f,
                 1e-4f,
                 0.0f,
                 TaskThreadPool::GlobalInstance(true).get());
    return v.Value();
}

void BodyShapeEditor::Init(std::shared_ptr<BodyLogic<float>> bodyLogic,
                           std::shared_ptr<BodyGeometry<float>> combinedBodyArchetypeGeometry,
                           std::shared_ptr<RigLogic<float>> CombinedBodyRigLogic,
                           std::shared_ptr<BodyGeometry<float>> bodyGeometry,
                           av::ConstArrayView<BodyMeasurement> contours,
                           const std::vector<std::map<std::string, std::map<std::string, float>>>& jointSkinningWeightLodPropagationMap,
                           const std::vector<int>& maxSkinWeightsPerVertexForEachLod,
                           std::shared_ptr<const LodGeneration<float>> combinedLodGenerationData,
                           const std::map<std::string, std::pair<int, int>>& helperJointMap,
                           const std::map<std::string, VertexWeights<float>>& partWeights)
{
    m->minMeasurementInput.clear();
    m->maxMeasurementInput.clear();
    m->rigLogic = bodyLogic;


    m->rigGeometry = bodyGeometry;
    m->combinedBodyArchetypeRigGeometry = combinedBodyArchetypeGeometry;
    m->helperJointMap = helperJointMap;
    m->combinedLodGenerationData = combinedLodGenerationData;
    m->partWeights = partWeights;
    if (!m->threadPool)
    {
        m->threadPool = TaskThreadPool::GlobalInstance(true);
    }

    m->rigGeometry->SetThreadPool(m->threadPool);

    m->Constraints.assign(contours.begin(), contours.end());

    const std::vector<std::string>& guiControlNames = m->rigLogic->GuiControlNames();
    const std::vector<std::string>& rawControlNames = m->rigLogic->RawControlNames();
    if (CombinedBodyRigLogic)
    {
        m->jointGroupInputIndices = CombinedBodyRigLogic->GetJointGroupInputIndices();
        m->jointGroupOutputIndices = CombinedBodyRigLogic->GetJointGroupOutputIndices();
    }

    m->localIndices.clear();
    m->globalIndices.clear();
    m->poseIndices.clear();
    {
        for (int i = 0; i < int(guiControlNames.size()); ++i)
        {
            const std::string& name = guiControlNames[i];
            if (name.find("global_") != name.npos)
            {
                m->globalIndices.emplace_back(i);
            }
            else if (name.find("local_") != name.npos)
            {
                m->localIndices.emplace_back(i);
            }
            else if (name.find("pose_") != name.npos)
            {
                m->poseIndices.emplace_back(i);
            }
            else
            {
                CARBON_CRITICAL("unknown control \"{}\"", name);
            }
        }
    }

    m->rawLocalIndices.clear();
    m->rawPoseIndices.clear();
    {
        for (int i = 0; i < int(rawControlNames.size()); ++i)
        {
            const std::string& name = rawControlNames[i];
            if (name.find("local_") != name.npos)
            {
                m->rawLocalIndices.emplace_back(i);
            }
            else if (name.find("pose_") != name.npos)
            {
                m->rawPoseIndices.emplace_back(i);
            }
            else
            {
                CARBON_CRITICAL("unknown raw control \"{}\"", name);
            }
        }
    }


	Eigen::SparseMatrix<float, Eigen::ColMajor> invertedJointMatrix = m->rigLogic->GetJointMatrix(0);

    std::map<std::string, std::vector<int>> skeletonPcaControls;
    std::map<std::string, std::vector<int>> shapePcaControls;
    std::map<std::string, std::string> symmetricPartMapping;
    std::set<std::string> regionNamesSet;
	std::vector<int> jointControls;
	std::vector<int> shapeControls;
    for (int i = 0; i < int(rawControlNames.size()); ++i)
    {
        std::string name = rawControlNames[i];
        size_t suffixPos = name.find_last_of('_');
        if (suffixPos != std::string::npos)
        {
            name = name.substr(0, suffixPos);
        }
        const bool isLeft = StringEndsWith(name, "_l");
        const bool isRight = StringEndsWith(name, "_r");
        // const bool isCenter = !isLeft && !isRight;
        const bool isPose = StringStartsWith(name, "pose");
        std::string partname = name;
        if (StringStartsWith(name, "local_joint_"))
        {
            // skeleton pca
            partname = ReplaceSubstring(partname, "local_", "");
            skeletonPcaControls[partname].push_back(i);
			jointControls.push_back(i);	

        	auto regionName = ReplaceSubstring(partname, "joint_", "");
        	for (Eigen::SparseMatrix<float, Eigen::ColMajor>::InnerIterator it(invertedJointMatrix, i); it; ++it) {
        		m->regionToJoints[regionName].emplace(it.row()/9);
        	}
        }
        else if (StringStartsWith(name, "local_"))
        {
            // shape pca
            partname = ReplaceSubstring(partname, "local_", "");
            shapePcaControls[partname].push_back(i);
        	shapeControls.push_back(i);
        }
        else if (!isPose)
        {
            LOG_ERROR("unknown control {}", rawControlNames[i]);
        }

        if (!isPose)
        {
            if (isLeft) { symmetricPartMapping[partname] = partname.substr(0, partname.size() - 2) + "_r"; }
            else if (isRight) { symmetricPartMapping[partname] = partname.substr(0, partname.size() - 2) + "_l"; }
            else { symmetricPartMapping[partname] = partname; }
            regionNamesSet.insert(partname);
        }
    }
    m->skeletonPcaControls = skeletonPcaControls;
    m->shapePcaControls = shapePcaControls;
    m->symmetricPartMapping = symmetricPartMapping;
    m->regionNames = std::vector<std::string>(regionNamesSet.begin(), regionNamesSet.end());

    m->rawToGuiControls = std::vector<int>(rawControlNames.size(), -1);
    m->guiToRawControls = std::vector<int>(guiControlNames.size(), -1);
    for (int i = 0; i < (int)rawControlNames.size(); ++i)
    {
        m->rawToGuiControls[i] = GetItemIndex<std::string>(m->rigLogic->GuiControlNames(), rawControlNames[i]);
    }
    for (int i = 0; i < (int)guiControlNames.size(); ++i)
    {
        m->guiToRawControls[i] = GetItemIndex<std::string>(m->rigLogic->RawControlNames(), guiControlNames[i]);
    }

    // get mapping matrix from gui to raw controls
    m->guiToRawMappingMatrix = Eigen::SparseMatrix<float, Eigen::RowMajor>(m->rigLogic->NumRawControls(), m->rigLogic->NumGUIControls());
    for (const auto& mapping : m->rigLogic->GuiToRawMapping())
    {
        m->guiToRawMappingMatrix.coeffRef(mapping.outputIndex, mapping.inputIndex) += mapping.slope;
        if (mapping.cut != 0)
        {
            CARBON_CRITICAL("invalid cut value {}", mapping.cut);
        }
    }
    m->guiToRawMappingMatrix.makeCompressed();

    {
        Eigen::MatrixXf A = Eigen::MatrixXf(m->guiToRawMappingMatrix)(Eigen::all, m->globalIndices);
        m->rawToGlobalGuiControlsSolveMatrix = (A.transpose() * A).inverse() * A.transpose();
    }

    m->meshTriangles.resize(NumLODs());
    // get lod 0 from the pca model rig geometry, as we will always have this (combinedBodyArchetypeRigGeometry is optional)
    Mesh<float> triMesh = m->rigGeometry->GetMesh(0);
    triMesh.Triangulate();
    m->meshTriangles[0] = triMesh.Triangles();
    m->triTopology = std::make_shared<Mesh<float>>(triMesh);
    m->heTopology = std::make_shared<HalfEdgeMesh<float>>(triMesh);

    if (m->combinedBodyArchetypeRigGeometry)
    {
        for (size_t i = 1; i < static_cast<size_t>(NumLODs()); ++i)
        {
            triMesh = m->combinedBodyArchetypeRigGeometry->GetMesh(static_cast<int>(i));
            triMesh.Triangulate();
            m->meshTriangles[i] = triMesh.Triangles();
        }
    }

    m->symControls = std::make_unique<SymmetricControls<float>>(*m->rigLogic);
    BoundedVectorVariable<float> pose { m->rigLogic->NumGUIControls() };
    Eigen::VectorXf guiWeight = Eigen::VectorXf::Zero(m->rigLogic->NumGUIControls());
    for (const auto& index : m->localIndices)
    {
	   guiWeight[index] = 1.0f; 
    }
    for (const auto& index : m->globalIndices)
    {
        guiWeight[index] = 0.33f;
    }
    m->gwm = SparseMatrix<float>(guiWeight.size(), guiWeight.size());
    m->gwm.setIdentity();
    m->gwm.diagonal() = guiWeight;

    m->maxSkinWeights = maxSkinWeightsPerVertexForEachLod;

    m->jointSkinningWeightLodPropagationMap = jointSkinningWeightLodPropagationMap;

    {
        // create a linear evaluation matrix
        Eigen::MatrixXf identityEvaluationMatrix = Eigen::MatrixXf::Zero(m->rigGeometry->GetMesh(0).NumVertices() * 3, (int)m->rawLocalIndices.size());
        Eigen::MatrixXf jointEvaluationMatrix = Eigen::MatrixXf::Zero(m->rigGeometry->GetBindMatrices().size() * 3, (int)m->rawLocalIndices.size());
        Eigen::VectorXf zeroRawControls = Eigen::VectorXf::Zero(m->rigLogic->NumRawControls());
        DiffData<float> zeroJoints = m->rigLogic->EvaluateJoints(0, zeroRawControls);
        BodyGeometry<float>::State zeroState;
        m->rigGeometry->EvaluateBodyGeometry(0, zeroJoints, zeroRawControls, zeroState);
        Eigen::Matrix3Xf zeroVertices = zeroState.Vertices().Matrix();

		auto calcVertexEvalMatrices = [&](int start, int end) {
			const auto& blendShapeMap = m->rigGeometry->GetBlendshapeMap(0);
			const auto& blendShapes = m->rigGeometry->GetBlendshapeMatrix(0);
			for (int i = start; i < end; ++i)
                        {
							const int rawControlIndex = shapeControls[i];
                            identityEvaluationMatrix.col(rawControlIndex) = blendShapes.col(blendShapeMap[rawControlIndex]);
                        }
                    };
    	
        auto calcJointMatrices = [&](int start, int end) {
                BodyGeometry<float>::State geometryState;
                for (int i = start; i < end; ++i)
                {
					const int rawControlIndex = jointControls[i];	
                    Eigen::VectorXf rawControls = Eigen::VectorXf::Zero(m->rigLogic->NumRawControls());
                    rawControls[rawControlIndex] = 1.0f;
                    DiffData<float> joints = m->rigLogic->EvaluateJoints(0, rawControls);
                    m->rigGeometry->EvaluateBodyGeometry(0, joints, rawControls, geometryState);
                	identityEvaluationMatrix.col(rawControlIndex) = (geometryState.Vertices().Matrix()-zeroVertices).reshaped();
                    for (int ji = 0; ji < (int)geometryState.GetWorldMatrices().size(); ++ji)
                    {
                        jointEvaluationMatrix.col(rawControlIndex).segment(3 * ji,
                                                             3) = geometryState.GetWorldMatrices()[ji].translation() -
                            m->rigGeometry->GetBindMatrices()[ji].translation();
                    }
                }
            };
    	
        m->threadPool->AddTaskRangeAndWait((int)jointControls.size(), calcJointMatrices);
		m->threadPool->AddTaskRangeAndWait((int)shapeControls.size(), calcVertexEvalMatrices);
        m->identityVertexEvaluationMatrix = identityEvaluationMatrix.sparseView(0, 0);
        m->identityVertexEvaluationMatrix.makeCompressed();

        m->identityJointEvaluationMatrix = jointEvaluationMatrix.sparseView(0, 0);
        m->identityJointEvaluationMatrix.makeCompressed();

        // create the identity evaluation matrix based on the symmetric constrols
        const auto& symToGuiMat = m->symControls->SymmetricToGuiControlsMatrix();
        const auto& guiToRawMat = m->guiToRawMappingMatrix;
        Eigen::SparseMatrix<float, Eigen::RowMajor> rawLocalIndicesMat(m->rawLocalIndices.size(), m->rawLocalIndices.size());
        for (int i = 0; i < m->rawLocalIndices.size(); ++i)
        {
            rawLocalIndicesMat.coeffRef(i, m->rawLocalIndices[i]) = 1.0f;
        }
        rawLocalIndicesMat.makeCompressed();
        m->symmetricIdentityVertexEvaluationMatrix = m->identityVertexEvaluationMatrix * (rawLocalIndicesMat * guiToRawMat * symToGuiMat);
    }

    m->Constraints = CreateState()->m->Constraints;

    // retrieve floor index
    for (size_t i = 0; i < m->Constraints.size(); ++i)
    {
        if (m->Constraints[i].GetName() == "Height")
        {
            m->floorIndex = m->Constraints[i].GetVertexIDs().front();
            for (int vID : m->Constraints[i].GetVertexIDs())
            {
                if (m->rigGeometry->GetMesh(0).Vertices().col(vID)[1] < m->rigGeometry->GetMesh(0).Vertices().col(m->floorIndex)[1])
                {
                    m->floorIndex = vID;
                }
            }
        }
    }
}

std::vector<std::vector<int>> ReadBodyToCombinedMapping(const JsonElement& json)
{
    std::vector<std::vector<int>> mappings {};

    if (!json.Contains("body_to_combined"))
    {
        CARBON_CRITICAL("Invalid json file. Missing \"body_to_combined\" mapping.");
    }

    const std::uint16_t LODCount = 4u;

    if (json["body_to_combined"].Size() == LODCount)
    {
        mappings = json["body_to_combined"].Get<std::vector<std::vector<int>>>();
    }
    else
    {
        mappings.push_back(json["body_to_combined"].Get<std::vector<int>>());
        mappings.resize(LODCount);
    }
    return mappings;
}

std::vector<std::vector<int>> ReadBodyToCombinedMapping(const std::string& jsonString)
{
    const JsonElement json = ReadJson(jsonString);
    return ReadBodyToCombinedMapping(json);
}

void BodyShapeEditor::Init(const dna::Reader* reader,
                           dna::Reader* InCombinedArchetypeBodyDnaReader,
                           const std::vector<std::map<std::string,
                                                      std::map<std::string,
                                                               float>>>& JointSkinningWeightLodPropagationMap,
                           const std::vector<int>& maxSkinWeightsPerVertexForEachLod,
                           std::shared_ptr<const LodGeneration<float>> combinedLodGenerationData)
{
    if (!m->threadPool)
    {
        m->threadPool = TaskThreadPool::GlobalInstance(true);
    }
    auto rigLogic = std::make_shared<BodyLogic<float>>();
    auto rigGeometry = std::make_shared<BodyGeometry<float>>(m->threadPool);
    std::shared_ptr<BodyGeometry<float>> combinedArchetypeRigGeometry = nullptr;
    std::shared_ptr<RigLogic<float>> combinedArchetypeRigLogic;
    if (!rigLogic->Init(reader))
    {
        CARBON_CRITICAL("failed to decode rig");
    }
    if (!rigGeometry->Init(reader))
    {
        CARBON_CRITICAL("failed to decode rig");
    }
    if (InCombinedArchetypeBodyDnaReader)
    {
        combinedArchetypeRigGeometry = std::make_shared<BodyGeometry<float>>(m->threadPool);
        if (!combinedArchetypeRigGeometry->Init(InCombinedArchetypeBodyDnaReader))
        {
            CARBON_CRITICAL("failed to decode body archetype");
        }
        combinedArchetypeRigLogic = std::make_shared<RigLogic<float>>();
        if (!combinedArchetypeRigLogic->Init(InCombinedArchetypeBodyDnaReader))
        {
            CARBON_CRITICAL("failed to decode body archetype");
        }
    }

    const auto pcaJsonStringView = reader->getMetaDataValue("pca_model");
    const JsonElement pcaModelJson = ReadJson(std::string { pcaJsonStringView.data(), pcaJsonStringView.size() });
    const std::vector<BodyMeasurement> contours = BodyMeasurement::FromJSON(pcaModelJson, rigGeometry->GetMesh(0).Vertices());
    std::map<std::string, std::pair<int, int>> helperJointMap;
    if (pcaModelJson.Contains("joint_correspondence"))
    {
        for (const auto& element : pcaModelJson["joint_correspondence"].Array())
        {
            const std::string jointName = element["joint_name"].String();
            const int jointIndex = rigGeometry->GetJointIndex(jointName);
            helperJointMap[jointName] = std::pair<int, int>(jointIndex, element["vID"].Get<int>());
        }
    }
    if (pcaModelJson.Contains("solve_hierarchy"))
    {
        m->solveSteps = pcaModelJson["solve_hierarchy"].Get<std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>>>();
    	for (auto& pair : m->solveSteps)
    	{
    		for (auto& name : pair.first)
    		{
    			name = BodyMeasurement::GetAlias(name);	
    		}
    	}
    	
    }
	if (pcaModelJson.Contains("model_version")){
		m->ModelVersion = pcaModelJson["model_version"].Get<std::string>();
	} else {
		m->ModelVersion = "0.4.4";
	}
    std::map<std::string, VertexWeights<float>> partWeights;
    if (pcaModelJson.Contains("part_weights"))
    {
        partWeights = VertexWeights<float>::LoadAllVertexWeights(pcaModelJson["part_weights"], rigGeometry->GetMesh(0).NumVertices());
    }
    m->skinWeightsPCA.ReadFromDNA(reader, "skin_model");
    m->rbfPCA.ReadFromDNA(reader, "rbf_model");

    Init(rigLogic,
         combinedArchetypeRigGeometry,
         combinedArchetypeRigLogic,
         rigGeometry,
         contours,
         JointSkinningWeightLodPropagationMap,
         maxSkinWeightsPerVertexForEachLod,
         combinedLodGenerationData,
         helperJointMap,
         partWeights);

    m->bodyToCombinedMapping = ReadBodyToCombinedMapping(pcaModelJson);

    // create the reverse mapping
    m->combinedToBodyMapping = std::vector<std::map<int, int>>(m->bodyToCombinedMapping.size());
    for (size_t lod = 0; lod < m->bodyToCombinedMapping.size(); ++lod)
    {
        for (size_t i = 0; i < m->bodyToCombinedMapping[lod].size(); ++i)
        {
            const int combinedIndex = m->bodyToCombinedMapping[lod][i];
            m->combinedToBodyMapping[lod][combinedIndex] = int(i);
        }
    }
}

void BodyShapeEditor::SetFittingVertexIDs(const std::vector<int>& vertexIds) { m->combinedFittingIndices = vertexIds; }

void BodyShapeEditor::SetNeckSeamVertexIDs(const std::vector<std::vector<int>>& vertexIds)
{
    m->neckSeamIndices = vertexIds;

    // set up skinning weight snap configs from the neck seam indices
    m->skinningWeightSnapConfigs.resize(m->combinedBodyArchetypeRigGeometry->GetNumLODs() - 1);
    for (int lod = 1; lod < m->combinedBodyArchetypeRigGeometry->GetNumLODs(); ++lod)
    {
        m->skinningWeightSnapConfigs[size_t(lod - 1)] = m->CalcNeckSeamSkinningWeightsSnapConfig(lod);
    }
}

void BodyShapeEditor::SetBodyToCombinedMapping(int lod, const std::vector<int>& bodyToCombinedMapping)
{
    if (lod >= m->bodyToCombinedMapping.size())
    {
        m->bodyToCombinedMapping.resize(lod + 1u);
    }
    m->bodyToCombinedMapping[lod].assign(bodyToCombinedMapping.begin(),
                                         bodyToCombinedMapping.end());
}

const std::vector<int>& BodyShapeEditor::GetBodyToCombinedMapping(int lod) const
{
    return m->bodyToCombinedMapping[lod];
}

void BodyShapeEditor::EvaluateConstraintRange(const State& State, av::ArrayView<float> MinValues, av::ArrayView<float> MaxValues) const
{
    const auto& Constraints = State.m->Constraints;
    if ((MinValues.size() != MaxValues.size()) || (MinValues.size() != Constraints.size()))
    {
        CARBON_CRITICAL("Output values buffer is not of right size");
    }
    if (!m->minMeasurementInput.empty())
    {
        std::copy_n(m->maxMeasurementInput.data(), m->maxMeasurementInput.size(), MaxValues.data());
        std::copy_n(m->minMeasurementInput.data(), m->minMeasurementInput.size(), MinValues.data());
        return;
    }

    m->maxMeasurementInput.resize(m->Constraints.size());
    m->minMeasurementInput.resize(m->Constraints.size());
    std::vector<int> missingIndices{};
    for (int i = 0; i < m->Constraints.size(); i++)
    {
        m->maxMeasurementInput[i] = m->Constraints[i].GetMaxInputValue();
        m->minMeasurementInput[i] = m->Constraints[i].GetMinInputValue();
        if ((m->minMeasurementInput[i] == BodyMeasurement::InvalidValue) || (m->maxMeasurementInput[i] == BodyMeasurement::InvalidValue))
        {
            missingIndices.push_back(i);
        }
    }

    if (missingIndices.empty())
    {
        return;
    }

    const auto GetMeasurements = [&](const Eigen::VectorXf& pose) {
            const DiffData<float> rawControls = m->rigLogic->EvaluateRawControls(pose);
            const DiffData<float> joints = m->rigLogic->EvaluateJoints(0, rawControls);
            BodyGeometry<float>::State state;
            m->rigGeometry->EvaluateBodyGeometry(0, joints, rawControls, state);
            return BodyMeasurement::GetBodyMeasurements(State.m->Constraints, state.Vertices().Matrix(), rawControls.Value());
        };
    Eigen::VectorXf minVals = Eigen::VectorXf::Constant(Constraints.size(), 1000000.0f);
    Eigen::VectorXf maxVals = Eigen::VectorXf::Constant(Constraints.size(), -1000000.0f);
    Eigen::VectorXf pose = Eigen::VectorXf::Zero(m->rigLogic->NumGUIControls());
    const float rawControlRange = 5.f;

    for (size_t i = 0; i < m->globalIndices.size(); i++)
    {
        pose.setZero();
        pose[m->globalIndices[i]] = rawControlRange;
        Eigen::VectorXf measurements = GetMeasurements(pose);
        minVals = minVals.cwiseMin(measurements);
        maxVals = maxVals.cwiseMax(measurements);
        pose[m->globalIndices[i]] = -rawControlRange;
        measurements = GetMeasurements(pose);
        minVals = minVals.cwiseMin(measurements);
        maxVals = maxVals.cwiseMax(measurements);
    }
    for (int i : missingIndices)
    {
        if (Constraints[i].GetType() == BodyMeasurement::type_t::Semantic)
        {
            minVals[i] *= 1.5f;
            maxVals[i] *= 1.5f;
        }
        m->minMeasurementInput[i] = minVals[i];
        m->maxMeasurementInput[i] = maxVals[i];
    }

    std::copy_n(m->maxMeasurementInput.data(), m->maxMeasurementInput.size(), MaxValues.data());
    std::copy_n(m->minMeasurementInput.data(), m->minMeasurementInput.size(), MinValues.data());
}

std::shared_ptr<BodyShapeEditor::State> BodyShapeEditor::RestoreState(trio::BoundedIOStream* InputStream)
{
    auto state = std::shared_ptr<State>(new State());
    state->m->GuiControls = Eigen::VectorX<float>::Zero(m->rigLogic->NumGUIControls());
    state->m->GuiControlsPrior = Eigen::VectorX<float>::Zero(m->rigLogic->NumGUIControls());
    state->m->RawControls = Eigen::VectorX<float>::Zero(m->rigLogic->NumRawControls());
    state->m->Constraints = m->Constraints;
    state->m->JointBindMatrices = m->rigGeometry->GetBindMatrices();

    uint64_t startPos = InputStream->tell();
    bool success = true;
    {
        MHCBinaryInputArchive archive(InputStream);

        int32_t MagicNumber = { -1 };
        int32_t Version = { -1 };
        archive(MagicNumber);
        archive(Version);
        if (MagicNumber != m->MagicNumber)
        {
            LOG_ERROR("stream does not contain a MHC body state");
            success = false;
        }
        if ((Version < 1) || (Version > 5))
        {
            LOG_ERROR("version {} is not supported", Version);
            success = false;
        }

        if (success)
        {
            if (Version > 3)
            {
                archive(state->m->ModelVersion);
            } else
            {
                state->m->ModelVersion = "0.4.4";
            }
            DeserializeEigenMatrix(archive, InputStream, state->m->GuiControls);
            if (state->m->GuiControls.size() != m->rigLogic->NumGUIControls())
            {
                state->m->GuiControls.setZero(m->rigLogic->NumGUIControls());
            }
            state->m->GuiControlsPrior = state->m->GuiControls;

            // Target Measurements
            if(Version > 3)
            {
                std::size_t targetMeasurementsSize;
                archive(targetMeasurementsSize);
                state->m->TargetMeasurements.reserve(targetMeasurementsSize);
                for (std::size_t i = 0; i < targetMeasurementsSize; i++)
                {
                    std::string targetName;
                    float targetValue;
                	
                    archive(targetName);
                	targetName = BodyMeasurement::GetAlias(targetName);
                	archive(targetValue);
                    auto constraintIter = std::find_if(m->Constraints.begin(), m->Constraints.end(), [&targetName](const auto& Constraint)
                    {
                        return Constraint.GetName() == targetName;
                    });
                    if (constraintIter != m->Constraints.end())
                    {
                        state->m->TargetMeasurements.push_back({std::distance(m->Constraints.begin(), constraintIter), targetValue});
                    }
                }
            } else
            {
                archive(state->m->TargetMeasurements);
                // We cant be sure if indices will be respected in newer version of model, hopefully by then everyone will migrate to 0.4.5
                if (m->ModelVersion != "0.4.5" && m->ModelVersion != "0.4.6")
                {
                    state->m->TargetMeasurements.clear();
                }
            }
            
            DeserializeEigenMatrix(archive, InputStream, state->m->VertexDeltas);
            if (Version > 4)
            {
                DeserializeEigenMatrix(archive, InputStream, state->m->JointDeltas);
            }
            Eigen::Matrix<float, 3, -1> vertices;
            DeserializeEigenMatrix(archive, InputStream, vertices);
            Eigen::Matrix<float, 3, -1> jointPositions;
            if (Version > 1)
            {
                DeserializeEigenMatrix(archive, InputStream, jointPositions);
                for (size_t i = 0; i < std::min<int>((int)state->m->JointBindMatrices.size(), (int)jointPositions.cols()); ++i)
                {
                    state->m->JointBindMatrices[i].translation() = jointPositions.col(i);
                }
            }
            if (Version > 2)
            {
                DeserializeEigenMatrix(archive, InputStream, state->m->ModelTranslation);
            }
            if (Version > 4)
            {
                archive(state->m->VertexDeltaScale);
                archive(state->m->FloorOffsetApplied);
            }

            //We need to update the state to new model version
            if (state->m->ModelVersion != m->ModelVersion)
            {
                FitToTarget(*state, FitToTargetOptions{}, vertices, jointPositions);
                const auto& newMesurements = state->GetNamedConstraintMeasurements();
                for (auto& [k, v] : state->m->TargetMeasurements)
                {
                    v = newMesurements[k];
                }
            }
        }
    }

    if (!success)
    {
        // try loading the data using old stream
        InputStream->seek(startPos);
        terse::BinaryInputArchive<trio::BoundedIOStream> archive{ InputStream };
        std::vector<float> tempValues;
        archive(tempValues);
        state->m->GuiControls = Eigen::Map<const Eigen::VectorX<float>>{ tempValues.data(), static_cast<Eigen::VectorXf::Index>(tempValues.size()) };
        archive(state->m->TargetMeasurements);
    }

    UpdateState(*state);
    return state;
}

void BodyShapeEditor::DumpState(const State& state, trio::BoundedIOStream* OutputStream)
{
    MHCBinaryOutputArchive archive{ OutputStream };

    int32_t Version = 5;
    archive(m->MagicNumber);
    archive(Version);
    archive(m->ModelVersion);
    // archive gui controls
    SerializeEigenMatrix(archive, OutputStream, state.m->GuiControls);

    // archive the target measurements
    archive(state.m->TargetMeasurements.size());
    for (const auto& [k, v] : state.m->TargetMeasurements)
    {
        archive(m->Constraints[k].GetName());
        archive(v);
    }
    // archive the vertex deltas
    SerializeEigenMatrix(archive, OutputStream, state.m->VertexDeltas);
    SerializeEigenMatrix(archive, OutputStream, state.m->JointDeltas);
    // archive the actual body vertices (in order to be able to reconstruct the body in a future release)
    SerializeEigenMatrix(archive, OutputStream, state.m->RigMeshes[0].Vertices());
    // archive the joint positions
    Eigen::Matrix<float, 3, -1> jointPositions(3, (int)state.m->JointBindMatrices.size());
    for (size_t i = 0; i < state.m->JointBindMatrices.size(); ++i)
    {
        jointPositions.col(i) = state.m->JointBindMatrices[i].translation();
    }
    SerializeEigenMatrix(archive, OutputStream, jointPositions);
    SerializeEigenMatrix(archive, OutputStream, state.m->ModelTranslation);
    archive(state.m->VertexDeltaScale);
    archive(state.m->FloorOffsetApplied);
}

template <typename T>
std::pair<T, Eigen::RowVectorX<T>> EvaluateLength(const Eigen::Matrix<T, 3, -1>& vertices,
                                                  const Eigen::SparseMatrix<T, Eigen::RowMajor>& evaluationMatrix,
                                                  const std::vector<BarycentricCoordinates<T>>& lines)
{
    T length = 0;
    Eigen::RowVectorX<T> jacobian = Eigen::RowVectorX<T>::Zero(evaluationMatrix.cols());

    for (int j = 0; j < (int)lines.size() - 1; j++)
    {
        const BarycentricCoordinates<float>& b0 = lines[j];
        const BarycentricCoordinates<float>& b1 = lines[j + 1];
        const Eigen::Vector3<T> segment = b1.template Evaluate<3>(vertices) - b0.template Evaluate<3>(vertices);
        const T segmentLength = segment.norm();
        const T segmentWeight = segmentLength > T(1e-9f) ? T(1) / segmentLength : T(0);
        length += segmentLength;

        for (int d = 0; d < 3; d++)
        {
            T b0w = (T)b0.Weight(d);
            T b1w = (T)b1.Weight(d);
            jacobian += (-segmentWeight * segment[0] * b0w) * evaluationMatrix.row(3 * b0.Index(d) + 0);
            jacobian += (-segmentWeight * segment[1] * b0w) * evaluationMatrix.row(3 * b0.Index(d) + 1);
            jacobian += (-segmentWeight * segment[2] * b0w) * evaluationMatrix.row(3 * b0.Index(d) + 2);
            jacobian += (segmentWeight * segment[0] * b1w) * evaluationMatrix.row(3 * b1.Index(d) + 0);
            jacobian += (segmentWeight * segment[1] * b1w) * evaluationMatrix.row(3 * b1.Index(d) + 1);
            jacobian += (segmentWeight * segment[2] * b1w) * evaluationMatrix.row(3 * b1.Index(d) + 2);
        }
    }

    return { length, jacobian };
}

template <typename T>
std::pair<T, Eigen::RowVectorX<T>> EvaluateDistance(const Eigen::Matrix<T, 3, -1>& vertices,
                                                    const Eigen::SparseMatrix<T, Eigen::RowMajor>& evaluationMatrix,
                                                    int vID1,
                                                    int vID2)
{
    Eigen::RowVectorX<T> jacobian = evaluationMatrix.row(3 * vID2 + 1) - evaluationMatrix.row(3 * vID1 + 1);
    return { vertices(1, vID2) - vertices(1, vID1), jacobian };
}

template <typename T>
std::pair<T, Eigen::RowVectorX<T>> EvaluateSemantic(const Eigen::VectorX<T>& rawControls, const Eigen::VectorX<T>& weights)
{
    Eigen::RowVectorX<T> jacobian = weights.transpose();
    return { rawControls.head(weights.size()).dot(weights), jacobian };
}

std::vector<int> GetUsedVertexIndices(int numVertices, const std::vector<BodyMeasurement>& measurements)
{
    std::vector<int> indices;
    indices.reserve(numVertices);

    std::vector<bool> used(numVertices, false);
    for (size_t i = 0; i < measurements.size(); i++)
    {
        for (const auto& b : measurements[i].GetBarycentricCoordinates())
        {
            used[b.Index(0)] = true;
            used[b.Index(1)] = true;
            used[b.Index(2)] = true;
        }
        for (size_t j = 0; j < measurements[i].GetVertexIDs().size(); ++j)
        {
            used[measurements[i].GetVertexIDs()[j]] = true;
        }
    }
    for (int vID = 0; vID < (int)used.size(); ++vID)
    {
        if (used[vID])
        {
            indices.push_back(vID);
        }
    }

    return indices;
}

void BodyShapeEditor::Solve(State& State, float priorWeight, const int /*iterations*/) const
{
    const int numVertices = m->rigGeometry->GetMesh(0).NumVertices();

    // list of vertices that's being controlled
    const std::vector<int> indices = GetUsedVertexIndices(numVertices, State.m->Constraints);

    Eigen::VectorXf symControls = m->symControls->GuiToSymmetricControls(State.m->GuiControls);
    const auto& symToGuiMat = m->symControls->SymmetricToGuiControlsMatrix();
    const auto& guiToRawMat = m->guiToRawMappingMatrix;
    Eigen::SparseMatrix<float, Eigen::RowMajor> symToRawMat = guiToRawMat * symToGuiMat;
    const Eigen::SparseMatrix<float, Eigen::RowMajor>& symEvalMat = m->symmetricIdentityVertexEvaluationMatrix;
    Eigen::Matrix<float, 3, -1> currVertices = m->rigGeometry->GetMesh(0).Vertices();

    Eigen::MatrixXf AtA(symControls.size(), symControls.size());
    Eigen::VectorXf Atb(symControls.size());

    const int numIterationsSteps = 2;
    std::set<std::string> involvedConstraintNames = {};
    for (const auto& [constraintNames, affectedRegions] : m->solveSteps)
    {
        involvedConstraintNames.insert(constraintNames.begin(), constraintNames.end());
        std::vector<bool> usedSymmetricControls(symControls.size(), false);
        int affected = 0;
        for (const std::string& regionName : affectedRegions)
        {
            bool found = false;
            auto it = m->shapePcaControls.find(regionName);
            if (it != m->shapePcaControls.end())
            {
                found = true;
                for (int rawControl : it->second)
                {
                    int guiControl = m->rawToGuiControls[rawControl];
                    for (Eigen::SparseMatrix<float, Eigen::RowMajor>::InnerIterator itMat(symToGuiMat, guiControl); itMat; ++itMat)
                    {
                        usedSymmetricControls[itMat.col()] = true;
                        affected++;
                    }
                }
            }
            it = m->skeletonPcaControls.find(regionName);
            if (it != m->skeletonPcaControls.end())
            {
                found = true;
                for (int rawControl : it->second)
                {
                    int guiControl = m->rawToGuiControls[rawControl];
                    for (Eigen::SparseMatrix<float, Eigen::RowMajor>::InnerIterator itMat(symToGuiMat, guiControl); itMat; ++itMat)
                    {
                        usedSymmetricControls[itMat.col()] = true;
                        affected++;
                    }
                }
            }
            if (!found)
            {
                for (int guiControl = 0; guiControl < symToGuiMat.outerSize(); ++guiControl)
                {
                    for (Eigen::SparseMatrix<float, Eigen::RowMajor>::InnerIterator itMat(symToGuiMat, guiControl); itMat; ++itMat)
                    {
                        usedSymmetricControls[itMat.col()] = true;
                        affected++;
                    }
                }
            }
        }

        for (int iter = 0; iter < numIterationsSteps; ++iter)
        {
            Eigen::VectorXf guiControls = symToGuiMat * symControls;
            Eigen::VectorXf rawControls = guiToRawMat * guiControls;

            AtA.setZero();
            Atb.setZero();

            // prior cost
            if (priorWeight > 0)
            {
                if (State.m->GuiControlsPrior.size() != guiControls.size())
                {
                    State.m->GuiControlsPrior = Eigen::VectorXf::Zero(guiControls.size());
                }
                Eigen::SparseMatrix<float, Eigen::RowMajor> gwmFullMat = m->gwm * symToGuiMat;
                AtA += priorWeight * (gwmFullMat.transpose() * gwmFullMat);
                Atb += priorWeight * (gwmFullMat.transpose() * m->gwm * (State.m->GuiControlsPrior - symToGuiMat * symControls));
            }

            for (int vID : indices)
            {
                currVertices(0, vID) = m->rigGeometry->GetMesh(0).Vertices()(0, vID) + symEvalMat.row(3 * vID + 0) * symControls;
                currVertices(1, vID) = m->rigGeometry->GetMesh(0).Vertices()(1, vID) + symEvalMat.row(3 * vID + 1) * symControls;
                currVertices(2, vID) = m->rigGeometry->GetMesh(0).Vertices()(2, vID) + symEvalMat.row(3 * vID + 2) * symControls;
            }

            for (const auto& keyValuePair : State.m->TargetMeasurements)
            {
                int ConstraintIndex = keyValuePair.first;
                float ConstraintWeight = keyValuePair.second;
                if (involvedConstraintNames.count(State.m->Constraints[ConstraintIndex].GetName()) > 0)
                {
                    if (State.m->Constraints[ConstraintIndex].GetType() == BodyMeasurement::Axis)
                    {
                        // axis costs
                        auto [dist, jacobian] = EvaluateDistance<float>(currVertices,
                                                                        symEvalMat,
                                                                        State.m->Constraints[ConstraintIndex].GetVertexIDs()[0],
                                                                        State.m->Constraints[ConstraintIndex].GetVertexIDs()[1]);
                        AtA.template triangularView<Eigen::Lower>() += jacobian.transpose() * jacobian;
                        Atb += jacobian.transpose() * (ConstraintWeight - dist);
                    }
                    else if (State.m->Constraints[ConstraintIndex].GetType() == BodyMeasurement::Semantic)
                    {
                        // semantic costs
                        auto [value, jacobian] = EvaluateSemantic<float>(rawControls, State.m->Constraints[ConstraintIndex].GetWeights());
                        // jacobian of raw controls, convert to sym
                        Eigen::RowVectorXf symJacobian = jacobian * guiToRawMat * symToGuiMat;
                        float diff = ConstraintWeight - value;
                        AtA.template triangularView<Eigen::Lower>() += State.m->SemanticWeight * (symJacobian.transpose() * symJacobian);
                        Atb += State.m->SemanticWeight * (symJacobian.transpose() * diff);
                    }
                    else
                    {
                        // length costs
                        auto [value, jacobian] = EvaluateLength(currVertices, symEvalMat, State.m->Constraints[ConstraintIndex].GetBarycentricCoordinates());
                        float diff = ConstraintWeight - value;
                        AtA.template triangularView<Eigen::Lower>() += (jacobian.transpose() * jacobian);
                        Atb += (jacobian.transpose() * diff);
                    }
                }
            }

            for (int i = 0; i < (int)usedSymmetricControls.size(); ++i)
            {
                if (!usedSymmetricControls[i])
                {
                    AtA.col(i).setZero();
                    AtA.row(i).setZero();
                    Atb(i) = 0;
                }
            }

            // regularization
            for (int i = 0; i < AtA.cols(); ++i)
            {
                AtA(i, i) += 1e-2f;
            }

            // solve
            const Eigen::VectorXf dx = AtA.template selfadjointView<Eigen::Lower>().llt().solve(Atb);
            symControls += dx;
        }
    }

    State.m->GuiControls = symToGuiMat * symControls;
    State.m->RawControls = m->rigLogic->EvaluateRawControls(DiffData<float>(State.m->GuiControls)).Value();
    const auto& rawControls = State.m->RawControls;
    const float rawMean = rawControls.mean();
    const float rawStdDev = std::sqrt((rawControls.array() - rawMean).square().sum() / rawControls.size());
    for (int i = 0; i < State.m->RawControls.size(); ++i)
    {
    	const auto& rawControlName = m->rigLogic->RawControlNames()[i];
        if (rawControlName.starts_with("local_groin") || rawControlName.starts_with("local_pelvis_0") )
        {
            auto& groinControl = State.m->RawControls[i];
            if (groinControl < rawMean - 2 * rawStdDev)
            {
                groinControl = rawMean - 2 * rawStdDev;
            }
            else if (groinControl > rawMean + 2 * rawStdDev)
            {
                groinControl = rawMean + 2 * rawStdDev;
            }
        }
    }
    UpdateGuiFromRawControls(State);
    UpdateState(State);
}

void BodyShapeEditor::UpdateHelperJoints(const Eigen::Matrix<float, 3, -1>& vertices,
                                         std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& bindMatrices) const
{
    for (const auto& [jointName, jointId_vID] : m->helperJointMap)
    {
        bindMatrices[jointId_vID.first].translation() = vertices.col(jointId_vID.second);
    }
}

void BodyShapeEditor::StateToDna(const State& State, dna::Writer* InOutDnaWriter, bool combinedBodyAndFace) const
{
    std::vector<SparseMatrix<float>> vertexInfluenceWeights;
    GetVertexInfluenceWeights(State, vertexInfluenceWeights);
    if (vertexInfluenceWeights.size() > 0)
    {
        // update the skinning weights in the DNA
        for (int lod = 0; lod < NumLODs(); ++lod)
        {
            InOutDnaWriter->clearSkinWeights((uint16_t)lod);
            int numVertices = static_cast<int>(vertexInfluenceWeights[size_t(lod)].rows());
            if (combinedBodyAndFace)
            {
                // write skin weights for the combined face and body
                for (int vID = numVertices - 1; vID >= 0; --vID)
                {
                    std::vector<float> weights;
                    std::vector<uint16_t> indices;
                    for (typename SparseMatrix<float>::InnerIterator it(vertexInfluenceWeights[size_t(lod)], vID); it; ++it)
                    {
                        if (it.value() != float(0))
                        {
                            weights.push_back(it.value());
                            indices.emplace_back((uint16_t)it.col());
                        }
                    }

                    InOutDnaWriter->setSkinWeightsValues(lod, vID, weights.data(), (uint16_t)weights.size());
                    InOutDnaWriter->setSkinWeightsJointIndices(lod, vID, indices.data(), (uint16_t)indices.size());
                }
            }
            else
            {
                // map weights from combined to headless body
                for (int vID = numVertices - 1; vID >= 0; --vID)
                {
                    if (m->combinedToBodyMapping[lod].find(vID) != m->combinedToBodyMapping[lod].end())
                    {
                        int bodyVID = m->combinedToBodyMapping[lod][vID];
                        std::vector<float> weights;
                        std::vector<uint16_t> indices;

                        for (typename SparseMatrix<float>::InnerIterator it(vertexInfluenceWeights[size_t(lod)], vID); it; ++it)
                        {
                            if (it.value() != float(0))
                            {
                                weights.push_back(it.value());
                                indices.emplace_back((uint16_t)it.col());
                            }
                        }

                        InOutDnaWriter->setSkinWeightsValues(lod, bodyVID, weights.data(), (uint16_t)weights.size());
                        InOutDnaWriter->setSkinWeightsJointIndices(lod, bodyVID, indices.data(), (uint16_t)indices.size());
                    }
                }
            }
        }
    }


    for (int lod = 0; lod < NumLODs(); ++lod)
    {
        const uint16_t meshIndex = static_cast<uint16_t>(lod);
        if (combinedBodyAndFace)
        {
            const Eigen::Matrix3Xf& bodyVertices = State.GetMesh(lod).Vertices();
            const Eigen::Matrix3Xf& bodyNormals = State.GetMesh(lod).VertexNormals();
            InOutDnaWriter->setVertexPositions(meshIndex, (const dna::Position*)bodyVertices.data(), uint32_t(bodyVertices.cols()));
            InOutDnaWriter->setVertexNormals(meshIndex, (const dna::Position*)bodyNormals.data(), uint32_t(bodyNormals.cols()));
        }
        else
        {
            const std::vector<int>& bodyToCombinedMapping = GetBodyToCombinedMapping(static_cast<int>(lod));
            const Eigen::Matrix3Xf bodyVertices = State.GetMesh(lod).Vertices()(Eigen::all, bodyToCombinedMapping);
            const Eigen::Matrix3Xf bodyNormals = State.GetMesh(lod).VertexNormals()(Eigen::all, bodyToCombinedMapping);
            InOutDnaWriter->setVertexPositions(meshIndex, (const dna::Position*)bodyVertices.data(), uint32_t(bodyVertices.cols()));
            InOutDnaWriter->setVertexNormals(meshIndex, (const dna::Position*)bodyNormals.data(), uint32_t(bodyNormals.cols()));
        }
    }

    // Set neutral joints
    constexpr float rad2deg = float(180.0 / CARBON_PI);

    uint16_t numJoints = m->rigGeometry->NumJoints();

    Eigen::Matrix<float, 3, -1> jointTranslations(3, numJoints);
    Eigen::Matrix<float, 3, -1> jointRotations(3, numJoints);

    const auto& jointHierarchy = m->rigGeometry->GetJointParentIndices();

    const auto getJointParent = [&jointHierarchy](std::uint16_t jointIndex) {
            return jointHierarchy[jointIndex];
        };

    const std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& jointMatrices = State.GetJointBindMatrices();

    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Eigen::Transform<float, 3, Eigen::Affine> localTransform;
        const int parentJointIndex = getJointParent(jointIndex);
        if (parentJointIndex >= 0)
        {
            auto parentTransform = jointMatrices[parentJointIndex];
            localTransform = parentTransform.inverse() * jointMatrices[jointIndex];
        }
        else
        {
            localTransform = jointMatrices[jointIndex];
        }

        jointTranslations.col(jointIndex) = localTransform.translation();
        jointRotations.col(jointIndex) = rad2deg * RotationMatrixToEulerXYZ<float>(localTransform.linear());
    }

    InOutDnaWriter->setNeutralJointTranslations((dna::Vector3*)jointTranslations.data(), numJoints);
    InOutDnaWriter->setNeutralJointRotations((dna::Vector3*)jointRotations.data(), numJoints);

    if (m->rbfPCA.mods.size() > 0)
    {
        auto rbfMatrix = m->rbfPCA.calculateResult(State.m->GuiControls(m->globalIndices));
        for (int jg = 0; jg < m->jointGroupInputIndices.size(); ++jg)
        {
            const auto& inputIndices = m->jointGroupInputIndices[jg];
            const auto& outputIndices = m->jointGroupOutputIndices[jg];
            std::vector<float> values;
            values.reserve(inputIndices.size() * outputIndices.size());
            for (const auto oi : outputIndices)
            {
                for (const auto ii : inputIndices)
                {
                    values.push_back(rbfMatrix.coeff(oi, ii));
                }
            }
        }
    }


    // TODO: add RBF
}

int BodyShapeEditor::NumJoints() const
{
	return m->rigGeometry->NumJoints();
}

void BodyShapeEditor::GetNeutralJointTransform(const State& State, std::uint16_t JointIndex, Eigen::Vector3f& OutJointTranslation, Eigen::Vector3f& OutJointRotation) const
{
	constexpr float rad2deg = float(180.0 / CARBON_PI);

    if (JointIndex >= m->rigGeometry->NumJoints())
	{
		CARBON_CRITICAL("JointIndex out of range");
	}

    const auto& jointHierarchy = m->rigGeometry->GetJointParentIndices();

    const std::vector<Eigen::Transform<float, 3, Eigen::Affine>>& jointMatrices = State.GetJointBindMatrices();

	Eigen::Transform<float, 3, Eigen::Affine> localTransform;
	const int parentJointIndex = jointHierarchy[JointIndex];
	if (parentJointIndex >= 0)
	{
		auto parentTransform = jointMatrices[parentJointIndex];
		localTransform = parentTransform.inverse() * jointMatrices[JointIndex];
	}
	else
	{
		localTransform = jointMatrices[JointIndex];
	}

	OutJointTranslation = localTransform.translation();
	OutJointRotation = rad2deg * RotationMatrixToEulerXYZ<float>(localTransform.linear());
}

Eigen::Matrix<float, 3, -1> ScatterCol(const Eigen::Matrix<float, 3, -1>& input, const std::vector<int>& ids, int numCols)
{
    Eigen::Matrix<float, 3, -1> target = Eigen::Matrix<float, 3, -1>::Zero(3, numCols);
    for (int i = 0; i < (int)ids.size(); ++i)
    {
        target.col(ids[i]) = input.col(i);
    }
    return target;
}

void BodyShapeEditor::FitToTarget(State& State,
                                  const FitToTargetOptions& options,
                                  const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InVertices,
                                  const Eigen::Ref<const Eigen::Matrix<float, 3, -1>>& InJoints)
{
    if ((InVertices.cols() == 0) && (InJoints.cols() == 0)) { return; }

    const int meshId = 0;
    std::vector<int> mapping, fittingMapping;
    Eigen::Matrix<float, 3, -1> InVerticesFiltered;

    if ((int)InVertices.cols() == (int)(m->rigGeometry->GetMesh(0).NumVertices()))
    {
        mapping.resize(InVertices.cols());
        std::iota(mapping.begin(), mapping.end(), 0);
        fittingMapping = NonZeroMaskVerticesIntersection(mapping, m->combinedFittingIndices);
        InVerticesFiltered = InVertices(Eigen::all, fittingMapping);
    }
    else if ((int)InVertices.cols() == (int)GetBodyToCombinedMapping(meshId).size())
    {
        mapping = GetBodyToCombinedMapping(meshId);
        Eigen::Matrix<float, 3, -1> InVerticesScattered = ScatterCol(InVertices, mapping, m->rigGeometry->GetMesh(0).NumVertices());
        fittingMapping = NonZeroMaskVerticesIntersection(mapping, m->combinedFittingIndices);;

        InVerticesFiltered = InVerticesScattered(Eigen::all, fittingMapping);
    }
    else if (((int)InJoints.cols() > 0) && ((int)InVertices.cols() == 0))
    {
        mapping.resize(0);
    }
    else
    {
        CARBON_CRITICAL("Failed to fit to target. Invalid number of input vertices or joints.");
    }

    Affine<float, 3, 3> transform;
    float scale = 1.0f;

    Eigen::VectorXf x = State.m->GuiControls;

    SolveForTemplateMesh(x, scale, transform, State.m->ModelTranslation, InVerticesFiltered, InJoints, options, fittingMapping);
    State.m->RawControls = m->guiToRawMappingMatrix * x;

    Eigen::VectorXf resultVerticesVector = m->rigGeometry->GetMesh(0).Vertices().reshaped() + m->identityVertexEvaluationMatrix * State.m->RawControls;
    Eigen::Matrix3Xf resultVertices = Eigen::Map<Eigen::Matrix3Xf>(resultVerticesVector.data(), 3, m->rigGeometry->GetMesh(0).NumVertices());
    Eigen::Matrix3Xf transformedTargets = transform.Transform(scale * InVertices);
    Eigen::Matrix3Xf translatedResult = resultVertices.colwise() + State.m->ModelTranslation;

    if ((int)InVertices.cols() != (int)(m->rigGeometry->GetMesh(0).NumVertices()))
    {
        Eigen::Matrix<float, 3, -1> transformedScatteredTargets = ScatterCol(transformedTargets, mapping, m->rigGeometry->GetMesh(0).NumVertices());
        DeformationModelVertex<float> defModelVertex;

        auto config = defModelVertex.GetConfiguration();
        config["vertexOffsetRegularization"] = 0.0f;
        config["vertexLaplacian"] = 5.0f;
        defModelVertex.SetConfiguration(config);
        defModelVertex.SetMeshTopology(m->rigGeometry->GetMesh(0));
        defModelVertex.SetRestVertices(translatedResult);
        defModelVertex.MakeVerticesConstant(mapping);

        Eigen::Matrix3Xf offsets = Eigen::Matrix3Xf::Zero(3, m->rigGeometry->GetMesh(0).NumVertices());
        offsets(Eigen::all, mapping) = transformedScatteredTargets(Eigen::all, mapping) - translatedResult(Eigen::all, mapping);
        defModelVertex.SetVertexOffsets(offsets);

        std::function<DiffData<float>(Context<float>*)> evaluationFunction = [&](Context<float>* context)
            {
                Cost<float> cost;
                cost.Add(defModelVertex.EvaluateModelConstraints(context));
                return cost.CostToDiffData();
            };

        GaussNewtonSolver<float> solver;
        const float startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        if (solver.Solve(evaluationFunction, 3))
        {
            const float finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
            LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
        }
        else
        {
            LOG_ERROR("could not solve optimization problem");
        }
        State.m->VertexDeltas = defModelVertex.DeformedVertices() - translatedResult;
    }
    else
    {
        State.m->VertexDeltas = transformedTargets - translatedResult;
    }

    if (InJoints.cols() > 0)
    {
        Eigen::Matrix<float, 3, -1> jointPositions(3, (int)m->rigGeometry->GetBindMatrices().size());
        for (size_t i = 0; i < m->rigGeometry->GetBindMatrices().size(); ++i)
        {
            jointPositions.col(i) = m->rigGeometry->GetBindMatrices()[i].translation();
        }

        Eigen::VectorXf resultJointsVector = jointPositions.reshaped() + m->identityJointEvaluationMatrix * State.m->RawControls;
        Eigen::Matrix3Xf resultJoints = Eigen::Map<Eigen::Matrix3Xf>(resultJointsVector.data(), 3, m->rigGeometry->NumJoints());
        Eigen::Matrix3Xf translatedJoints = resultJoints.colwise() +  State.m->ModelTranslation;
        Eigen::Matrix3Xf targetJoints = transform.Transform(scale * InJoints);

        State.m->JointDeltas = targetJoints - translatedJoints;
    }
    else
    {
        State.m->JointDeltas = Eigen::Matrix<float, 3, -1>::Zero(3, (int)m->rigGeometry->GetBindMatrices().size());
    }

    UpdateGuiFromRawControls(State);
    State.m->GuiControlsPrior = State.m->GuiControls;
    EvaluateState(State, options.snapToFloor);
}

void BodyShapeEditor::SetCustomGeometryToState(State& State, std::shared_ptr<const BodyGeometry<float>> Geometry, bool Fit)
{
    if (!Geometry) { return; }

    if (Fit)
    {
        FitToTargetOptions fitToTargetOptions;

        const int numVertices = Geometry->GetMesh(0).NumVertices();
        std::vector<int> mapping(numVertices);
        std::iota(mapping.begin(), mapping.end(), 0);
        Eigen::Matrix3Xf InJoints = Eigen::Matrix3Xf::Zero(3, Geometry->NumJoints());
        for (int i = 0; i < Geometry->NumJoints(); ++i)
        {
            InJoints.col(i) = Geometry->GetBindMatrices()[i].translation();
        }

        FitToTarget(State, fitToTargetOptions, Geometry->GetMesh(0).Vertices(), InJoints);
    }
    else
    {
        State.m->RigMeshes.resize(Geometry->GetNumLODs());
        for (int lod = 0; lod < Geometry->GetNumLODs(); ++lod)
        {
            State.m->RigMeshes[lod].SetTriangles(m->meshTriangles[lod]);
            State.m->RigMeshes[lod].SetVertices(Geometry->GetMesh(lod).Vertices());
            State.m->RigMeshes[lod].CalculateVertexNormals(false, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/ true, m->threadPool.get());
        }
        State.m->JointBindMatrices = Geometry->GetBindMatrices();
    }
}

const std::vector<std::string>& BodyShapeEditor::GetRegionNames() const
{
    return m->regionNames;
}

bool BodyShapeEditor::Blend(State& state, int RegionIndex, const std::vector<std::pair<float, const State*>>& States, BodyAttribute Type)
{
    const int numRegions = (int)m->regionNames.size();
    Eigen::VectorXf rawControls = state.m->RawControls;
    Eigen::Matrix3Xf VertexDeltas = state.m->VertexDeltas;
	Eigen::Matrix3Xf JointDeltas = state.m->JointDeltas;
    if (VertexDeltas.size() == 0)
    {
        VertexDeltas = Eigen::Matrix3Xf::Zero(3, m->rigGeometry->GetMesh(0).NumVertices());
    }
	if (JointDeltas.size() == 0)
	{
		JointDeltas = Eigen::Matrix3Xf::Zero(3, m->rigGeometry->NumJoints());
	}

    auto BlendRegion = [&](int idx) {
            if ((idx < 0) || (idx >= (int)m->regionNames.size())) { return; }
            for (const auto& [alpha, otherState] : States)
            {
                if ((Type == BodyAttribute::Skeleton) || (Type == BodyAttribute::Both))
                {
                    auto it = m->skeletonPcaControls.find("joint_" + m->regionNames[idx]);
                    if (it != m->skeletonPcaControls.end())
                    {
                        rawControls(it->second) += alpha * (otherState->m->RawControls(it->second) - rawControls(it->second));
                    	
                    }
                	if ((otherState->m->JointDeltas.size() > 0) || (state.m->JointDeltas.size() > 0))
                	{
                		for (int ji : m->regionToJoints[m->regionNames[idx]])
                		{
                			if (ji < otherState->m->JointDeltas.cols() && ji< state.m->JointDeltas.cols())
                			{
                				JointDeltas.col(ji) += alpha * (otherState->m->VertexDeltaScale * otherState->m->JointDeltas.col(ji)) - state.m->JointDeltas.col(ji);
                			}
                			else if (ji < otherState->m->JointDeltas.cols())
                			{
                				JointDeltas.col(ji) += alpha * (otherState->m->VertexDeltaScale * otherState->m->JointDeltas.col(ji));
                			}
                			else
                			{
                				JointDeltas.col(ji) -= alpha * state.m->JointDeltas.col(ji);
                			}
                		}
                	}
                }
                if ((Type == BodyAttribute::Shape) || (Type == BodyAttribute::Both))
                {
                    auto it = m->shapePcaControls.find(m->regionNames[idx]);
                    if (it != m->shapePcaControls.end())
                    {
                        rawControls(it->second) += alpha * (otherState->m->RawControls(it->second) - rawControls(it->second));
                    }

                    if ((otherState->m->VertexDeltas.size() > 0) || (state.m->VertexDeltas.size() > 0))
                    {
                        auto it2 = m->partWeights.find(m->regionNames[idx]);
                        if (it2 != m->partWeights.end())
                        {
                            for (const auto& [vID, weight] : it2->second.NonzeroVerticesAndWeights())
                            {
                            	if ((vID < otherState->m->VertexDeltas.cols()) && (vID < state.m->VertexDeltas.cols()))
                            	{
                            		VertexDeltas.col(vID) += weight * alpha * ((otherState->m->VertexDeltaScale * otherState->m->VertexDeltas.col(vID)) - state.m->VertexDeltas.col(vID));
                            	}
                            	else if (vID < otherState->m->VertexDeltas.cols())
                            	{
                            		VertexDeltas.col(vID) += (weight * alpha * otherState->m->VertexDeltaScale) * (otherState->m->VertexDeltas.col(vID));
                            	}
                                else
                                {
                                    VertexDeltas.col(vID) -= weight * alpha * (state.m->VertexDeltas.col(vID));
                                }
                            }
                        }
                    }
                }
            }
        };

    if ((RegionIndex < 0) || (RegionIndex >= numRegions))
    {
        for (int idx = 0; idx < numRegions; ++idx)
        {
            BlendRegion(idx);
        }
    }
    else
    {
        BlendRegion(RegionIndex);
        if (state.m->UseSymmetry)
        {
            auto it = m->symmetricPartMapping.find(m->regionNames[RegionIndex]);
            if (it != m->symmetricPartMapping.end())
            {
                const int SymmetricRegionIndex = GetItemIndex(m->regionNames, it->second);
                if ((SymmetricRegionIndex != RegionIndex) && (SymmetricRegionIndex >= 0))
                {
                    BlendRegion(SymmetricRegionIndex);
                }
            }
        }
    }

    state.m->RawControls = rawControls;
    if (VertexDeltas.squaredNorm() > 0)
    {
        state.m->VertexDeltas = VertexDeltas;
    }
    else
    {
        state.m->VertexDeltas.resize(3, 0);
    }

    UpdateGuiFromRawControls(state);
    EvaluateState(state, /*applyFloorOffset=*/ true);
    state.m->GuiControlsPrior = state.m->GuiControls;
    state.m->TargetMeasurements.clear();
    // std::set<std::string> alwaysON{ "Height", "Body Type", "Fat", "Muscularity" };
    // for (size_t i = 0; i < state.m->Constraints.size(); ++i)
    // {
    //     if (alwaysON.contains(state.m->Constraints[i].GetName()))
    //     {
    //         state.m->TargetMeasurements.push_back({ (int)i, state.m->ConstraintMeasurements[i] });
    //     }
    // }
    return true;
}

bool BodyShapeEditor::GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> combinedBodyAndFaceVertices, Eigen::VectorXf& measurements,
                                      std::vector<std::string>& measurementNames) const
{
    if ((int)combinedBodyAndFaceVertices.cols() != m->rigGeometry->GetMesh(0).NumVertices())
    {
        CARBON_CRITICAL("vertices have incorrect size for combined body and face: {}, but expected {}",
                        combinedBodyAndFaceVertices.cols(),
                        m->rigGeometry->GetMesh(0).NumVertices());
    }

    std::vector<BodyMeasurement> Constraints;
    measurementNames.clear();
    for (int i = 0; i < (int)m->Constraints.size(); ++i)
    {
        if (m->Constraints[i].GetType() != BodyMeasurement::type_t::Semantic)
        {
            Constraints.push_back(m->Constraints[i]);
            measurementNames.push_back(m->Constraints[i].GetName());
        }
    }
    Eigen::Matrix<float, 3, -1> vertexNormals;
    m->triTopology->CalculateVertexNormals(combinedBodyAndFaceVertices,
                                           vertexNormals,
                                           VertexNormalComputationType::AreaWeighted,
                                           /*stableNormalize*/ true,
                                           m->threadPool.get());
    BodyMeasurement::UpdateBodyMeasurementPoints(Constraints, combinedBodyAndFaceVertices, vertexNormals, *m->heTopology, m->threadPool.get());
    measurements = BodyMeasurement::GetBodyMeasurements(Constraints, combinedBodyAndFaceVertices, Eigen::VectorXf());

    return true;
}

bool BodyShapeEditor::GetMeasurements(Eigen::Ref<const Eigen::Matrix<float, 3, -1>> faceVertices,
                                      Eigen::Ref<const Eigen::Matrix<float, 3, -1>> bodyVertices,
                                      Eigen::VectorXf& measurements,
                                      std::vector<std::string>& measurementNames) const
{
    if (m->bodyToCombinedMapping.empty())
    {
        CARBON_CRITICAL("body to combined mapping is missing");
    }
    if ((int)bodyVertices.cols() != (int)m->bodyToCombinedMapping.front().size())
    {
        CARBON_CRITICAL("incorrect body vertices size: {}, but expected {}", bodyVertices.cols(), m->bodyToCombinedMapping.front().size());
    }
    Eigen::Matrix<float, 3, -1> combinedBodyAndFaceVertices = m->rigGeometry->GetMesh(0).Vertices();
    if (faceVertices.cols() > combinedBodyAndFaceVertices.cols())
    {
        CARBON_CRITICAL("invalid number of face vertices: {}", faceVertices.cols());
    }
    combinedBodyAndFaceVertices.leftCols(faceVertices.cols()) = faceVertices;
    combinedBodyAndFaceVertices(Eigen::all, m->bodyToCombinedMapping.front()) = bodyVertices;
    return GetMeasurements(combinedBodyAndFaceVertices, measurements, measurementNames);
}

SnapConfig<float> BodyShapeEditor::Private::CalcNeckSeamSkinningWeightsSnapConfig(int lod)const
{
    SnapConfig<float> curSnapConfig;
    curSnapConfig.sourceVertexIndices = neckSeamIndices[0];

    const Eigen::Matrix<float, 3, -1>& curLodMeshVertices = combinedBodyArchetypeRigGeometry->GetMesh(lod).Vertices();
    const Eigen::Matrix<float, 3, -1>& lod0MeshVertices = combinedBodyArchetypeRigGeometry->GetMesh(0).Vertices();

    // find closest vertex in lod N for each sourceVertexIndex, and ensure the distance is close to zero
    for (size_t sInd = 0; sInd < curSnapConfig.sourceVertexIndices.size(); ++sInd)
    {
        Eigen::Matrix<float, 3, 1> curSourceVert = lod0MeshVertices.col(curSnapConfig.sourceVertexIndices[sInd]);
        float closestDist2 = std::numeric_limits<float>::max();
        int closestVInd = 0;
        for (int tInd = 0; tInd < curLodMeshVertices.cols(); ++tInd)
        {
            float curDist2 = (curSourceVert - curLodMeshVertices.col(tInd)).squaredNorm();
            if (curDist2 < closestDist2)
            {
                closestDist2 = curDist2;
                closestVInd = tInd;
            }
        }

        curSnapConfig.targetVertexIndices.emplace_back(closestVInd);
    }

    return curSnapConfig;
}

void BodyShapeEditor::GetVertexInfluenceWeights(const State& state, std::vector<SparseMatrix<float>>& vertexInfluenceWeights) const
{
    if (m->skinWeightsPCA.mean.size() > 0u)
    {
        vertexInfluenceWeights.resize(size_t(NumLODs()));
        vertexInfluenceWeights.resize(NumLODs());
        vertexInfluenceWeights[0] = m->skinWeightsPCA.calculateResult(state.m->GuiControls(m->globalIndices));
        skinningweightutils::SortPruneAndRenormalizeSkinningWeights(vertexInfluenceWeights[0], GetMaxSkinWeights()[0]);

        // now propagate the skinning weights to higher lods
        std::map<std::string, std::vector<BarycentricCoordinates<float>>> lod0MeshClosestPointBarycentricCoordinates;
        m->combinedLodGenerationData->GetDriverMeshClosestPointBarycentricCoordinates(lod0MeshClosestPointBarycentricCoordinates);

        for (int lod = 1; lod < NumLODs(); ++lod)
        {
            std::vector<BarycentricCoordinates<float>> curLodBarycentricCoordinates = lod0MeshClosestPointBarycentricCoordinates.at(
                m->combinedLodGenerationData->HigherLodMeshNames()[lod - 1]);
            skinningweightutils::PropagateSkinningWeightsToHigherLOD<float>(curLodBarycentricCoordinates,
                                                                            m->combinedBodyArchetypeRigGeometry->GetMesh(0).Vertices(),
                                                                            vertexInfluenceWeights[0],
                                                                            m->jointSkinningWeightLodPropagationMap[lod - 1],
                                                                            m->skinningWeightSnapConfigs[size_t(lod - 1)],
                                                                            *m->combinedBodyArchetypeRigGeometry,
                                                                            GetMaxSkinWeights()[size_t(lod)],
                                                                            vertexInfluenceWeights[size_t(lod)]);
        }
    }
}

int BodyShapeEditor::GetJointIndex(const std::string& JointName) const
{
    return m->combinedBodyArchetypeRigGeometry->GetJointIndex(JointName);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
