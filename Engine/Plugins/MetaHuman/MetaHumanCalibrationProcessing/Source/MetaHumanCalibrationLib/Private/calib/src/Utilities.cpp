// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/Utilities.h>

#include <carbon/Common.h>

#include <calib/BeforeOpenCvHeaders.h>
CARBON_DISABLE_EIGEN_WARNINGS
#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
CARBON_RENABLE_WARNINGS
#include <calib/AfterOpenCvHeaders.h>

#define CV_REAL CV_64F

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

Eigen::Vector3<real_t> linearTriangulation(const Eigen::Vector2<real_t>& p2d1,
                                    const Eigen::Vector2<real_t>& p2d2,
                                    const Eigen::Matrix3<real_t>& K1,
                                    const Eigen::Matrix3<real_t>& K2,
                                    const Eigen::Matrix4<real_t>& T1,
                                    const Eigen::Matrix4<real_t>& T2,
                                    real_t w1,
                                    real_t w2)
{
    // From Hartley and Zisserman, Multiple View Geometry, p312
    Eigen::Vector3<real_t> p1h, p2h, ray1, ray2;

    p1h = Eigen::Vector3<real_t>(p2d1(0), p2d1(1), 1.0);
    p2h = Eigen::Vector3<real_t>(p2d2(0), p2d2(1), 1.0);

    ray1 = K1.inverse() * p1h;
    ray2 = K2.inverse() * p2h;


    Eigen::Matrix<real_t, 4, 3> A;
    A << (ray1(0, 0) * T1(2, 0) - T1(0, 0)) / w1, (ray1(0, 0) * T1(2, 1) - T1(0, 1)) / w1,
    (ray1(0, 0) * T1(2, 2) - T1(0, 2)) / w1,
    (ray1(1, 0) * T1(2, 0) - T1(1, 0)) / w1, (ray1(1, 0) * T1(2, 1) - T1(1, 1)) / w1,
    (ray1(1, 0) * T1(2, 2) - T1(1, 2)) / w1,
    (ray2(0, 0) * T2(2, 0) - T2(0, 0)) / w2, (ray2(0, 0) * T2(2, 1) - T2(0, 1)) / w2,
    (ray2(0, 0) * T2(2, 2) - T2(0, 2)) / w2,
    (ray2(1, 0) * T2(2, 0) - T2(1, 0)) / w2, (ray2(1, 0) * T2(2, 1) - T2(1, 1)) / w2,
    (ray2(1, 0) * T2(2, 2) - T2(1, 2)) / w2;

    Eigen::Vector4<real_t> b;
    b << -(ray1(0, 0) * T1(2, 3) - T1(0, 3)) / w1,
    -(ray1(1, 0) * T1(2, 3) - T1(1, 3)) / w1,
    -(ray2(0, 0) * T2(2, 3) - T2(0, 3)) / w2,
    -(ray2(1, 0) * T2(2, 3) - T2(1, 3)) / w2;
    Eigen::Vector3<real_t> X;

    X = (A.transpose() * A).ldlt().solve(A.transpose() * b);

    return X;
}


void splitRotationAndTranslation(const Eigen::Matrix4<real_t>& transformation, Eigen::Matrix3<real_t>& rotation, Eigen::Vector3<real_t>& translation)
{
    translation(0, 0) = transformation(0, 3);
    translation(1, 0) = transformation(1, 3);
    translation(2, 0) = transformation(2, 3);

    for (int j = 0; j < rotation.rows(); j++)
    {
        for (int i = 0; i < rotation.cols(); i++)
        {
            rotation(j, i) = transformation(j, i);
        }
    }
}

void inverseGeometricTransform(Eigen::Matrix4<real_t>& transformation)
{
    Eigen::Matrix3<real_t> R, Ri;
    Eigen::Vector3<real_t> t, ti;
    splitRotationAndTranslation(transformation, R, t);
    Ri = R.transpose();
    ti = (-1.0 * Ri) * t;
    Eigen::MatrixX<real_t> maybeTransform = makeTransformationMatrix(Ri, ti);
    transformation = maybeTransform;
}

bool pointFromRow3dHomogenious(const Eigen::MatrixX<real_t>& matrix, size_t row, Eigen::Vector4<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    if (rowCount < row)
    {
        std::cout << "Input point container dimension is less than input row position." << std::endl;
        return false;
    }

    point(0, 0) = matrix(row, 0);
    point(1, 0) = matrix(row, 1);
    point(2, 0) = matrix(row, 2);
    point(3, 0) = 1.;
    return true;
}

