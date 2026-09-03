// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/Cost.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/MetaShapeCamera.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/solver/LMSolver.h>
#include <nls/DiffScalar.h>

#include <calib/Calibration.h>
#include <calib/Utilities.h>
#include <calib/BundleAdjustmentPerformer.h>

#include <vector>
#include <iostream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

Affine<real_t, 3, 3> Eigen2Affine(const Eigen::Matrix3<real_t>& rot, const Eigen::Vector3<real_t>& tvec)
{
    Affine<real_t, 3, 3> transform;
    transform.SetLinear(rot);
    transform.SetTranslation(tvec);
    return transform;
}

template <class T>
class DiffDataCamera
{
public:
    DiffDataCamera(const DiffDataAffine<T, 3, 3>& diffDataAffine, const DiffData<T>& K, const DiffData<T>& D) : m_diffDataAffine(
            diffDataAffine.Clone()), m_diffIntrinsics(K.Clone()), m_diffDistortion(D.Clone())
    {}

    DiffDataMatrix<T, 2, -1> Project(const DiffDataMatrix<T, 3, -1>& points) const
    {
        // transform 3D points into camera coordinate system
        DiffDataMatrix<T, 3, -1> pointsInCameraSpace = m_diffDataAffine.Transform(points);

        const size_t numVertices = pointsInCameraSpace.Cols();
        std::vector<DiffScalar<T>> diffScalars;
        diffScalars.reserve(numVertices * size_t(2));

        for (int i = 0; i < (int)numVertices; ++i)
        {
            DiffScalar<T> vx = ExtractScalar<T>(pointsInCameraSpace, 3 * i + 0);
            DiffScalar<T> vy = ExtractScalar<T>(pointsInCameraSpace, 3 * i + 1);
            DiffScalar<T> vz = ExtractScalar<T>(pointsInCameraSpace, 3 * i + 2);

            // undistort in camera space
            DiffScalar<T> x = vx / vz;
            DiffScalar<T> y = vy / vz;
            DiffScalar<T> xx = x * x;
            DiffScalar<T> xy = x * y;
            DiffScalar<T> yy = y * y;
            DiffScalar<T> r2 = xx + yy;
            DiffScalar<T> r4 = r2 * r2;
            DiffScalar<T> r6 = r4 * r2;
            DiffScalar<T> K1 = ExtractScalar<T>(m_diffDistortion, 0);
            DiffScalar<T> K2 = ExtractScalar<T>(m_diffDistortion, 1);
            DiffScalar<T> K3 = ExtractScalar<T>(m_diffDistortion, 4);
            DiffScalar<T> radial = (T(1) + K1 * r2 + K2 * r4 + K3 * r6);
            DiffScalar<T> P1 = ExtractScalar<T>(m_diffDistortion, 2);
            DiffScalar<T> P2 = ExtractScalar<T>(m_diffDistortion, 3);
            DiffScalar<T> tangentialX = P1 * (r2 + T(2) * xx) + T(2) * P2 * xy;
            DiffScalar<T> tangentialY = P2 * (r2 + T(2) * yy) + T(2) * P1 * xy;
            DiffScalar<T> xdash = x * radial + tangentialX;
            DiffScalar<T> ydash = y * radial + tangentialY;

            // project to image space (pixels)
            // TO DO: create a proper way to fix focal aspect ratio
            // DiffScalar<T> fx = ExtractScalar<T>(m_diffIntrinsics, 0);
            DiffScalar<T> f = ExtractScalar<T>(m_diffIntrinsics, 0);
            DiffScalar<T> cx = ExtractScalar<T>(m_diffIntrinsics, 1);
            DiffScalar<T> cy = ExtractScalar<T>(m_diffIntrinsics, 2);
            DiffScalar<T> u = f * xdash + cx;
            DiffScalar<T> v = f * ydash + cy;
            diffScalars.emplace_back(u);
            diffScalars.emplace_back(v);
        }
        return DiffDataMatrix<T, 2, -1>(2, static_cast<int>(numVertices), AssembleDiffData<T>(diffScalars));
    }

private:
    DiffDataAffine<T, 3, 3> m_diffDataAffine;
    DiffData<T> m_diffIntrinsics;
    DiffData<T> m_diffDistortion;
};

