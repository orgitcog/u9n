// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/FaceFitting.h>
#include <conformer/GeometryConstraints.h>

#include <carbon/io/Utils.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/Cost.h>
#include <nls/DiffDataMatrix.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/functions/VertexConstraintsFunction.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/Procrustes.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/geometry/QRigidMotion.h>
#include <nls/math/SparseMatrixMultiply.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nrr/landmarks/LipClosure.h>
#include <nrr/CollisionConstraints.h>
#include <nrr/EyeballConstraints.h>
#include <nrr/PatchBlendModel.h>
#include <nrr/deformation_models/DeformationModelRigid.h>
#include <nrr/deformation_models/DeformationModelVertex.h>
#include <nrr/LinearVertexModel.h>

#include <nls/serialization/ObjFileFormat.h>

CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/IterativeLinearSolvers>
CARBON_RENABLE_WARNINGS

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// ! @returns True if the linear component of the transformation contains scale
template <class T>
bool HasScaling(const Affine<T, 3, 3>& affine, const T eps = T(1e-4)) { return (fabs(affine.Linear().norm() - std::sqrt(T(3))) > eps); }

template <class T>
struct FaceFitting<T>::Private
{
    // ! The source mesh (the vertices are the latest deformed state, or set by the user)
    Mesh<T> sourceMesh;

    // ! Structure to keep lip closure data points
    std::vector<LipClosure3D<T>> lipClosure;

    // ! Structure to calculate landmark constraints
    std::vector<std::shared_ptr<LandmarkConstraints2D<T>>> landmarkConstraints2d;

    // ! Structure to calculate 3D landmark constraints
    std::vector<std::shared_ptr<LandmarkConstraints3D<T>>> landmarkConstraints3d;

    // ! Structure to calculate lip closure constraints
    std::vector<std::shared_ptr<LipClosureConstraints<T>>> lipClosureConstraints;

    // ! Structure to keep calculated correspondences
    std::vector<std::shared_ptr<const fitting_tools::CorrespondenceData<T>>> fixedCorrespondenceData;

    // ! Structure to keep mesh landmarks
    MeshLandmarks<T> meshLandmarks;

    // ! An identity model for part-based nonrigid registration
    PatchBlendModel<T> patchBlendModel;
    typename PatchBlendModel<T>::OptimizationState patchBlendModelState;

    // ! the base of the mesh
    Eigen::Matrix<T, 3, -1> sourceBase;

    // ! the per-vertex offsets of the mesh
    Eigen::Matrix<T, 3, -1> sourceOffsets;

    std::vector<std::shared_ptr<GeometryConstraints<T>>> icpConstraints;
    FlowConstraints<T> modelFlowConstraints;
    FlowConstraints<T> uvFlowConstraints;
    EyeballConstraints<T> leftEyeballConstraints;
    EyeballConstraints<T> rightEyeballConstraints;
    std::vector<CollisionConstraints<T>> selfCollisionConstraints;
    std::vector<CollisionConstraints<T>> staticCollisionConstraints;
    std::vector<Eigen::Matrix<T, 3, -1>> staticCollisionVertices;

    // ! current fitting constraints
    std::shared_ptr<const FaceFittingConstraintsDebugInfo<T>> constraintsDebugInfo;

    VertexWeights<T> upperInnerLip;
    VertexWeights<T> lowerInnerLip;

    bool isIdentityFit = false;

    std::vector<int> fixedVertices;

    Eigen::Matrix<T, 3, -1> CurrentBase()
    {
        if ((patchBlendModelState.NumParameters() > 0) && isIdentityFit)
        {
            return patchBlendModel.DeformedVertices(patchBlendModelState);
        }
        else
        {
            return sourceBase;
        }
    }

    // ! update the deformed vertices based on the current model state
    void UpdateDeformed()
    {
        if (sourceOffsets.cols() > 0)
        {
            sourceDeformed = CurrentBase() + sourceOffsets;
        }
        constraintsDebugInfo.reset();
    }

    const Eigen::Matrix<T, 3, -1>& CurrentDeformed() const
    {
        return sourceDeformed;
    }

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> globalThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(
        /*createIfNotAvailable=*/true);

    private:
        // ! the current final deformed source (internal representation after evaluation)
        Eigen::Matrix<T, 3, -1> sourceDeformed;
};


template <class T>
FaceFitting<T>::FaceFitting()
    : m(new Private)
{}

template <class T> FaceFitting<T>::~FaceFitting() = default;
template <class T> FaceFitting<T>::FaceFitting(FaceFitting&& other) = default;
template <class T>
FaceFitting<T>& FaceFitting<T>::operator=(FaceFitting&& other) = default;

template <class T>
void FaceFitting<T>::InitIcpConstraints(int numOfObservations)
{
    m->icpConstraints.clear();

    if (int(m->icpConstraints.size()) != numOfObservations)
    {
        m->icpConstraints.resize(numOfObservations);
    }

    for (int i = 0; i < numOfObservations; i++)
    {
        m->icpConstraints[i] = std::make_shared<GeometryConstraints<T>>();
    }
}

template <class T>
void FaceFitting<T>::Init2DLandmarksConstraints(int numOfObservations)
{
    m->landmarkConstraints2d.clear();
    m->lipClosure.clear();
    m->lipClosureConstraints.clear();

    if (int(m->landmarkConstraints2d.size()) != numOfObservations)
    {
        m->landmarkConstraints2d.resize(numOfObservations);
        m->lipClosure.resize(numOfObservations);
        m->lipClosureConstraints.resize(numOfObservations);
    }

    for (int i = 0; i < numOfObservations; i++)
    {
        m->landmarkConstraints2d[i] = std::make_shared<LandmarkConstraints2D<T>>();
        m->lipClosureConstraints[i] = std::make_shared<LipClosureConstraints<T>>();
    }
}

template <class T>
void FaceFitting<T>::Init3DLandmarksConstraints(int numOfObservations)
{
    m->landmarkConstraints3d.clear();

    if (int(m->landmarkConstraints3d.size()) != numOfObservations)
    {
        m->landmarkConstraints3d.resize(numOfObservations);
    }

    for (int i = 0; i < numOfObservations; i++)
    {
        m->landmarkConstraints3d[i] = std::make_shared<LandmarkConstraints3D<T>>();
    }
}

template <class T>
void FaceFitting<T>::SetFixedVertices(const std::vector<int>& fixedVertices) { m->fixedVertices = fixedVertices; }

template <class T>
void FaceFitting<T>::LoadModelBinary(const std::string& patchModelBinaryFilepath)
{
    m->patchBlendModel.LoadModelBinary(patchModelBinaryFilepath);
    m->patchBlendModelState = m->patchBlendModel.CreateOptimizationState();
    m->sourceBase = Eigen::Matrix<T, 3, -1>::Zero(3, m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols());
    m->sourceOffsets = Eigen::Matrix<T, 3, -1>::Zero(3, m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols());

    m->isIdentityFit = true;
    m->UpdateDeformed();
}

template <class T>
void FaceFitting<T>::LoadModel(const std::string& identityModelFileOrData)
{
    const bool isValidFile = std::filesystem::exists(identityModelFileOrData);
    std::string jsonString;
    if (isValidFile)
    {
        jsonString = ReadFile(identityModelFileOrData);
    }
    else
    {
        jsonString = identityModelFileOrData;
    }

    m->patchBlendModel.LoadModel(jsonString);
    m->patchBlendModelState = m->patchBlendModel.CreateOptimizationState();
    m->sourceBase = Eigen::Matrix<T, 3, -1>::Zero(3, m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols());
    m->sourceOffsets = Eigen::Matrix<T, 3, -1>::Zero(3, m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols());

    m->isIdentityFit = true;
    m->UpdateDeformed();
}

