// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <nls/geometry/Quaternion.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

#if !defined(NDEBUG) || (defined(CARBON_ENABLE_ASSERTS) && CARBON_ENABLE_ASSERTS)
#define ASSERT_VALID_DQ_RESULT(dq, result)                                                                          \
        Eigen::Vector2<T> norm = DualQuaternionNorm(dq);                                                     \
        if (fabs(norm(0) - T(1)) < T(1e-5) && fabs(norm(1)) < T(1e-5))                                                  \
        {                                                                                                               \
            CARBON_POSTCONDITION(fabs(result[0]) < T(1e-5), "dq operation should keep px equal 0");                     \
            CARBON_POSTCONDITION(fabs(result[1]) < T(1e-5), "dq operation should keep py equal 0");         \
            CARBON_POSTCONDITION(fabs(result[2]) < T(1e-5), "dq operation should keep pz equal 0");         \
            CARBON_POSTCONDITION(fabs(result[3] - T(1)) < T(1e-5), "dq operation should keep pw equal 1");  \
            CARBON_POSTCONDITION(fabs(result[7]) < T(1e-5), "dq operation should keep qw equal 0");         \
        }
#else
#define ASSERT_VALID_DQ_RESULT(dq, result) {}
#endif

template <class T>
Vector<T> Unambiguify(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");
    const std::vector<T> abses { std::abs(dq[0]), std::abs(dq[1]), std::abs(dq[2]), std::abs(dq[3]) };
    const size_t max_i = std::max_element(abses.cbegin(), abses.cend()) - abses.cbegin();
    const T max = dq[max_i];
    if (max > 0)
    {
        return dq;
    }
    else
    {
        return -dq;
    }
}

template <class T>
Vector<T> IdentityDq()
{
    Vector<T> dq(8);
    dq.setZero();
    dq(3) = T(1);
    return dq;
}

template <class T>
Eigen::Matrix4<T> MatrixOfQuaternionPreMultiplication(const Eigen::Vector4<T>& q)
{
    Eigen::Matrix4<T> Q;
    Q << q[3], -q[2], q[1], q[0],
    q[2], q[3], -q[0], q[1],
    -q[1], q[0], q[3], q[2],
    -q[0], -q[1], -q[2], q[3];
    return Q;
}

template <class T>
Eigen::Matrix4<T> MatrixOfQuaternionPostMultiplication(const Eigen::Vector4<T>& q)
{
    Eigen::Matrix4<T> Q;
    Q << q[3], q[2], -q[1], q[0],
    -q[2], q[3], q[0], q[1],
    q[1], -q[0], q[3], q[2],
    -q[0], -q[1], -q[2], q[3];
    return Q;
}

template <class T>
Eigen::Matrix<T, 8, 8> MatrixOfDualQuaternionPreMultiplication(const Vector<T>& dq)
{
    CARBON_ASSERT(dq.rows() == 8, "Size of dual quaternion must be 8");
    const Eigen::Vector4<T> p = dq.block(0, 0, 4, 1);
    const Eigen::Vector4<T> q = dq.block(4, 0, 4, 1);
    const Eigen::Matrix4<T> Q_p = MatrixOfQuaternionPreMultiplication(p);
    const Eigen::Matrix4<T> Q_q = MatrixOfQuaternionPreMultiplication(q);
    Eigen::Matrix<T, 8, 8> S;
    S.block(0, 0, 4, 4) = Q_p;
    S.block(0, 4, 4, 4).setZero();
    S.block(4, 0, 4, 4) = Q_q;
    S.block(4, 4, 4, 4) = Q_p;
    return S;
}

template <class T>
Eigen::Matrix<T, 8, 8> MatrixOfDualQuaternionPostMultiplication(const Vector<T>& dq)
{
    CARBON_ASSERT(dq.rows() == 8, "Size of dual quaternion must be 8");
    const Eigen::Vector4<T> p = dq.block(0, 0, 4, 1);
    const Eigen::Vector4<T> q = dq.block(4, 0, 4, 1);
    const Eigen::Matrix4<T> Q_p = MatrixOfQuaternionPostMultiplication(p);
    const Eigen::Matrix4<T> Q_q = MatrixOfQuaternionPostMultiplication(q);
    Eigen::Matrix<T, 8, 8> S;
    S.block(0, 0, 4, 4) = Q_p;
    S.block(0, 4, 4, 4).setZero();
    S.block(4, 0, 4, 4) = Q_q;
    S.block(4, 4, 4, 4) = Q_p;
    return S;
}

