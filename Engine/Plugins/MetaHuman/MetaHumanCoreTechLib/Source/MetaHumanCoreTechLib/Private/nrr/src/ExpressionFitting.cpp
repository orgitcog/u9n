// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/ExpressionFitting.h>
#include <nls/Cost.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/ScatterFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/AddFunction.h>
#include <nls/functions/ScaleFunction.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <carbon/utils/TaskThreadPoolUtils.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
SculptFittingState<T> SculptFittingOptimization<T>::RegisterPose(const Eigen::Matrix<T, 3, -1>& targetVertices,
                                                                 const SculptFittingState<T>& previousState,
                                                                 const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                                                 const std::shared_ptr<IdentityBlendModel<T>> model,
                                                                 const SculptFittingParams<T>& params,
                                                                 const Eigen::VectorX<T>& mask)
{
    CARBON_ASSERT(previousState.pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

    SculptFittingState<T> outputData;

    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const int numVertices = (int)rigGeometry->GetMesh(0).NumVertices();

    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsBs = Eigen::VectorXi::Zero(numVertices);

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;
    }

    for (int i = 0; i < numVertices; ++i)
    {
        gatherIdsBs[i] = 3 * numJoints + i;
    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);

    for (int i = 0; i < numJoints; ++i)
    {
        scatterIdsTrans[i] = 3 * i;
    }
    for (int i = 0; i < numJoints; ++i)
    {
        scatterIdsRot[i] = 3 * i + 1;
    }
    for (int i = 0; i < numJoints; ++i)
    {
        scatterIdsScale[i] = 3 * i + 2;
    }

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(previousState.toTargetTransform);
    VectorVariable<T> parametersVar = VectorVariable<T>(previousState.pcaParameters);

    if (!params.fitRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;

            const DiffData<T> diffParams = parametersVar.Evaluate(context);
            const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);
            const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParams);

            DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues,
                                                                                                                     gatherIdsTrans);
            DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues,
                                                                                                                  gatherIdsRot);
            DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues,
                                                                                                              gatherIdsScale);

            const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredTranslationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * 3),
                scatterIdsTrans);
            DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredRotationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * 3),
                scatterIdsRot);
            const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredScaleDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * 3),
                scatterIdsTrans);

            const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(3,
                                                                                                      (int)(rigGeometry->
                                                                                                            GetJointRig().
                                                                                                            NumJoints() * 3),
                                                                                                      degToRad *
                                                                                                      (DiffData<T>(std::move(
                                                                                                                       scatteredRotationsDiff
                                                                                                                       .Value()),
                                                                                                                   scatteredRotationsDiff
                                                                                                                   .JacobianPtr())));

            DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

            DiffData<T> modelConstraints = model->EvaluateRegularization(diffParams);
            const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * 9, 1,
                                                           std::move(jointsDiffMatrix));

            std::vector<DiffDataMatrix<T, 3, -1>> blendshapeDeltas;
            if (params.fitBlendshapes)
            {
                blendshapeDeltas.emplace_back(GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsBs));
            }
            DiffDataMatrix<T, 3, -1> vertices = std::move(rigGeometry->EvaluateWithPerMeshBlendshapes(diffToTargetTransform,
                                                                                                      flattenedValues,
                                                                                                      blendshapeDeltas,
                                                                                                      { 0 }).MoveVertices().front());

            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices,
                                                                                targetVertices,
                                                                                mask,
                                                                                T(1));

            cost.Add(std::move(residual), params.p2pWeight);
            cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    outputData.toTargetTransform = globalTransformVariable.Affine();
    outputData.pcaParameters = parametersVar.Value();

    return outputData;
}


