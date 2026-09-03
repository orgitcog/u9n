// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/PCAFaceFitting.h>
#include <nls/math/ParallelBLAS.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

struct PCAFaceFitting::Private
{
    Private() : globalThreadPool(TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true)) {}

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> globalThreadPool;
};

void PCAFaceFitting::FitPCAData(const Mesh<float>& topology,
                                 std::vector<DepthmapConstraints>& vectorOfDepthmapConstraints,
                                 ICPConstraints<float>* icpConstraints,
                                 LandmarkConstraints2D<float>* landmarkConstraints,
                                 const std::vector<FlowConstraints<float>*>& vectorOfFlowConstraints,
                                 ImageConstraints<float>* imageConstraints,
                                 LipClosureConstraints<float>* lipClosureConstraints,
                                 Affine<float, 3, 3>& rigidMotion,
                                 Eigen::VectorXf& pcaCoeffs,
                                 const std::vector<Eigen::VectorXf>& pcaCoeffsPrevFrames,
                                 const Settings& settings,
                                 std::vector<State>& states) const
{
    // Timer timer;

    if (states.empty())
    {
        states.emplace_back(State());
    }
    const int stateId = 0;

    const int numPcaParameters = int(pcaCoeffs.size());
    const int numTotalParameters = numPcaParameters + (settings.withRigid ? 6 : 0);

    Eigen::VectorXf params(numTotalParameters);
    params.segment(0, numPcaParameters) = pcaCoeffs;
    if (settings.withRigid) { params.tail(6).setZero(); }

    QRigidMotion<float> qrm(rigidMotion.Matrix());

    Eigen::Matrix<float, 3, -1> faceNormals;

    const auto evaluationMode = settings.withRigid ? rt::LinearVertexModel<float>::EvaluationMode::RIGID : rt::LinearVertexModel<float>::EvaluationMode::STATIC;

    Eigen::Matrix<float, -1, -1> AtA(numTotalParameters, numTotalParameters);
    Eigen::VectorXf Atb(numTotalParameters);
    constexpr int numParts = 11;
    std::vector<Eigen::Matrix<float, -1, -1>> AtA_parts(numParts);
    for (size_t i = 0; i < AtA_parts.size(); ++i)
    {
        AtA_parts[i].resize(numTotalParameters, numTotalParameters);
    }
    std::vector<Eigen::VectorXf> Atb_parts(numParts);

    // LOG_INFO("start: {}", timer.Current()); timer.Restart();

    for (int iter = 0; iter < settings.iterations; ++iter)
    {
        // evaluate the vertices for the current parameters
        m_pcaRig.facePCA.EvaluateLinearized(params, evaluationMode, states[stateId].face);

        // LOG_INFO("eval: {}", timer.Current()); timer.Restart();
        // m_pcaRig.teethPCA.Evaluate(params, evaluationMode, states[stateId].teeth);
        m_pcaRigSubsampled.teethPCA.EvaluateLinearized(params, evaluationMode, states[stateId].teeth);
        // LOG_INFO("eval: {}", timer.Current()); timer.Restart();
        // m_pcaRig.eyeLeftPCA.Evaluate(params, evaluationMode, states[stateId].eyeLeft);
        // m_pcaRig.eyeRightPCA.Evaluate(params, evaluationMode, states[stateId].eyeRight);
        m_pcaRigSubsampled.eyeLeftTransformPCA.EvaluateVerticesAndJacobian(params, evaluationMode, states[stateId].eyeLeft);
        // LOG_INFO("eval: {}", timer.Current()); timer.Restart();
        m_pcaRigSubsampled.eyeRightTransformPCA.EvaluateVerticesAndJacobian(params, evaluationMode, states[stateId].eyeRight);
        // LOG_INFO("eval: {}", timer.Current()); timer.Restart();

        // update rigid transform jacobian
        // states[stateId].face.SetRotationModes(states[stateId].face.Base());
        // states[stateId].teeth.SetRotationModes(states[stateId].teeth.Base());
        // states[stateId].eyeLeft.SetRotationModes(states[stateId].eyeLeft.Base());
        // states[stateId].eyeRight.SetRotationModes(states[stateId].eyeRight.Base());
        // or instead keep rotation jacobian using the cross product matrix with the mean pca face. it is not correct, but should be sufficient as approximation

        // calculate the normals for the head mesh
        topology.CalculateVertexNormals(states[stateId].face.Base(), faceNormals, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/false);
        // LOG_INFO("normals: {}", timer.Current()); timer.Restart();

        // clear all vertex constraints
        for (auto& state : states)
        {
            state.cache.Clear();
        }

        TITAN_NAMESPACE::TaskFutures taskFutures;

        taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                // setup depth constraints (point2surface)
                if (vectorOfDepthmapConstraints.size() > 0)
                {
                    for (size_t i = 0; i < vectorOfDepthmapConstraints.size(); ++i)
                    {
                        vectorOfDepthmapConstraints[i].SetupDepthConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), faceNormals,
                                                                             states[stateId].cache.point2SurfaceVertexConstraints);
                    }
                }

                // setup icp constraints (point2surface and point2point)
                if (icpConstraints)
                {
                    icpConstraints->SetupICPConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), faceNormals,
                                                        states[stateId].cache.point2SurfaceVertexConstraints,
                                                        states[stateId].cache.point2PointVertexConstraints);
                }

                // evaluate point2surface vertex constraints
                if (states[stateId].cache.point2SurfaceVertexConstraints.NumberOfConstraints() > 0)
                {
                    auto jacobian =
                        states[stateId].cache.point2SurfaceVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                              states[stateId].cache.point2SurfaceVertexConstraintsJacobian,
                                                                                              m->globalThreadPool.get());
                    auto residual = states[stateId].cache.point2SurfaceVertexConstraints.Residual();
                    ParallelAtALower<float>(AtA_parts[0], jacobian, m->globalThreadPool.get());
                    Atb_parts[0] = -jacobian.transpose() * residual;
                }
                else
                {
                    Atb_parts[0].resize(0);
                }

                // evaluate point2point vertex constraints
                if (states[stateId].cache.point2PointVertexConstraints.NumberOfConstraints() > 0)
                {
                    auto jacobian =
                        states[stateId].cache.point2PointVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                            states[stateId].cache.point2PointVertexConstraintsJacobian);
                    auto residual = states[stateId].cache.point2PointVertexConstraints.Residual();
                    ParallelAtALower<float>(AtA_parts[1], jacobian, m->globalThreadPool.get());
                    Atb_parts[1] = -jacobian.transpose() * residual;
                }
                else
                {
                    Atb_parts[1].resize(0);
                }
            }));

        if (landmarkConstraints)
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupLandmarkConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), &m_subsampledFaceMeshLandmarks,
                                                                  LandmarkConstraintsBase<float>::MeshType::Face,
                                                                  states[stateId].cache.landmarksVertexConstraints);

                    if (states[stateId].cache.landmarksVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.landmarksVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                              states[stateId].cache.landmarksVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.landmarksVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[2], jacobian, m->globalThreadPool.get());
                        Atb_parts[2] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[2].resize(0);
                    }
                }));

            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupCurveConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), &m_subsampledFaceMeshLandmarks,
                                                               LandmarkConstraintsBase<float>::MeshType::Face, states[stateId].cache.curvesVertexConstraints);

                    if (states[stateId].cache.curvesVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.curvesVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                           states[stateId].cache.curvesVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.curvesVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[3], jacobian, m->globalThreadPool.get());
                        Atb_parts[3] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[3].resize(0);
                    }
                }));

            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupContourConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), faceNormals,
                                                                 &m_subsampledFaceMeshLandmarks, LandmarkConstraintsBase<float>::MeshType::Face,
                                                                 states[stateId].cache.contourVertexConstraints);
                    landmarkConstraints->SetupInnerLipConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), faceNormals,
                                                                  &m_subsampledFaceMeshLandmarks, states[stateId].cache.contourVertexConstraints);

                    if (states[stateId].cache.contourVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.contourVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                            states[stateId].cache.contourVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.contourVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[4], jacobian, m->globalThreadPool.get());
                        Atb_parts[4] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[4].resize(0);
                    }
                }));

            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupCurveConstraints(qrm.ToEigenTransform(), states[stateId].eyeLeft.Base(), &m_subsampledEyeLeftMeshLandmarks,
                                                               LandmarkConstraintsBase<float>::MeshType::EyeLeft,
                                                               states[stateId].cache.eyeLeftCurvesVertexConstraints);

                    if (states[stateId].cache.eyeLeftCurvesVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.eyeLeftCurvesVertexConstraints.EvaluateJacobian(states[stateId].eyeLeft.Modes(evaluationMode),
                                                                                                  states[stateId].cache.eyeLeftCurvesVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.eyeLeftCurvesVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[5], jacobian, m->globalThreadPool.get());
                        Atb_parts[5] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[5].resize(0);
                    }
                }));

            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupCurveConstraints(qrm.ToEigenTransform(), states[stateId].eyeRight.Base(), &m_subsampledEyeRightMeshLandmarks,
                                                               LandmarkConstraintsBase<float>::MeshType::EyeRight,
                                                               states[stateId].cache.eyeRightCurvesVertexConstraints);

                    if (states[stateId].cache.eyeRightCurvesVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.eyeRightCurvesVertexConstraints.EvaluateJacobian(states[stateId].eyeRight.Modes(evaluationMode),
                                                                                                   states[stateId].cache.eyeRightCurvesVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.eyeRightCurvesVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[6], jacobian, m->globalThreadPool.get());
                        Atb_parts[6] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[6].resize(0);
                    }
                }));

            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    landmarkConstraints->SetupLandmarkConstraints(qrm.ToEigenTransform(), states[stateId].teeth.Base(), &m_subsampledTeethMeshLandmarks,
                                                                  LandmarkConstraintsBase<float>::MeshType::Teeth,
                                                                  states[stateId].cache.teethVertexConstraints);

                    if (states[stateId].cache.teethVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.teethVertexConstraints.EvaluateJacobian(states[stateId].teeth.Modes(evaluationMode),
                                                                                          states[stateId].cache.teethVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.teethVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[7], jacobian, m->globalThreadPool.get());
                        Atb_parts[7] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[7].resize(0);
                    }
                }));
        }

        // setup flow constraints
        if (vectorOfFlowConstraints.size() > 0)
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    for (size_t i = 0; i < vectorOfFlowConstraints.size(); ++i)
                    {
                        vectorOfFlowConstraints[i]->SetupFlowConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(),
                                                                         states[stateId].cache.flowVertexConstraints);
                    }
                    if (states[stateId].cache.flowVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.flowVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                         states[stateId].cache.flowVertexConstraintsJacobian,
                                                                                         m->globalThreadPool.get());
                        auto residual = states[stateId].cache.flowVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[8], jacobian, m->globalThreadPool.get());
                        Atb_parts[8] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[8].resize(0);
                    }
                }));
        }

        // setup image constraints
        if (imageConstraints)
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    imageConstraints->SetupImageConstraints(qrm.ToEigenTransform(), states[stateId].face.Base(), states[stateId].cache.imageVertexConstraints);
                    if (states[stateId].cache.imageVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.imageVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                          states[stateId].cache.imageVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.imageVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[9], jacobian, m->globalThreadPool.get());
                        Atb_parts[9] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[9].resize(0);
                    }
                }));
        }

        // setup lip closure constraints
        if (lipClosureConstraints && lipClosureConstraints->ValidLipClosure() &&
            (lipClosureConstraints->Config()["lip closure weight"].template Value<float>() > 0))
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]() {
                    lipClosureConstraints->CalculateLipClosureData(states[stateId].face.Base(), faceNormals, qrm.ToEigenTransform(), /*useLandmarks=*/true,
                                                                   Eigen::Transform<float, 3, Eigen::Affine>::Identity(), m->globalThreadPool.get());
                    lipClosureConstraints->EvaluateLipClosure(states[stateId].face.Base(), states[stateId].cache.lipClosureVertexConstraints);
                    if (states[stateId].cache.lipClosureVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian =
                            states[stateId].cache.lipClosureVertexConstraints.EvaluateJacobian(states[stateId].face.Modes(evaluationMode),
                                                                                               states[stateId].cache.lipClosureVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.lipClosureVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[10], jacobian, m->globalThreadPool.get());
                        Atb_parts[10] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[10].resize(0);
                    }
                }));
        }

        taskFutures.Wait();
        // LOG_INFO("wait: {}", timer.Current()); timer.Restart();

        // sum all valid AtA and Atb parts
        bool firstTerm = true;
        for (size_t i = 0; i < Atb_parts.size(); ++i)
        {
            if (Atb_parts[i].size() > 0)
            {
                if (firstTerm)
                {
                    AtA.template triangularView<Eigen::Lower>() = AtA_parts[i];
                    Atb.noalias() = Atb_parts[i];
                    firstTerm = false;
                }
                else
                {
                    AtA.template triangularView<Eigen::Lower>() += AtA_parts[i];
                    Atb.noalias() += Atb_parts[i];
                }
            }
        }
        // LOG_INFO("sum: {}", timer.Current()); timer.Restart();

        // add regularization
        for (int k = 0; k < numPcaParameters; ++k)
        {
            AtA(k, k) += settings.pcaRegularization;
            Atb[k] -= settings.pcaRegularization * params[k];
        }

        // add temporal constraints on pca coefficients
        if ((pcaCoeffsPrevFrames.size() > 0) && (settings.pcaVelocityRegularization > 0))
        {
            for (int k = 0; k < numPcaParameters; ++k)
            {
                AtA(k, k) += settings.pcaVelocityRegularization;
                Atb[k] -= settings.pcaVelocityRegularization * (params[k] - pcaCoeffsPrevFrames[0][k]);
            }
        }
        if ((pcaCoeffsPrevFrames.size() > 1) && (settings.pcaAccelerationRegularization > 0))
        {
            for (int k = 0; k < numPcaParameters; ++k)
            {
                AtA(k, k) += settings.pcaAccelerationRegularization;
                Atb[k] -= settings.pcaAccelerationRegularization * (params[k] - 2 * pcaCoeffsPrevFrames[0][k] + pcaCoeffsPrevFrames[1][k]);
            }
        }
        // LOG_INFO("iter regularization: {}", timer.Current()); timer.Restart();

        // solve
        const Eigen::VectorX<float> dx = AtA.template selfadjointView<Eigen::Lower>().llt().solve(Atb);

        params.segment(0, numPcaParameters) += dx.segment(0, numPcaParameters);

        if (settings.withRigid)
        {
            // extract rigid motion from params
            qrm.t += qrm.q._transformVector(dx.tail(3));
            qrm.q = (qrm.q * Eigen::Quaternion<float>(1.0f, dx[numPcaParameters + 0], dx[numPcaParameters + 1], dx[numPcaParameters + 2])).normalized();
            // set rigid motion explicitly to zero
            params.tail(6).setZero();
        }
        // LOG_INFO("iter solve: {}", timer.Current()); timer.Restart();
    }

    rigidMotion.SetMatrix(qrm.ToEigenTransform().matrix());
    pcaCoeffs = params.segment(0, numPcaParameters);
    // LOG_INFO("total: {}", timer.Current()); timer.Restart();
}

