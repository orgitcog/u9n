// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <tuple>
#include <carbon/io/JsonIO.h>
#include <nls/geometry/DualQuaternion.h>
#include <nls/geometry/DualQuaternionVariable.h>
#include <nls/geometry/Procrustes.h>
#include <nls/Cost.h>
#include <nls/functions/AddFunction.h>
#include <nls/functions/ScaleFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <carbon/utils/Timer.h>
#include <carbon/utils/TaskThreadPool.h>

//! Stabilization alpha parameters, each tuple containing alphaPos and alphaVel for one stabilization cycle
CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::mps)

const std::vector<std::tuple<float, float>> stabilizationCyclesAlphas { std::tuple<float, float>(0.8f, 0.2f),
                                                                        std::tuple<float, float>(0.4f, 0.1f),
                                                                        std::tuple<float, float>(0.2f, 0.05f),
                                                                        std::tuple<float, float>(0.1f, 0.025f),
                                                                        std::tuple<float, float>(0.05f, 0.0125f),
                                                                        std::tuple<float, float>(0.025f, 0.00625f),
                                                                        std::tuple<float, float>(0.015f, 0.00375f) };

template <class T>
int sgn(T val) { return (T(0) < val) - (val < T(0)); }

template <class T>
T PolyPenalty(const T& x)
{
    const T absX = std::abs(x);
    if (absX <= T(0.5))
    {
        return std::sqrt(T(2)) * absX;
    }
    else if (absX <= T(1))
    {
        return std::sqrt(-T(2) * x * x + T(4) * absX - T(1));
    }
    return T(1);
}

template <class T>
T PolyPenaltyDerivative(const T& x)
{
    const T absX = std::abs(x);
    if (absX <= T(0.5))
    {
        return std::sqrt(T(2)) * sgn<T>(x);
    }
    else if (absX < T(1))
    {
        return T(2) * (sgn<T>(x) - x) / PolyPenalty<T>(x);
    }
    return T(0);
}

template <class T>
T Sigmoid(const T& x) { return T(1) / (T(1) + std::exp(-x)); }

template <class T>
T SigmoidDerivative(const T& x) { return std::exp(-x) * Sigmoid(x) * Sigmoid(x); }

const double s = 3.5;

template <class T>
T SigmoidPenalty(const T& x) { return Sigmoid(T(s) * x - 1) + Sigmoid(-T(s) * x - 1); }

template <class T>
T SigmoidPenaltyDerivative(const T& x) { return T(s) * SigmoidDerivative(T(s) * x - T(1)) - T(s) * (SigmoidDerivative(-T(s) * x - T(1))); }

template <class T>
T PenaltyFunction(const T& x)
{
    return PolyPenalty(x);
    // return SigmoidPenalty(x);
}

template <class T>
T PenaltyFunctionDerivative(const T& x)
{
    return PolyPenaltyDerivative(x);
    // return SigmoidPenaltyDerivative(x);
}

template <class T>
DiffDataMatrix<T, 3, -1> ModePursuitPenalty(const DiffDataMatrix<T, 3, -1>& x, const T alpha)
{
    const int nVertices = x.Cols();
    const int nTerms = 3 * nVertices;
    const T oneOverAlpha = T(1) / alpha;
    const Vector<T>& xv = x.Value();

    Vector<T> value(nTerms);
    for (int i = 0; i < nTerms; ++i)
    {
        value(i) = PenaltyFunction<T>(xv(i) * oneOverAlpha);
    }

    static Timer timer;
    // static int counter = 0;

    JacobianConstPtr<T> jacobian;
    if (x.HasJacobian() && (x.Jacobian().NonZeros() > 0))
    {
        std::vector<Eigen::Triplet<T>> triplets;
        triplets.reserve(nTerms);
        for (int i = 0; i < nTerms; ++i)
        {
            const T jElement = PenaltyFunctionDerivative<T>(xv(i) * oneOverAlpha) * oneOverAlpha;
            triplets.push_back(Eigen::Triplet(i, i, jElement));
        }

        SparseMatrix<T> lossJacobian(nTerms, nTerms);
        lossJacobian.setFromTriplets(triplets.begin(), triplets.end());

        timer.Restart();
        jacobian = x.Jacobian().Premultiply(lossJacobian);
    }

    return DiffDataMatrix<T, 3, -1>(3, nVertices, DiffData<T>(value, jacobian));
}