template<class T>
SculptFittingState<T> SculptFittingOptimization<T>::RegisterPoseProjectToLinear(
    const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
    const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
    const SculptFittingState<T> &previousState,
    const std::shared_ptr<RigGeometry<T>> &rigGeometry,
    const std::shared_ptr<IdentityBlendModel<T>> model,
    const Eigen::Matrix<T, -1, -1> &linearModel,
    const Eigen::Vector<T, -1> &linearModelMean,
    const SculptFittingParams<T> &params)
{
    CARBON_ASSERT(previousState.pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

    SculptFittingState<T> outputData;

    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<std::string> blendshapeNames;
    std::vector<int> blendshapeIndices;
    std::vector<Eigen::VectorXi> gatherIdsBs;
    std::vector<Eigen::VectorX<T>> masks;
    std::vector<Eigen::Matrix<T, 3, -1>> targetVec;

    for (const auto &[name, mapping] : targetToModelMapping)
    {
        LOG_INFO("name {}", name);
        LOG_INFO("mapping {} {}", mapping.first, mapping.second);

        int index = rigGeometry->GetMeshIndex(name);
        auto indexIt = std::find(blendshapeMeshIndices.begin(), blendshapeMeshIndices.end(), index);
        if (indexIt == blendshapeMeshIndices.end())
        {
            LOG_WARNING("Target {} has no blendshapes.");
            continue;
        }

        auto targetIt = targetVertices.find(name);
        if (targetIt == targetVertices.end())
        {
            CARBON_CRITICAL("Register pose failed, bad input arguments.");
        }

        const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

        const auto &[startRange, rangeSize] = mapping;

        Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
        Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
        for (int i = 0; i < (int)rangeSize; ++i)
        {
            range[i] = startRange + i;
        }

        blendshapeNames.push_back(name);
        gatherIdsBs.push_back(range);
        blendshapeIndices.push_back(index);
        targetVec.push_back(targetMeshVertices);
        masks.push_back(mask);
    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    // calculate starting gene code modes mixing coefficients
    const auto &A = linearModel;

    const Eigen::Matrix<T, -1, -1> E = Eigen::Matrix<T, -1, -1>::Identity(A.cols(), A.cols());
    const Eigen::Matrix<T, -1, -1> M = (A.transpose() * A + 1e-4 * E).inverse() * A.transpose();

    const Eigen::Vector<T, -1> b = previousState.pcaParameters - linearModelMean;

    const Eigen::Vector<T, -1> x = M * b;
    VectorVariable<T> modesMixingVar = VectorVariable<T>(x);

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(previousState.toTargetTransform);

    if (!params.fitRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;

        const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);

        // calculate PCA coefficients by mixing gene code (linear) modes
        const DiffData<T> diffMixParams = modesMixingVar.Evaluate(context);
        const Eigen::Matrix<T, -1, -1> projectedParams = A * diffMixParams.Value() + linearModelMean;

        JacobianConstPtr<T> jacobian;
        if (diffMixParams.HasJacobian())
        {
            jacobian = diffMixParams.Jacobian().Premultiply(A.sparseView());
        }
        const DiffData<T> diffParamsProjected(projectedParams, jacobian);

        const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParamsProjected);

        DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
        DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
        DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

        const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredTranslationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredRotationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsRot);

        const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredScaleDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
            dofPerTransformType,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));


        DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

        DiffData<T> modelConstraints = model->EvaluateRegularization(diffParamsProjected);
        const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));

        std::vector<DiffDataMatrix<T, 3, -1>> blendshapeDeltas;
        if (params.fitBlendshapes)
        {
            for (int i = 0; i < (int)gatherIdsBs.size(); ++i)
            {
                blendshapeDeltas.emplace_back(GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsBs[i]));
            }
        }

        std::vector<DiffDataMatrix<T, 3, -1>> vertices = rigGeometry
                                                             ->EvaluateWithPerMeshBlendshapes(diffToTargetTransform,
                                                                                              flattenedValues,
                                                                                              blendshapeDeltas,
                                                                                              blendshapeIndices)
                                                             .MoveVertices();

        for (int i = 0; i < (int)targetVec.size(); ++i)
        {
            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[i], masks[i], T(1));

            cost.Add(std::move(residual), params.p2pWeight);
        }

        cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    outputData.toTargetTransform = globalTransformVariable.Affine();
    outputData.pcaParameters = A * modesMixingVar.Value() + linearModelMean;

    return outputData;
}


template<class T>
std::vector<SculptFittingState<T>> SculptFittingOptimization<T>::RegisterMultiplePosesProjectToLinear(
    const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
    const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
    const std::vector<SculptFittingState<T>> &previousStates,
    const std::shared_ptr<RigGeometry<T>> &rigGeometry,
    const std::vector<std::shared_ptr<IdentityBlendModel<T>>> models,
    const std::vector<Eigen::Matrix<T, -1, -1>> &linearModels,
    const std::vector<Eigen::Vector<T, -1>> &linearModelsMeans,
    const SculptFittingParams<T> &params)
{
    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<std::vector<int>> blendshapeIndices(linearModels.size());
    std::vector<std::vector<Eigen::VectorXi>> gatherIdsBs(linearModels.size());
    std::vector<std::vector<Eigen::VectorX<T>>> masks(linearModels.size());
    std::vector<std::vector<Eigen::Matrix<T, 3, -1>>> targetVec(linearModels.size());

    for (int tgt = 0; tgt < (int)linearModels.size(); ++tgt)
    {
        CARBON_ASSERT(previousStates[tgt].pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");
        for (const auto &[name, mapping] : targetToModelMapping[tgt])
        {
            int index = rigGeometry->GetMeshIndex(name);
            auto indexIt = std::find(blendshapeMeshIndices.begin(), blendshapeMeshIndices.end(), index);
            if (indexIt == blendshapeMeshIndices.end())
            {
                LOG_WARNING("Target {} has no blendshapes.");
                continue;
            }

            auto targetIt = targetVertices[tgt].find(name);
            if (targetIt == targetVertices[tgt].end())
            {
                CARBON_CRITICAL("Register pose failed, bad input arguments.");
            }

            const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

            const auto &[startRange, rangeSize] = mapping;

            Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
            Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
            for (int i = 0; i < (int)rangeSize; ++i)
            {
                range[i] = startRange + i;
            }

            gatherIdsBs[tgt].push_back(range);
            blendshapeIndices[tgt].push_back(index);
            targetVec[tgt].push_back(targetMeshVertices);
            masks[tgt].push_back(mask);
        }
    }

    

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    // calculate starting gene code modes mixing coefficients
    const auto &A = linearModels[0];

    const Eigen::Matrix<T, -1, -1> E = Eigen::Matrix<T, -1, -1>::Identity(A.cols(), A.cols());
    const Eigen::Matrix<T, -1, -1> M = (A.transpose() * A + 1e-4 * E).inverse() * A.transpose();

    const Eigen::Vector<T, -1> b = previousStates[0].pcaParameters - linearModelsMeans[0];

    const Eigen::Vector<T, -1> x = M * b;
    VectorVariable<T> modesMixingVar = VectorVariable<T>(x);

    std::vector<AffineVariable<QuaternionVariable<T>>> globalTransformVariable(linearModels.size());
    for (int i = 0; i < (int)globalTransformVariable.size(); ++i)
    {
        globalTransformVariable[i].SetAffine(previousStates[i].toTargetTransform);

        if (!params.fitRigid)
        {
            globalTransformVariable[i].MakeConstant(/*linear=*/true, /*translation=*/true);
        }
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;

        const DiffData<T> diffMixParams = modesMixingVar.Evaluate(context);

        for (int tgt = 0; tgt < (int)linearModels.size(); ++tgt)
        {
            const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable[tgt].EvaluateAffine(context);

            // calculate PCA coefficients by mixing gene code (linear) modes
            const auto &A = linearModels[tgt];
            const Eigen::Matrix<T, -1, -1> projectedParams = A * diffMixParams.Value() + linearModelsMeans[tgt];

            JacobianConstPtr<T> jacobian;
            if (diffMixParams.HasJacobian())
            {
                jacobian = diffMixParams.Jacobian().Premultiply(A.sparseView());
            }
            const DiffData<T> diffParamsProjected(projectedParams, jacobian);

            const DiffDataMatrix<T, 3, -1> modelValues = models[tgt]->Evaluate(diffParamsProjected);

            DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
            DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
            DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

            const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredTranslationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredRotationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsRot);

            const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredScaleDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
                dofPerTransformType,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));


            DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

            DiffData<T> modelConstraints = models[tgt]->EvaluateRegularization(diffParamsProjected);
            const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));

            std::vector<DiffDataMatrix<T, 3, -1>> blendshapeDeltas;
            if (params.fitBlendshapes)
            {
                for (int i = 0; i < (int)gatherIdsBs[tgt].size(); ++i)
                {
                    blendshapeDeltas.emplace_back(GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsBs[tgt][i]));
                }
            }

            std::vector<DiffDataMatrix<T, 3, -1>> vertices = rigGeometry
                                                                 ->EvaluateWithPerMeshBlendshapes(diffToTargetTransform,
                                                                                                  flattenedValues,
                                                                                                  blendshapeDeltas,
                                                                                                  blendshapeIndices[tgt])
                                                                 .MoveVertices();

            for (int i = 0; i < (int)targetVec[tgt].size(); ++i)
            {
                DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[tgt][i], masks[tgt][i], T(1));

                cost.Add(std::move(residual), params.p2pWeight);
            }

            cost.Add(std::move(modelConstraints), params.modelRegularization);
        }

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

    std::vector<SculptFittingState<T>> outputData;
    for (int i = 0; i < (int)linearModels.size(); ++i)
    {
        SculptFittingState<T> output;

        output.toTargetTransform = globalTransformVariable[i].Affine();
        output.pcaParameters = linearModels[i] * modesMixingVar.Value() + linearModelsMeans[i];

        outputData.push_back(output);
    }
    

    return outputData;
}


