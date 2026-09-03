// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/PcaFittingWrapper.h>

#include <nrr/PCAFaceFitting.h>
#include <nls/Cost.h>
#include <nls/DiffDataMatrix.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/functions/PointSurfaceConstraintFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nrr/ICPConstraints.h>
#include <nrr/deformation_models/DeformationModelRigid.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct PcaRigFitting<T>::Private
{
    //! The mesh topology
    Mesh<T> topology;
    Mesh<T> triangulatedTopology;

    //! Structure to keep lip closure data points
    std::vector<LipClosure3D<T>> lipClosure;

    //! Structure to calculate 2d landmark constraints
    std::vector<std::shared_ptr<LandmarkConstraints2D<T>>> landmarkConstraints2d;

    //! Structure to calculate lip closure constraints
    std::vector<std::shared_ptr<LipClosureConstraints<T>>> lipClosureConstraints;

    //! Structure to keep mesh landmarks
    MeshLandmarks<T> headMeshLandmarks;
    MeshLandmarks<T> teethMeshLandmarks;
    MeshLandmarks<T> eyeLeftMeshLandmarks;
    MeshLandmarks<T> eyeRightMeshLandmarks;

    std::shared_ptr<PCAFaceFitting> pcaFaceTracking;
    std::vector<PCAFaceFitting::State> pcaFaceTrackingStates;

    FlowConstraints<T> modelFlowConstraints;

    std::vector<std::shared_ptr<ICPConstraints<T>>> icpConstraints;

    Eigen::Matrix<T, 3, -1> currentDeformed[4];
    Eigen::VectorX<T> pcaCoeffs;
    Eigen::VectorX<T> pcaCoeffsNeck;
    VertexWeights<T> maskUpperLip;
    VertexWeights<T> maskLowerLip;
};

template <class T>
PcaRigFitting<T>::PcaRigFitting() : m(new Private)
{}

template <class T> PcaRigFitting<T>::~PcaRigFitting() = default;
template <class T> PcaRigFitting<T>::PcaRigFitting(PcaRigFitting&& other) = default;
template <class T> PcaRigFitting<T>& PcaRigFitting<T>::operator=(PcaRigFitting&& other) = default;

template <class T>
void PcaRigFitting<T>::SetFlowConstraints(const std::map<std::string, std::shared_ptr<const FlowConstraintsData<T>>>& flowConstraintsData)
{
    m_pcaRigFittingConfig["useFlowConstraints"].Set(true);
    m->modelFlowConstraints.SetFlowData(flowConstraintsData);
    m->modelFlowConstraints.SetFlowWeight(m_pcaRigFittingConfig["flowWeight"].template Value<T>());
}

template <class T>
bool PcaRigFitting<T>::HasFlowConstraints() const
{
    return m->modelFlowConstraints.FlowData().size() > 0;
}

template <class T>
void PcaRigFitting<T>::InitIcpConstraints(int numOfObservations)
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
void PcaRigFitting<T>::Init2DLandmarksConstraints(int numOfObservations)
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
void PcaRigFitting<T>::LoadRig(dna::Reader* reader)
{
    m->pcaFaceTracking = std::make_shared<PCAFaceFitting>();
    m->pcaFaceTracking->LoadPCARig(reader);
    m->pcaCoeffs = Eigen::VectorX<T> {};
    m->pcaCoeffsNeck = Eigen::VectorX<T> {};
    m->pcaFaceTrackingStates.clear();
}

template <class T>
void PcaRigFitting<T>::SaveRig(dna::Writer* writer) { m->pcaFaceTracking->SavePCARig(writer); }

template <class T>
bool PcaRigFitting<T>::IsRigLoaded() const { return m->pcaFaceTracking ? true : false; }

template <class T>
bool PcaRigFitting<T>::HasNeckPCA() const { return m->pcaFaceTracking ? (m->pcaFaceTracking->GetPCARig().neckPCA.NumPCAModes() > 0) : false; }

template <class T>
rt::PCARig PcaRigFitting<T>::GetRig()
{
    CARBON_ASSERT(m->pcaFaceTracking, "PCA rig not loaded.");
    return m->pcaFaceTracking->GetPCARig();
}