void PCAFaceFitting::FitPCADataNeck(const Mesh<float>& topology,
                                     std::vector<DepthmapConstraints>& vectorOfDepthmapConstraints,
                                     ICPConstraints<float>* icpConstraints,
                                     const std::vector<FlowConstraints<float>*>& vectorOfFlowConstraints,
                                     ImageConstraints<float>* imageConstraints,
                                     Affine<float, 3, 3>& rigidMotion,
                                     Eigen::Matrix3Xf faceVertices,
                                     Eigen::VectorXf& pcaCoeffsNeck,
                                     const std::vector<Eigen::VectorXf>& pcaCoeffsPrevFrames,
                                     const Settings& settings,
                                     std::vector<State>& states) const
{
    // Timer timer;

    if (states.empty())
    {
        states.emplace_back(State());
    }
    const int stateId = 0;

    const int numPcaParameters = int(pcaCoeffsNeck.size());

    Eigen::VectorXf params(numPcaParameters);
    params.segment(0, numPcaParameters) = pcaCoeffsNeck;

    QRigidMotion<float> qrm(rigidMotion.Matrix());

    Eigen::Matrix<float, 3, -1> faceNormals;

    const auto evaluationMode = rt::LinearVertexModel<float>::EvaluationMode::STATIC;

    Eigen::Matrix3Xf faceWithNeck = faceVertices;

    Eigen::Matrix<float, -1, -1> AtA(numPcaParameters, numPcaParameters);
    Eigen::VectorXf Atb(numPcaParameters);
    constexpr int numParts = 11;
    std::vector<Eigen::Matrix<float, -1, -1>> AtA_parts(numParts);
    for (size_t i = 0; i < AtA_parts.size(); ++i)
    {
        AtA_parts[i].resize(numPcaParameters, numPcaParameters);
    }
    std::vector<Eigen::VectorXf> Atb_parts(numParts);

    // LOG_INFO("start: {}", timer.Current()); timer.Restart();

    for (int iter = 0; iter < settings.iterations; ++iter)
    {
        // evaluate the vertices for the current parameters
        m_pcaRig.neckPCA.EvaluateLinearized(params, evaluationMode, states[stateId].neck);
        faceWithNeck = faceVertices + states[stateId].neck.Base();

        // calculate the normals for the head mesh
        topology.CalculateVertexNormals(faceWithNeck, faceNormals, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/false);
        // LOG_INFO("normals: {}", timer.Current()); timer.Restart();

        // clear all vertex constraints
        for (auto& state : states)
        {
            state.cache.Clear();
        }

        TITAN_NAMESPACE::TaskFutures taskFutures;

        taskFutures.Add(m->globalThreadPool->AddTask([&]()
            {
                // setup depth constraints (point2surface)
                if (vectorOfDepthmapConstraints.size() > 0)
                {
                    for (size_t i = 0; i < vectorOfDepthmapConstraints.size(); ++i)
                    {
                        vectorOfDepthmapConstraints[i].SetupDepthConstraints(qrm.ToEigenTransform(), faceWithNeck, faceNormals,
                            states[stateId].cache.point2SurfaceVertexConstraints);
                    }
                }

                // setup icp constraints (point2surface and point2point)
                if (icpConstraints)
                {
                    icpConstraints->SetupICPConstraints(qrm.ToEigenTransform(), faceWithNeck, faceNormals,
                        states[stateId].cache.point2SurfaceVertexConstraints,
                        states[stateId].cache.point2PointVertexConstraints);
                }

                // evaluate point2surface vertex constraints
                if (states[stateId].cache.point2SurfaceVertexConstraints.NumberOfConstraints() > 0)
                {
                    auto jacobian = states[stateId].cache.point2SurfaceVertexConstraints.EvaluateJacobian(states[stateId].neck.Modes(evaluationMode),
                        states[stateId].cache.point2SurfaceVertexConstraintsJacobian,
                        m->globalThreadPool.get());
                    auto residual = states[stateId].cache.point2SurfaceVertexConstraints.Residual();
                    ParallelAtALower<float>(AtA_parts[0], jacobian, m->globalThreadPool.get());
                    Atb_parts[0] = -jacobian.transpose() * residual;
                }
                else
                {
                    Atb_parts[0].resize(0);
                }

                // evaluate point2point vertex constraints
                if (states[stateId].cache.point2PointVertexConstraints.NumberOfConstraints() > 0)
                {
                    auto jacobian = states[stateId].cache.point2PointVertexConstraints.EvaluateJacobian(states[stateId].neck.Modes(evaluationMode),
                        states[stateId].cache.point2PointVertexConstraintsJacobian);
                    auto residual = states[stateId].cache.point2PointVertexConstraints.Residual();
                    ParallelAtALower<float>(AtA_parts[1], jacobian, m->globalThreadPool.get());
                    Atb_parts[1] = -jacobian.transpose() * residual;
                }
                else
                {
                    Atb_parts[1].resize(0);
                }
            }));

        // setup flow constraints
        if (vectorOfFlowConstraints.size() > 0)
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]()
                {
                    for (size_t i = 0; i < vectorOfFlowConstraints.size(); ++i)
                    {
                        vectorOfFlowConstraints[i]->SetupFlowConstraints(qrm.ToEigenTransform(), faceWithNeck,
                            states[stateId].cache.flowVertexConstraints);
                    }
                    if (states[stateId].cache.flowVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian = states[stateId].cache.flowVertexConstraints.EvaluateJacobian(states[stateId].neck.Modes(evaluationMode),
                            states[stateId].cache.flowVertexConstraintsJacobian,
                            m->globalThreadPool.get());
                        auto residual = states[stateId].cache.flowVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[8], jacobian, m->globalThreadPool.get());
                        Atb_parts[8] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[8].resize(0);
                    }
                }));
        }

        // setup image constraints
        if (imageConstraints)
        {
            taskFutures.Add(m->globalThreadPool->AddTask([&]()
                {
                    imageConstraints->SetupImageConstraints(qrm.ToEigenTransform(), faceWithNeck, states[stateId].cache.imageVertexConstraints);
                    if (states[stateId].cache.imageVertexConstraints.NumberOfConstraints() > 0)
                    {
                        auto jacobian = states[stateId].cache.imageVertexConstraints.EvaluateJacobian(states[stateId].neck.Modes(evaluationMode),
                            states[stateId].cache.imageVertexConstraintsJacobian);
                        auto residual = states[stateId].cache.imageVertexConstraints.Residual();
                        ParallelAtALower<float>(AtA_parts[9], jacobian, m->globalThreadPool.get());
                        Atb_parts[9] = -jacobian.transpose() * residual;
                    }
                    else
                    {
                        Atb_parts[9].resize(0);
                    }
                }));
        }

        taskFutures.Wait();
        // LOG_INFO("wait: {}", timer.Current()); timer.Restart();

        // sum all valid AtA and Atb parts
        bool firstTerm = true;
        for (size_t i = 0; i < Atb_parts.size(); ++i)
        {
            if (Atb_parts[i].size() > 0)
            {
                if (firstTerm)
                {
                    AtA.template triangularView<Eigen::Lower>() = AtA_parts[i];
                    Atb.noalias() = Atb_parts[i];
                    firstTerm = false;
                }
                else
                {
                    AtA.template triangularView<Eigen::Lower>() += AtA_parts[i];
                    Atb.noalias() += Atb_parts[i];
                }
            }
        }
        // LOG_INFO("sum: {}", timer.Current()); timer.Restart();

        // add regularization
        for (int k = 0; k < numPcaParameters; ++k)
        {
            AtA(k, k) += settings.pcaRegularization;
            Atb[k] -= settings.pcaRegularization * params[k];
        }

        // add temporal constraints on pca coefficients
        if ((pcaCoeffsPrevFrames.size() > 0) && (settings.pcaVelocityRegularization > 0))
        {
            for (int k = 0; k < numPcaParameters; ++k)
            {
                AtA(k, k) += settings.pcaVelocityRegularization;
                Atb[k] -= settings.pcaVelocityRegularization * (params[k] - pcaCoeffsPrevFrames[0][k]);
            }
        }
        if ((pcaCoeffsPrevFrames.size() > 1) && (settings.pcaAccelerationRegularization > 0))
        {
            for (int k = 0; k < numPcaParameters; ++k)
            {
                AtA(k, k) += settings.pcaAccelerationRegularization;
                Atb[k] -= settings.pcaAccelerationRegularization * (params[k] - 2 * pcaCoeffsPrevFrames[0][k] + pcaCoeffsPrevFrames[1][k]);
            }
        }
        // LOG_INFO("iter regularization: {}", timer.Current()); timer.Restart();

        // solve
        const Eigen::VectorX<float> dx = AtA.template selfadjointView<Eigen::Lower>().llt().solve(Atb);

        params.segment(0, numPcaParameters) += dx.segment(0, numPcaParameters);
        // LOG_INFO("iter solve: {}", timer.Current()); timer.Restart();
    }

    pcaCoeffsNeck = params.segment(0, numPcaParameters);
    // LOG_INFO("total: {}", timer.Current()); timer.Restart();
}