template<class T>
std::vector<SculptFittingState<T>> SculptFittingOptimization<T>::RegisterMultiplePosesPerRegionProjectToLinear(
    const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
    const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
    const std::vector<SculptFittingState<T>> &previousStates,
    const std::shared_ptr<RigGeometry<T>> &rigGeometry,
    const std::vector<std::shared_ptr<IdentityBlendModel<T>>> models,
    const std::vector<std::vector<Eigen::Matrix<T, -1, -1>>> &linearModels,
    const std::vector<std::vector<Eigen::Vector<T, -1>>> &linearModelsMeans,
    const std::vector<std::map<std::string, Eigen::Vector<T, -1>>> &inputMasks,
    const SculptFittingParams<T> &params)
{
    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    int numExpressions = (int)linearModels.size();
    int numRegions = (int)linearModels[0].size();

    std::vector<std::vector<int>> blendshapeIndices(numExpressions);
    std::vector<std::vector<Eigen::VectorXi>> gatherIdsBs(numExpressions);
    std::vector<std::vector<Eigen::VectorX<T>>> masks(numExpressions);
    std::vector<std::vector<Eigen::Matrix<T, 3, -1>>> targetVec(numExpressions);

    for (int tgt = 0; tgt < (int)numExpressions; ++tgt)
    {
        CARBON_ASSERT(previousStates[tgt].pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");
        for (const auto &[name, mapping] : targetToModelMapping[tgt])
        {
            int index = rigGeometry->GetMeshIndex(name);
            auto indexIt = std::find(blendshapeMeshIndices.begin(), blendshapeMeshIndices.end(), index);
            if (indexIt == blendshapeMeshIndices.end())
            {
                LOG_WARNING("Target {} has no blendshapes.", name);

                if (params.fitBlendshapes)
                {
                    continue;
                } 
            }

            auto targetIt = targetVertices[tgt].find(name);
            if (targetIt == targetVertices[tgt].end())
            {
                CARBON_CRITICAL("Register pose failed, bad input arguments.");
            }

            const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

            const auto &[startRange, rangeSize] = mapping;

            Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
            for (int i = 0; i < (int)rangeSize; ++i)
            {
                range[i] = startRange + i;
            }

            Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
            auto maskIt = inputMasks[tgt].find(name);
            if (maskIt != inputMasks[tgt].end())
            {
                mask = maskIt->second;
            }

            gatherIdsBs[tgt].push_back(range);
            blendshapeIndices[tgt].push_back(index);
            targetVec[tgt].push_back(targetMeshVertices);
            masks[tgt].push_back(mask);
        }
    }


    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    // calculate starting gene code modes mixing coefficients (zeros)
    std::vector<VectorVariable<T>> modesMixingVars;
    for (int i = 0; i < numRegions; ++i)
    {
        modesMixingVars.push_back(VectorVariable<T>(Eigen::Vector<T, -1>::Zero(linearModels[0][i].cols())));
    }

    // count the number of PCA parameters per target, it is useful later
    std::vector<int> numPCAParams;
    for (int tgt = 0; tgt < (int)numExpressions; ++tgt)
    {
        numPCAParams.push_back(0);
        for (int r = 0; r < numRegions; ++r)
        {
            numPCAParams[numPCAParams.size() - 1] += (int)linearModelsMeans[tgt][r].size();
        }
    }

    std::vector<AffineVariable<QuaternionVariable<T>>> globalTransformVariable(linearModels.size());
    for (int i = 0; i < (int)globalTransformVariable.size(); ++i)
    {
        globalTransformVariable[i].SetAffine(previousStates[i].toTargetTransform);

        if (!params.fitRigid)
        {
            globalTransformVariable[i].MakeConstant(/*linear=*/true, /*translation=*/true);
        }
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    // constraInts and residuals are calculated in a function that is parallelized, thus they are saved for later adding to the cost
    // here we allocate the spaces
    std::vector<std::shared_ptr<DiffData<T>>> modelConstraints(numExpressions);
    std::vector<std::vector<std::shared_ptr<DiffData<T>>>> residuals(numExpressions);

    for (int tgt = 0; tgt < numExpressions; ++tgt)
    {
        residuals[tgt].resize((int)targetVec[tgt].size());
    }

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;

        std::vector<DiffData<T>> diffMixParams;
        for (int r = 0; r < numRegions; ++r)
        {
            diffMixParams.emplace_back(modesMixingVars[r].Evaluate(context));
        }

        auto perExpressionCostCalc = [&](int startInd, int endInd)
        {
            for (int tgt = startInd; tgt < endInd; ++tgt)
            {
                // calculate number of parameters, variables and the space needed to save the Jacobian
                int numParams = 0;
                int numMixVar = 0;
                int sizeToReserve = 0;
                for (int r = 0; r < numRegions; ++r)
                {
                    numMixVar += (int)diffMixParams[r].Value().size();
                    numParams += (int)linearModelsMeans[tgt][r].size();

                    sizeToReserve += (int)diffMixParams[r].Value().size() * (int)linearModelsMeans[tgt][r].size();
                }

                // This is the Jacobian of "projectedParams" and it "unifies" the Jacobians of diffMixParams
                SparseMatrixPtr<T> jacobianCombined = std::make_shared<SparseMatrix<T>>(numParams, numMixVar);

                if (diffMixParams[0].HasJacobian())
                {
                    jacobianCombined->reserve(sizeToReserve);
                }

                Eigen::Vector<T, -1> projectedParams = Eigen::Vector<T, -1>::Zero(numParams);
                int iter = 0;
                for (int r = 0; r < numRegions; ++r)
                {
                    // calculate PCA coefficients by mixing gene code (linear) modes
                    const auto &A = linearModels[tgt][r];
                    const auto &b = diffMixParams[r].Value();
                    const auto &mean = linearModelsMeans[tgt][r];

                    const Eigen::Vector<T, -1> param = A * b + mean;

                    // save params for later use
                    projectedParams.segment(iter, param.size()) = param;

                    if (diffMixParams[r].HasJacobian())
                    {
                        const int start = (int)diffMixParams[r].Jacobian().StartCol();

                        for (int i = 0; i < (int)A.rows(); ++i)
                        {
                            jacobianCombined->startVec(iter + i);

                            Eigen::Vector<T, -1> row = A.row(i);
                            for (int j = 0; j < (int)A.cols(); ++j)
                            {
                                jacobianCombined->insertBackByOuterInner(iter + i, start + j) = row(j);
                            }
                        }

                        jacobianCombined->finalize();
                    }

                    iter += (int)param.size();
                }

                JacobianConstPtr<T> jacobian;
                if (diffMixParams[0].HasJacobian())
                {
                    jacobian = std::make_shared<SparseJacobian<T>>(jacobianCombined, (int)diffMixParams[0].Jacobian().StartCol());
                }
                const DiffData<T> diffParamsProjected(projectedParams, jacobian);

                const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable[tgt].EvaluateAffine(context);

                const DiffDataMatrix<T, 3, -1> modelValues = models[tgt]->Evaluate(diffParamsProjected);


                // Start reorganizing the data
                DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
                DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
                DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

                const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                    gatheredTranslationsDiff,
                    (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                    scatterIdsTrans);

                DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                    gatheredRotationsDiff,
                    (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                    scatterIdsRot);

                const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                    gatheredScaleDiff,
                    (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                    scatterIdsTrans);

                const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
                    dofPerTransformType,
                    (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                    degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));


                DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

                modelConstraints[tgt] = std::make_shared<DiffData<T>>(models[tgt]->EvaluateRegularization(diffParamsProjected));
                const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));

                std::vector<DiffDataMatrix<T, 3, -1>> blendshapeDeltas;
                if (params.fitBlendshapes)
                {
                    for (int i = 0; i < (int)gatherIdsBs[tgt].size(); ++i)
                    {
                        blendshapeDeltas.emplace_back(GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsBs[tgt][i]));
                    }
                }

                std::vector<DiffDataMatrix<T, 3, -1>> vertices = rigGeometry
                                                                     ->EvaluateWithPerMeshBlendshapes(diffToTargetTransform,
                                                                                                      flattenedValues,
                                                                                                      blendshapeDeltas,
                                                                                                      blendshapeIndices[tgt])
                                                                     .MoveVertices();

                for (int i = 0; i < (int)targetVec[tgt].size(); ++i)
                {
                    residuals[tgt][i] = std::make_shared<DiffData<T>>(
                        PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[tgt][i], masks[tgt][i], T(1)));
                }
            }
        };

        TITAN_NAMESPACE::TaskThreadPoolUtils::RunTaskRangeAndWait(numExpressions, perExpressionCostCalc);

        // add costs in one thread
        for (int tgt = 0; tgt < numExpressions; ++tgt)
        {
            cost.Add(std::move(*modelConstraints[tgt]), params.modelRegularization);

            for (int i = 0; i < (int)targetVec[tgt].size(); ++i)
            {
                cost.Add(std::move(*residuals[tgt][i]), params.p2pWeight);
            }
        }

        return cost.CostToDiffData();
    };

    GaussNewtonSolver<T> solver;
    typename GaussNewtonSolver<T>::Settings settings;
    settings.optimizeForRectangularDenseJacobian = true;
    settings.reg = DiagonalRegularization<T>();
    settings.iterations = params.numIterations;

    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, settings))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
    }
    else
    {
        LOG_ERROR("could not solve optimization problem");
    }

    std::vector<SculptFittingState<T>> outputData;
    for (int i = 0; i < (int)numExpressions; ++i)
    {
        SculptFittingState<T> output;
        int iterator = 0;

        output.toTargetTransform = globalTransformVariable[i].Affine();
        output.pcaParameters = Eigen::Vector<T, -1>::Zero(numPCAParams[i]);

        for (int r = 0; r < (int)numRegions; ++r)
        {
            Eigen::Vector<T, -1> pcaCoeff = linearModels[i][r] * modesMixingVars[r].Value() + linearModelsMeans[i][r];
            int length = (int)pcaCoeff.size();

            output.pcaParameters.segment(iterator, length) = pcaCoeff;

            iterator += length;
        }

        outputData.push_back(output);
    }


    return outputData;
}