template <class T>
class CameraVariable
{
public:
    void SetCamera(const Eigen::Matrix3<T>& K, const Eigen::VectorX<T>& D, const Eigen::Matrix3<T>& R, const Eigen::Vector3<T>& t)
    {
        Eigen::Vector3<T> Kvec;
        // use fixed focal aspect ratio
        Kvec << K(0, 0), K(0, 2), K(1, 2);

        m_intrinsics = new VectorVariable<T>(Kvec);
        m_distortion = new VectorVariable<T>(D);
        m_varAffine.SetAffine(Eigen2Affine(R, t));
    }

    Eigen::Matrix3<T> getIntrinsics()
    {
        auto Kvec = m_intrinsics->Value();
        Eigen::Matrix3<T> K;
        K << Kvec(0), T(0), Kvec(1),
        T(0), Kvec(0), Kvec(2),
        T(0), T(0), T(1);
        return K;
    }

    Eigen::VectorX<T> getDistortion() { return m_distortion->Value(); }

    Eigen::Matrix3<T> getRotation() { return m_varAffine.Affine().Linear(); }

    Eigen::Vector3<T> getTranslation() { return m_varAffine.Affine().Translation(); }

    DiffDataCamera<T> Evaluate(Context<T>* context)
    {
        return DiffDataCamera<T>(m_varAffine.EvaluateAffine(context),
                                 m_intrinsics->Evaluate(context),
                                 m_distortion->Evaluate(context));
    }

    void IndividualConstantIndices(const std::vector<int>& intrConst, const std::vector<int>& distConst)
    {
        m_intrinsics->MakeIndividualIndicesConstant(intrConst);
        m_distortion->MakeIndividualIndicesConstant(distConst);
    }

    void MakeConstant()
    {
        m_varAffine.MakeConstant(true, true);
        m_distortion->MakeConstant();
        m_intrinsics->MakeConstant();
    }

    void FixIntrinsics() { m_intrinsics->MakeConstant(); }

    void FixDistortion() { m_distortion->MakeConstant(); }

private:
    VectorVariable<T>* m_intrinsics;
    VectorVariable<T>* m_distortion;

    AffineVariable<QuaternionVariable<T>> m_varAffine;
};

real_t calculateReprojectionError(const Eigen::MatrixX<real_t>& points,
                                  const std::vector<Eigen::Matrix4<real_t>>& objTransforms,
                                  const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                                  const std::vector<std::vector<bool>>& visibility,
                                  const std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                                  const std::vector<Eigen::Matrix4<real_t>>& transform,
                                  const std::vector<Eigen::VectorX<real_t>>& distCoeffs)
{
    const size_t numFrames = objTransforms.size();
    const size_t numCameras = cameraMatrix.size();
    real_t globalMse = 0;

    for (size_t i = 0; i < numCameras; i++)
    {
        for (size_t j = 0; j < numFrames; j++)
        {
            if (!visibility[i][j])
            {
                continue;
            }
            Eigen::MatrixX<real_t> curr3d = points;
            Eigen::MatrixX<real_t> curr2d = imagePoints[i][j];

            transformPoints(curr3d, objTransforms[j]);

            Eigen::MatrixX<real_t> reproj2d;
            projectPointsOnImagePlane(curr3d, cameraMatrix[i], distCoeffs[i], transform[i], reproj2d);
            real_t localMse = calculateMeanSquaredError(curr2d, reproj2d).value();
            globalMse += localMse;
        }
    }

    return sqrt(globalMse / (real_t(numFrames) * real_t(numCameras)));
}