template <class T, bool NORMALIZE>
Eigen::Matrix3<T> DualQuaternionToRotationMatrix(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    if constexpr (NORMALIZE)
    {
        const Vector<T> dqN = DualQuaternionNormalize(dq);
        return QuaternionToRotationMatrix<T, false>(Vector<T>(dqN.block(0, 0, 4, 1)));
    }
    else
    {
        return QuaternionToRotationMatrix<T, false>(Vector<T>(dq.block(0, 0, 4, 1)));
    }
}

template <class T, bool NORMALIZE>
Eigen::Vector3<T> DualQuaternionToTranslationVector(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    if constexpr (NORMALIZE)
    {
        const Vector<T> dqN = DualQuaternionNormalize(dq);
        const Eigen::Vector4<T> p = dqN.block(0, 0, 4, 1);
        const Eigen::Vector4<T> q = dqN.block(4, 0, 4, 1);
        const Eigen::Vector4<T> result = T(2) * QuaternionMultiplication<T, false>(q, QuaternionInverse<T, false>(p));
        return Eigen::Vector3<T>(result[0], result[1], result[2]);
    }
    else
    {
        const Eigen::Vector4<T> p = dq.block(0, 0, 4, 1);
        const Eigen::Vector4<T> q = dq.block(4, 0, 4, 1);
        const Eigen::Vector4<T> result = T(2) * QuaternionMultiplication<T, false>(q, QuaternionInverse<T, false>(p));
        return Eigen::Vector3<T>(result[0], result[1], result[2]);
    }
}

template <class T>
Vector<T> RotationMatrixTranslationVectorToDualQuaternion(const Eigen::Matrix3<T>& R, const Eigen::Vector3<T>& t)
{
    const Eigen::Vector4<T> tExt(t[0], t[1], t[2], T(0));
    Vector<T> result(8, 1);
    const Eigen::Vector4<T> p = Eigen::Quaternion<T>(R).coeffs();
    const Eigen::Vector4<T> q = T(0.5) * QuaternionMultiplication<T, false>(tExt, p);
    result.block(0, 0, 4, 1) = p;
    result.block(4, 0, 4, 1) = q;
    return result;
}

template <class T>
Vector<T> TranslationVectorToDualQuaternion(const Eigen::Vector3<T>& t)
{
    Eigen::Matrix3<T> R = Eigen::Matrix3<T>::Identity();
    return RotationMatrixTranslationVectorToDualQuaternion<T>(R, t);
}

template <class T>
Eigen::Vector2<T> DualQuaternionNorm(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    const T px = dq(0);
    const T py = dq(1);
    const T pz = dq(2);
    const T pw = dq(3);
    const T qx = dq(4);
    const T qy = dq(5);
    const T qz = dq(6);
    const T qw = dq(7);

    const T pNorm = std::sqrt(px * px + py * py + pz * pz + pw * pw);
    const T dual = (px * qx + py * qy + pz * qz + pw * qw) / pNorm;

    return Eigen::Vector2<T>(pNorm, dual);
}

//! Calculates the normalized dual quaternion
template <class T>
Vector<T> DualQuaternionNormalize(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    const T px = dq(0);
    const T py = dq(1);
    const T pz = dq(2);
    const T pw = dq(3);
    const T qx = dq(4);
    const T qy = dq(5);
    const T qz = dq(6);
    const T qw = dq(7);

    const T oneOverNormP = T(1) / std::sqrt(px * px + py * py + pz * pz + pw * pw);
    const T pDotQOverNormP3 = (px * qx + py * qy + pz * qz + pw * qw) * oneOverNormP * oneOverNormP * oneOverNormP;

    Vector<T> normalized(8, 1);

    normalized.coeffRef(0) = px * oneOverNormP;
    normalized.coeffRef(1) = py * oneOverNormP;
    normalized.coeffRef(2) = pz * oneOverNormP;
    normalized.coeffRef(3) = pw * oneOverNormP;

    normalized.coeffRef(4) = qx * oneOverNormP - px * pDotQOverNormP3;
    normalized.coeffRef(5) = qy * oneOverNormP - py * pDotQOverNormP3;
    normalized.coeffRef(6) = qz * oneOverNormP - pz * pDotQOverNormP3;
    normalized.coeffRef(7) = qw * oneOverNormP - pw * pDotQOverNormP3;

    return normalized;
}

template <class T, bool NORMALIZE>
Vector<T> DualQuaternionQuatConjugate(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    Vector<T> result(8);
    result << -dq[0], -dq[1], -dq[2], dq[3], -dq[4], -dq[5], -dq[6], dq[7];

    if constexpr (NORMALIZE)
    {
        result = DualQuaternionNormalize(result);
    }

    return result;
}

