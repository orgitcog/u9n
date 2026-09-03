// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/TeethAlignment.h>

#include <carbon/geometry/AABBTree.h>
#include <carbon/io/Utils.h>
#include <carbon/io/JsonIO.h>
#include <nls/Cost.h>
#include <nls/DiffDataMatrix.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/solver/LMSolver.h>
#include <nls/functions/PointSurfaceConstraintFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <rig/Rig.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <conformer/GeometryConstraints.h>
#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void CalculateCorrespondanceFromReferent(const Mesh<T>& refHeadMesh,
                                         const Mesh<T>& refTeethMesh,
                                         const VertexWeights<T>& interfaceVertices,
                                         std::vector<Eigen::Vector3i>& triangles,
                                         std::vector<Eigen::Vector3<T>>& barycentrics,
                                         std::vector<int>& passedTeethVertices,
                                         std::vector<Eigen::Vector3<T>>& deltas,
                                         bool useClosestPoint = false)
{
    triangles.clear();
    barycentrics.clear();
    passedTeethVertices.clear();
    deltas.clear();

    Mesh<T> headMesh = refHeadMesh;
    Mesh<T> teethMesh = refTeethMesh;
    headMesh.CalculateVertexNormals();
    teethMesh.CalculateVertexNormals();

    TITAN_NAMESPACE::AABBTree<T> aabbTree(headMesh.Vertices().transpose(), headMesh.Triangles().transpose());

    const std::vector<int> vertIds = interfaceVertices.NonzeroVertices();
    const Eigen::Matrix<T, 3, -1> teethVertices = teethMesh.Vertices();
    const Eigen::Matrix<T, 3, -1> teethNormals = teethMesh.VertexNormals();

    for (size_t i = 0; i < vertIds.size(); ++i)
    {
        const Eigen::Vector3<T> query = teethVertices.col(vertIds[i]);
        int triangleIndex;
        Eigen::Vector3<T> barycentric;

        if (useClosestPoint)
        {
            const auto [tId, bc, dist] = aabbTree.getClosestPoint(query.transpose(), T(1e9));
            triangleIndex = (int)tId;
            barycentric = bc.transpose();
        }
        else
        {
            const Eigen::Vector3<T> direction = teethNormals.col(vertIds[i]);
            const auto [tId, bc, dist] = aabbTree.intersectRay(query.transpose(), direction.transpose());
            triangleIndex = (int)tId;
            barycentric = bc.transpose();
        }

        if (triangleIndex == -1)
        {
            continue;
        }
        passedTeethVertices.push_back(vertIds[i]);
        triangles.push_back(headMesh.Triangles().col(triangleIndex));
        barycentrics.push_back(barycentric);

        const BarycentricCoordinates<T> bcOut(headMesh.Triangles().col(triangleIndex), barycentric);
        const Eigen::Vector3<T> headIntersectionPoint = bcOut.template Evaluate<3>(headMesh.Vertices());
        deltas.push_back(headIntersectionPoint - query);
    }
}