template <class T>
SculptFittingState<T> SculptFittingOptimization<T>::RegisterPose(const std::map<std::string, Eigen::Matrix<T, 3, -1>>& targetVertices,
                                                                 const std::map<std::string, Eigen::VectorX<T>>& masks,
                                                                 const std::map<std::string, std::pair<int, int>>& targetToModelMapping,
                                                                 const SculptFittingState<T>& previousState,
                                                                 const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                                                 const std::shared_ptr<IdentityBlendModel<T>> model,
                                                                 const SculptFittingParams<T>& params)
{
    CARBON_ASSERT(previousState.pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

    SculptFittingState<T> outputData;

    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<std::string> blendshapeNames;
    std::vector<int> blendshapeIndices;
    std::vector<Eigen::VectorXi> gatherIdsBs;
    std::vector<Eigen::VectorX<T>> localMasks;
    std::vector<Eigen::Matrix<T, 3, -1>> targetVec;

    for (const auto& [name, mapping] : targetToModelMapping)
    {
        int index = rigGeometry->GetMeshIndex(name);
        auto indexIt = std::find(blendshapeMeshIndices.begin(), blendshapeMeshIndices.end(), index);
        if (indexIt == blendshapeMeshIndices.end())
        {
            LOG_WARNING("Target {} has no blendshapes.");
            continue;
        }

        auto targetIt = targetVertices.find(name);
        if (targetIt == targetVertices.end())
        {
            CARBON_CRITICAL("Register pose failed, bad input arguments.");
        }

        auto maskIt = masks.find(name);

        const Eigen::Matrix<T, 3, -1>& targetMeshVertices = targetIt->second;

        const auto& [startRange, rangeSize] = mapping;

        Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
        Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
        if (maskIt != masks.end())
        {
            mask = maskIt->second;
        }

        for (int i = 0; i < (int)rangeSize; ++i)
        {
            range[i] = startRange + i;
        }

        blendshapeNames.push_back(name);
        gatherIdsBs.push_back(range);
        blendshapeIndices.push_back(index);
        targetVec.push_back(targetMeshVertices);
        localMasks.push_back(mask);

    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(previousState.toTargetTransform);
    VectorVariable<T> parametersVar = VectorVariable<T>(previousState.pcaParameters);

    if (!params.fitRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) 
    {
            Cost<T> cost;

            const DiffData<T> diffParams = parametersVar.Evaluate(context);
            const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);
            const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParams);

            DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
            DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
            DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

            const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredTranslationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredRotationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsRot);

            const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredScaleDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(dofPerTransformType,
                                                                                                      (int)(rigGeometry->
                                                                                                            GetJointRig().
                                                                                                            NumJoints() * dofPerTransformType),
                                                                                                      degToRad *
                                                                                                      (DiffData<T>(std::move(
                                                                                                                       scatteredRotationsDiff
                                                                                                                       .Value()),
                                                                                                                   scatteredRotationsDiff
                                                                                                                   .JacobianPtr())));


            DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

            DiffData<T> modelConstraints = model->EvaluateRegularization(diffParams);
            const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1,
                                                           std::move(jointsDiffMatrix));

            std::vector<DiffDataMatrix<T, 3, -1>> blendshapeDeltas;
            if (params.fitBlendshapes)
            {
                for (int i = 0; i < (int)gatherIdsBs.size(); ++i)
                {
                    blendshapeDeltas.emplace_back(GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsBs[i]));
                }
            }

            std::vector<DiffDataMatrix<T, 3, -1>> vertices = rigGeometry->EvaluateWithPerMeshBlendshapes(diffToTargetTransform,
                                                                                                         flattenedValues,
                                                                                                         blendshapeDeltas,
                                                                                                         blendshapeIndices).MoveVertices();

            for (int i = 0; i < (int)targetVec.size(); ++i)
            {
                DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i],
                                                                                    targetVec[i],
                                                                                    localMasks[i],
                                                                                    T(1));

                cost.Add(std::move(residual), params.p2pWeight);
            }

            cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    outputData.toTargetTransform = globalTransformVariable.Affine();
    outputData.pcaParameters = parametersVar.Value();

    return outputData;
}