void pointFromRow3d(const Eigen::MatrixX<real_t>& matrix, size_t row, Eigen::Vector3<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    CARBON_ASSERT(rowCount >= row, "Input point container dimension is less than input row position.");

    point(0, 0) = matrix(row, 0);
    point(1, 0) = matrix(row, 1);
    point(2, 0) = matrix(row, 2);
}

void rowFromPoint3d(Eigen::MatrixX<real_t>& matrix, size_t row, const Eigen::VectorX<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    size_t colCount = static_cast<size_t>(matrix.cols());

    CARBON_ASSERT(rowCount >= row && colCount == 3, "Matrix shape is not compatible.");
    CARBON_ASSERT(point.rows() <= 4 && point.cols() == 1, "Point coordinate count is not compatible.");

    matrix(row, 0) = point(0);
    matrix(row, 1) = point(1);
    matrix(row, 2) = point(2);
}

void pointFromRow2dHomogenious(const Eigen::MatrixX<real_t>& matrix, size_t row, Eigen::Vector3<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    CARBON_ASSERT(rowCount >= row, "Input point container dimension is less than input row position.");

    point(0, 0) = matrix(row, 0);
    point(1, 0) = matrix(row, 1);
    point(2, 0) = 1.;
}

void pointFromRow2d(const Eigen::MatrixX<real_t>& matrix, size_t row, Eigen::Vector2<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    CARBON_ASSERT(rowCount >= row, "Input point container dimension is less than input row position.");
    point(0, 0) = matrix(row, 0);
    point(1, 0) = matrix(row, 1);
}

void rowFromPoint2d(Eigen::MatrixX<real_t>& matrix, size_t row, const Eigen::VectorX<real_t>& point)
{
    size_t rowCount = static_cast<size_t>(matrix.rows());
    CARBON_ASSERT((rowCount >= row) && (matrix.cols() == 2), "Matrix shape is not compatible.");

    matrix(row, 0) = point(0, 0);
    matrix(row, 1) = point(1, 0);
}

Eigen::Matrix4<real_t> makeTransformationMatrix(const Eigen::Matrix3<real_t>& rotation, const Eigen::Vector3<real_t>& translation)
{
    Eigen::MatrixX<real_t> transform = Eigen::MatrixX<real_t>(4, 4);

    for (int j = 0; j < rotation.rows(); j++)
    {
        for (int i = 0; i < rotation.cols(); i++)
        {
            transform(j, i) = rotation(j, i);
        }
    }
    transform(0, 3) = translation(0, 0);
    transform(1, 3) = translation(1, 0);
    transform(2, 3) = translation(2, 0);

    transform(3, 0) = 0.;
    transform(3, 1) = 0.;
    transform(3, 2) = 0.;
    transform(3, 3) = 1.;

    return transform;
}

inline real_t SIGN(real_t x) { return (x >= 0.0f) ? +1.0f : -1.0f; }

Eigen::Vector4<real_t> rotationMatrixToQuaternion(const Eigen::Matrix3<real_t>& r)
{
    real_t r11 = r(0, 0);
    real_t r12 = r(0, 1);
    real_t r13 = r(0, 2);
    real_t r21 = r(1, 0);
    real_t r22 = r(1, 1);
    real_t r23 = r(1, 2);
    real_t r31 = r(2, 0);
    real_t r32 = r(2, 1);
    real_t r33 = r(2, 2);

    real_t q0, q1, q2, q3;

    q0 = (r11 + r22 + r33 + 1.0f) / 4.0f;
    q1 = (r11 - r22 - r33 + 1.0f) / 4.0f;
    q2 = (-r11 + r22 - r33 + 1.0f) / 4.0f;
    q3 = (-r11 - r22 + r33 + 1.0f) / 4.0f;
    if (q0 < 0.0f)
    {
        q0 = 0.0f;
    }
    if (q1 < 0.0f)
    {
        q1 = 0.0f;
    }
    if (q2 < 0.0f)
    {
        q2 = 0.0f;
    }
    if (q3 < 0.0f)
    {
        q3 = 0.0f;
    }
    q0 = sqrt(q0);
    q1 = sqrt(q1);
    q2 = sqrt(q2);
    q3 = sqrt(q3);
    if ((q0 >= q1) && (q0 >= q2) && (q0 >= q3))
    {
        q0 *= +1.0f;
        q1 *= SIGN(r32 - r23);
        q2 *= SIGN(r13 - r31);
        q3 *= SIGN(r21 - r12);
    }
    else if ((q1 >= q0) && (q1 >= q2) && (q1 >= q3))
    {
        q0 *= SIGN(r32 - r23);
        q1 *= +1.0f;
        q2 *= SIGN(r21 + r12);
        q3 *= SIGN(r13 + r31);
    }
    else if ((q2 >= q0) && (q2 >= q1) && (q2 >= q3))
    {
        q0 *= SIGN(r13 - r31);
        q1 *= SIGN(r21 + r12);
        q2 *= +1.0f;
        q3 *= SIGN(r32 + r23);
    }
    else if ((q3 >= q0) && (q3 >= q1) && (q3 >= q2))
    {
        q0 *= SIGN(r21 - r12);
        q1 *= SIGN(r31 + r13);
        q2 *= SIGN(r32 + r23);
        q3 *= +1.0f;
    }
    else
    {
        std::cout << "Error in quaternion calculation." << std::endl;
    }
    real_t r_ = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 /= r_;
    q1 /= r_;
    q2 /= r_;
    q3 /= r_;

    Eigen::Vector4<real_t> quat = { q0, q1, q2, q3 };
    return quat;
}