PCAFaceFitting::PCAFaceFitting()
    : m(new Private)
{}

PCAFaceFitting::~PCAFaceFitting() = default;

bool PCAFaceFitting::LoadPCARig(const std::string& pcaFilename)
{
    if (!m_pcaRig.LoadFromDNA(pcaFilename))
    {
        LOG_ERROR("failed to read pca model from {}", pcaFilename);
        return false;
    }

    LOG_INFO("number of pca coeffs: {}", m_pcaRig.NumCoeffs());
    // move the midpoint of the eyes to the origin so that rotation is optimal for optimization
    const Eigen::Vector3f pcaRigEyesMidPoint = m_pcaRig.EyesMidpoint();
    m_pcaRig.Translate(-pcaRigEyesMidPoint);

    UpdateSubsampled();

    return true;
}

bool PCAFaceFitting::LoadPCARig(dna::Reader* dnaStream)
{
    if (!m_pcaRig.LoadFromDNA(dnaStream))
    {
        LOG_ERROR("could not load PCA rig");
        return false;
    }

    LOG_INFO("number of pca coeffs: {}", m_pcaRig.NumCoeffs());
    if (m_pcaRig.NumCoeffsNeck() > 0) {
        LOG_INFO("number of neck pca coeffs: {}", m_pcaRig.NumCoeffsNeck());
    }
    // move the midpoint of the eyes to the origin so that rotation is optimal for optimization
    const Eigen::Vector3f pcaRigEyesMidPoint = m_pcaRig.EyesMidpoint();
    m_pcaRig.Translate(-pcaRigEyesMidPoint);

    UpdateSubsampled();

    return true;
}

