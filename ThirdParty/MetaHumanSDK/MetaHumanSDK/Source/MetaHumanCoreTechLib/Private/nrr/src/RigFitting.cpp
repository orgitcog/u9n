// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/RigFitting.h>
#include <nls/Cost.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nrr/deformation_models/DeformationModelRigidScale.h>

#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/math/PCG.h>
#include <carbon/utils/Timer.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void NeutralPoseFittingParams<T>::SetFromConfiguration(const Configuration& config)
{
    if (config.HasParameter("p2p"))
    {
        p2pWeight = config["p2p"].template Value<T>();
    }
    else
    {
        LOG_WARNING("No p2p parameter in config {}", config.Name());
    }
    if (config.HasParameter("regularization"))
    {
        modelRegularization = config["regularization"].template Value<T>();
    }
    else
    {
        LOG_WARNING("No regularization parameter in config {}", config.Name());
    }
    if (config.HasParameter("smoothness"))
    {
        patchSmoothness = config["smoothness"].template Value<T>();
    }
    else
    {
        LOG_WARNING("No smoothness parameter in config {}", config.Name());
    }
    if (config.HasParameter("optimizePose"))
    {
        modelFitOptimizeRigid = config["optimizePose"].template Value<bool>();
    }
    else
    {
        LOG_WARNING("No optimizePose parameter in config {}", config.Name());
    }
    if (config.HasParameter("optimizeScale"))
    {
        modelFitOptimizeScale = config["optimizeScale"].template Value<bool>();
    }
    else
    {
        LOG_WARNING("No optimizeScale parameter in config {}", config.Name());
    }
    if (config.HasParameter("rigidFitOptimizeRotation"))
    {
        rigidFitOptimizeRotation = config["rigidFitOptimizeRotation"].template Value<bool>();
    }
    else
    {
        LOG_WARNING("No rigidFitOptimizeRotation parameter in config {}", config.Name());
    }
    if (config.HasParameter("rigidFitOptimizeScale"))
    {
        rigidFitOptimizeScale = config["rigidFitOptimizeScale"].template Value<bool>();
    }
    else
    {
        LOG_WARNING("No rigidFitOptimizeScale parameter in config {}", config.Name());
    }
    if (config.HasParameter("rigidFitOptimizeTranslation"))
    {
        rigidFitOptimizeTranslation = config["rigidFitOptimizeTranslation"].template Value<bool>();
    }
    else
    {
        LOG_WARNING("No rigidFitOptimizeTranslation parameter in config {}", config.Name());
    }
    if (config.HasParameter("fixedRegion"))
    {
        fixedRegion = config["fixedRegion"].template Value<int>();
    }
    else
    {
        LOG_WARNING("No fixedRegion parameter in config {}", config.Name());
    }
}

template <class T>
Configuration NeutralPoseFittingParams<T>::ToConfiguration() const
{
    Configuration config = { std::string("Rig Fitting Configuration"), {
        { "p2p", ConfigurationParameter(p2pWeight) },
        { "optimizePose", ConfigurationParameter(modelFitOptimizeRigid) },
        { "optimizeScale", ConfigurationParameter(modelFitOptimizeScale) },
        { "smoothness", ConfigurationParameter(patchSmoothness) },
        { "regularization", ConfigurationParameter(modelRegularization) },
        { "fixedRegion", ConfigurationParameter(fixedRegion) },
        { "rigidFitOptimizeScale", ConfigurationParameter(rigidFitOptimizeScale) },
        { "rigidFitOptimizeTranslation", ConfigurationParameter(rigidFitOptimizeTranslation) },
        { "rigidFitOptimizeRotation", ConfigurationParameter(rigidFitOptimizeRotation) }
    } };

    return config;
}

// explicitly instantiate the NeutralPoseFittingParams classes
template struct NeutralPoseFittingParams<float>;
template struct NeutralPoseFittingParams<double>;