template <class T>
void FaceFitting<T>::SetTopology(const Mesh<T>& mesh)
{
    CARBON_ASSERT(mesh.NumVertices() == m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols(),
                  "input  mesh not compatible with identity model.");
    m->sourceMesh = mesh;
    m->sourceMesh.Triangulate();
}

template <class T>
void FaceFitting<T>::SetSourceMesh(const Mesh<T>& mesh)
{
    CARBON_ASSERT(mesh.NumVertices() == m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols(),
                  "input source mesh not compatible with identity model.");
    m->sourceMesh = mesh;
    m->sourceMesh.Triangulate();
    m->isIdentityFit = false;

    m->sourceBase = m->sourceMesh.Vertices();
    m->sourceOffsets = Eigen::Matrix<T, 3, -1>::Zero(3, m->sourceMesh.NumVertices());
    m->UpdateDeformed();

    m->patchBlendModelState.ResetParameters(m->patchBlendModel);
}

template <class T>
void FaceFitting<T>::SetSourceAndDeformedMesh(const Mesh<T>& baseMesh, const Mesh<T>& deformedMesh)
{
    CARBON_ASSERT(baseMesh.NumVertices() == m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols(),
                  "input base mesh not compatible with identity model.");
    CARBON_ASSERT(deformedMesh.NumVertices() == m->patchBlendModel.DeformedVertices(m->patchBlendModelState).cols(),
                  "input base mesh not compatible with identity model.");
    m->sourceMesh = baseMesh;
    m->sourceMesh.Triangulate();
    m->isIdentityFit = false;

    m->sourceBase = m->sourceMesh.Vertices();
    m->sourceOffsets = deformedMesh.Vertices() - baseMesh.Vertices();
    m->UpdateDeformed();

    m->patchBlendModelState.ResetParameters(m->patchBlendModel);
}

template <class T>
void FaceFitting<T>::SetModelFlowConstraints(const std::map<std::string,
                                                            std::shared_ptr<const FlowConstraintsData<T>>>& flowConstraintsData)
{
    m_fineFittingConfig["useModelOpticalFlow"].Set(true);
    m->modelFlowConstraints.SetFlowData(flowConstraintsData);
    m->modelFlowConstraints.SetFlowWeight(m_fineFittingConfig["modelFlowWeight"].template Value<T>());
}

template <class T>
bool FaceFitting<T>::HasModelFlowConstraints() const
{
    return m->modelFlowConstraints.FlowData().size() > 0;
}

template <class T>
void FaceFitting<T>::SetUVFlowConstraints(const std::map<std::string,
                                                         std::shared_ptr<const FlowConstraintsData<T>>>& flowConstraintsData)
{
    m_fineFittingConfig["useUVOpticalFlow"].Set(true);
    m->uvFlowConstraints.SetFlowData(flowConstraintsData);
    m->uvFlowConstraints.SetFlowWeight(m_fineFittingConfig["uvFlowWeight"].template Value<T>());
}

template <class T>
bool FaceFitting<T>::HasUVFlowConstraints() const
{
    return m->uvFlowConstraints.FlowData().size() > 0;
}

template <class T>
void FaceFitting<T>::SetGlobalUserDefinedLandmarkAndCurveWeights(const std::map<std::string,
                                                                                T>& userDefinedLandmarkAndCurveWeights)
{
    for (int i = 0; i < int(m->landmarkConstraints2d.size()); ++i)
    {
        m->landmarkConstraints2d[i]->SetUserDefinedLandmarkAndCurveWeights(userDefinedLandmarkAndCurveWeights);
    }
    for (int i = 0; i < int(m->landmarkConstraints3d.size()); ++i)
    {
        m->landmarkConstraints3d[i]->SetUserDefinedLandmarkAndCurveWeights(userDefinedLandmarkAndCurveWeights);
    }
}

template <class T>
void FaceFitting<T>::SetPerInstanceUserDefinedLandmarkAndCurveWeights(const std::vector<std::map<std::string,
                                                                                                 T>>& userDefinedLandmarkAndCurveWeights)
{
    CARBON_ASSERT(m->landmarkConstraints2d.size() == userDefinedLandmarkAndCurveWeights.size(),
                  "number of input weight instances does not align with number of landmark constraints");
    CARBON_ASSERT(m->landmarkConstraints3d.size() == userDefinedLandmarkAndCurveWeights.size(),
                  "number of input weight instances does not align with number of landmark constraints");

    for (int i = 0; i < int(m->landmarkConstraints2d.size()); ++i)
    {
        m->landmarkConstraints2d[i]->SetUserDefinedLandmarkAndCurveWeights(userDefinedLandmarkAndCurveWeights[i]);
    }
    for (int i = 0; i < int(m->landmarkConstraints3d.size()); ++i)
    {
        m->landmarkConstraints3d[i]->SetUserDefinedLandmarkAndCurveWeights(userDefinedLandmarkAndCurveWeights[i]);
    }
}

template <class T>
void FaceFitting<T>::SetEyeConstraintVertexWeights(const VertexWeights<T>& vertexWeightsLeftEye, const VertexWeights<T>& vertexWeightsRightEye)
{
    m->leftEyeballConstraints.SetInterfaceVertices(vertexWeightsLeftEye);
    m->rightEyeballConstraints.SetInterfaceVertices(vertexWeightsRightEye);
}

template <class T>
void FaceFitting<T>::SetInnerLipInterfaceVertices(const VertexWeights<T>& maskUpperLip, const VertexWeights<T>& maskLowerLip)
{
    m->upperInnerLip = maskUpperLip;
    m->lowerInnerLip = maskLowerLip;
}

template <class T>
void FaceFitting<T>::SetSelfCollisionVertices(const std::vector<std::pair<std::vector<int>,
                                                                          std::vector<int>>>& selfCollisionMasks)
{
    m->selfCollisionConstraints.clear();
    for (const auto& [mask1, mask2] : selfCollisionMasks)
    {
        m->selfCollisionConstraints.emplace_back(CollisionConstraints<T>());
        m->selfCollisionConstraints.back().SetSourceTopology(m->sourceMesh, mask1);
        m->selfCollisionConstraints.back().SetTargetTopology(m->sourceMesh, mask2);
    }
}

template <class T>
void FaceFitting<T>::SetStaticCollisionMasks(const std::vector<std::tuple<std::vector<int>, Mesh<T>,
                                                                          std::vector<int>>>& staticCollisions)
{
    m->staticCollisionConstraints.clear();
    m->staticCollisionVertices.clear();
    for (const auto&[srcMask, targetMesh, targetMask] : staticCollisions)
    {
        m->staticCollisionConstraints.emplace_back(CollisionConstraints<T>());
        m->staticCollisionConstraints.back().SetSourceTopology(m->sourceMesh, srcMask);
        m->staticCollisionConstraints.back().SetTargetTopology(targetMesh, targetMask);
    }
}

template <class T>
void FaceFitting<T>::SetStaticCollisionVertices(const std::vector<Eigen::Matrix<T, 3, -1>>& staticCollisionVertices)
{
    if (staticCollisionVertices.size() != m->staticCollisionConstraints.size())
    {
        CARBON_CRITICAL("size of static collision vertices does not match nmber of static collision constraints");
    }
    m->staticCollisionVertices = staticCollisionVertices;
}

template <class T>
void FaceFitting<T>::SetMeshLandmarks(const MeshLandmarks<T>& meshLandmarks) { m->meshLandmarks = meshLandmarks; }