real_t fbCalculateReprojectionError(const Eigen::MatrixX<real_t>& points,
                                    const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                                    const std::vector<std::vector<std::vector<bool>>>& visibility,
                                    const std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                                    const std::vector<Eigen::Matrix4<real_t>>& transform,
                                    const std::vector<Eigen::VectorX<real_t>>& distCoeffs)
{
    const size_t numFrames = imagePoints[0].size();
    const size_t numCameras = cameraMatrix.size();
    real_t globalMse = 0;
    real_t frameMse = 0;
    for (size_t i = 0; i < numCameras; i++)
    {
        for (size_t j = 0; j < numFrames; j++)
        {
            size_t numPoints = imagePoints[i][j].rows();
            std::cout << numPoints << std::endl;
            for (size_t k = 0; k < numPoints; k++)
            {
                if (visibility[i][j][k] != 0)
                {
                    Eigen::Vector3<real_t> curr3d = points.row(k);
                    Eigen::Vector2<real_t> curr2d = imagePoints[i][j].row(k);
                    Eigen::MatrixX<real_t> reproj2d;
                    reproj2d = projectPointOnImagePlane(curr3d, cameraMatrix[i], distCoeffs[i], transform[i]);
                    real_t localMse = (curr2d(0) - reproj2d(0)) * (curr2d(0) - reproj2d(0)) + (curr2d(1) - reproj2d(1)) *
                        (curr2d(1) - reproj2d(1));
                    frameMse += localMse;
                }
            }
            globalMse += frameMse / numPoints;
        }
    }
    return sqrt(globalMse / (real_t(numFrames) * real_t(numCameras)));
}