template <class T>
void CollectTargets(const std::shared_ptr<Rig<T>>& refRig,
                    const std::shared_ptr<Rig<T>>& tgtRig,
                    const VertexWeights<T>& interfaceVertices,
                    const std::vector<Eigen::VectorX<T>>& perExprControls,
                    std::vector<Eigen::Matrix3X<T>>& targetDeltas,
                    std::vector<Eigen::Matrix3X<T>>& targetHeadVertices,
                    std::vector<Eigen::Matrix3X<T>>& targetTeethVertices,
                    bool useClosestPoint = false)
{
    targetHeadVertices.clear();
    targetTeethVertices.clear();
    targetDeltas.clear();

    Mesh<T> headMesh = refRig->GetRigGeometry()->GetMesh(0);
    headMesh.Triangulate();
    Mesh<T> teethMesh = refRig->GetRigGeometry()->GetMesh(1);
    teethMesh.Triangulate();

    for (size_t i = 0; i < perExprControls.size(); ++i)
    {
        const std::vector<Eigen::Matrix<T, 3, -1>> referentRigState = refRig->EvaluateVertices(perExprControls[i], /*lod=*/0, /*meshIndices=*/{ 0, 1 });
        const std::vector<Eigen::Matrix<T, 3, -1>> targetRigState = tgtRig->EvaluateVertices(perExprControls[i], /*lod=*/0, /*meshIndices=*/{ 0, 1 });

        headMesh.SetVertices(referentRigState[0]);
        teethMesh.SetVertices(referentRigState[1]);

        std::vector<Eigen::Vector3i> triangles;
        std::vector<Eigen::Vector3<T>> barycentrics;
        std::vector<Eigen::Vector3<T>> deltas;
        std::vector<int> passedTeethVertices;

        CalculateCorrespondanceFromReferent<T>(headMesh,
                                               teethMesh,
                                               interfaceVertices,
                                               triangles,
                                               barycentrics,
                                               passedTeethVertices,
                                               deltas,
                                               /*closestPoint=*/useClosestPoint);
        Eigen::Matrix3X<T> outputDeltas = Eigen::Matrix3X<T>::Zero(3, triangles.size());
        Eigen::Matrix3X<T> outputTeethVertices = Eigen::Matrix3X<T>::Zero(3, triangles.size());
        Eigen::Matrix3X<T> outputHeadVertices = Eigen::Matrix3X<T>::Zero(3, triangles.size());

        for (size_t j = 0; j < triangles.size(); ++j)
        {
            outputDeltas.col(j) = deltas[j];

            const BarycentricCoordinates<T> bcOut(triangles[j], barycentrics[j]);
            const Eigen::Vector3<T> headIntersectionPoint = bcOut.template Evaluate<3>(targetRigState[0]);
            outputHeadVertices.col(j) = headIntersectionPoint;
            outputTeethVertices.col(j) = targetRigState[1].col(passedTeethVertices[j]);
        }

        targetDeltas.push_back(outputDeltas);
        targetHeadVertices.push_back(outputHeadVertices);
        targetTeethVertices.push_back(outputTeethVertices);
    }
}

template <class T>
struct TeethAlignment<T>::Private
{
    std::shared_ptr<Rig<T>> referentRig;
    std::shared_ptr<Rig<T>> targetRig;

    std::vector<Eigen::VectorX<T>> controlsToEvaluate;

    std::vector<Eigen::Matrix<T, 3, -1>> headCollisionTargets;
    std::vector<Eigen::Matrix<T, 3, -1>> teethCollisionTargets;

    VertexWeights<T> teethInterfaceVertices;
};

template <class T>
TeethAlignment<T>::TeethAlignment() : m(new Private)
{}

template <class T> TeethAlignment<T>::~TeethAlignment() = default;
template <class T> TeethAlignment<T>::TeethAlignment(TeethAlignment&& other) = default;
template <class T> TeethAlignment<T>& TeethAlignment<T>::operator=(TeethAlignment&& other) = default;

template <class T>
void TeethAlignment<T>::SetRig(const std::shared_ptr<Rig<T>>& referentRig, const std::shared_ptr<Rig<T>>& targetRig)
{
    m->referentRig = referentRig;
    m->targetRig = targetRig;
}

template <class T>
void TeethAlignment<T>::LoadRig(dna::Reader* dnaReferentRig, dna::Reader* dnaTargetRig)
{
    auto referentRig = std::make_shared<Rig<T>>();
    if (!referentRig->LoadRig(dnaReferentRig))
    {
        CARBON_CRITICAL("Unable to initialize rig logic from loaded dna.");
    }
    auto targetRig = std::make_shared<Rig<T>>();
    if (!targetRig->LoadRig(dnaTargetRig))
    {
        CARBON_CRITICAL("Unable to initialize rig logic from loaded dna.");
    }
    SetRig(referentRig, targetRig);
}

template <class T>
bool TeethAlignment<T>::CheckControlsConfig(const std::string& configFilenameOrData)
{
    const bool isFileBased = std::filesystem::exists(configFilenameOrData);
    const TITAN_NAMESPACE::JsonElement configJson = TITAN_NAMESPACE::ReadJson(isFileBased ? TITAN_NAMESPACE::ReadFile(configFilenameOrData) : configFilenameOrData);
    std::vector<std::map<std::string, float>> loadedJsonResult;
    if (!configJson.Contains("targets"))
    {
        return false;
    }
    return true;
}