template <class T>
void FaceFitting<T>::SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& targetMeshes, const std::vector<Eigen::VectorX<T>>& targetWeights)
{
    InitIcpConstraints(int(targetMeshes.size()));

    for (int i = 0; i < int(targetMeshes.size()); ++i)
    {
        m->icpConstraints[i]->SetTargetMesh(targetMeshes[i]);
        if (targetWeights.size() > 0)
        {
            m->icpConstraints[i]->SetTargetWeights(targetWeights[i]);
        }
    }
    m->constraintsDebugInfo.reset();
}

template <class T>
void FaceFitting<T>::SetTargetDepths(const std::vector<std::vector<std::shared_ptr<const DepthmapData<T>>>>& targetDepths)
{
    InitIcpConstraints(int(targetDepths.size()));

    for (size_t i = 0; i < targetDepths.size(); ++i)
    {
        for (size_t j = 0; j < targetDepths[i].size(); ++j)
        {
            m->icpConstraints[i]->AddTargetDepthAndNormals(targetDepths[i][j]);
        }
    }
    m->constraintsDebugInfo.reset();
}

template <class T>
void FaceFitting<T>::SetTarget2DLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>,
                                                                                  Camera<T>>>>& landmarks)
{
    Init2DLandmarksConstraints(int(landmarks.size()));

    for (int i = 0; i < int(landmarks.size()); ++i)
    {
        m->landmarkConstraints2d[i]->SetMeshLandmarks(m->meshLandmarks);
        m->landmarkConstraints2d[i]->SetTargetLandmarks(landmarks[i]);

        for (const auto& [landmarkInstance, camera] : landmarks[i])
        {
            m->lipClosure[i].Add(landmarkInstance, camera);
        }
        if (m->lipClosure[i].Valid())
        {
            m->lipClosureConstraints[i]->SetTopology(m->sourceMesh,
                                                     m->upperInnerLip.NonzeroVertices(),
                                                     m->meshLandmarks.InnerUpperLipContourLines(),
                                                     m->lowerInnerLip.NonzeroVertices(),
                                                     m->meshLandmarks.InnerLowerLipContourLines());
            m->lipClosureConstraints[i]->SetLipClosure(m->lipClosure[i]);
        }
    }

    m->constraintsDebugInfo.reset();
}

template <class T>
void FaceFitting<T>::SetTarget3DLandmarks(const std::vector<LandmarkInstance<T, 3>>& landmarks)
{
    Init3DLandmarksConstraints(int(landmarks.size()));

    for (int i = 0; i < int(landmarks.size()); ++i)
    {
        m->landmarkConstraints3d[i]->SetMeshLandmarks(m->meshLandmarks);
        m->landmarkConstraints3d[i]->SetTargetLandmarks(landmarks[i]);
    }
    m->constraintsDebugInfo.reset();
}

template <class T>
const std::map<std::string, Eigen::Vector3<T>> FaceFitting<T>::CurrentMeshLandmarks() const
{
    const auto currentVertices = m->CurrentDeformed();
    std::map<std::string, Eigen::Vector3<T>> meshPositions;

    for (const auto& [landmarkName, bc] : m->meshLandmarks.LandmarksBarycentricCoordinates())
    {
        meshPositions.emplace(landmarkName, bc.template Evaluate<3>(currentVertices));
    }

    return meshPositions;
}

template <class T>
const std::map<std::string, std::vector<Eigen::Vector3<T>>> FaceFitting<T>::CurrentMeshCurves() const
{
    const auto currentVertices = m->CurrentDeformed();
    std::map<std::string, std::vector<Eigen::Vector3<T>>> meshPositions;
    for (const auto& [curveName, bc] : m->meshLandmarks.MeshCurvesBarycentricCoordinates())
    {
        std::vector<Eigen::Vector3<T>> currentPoints;
        for (size_t i = 0; i < bc.size(); ++i)
        {
            currentPoints.push_back(bc[i].template Evaluate<3>(currentVertices));
        }
        meshPositions.emplace(curveName, currentPoints);
    }
    return meshPositions;
}

template <class T>
void FaceFitting<T>::SetCurrentDeformedVertices(const Eigen::Matrix<T, 3, -1>& deformedVertices)
{
    CARBON_ASSERT(deformedVertices.cols() == m->CurrentBase().cols(),
                  "input deformed vertices must be compatible with identity model");
    m->sourceOffsets = deformedVertices - m->CurrentBase();
    m->UpdateDeformed();
}

template <class T>
void FaceFitting<T>::SetCurrentModelParameters(const Eigen::VectorXf& modelParameters)
{
    CARBON_ASSERT(m->patchBlendModelState.NumParameters() == modelParameters.size(),
                  "input model parameters must be compatible with the identity model");
    m->isIdentityFit = true;
    m->patchBlendModelState.SetModelParameters(modelParameters);
    m->UpdateDeformed();
}

template <class T>
const Eigen::VectorX<T>& FaceFitting<T>::CurrentModelParameters() const
{
    return m->patchBlendModelState.GetModelParameters();
}

template <class T>
const Eigen::Matrix<T, 3, -1>& FaceFitting<T>::CurrentDeformedVertices() const
{
    return m->CurrentDeformed();
}

template <class T>
void FaceFitting<T>::LoadInitialCorrespondencesVertices(const Eigen::Matrix<T, 3, -1>& sourceVertices)
{
    if (sourceVertices.cols() > 0)
    {
        if (sourceVertices.cols() != m->sourceMesh.NumVertices())
        {
            CARBON_CRITICAL("incompatible number of vertices with source mesh");
        }

        m->sourceMesh.SetVertices(sourceVertices);
    }
}

template <class T>
void FaceFitting<T>::SetEyeballMesh(const Mesh<T>& mesh)
{
    m->rightEyeballConstraints.SetEyeballMesh(mesh);
    m->leftEyeballConstraints.SetEyeballMesh(mesh);
}

template <class T>
void FaceFitting<T>::SetFixedCorrespondenceData(const std::vector<std::shared_ptr<const fitting_tools::CorrespondenceData<T>>>& correspondenceData)
{
    m->fixedCorrespondenceData = correspondenceData;
}

template <class T>
void FaceFitting<T>::ClearFixedCorrespondeceData()
{
    m->fixedCorrespondenceData.clear();
}

template <class T>
bool FaceFitting<T>::HasFixedCorrespondenceData() const
{
    return !m->fixedCorrespondenceData.empty();
}

template <class T>
void FaceFitting<T>::SetupEyeballConstraint(const Eigen::Matrix<T, 3, -1>& leftEyeballVertices, const Eigen::Matrix<T, 3, -1>& rightEyeballVertices)
{
    m->rightEyeballConstraints.SetRestPose(rightEyeballVertices, CurrentDeformedVertices());
    m->leftEyeballConstraints.SetRestPose(leftEyeballVertices, CurrentDeformedVertices());

    const T eyeballWeight = m_fineFittingConfig["eyeballWeight"].template Value<T>();
    Configuration config = m->leftEyeballConstraints.GetConfiguration();
    config["eyeball"].Set(eyeballWeight);
    // config["useClosestEyeballPosition"].Set(true);
    m->leftEyeballConstraints.SetConfiguration(config);
    m->rightEyeballConstraints.SetConfiguration(config);
    m_fineFittingConfig["useEyeballConstraint"].Set(true);
}