std::optional<real_t> bundleAdjustment(const Eigen::MatrixX<real_t>& points,
                                       std::vector<Eigen::Matrix4<real_t>>& objTransform,
                                       const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                                       const std::vector<std::vector<bool>>& visibility,
                                       std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                                       std::vector<Eigen::Matrix4<real_t>>& cameraTransform,
                                       std::vector<Eigen::VectorX<real_t>>& distCoeffs,
                                       const BAParams& params)
{
    CARBON_ASSERT((cameraMatrix.size() == cameraTransform.size()) && (cameraTransform.size() == distCoeffs.size()) && (distCoeffs.size() == visibility.size()),
                  "Error in input size vectors R,T... in bundleAdjustment");


    for (size_t i = 0; i < cameraMatrix.size(); i++)
    {
        CARBON_ASSERT(distCoeffs[i].size() == 5, "Error in distortion coeff (must be 1x5 or 5x1) in bundleAdjustment");
    }

    const size_t frameCount = objTransform.size();
    const size_t cameraCount = cameraTransform.size();
    std::vector<AffineVariable<QuaternionVariable<real_t>>> object2worldTransformsVariables(frameCount);

    for (size_t frameNumber = 0; frameNumber < frameCount; frameNumber++)
    {
        Eigen::Matrix3<real_t> objRot;
        Eigen::Vector3<real_t> objTrans;
        splitRotationAndTranslation(objTransform[frameNumber], objRot, objTrans);

        Affine<real_t, 3, 3> currentObjectTransform = Eigen2Affine(objRot, objTrans);
        object2worldTransformsVariables[frameNumber].SetAffine(currentObjectTransform);
    }

    std::vector<CameraVariable<real_t>> cameraVariables(cameraCount);

    for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
    {
        Eigen::Matrix3<real_t> cameraRot;
        Eigen::Vector3<real_t> cameraTrans;
        splitRotationAndTranslation(cameraTransform[cameraNumber], cameraRot, cameraTrans);
        cameraVariables[cameraNumber].SetCamera(cameraMatrix[cameraNumber], distCoeffs[cameraNumber], cameraRot, cameraTrans);
        cameraVariables[cameraNumber].IndividualConstantIndices(params.fixedIntrinsicIndices, params.fixedDistortionIndices);
        if (!params.optimizeIntrinsics)
        {
            cameraVariables[cameraNumber].FixIntrinsics();
        }
        if (!params.optimizeDistortion)
        {
            cameraVariables[cameraNumber].FixDistortion();
        }
    }

    DiffDataMatrix<real_t, 3, -1> diffObjectPoints(Eigen::Map<const Eigen::Matrix<real_t, 3, -1, Eigen::RowMajor>>(
                                                       (const real_t*)points.data(),
                                                       3,
                                                       points.rows()).template cast<real_t>());

    std::function<DiffData<real_t>(Context<real_t>*)> evaluationFunction =
        [&](Context<real_t>* context) {
            Cost<real_t> cost;
            for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
            {
                CameraVariable<real_t>& cameraVariable = cameraVariables[cameraNumber];
                DiffDataCamera<real_t> diffCamera = cameraVariable.Evaluate(context);

                for (size_t frameNumber = 0; frameNumber < frameCount; frameNumber++)
                {
                    /* *INDENT-OFF* */
                    DiffDataAffine<real_t, 3, 3> diffObject2WorldTransform =
                        object2worldTransformsVariables[frameNumber].EvaluateAffine(context);

                    DiffDataMatrix<real_t, 3, -1> diffObjectPointsTransformedToWorld =
                        diffObject2WorldTransform.Transform(diffObjectPoints);

                    auto currentImagePoints = Eigen::Map<const Eigen::Matrix<real_t, 2, -1, Eigen::RowMajor>>(
                                                        (const real_t*)imagePoints[cameraNumber][frameNumber].data(),
                                                        2,
                                                        points.rows()).template cast<real_t>();

                    /* *INDENT-ON* */
                    DiffDataMatrix<real_t, 2, -1> diffProjection = diffCamera.Project(
                        diffObjectPointsTransformedToWorld);

                    if (visibility[cameraNumber][frameNumber])
                    {
                        Eigen::VectorX<real_t> weights = Eigen::VectorX<real_t>::Ones(currentImagePoints.cols());
                        cost.Add(PointPointConstraintFunction<real_t, 2>::Evaluate(diffProjection,
                                                                                   currentImagePoints,
                                                                                   weights,
                                                                                   real_t(1)),
                                 real_t(1));
                    }
                }
            }

            return cost.CostToDiffData();
        };

    GaussNewtonSolver<real_t> solver;
    // LMSolver<T> solver;
    DiffData<real_t> initialResult = evaluationFunction(nullptr);
    const real_t startEnergy = initialResult.Value().squaredNorm();

    if (!solver.Solve(evaluationFunction, static_cast<int>(params.iterations)))
    {
        LOG_WARNING("Bundle adjustment error: Cannot solve bundle adjustment problem.");
        return std::nullopt;
    }
    DiffData<real_t> finalResult = evaluationFunction(nullptr);
    const real_t finalEnergy = finalResult.Value().squaredNorm();
    std::cout << startEnergy << " " << finalEnergy << std::endl;
    if (finalEnergy > startEnergy)
    {
        LOG_WARNING("Bundle adjustment error: Final energy larger than starting energy.");
        return std::nullopt;
    }

    for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
    {
        Eigen::Matrix3<real_t> estCamRot = cameraVariables[cameraNumber].getRotation();
        Eigen::Vector3<real_t> estCamTrans = cameraVariables[cameraNumber].getTranslation();
        Eigen::Matrix4<real_t> newCameraTransform = makeTransformationMatrix(estCamRot, estCamTrans);
        Eigen::Matrix3<real_t> newCameraMatrix = cameraVariables[cameraNumber].getIntrinsics();
        Eigen::VectorX<real_t> newDistortionCoeffs = cameraVariables[cameraNumber].getDistortion();
        cameraTransform[cameraNumber] = newCameraTransform;
        cameraMatrix[cameraNumber] = newCameraMatrix;
        distCoeffs[cameraNumber] = newDistortionCoeffs;

        for (size_t frameNumber = 0; frameNumber < frameCount; frameNumber++)
        {
            Affine<real_t, 3, 3> objTransformAffine = object2worldTransformsVariables[frameNumber].Affine();
            Eigen::Matrix4<real_t> newObjectTransform = makeTransformationMatrix(objTransformAffine.Linear(),
                                                                                        objTransformAffine.Translation());
            objTransform[frameNumber] = newObjectTransform;
        }
    }

    return calculateReprojectionError(points, objTransform, imagePoints, visibility, cameraMatrix, cameraTransform, distCoeffs);
}