Eigen::Vector3<real_t> quaternionToNormQuat(const Eigen::Vector4<real_t>& quat)
{
    Eigen::Vector3<real_t> normQuat;
    double mag, sg;
    mag = sqrt(quat[0] * quat[0] + quat[1] * quat[1] +
               quat[2] * quat[2] + quat[3] * quat[3]);
    sg = (quat[0] >= 0.0) ? 1.0 : -1.0;
    mag = sg / mag;
    normQuat[0] = quat[1] * mag;
    normQuat[1] = quat[2] * mag;
    normQuat[2] = quat[3] * mag;

    return normQuat;
}

Eigen::Vector4<real_t> normQuatToQuaternion(const Eigen::Vector3<real_t>& normQuat)
{
    Eigen::Vector4<real_t> quat;

    quat[1] = normQuat[0];
    quat[2] = normQuat[1];
    quat[3] = normQuat[2];
    quat[0] = sqrt(1.0 - quat[1] * quat[1] -
                   quat[2] * quat[2] -
                   quat[3] * quat[3]);

    return quat;
}

Eigen::Matrix3<real_t> quaternionToRotationMatrix(const Eigen::Vector4<real_t>& quat)
{
    Eigen::Matrix3<real_t> rotation;

    double a = quat[0];
    double b = quat[1];
    double c = quat[2];
    double d = quat[3];

    rotation(0, 0) = a * a + b * b - c * c - d * d;
    rotation(0, 1) = 2 * (b * c - a * d);
    rotation(0, 2) = 2 * (b * d + a * c);

    rotation(1, 0) = 2 * (b * c + a * d);
    rotation(1, 1) = a * a - b * b + c * c - d * d;
    rotation(1, 2) = 2 * (c * d - a * b);

    rotation(2, 0) = 2 * (b * d - a * c);
    rotation(2, 1) = 2 * (c * d + a * b);
    rotation(2, 2) = a * a - b * b - c * c + d * d;

    return rotation;
}

Eigen::Matrix4<real_t> averageTransformationMatrices(const std::vector<Eigen::Matrix4<real_t>>& transformations)
{
    Eigen::Vector3<real_t> t, t_sum;
    Eigen::Matrix3<real_t> r, r_sum;

    size_t transform_num = transformations.size();

    for (size_t i = 0; i < transform_num; i++)
    {
        splitRotationAndTranslation(transformations[i], r, t);
        r_sum += r;
        t_sum += t;
    }

    t = t_sum / (real_t)transform_num;
    r = r_sum / (real_t)transform_num;

    return makeTransformationMatrix(r, t);
}

Eigen::Vector4<real_t> vectorToQuaternion(const Eigen::Vector3<real_t>& vec)
{
    Eigen::Vector4<real_t> q;
    q[1] = vec[0];
    q[2] = vec[1];
    q[3] = vec[2];
    q[0] = sqrt(1.0 - q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);

    return q;
}

Eigen::Vector4<real_t> quaternionMultFast(const Eigen::Vector4<real_t>& q1, const Eigen::Vector4<real_t>& q2)
{
    real_t t1, t2, t3, t4, t5, t6, t7, t8, t9;
    Eigen::Vector4<real_t> qm;

    t1 = (q1[0] + q1[1]) * (q2[0] + q2[1]);
    t2 = (q1[3] - q1[2]) * (q2[2] - q2[3]);
    t3 = (q1[1] - q1[0]) * (q2[2] + q2[3]);
    t4 = (q1[2] + q1[3]) * (q2[1] - q2[0]);
    t5 = (q1[1] + q1[3]) * (q2[1] + q2[2]);
    t6 = (q1[1] - q1[3]) * (q2[1] - q2[2]);
    t7 = (q1[0] + q1[2]) * (q2[0] - q2[3]);
    t8 = (q1[0] - q1[2]) * (q2[0] + q2[3]);

    t9 = 0.5 * (t5 - t6 + t7 + t8);
    qm[0] = t2 + t9 - t5;
    qm[1] = t1 - t9 - t6;
    qm[2] = -t3 + t9 - t8;
    qm[3] = -t4 + t9 - t7;

    return qm;
}