template <class T>
Affine<T, 3, 3> FaceFitting<T>::RegisterRigid(const Affine<T, 3, 3>& source2target, const VertexWeights<T>& searchWeights, int numIterations, int scanFrame)
{
    CARBON_PRECONDITION(!source2target.HasScaling(), "the source2target transformation cannot contain any scaling component");

    std::vector<Eigen::VectorX<T>> knownCorrespondencesWeights;
    if (!m->fixedCorrespondenceData.empty())
    {
        for (int i = 0; i < (int)m->fixedCorrespondenceData.size(); ++i)
        {
            knownCorrespondencesWeights.push_back(Eigen::VectorX<T>::Ones(m->fixedCorrespondenceData[i]->srcIDs.size()));
        }
    }

    DeformationModelRigid<T> deformationModelRigid;

    deformationModelRigid.SetRigidTransformation(source2target);
    deformationModelRigid.SetVertices(m->CurrentDeformed());

    UpdateIcpConfiguration(m_rigidFittingConfig);
    const T landmarksWeights2D = m_rigidFittingConfig["landmarksWeight"].template Value<T>();
    const T landmarksWeights3D = m_rigidFittingConfig["3DlandmarksWeight"].template Value<T>();

    m->icpConstraints[scanFrame]->SetSourceWeights(searchWeights);
    m->icpConstraints[scanFrame]->ClearPreviousCorrespondences();

    const bool use3dLandmarks = !m->landmarkConstraints3d.empty();
    const bool use2dLandmarks = !m->landmarkConstraints2d.empty();

    if (!use3dLandmarks && !use2dLandmarks)
    {
        LOG_WARNING("No landmark constraints set for rigid face fitting.");
    }

    Mesh<T> currentMesh = m->sourceMesh;

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context)
        {
            Cost<T> cost;

            const DiffDataMatrix<T, 3, -1> transformedVertices = deformationModelRigid.EvaluateVertices(context);

            if (m->fixedCorrespondenceData.empty())
            {
                if (context || !m->icpConstraints[scanFrame]->HasCorrespondences())
                {
                    currentMesh.SetVertices(transformedVertices.Matrix());
                    currentMesh.CalculateVertexNormals();
                }
                m->icpConstraints[scanFrame]->SetupCorrespondences(currentMesh, /*useTarget2Source=*/false);
                cost.Add(m->icpConstraints[scanFrame]->EvaluateICP(transformedVertices), T(1));
            }
            else
            {
                if (m->fixedCorrespondenceData[scanFrame])
                {
                    DiffDataMatrix<T, 3, -1> srcCorrespondences = GatherFunction<T>::template GatherColumns<3, -1, -1>(transformedVertices, m->fixedCorrespondenceData[scanFrame]->srcIDs);
                    Eigen::Matrix<T, 3, -1> tgtCorrespondences = m->fixedCorrespondenceData[scanFrame]->EvaluateTargetBCs(m->icpConstraints[scanFrame]->TargetMesh()->Vertices());
                    cost.Add(PointPointConstraintFunction<T, 3>::Evaluate(srcCorrespondences,
                                                                          tgtCorrespondences,
                                                                          knownCorrespondencesWeights[scanFrame],
                                                                          T(1)), T(1));
                }
                else
                {
                    LOG_ERROR("No set correspondences for frame {}", scanFrame);
                }
            }

            if (use2dLandmarks && (landmarksWeights2D > 0))
            {
                cost.Add(m->landmarkConstraints2d[scanFrame]->EvaluateLandmarks(transformedVertices), landmarksWeights2D);
            }
            if (use3dLandmarks && (landmarksWeights3D > 0))
            {
                cost.Add(m->landmarkConstraints3d[scanFrame]->EvaluateLandmarks(transformedVertices), landmarksWeights3D);
            }

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, numIterations))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
        m->constraintsDebugInfo.reset();
    }
    else
    {
        LOG_WARNING("could not solve optimization problem");
    }
    return deformationModelRigid.RigidTransformation();
}

template <class T>
std::vector<Affine<T, 3, 3>> FaceFitting<T>::RegisterRigid(const std::vector<Affine<T, 3, 3>>& source2target,
                                                           const VertexWeights<T>& searchWeights,
                                                           int numIterations)
{
    CARBON_ASSERT(m->icpConstraints.size() == source2target.size(), "number of targets does not match number of icp constraints");

    std::vector<Affine<T, 3, 3>> outSource2Target(source2target.size());
    for (int frame = 0; frame < int(m->icpConstraints.size()); ++frame)
    {
        outSource2Target[frame] = RegisterRigid(source2target[frame], searchWeights, numIterations, frame);
    }

    return outSource2Target;
}

template <class T>
void FaceFitting<T>::ResetNonrigid()
{
    m->patchBlendModelState.ResetParameters(m->patchBlendModel);
    ResetFine();
}

template <class T>
void ParallelAtALowerAdd(Eigen::Matrix<T, -1, -1>& AtA, const SparseMatrix<T>& A, int offset, TaskThreadPool* threadPool)
{
    if (threadPool && (A.rows() > 1000))
    {
        const int numSplits = int(threadPool->NumThreads());
        std::vector<Eigen::Matrix<T, -1, -1>> AtA_vec(numSplits);
        auto sparseAtA = [&](int start, int end)
            {
                for (int split = start; split < end; ++split)
                {
                    AtA_vec[split] = Eigen::Matrix<T, -1, -1>::Zero(A.cols(), A.cols());
                    const Eigen::Index rstart = A.rows() / numSplits * split;
                    const Eigen::Index rend = (split < numSplits - 1) ? (rstart + A.rows() / numSplits) : A.rows();
                    for (Eigen::Index r = rstart; r < rend; ++r)
                    {
                        for (typename SparseMatrix<T>::InnerIterator it(A, r); it; ++it)
                        {
                            for (typename SparseMatrix<T>::InnerIterator it2 = it; it2; ++it2)
                            {
                                AtA_vec[split](it2.col(), it.col()) += it.value() * it2.value();
                            }
                        }
                    }
                }
            };
        threadPool->AddTaskRangeAndWait(numSplits, sparseAtA);
        for (int k = 0; k < numSplits; ++k)
        {
            AtA.block(offset, offset, A.cols(), A.cols()).noalias() += AtA_vec[k];
        }
    }
    else
    {
        for (Eigen::Index r = 0; r < A.rows(); ++r)
        {
            for (typename SparseMatrix<T>::InnerIterator it(A, r); it; ++it)
            {
                for (typename SparseMatrix<T>::InnerIterator it2 = it; it2; ++it2)
                {
                    AtA(it2.col() + offset, it.col() + offset) += it.value() * it2.value();
                }
            }
        }
    }
}

template <class T, int ResidualSize, int NumConstraintVertices>
void ApplyVertexConstraints(const VertexConstraints<T, ResidualSize, NumConstraintVertices>& vertexContraints,
                            const rt::LinearVertexModel<T>& vertices,
                            const SparseMatrix<T>& verticesModelJacobian,
                            Eigen::Matrix<T, -1, -1>& AtA,
                            Eigen::VectorX<T>& Atb,
                            int rigidIndex,
                            TaskThreadPool* threadPool)
{
    if (vertexContraints.NumberOfConstraints() > 0)
    {
        // evaluate jacobian relative to model
        const int numModelParams = int(verticesModelJacobian.cols());
        SparseMatrix<T> jacobian;
        SparseMatrixMultiply(vertexContraints.SparseJacobian(vertices.NumVertices()),
                             false,
                             verticesModelJacobian,
                             false,
                             jacobian);
        ParallelAtALowerAdd(AtA, jacobian, /*offset=*/0, threadPool);
        Atb.segment(0, numModelParams).noalias() += -jacobian.transpose() * vertexContraints.Residual();

        // evaluate jacobian relative to rigid transform
        Eigen::Matrix<T, -1, -1, Eigen::RowMajor> vertexContraintsJacobian;
        const auto rmJacobian = vertexContraints.EvaluateJacobian(vertices.Modes(rt::LinearVertexModel<T>::EvaluationMode::RIGID),
                                                                  vertexContraintsJacobian);
        AtA.block(numModelParams + 6 * rigidIndex, numModelParams + 6 * rigidIndex, 6,
                  6).template triangularView<Eigen::Lower>() += rmJacobian.transpose() * rmJacobian;
        Atb.segment(numModelParams + 6 * rigidIndex, 6).noalias() += -rmJacobian.transpose() * vertexContraints.Residual();

        // dependency for rigid and model
        // AtA.block(numModelParams + 6 * rigidIndex, 0, 6, numModelParams) += rmJacobian.transpose() * jacobian;
        AtA.block(numModelParams + 6 * rigidIndex, 0, 6, numModelParams) += (jacobian.transpose() * rmJacobian).transpose();
    }
}