template <class T>
void TeethAlignment<T>::LoadControlsToEvaluate(const std::string& configFilenameOrData)
{
    const bool isFileBased = std::filesystem::exists(configFilenameOrData);
    const TITAN_NAMESPACE::JsonElement configJson = TITAN_NAMESPACE::ReadJson(isFileBased ? TITAN_NAMESPACE::ReadFile(configFilenameOrData) : configFilenameOrData);
    std::vector<std::map<std::string, float>> loadedJsonResult;
    if (!configJson.Contains("targets"))
    {
        CARBON_CRITICAL("json controls file invalid.");
    }
    const int numTargets = (int)configJson["targets"].Array().size();
    for (int i = 0; i < numTargets; ++i)
    {
        const auto currentControlMap = configJson["targets"][i].Get<std::map<std::string, float>>();
        loadedJsonResult.push_back(currentControlMap);
    }
    SetControlsToEvaluate(loadedJsonResult);
}

template <class T>
void TeethAlignment<T>::SetControlsToEvaluate(const std::vector<Eigen::VectorX<T>>& controls) { m->controlsToEvaluate = controls; }

template <class T>
void TeethAlignment<T>::SetControlsToEvaluate(const std::vector<std::map<std::string, float>>& controls)
{
    CARBON_ASSERT(m->referentRig, "referent rig not loaded");
    CARBON_ASSERT(m->targetRig, "target rig not loaded");

    std::vector<Eigen::VectorX<T>> controlsVector;
    const int numGuiControls = m->targetRig->GetRigLogic()->NumGUIControls();
    const std::vector<std::string> guiControlNames = m->targetRig->GetGuiControlNames();

    for (size_t expr = 0; expr < controls.size(); expr++)
    {
        Eigen::VectorX<T> exprControls = Eigen::VectorX<T>::Zero(numGuiControls);
        for (const auto& [controlName, controlValue] : controls[expr])
        {
            for (int i = 0; i < numGuiControls; i++)
            {
                if (controlName == guiControlNames[i])
                {
                    exprControls[i] = controlValue;
                }
            }
        }
        controlsVector.push_back(exprControls);
    }

    m->controlsToEvaluate = controlsVector;
}

template <class T>
const Eigen::Matrix<T, 3, -1> TeethAlignment<T>::CurrentVertices() const
{
    return m->targetRig->GetRigGeometry()->GetMesh(1).Vertices();
}

template <class T>
void TeethAlignment<T>::SetInterfaceVertices(const VertexWeights<T>& mask) { m->teethInterfaceVertices = mask; }