std::optional<Eigen::MatrixX<real_t>> triangulatePoint(const Eigen::Vector2<real_t>& p2d1,
                                                const Eigen::Vector2<real_t>& p2d2,
                                                const Eigen::Matrix3<real_t>& K1,
                                                const Eigen::Matrix3<real_t>& K2,
                                                const Eigen::Matrix4<real_t>& T1,
                                                const Eigen::Matrix4<real_t>& T2)
{
    real_t wi1 = 1., wi2 = 1.;
    real_t epsilon = 10e-8;
    int iterNum = 1000;
    Eigen::Vector3<real_t> X;
    cv::Mat Xcv = cv::Mat::ones(4, 1, CV_REAL);

    for (int i = 0; i < iterNum; i++)
    {
        X = linearTriangulation(p2d1, p2d2, K1, K2, T1, T2, wi1, wi2);
        Xcv.at<real_t>(0, 0) = X(0);
        Xcv.at<real_t>(1, 0) = X(1);
        Xcv.at<real_t>(2, 0) = X(2);
        cv::Mat t1r2, t2r2, t1, t2;
        cv::eigen2cv(T1, t1);
        t1r2 = t1.row(2);
        cv::eigen2cv(T2, t2);
        t2r2 = t2.row(2);
        t1r2.convertTo(t1r2, CV_REAL);
        t2r2.convertTo(t2r2, CV_REAL);
        Xcv.convertTo(Xcv, CV_REAL);
        real_t t1x1 = t1r2.t().dot(Xcv);
        real_t t2x1 = t2r2.t().dot(Xcv);

        if ((fabs(wi1 - t1x1) <= epsilon) && (fabs(wi2 - t2x1) <= epsilon))
        {
            break;
        }

        wi1 = t1x1;
        wi2 = t2x1;
    }
    return X;
}

std::optional<Eigen::MatrixX<real_t>> triangulatePoints(const Eigen::MatrixX<real_t>& p2d1,
                                                 const Eigen::MatrixX<real_t>& p2d2,
                                                 const Eigen::Matrix3<real_t>& K1,
                                                 const Eigen::Matrix3<real_t>& K2,
                                                 const Eigen::Matrix4<real_t>& T1,
                                                 const Eigen::Matrix4<real_t>& T2)
{
    Eigen::MatrixX<real_t> p3d(p2d1.rows(), 3);
    for (int i = 0; i < p3d.rows(); i++)
    {
        Eigen::Vector2<real_t> p1, p2;
        Eigen::Vector3<real_t> P;
        pointFromRow2d(p2d1, i, p1);
        pointFromRow2d(p2d2, i, p2);

        std::optional<Eigen::MatrixX<real_t>> maybePoint = triangulatePoint(p1, p2, K1, K2, T1, T2);
        if (maybePoint.has_value())
        {
            P = maybePoint.value();
            rowFromPoint3d(p3d, i, P);
        }
    }
    return p3d;
}

std::optional<real_t> calculateMeanSquaredError(const Eigen::MatrixX<real_t>& lhs, const Eigen::MatrixX<real_t>& rhs)
{
    CARBON_ASSERT(lhs.size() == rhs.size(), "Input matrices must have the same shape.");

    real_t errorSum = 0;
    const size_t pointCount = lhs.rows();
    const size_t dimCount = lhs.cols();

    for (size_t i = 0; i < pointCount; i++)
    {
        real_t pErr = 0;
        for (size_t j = 0; j < dimCount; j++)
        {
            pErr += (rhs(i, j) - lhs(i, j)) * (rhs(i, j) - lhs(i, j));
        }
        errorSum += pErr;
    }
    return errorSum / pointCount;
}

void transformPoints(Eigen::MatrixX<real_t>& points, const Eigen::Matrix4<real_t>& transform)
{
    CARBON_ASSERT(points.cols() == 3, "Points container must have Nx3 shape.");

    for (int i = 0; i < points.rows(); i++)
    {
        Eigen::Vector3<real_t> p;
        pointFromRow3d(points, i, p);
        Eigen::Vector4<real_t> transformedH = transform * Eigen::Vector4<real_t>(p[0], p[1], p[2], real_t(1));
        points.row(i) = Eigen::Vector3<real_t>(transformedH[0], transformedH[1], transformedH[2]);
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