template <class T, bool NORMALIZE>
Vector<T> DualQuaternionDualQuatConjugate(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    Vector<T> result(8);
    result << -dq[0], -dq[1], -dq[2], dq[3], dq[4], dq[5], dq[6], -dq[7];

    if constexpr (NORMALIZE)
    {
        return DualQuaternionNormalize(Vector<T>(result));
    }

    return result;
}

template <class T, bool NORMALIZE>
Vector<T> DualQuaternionMultiplication(const Vector<T>& dq1, const Vector<T>& dq2)
{
    CARBON_PRECONDITION(dq1.rows() == 8, "dual quaternion must be a vector of size 8");
    CARBON_PRECONDITION(dq2.rows() == 8, "dual quaternion must be a vector of size 8");

    Eigen::Vector4<T> p1, p2, q1, q2;

    if constexpr (NORMALIZE)
    {
        const Vector<T> dq1N = DualQuaternionNormalize(dq1);
        const Vector<T> dq2N = DualQuaternionNormalize(dq2);

        p1 = dq1N.block(0, 0, 4, 1);
        p2 = dq2N.block(0, 0, 4, 1);
        q1 = dq1N.block(4, 0, 4, 1);
        q2 = dq2N.block(4, 0, 4, 1);
    }
    else
    {
        p1 = dq1.block(0, 0, 4, 1);
        p2 = dq2.block(0, 0, 4, 1);
        q1 = dq1.block(4, 0, 4, 1);
        q2 = dq2.block(4, 0, 4, 1);
    }

    Vector<T> result(8, 1);
    result.block(0, 0, 4, 1) = QuaternionMultiplication<T, false>(p1, p2);
    result.block(4, 0, 4, 1) = QuaternionMultiplication<T, false>(p1, q2) + QuaternionMultiplication<T, false>(q1, p2);
    return result;
}

template <class T, bool NORMALIZE>
Eigen::Vector3<T> DualQuaternionVectorTransform(const Eigen::Vector3<T>& p, const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    const Vector<T> pExt = Eigen::Matrix<T, 8, 1>{ T(0), T(0), T(0), T(1), p[0], p[1], p[2], T(0) };
    Vector<T> result(8, 1);

    if constexpr (NORMALIZE)
    {
        const Vector<T> dqN = DualQuaternionNormalize(dq);
        const Vector<T> dqNConj = DualQuaternionDualQuatConjugate<T, false>(dqN);
        result = DualQuaternionMultiplication<T, false>(dqN, DualQuaternionMultiplication<T, false>(pExt, dqNConj));
    }
    else
    {
        const Vector<T> dqConj = DualQuaternionDualQuatConjugate<T, false>(dq);
        result = DualQuaternionMultiplication<T, false>(dq, DualQuaternionMultiplication<T, false>(pExt, dqConj));
    }

    ASSERT_VALID_DQ_RESULT(dq, result);
    return Eigen::Vector3<T>(result[4], result[5], result[6]);
}

template <class T>
Eigen::Matrix<T, 3, -1> DualQuaternionShapeTransform(const Eigen::Matrix<T, 3, -1>& shape, const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    const int nVertices = static_cast<int>(shape.cols());
    Eigen::Matrix<T, 3, -1> result(3, nVertices);
    for (int i = 0; i < nVertices; ++i)
    {
        result.col(i) = DualQuaternionVectorTransform<T, false>(shape.col(i), dq);
    }
    return result;
}

template <class T>
Eigen::Matrix<T, 4, 4> DualQuaternionToAffineMatrix(const Vector<T>& dq)
{
    CARBON_PRECONDITION(dq.rows() == 8, "dual quaternion must be a vector of size 8");

    Eigen::Matrix<T, 4, 4> m;
    m.setZero();
    m.block(0, 0, 3, 3) = DualQuaternionToRotationMatrix<T, false>(dq);
    m.block(0, 3, 3, 1) = DualQuaternionToTranslationVector<T, false>(dq);
    m(3, 3) = T(1);
    return m;
}

template <class T>
Vector<T> AffineMatrixToDualQuaternion(const Eigen::Matrix<T, 4, 4>& m)
{
    const Eigen::Matrix<T, 3, 3> R = m.block(0, 0, 3, 3);
    const Eigen::Vector3<T> t = m.block(0, 3, 3, 1);
    return RotationMatrixTranslationVectorToDualQuaternion<T>(R, t);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