template <class T>
std::pair<T, Affine<T, 3, 3>> NeutralPoseFittingOptimization<T>::RegisterPose(const Eigen::Matrix<T, 3, -1>& targetData,
                                                                              const std::shared_ptr<IdentityBlendModel<T>>& model,
                                                                              const NeutralPoseFittingParams<T>& params,
                                                                              const Eigen::VectorX<T>& mask,
                                                                              Eigen::VectorX<T>& result)
{
    Eigen::VectorX<T> scaleStart = Eigen::VectorX<T>::Ones(1);
    VectorVariable<T> varScale = VectorVariable<T>(scaleStart);
    VectorVariable<T> varModelParameters = VectorVariable<T>(model->DefaultParameters());

    const Eigen::Matrix<T, 3, -1> initialVertices = model->Evaluate(model->DefaultParameters());
    const Eigen::Vector3<T> centerOfGravity = initialVertices.rowwise().mean();
    const DiffDataAffine<T, 3, 3> diffToCenterOfGravity(Affine<T, 3, 3>::FromTranslation(-centerOfGravity));
    const DiffDataAffine<T, 3, 3> diffFromCenterOfGravity(Affine<T, 3, 3>::FromTranslation(centerOfGravity));

    VertexWeights<T> inputMaskWeights(mask);
    Eigen::VectorXi regionIds = model->RegionVertexIds(0);
    Eigen::VectorX<T> regionWeights = model->RegionWeights(0);

    Eigen::VectorX<T> combinedWeights = Eigen::VectorX<T>::Zero(initialVertices.cols());
    T weightThreshold = 0.5;
    for (int i = 0; i < (int)regionIds.size(); ++i)
    {
        int vtxId = regionIds[i];
        T weight = regionWeights[i];
        T inputMaskWeight = inputMaskWeights.Weights()[vtxId];

        T combinedWeight = weight * inputMaskWeight;
        if (combinedWeight > weightThreshold)
        {
            combinedWeights[vtxId] = combinedWeight;
        }
    }

    AffineVariable<QuaternionVariable<T>> transformVariable;
    transformVariable.SetAffine(Affine<T, 3, 3>());
    transformVariable.MakeConstant(!params.rigidFitOptimizeRotation, !params.rigidFitOptimizeTranslation);

    if (!params.rigidFitOptimizeScale)
    {
        varScale.MakeIndividualIndicesConstant(std::vector<int>{0});
    }

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;
            DiffData<T> modelParams = varModelParameters.Evaluate(context);
            DiffDataMatrix<T, 1, 1> scaleMatrix = varScale.Evaluate(context);
            DiffDataAffine<T, 3, 3> diffToTargetTransform = transformVariable.EvaluateAffine(context);

            // evaluated vertices in model space
            DiffDataMatrix<T, 3, -1> stabilizedVertices = model->Evaluate(modelParams);

            // centered vertices
            DiffDataMatrix<T, 3, -1> centeredVertices = diffToCenterOfGravity.Transform(stabilizedVertices);

            // apply scale in center
            DiffDataMatrix<T, 1, -1> flattenedVertices(1, static_cast<int>(stabilizedVertices.Cols() * stabilizedVertices.Rows()), DiffData<T>(std::move(centeredVertices)));
            DiffDataMatrix<T, 1, -1> scaledAndCenteredVerticesFlattenedVertices = MatrixMultiplyFunction<T>::DenseMatrixMatrixMultiply(scaleMatrix, flattenedVertices);
            DiffDataMatrix<T, 3, -1> scaledAndCenteredVertices(3, static_cast<int>(stabilizedVertices.Cols()), std::move(scaledAndCenteredVerticesFlattenedVertices));

            // transform
            DiffDataMatrix<T, 3, -1> scaledAndTransformedVertices = diffToTargetTransform.Transform(scaledAndCenteredVertices);

            // move back to model space
            DiffDataMatrix<T, 3, -1> verticesToEvaluate = diffFromCenterOfGravity.Transform(scaledAndTransformedVertices);

            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(verticesToEvaluate,
                                                                                targetData,
                                                                                combinedWeights,
                                                                                T(1));
            // model regularization
            DiffData<T> modelRegularization = model->EvaluateRegularization(modelParams);
            cost.Add(std::move(residual), params.p2pWeight);
            cost.Add(std::move(modelRegularization), params.modelRegularization);

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, params.numIterations))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
    }
    else
    {
        LOG_ERROR("could not solve optimization problem");
    }

    T scale = varScale.Value()[0];
    Affine<T, 3, 3> affine = Affine<T, 3, 3>::FromTranslation(centerOfGravity) * transformVariable.Affine() * Affine<T, 3, 3>::FromTranslation(scale * (-centerOfGravity));
    affine.SetTranslation(affine.Translation() * (T(1) / scale));

    result = varModelParameters.Value();
    return std::make_pair(scale, affine);
}