template<class T>
std::vector<SculptFittingState<T>> SculptFittingOptimization<T>::RegisterMultiplePosesJointsOnlyProjectToLinear(
    const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
    const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
    const std::vector<SculptFittingState<T>> &previousStates,
    const std::shared_ptr<RigGeometry<T>> &rigGeometry,
    const std::shared_ptr<IdentityBlendModel<T>> jointsModel,
    const std::vector<Eigen::Matrix<T, -1, -1>> &linearModels,
    const std::vector<Eigen::Vector<T, -1>> &linearModelsMeans,
    const SculptFittingParams<T> &params)
{
    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<std::vector<Eigen::VectorX<T>>> masks(linearModels.size());
    std::vector<std::vector<Eigen::Matrix<T, 3, -1>>> targetVec(linearModels.size());
    std::vector<std::vector<int>> targetIndices(linearModels.size());

    for (int tgt = 0; tgt < (int)linearModels.size(); ++tgt)
    {
        CARBON_ASSERT(previousStates[tgt].pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

        for (const auto &[name, mapping] : targetToModelMapping[tgt])
        {
            int index = rigGeometry->GetMeshIndex(name);

            auto targetIt = targetVertices[tgt].find(name);
            if (targetIt == targetVertices[tgt].end())
            {
                CARBON_CRITICAL("Register pose failed, bad input arguments.");
            }

            const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

            const auto &[startRange, rangeSize] = mapping;

            Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
            Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
            for (int i = 0; i < (int)rangeSize; ++i)
            {
                range[i] = startRange + i;
            }

            targetVec[tgt].push_back(targetMeshVertices);
            masks[tgt].push_back(mask);
            targetIndices[tgt].push_back(index);
        }
    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    std::vector<AffineVariable<QuaternionVariable<T>>> globalTransformVariable(linearModels.size());

    for (int i = 0; i < (int)globalTransformVariable.size(); ++i)
    {
        globalTransformVariable[i].SetAffine(previousStates[i].toTargetTransform);

        if (!params.fitRigid)
        {
            globalTransformVariable[i].MakeConstant(/*linear=*/true, /*translation=*/true);
        }
    }
    

    T degToRad = T(CARBON_PI) / T(180.0);

    // calculate starting gene code modes mixing coefficients
    const auto &A = linearModels[0];

    const Eigen::Matrix<T, -1, -1> E = Eigen::Matrix<T, -1, -1>::Identity(A.cols(), A.cols());
    const Eigen::Matrix<T, -1, -1> M = (A.transpose() * A + 1e-4 * E).inverse() * A.transpose();

    const Eigen::Vector<T, -1> b = previousStates[0].pcaParameters - linearModelsMeans[0];

    const Eigen::Vector<T, -1> x = M * b;
    VectorVariable<T> modesMixingVar = VectorVariable<T>(x);

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;
        const DiffData<T> diffMixParams = modesMixingVar.Evaluate(context);

        for (int tgt = 0; tgt < (int)linearModels.size(); ++tgt)
        {
            const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable[tgt].EvaluateAffine(context);

            const Eigen::Matrix<T, -1, -1> projectedParams = linearModels[tgt] * diffMixParams.Value() + linearModelsMeans[tgt];

            JacobianConstPtr<T> jacobian;
            if (diffMixParams.HasJacobian())
            {
                jacobian = diffMixParams.Jacobian().Premultiply(linearModels[tgt].sparseView());
            }

            const DiffData<T> diffParamsProjected(projectedParams, jacobian);

            const DiffDataMatrix<T, 3, -1> modelValues = jointsModel->Evaluate(diffParamsProjected);

            DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
            DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
            DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

            const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredTranslationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredRotationsDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsRot);

            const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
                gatheredScaleDiff,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                scatterIdsTrans);

            const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
                dofPerTransformType,
                (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
                degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));

            DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

            DiffData<T> modelConstraints = jointsModel->EvaluateRegularization(diffParamsProjected);
            const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));


            std::vector<DiffDataMatrix<T, 3, -1>>
                vertices = rigGeometry->EvaluateWithPerMeshBlendshapes(diffToTargetTransform, flattenedValues, {}, targetIndices[tgt]).MoveVertices();

            for (int i = 0; i < (int)targetVec[tgt].size(); ++i)
            {
                DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[tgt][i], masks[tgt][i], T(1));

                cost.Add(std::move(residual), params.p2pWeight);
            }

            cost.Add(std::move(modelConstraints), params.modelRegularization);
        }

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

    std::vector<SculptFittingState<T>> outputData;
    for (int i = 0; i < (int)linearModels.size(); ++i)
    {
        SculptFittingState<T> output;

        output.toTargetTransform = globalTransformVariable[i].Affine();
        output.pcaParameters = linearModels[i] * modesMixingVar.Value() + linearModelsMeans[i];

        outputData.push_back(output);
    }


    return outputData;
}