template <class T>
void PcaRigFitting<T>::SetInnerLipInterfaceVertices(const VertexWeights<T>& maskUpperLip, const VertexWeights<T>& maskLowerLip)
{
    m->maskUpperLip = maskUpperLip;
    m->maskLowerLip = maskLowerLip;
}

template <class T>
void PcaRigFitting<T>::SetMeshLandmarks(const MeshLandmarks<T>& headMeshLandmarks,
                                        const MeshLandmarks<T>& teethMeshLandmarks,
                                        const MeshLandmarks<T>& eyeLeftMeshLandmarks,
                                        const MeshLandmarks<T>& eyeRightMeshLandmarks)
{
    m->headMeshLandmarks = headMeshLandmarks;
    m->teethMeshLandmarks = teethMeshLandmarks;
    m->eyeLeftMeshLandmarks = eyeLeftMeshLandmarks;
    m->eyeRightMeshLandmarks = eyeRightMeshLandmarks;
}

template <class T>
void PcaRigFitting<T>::SetTargetDepths(const std::vector<std::vector<std::shared_ptr<const DepthmapData<T>>>>& targetDepths)
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
void PcaRigFitting<T>::SetTopology(const Mesh<T>& topology)
{
    m->topology = topology;
    m->triangulatedTopology = topology;
    m->triangulatedTopology.Triangulate();
}

template <class T>
void PcaRigFitting<T>::SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& targetMeshes, const std::vector<Eigen::VectorX<T>>& targetWeights)
{
    InitIcpConstraints(int(targetMeshes.size()));

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
void PcaRigFitting<T>::SetTarget2DLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>>& landmarks)
{
    Init2DLandmarksConstraints(int(landmarks.size()));

    for (int i = 0; i < int(landmarks.size()); ++i)
    {
        m->landmarkConstraints2d[i]->SetMeshLandmarks(m->headMeshLandmarks);
        m->landmarkConstraints2d[i]->SetTargetLandmarks(landmarks[i]);

        for (const auto& [landmarkInstance, camera] : landmarks[i])
        {
            m->lipClosure[i].Add(landmarkInstance, camera);
        }
        if (m->lipClosure[i].Valid())
        {
            m->lipClosureConstraints[i]->SetTopology(m->topology,
                                                     m->maskUpperLip.NonzeroVertices(),
                                                     m->headMeshLandmarks.InnerUpperLipContourLines(),
                                                     m->maskLowerLip.NonzeroVertices(),
                                                     m->headMeshLandmarks.InnerLowerLipContourLines());
            m->lipClosureConstraints[i]->SetLipClosure(m->lipClosure[i]);
        }
    }
}

template <class T>
void PcaRigFitting<T>::SetCurrentVertices(const Eigen::Matrix<T, 3, -1>& headVertices)
{
    if ((int)headVertices.cols() != m->topology.NumVertices())
    {
        CARBON_CRITICAL("Invalid number of vertices");
    }

    const auto& pcaRig = m->pcaFaceTracking->GetPCARig();
    rt::HeadVertexState<float> state;
    state.faceVertices = headVertices;
    state.faceVertices.colwise() += pcaRig.offset;
    m->pcaCoeffs = pcaRig.Project(state, m->pcaCoeffs);
    if (pcaRig.NumCoeffsNeck() > 0)
    {
        rt::HeadVertexState<float> pcaState = pcaRig.EvaluatePCARig(m->pcaCoeffs);
        m->pcaCoeffsNeck = pcaRig.ProjectNeck(state, pcaState, m->pcaCoeffsNeck);
    }
}

template <class T>
Eigen::Matrix<T, 3, -1> PcaRigFitting<T>::CurrentVertices(const int meshId)
{
    CARBON_ASSERT(meshId < 4, "mesh id out of scope");
    if (meshId < 4)
    {
        return m->currentDeformed[meshId];
    }
    return Eigen::Matrix<T, 3, -1>{};
}