template <class T>
Affine<T, 3, 3> NeutralPoseFittingOptimization<T>::RegisterPose(const Eigen::Matrix<T, 3, -1>& targetData,
                                                                const Eigen::VectorXi& targetToModelDataMapping,
                                                                const Affine<T, 3, 3> toTargetTransform,
                                                                const std::shared_ptr<PatchBlendModel<T>>& model,
                                                                typename PatchBlendModel<T>::OptimizationState& modelState,
                                                                const NeutralPoseFittingParams<T>& params,
                                                                const Eigen::VectorX<T>& mask)
{
    CARBON_ASSERT((int)mask.size() == model->NumVertices(), "Input weights should be of same dimensionality as target vertices.");
    CARBON_ASSERT(targetToModelDataMapping.size() == targetData.cols(), "Target mapping should be of the same dimensionality as target vertices.");

    modelState.SetFixedPatch(params.fixedRegion);
    if (params.modelFitOptimizeRigid && params.fixedRegion < 0)
    {
        LOG_WARNING("optimizing global rigid while not keeping a region fixed leads to unstable results - setting region 10 to fixed");
        modelState.SetFixedPatch(10);
    }

    modelState.SetOptimizeScale(params.modelFitOptimizeScale);

#if 1
    // Timer timer;
    const Eigen::Vector3<T> targetCenterOfGravity = targetData.rowwise().mean();
    const Eigen::Matrix<T, 3, -1> centeredTargetData = targetData.colwise() - targetCenterOfGravity;
    Eigen::Transform<T, 3, Eigen::Affine> centeredTargetToModelTransform((toTargetTransform.Inverse() * Affine<T, 3, 3>::FromTranslation(targetCenterOfGravity)).Matrix());
    Eigen::Quaternion<T> currTargetRotation(centeredTargetToModelTransform.linear());
    Eigen::Vector3<T> currTargetTranslation = centeredTargetToModelTransform.translation();

    {
        const int numParameters = modelState.NumParameters() + 6;
        Eigen::Matrix<T, -1, -1> AtA(numParameters, numParameters);
        Eigen::VectorX<T> Atb(numParameters);
        const int numThreads = 8;
        std::vector<Eigen::Matrix<T, -1, -1>> AtAvec(numThreads, AtA);
        std::vector<Eigen::VectorX<T>> Atbvec(numThreads, Atb);
        rt::LinearVertexModel<T> rigidModel;
        Eigen::Matrix<T, 3, -1> currCenteredTargetData = centeredTargetData;
        // LOG_INFO("time to solve 0: {}", timer.Current()); timer.Restart();
        for (int iter = 0; iter < params.numIterations; ++iter)
        {
            AtA.setZero();
            Atb.setZero();
            const Eigen::Matrix<T, 3, -1> currVertices = model->DeformedVertices(modelState, /*withModes=*/true);
            // LOG_INFO("time to solve 1: {}", timer.Current()); timer.Restart();
            currCenteredTargetData = currTargetRotation.toRotationMatrix() * centeredTargetData;
            rigidModel.Create(currCenteredTargetData, Eigen::Matrix<T, -1, -1, Eigen::RowMajor>(currCenteredTargetData.size(), 0));
            // only disable scaling, or rotation, translation, and scaling
            rigidModel.MutableModes().block(0, params.modelFitOptimizeRigid ? 6 : 0, rigidModel.NumVertices() * 3, params.modelFitOptimizeRigid ? 1 : 7).setZero();
            currCenteredTargetData.colwise() += currTargetTranslation;
            // LOG_INFO("time to solve 2: {}", timer.Current()); timer.Restart();
            auto processVertices = [&](int t, int start, int end) {
                AtAvec[t].setZero();
                Atbvec[t].setZero();
                for (int j = start; j < end; ++j)
                {
                    const int vID = targetToModelDataMapping[j];
                    // p2p_cost = params.p2pWeight * || w * (vertex + modes * dx - rigid * target) ||
                    const Eigen::Vector3<T> diff = currVertices.col(vID) - currCenteredTargetData.col(j);

                    const T w = mask[vID];
                    const auto rigidModes = rigidModel.MutableModes().block(3 * j, 0, 3, 6);
                    Atbvec[t].segment(numParameters - 6, 6).noalias() += (w * w * params.p2pWeight) * rigidModes.transpose() * (diff);
                    AtAvec[t].block(numParameters - 6, numParameters - 6, 6, 6).noalias() += (w * w * params.p2pWeight) * rigidModes.transpose() * rigidModes;
                    for (size_t i1 = 0; i1 < model->BlendMatrix()[vID].size(); ++i1)
                    {
                        const auto [r1, r1vID, w1] = model->BlendMatrix()[vID][i1];
                        const int r1offset = modelState.PatchVariableOffset(r1);
                        const auto& r1modes = modelState.TransformedRegionModels()[r1].Modes(rt::LinearVertexModel<T>::EvaluationMode::RIGID_SCALE);
                        const int r1length = (int)r1modes.cols();
                        const auto r1vmodes = r1modes.block(3 * r1vID, 0, 3, r1length);
                        Atbvec[t].segment(r1offset, r1length).noalias() += (w * w * params.p2pWeight * w1) * r1vmodes.transpose() * (-diff);
                        AtAvec[t].block(numParameters - 6, r1offset, 6, r1length).noalias() -= (w * w * params.p2pWeight * w1) * rigidModes.transpose() * r1vmodes;

                        for (size_t i2 = 0; i2 <= i1; ++i2)
                        {
                            const auto [r2, r2vID, w2] = model->BlendMatrix()[vID][i2];
                            const int r2offset = modelState.PatchVariableOffset(r2);
                            const auto& r2modes = modelState.TransformedRegionModels()[r2].Modes(rt::LinearVertexModel<T>::EvaluationMode::RIGID_SCALE);
                            const int r2length = (int)r2modes.cols();
                            const auto r2vmodes = r2modes.block(3 * r2vID, 0, 3, r2length);
                            // AtAvec[t].block(r1offset, r2offset, r1length, r2length).noalias() += (params.p2pWeight * w * w * w1 * w2) * (r1vmodes.transpose() * r2vmodes);

                            if (i2 < i1)
                            {
                                const Eigen::Vector3<T> v1 = modelState.TransformedRegionModels()[r1].Base().col(r1vID);
                                const Eigen::Vector3<T> v2 = modelState.TransformedRegionModels()[r2].Base().col(r2vID);
                                const Eigen::Vector3<T> vDiff = params.patchSmoothness * (v1 - v2);
                                Atbvec[t].segment(r1offset, r1length).noalias() += - r1vmodes.transpose() * vDiff;
                                Atbvec[t].segment(r2offset, r2length).noalias() +=   r2vmodes.transpose() * vDiff;
                                AtAvec[t].block(r1offset, r1offset, r1length, r1length).noalias() += (params.patchSmoothness) * (r1vmodes.transpose() * r1vmodes);
                                AtAvec[t].block(r2offset, r2offset, r2length, r2length).noalias() += (params.patchSmoothness) * (r2vmodes.transpose() * r2vmodes);
                                AtAvec[t].block(r1offset, r2offset, r1length, r2length).noalias() += (params.p2pWeight * w * w * w1 * w2 - params.patchSmoothness) * (r1vmodes.transpose() * r2vmodes);
                            }
                            else
                            {
                                AtAvec[t].block(r1offset, r2offset, r1length, r2length).noalias() += (params.p2pWeight * w * w * w1 * w2) * (r1vmodes.transpose() * r2vmodes);
                            }
                        }
                    }
                }
            };
            TITAN_NAMESPACE::TaskFutures futures;
            const int numTasks = (int)targetToModelDataMapping.size();
            const int tasksPerThread = numTasks / numThreads;
            const int additionalTasks = numTasks - tasksPerThread * numThreads;
            int startIndex = 0;
            for (int t = 0; t < numThreads; ++t)
            {
                const int numTasksForThisThread = (t < additionalTasks) ? (tasksPerThread + 1) : tasksPerThread;
                futures.Add(TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(true)->AddTask([processVertices, t, startIndex, numTasksForThisThread]() { processVertices(t, startIndex, startIndex + numTasksForThisThread); }));
                startIndex += numTasksForThisThread;
            }
            futures.Wait();
            AtA = AtAvec[0];
            Atb = Atbvec[0];
            for (int t = 1; t < numThreads; ++t)
            {
                AtA.noalias() += AtAvec[t];
                Atb.noalias() += Atbvec[t];
            }
            for (int regionIndex = 0; regionIndex < model->NumPatches(); ++regionIndex)
            {
                const int offset = modelState.PatchVariableOffset(regionIndex);
                const int length = modelState.NumPcaModesForPatch(regionIndex);
                for (int k = 0; k < length; ++k)
                {
                    AtA(offset + k, offset + k) += params.modelRegularization;
                    Atb(offset + k) += -params.modelRegularization * modelState.PatchPcaWeights(regionIndex)[k];
                }
            }
            for (int i = 0; i < (int)AtA.rows(); ++i)
            {
                AtA(i, i) += T(1e-4);
            }
            // LOG_INFO("time to solve 3: {}", timer.Current()); timer.Restart();
            Eigen::VectorX<T> dx = AtA.template selfadjointView<Eigen::Lower>().llt().solve(Atb);
            // LOG_INFO("time to solve 4: {}", timer.Current()); timer.Restart();
            // Eigen::Matrix<T, -1, -1, Eigen::RowMajor> AtArm = AtA;
            // LOG_INFO("time to solve 4a: {}", timer.Current()); timer.Restart();
            // PCG<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> pcg;
            // Eigen::VectorX<T> dx = pcg.Solve(50, AtArm, Atb, Eigen::VectorX<T>::Zero(Atb.size()));
            // LOG_INFO("time to solve 4b: {}", timer.Current()); timer.Restart();
            for (int regionIndex = 0; regionIndex < modelState.NumPatches(); ++regionIndex)
            {
                const int roffset = modelState.PatchVariableOffset(regionIndex);
                const int rpcalength = modelState.NumPcaModesForPatch(regionIndex);
                modelState.SetPatchPcaWeights(regionIndex, dx.segment(roffset, rpcalength) + modelState.PatchPcaWeights(regionIndex));
                modelState.SetPatchRotation(regionIndex, dx.segment(roffset + rpcalength, 3) + modelState.PatchRotation(regionIndex));
                modelState.SetPatchTranslation(regionIndex, dx.segment(roffset + rpcalength + 3, 3) + modelState.PatchTranslation(regionIndex));
                modelState.SetPatchScale(regionIndex, dx.segment(roffset + rpcalength + 6, 1)[0] + modelState.PatchScale(regionIndex));
            }
            modelState.BakeRotationLinearization();
            const Eigen::Vector3<T> dR = dx.segment(dx.size() - 6, 3);
            currTargetRotation = (Eigen::Quaternion<T>(T(1), dR[0], dR[1], dR[2]) * currTargetRotation).normalized();
            currTargetTranslation += dx.segment(dx.size() - 3, 3);
            // LOG_INFO("time to solve 5: {}", timer.Current()); timer.Restart();
        }
    }

    centeredTargetToModelTransform.linear() = currTargetRotation.toRotationMatrix();
    centeredTargetToModelTransform.translation() = currTargetTranslation;
    Affine<T, 3, 3> newToTargetTransform = (Affine<T, 3, 3>(centeredTargetToModelTransform.matrix()) * Affine<T, 3, 3>::FromTranslation(-targetCenterOfGravity)).Inverse();
    // LOG_INFO("time for total solve: {}", timer.Current()); timer.Restart();

    return newToTargetTransform;
#else

    const Eigen::Matrix<T, 3, -1> initialVertices = model->DeformedVertices(modelState);
    // const Eigen::Vector3<T> centerOfGravity = initialVertices.rowwise().mean();
    // use zero for now as that was the original implementation
    const Eigen::Vector3<T> centerOfGravity = Eigen::Vector3<T>::Zero();
    DiffDataAffine<T, 3, 3> diffToCenterOfGravity(Affine<T, 3, 3>::FromTranslation(-centerOfGravity));
    DiffDataAffine<T, 3, 3> diffFromCenterOfGravity(Affine<T, 3, 3>::FromTranslation(centerOfGravity));

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(diffToCenterOfGravity.Affine() * toTargetTransform * diffFromCenterOfGravity.Affine());
    if (!params.optimizeRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;

            auto [stabilizedVertices, modelConstraints] = model->EvaluateVerticesAndConstraints(context, modelState, params.modelRegularization, params.patchSmoothness);
            DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);
            diffToTargetTransform = diffFromCenterOfGravity.Multiply(diffToTargetTransform.Multiply(diffToCenterOfGravity));
            DiffDataMatrix<T, 3, -1> transformedVertices = diffToTargetTransform.Transform(stabilizedVertices);
            DiffDataMatrix<T, 3, -1> transformedVerticesToEvaluate = GatherFunction<T>::template GatherColumns<3, -1, -1>(
                transformedVertices,
                targetToModelDataMapping);

            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(transformedVerticesToEvaluate,
                                                                                targetData,
                                                                                mask,
                                                                                T(1));
            cost.Add(std::move(residual), params.p2pWeight);
            cost.Add(std::move(modelConstraints), T(1));

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, params.numIterations))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
    }
    else
    {
        LOG_ERROR("could not solve optimization problem");
    }

    Affine<T, 3, 3> newToTargetTransform = diffFromCenterOfGravity.Affine() * globalTransformVariable.Affine() * diffToCenterOfGravity.Affine();

    return newToTargetTransform;