template <class T>
std::vector<DiffData<T>> CubicCatmullRomSegment(const DiffData<T>& a, const DiffData<T>& b, const DiffData<T>& c, const DiffData<T>& d, const int nInterval)
{
    std::vector<T> tt(nInterval);
    const T oneOverNInterval = T(1) / T(nInterval);
    for (int i = 0; i < nInterval; ++i)
    {
        tt[i] = T(i) * oneOverNInterval;
    }

    std::vector<DiffData<T>> splineSegment;
    for (int i = 0; i < nInterval; ++i)
    {
        const T t = tt[i];
        const T wA = T(0.5) * ((T(2) - t) * t - T(1)) * t;
        const T wB = T(0.5) * ((T(3) * t - T(5)) * t * t + T(2));
        const T wC = T(0.5) * (((T(4) - T(3) * t) * t + T(1)) * t);
        const T wD = T(0.5) * (t - T(1)) * t * t;
        splineSegment.push_back(wA * a + wB * b + wC * c + wD * d);
    }

    return splineSegment;
}

//! Calculates a cubic catmul rom from the input control points. The input vector of control points is moved from and is in an unknown state
template <class T>
std::vector<DiffData<T>> CubicCatmullRom(std::vector<DiffData<T>>&& controlPoints, const int nInterval)
{
    if (nInterval == 1)
    {
        return std::move(controlPoints);
    }

    const int nControlPoints = static_cast<int>(controlPoints.size());

    CARBON_ASSERT(nInterval > 1, "At least two points per segment required to calculate CubicCatmulRom");
    CARBON_ASSERT(nControlPoints >= 2, "Need at least 2 control points for spline");
    CARBON_ASSERT(nInterval > 0, "Spline nInterval must be positive");

    DiffData<T> p0 = controlPoints[0] + controlPoints[0] - controlPoints[1];
    DiffData<T> pF = controlPoints[nControlPoints - 1] + controlPoints[nControlPoints - 1] - controlPoints[nControlPoints - 2];

    std::vector<DiffData<T>> controlPointsExtended;
    controlPointsExtended.emplace_back(std::move(p0));
    controlPointsExtended.insert(controlPointsExtended.end(), std::make_move_iterator(controlPoints.begin()), std::make_move_iterator(controlPoints.end()));
    controlPointsExtended.emplace_back(std::move(pF));

    std::vector<DiffData<T>> spline;
    for (int i = 0; i < int(controlPointsExtended.size()) - 3; ++i)
    {
        std::vector<DiffData<T>> splineSegment = CubicCatmullRomSegment(controlPointsExtended[i],
                                                                        controlPointsExtended[i + 1],
                                                                        controlPointsExtended[i + 2],
                                                                        controlPointsExtended[i + 3],
                                                                        nInterval);
        spline.insert(spline.end(), std::make_move_iterator(splineSegment.begin()), std::make_move_iterator(splineSegment.end()));
    }
    spline.emplace_back(std::move(controlPointsExtended[controlPointsExtended.size() - 2]));

    return spline;
}

template <class T>
std::vector<DiffDataMatrix<T, 3, -1>> CreateDDMVector(const int size)
{
    std::vector<DiffDataMatrix<T, 3, -1>> result;
    for (int i = 0; i < size; ++i)
    {
        result.push_back(DiffDataMatrix<T, 3, -1>(3, 1, DiffData<T>(Vector<T>(3))));
    }
    return result;
}