bool PCAFaceFitting::SavePCARig(dna::Writer* dnaStream) const
{
    // translate the PCA rig back to its original position before saving it
    auto pcaRigCopy = m_pcaRig;
    pcaRigCopy.Translate(-pcaRigCopy.offset);
    pcaRigCopy.SaveAsDNA(dnaStream);

    return true;
}

void PCAFaceFitting::SavePCARigAsNpy(const std::string& filename) const
{
    auto pcaRigCopy = m_pcaRig;
    pcaRigCopy.Translate(-m_pcaRig.offset);
    pcaRigCopy.SaveAsNpy(filename);
}

void PCAFaceFitting::UpdateSubsampled()
{
    m_pcaRigSubsampled = m_pcaRig;

    LoadFaceMeshLandmarks(m_faceMeshLandmarks);
    LoadEyeLeftMeshLandmarks(m_eyeLeftMeshLandmarks);
    LoadEyeRightMeshLandmarks(m_eyeRightMeshLandmarks);
    LoadTeethMeshLandmarks(m_teethMeshLandmarks);
}

namespace
{

auto setToVec = [](const std::set<int>& set) {
        return std::vector<int>(set.begin(), set.end());
    };

auto invertMap = [](const std::vector<int>& vec) {
        std::map<int, int> map;
        for (int newId = 0; newId < int(vec.size()); ++newId)
        {
            map[vec[newId]] = newId;
        }
        return map;
    };

} // namespace