#endif
}


// explicitly instantiate the NeutralPoseFittingOptimization classes
template class NeutralPoseFittingOptimization<float>;
template class NeutralPoseFittingOptimization<double>;


namespace
{
template<class T>
std::pair<T, Eigen::Transform<T, 3, Eigen::Affine>> CalculateProcrustes(const Eigen::Matrix<T, 3, -1> &src,
                                                                        const Eigen::Matrix<T, 3, -1> &target,
                                                                        const bool withScale)
{
    if (withScale)
    {
        auto [scale, aff] = Procrustes<T, 3>::AlignRigidAndScale(src, target);
        return { scale, Eigen::Transform<T, 3, Eigen::Affine>(aff.Matrix()) };
    }
    else
    {
        return { T(1), Eigen::Transform<T, 3, Eigen::Affine>(Procrustes<T, 3>::AlignRigid(src, target).Matrix()) };
    } 
}
}

template <typename T>
void FastPatchModelFitting<T>::Init(const std::shared_ptr<const PatchBlendModel<T>>& patchModel, const std::shared_ptr<TaskThreadPool>& taskThreadPool)
{
    m_taskThreadPool = taskThreadPool;
    m_patchModel = patchModel;
    m_solveData.clear();
    m_mask = VertexWeights<float>(patchModel->NumVertices(), 1.0f);
    UpdateSolveData();
}

