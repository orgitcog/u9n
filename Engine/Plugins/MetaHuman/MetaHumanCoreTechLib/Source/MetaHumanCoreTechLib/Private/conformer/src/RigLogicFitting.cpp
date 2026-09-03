// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/RigLogicFitting.h>

#include <nls/Cost.h>
#include <nls/DiffDataMatrix.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/solver/LMSolver.h>
#include <nls/solver/BoundedCoordinateDescentSolver.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/functions/BarycentricCoordinatesFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/PointSurfaceConstraintFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/VertexConstraintsFunction.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nrr/ICPConstraints.h>
#include <nrr/CollisionConstraints.h>
#include <nrr/landmarks/LipClosure.h>
#include <nrr/LipClosureConstraints.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct RigLogicFitting<T>::Private
{
    //! The source mesh (the vertices are the latest deformed state, or set by the user)
    Mesh<T> sourceMesh;

    //! Structure to keep lip closure data points
    std::vector<LipClosure3D<T>> lipClosure;

    //! Structure to calculate 2d landmark constraints
    std::vector<std::shared_ptr<LandmarkConstraints2D<T>>> landmarkConstraints2d;

    //! Structure to calculate 3d landmark constraints
    std::vector<std::shared_ptr<LandmarkConstraints3D<T>>> landmarkConstraints3d;

    //! Structure to calculate lip closure constraints
    std::vector<std::shared_ptr<LipClosureConstraints<T>>> lipClosureConstraints;

    // ! Structure to keep calculated correspondences
    std::vector<std::shared_ptr<const fitting_tools::CorrespondenceData<T>>> fixedCorrespondenceData;

    //! Structure to keep mesh landmarks
    MeshLandmarks<T> meshLandmarks;

    //! Target mesh pointers
    std::vector<std::shared_ptr<const Mesh<T>>> targetMeshes;

    //! An identity model for part-based nonrigid registration
    DeformationModelRigLogic<T> deformationModelRigLogic;

    CollisionConstraints<T> lipCollisionConstraints;

    std::vector<std::shared_ptr<ICPConstraints<T>>> icpConstraints;

    VertexWeights<T> maskUpperLip;
    VertexWeights<T> maskLowerLip;
};

template <class T>
RigLogicFitting<T>::RigLogicFitting() : m(new Private)
{}

template <class T> RigLogicFitting<T>::~RigLogicFitting() = default;
template <class T> RigLogicFitting<T>::RigLogicFitting(RigLogicFitting&& other) = default;
template <class T> RigLogicFitting<T>& RigLogicFitting<T>::operator=(RigLogicFitting&& other) = default;


template <class T>
void RigLogicFitting<T>::InitIcpConstraints(int numOfObservations)
{
    if (int(m->icpConstraints.size()) != numOfObservations)
    {
        m->icpConstraints.resize(numOfObservations);
    }

    for (int i = 0; i < numOfObservations; i++)
    {
        m->icpConstraints[i] = std::make_shared<ICPConstraints<T>>();
    }
}

template <class T>
void RigLogicFitting<T>::Init2DLandmarksConstraints(int numOfObservations)
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
void RigLogicFitting<T>::Init3DLandmarksConstraints(int numOfObservations)
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
void RigLogicFitting<T>::LoadRig(dna::Reader* dnaRig)
{
    std::shared_ptr<Rig<T>> rig = std::make_shared<Rig<T>>();
    if (!rig->LoadRig(dnaRig))
    {
        CARBON_CRITICAL("Unable to initialize rig logic from loaded dna.");
    }
    SetRig(rig);
}

template <class T>
void RigLogicFitting<T>::SetFixedCorrespondenceData(const std::vector<std::shared_ptr<const fitting_tools::CorrespondenceData<T>>>& correspondenceData)
{
    m->fixedCorrespondenceData = correspondenceData;
}

template <class T>
void RigLogicFitting<T>::ClearFixedCorrespondeceData()
{
    m->fixedCorrespondenceData.clear();
}

template <class T>
bool RigLogicFitting<T>::HasFixedCorrespondenceData() const
{
    return !m->fixedCorrespondenceData.empty();
}

template <class T>
void RigLogicFitting<T>::SetInnerLipInterfaceVertices(const VertexWeights<T>& maskUpperLip, const VertexWeights<T>& maskLowerLip)
{
    m->maskUpperLip = maskUpperLip;
    m->maskLowerLip = maskLowerLip;
    m->lipCollisionConstraints.SetSourceTopology(m->sourceMesh, m->maskUpperLip.NonzeroVertices());
    m->lipCollisionConstraints.SetTargetTopology(m->sourceMesh, m->maskLowerLip.NonzeroVertices());
}

template <class T>
void RigLogicFitting<T>::SetRig(const std::shared_ptr<const Rig<T>>& rig)
{
    if (rig != m->deformationModelRigLogic.GetRig())
    {
        m->deformationModelRigLogic = DeformationModelRigLogic<T>();
        m->deformationModelRigLogic.SetRig(rig);
        m->sourceMesh = rig->GetRigGeometry()->GetMesh(0);
        m->sourceMesh.Triangulate();
        m->sourceMesh.CalculateVertexNormals();
    }
}