template <class T>
std::vector<Affine<T, 3, 3>> PcaRigFitting<T>::RegisterPcaRig(const std::vector<Affine<T, 3, 3>>& source2target,
                                                              const VertexWeights<T>& faceSearchWeights,
                                                              const VertexWeights<T>& neckSearchWeights,
                                                              int numIterations)
{
    CARBON_ASSERT(m->icpConstraints.size() == source2target.size(), "number of targets does not match number of icp constraints");

    Update2DLandmarkConfiguration(m_pcaRigFittingConfig);
    UpdateLipClosureConfiguration(m_pcaRigFittingConfig);
    UpdateIcpConfiguration(m_pcaRigFittingConfig);
    UpdateIcpWeights(faceSearchWeights);

    m->pcaFaceTracking->LoadFaceMeshLandmarks(m->headMeshLandmarks);
    m->pcaFaceTracking->LoadEyeLeftMeshLandmarks(m->eyeLeftMeshLandmarks);
    m->pcaFaceTracking->LoadEyeRightMeshLandmarks(m->eyeRightMeshLandmarks);
    m->pcaFaceTracking->LoadTeethMeshLandmarks(m->teethMeshLandmarks);

    const bool use2dLandmarks = !m->landmarkConstraints2d.empty();

    if (!use2dLandmarks)
    {
        LOG_WARNING("No landmark constraints set for pca rig fitting.");
    }

    Mesh<T> currentMesh = m->triangulatedTopology;

    PCAFaceFitting::Settings settings;
    settings.iterations = numIterations;
    settings.pcaRegularization = m_pcaRigFittingConfig["regularization"].template Value<float>();
    settings.withRigid = m_pcaRigFittingConfig["optimizePose"].template Value<bool>();
    settings.pcaVelocityRegularization = m_pcaRigFittingConfig["velocity"].template Value<float>();
    settings.pcaAccelerationRegularization = m_pcaRigFittingConfig["acceleration"].template Value<float>();

    const bool withFlow = m_pcaRigFittingConfig["useFlowConstraints"].template Value<bool>();

    std::vector<Affine<T, 3, 3>> updatedTransforms = source2target;
    std::vector<Eigen::VectorXf> pcaCoeffsPrevFrames;

    if (m->pcaCoeffs.size() != m->pcaFaceTracking->GetPCARig().NumCoeffs())
    {
        m->pcaCoeffs = Eigen::VectorXf::Zero(m->pcaFaceTracking->GetPCARig().NumCoeffs());
    }

    if (m->pcaCoeffsNeck.size() != m->pcaFaceTracking->GetPCARig().NumCoeffsNeck())
    {
        m->pcaCoeffsNeck = Eigen::VectorXf::Zero(m->pcaFaceTracking->GetPCARig().NumCoeffsNeck());
    }

    Affine<float, 3, 3> rigid = source2target[0] * m->pcaFaceTracking->PCARigToMesh();
    std::vector<DepthmapConstraints> depthmapConstraints;
    std::vector<FlowConstraints<T>*> flowConstraints;
    if (withFlow)
    {
        flowConstraints.push_back(&m->modelFlowConstraints);
    }

    m->pcaFaceTracking->FitPCAData(m->triangulatedTopology,
                                    depthmapConstraints,
                                    m->icpConstraints[0].get(),
                                    m->landmarkConstraints2d[0].get(),
                                    flowConstraints,
                                    nullptr,
                                    m->lipClosureConstraints[0].get(),
                                    rigid,
                                    m->pcaCoeffs,
                                    pcaCoeffsPrevFrames,
                                    settings,
                                    m->pcaFaceTrackingStates);

    updatedTransforms[0] = rigid * m->pcaFaceTracking->PCARigToMesh().Inverse();

    const Eigen::Matrix3Xf facePCAVertices = m->pcaFaceTracking->GetPCARig().facePCA.EvaluateLinearized(m->pcaCoeffs, rt::LinearVertexModel<float>::EvaluationMode::STATIC);

    m->currentDeformed[0] = m->pcaFaceTracking->PCARigToMesh().Transform(facePCAVertices);
    m->currentDeformed[1] =
        m->pcaFaceTracking->PCARigToMesh().Transform(m->pcaFaceTracking->GetPCARig().teethPCA.EvaluateLinearized(m->pcaCoeffs,
                                                                                                                 rt::LinearVertexModel<float>::EvaluationMode::
                                                                                                                 STATIC));
    m->currentDeformed[2] =
        m->pcaFaceTracking->PCARigToMesh().Transform(m->pcaFaceTracking->GetPCARig().eyeLeftTransformPCA.EvaluateVertices(m->pcaCoeffs,
                                                                                                                          rt::LinearVertexModel<float>::
                                                                                                                          EvaluationMode::STATIC));
    m->currentDeformed[3] =
        m->pcaFaceTracking->PCARigToMesh().Transform(m->pcaFaceTracking->GetPCARig().eyeRightTransformPCA.EvaluateVertices(m->pcaCoeffs,
                                                                                                                           rt::LinearVertexModel<float>::
                                                                                                                           EvaluationMode::STATIC));

    DeformationModelRigid<T> deformationModelRigid;
    deformationModelRigid.SetVertices(m->currentDeformed[0]);

    for (int i = 1; i < (int)source2target.size(); ++i)
    {
        deformationModelRigid.SetRigidTransformation(source2target[i]);

        std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
                Cost<T> cost;

                const DiffDataMatrix<T, 3, -1> transformedVertices = deformationModelRigid.EvaluateVertices(context);

                if (context || !m->icpConstraints[i]->HasCorrespondences())
                {
                    currentMesh.SetVertices(transformedVertices.Matrix());
                    currentMesh.CalculateVertexNormals();
                }
                m->icpConstraints[i]->SetupCorrespondences(currentMesh.Vertices(), currentMesh.VertexNormals());
                cost.Add(m->icpConstraints[i]->EvaluateICP(transformedVertices), T(1));
                if (use2dLandmarks)
                {
                    cost.Add(m->landmarkConstraints2d[i]->Evaluate(transformedVertices, currentMesh.VertexNormals()), T(1));
                }

                return cost.CostToDiffData();
            };

        GaussNewtonSolver<T> solver;
        const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        if (solver.Solve(evaluationFunction, numIterations))
        {
            const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
            LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
            // m->constraintsDebugInfo.reset();
        }
        else
        {
            LOG_WARNING("could not solve optimization problem");
        }

        updatedTransforms[i] = deformationModelRigid.RigidTransformation();
    }

    if (m->pcaFaceTracking->GetPCARig().NumCoeffsNeck() > 0 && neckSearchWeights.NumVertices() == (int)facePCAVertices.cols())
    {
        settings.pcaRegularization = m_pcaRigFittingConfig["neckRegularization"].template Value<float>();
        UpdateIcpWeights(neckSearchWeights);
        m->pcaFaceTracking->FitPCADataNeck(currentMesh,
                                            depthmapConstraints,
                                            m->icpConstraints[0].get(),
                                            flowConstraints,
                                            nullptr,
                                            rigid, facePCAVertices, m->pcaCoeffsNeck, {}, settings, m->pcaFaceTrackingStates);

        Eigen::Matrix3Xf neckPCAVertices = m->pcaFaceTracking->GetPCARig().neckPCA.EvaluateLinearized(m->pcaCoeffsNeck, rt::LinearVertexModel<float>::EvaluationMode::STATIC);
        m->currentDeformed[0] = m->pcaFaceTracking->PCARigToMesh().Transform(facePCAVertices + neckPCAVertices);
    }

    return updatedTransforms;
}

template <class T>
void PcaRigFitting<T>::UpdateIcpConfiguration(const Configuration& targetConfig)
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
void PcaRigFitting<T>::Update2DLandmarkConfiguration(const Configuration& targetConfig)
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
void PcaRigFitting<T>::UpdateLipClosureConfiguration(const Configuration& targetConfig)
{
    for (auto lipClosureConstr : m->lipClosureConstraints)
    {
        const T weight = targetConfig["lipClosureWeight"].template Value<T>();
        lipClosureConstr->Config()["lip closure weight"].Set(weight);
    }
}

template <class T>
void PcaRigFitting<T>::UpdateIcpWeights(const VertexWeights<T>& weights)
{
    for (auto icp : m->icpConstraints)
    {
        icp->ClearPreviousCorrespondences();
        icp->SetCorrespondenceSearchVertexWeights(weights);
    }
}

// explicitly instantiate the rig logic fitting classes
template class PcaRigFitting<float>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