template<class T>
SculptFittingState<T> SculptFittingOptimization<T>::RegisterPoseJointsOnlyProjectToLinear(
    const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
    const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
    const SculptFittingState<T> &previousState,
    const std::shared_ptr<RigGeometry<T>> &rigGeometry,
    const std::shared_ptr<IdentityBlendModel<T>> model,
    const Eigen::Matrix<T, -1, -1> &linearModel,
    const Eigen::Vector<T, -1> &linearModelMean,
    const SculptFittingParams<T> &params)
{
    CARBON_ASSERT(previousState.pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

    SculptFittingState<T> outputData;

    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<Eigen::VectorX<T>> masks;
    std::vector<Eigen::Matrix<T, 3, -1>> targetVec;
    std::vector<int> targetIndices;

    for (const auto &[name, mapping] : targetToModelMapping)
    {
        int index = rigGeometry->GetMeshIndex(name);

        auto targetIt = targetVertices.find(name);
        if (targetIt == targetVertices.end())
        {
            CARBON_CRITICAL("Register pose failed, bad input arguments.");
        }

        const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

        const auto &[startRange, rangeSize] = mapping;

        Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
        Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
        for (int i = 0; i < (int)rangeSize; ++i)
        {
            range[i] = startRange + i;
        }

        targetVec.push_back(targetMeshVertices);
        masks.push_back(mask);
        targetIndices.push_back(index);
    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(previousState.toTargetTransform);

    if (!params.fitRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    // calculate starting gene code modes mixing coefficients
    const auto &A = linearModel;

    const Eigen::Matrix<T, -1, -1> E = Eigen::Matrix<T, -1, -1>::Identity(A.cols(), A.cols());
    const Eigen::Matrix<T, -1, -1> M = (A.transpose() * A + 1e-4 * E).inverse() * A.transpose();

    const Eigen::Vector<T, -1> b = previousState.pcaParameters - linearModelMean;

    const Eigen::Vector<T, -1> x = M * b;
    VectorVariable<T> modesMixingVar = VectorVariable<T>(x);

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;

        const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);

        const DiffData<T> diffMixParams = modesMixingVar.Evaluate(context);

        const Eigen::Matrix<T, -1, -1> projectedParams = A * diffMixParams.Value() + linearModelMean;

        JacobianConstPtr<T> jacobian;
        if (diffMixParams.HasJacobian())
        {
            jacobian = diffMixParams.Jacobian().Premultiply(A.sparseView());
        }

        const DiffData<T> diffParamsProjected(projectedParams, jacobian);
        
        const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParamsProjected);

        DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
        DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
        DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

        const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredTranslationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredRotationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsRot);

        const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredScaleDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
            dofPerTransformType,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));

        DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

        DiffData<T> modelConstraints = model->EvaluateRegularization(diffParamsProjected);
        const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));


        std::vector<DiffDataMatrix<T, 3, -1>>
            vertices = rigGeometry->EvaluateWithPerMeshBlendshapes(diffToTargetTransform, flattenedValues, {}, targetIndices).MoveVertices();

        for (int i = 0; i < (int)targetVec.size(); ++i)
        {
            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[i], masks[i], T(1));

            cost.Add(std::move(residual), params.p2pWeight);
        }

        cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    outputData.toTargetTransform = globalTransformVariable.Affine();
    outputData.pcaParameters = A * modesMixingVar.Value() + linearModelMean;

    return outputData;
}