template <class T>
std::vector<Affine<T, 3, 3>> FaceFitting<T>::RegisterNonRigid(const std::vector<Affine<T, 3, 3>>& source2target,
                                                              const VertexWeights<T>& searchWeights,
                                                              int numIterations)
{
    CARBON_PRECONDITION(m->patchBlendModel.NumPatches() > 0,
                        "no identity model - first load model before nonrigid registration");
    CARBON_ASSERT(m->icpConstraints.size() == source2target.size(), "number of targets does not match number of icp constraints");

    // Timer timer;

    // store current model
    const Eigen::Matrix<T, 3, -1> previousVertices = m->CurrentDeformed();

    std::vector<QRigidMotion<T>> qrms(source2target.size());
    for (int i = 0; i < int(qrms.size()); ++i)
    {
        // TODO: soruce2target is around model that is not centered at the origin which is bad for numerics
        qrms[i] = source2target[i].Matrix();
    }

    m->patchBlendModelState.SetOptimizeScale(m_modelFittingConfig["optimizeScale"].template Value<bool>());
    const T modelRegularization = m_modelFittingConfig["modelRegularization"].template Value<T>();
    const T patchSmoothness = m_modelFittingConfig["patchSmoothness"].template Value<T>();

    Update2DLandmarkConfiguration(m_modelFittingConfig);
    Update3DLandmarkConfiguration(m_modelFittingConfig);
    UpdateLipClosureConfiguration(m_modelFittingConfig);
    UpdateIcpConfiguration(m_modelFittingConfig);
    UpdateIcpWeights(searchWeights);

    const T lipClosureWeight = m_modelFittingConfig["lipClosureWeight"].template Value<T>();

    const bool use3dLandmarks = !m->landmarkConstraints3d.empty();
    const bool use2dLandmarks = !m->landmarkConstraints2d.empty();
    const bool useLipClosure = !m->lipClosureConstraints.empty() && lipClosureWeight > T(0);

    // const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();

    const int numModelParams = m->patchBlendModelState.NumParameters();
    const int numTotalParameters = numModelParams + int(qrms.size()) * 6; // rigid per scan/depth
    Eigen::Matrix<T, -1, -1> AtA(numTotalParameters, numTotalParameters);
    Eigen::VectorX<T> Atb(numTotalParameters);
    Eigen::Matrix<T, 3, -1> faceNormals;

    m->sourceMesh.CalculateVertexNormals(m->sourceMesh.Vertices(),
                                         faceNormals,
                                         VertexNormalComputationType::AreaWeighted,
                                         /*stableNormalize=*/false);
    if (useLipClosure)
    {
        for (int i = 0; i < int(m->icpConstraints.size()); ++i)
        {
            const Eigen::Transform<T, 3, Eigen::Affine> toFace(qrms[i].ToEigenTransform());
            m->lipClosureConstraints[i]->CalculateLipClosureData(m->CurrentDeformed(),
                                                                 faceNormals,
                                                                 Eigen::Transform<T, 3, Eigen::Affine>::Identity(),
                                                                 /*useLandmarks=*/true,
                                                                 toFace.inverse());
        }
    }

    struct OptData
    {
        rt::LinearVertexModel<T> verticesModel;
        struct OptVertexConstraints
        {
            VertexConstraints<T, 3, 1> point2PointVertexConstraints;
            VertexConstraints<T, 1, 1> point2SurfaceVertexConstraints;
            VertexConstraints<T, 2, 3> landmarksVertexConstraints;
            VertexConstraints<T, 1, 3> curvesVertexConstraints;
            VertexConstraints<T, 1, 2> innerLipVertexConstraints;
            VertexConstraints<T, 3, 3> landmarksVertexConstraints3d;
            VertexConstraints<T, 2, 3> curvesVertexConstraints3d;
            VertexConstraints<T, 3, 1> lipClosureVertexConstraints;
        };
        std::vector<OptVertexConstraints> constraints;

        OptData(size_t numFrames)
            : constraints(numFrames)
        {}

        void Clear()
        {
            for (size_t i = 0; i < constraints.size(); ++i)
            {
                constraints[i].point2PointVertexConstraints.Clear();
                constraints[i].point2SurfaceVertexConstraints.Clear();
                constraints[i].landmarksVertexConstraints.Clear();
                constraints[i].curvesVertexConstraints.Clear();
                constraints[i].innerLipVertexConstraints.Clear();
                constraints[i].landmarksVertexConstraints3d.Clear();
                constraints[i].curvesVertexConstraints3d.Clear();
                constraints[i].lipClosureVertexConstraints.Clear();
            }
        }
    } optData(qrms.size());

    optData.verticesModel.Create(previousVertices, Eigen::Matrix<T, -1, -1, Eigen::RowMajor>(previousVertices.size(), 0));

    // auto calculateEnergy = [&]() {
    // T energy = 0;
    // const auto [stabilizedVertices, modelConstraints] = m->patchBlendModel.EvaluateVerticesAndConstraints(nullptr);
    // Eigen::Matrix<T, 3, -1> vertices = stabilizedVertices.Matrix();
    // const Eigen::Matrix<T, 3, -1>& prevVertices = optData.verticesModel.Base();
    // energy += modelConstraints.CostToDiffData().Value().squaredNorm();
    // for (size_t i = 0; i < optData.constraints.size(); ++i) {
    // energy += optData.constraints[i].point2PointVertexConstraints.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].point2SurfaceVertexConstraints.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].landmarksVertexConstraints.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].curvesVertexConstraints.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].innerLipVertexConstraints.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].landmarksVertexConstraints3d.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // energy += optData.constraints[i].curvesVertexConstraints3d.EvaluateResidual(vertices, prevVertices).squaredNorm();
    // }
    // return energy;
    // };

    for (int iter = 0; iter < numIterations; ++iter)
    {
        AtA.setZero();
        Atb.setZero();
        optData.Clear();
        Context<T> context;
        const auto [stabilizedVertices, modelConstraints] = m->patchBlendModel.EvaluateVerticesAndConstraints(&context, m->patchBlendModelState, modelRegularization, patchSmoothness);
        const SparseMatrix<T>& verticesModelJacobian = *(stabilizedVertices.Jacobian().AsSparseMatrix());
        optData.verticesModel.Create(stabilizedVertices.Matrix(),
                                     Eigen::Matrix<T, -1, -1, Eigen::RowMajor>(stabilizedVertices.Size(), 0));

        m->sourceMesh.CalculateVertexNormals(optData.verticesModel.Base(),
                                             faceNormals,
                                             VertexNormalComputationType::AreaWeighted,
                                             /*stableNormalize=*/false);

        {
            // model constraints only apply to the model parameters
            const auto diffData = modelConstraints.CostToDiffData();
            const auto& J = *(diffData.Jacobian().AsSparseMatrix());
            ParallelAtALowerAdd(AtA, J, /*offset=*/0, m->globalThreadPool.get());
            Atb.segment(0, numModelParams) += -J.transpose() * diffData.Value();
        }

        for (int i = 0; i < int(m->icpConstraints.size()); ++i)
        {
            m->icpConstraints[i]->SetupConstraints(qrms[i].ToEigenTransform(),
                                                   optData.verticesModel.Base(),
                                                   faceNormals,
                                                   optData.constraints[i].point2SurfaceVertexConstraints,
                                                   optData.constraints[i].point2PointVertexConstraints);
            ApplyVertexConstraints(optData.constraints[i].point2PointVertexConstraints,
                                   optData.verticesModel,
                                   verticesModelJacobian,
                                   AtA,
                                   Atb,
                                   i,
                                   m->globalThreadPool.get());
            ApplyVertexConstraints(optData.constraints[i].point2SurfaceVertexConstraints,
                                   optData.verticesModel,
                                   verticesModelJacobian,
                                   AtA,
                                   Atb,
                                   i,
                                   m->globalThreadPool.get());

            if (use2dLandmarks)
            {
                m->landmarkConstraints2d[i]->SetupLandmarkConstraints(qrms[i].ToEigenTransform(),
                                                                      optData.verticesModel.Base(),
                                                                      &m->meshLandmarks,
                                                                      LandmarkConstraintsBase<T>::MeshType::Face,
                                                                      optData.constraints[i].landmarksVertexConstraints);
                m->landmarkConstraints2d[i]->SetupCurveConstraints(qrms[i].ToEigenTransform(),
                                                                   optData.verticesModel.Base(),
                                                                   &m->meshLandmarks,
                                                                   LandmarkConstraintsBase<T>::MeshType::Face,
                                                                   optData.constraints[i].curvesVertexConstraints);
                m->landmarkConstraints2d[i]->SetupInnerLipConstraints(qrms[i].ToEigenTransform(),
                                                                      optData.verticesModel.Base(),
                                                                      faceNormals,
                                                                      &m->meshLandmarks,
                                                                      optData.constraints[i].innerLipVertexConstraints);
                ApplyVertexConstraints(optData.constraints[i].landmarksVertexConstraints,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
                ApplyVertexConstraints(optData.constraints[i].curvesVertexConstraints,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
                ApplyVertexConstraints(optData.constraints[i].innerLipVertexConstraints,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
            }

            if (use3dLandmarks)
            {
                m->landmarkConstraints3d[i]->SetupLandmarkConstraints(qrms[i].ToEigenTransform(),
                                                                      optData.verticesModel.Base(),
                                                                      &m->meshLandmarks,
                                                                      LandmarkConstraintsBase<T>::MeshType::Face,
                                                                      optData.constraints[i].landmarksVertexConstraints3d);
                m->landmarkConstraints3d[i]->SetupCurveConstraints(qrms[i].ToEigenTransform(),
                                                                   optData.verticesModel.Base(),
                                                                   &m->meshLandmarks,
                                                                   LandmarkConstraintsBase<T>::MeshType::Face,
                                                                   optData.constraints[i].curvesVertexConstraints3d);
                ApplyVertexConstraints(optData.constraints[i].landmarksVertexConstraints3d,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
                ApplyVertexConstraints(optData.constraints[i].curvesVertexConstraints3d,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
            }

            if (useLipClosure)
            {
                m->lipClosureConstraints[i]->EvaluateLipClosure(optData.verticesModel.Base(),
                                                                optData.constraints[i].lipClosureVertexConstraints);
                ApplyVertexConstraints(optData.constraints[i].lipClosureVertexConstraints,
                                       optData.verticesModel,
                                       verticesModelJacobian,
                                       AtA,
                                       Atb,
                                       i,
                                       m->globalThreadPool.get());
            }
        }

        const Eigen::VectorX<T> dx =
            (AtA +
             Eigen::Matrix<T, -1, -1>::Identity(AtA.rows(),
                                                AtA.cols()) * T(0.01)).template selfadjointView<Eigen::Lower>().llt().solve(Atb);
        bool isFinite = true;
        for (Eigen::Index i = 0; i < dx.size(); ++i)
        {
            isFinite &= std::isfinite(dx[i]);
        }
        if (!isFinite)
        {
            LOG_WARNING("solve results in invalid values - abort");
            break;
        }
        // LOG_INFO("solve iter 11: {}", timer2.Current()); timer2.Restart();
        // update parameters
        context.Update(dx.segment(0, numModelParams));
        m->patchBlendModelState.BakeRotationLinearization();

        for (size_t i = 0; i < qrms.size(); ++i)
        {
            const int offset = numModelParams + 6 * int(i);
            qrms[i].t += qrms[i].q._transformVector(dx.segment(offset + 3, 3));
            qrms[i].q = (qrms[i].q * Eigen::Quaternion<T>(1.0f, dx[offset + 0], dx[offset + 1], dx[offset + 2])).normalized();
        }

        // LOG_INFO("solve iter 12: {}", timer2.Current()); timer2.Restart();
    }

    // rigidly align the new model with the previous model (as the region-based deformation model is anchored arbitrarily using
    // the first region)
    {
        const Eigen::Matrix<T, 3, -1> newVertices = m->patchBlendModel.DeformedVertices(m->patchBlendModelState);
        const Affine<T, 3, 3> new2prev = Procrustes<T, 3>::AlignRigid(newVertices, previousVertices);
        m->patchBlendModelState.TransformPatches(new2prev);
        for (int i = 0; i < int(qrms.size()); ++i)
        {
            qrms[i] = QRigidMotion<T>(qrms[i].ToEigenTransform().matrix() * new2prev.Inverse().Matrix());
        }
    }

    m->sourceOffsets.setZero();
    m->isIdentityFit = true;
    m->UpdateDeformed();

    std::vector<Affine<T, 3, 3>> updatedTransforms(qrms.size());
    for (int i = 0; i < int(qrms.size()); ++i)
    {
        updatedTransforms[i] = Affine<T, 3, 3>(qrms[i].ToEigenTransform().matrix());
    }

    return updatedTransforms;
}

template <class T>
void FaceFitting<T>::ResetFine()
{
    m->sourceOffsets.setZero();
    m->UpdateDeformed();
}

template <class T>
std::vector<Affine<T, 3, 3>> FaceFitting<T>::RegisterFine(const std::vector<Affine<T, 3, 3>>& source2target,
                                                          const VertexWeights<T>& searchWeights,
                                                          int numIterations)
{
    CARBON_ASSERT(m->icpConstraints.size() == source2target.size(), "number of targets does not match number of icp constraints");
    std::vector<Eigen::VectorX<T>> knownCorrespondencesWeights;
    if (!m->fixedCorrespondenceData.empty())
    {
        for (int i = 0; i < (int)m->fixedCorrespondenceData.size(); ++i)
        {
            knownCorrespondencesWeights.push_back(Eigen::VectorX<T>::Ones(m->fixedCorrespondenceData[i]->srcIDs.size()));
        }
    }

    std::vector<AffineVariable<QuaternionVariable<T>>> face2scanTransformVariables(source2target.size());
    const bool optimizePose = m_fineFittingConfig["optimizePose"].template Value<bool>();
    for (int i = 0; i < int(face2scanTransformVariables.size()); ++i)
    {
        face2scanTransformVariables[i].SetAffine(source2target[i]);
        face2scanTransformVariables[i].MakeConstant(!optimizePose, !optimizePose);
    }

    DeformationModelVertex<T> deformationModelVertex;
    deformationModelVertex.SetMeshTopology(m->sourceMesh);
    deformationModelVertex.SetRestVertices(m->CurrentBase());
    deformationModelVertex.SetVertexOffsets(m->sourceOffsets);
    deformationModelVertex.SetRigidTransformation(Affine<T, 3, 3>());
    if (m_fineFittingConfig["fixVertices"].template Value<bool>())
    {
        deformationModelVertex.MakeVerticesConstant(m->fixedVertices);
    }

    // TODO: setting the configuration parameters this way is not nice, instead we should combine all the configuration parameters
    // into a hierarchical structure
    Configuration config = deformationModelVertex.GetConfiguration();
    config["optimizePose"].Set(false);
    config["vertexOffsetRegularization"] = m_fineFittingConfig["vertexOffsetRegularization"];
    config["projectiveStrain"] = m_fineFittingConfig["projectiveStrain"];
    config["greenStrain"] = m_fineFittingConfig["greenStrain"];
    config["quadraticBending"] = m_fineFittingConfig["quadraticBending"];
    config["dihedralBending"] = m_fineFittingConfig["dihedralBending"];
    config["vertexLaplacian"] = m_fineFittingConfig["vertexLaplacian"];
    deformationModelVertex.SetConfiguration(config);

    Update2DLandmarkConfiguration(m_fineFittingConfig);
    Update3DLandmarkConfiguration(m_fineFittingConfig);
    UpdateLipClosureConfiguration(m_fineFittingConfig);
    UpdateIcpConfiguration(m_fineFittingConfig);
    UpdateIcpWeights(searchWeights);

    m->modelFlowConstraints.SetFlowWeight(m_fineFittingConfig["modelFlowWeight"].template Value<T>());
    m->uvFlowConstraints.SetFlowWeight(m_fineFittingConfig["uvFlowWeight"].template Value<T>());

    const T eyeballWeight = m_fineFittingConfig["eyeballWeight"].template Value<T>();
    Configuration eyeballConfig = m->leftEyeballConstraints.GetConfiguration();
    eyeballConfig["eyeball"].Set(eyeballWeight);
    m->leftEyeballConstraints.SetConfiguration(eyeballConfig);
    m->rightEyeballConstraints.SetConfiguration(eyeballConfig);

    const T collisionWeight = m_fineFittingConfig["collisionWeight"].template Value<T>();
    const T lipClosureWeight = m_fineFittingConfig["lipClosureWeight"].template Value<T>();

    bool useInitialCorrespondences = true;
    bool firstCall = true;
    Mesh<T> currentMesh = m->sourceMesh;
    Eigen::Matrix<T, 3, -1> baseVertices;
    const bool sampleScan = m_fineFittingConfig["sampleScan"].template Value<bool>();
    const bool useModelFlow = m_fineFittingConfig["useModelOpticalFlow"].template Value<bool>();
    const bool useUvFlow = m_fineFittingConfig["useUVOpticalFlow"].template Value<bool>();
    const bool useEyeballConstraint = m_fineFittingConfig["useEyeballConstraint"].template Value<bool>();
    const bool use3dLandmarks = !m->landmarkConstraints3d.empty();
    const bool use2dLandmarks = !m->landmarkConstraints2d.empty();
    const bool useLipClosure = !m->lipClosureConstraints.empty() && lipClosureWeight > T(0);

    if (!use3dLandmarks && !use2dLandmarks)
    {
        LOG_WARNING("No landmark constraints set for fine face fitting.");
    }

    std::vector<std::shared_ptr<const typename CollisionConstraints<T>::Data>> selfCollisionConstraintsData;
    std::vector<std::shared_ptr<const typename CollisionConstraints<T>::Data>> staticCollisionConstraintsData;

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context)
        {
            Cost<T> cost;

            const auto [stabilizedVertices, _] = deformationModelVertex.EvaluateBothStabilizedAndTransformedVertices(context);
            std::vector<DiffDataMatrix<T, 3, -1>> transformedVertices;

            for (auto& transformVar : face2scanTransformVariables)
            {
                DiffDataAffine<T, 3, 3> diffFace2ScanTransform = transformVar.EvaluateAffine(context);
                transformedVertices.push_back(diffFace2ScanTransform.Transform(stabilizedVertices));
            }

            for (int i = 0; i < int(m->icpConstraints.size()); ++i)
            {
                if (!m->fixedCorrespondenceData.empty())
                {
                    if (m->fixedCorrespondenceData[i])
                    {
                        DiffDataMatrix<T, 3, -1> srcCorrespondences = GatherFunction<T>::template GatherColumns<3, -1, -1>(transformedVertices[i], m->fixedCorrespondenceData[i]->srcIDs);
                        Eigen::Matrix<T, 3, -1> tgtCorrespondences = m->fixedCorrespondenceData[i]->EvaluateTargetBCs(m->icpConstraints[i]->TargetMesh()->Vertices());
                        cost.Add(PointPointConstraintFunction<T, 3>::Evaluate(srcCorrespondences,
                                                                              tgtCorrespondences,
                                                                              knownCorrespondencesWeights[i],
                                                                              T(1)),T(1));
                    }
                    else
                    {
                        LOG_ERROR("No set correspondences for frame {}", i);
                    }
                }
                else
                {
                    if (context || firstCall)
                    {
                        if (useInitialCorrespondences)
                        {
                            currentMesh.SetVertices(face2scanTransformVariables[i].Affine().Transform(m->CurrentDeformed()));
                        }
                        else
                        {
                            currentMesh.SetVertices(transformedVertices[i].Matrix());
                        }
                        currentMesh.CalculateVertexNormals();
                        if (context)
                        {
                            // when we use a Jacobian then we have an update step, and then we should not use the initial
                            // correspondences
                            useInitialCorrespondences = false;
                        }

                        m->icpConstraints[i]->SetupCorrespondences(currentMesh, sampleScan);
                    }

                    Cost<T> icpResidual = m->icpConstraints[i]->EvaluateICP(transformedVertices[i]);
                    if (icpResidual.Size() > 0)
                    {
                        // resize the "energy of the ICP constraints" to be the same whether model or scan are sampled
                        cost.Add(std::move(icpResidual), (T(1) / T(icpResidual.Size()) * T(96196)) / T(m->icpConstraints.size()));
                    }
                }

                if (use2dLandmarks)
                {
                    cost.Add(m->landmarkConstraints2d[i]->Evaluate(transformedVertices[i], currentMesh.VertexNormals()), T(1));
                }
                if (use3dLandmarks)
                {
                    cost.Add(m->landmarkConstraints3d[i]->Evaluate(transformedVertices[i], currentMesh.VertexNormals()), T(1));
                }

                if (useLipClosure && (i == 0))
                {
                    if (context)
                    {
                        const Eigen::Transform<T, 3, Eigen::Affine> toFace(face2scanTransformVariables[i].Affine().Matrix());
                        m->lipClosureConstraints[i]->CalculateLipClosureData(currentMesh.Vertices(),
                                                                             currentMesh.VertexNormals(),
                                                                             Eigen::Transform<T, 3, Eigen::Affine>::Identity(),
                                                                             /*useLandmarks=*/true,
                                                                             toFace.inverse());
                    }

                    VertexConstraints<T, 3, 1> vertexConstraints;
                    m->lipClosureConstraints[i]->EvaluateLipClosure(currentMesh.Vertices(), vertexConstraints);
                    if (context || (baseVertices.size() == 0))
                    {
                        baseVertices = currentMesh.Vertices();
                    }
                    cost.Add(ApplyVertexConstraints(transformedVertices[i], baseVertices, vertexConstraints), T(1),
                             "lip closure");
                }

                if (i == 0)
                {
                    if (useModelFlow)
                    {
                        cost.Add(m->modelFlowConstraints.Evaluate(transformedVertices[i]), T(1));
                    }

                    if (useUvFlow)
                    {
                        cost.Add(m->uvFlowConstraints.Evaluate(transformedVertices[i]), T(1));
                    }
                }
            }

            if (collisionWeight > 0)
            {
                if (context || firstCall)
                {
                    selfCollisionConstraintsData.clear();
                    for (const auto& contraints : m->selfCollisionConstraints)
                    {
                        selfCollisionConstraintsData.push_back(contraints.CalculateCollisions(stabilizedVertices.Matrix(),
                                                                                              stabilizedVertices.Matrix()));
                    }
                    staticCollisionConstraintsData.clear();
                    for (size_t j = 0; j < std::min(m->staticCollisionConstraints.size(), m->staticCollisionVertices.size());
                         ++j)
                    {
                        staticCollisionConstraintsData.push_back(m->staticCollisionConstraints[j].CalculateCollisions(
                                                                     stabilizedVertices.Matrix(),
                                                                     m->
                                                                     staticCollisionVertices[j]));
                    }
                }
                for (const auto& constraintsData : selfCollisionConstraintsData)
                {
                    if (constraintsData)
                    {
                        cost.Add(constraintsData->Evaluate(stabilizedVertices, stabilizedVertices), collisionWeight);
                    }
                }
                for (size_t j = 0; j < staticCollisionConstraintsData.size(); ++j)
                {
                    if (staticCollisionConstraintsData[j])
                    {
                        cost.Add(staticCollisionConstraintsData[j]->Evaluate(stabilizedVertices, m->staticCollisionVertices[j]),
                                 collisionWeight);
                    }
                }
            }
            else
            {
                selfCollisionConstraintsData.clear();
                staticCollisionConstraintsData.clear();
            }

            if (useEyeballConstraint)
            {
                cost.Add(m->rightEyeballConstraints.EvaluateEyeballConstraints(stabilizedVertices), T(1));
                cost.Add(m->leftEyeballConstraints.EvaluateEyeballConstraints(stabilizedVertices), T(1));
            }

            cost.Add(deformationModelVertex.EvaluateModelConstraints(context), T(1));

            firstCall = false;

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, numIterations))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
        m->sourceOffsets = deformationModelVertex.VertexOffsets();
        m->UpdateDeformed();
    }
    else
    {
        LOG_WARNING("could not solve optimization problem");
    }

    std::vector<Affine<T, 3, 3>> updatedTransforms(face2scanTransformVariables.size());
    for (int i = 0; i < int(face2scanTransformVariables.size()); ++i)
    {
        updatedTransforms[i] = face2scanTransformVariables[i].Affine();
    }

    return updatedTransforms;
}

template <class T>
std::shared_ptr<const FaceFittingConstraintsDebugInfo<T>> FaceFitting<T>::CurrentDebugConstraints(const Affine<T, 3,
                                                                                                               3>& source2target, int scanFrame)
{
    if (m->constraintsDebugInfo)
    {
        return m->constraintsDebugInfo;
    }

    if (scanFrame >= (int)m->icpConstraints.size())
    {
        return m->constraintsDebugInfo;
    }

    Mesh<T> mesh = m->sourceMesh;
    mesh.SetVertices(source2target.Transform(m->CurrentDeformed()));
    mesh.CalculateVertexNormals();
    DiffDataMatrix<T, 3, -1> diffVertices(mesh.Vertices());

    std::shared_ptr<FaceFittingConstraintsDebugInfo<T>> constraintsDebugInfo =
        std::make_shared<FaceFittingConstraintsDebugInfo<T>>();
    m->icpConstraints[scanFrame]->FindCorrespondences(mesh.Vertices(), mesh.VertexNormals(),
                                                      constraintsDebugInfo->correspondences);
    if (m->landmarkConstraints2d.size() > 0)
    {
        m->landmarkConstraints2d[scanFrame]->EvaluateLandmarks(diffVertices,
                                                               LandmarkConstraints2D<T>::MeshType::Face,
                                                               &constraintsDebugInfo->landmarkConstraints);
        m->landmarkConstraints2d[scanFrame]->EvaluateCurves(diffVertices,
                                                            LandmarkConstraints2D<T>::MeshType::Face,
                                                            &constraintsDebugInfo->curveConstraints);
        m->landmarkConstraints2d[scanFrame]->EvaluateInnerLips(diffVertices,
                                                               mesh.VertexNormals(),
                                                               &constraintsDebugInfo->lipConstraintsUpper,
                                                               &constraintsDebugInfo->lipConstraintsLower);
    }
    m->constraintsDebugInfo = constraintsDebugInfo;
    return constraintsDebugInfo;
}

template <class T>
void FaceFitting<T>::UpdateIcpConfiguration(const Configuration& targetConfig)
{
    for (auto icpConstr : m->icpConstraints)
    {
        Configuration currentConfig = icpConstr->GetConfiguration();
        currentConfig["geometryWeight"] = targetConfig["geometryWeight"];
        currentConfig["point2point"] = targetConfig["point2point"];
        currentConfig["useDistanceThreshold"] = targetConfig["useDistanceThreshold"];
        currentConfig["minimumDistanceThreshold"] = targetConfig["minimumDistanceThreshold"];
        icpConstr->SetConfiguration(currentConfig);
    }
}

template <class T>
void FaceFitting<T>::Update2DLandmarkConfiguration(const Configuration& targetConfig)
{
    for (auto landmarkConstr : m->landmarkConstraints2d)
    {
        Configuration currentConfig = landmarkConstr->GetConfiguration();
        currentConfig["landmarksWeight"] = targetConfig["landmarksWeight"];
        currentConfig["innerLipWeight"] = targetConfig["innerLipWeight"];
        currentConfig["curveResampling"] = targetConfig["curveResampling"];
        landmarkConstr->SetConfiguration(currentConfig);
    }
}

template <class T>
void FaceFitting<T>::UpdateLipClosureConfiguration(const Configuration& targetConfig)
{
    for (auto lipClosureConstr : m->lipClosureConstraints)
    {
        const T weight = targetConfig["lipClosureWeight"].template Value<T>();
        lipClosureConstr->Config()["lip closure weight"].Set(weight);
    }
}

template <class T>
void FaceFitting<T>::Update3DLandmarkConfiguration(const Configuration& targetConfig)
{
    for (auto landmarkConstr : m->landmarkConstraints3d)
    {
        Configuration currentConfig = landmarkConstr->GetConfiguration();
        currentConfig["landmarksWeight"] = targetConfig["3DlandmarksWeight"];
        currentConfig["innerLipWeight"] = targetConfig["innerLipWeight"];
        currentConfig["curveResampling"] = targetConfig["curveResampling"];
        landmarkConstr->SetConfiguration(currentConfig);
    }
}

template <class T>
void FaceFitting<T>::UpdateIcpWeights(const VertexWeights<T>& weights)
{
    for (auto icp : m->icpConstraints)
    {
        icp->SetSourceWeights(weights);
        icp->ClearPreviousCorrespondences();
    }
}

// explicitly instantiate the face fitting classes
template class FaceFitting<float>;
//template class FaceFitting<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