template <typename T>
void FastPatchModelFitting<T>::UpdateMask(const VertexWeights<T>& mask)
{
    if (mask.NumVertices() == m_patchModel->NumVertices())
    {
        m_mask = mask;
        UpdateSolveData();
    }
    else
    {
        CARBON_CRITICAL("mask is not valid for fitting");
    }
}

template <typename T>
void FastPatchModelFitting<T>::UpdateRegularization(T regularization)
{
    if (m_regularization != regularization)
    {
        m_regularization = regularization;
        UpdateSolveData();
    }
}

template <typename T>
void FastPatchModelFitting<T>::UpdateSolveData()
{
    m_solveData.clear();
    m_solveData.resize(m_patchModel->NumPatches());
    std::vector<std::vector<int>> region_vIDs(m_patchModel->NumPatches());
    for (int vID = 0; vID < m_patchModel->NumVertices(); ++vID)
    {
        T maskWeight = m_mask.Weights()[vID];
        if (maskWeight > 0)
        {
            for (const auto& [regionId, region_vID, weight] : m_patchModel->BlendMatrix()[vID])
            {
                m_solveData[regionId].vIDs.push_back(vID);
                region_vIDs[regionId].push_back(region_vID);
            }
        }
    }

    
    auto processRegion = [&](int start, int end)
    {
        for (int regionId = start; regionId < end; ++regionId)
        {
            SolveData& solveData = m_solveData[regionId];
            solveData.base = m_patchModel->PatchModels()[regionId].Base()(Eigen::all, region_vIDs[regionId]);
            const auto& modes = m_patchModel->PatchModels()[regionId].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC);
            Eigen::Matrix<T, -1, -1, Eigen::RowMajor> A(3 * solveData.vIDs.size(), modes.cols());
            for (int k = 0; k < (int)solveData.vIDs.size(); ++k)
            {
                A.block(3 * k, 0, 3, A.cols()) = m_mask.Weights()[solveData.vIDs[k]] * modes.block(3 * region_vIDs[regionId][k], 0, 3, A.cols());
            }
            Eigen::Matrix<T, -1, -1> AtA = A.transpose() * A;
            // add regularization
            T regSquared = m_regularization * m_regularization;
            AtA += Eigen::VectorX<T>::Constant(A.cols(), regSquared).asDiagonal();
            solveData.AtAinvAt = AtA.inverse() * A.transpose();
        }
    };

    if (m_taskThreadPool) m_taskThreadPool->AddTaskRangeAndWait((int)m_solveData.size(), processRegion);
    else processRegion(0, (int)m_solveData.size());
}