void PCAFaceFitting::LoadFaceMeshLandmarks(const MeshLandmarks<float>& faceMeshLandmarks)
{
    m_faceMeshLandmarks = faceMeshLandmarks;
    m_subsampledFaceMeshLandmarks = faceMeshLandmarks;
}

void PCAFaceFitting::LoadEyeLeftMeshLandmarks(const MeshLandmarks<float>& eyeLeftMeshLandmarks)
{
    m_eyeLeftMeshLandmarks = eyeLeftMeshLandmarks;
    const auto eyeLeftSubsampledToFullMap = setToVec(eyeLeftMeshLandmarks.GetAllVertexIndices());
    m_subsampledEyeLeftMeshLandmarks = eyeLeftMeshLandmarks;
    m_subsampledEyeLeftMeshLandmarks.Remap(invertMap(eyeLeftSubsampledToFullMap));
    m_pcaRigSubsampled.eyeLeftTransformPCA = m_pcaRig.eyeLeftTransformPCA;
    m_pcaRigSubsampled.eyeLeftTransformPCA.Resample(eyeLeftSubsampledToFullMap);
    // LOG_INFO("reduce eye left size from {} to {}", m_pcaRig.eyeLeftTransformPCA.eyeBody.baseVertices.cols(),
    // m_pcaRigSubsampled.eyeLeftTransformPCA.eyeBody.baseVertices.cols());
}