template <class T>
std::pair<T, Affine<T, 3, 3>> TeethAlignment<T>::Align(const Affine<T, 3, 3>& inAffine, T inScale, int numIterations)
{
    CARBON_ASSERT(m->referentRig, "referent rig not loaded");
    CARBON_ASSERT(m->targetRig, "target rig not loaded");
    CARBON_ASSERT(!m->controlsToEvaluate.empty(), "controls to evaluate not set");

    AffineVariable<QuaternionVariable<T>> affineVariable;
    VectorVariable<T> scaleVariable = VectorVariable<T>(1);
    scaleVariable.Set(Eigen::Vector<T, 1>(inScale));

    const bool fixTranslation = m_teethPlacementConfig["fixTranslation"].template Value<bool>();
    const bool fixRotation = m_teethPlacementConfig["fixRotation"].template Value<bool>();
    const bool fixScale = m_teethPlacementConfig["fixScale"].template Value<bool>();
    const bool useClosestPoint = m_teethPlacementConfig["useClosestPoint"].template Value<bool>();
    const T p2pWeight = m_teethPlacementConfig["geometryWeight"].template Value<T>();
    affineVariable.MakeConstant(fixRotation, fixTranslation);

    if (fixScale)
    {
        scaleVariable.MakeConstant();
    }

    std::vector<Eigen::Matrix<T, 3, -1>> targetDeltas;
    std::vector<Eigen::Matrix<T, 3, -1>> targetHeadVertices;
    std::vector<Eigen::Matrix<T, 3, -1>> targetTeethVertices;
    std::vector<Eigen::VectorX<T>> weights;

    std::vector<DiffDataMatrix<T, 3, -1>> diffTargetHeadVertices;
    std::vector<DiffDataMatrix<T, 3, -1>> diffTargetTeethVertices;

    CollectTargets<T>(m->referentRig,
                      m->targetRig,
                      m->teethInterfaceVertices,
                      m->controlsToEvaluate,
                      targetDeltas,
                      targetHeadVertices,
                      targetTeethVertices,
                      useClosestPoint);

    Affine<T, 3, 3> newAffine = inAffine;
    newAffine.SetTranslation(newAffine.Translation() * scaleVariable.Value()[0]);

    Eigen::Vector3<T> centerOfGravity = targetTeethVertices[0].rowwise().mean();
    affineVariable.SetAffine(Affine<T, 3,
                                    3>::FromTranslation(-centerOfGravity) * newAffine *
                             Affine<T, 3, 3>::FromTranslation(scaleVariable.Value()[0] * centerOfGravity));

    for (size_t i = 0; i < targetHeadVertices.size(); ++i)
    {
        diffTargetHeadVertices.push_back(DiffDataMatrix<T, 3, -1>(targetHeadVertices[i]));
        diffTargetTeethVertices.push_back(DiffDataMatrix<T, 3, -1>(targetTeethVertices[i]));
        weights.push_back(Eigen::VectorX<T>::Ones(targetHeadVertices[i].cols()));
    }

    const DiffDataAffine<T, 3, 3> diffToCenterOfGravity(Affine<T, 3, 3>::FromTranslation(-centerOfGravity));
    const DiffDataAffine<T, 3, 3> diffFromCenterOfGravity(Affine<T, 3, 3>::FromTranslation(centerOfGravity));

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
            Cost<T> cost;

            const DiffDataAffine<T, 3, 3> diffAffine = affineVariable.EvaluateAffine(context);
            const DiffDataMatrix<T, 1, 1> scaleMatrix = DiffDataMatrix<T, 1, 1>(scaleVariable.Evaluate(context));

            for (size_t i = 0; i < targetHeadVertices.size(); ++i)
            {
                DiffDataMatrix<T, 3, -1> centeredVertices = diffToCenterOfGravity.Transform(diffTargetTeethVertices[i]);
                const DiffDataMatrix<T, 1, -1> centeredFlattenedVertices = DiffDataMatrix<T, 1, -1>(1,
                                                                                                    (int)(3 * targetTeethVertices[i].cols()),
                                                                                                    (DiffData<T>(std::move(centeredVertices.Value()),
                                                                                                                 centeredVertices.JacobianPtr())));
                DiffDataMatrix<T, 1, -1> scaledAndCenteredFlattenedVertices = MatrixMultiplyFunction<T>::DenseMatrixMatrixMultiply(scaleMatrix,
                                                                                                                                   centeredFlattenedVertices);
                const DiffDataMatrix<T, 3,
                                     -1> scaledAndCenteredVertices =
                    DiffDataMatrix<T, 3, -1>(3, (int)(targetTeethVertices[i].cols()), std::move(scaledAndCenteredFlattenedVertices));

                const DiffDataAffine<T, 3, 3> diffAffineComp = diffFromCenterOfGravity.Multiply(diffAffine);
                const DiffDataMatrix<T, 3, -1> transformedTeethVertices = diffAffineComp.Transform(scaledAndCenteredVertices);
                const DiffDataMatrix<T, 3, -1> interfaceDelta = diffTargetHeadVertices[i] - transformedTeethVertices;

                DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(interfaceDelta,
                                                                                    targetDeltas[i],
                                                                                    weights[i],
                                                                                    T(1));

                cost.Add(std::move(residual), p2pWeight);
            }

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    if (solver.Solve(evaluationFunction, numIterations))
    {
        const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);
    }
    else
    {
        LOG_ERROR("could not solve optimization problem");
    }

    const T scaleVal = scaleVariable.Value()[0];
    Affine<T, 3,
           3> affineVal = Affine<T, 3, 3>::FromTranslation(centerOfGravity) * affineVariable.Affine() * Affine<T, 3, 3>::FromTranslation(
        scaleVal * (-centerOfGravity));
    affineVal.SetTranslation(affineVal.Translation() * (T(1) / scaleVal));

    return std::pair<T, Affine<T, 3, 3>>(scaleVal, affineVal);
}

// explicitly instantiate the face fitting classes
template class TeethAlignment<float>;
template class TeethAlignment<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