template <class T>
std::vector<Vector<T>> Stabilize(const Eigen::Matrix<T, 3, -1>& neutral,
                                 const std::vector<Eigen::Matrix<T, 3, -1>>& scene,
                                 std::vector<Vector<T>> dqs_neutralToScene,
                                 const int nCycles,
                                 const int nInterval,
                                 const int nIterations)
{
    const int cappedNCycles = std::min(nCycles, (int)stabilizationCyclesAlphas.size());
    const std::vector<std::tuple<float, float>> alphas(stabilizationCyclesAlphas.cbegin(), stabilizationCyclesAlphas.cbegin() + cappedNCycles);
    const int nFrames = static_cast<int>(scene.size());
    const int nVertices = static_cast<int>(neutral.cols());
    CARBON_ASSERT(int(dqs_neutralToScene.size()) == nFrames, "Number of initial dqs must match scene size");

    // Center the neutral
    const Eigen::Vector3<T> centerOfNeutral = neutral.rowwise().mean();
    const Vector<T> dq_neutralToNeutralCentered = TranslationVectorToDualQuaternion<T>(-centerOfNeutral);
    const Vector<T> dq_neutralCenteredToNeutral = TranslationVectorToDualQuaternion<T>(centerOfNeutral);
    const Eigen::Matrix<T, 3, -1> neutralCentered = DualQuaternionShapeTransform<T>(neutral, dq_neutralToNeutralCentered);

    // Needed for convenience
    Vector<T> neutralCentered_V = Eigen::Map<const Vector<T>>((const T*)neutralCentered.data(), neutralCentered.size());
    DiffDataMatrix<T, 3, -1> neutralCentered_DDM(3, nVertices, DiffData<T>(neutralCentered_V));

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);

    Timer timer;

    // Optimization cycles
    for (int c = 0; c < nCycles; ++c)
    {
        const T alphaPos = std::get<0>(alphas[c]);
        const T alphaVel = std::get<1>(alphas[c]);

        LOG_INFO("Stabilization cycle {}/{}", c + 1, nCycles);

        const int nControlDqs = int(std::ceil(T(nFrames - 1) / nInterval)) + 1;

        // Extend the scene and initial dqs with extra frames for spline consistency
        const int nFramesExtended = (nControlDqs - 1) * nInterval + 1;
        const int nExtraFrames = nFramesExtended - nFrames;
        std::vector<Eigen::Matrix<T, 3, -1>> scene_ext = scene;
        std::vector<Vector<T>> dqs_neutralToScene_ext = dqs_neutralToScene;
        for (int i = 0; i < nExtraFrames; ++i)
        {
            scene_ext.push_back(scene.back());
            dqs_neutralToScene_ext.push_back(dqs_neutralToScene.back());
        }

        // Create variables for control dqs and initialize them
        std::vector<DualQuaternionVariable<T>> controlDqsVar_neutralCenteredToScene;
        for (int i = 0; i < nControlDqs; ++i)
        {
            const int frame = i * nInterval;
            const Vector<T> controlDq_neutralCenteredToScene =
                DualQuaternionMultiplication<T, false>(dqs_neutralToScene_ext[frame], dq_neutralCenteredToNeutral);
            controlDqsVar_neutralCenteredToScene.push_back(DualQuaternionVariable<T>(controlDq_neutralCenteredToScene));
        }

        auto EvaluateSpline = [&](Context<T>* context) {
                std::vector<DiffData<T>> dqsDD_neutralCenteredToScene_ext;

                // Create DiffDatas for control dqs
                std::vector<DiffData<T>> controlDqsDD_neutralCenteredToScene;
                for (auto& var : controlDqsVar_neutralCenteredToScene)
                {
                    controlDqsDD_neutralCenteredToScene.push_back(var.Evaluate(context));
                }

                // Calculate spline of DiffData dqs
                dqsDD_neutralCenteredToScene_ext = CubicCatmullRom<T>(std::move(controlDqsDD_neutralCenteredToScene), nInterval);
                CARBON_ASSERT(int(dqsDD_neutralCenteredToScene_ext.size()) == nFramesExtended, "Spline has the wrong size");

                return dqsDD_neutralCenteredToScene_ext;
            };

        std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
                const std::vector<DiffData<T>> dqsDD_neutralCenteredToScene_ext = EvaluateSpline(context);

                // Align shapes to neutral using current dqs
                std::vector<DiffDataMatrix<T, 3, -1>> sceneStabilizedDDM_ext = CreateDDMVector<T>(nFramesExtended);

                taskThreadPool->AddTaskRangeAndWait(nFramesExtended, [&](int start, int end) {
                    for (int i = start; i < end; ++i)
                    {
                        const DiffData<T> dqDD_sceneToNeutralCentered = DualQuaternionQuatConjugateDiff<T>(dqsDD_neutralCenteredToScene_ext[i]);
                        sceneStabilizedDDM_ext[i] = DualQuaternionShapeTransformDiff(dqDD_sceneToNeutralCentered, scene_ext[i]);
                    }
                });

                Cost<T> cost;

                std::vector<DiffDataMatrix<T, 3, -1>> posLosses = CreateDDMVector<T>(nFramesExtended);
                std::vector<DiffDataMatrix<T, 3, -1>> velLosses = CreateDDMVector<T>(nFramesExtended - 1);

                // Position loss
                taskThreadPool->AddTaskRangeAndWait(nFramesExtended, [&](int start, int end) {
                    for (int i = start; i < end; ++i)
                    {
                        DiffDataMatrix<T, 3, -1> posDiff = sceneStabilizedDDM_ext[i] - neutralCentered_DDM;
                        posLosses[i] = ModePursuitPenalty<T>(posDiff, alphaPos);
                    }
                });

                // Velocity loss
                taskThreadPool->AddTaskRangeAndWait(nFramesExtended - 1, [&](int start, int end) {
                    for (int i = start; i < end; ++i)
                    {
                        DiffDataMatrix<T, 3, -1> pointVel = sceneStabilizedDDM_ext[i + 1] - sceneStabilizedDDM_ext[i];
                        velLosses[i] = ModePursuitPenalty<T>(pointVel, alphaVel);
                    }
                });

                for (auto&& ddm : posLosses)
                {
                    cost.Add(std::move(ddm), T(1));
                }
                for (auto&& ddm : velLosses)
                {
                    cost.Add(std::move(ddm), T(1));
                }

                return cost.CostToDiffData();
            };

        // Solve
        const T startEnergy = T(0.5) * evaluationFunction(nullptr).Value().squaredNorm();
        GaussNewtonSolver<T> solver;
        if (!solver.Solve(evaluationFunction, nIterations))
        {
            LOG_WARNING("could not solve optimization problem");
            break;
        }
        const T finalEnergy = T(0.5) * evaluationFunction(nullptr).Value().squaredNorm();
        LOG_INFO("energy changed from {} to {}", startEnergy, finalEnergy);

        // Recalculate spline and save it to dqs_neutralToScene for next cycle or returning
        const std::vector<DiffData<T>> dqsDD_neutralCenteredToScene_ext = EvaluateSpline(nullptr);

        for (int i = 0; i < nFrames; ++i)
        {
            const Vector<T> dq_neutralCenteredToScene = dqsDD_neutralCenteredToScene_ext[i].Value();
            dqs_neutralToScene[i] = DualQuaternionMultiplication<T, false>(dq_neutralCenteredToScene, dq_neutralToNeutralCentered);
        }
    }

    LOG_INFO("Stabilization time: {} seconds",
             std::ceil(timer.Current() / 1000.0));

    return dqs_neutralToScene;
}