template<class T>
SculptFittingState<T> SculptFittingOptimization<T>::RegisterPoseJointsOnly(const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
                                                                           const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
                                                                           const SculptFittingState<T> &previousState,
                                                                           const std::shared_ptr<RigGeometry<T>> &rigGeometry,
                                                                           const std::shared_ptr<IdentityBlendModel<T>> model,
                                                                           const SculptFittingParams<T> &params)
{
    CARBON_ASSERT(previousState.pcaParameters.size() > 0, "Optimization should have initialized previous state as input.");

    SculptFittingState<T> outputData;

    const int numJoints = (int)rigGeometry->GetJointRig().NumJoints();
    const auto blendshapeMeshIndices = rigGeometry->GetBlendshapeMeshIndices();

    std::vector<Eigen::VectorX<T>> masks;
    std::vector<Eigen::Matrix<T, 3, -1>> targetVec;
    std::vector<int> targetIndices;

    for (const auto &[name, mapping] : targetToModelMapping)
    {
        int index = rigGeometry->GetMeshIndex(name);

        auto targetIt = targetVertices.find(name);
        if (targetIt == targetVertices.end())
        {
            CARBON_CRITICAL("Register pose failed, bad input arguments.");
        }

        const Eigen::Matrix<T, 3, -1> &targetMeshVertices = targetIt->second;

        const auto &[startRange, rangeSize] = mapping;

        Eigen::VectorXi range = Eigen::VectorXi::Zero(rangeSize);
        Eigen::VectorX<T> mask = Eigen::VectorX<T>::Ones(rangeSize);
        for (int i = 0; i < (int)rangeSize; ++i)
        {
            range[i] = startRange + i;
        }

        targetVec.push_back(targetMeshVertices);
        masks.push_back(mask);
        targetIndices.push_back(index);
    }

    Eigen::VectorXi scatterIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi scatterIdsScale = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsTrans = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsRot = Eigen::VectorXi::Zero(numJoints);
    Eigen::VectorXi gatherIdsScale = Eigen::VectorXi::Zero(numJoints);

    const int dof = 9;
    const int dofPerTransformType = 3;

    for (int i = 0; i < numJoints; ++i)
    {
        gatherIdsTrans[i] = i;
        gatherIdsRot[i] = numJoints + i;
        gatherIdsScale[i] = 2 * numJoints + i;

        scatterIdsTrans[i] = dofPerTransformType * i;
        scatterIdsRot[i] = dofPerTransformType * i + 1;
        scatterIdsScale[i] = dofPerTransformType * i + 2;
    }

    AffineVariable<QuaternionVariable<T>> globalTransformVariable;
    globalTransformVariable.SetAffine(previousState.toTargetTransform);
    VectorVariable<T> parametersVar = VectorVariable<T>(previousState.pcaParameters);

    if (!params.fitRigid)
    {
        globalTransformVariable.MakeConstant(/*linear=*/true, /*translation=*/true);
    }

    T degToRad = T(CARBON_PI) / T(180.0);

    std::function<DiffData<T>(Context<T> *)> evaluationFunction = [&](Context<T> *context)
    {
        Cost<T> cost;

        const DiffData<T> diffParams = parametersVar.Evaluate(context);
        const DiffDataAffine<T, 3, 3> diffToTargetTransform = globalTransformVariable.EvaluateAffine(context);
        const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParams);

        DiffDataMatrix<T, 3, -1> gatheredTranslationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsTrans);
        DiffDataMatrix<T, 3, -1> gatheredRotationsDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsRot);
        DiffDataMatrix<T, 3, -1> gatheredScaleDiff = GatherFunction<T>::template GatherColumns<3, -1, -1>(modelValues, gatherIdsScale);

        const DiffDataMatrix<T, 3, -1> scatteredTranslationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredTranslationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        DiffDataMatrix<T, 3, -1> scatteredRotationsDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredRotationsDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsRot);

        const DiffDataMatrix<T, 3, -1> scatteredScaleDiff = ScatterFunction<T>::template ScatterColumns<3, -1, -1>(
            gatheredScaleDiff,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            scatterIdsTrans);

        const DiffDataMatrix<T, 3, -1> scatteredRotationsDiffAsRadians = DiffDataMatrix<T, 3, -1>(
            dofPerTransformType,
            (int)(rigGeometry->GetJointRig().NumJoints() * dofPerTransformType),
            degToRad * (DiffData<T>(std::move(scatteredRotationsDiff.Value()), scatteredRotationsDiff.JacobianPtr())));

        DiffDataMatrix<T, 3, -1> jointsDiffMatrix = scatteredScaleDiff + scatteredTranslationsDiff + scatteredRotationsDiffAsRadians;

        DiffData<T> modelConstraints = model->EvaluateRegularization(diffParams);
        const DiffDataMatrix<T, -1, 1> flattenedValues((int)rigGeometry->GetJointRig().NumJoints() * dof, 1, std::move(jointsDiffMatrix));


        std::vector<DiffDataMatrix<T, 3, -1>>
            vertices = rigGeometry->EvaluateWithPerMeshBlendshapes(diffToTargetTransform, flattenedValues, {}, targetIndices).MoveVertices();

        for (int i = 0; i < (int)targetVec.size(); ++i)
        {
            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(vertices[i], targetVec[i], masks[i], T(1));

            cost.Add(std::move(residual), params.p2pWeight);
        }

        cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    outputData.toTargetTransform = globalTransformVariable.Affine();
    outputData.pcaParameters = parametersVar.Value();

    return outputData;
}