int countNonZero(const std::vector<bool>& array)
{
    int count = 0;
    for (size_t i = 0; i < array.size(); i++)
    {
        if (array[i] != 0)
        {
            count++;
        }
    }
    return count;
}

void reorganizeByVisibility(Eigen::MatrixX<real_t>& points, Eigen::MatrixX<real_t>& imagePoints, const std::vector<bool>& vis)
{
    int visSize = countNonZero(vis);
    Eigen::MatrixX<real_t> newProjPoints(visSize, 3);
    Eigen::MatrixX<real_t> newImgPoints(visSize, 2);
    int counter = 0;
    for (size_t i = 0; i < vis.size(); i++)
    {
        if (vis[i] == 1)
        {
            newProjPoints.row(counter) = points.row(i);
            newImgPoints.row(counter) = imagePoints.row(i);
            counter++;
        }
    }
    points = newProjPoints;
    imagePoints = newImgPoints;
}

Eigen::VectorX<real_t> VisibilityToWeights(const std::vector<bool>& vis)
{
    Eigen::VectorX<real_t> w = Eigen::VectorX<real_t>::Zero((vis.size()));
    for (size_t i = 0; i < vis.size(); ++i)
    {
        if (vis[i])
        {
            w[i] = 1.0;
        }
    }

    return w;
}