template <class T>
void RigLogicFitting<T>::SetMeshLandmarks(const MeshLandmarks<T>& meshLandmarks) { m->meshLandmarks = meshLandmarks; }

template <class T>
void RigLogicFitting<T>::SetTargetDepths(const std::vector<std::vector<std::shared_ptr<const DepthmapData<T>>>>& targetDepths)
{
    InitIcpConstraints(int(targetDepths.size()));

    for (size_t i = 0; i < targetDepths.size(); ++i)
    {
        for (size_t j = 0; j < targetDepths[i].size(); ++j)
        {
            m->icpConstraints[i]->AddTargetDepthAndNormals(targetDepths[i][j]);
        }
    }
}

template <class T>
void RigLogicFitting<T>::SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& targetMeshes, const std::vector<Eigen::VectorX<T>>& targetWeights)
{
    InitIcpConstraints(int(targetMeshes.size()));
    m->targetMeshes = targetMeshes;

    for (int i = 0; i < int(targetMeshes.size()); ++i)
    {
        m->icpConstraints[i]->SetTargetMesh(*targetMeshes[i]);
        if (targetWeights.size() > 0)
        {
            m->icpConstraints[i]->SetTargetWeights(targetWeights[i]);
        }
    }
}

template <class T>
void RigLogicFitting<T>::SetGuiControls(const Eigen::VectorX<T>& currentControls) { m->deformationModelRigLogic.SetGuiControls(currentControls); }

template <class T>
void RigLogicFitting<T>::SetTarget2DLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>>& landmarks)
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
                                                     m->maskUpperLip.NonzeroVertices(),
                                                     m->meshLandmarks.InnerUpperLipContourLines(),
                                                     m->maskLowerLip.NonzeroVertices(),
                                                     m->meshLandmarks.InnerLowerLipContourLines());
            m->lipClosureConstraints[i]->SetLipClosure(m->lipClosure[i]);
        }
    }
}

template <class T>
void RigLogicFitting<T>::SetTarget3DLandmarks(const std::vector<LandmarkInstance<T, 3>>& landmarks)
{
    Init3DLandmarksConstraints(int(landmarks.size()));

    for (int i = 0; i < int(landmarks.size()); ++i)
    {
        m->landmarkConstraints3d[i]->SetMeshLandmarks(m->meshLandmarks);
        m->landmarkConstraints3d[i]->SetTargetLandmarks(landmarks[i]);
    }
}

template <class T>
Eigen::VectorX<T> RigLogicFitting<T>::CurrentGuiControls() const
{
    return m->deformationModelRigLogic.GuiControls();
}

template <class T>
Eigen::Matrix<T, 3, -1> RigLogicFitting<T>::CurrentVertices(const int meshId) { return m->deformationModelRigLogic.DeformedVertices(meshId); }