void PCAFaceFitting::LoadEyeRightMeshLandmarks(const MeshLandmarks<float>& eyeRightMeshLandmarks)
{
    m_eyeRightMeshLandmarks = eyeRightMeshLandmarks;
    const auto eyeRightSubsampledToFullMap = setToVec(eyeRightMeshLandmarks.GetAllVertexIndices());
    m_subsampledEyeRightMeshLandmarks = eyeRightMeshLandmarks;
    m_subsampledEyeRightMeshLandmarks.Remap(invertMap(eyeRightSubsampledToFullMap));
    m_pcaRigSubsampled.eyeRightTransformPCA = m_pcaRig.eyeRightTransformPCA;
    m_pcaRigSubsampled.eyeRightTransformPCA.Resample(eyeRightSubsampledToFullMap);
    // LOG_INFO("reduce eye right size from {} to {}", m_pcaRig.eyeRightTransformPCA.eyeBody.baseVertices.cols(),
    // m_pcaRigSubsampled.eyeRightTransformPCA.eyeBody.baseVertices.cols());
}

void PCAFaceFitting::LoadTeethMeshLandmarks(const MeshLandmarks<float>& teethMeshLandmarks)
{
    m_teethMeshLandmarks = teethMeshLandmarks;
    const auto teethSubsampledToFullMap = setToVec(teethMeshLandmarks.GetAllVertexIndices());
    m_subsampledTeethMeshLandmarks = teethMeshLandmarks;
    m_subsampledTeethMeshLandmarks.Remap(invertMap(teethSubsampledToFullMap));
    m_pcaRigSubsampled.teethPCA = m_pcaRig.teethPCA;
    m_pcaRigSubsampled.teethPCA.Resample(teethSubsampledToFullMap);
    // LOG_INFO("reduce teeth size from {} to {}", m_pcaRig.teethPCA.NumVertices(), m_pcaRigSubsampled.teethPCA.NumVertices());
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