std::optional<real_t> featureBundleAdjustment(Eigen::MatrixX<real_t>& points,
                                              const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                                              const std::vector<std::vector<std::vector<bool>>>& visibility,
                                              std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                                              std::vector<Eigen::Matrix4<real_t>>& cameraTransform,
                                              std::vector<Eigen::VectorX<real_t>>& distCoeffs,
                                              const BAParams& params)
{
    real_t initMse = fbCalculateReprojectionError(points, imagePoints, visibility, cameraMatrix, cameraTransform, distCoeffs);
    std::cout << "initMse: " << initMse << std::endl;

    if ((cameraMatrix.size() != cameraTransform.size()) || (cameraTransform.size() != distCoeffs.size()) ||
        (distCoeffs.size() != visibility.size()))
    {
        CARBON_ASSERT(false, "Error in input size vectors R,T... in bundleAdjustment");
    }

    for (size_t i = 0; i < cameraMatrix.size(); i++)
    {
        CARBON_ASSERT(distCoeffs[i].size() == 5, "Error in distortion coeff  (must be 1x5 or 5x1) in bundleAdjustment");
    }

    const size_t frameCount = params.frameNum;
    const size_t cameraCount = static_cast<size_t>(cameraTransform.size());

    std::vector<CameraVariable<real_t>> cameraVariables(cameraCount);

    for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
    {
        Eigen::Matrix3<real_t> cameraRot;
        Eigen::Vector3<real_t> cameraTrans;
        splitRotationAndTranslation(cameraTransform[cameraNumber], cameraRot, cameraTrans);
        cameraVariables[cameraNumber].SetCamera(cameraMatrix[cameraNumber], distCoeffs[cameraNumber], cameraRot, cameraTrans);
        cameraVariables[cameraNumber].IndividualConstantIndices(params.fixedIntrinsicIndices, params.fixedDistortionIndices);
        if (!params.optimizeIntrinsics)
        {
            cameraVariables[cameraNumber].FixIntrinsics();
        }
        if (!params.optimizeDistortion)
        {
            cameraVariables[cameraNumber].FixDistortion();
        }
    }

    VectorVariable<real_t> pointsVariable(static_cast<int>(points.size()));

    Eigen::Matrix<real_t, 3, -1> objectPoints(Eigen::Map<const Eigen::Matrix<real_t, 3, -1, Eigen::RowMajor>>(
                                                  (const real_t*)points.data(),
                                                  3,
                                                  points.rows()).template cast<real_t>());
    Eigen::VectorX<real_t> pointsVec = Eigen::Map<const Eigen::VectorX<real_t>, Eigen::ColMajor>(objectPoints.data(), points.size());
    pointsVariable.Set(pointsVec);
    if (!params.optimizePoints)
    {
        pointsVariable.MakeConstant();
    }

    std::function<DiffData<real_t>(Context<real_t>*)> evaluationFunction =
        [&](Context<real_t>* context) {
            Cost<real_t> cost;

            DiffDataMatrix<real_t, 3, -1> diffObjectPoints(3, static_cast<int>(points.rows()), pointsVariable.Evaluate(context));

            for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
            {
                CameraVariable<real_t>& cameraVariable = cameraVariables[cameraNumber];
                DiffDataCamera<real_t> diffCamera = cameraVariable.Evaluate(context);

                for (size_t frameNumber = 0; frameNumber < frameCount; frameNumber++)
                {
                    auto weights = VisibilityToWeights(visibility[cameraNumber][frameNumber]);
                    auto currentImagePoints = Eigen::Map<const Eigen::Matrix<real_t, 2, -1, Eigen::RowMajor>>(
                        (const real_t*)imagePoints[cameraNumber][frameNumber].data(),
                        2,
                        points.rows()).template cast<real_t>();

                    DiffDataMatrix<real_t, 2, -1> diffProjection = diffCamera.Project(diffObjectPoints);
                    cost.Add(PointPointConstraintFunction<real_t, 2>::Evaluate(diffProjection,
                                                                               currentImagePoints,
                                                                               weights,
                                                                               real_t(1)),
                             real_t(1));
                }
            }
            return cost.CostToDiffData();
        };

    // GaussNewtonSolver<real_t> solver;
    LMSolver<real_t> solver;
    DiffData<real_t> initialResult = evaluationFunction(nullptr);
    const real_t startEnergy = initialResult.Value().squaredNorm();

    if (!solver.Solve(evaluationFunction, static_cast<int>(params.iterations)))
    {
        LOG_WARNING("Bundle adjustment error: Cannot solve bundle adjustment problem.");
        return std::nullopt;
    }
    DiffData<real_t> finalResult = evaluationFunction(nullptr);
    const real_t finalEnergy = finalResult.Value().squaredNorm();

    if (finalEnergy > startEnergy)
    {
        LOG_WARNING("Bundle adjustment error: Final energy larger than starting energy.");
        return std::nullopt;
    }

    std::cout << "Bundle adjustment start energy: " << startEnergy << std::endl;
    std::cout << "Bundle adjustment final energy: " << finalEnergy << std::endl;

    points = Eigen::Map<const Eigen::Matrix<real_t, -1, 3, Eigen::RowMajor>>(
        (const real_t*)pointsVariable.Value().data(),
        points.rows(),
        3).template cast<real_t>();

    for (size_t cameraNumber = 0; cameraNumber < cameraCount; cameraNumber++)
    {
        const Eigen::Matrix3<real_t> estCamRot = cameraVariables[cameraNumber].getRotation();
        const Eigen::Vector3<real_t> estCamTrans = cameraVariables[cameraNumber].getTranslation();
        const Eigen::Matrix4<real_t> newCameraTransform = makeTransformationMatrix(estCamRot, estCamTrans);
        cameraMatrix[cameraNumber] = cameraVariables[cameraNumber].getIntrinsics();
        distCoeffs[cameraNumber] = cameraVariables[cameraNumber].getDistortion();
        cameraTransform[cameraNumber] = newCameraTransform;
    }

    return fbCalculateReprojectionError(points, imagePoints, visibility, cameraMatrix, cameraTransform, distCoeffs);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