template <class T>
std::vector<Affine<T, 3, 3>> RigLogicFitting<T>::RegisterRigLogic(const std::vector<Affine<T, 3, 3>>& source2target,
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

    const bool optimizePose = m_rigLogicFittingConfig["optimizePose"].template Value<bool>();
    std::vector<AffineVariable<QuaternionVariable<T>>> face2scanTransformVariables(source2target.size());
    for (int i = 0; i < int(face2scanTransformVariables.size()); ++i)
    {
        face2scanTransformVariables[i].SetAffine(source2target[i]);
        face2scanTransformVariables[i].MakeConstant(!optimizePose, !optimizePose);
    }

    Update2DLandmarkConfiguration(m_rigLogicFittingConfig);
    Update3DLandmarkConfiguration(m_rigLogicFittingConfig);
    const T landmarksWeights3D = m_rigLogicFittingConfig["3DlandmarksWeight"].template Value<T>();

    UpdateLipClosureConfiguration(m_rigLogicFittingConfig);
    UpdateIcpConfiguration(m_rigLogicFittingConfig);
    UpdateIcpWeights(searchWeights);

    const T lipClosureWeight = m_rigLogicFittingConfig["lipClosureWeight"].template Value<T>();

    const bool use3dLandmarks = !m->landmarkConstraints3d.empty();
    const bool use2dLandmarks = !m->landmarkConstraints2d.empty();
    const bool useLipClosure = !m->lipClosureConstraints.empty() && lipClosureWeight > T(0);

    if (!use3dLandmarks && !use2dLandmarks)
    {
        LOG_WARNING("No landmark constraints set for riglogic fitting.");
    }

    Mesh<T> currentMesh = m->sourceMesh;
    Eigen::Matrix<T, 3, -1> baseVertices;
    const T collisionWeight = m_rigLogicFittingConfig["collisionWeight"].template Value<T>();

    typename RigGeometry<T>::State state;

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;

            m->deformationModelRigLogic.EvaluateVertices(context, /*lod=*/0, /*meshIndices=*/{ 0 }, /*withRigid=*/false, state);
            const DiffDataMatrix<T, 3, -1>& stabilizedVerticesTmp = state.Vertices().front();
            // rig logic evaluation outputs a dense jacobian, but subsequent jacobian operations assume it to be spare, hence we convert the jacobian directly
            // to
            // a sparse jacobian
            std::shared_ptr<SparseJacobian<T>> sparseJacobian;
            if (stabilizedVerticesTmp.HasJacobian())
            {
                const auto& jac = stabilizedVerticesTmp.Jacobian();
                sparseJacobian = std::make_shared<SparseJacobian<T>>(jac.AsSparseMatrix(), jac.StartCol());
            }
            DiffDataMatrix<T, 3, -1> stabilizedVertices(stabilizedVerticesTmp.Matrix(), sparseJacobian);
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
                        Eigen::Matrix<T, 3, -1> tgtCorrespondences = m->fixedCorrespondenceData[i]->EvaluateTargetBCs(m->targetMeshes[i]->Vertices());
                        cost.Add(PointPointConstraintFunction<T, 3>::Evaluate(srcCorrespondences,
                                                                              tgtCorrespondences,
                                                                              knownCorrespondencesWeights[i],
                                                                              T(1)), T(1));
                    }
                    else
                    {
                        LOG_ERROR("No set correspondences for frame {}", i);
                    }
                }
                else
                {
                    currentMesh.SetVertices(transformedVertices[i].Matrix());
                    currentMesh.CalculateVertexNormals();
                    cost.Add(m->icpConstraints[i]->EvaluateICP(transformedVertices[i], currentMesh.VertexNormals(), /*searchCorrespondences=*/bool(context)), T(1));
                }

                if (use2dLandmarks)
                {
                    cost.Add(m->landmarkConstraints2d[i]->Evaluate(transformedVertices[i], currentMesh.VertexNormals()), T(1));
                }
                if (use3dLandmarks)
                {
                    cost.Add(m->landmarkConstraints3d[i]->EvaluateLandmarks(transformedVertices[i]), landmarksWeights3D);
                }
                if (useLipClosure && m->lipClosureConstraints[i]->ValidLipClosure())
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
                    VertexConstraints<T, 3, 4> vertexConstraints;
                    m->lipClosureConstraints[i]->EvaluateLipClosure(currentMesh.Vertices(), vertexConstraints);
                    if (context || (baseVertices.size() == 0))
                    {
                        baseVertices = currentMesh.Vertices();
                    }
                    cost.Add(ApplyVertexConstraints(transformedVertices[i], baseVertices, vertexConstraints), T(1), "lip closure");
                }
            }

            if (collisionWeight > T(0))
            {
                auto collisionConstraintsData = m->lipCollisionConstraints.CalculateCollisions(stabilizedVertices.Matrix(), stabilizedVertices.Matrix());
                cost.Add(collisionConstraintsData->Evaluate(stabilizedVertices, stabilizedVertices), collisionWeight);
            }

            cost.Add(m->deformationModelRigLogic.EvaluateModelConstraints(context), T(1));

            return cost.CostToDiffData();
        };

    T l1reg = m_rigLogicFittingConfig["l1regularization"].template Value<T>();

    BoundedCoordinateDescentSolverSettings<T> settings;
    settings.l1reg = l1reg;
    settings.iterations = numIterations;
    const T startEnergy = BoundedCoordinateDescentSolver<T>::Evaluate(evaluationFunction, { m->deformationModelRigLogic.SolveControlVariable() }, settings);
    Context<T> context;
    auto threadPool = TaskThreadPool::GlobalInstance(false);
    if (BoundedCoordinateDescentSolver<T>::Solve(evaluationFunction, context, { m->deformationModelRigLogic.SolveControlVariable() }, settings, threadPool.get()))
    {
        const T finalEnergy = BoundedCoordinateDescentSolver<T>::Evaluate(evaluationFunction, { m->deformationModelRigLogic.SolveControlVariable() }, settings);
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
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
void RigLogicFitting<T>::UpdateIcpConfiguration(const Configuration& targetConfig)
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
void RigLogicFitting<T>::Update2DLandmarkConfiguration(const Configuration& targetConfig)
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
void RigLogicFitting<T>::Update3DLandmarkConfiguration(const Configuration& targetConfig)
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
void RigLogicFitting<T>::UpdateLipClosureConfiguration(const Configuration& targetConfig)
{
    for (auto lipClosureConstr : m->lipClosureConstraints)
    {
        const T weight = targetConfig["lipClosureWeight"].template Value<T>();
        lipClosureConstr->Config()["lip closure weight"].Set(weight);
    }
}

template <class T>
void RigLogicFitting<T>::UpdateIcpWeights(const VertexWeights<T>& weights)
{
    for (auto icp : m->icpConstraints)
    {
        icp->ClearPreviousCorrespondences();
        icp->SetCorrespondenceSearchVertexWeights(weights);
    }
}

// explicitly instantiate the rig logic fitting classes
template class RigLogicFitting<float>;
//template class RigLogicFitting<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