// explicitly instantiate the SculptFittingOptimization classes
template class SculptFittingOptimization<float>;
template class SculptFittingOptimization<double>;


template <class T>
Eigen::VectorX<T> ExpressionParametersFittingOptimization<T>::RegisterPose(const Eigen::Matrix<T, 3, -1>& targetJointDeltas,
                                                                           const Eigen::Matrix<T, 3, -1>& targetBlendshapeDeltas,
                                                                           const Eigen::VectorX<T>& previousParams,
                                                                           const std::shared_ptr<IdentityBlendModel<T>>& model,
                                                                           const ExpressionParametersFittingParams<T>& params)
{
    CARBON_ASSERT(previousParams.size() > 0, "Optimization should have initialized previous state as input.");

    const int modelDim = model->NumVertices();

    const int numJointParams = (int)targetJointDeltas.cols();

    bool isJointsOnly = modelDim == numJointParams ? true : false;

    Eigen::Matrix<T, 3, -1> concatenatedTargets = Eigen::Matrix<T, 3, -1>::Zero(3, modelDim);
    if (isJointsOnly)
    {
        concatenatedTargets << targetJointDeltas;
    }
    else
    {
        concatenatedTargets << targetJointDeltas, targetBlendshapeDeltas;
    }


    VectorVariable<T> parametersVar = VectorVariable<T>(previousParams);
    Eigen::VectorX<T> weights = Eigen::VectorX<T>::Ones(modelDim);

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;

            const DiffData<T> diffParams = parametersVar.Evaluate(context);
            const DiffDataMatrix<T, 3, -1> modelValues = model->Evaluate(diffParams);

            DiffData<T> modelConstraints = model->EvaluateRegularization(diffParams);
            DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(modelValues,
                                                                                concatenatedTargets,
                                                                                weights,
                                                                                T(1));


            cost.Add(std::move(residual), params.geometryWeight);
            cost.Add(std::move(modelConstraints), params.modelRegularization);

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

    return parametersVar.Value();
}

// explicitly instantiate the ExpressionParametersFittingOptimization classes
template class ExpressionParametersFittingOptimization<float>;
template class ExpressionParametersFittingOptimization<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