template <typename T>
void FastPatchModelFitting<T>::Fit(PatchBlendModel<T>::State& state, const Eigen::Matrix3X<T>& vertices, const Settings& settings)
{
    UpdateRegularization(settings.regularization);

    state.ResetPatchVertexDeltas();

    auto processRegion = [&](int start, int end)
    {
        for (int regionId = start; regionId < end; ++regionId)
        {
            // if (regionId == settings.fixedRegion) continue;
            
            Eigen::Matrix3X<T> regionTargetVertices = vertices(Eigen::all, m_solveData[regionId].vIDs);

            T scale = T(1);
            Eigen::Transform<T, 3, Eigen::Affine> transform = Eigen::Transform<T, 3, Eigen::Affine>::Identity();

            if (regionId != settings.fixedRegion)
            {
                std::tie(scale, transform) = CalculateProcrustes<T>(regionTargetVertices, m_solveData[regionId].base, settings.withScale);
            }
            else
            {
                transform.translation() = -m_patchModel->PatchCenterOfGravity(regionId);
            }
            Eigen::Transform<T, 3, Eigen::Affine> scaleTransform = transform;
            scaleTransform.linear() *= scale;

            // // inverse scale and transform for patch
            const T invScale = T(1) / scale;
            Eigen::Transform<T, 3, Eigen::Affine> toBodyTransform = transform;
            toBodyTransform.linear() = transform.linear().transpose();
            toBodyTransform.translation() = - invScale * toBodyTransform.linear() * transform.translation();

            state.SetPatchScale(regionId, invScale);
            state.SetPatchRotation(regionId, Eigen::Quaternion<T>(toBodyTransform.linear()));
            state.SetPatchTranslation(regionId, toBodyTransform.translation());

            Eigen::Matrix3X<T> transformedTargetRegion = scaleTransform * regionTargetVertices - m_solveData[regionId].base;
            Eigen::VectorX<T> coeffs = m_solveData[regionId].AtAinvAt * transformedTargetRegion.reshaped(transformedTargetRegion.size(), 1);
            state.SetPatchPcaWeights(regionId, coeffs);
        }
    };

    if (m_taskThreadPool) m_taskThreadPool->AddTaskRangeAndWait((int)m_solveData.size(), processRegion);
    else processRegion(0, (int)m_solveData.size());
}

template <typename T>
Eigen::VectorX<T> FastPatchModelFitting<T>::FitRegion(int regionIndex, const Eigen::Matrix3X<T>& vertices) const
{
    // Eigen::Matrix3X<T> delta = (vertices - m_solveData[regionIndex].base).array().colwise() - m_patchModel->PatchCenterOfGravity(regionIndex).array();
    return m_solveData[regionIndex].AtAinvAt * vertices.reshaped(vertices.size(), 1);
}

template class FastPatchModelFitting<float>;


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