template <class T>
std::tuple<std::vector<Eigen::Matrix<float, 5, -1>>, std::vector<Eigen::Matrix<float, 5, -1>>> GenerateLossMapsData(const Eigen::Matrix<T, 3, -1>& model,
                                                                                                                    const Eigen::Matrix<T, 3, -1>& neutral,
                                                                                                                    const std::vector<Eigen::Matrix<T, 3,
                                                                                                                                                    -1>>& scene,
                                                                                                                    const std::vector<Vector<T>>& dqs_sceneToNeutral,
                                                                                                                    const int nCycles,
                                                                                                                    const int resU,
                                                                                                                    const int resV)
{
    // Maps image params
    const float colorMin = 30.0;
    const float colorMax = 255.0;
    const float xMin = -10.0f;
    const float xMax = 10.0f;
    const float yMin = -12.5f;
    const float yMax = 12.5f;

    const int cappedNCycles = std::min(nCycles, (int)stabilizationCyclesAlphas.size());
    const std::tuple<float, float> alphasLastCycle = stabilizationCyclesAlphas[cappedNCycles - 1];
    const T alphaPos = std::get<0>(alphasLastCycle);
    const T alphaVel = std::get<1>(alphasLastCycle);

    CARBON_ASSERT(scene.size() == dqs_sceneToNeutral.size(), "There must be one transform per frame");
    const int nFrames = scene.size();
    const int nVertices = static_cast<int>(neutral.cols());

    // Stabilize scene to pose of neutral. Create Diff Datas to use the functions for MPS loss
    std::vector<DiffDataMatrix<T, 3, -1>> sceneStabilizedDDM;
    for (int i = 0; i < nFrames; ++i)
    {
        const Eigen::Matrix<T, 3, -1> stabilizedFrame = DualQuaternionShapeTransform(scene[i], dqs_sceneToNeutral[i]);
        const Vector<T> vectorData = Eigen::Map<const Vector<T>>((const T*)stabilizedFrame.data(), stabilizedFrame.size());
        sceneStabilizedDDM.push_back(DiffDataMatrix<T, 3, -1>(3, nVertices, DiffData<T>(vectorData)));
    }

    // Create diff data of neutral
    const Vector<T> neutralVectorData = Eigen::Map<const Vector<T>>((const T*)neutral.data(), neutral.size());
    const DiffDataMatrix<T, 3, -1> neutralDDM(3, nVertices, DiffData<T>(neutralVectorData));

    // Position losses
    std::vector<DiffDataMatrix<T, 3, -1>> posLosses = CreateDDMVector<T>(nFrames);
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
    taskThreadPool->AddTaskRangeAndWait(nFrames, [&](int start, int end) {
            for (int i = start; i < end; ++i)
            {
                DiffDataMatrix<T, 3, -1> posDiff = sceneStabilizedDDM[i] - neutralDDM;
                posLosses[i] = ModePursuitPenalty<T>(posDiff, alphaPos);
            }
        });

    // Velocity losses
    std::vector<DiffDataMatrix<T, 3, -1>> velLosses = CreateDDMVector<T>(nFrames - 1);
    taskThreadPool->AddTaskRangeAndWait(nFrames - 1, [&](int start, int end) {
            for (int i = start; i < end; ++i)
            {
                DiffDataMatrix<T, 3, -1> pointVel = sceneStabilizedDDM[i + 1] - sceneStabilizedDDM[i];
                velLosses[i] = ModePursuitPenalty<T>(pointVel, alphaVel);
            }
        });

    // Center the model
    const Eigen::Vector3<T> centerOfModel = model.rowwise().mean();
    const Vector<T> dq_modelToModelCentered = TranslationVectorToDualQuaternion<T>(-centerOfModel);
    const Eigen::Matrix<T, 3, -1> modelCentered = DualQuaternionShapeTransform<T>(model, dq_modelToModelCentered);

    // Create position loss maps
    std::vector<Eigen::Matrix<float, 5, -1>> posLossMapsData(nFrames);
    taskThreadPool->AddTaskRangeAndWait(nFrames, [&](int start, int end) {
            for (int frame = start; frame < end; ++frame)
            {
                Eigen::Matrix<float, 5, -1>& mapData = posLossMapsData[frame];
                mapData = Eigen::Matrix<float, 5, -1>(5, nVertices);
                const Vector<T>& posLossVec = posLosses[frame].Value();
                for (int vertex = 0; vertex < nVertices; ++vertex)
                {
                    const float x = modelCentered(0, vertex);
                    const float y = modelCentered(1, vertex);

                    mapData(0, vertex) = (x - xMin) / (xMax - xMin) * resU; // u
                    mapData(1, vertex) = (yMax - y) / (yMax - yMin) * resV; // v

                    const float valueB = posLossVec(3 * vertex + 2);
                    const float valueG = posLossVec(3 * vertex + 1);
                    const float valueR = posLossVec(3 * vertex);

                    mapData(2, vertex) = colorMin + (1.0 - valueB) * (colorMax - colorMin); // B
                    mapData(3, vertex) = colorMin + (1.0 - valueG) * (colorMax - colorMin); // G
                    mapData(4, vertex) = colorMin + (1.0 - valueR) * (colorMax - colorMin); // R
                }
            }
        });

    // Create velocity loss maps
    std::vector<Eigen::Matrix<float, 5, -1>> velLossMapsData(nFrames);
    velLossMapsData[0] = Eigen::Matrix<float, 5, -1>(5, 0); // First frame has empty vel loss map (vel calculated with backward derivatives)
    taskThreadPool->AddTaskRangeAndWait(nFrames - 1, [&](int start, int end) {
            for (int frame = start; frame < end; ++frame)
            {
                Eigen::Matrix<float, 5, -1>& mapData = velLossMapsData[frame];
                mapData = Eigen::Matrix<float, 5, -1>(5, nVertices);
                const Vector<T>& velLossVec = velLosses[frame].Value();
                for (int vertex = 0; vertex < nVertices; ++vertex)
                {
                    const float x = modelCentered(0, vertex);
                    const float y = modelCentered(1, vertex);

                    mapData(0, vertex) = (x - xMin) / (xMax - xMin) * resU; // u
                    mapData(1, vertex) = (yMax - y) / (yMax - yMin) * resV; // v

                    const float valueB = velLossVec(3 * vertex + 2);
                    const float valueG = velLossVec(3 * vertex + 1);
                    const float valueR = velLossVec(3 * vertex);

                    mapData(2, vertex) = colorMin + (1.0 - valueB) * (colorMax - colorMin); // B
                    mapData(3, vertex) = colorMin + (1.0 - valueG) * (colorMax - colorMin); // G
                    mapData(4, vertex) = colorMin + (1.0 - valueR) * (colorMax - colorMin); // R
                }
            }
        });

    return std::make_tuple(posLossMapsData, velLossMapsData);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::mps)
