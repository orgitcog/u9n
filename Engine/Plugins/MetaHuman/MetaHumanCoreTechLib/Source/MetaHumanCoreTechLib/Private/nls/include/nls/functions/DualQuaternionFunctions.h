// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <nls/geometry/DualQuaternion.h>
#include <carbon/utils/Timer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
SparseMatrix<T> DualQuaternionNormalizationJacobian(const DiffData<T>& dq)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");

    const Vector<T> p = dq.Value().block(0, 0, 4, 1);
    const Vector<T> q = dq.Value().block(4, 0, 4, 1);

    const T pNormSquare = p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3];
    const T pNorm = std::sqrt(pNormSquare);
    const T pNormCube = pNorm * pNormSquare;
    const T pNormFifth = pNormSquare * pNormCube;
    const T oneOverPNorm = T(1) / pNorm;
    const T oneOverPNormCube = T(1) / pNormCube;
    const T oneOverPNormFifth = T(1) / pNormFifth;
    const T pDotQ = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

    std::vector<Eigen::Triplet<T>> triplets;
    triplets.reserve(64);

    // Top left quarter of jacobian
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            T value;
            if (i == j)
            {
                value = (pNormSquare - p[i] * p[i]) * oneOverPNormCube;
            }
            else
            {
                value = -p[i] * p[j] * oneOverPNormCube;
            }
            triplets.push_back(Eigen::Triplet<T>(i, j, value));
        }
    }

    // Top right quarter of jacobian
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 4; j < 8; ++j)
        {
            triplets.push_back(Eigen::Triplet<T>(i, j, T(0)));
        }
    }

    // Bottom left
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            T value;
            if (i == j)
            {
                value = -(T(2) * p[i] * q[i] + pDotQ) * oneOverPNormCube + T(3) * p[i] * p[i] * pDotQ * oneOverPNormFifth;
            }
            else
            {
                value = -(p[i] * q[j] + p[j] * q[i]) * oneOverPNormCube + T(3) * p[i] * p[j] * pDotQ * oneOverPNormFifth;
            }
            triplets.push_back(Eigen::Triplet<T>(i + 4, j, value));
        }
    }

    // Bottom right
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            T value;
            if (i == j)
            {
                value = oneOverPNorm - p[i] * p[i] * oneOverPNormCube;
            }
            else
            {
                value = -p[i] * p[j] * oneOverPNormCube;
            }
            triplets.push_back(Eigen::Triplet<T>(i + 4, j + 4, value));
        }
    }

    SparseMatrix<T> normalizationJacobian(8, 8);
    normalizationJacobian.setFromTriplets(triplets.begin(), triplets.end());

    return normalizationJacobian;
}

template <class T>
DiffData<T> DualQuaternionNormalizeDiff(const DiffData<T>& dq)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");

    const Vector<T> p = dq.Value().block(0, 0, 4, 1);
    const Vector<T> q = dq.Value().block(4, 0, 4, 1);

    const T pNormSquare = p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3];
    const T pNorm = std::sqrt(pNormSquare);
    const T pNormCube = pNorm * pNormSquare;
    // const T pNormFifth = pNormSquare * pNormCube;
    const T oneOverPNorm = T(1) / pNorm;
    const T oneOverPNormCube = T(1) / pNormCube;
    // const T oneOverPNormFifth = T(1) / pNormFifth;
    const T pDotQ = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

    Vector<T> result(8);
    result[0] = p[0] * oneOverPNorm;
    result[1] = p[1] * oneOverPNorm;
    result[2] = p[2] * oneOverPNorm;
    result[3] = p[3] * oneOverPNorm;
    result[4] = q[0] * oneOverPNorm - p[0] * pDotQ * oneOverPNormCube;
    result[5] = q[1] * oneOverPNorm - p[1] * pDotQ * oneOverPNormCube;
    result[6] = q[2] * oneOverPNorm - p[2] * pDotQ * oneOverPNormCube;
    result[7] = q[3] * oneOverPNorm - p[3] * pDotQ * oneOverPNormCube;

    JacobianConstPtr<T> jacobian;
    if (dq.HasJacobian() && (dq.Jacobian().NonZeros() > 0))
    {
        SparseMatrix<T> normalizationJacobian = DualQuaternionNormalizationJacobian(dq);
        jacobian = dq.Jacobian().Premultiply(normalizationJacobian);
    }

    return DiffData<T>(std::move(result), jacobian);
}

template <class T>
DiffData<T> DualQuaternionVectorTransformDiff(const DiffData<T>& dq, const Eigen::Vector3<T>& v)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");
    Vector<T> v2 = DualQuaternionVectorTransform<T, false>(v, dq.Value());

    JacobianConstPtr<T> jacobian;
    if (dq.HasJacobian() && (dq.Jacobian().NonZeros() > 0))
    {
        const Vector<T> dq_conj = DualQuaternionDualQuatConjugate<T, false>(dq.Value());
        const Vector<T> v_ext = Eigen::Matrix<T, 8, 1> { T(0), T(0), T(0), T(1), v[0], v[1], v[2], T(0) };
        const Vector<T> dq_v = DualQuaternionMultiplication<T, false>(dq.Value(), v_ext);
        const Vector<T> v_dq_conj = DualQuaternionMultiplication<T, false>(v_ext, dq_conj);

        const Eigen::Matrix<T, 8, 8> I = Eigen::DiagonalMatrix<T, 8, 8>({ T(-1), T(-1), T(-1), T(1), T(1), T(1), T(1), T(-1) });
        const Eigen::Matrix<T, 8, 8> S_dq_v = MatrixOfDualQuaternionPreMultiplication(dq_v);
        const Eigen::Matrix<T, 8, 8> S_v_dq_conj_post = MatrixOfDualQuaternionPostMultiplication(v_dq_conj);
        const Eigen::Matrix<T, 8, 8> full_jacobian = (S_dq_v * I + S_v_dq_conj_post);

        const SparseMatrix<T> operation_jacobian = full_jacobian.block(4, 0, 3, 8).sparseView();

        jacobian = dq.Jacobian().Premultiply(operation_jacobian);
    }
    return DiffData<T>(std::move(v2), jacobian);
}

template <class T>
DiffData<T> DualQuaternionVectorTransformDiff_2(const DiffData<T>& dq, const Eigen::Vector3<T>& v)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");

    const Eigen::Vector4<T> p = dq.Value().block(0, 0, 4, 1);
    const Eigen::Vector4<T> q = dq.Value().block(4, 0, 4, 1);

    const T t0 = p(3) * v(0) - p(2) * v(1) + p(1) * v(2) + q(0);
    const T t1 = p(2) * v(0) + p(3) * v(1) - p(0) * v(2) + q(1);
    const T t2 = -p(1) * v(0) + p(0) * v(1) + p(3) * v(2) + q(2);
    const T t3 = -p(0) * v(0) - p(1) * v(1) - p(2) * v(2) + q(3);
    Eigen::Vector3<T> v2;
    v2(0) = p(3) * q(0) - p(2) * q(1) + p(1) * q(2) - p(0) * q(3) - p(0) * t3 + p(1) * t2 - p(2) * t1 + p(3) * t0;
    v2(1) = p(2) * q(0) + p(3) * q(1) - p(0) * q(2) - p(1) * q(3) - p(0) * t2 - p(1) * t3 + p(2) * t0 + p(3) * t1;
    v2(2) = -p(1) * q(0) + p(0) * q(1) + p(3) * q(2) - p(2) * q(3) + p(0) * t1 - p(1) * t0 - p(2) * t3 + p(3) * t2;

    JacobianConstPtr<T> jacobian;
    if (dq.HasJacobian() && (dq.Jacobian().NonZeros() > 0))
    {
        Eigen::Matrix<T, 3, 8> jj;
        jj.setZero();
        jj(0, 0) = T(2) * (p(0) * v(0) + p(1) * v(1) + p(2) * v(2) - q(3));
        jj(0, 1) = T(2) * (-p(1) * v(0) + p(0) * v(1) + p(3) * v(2) + q(2));
        jj(0, 2) = T(2) * (-p(2) * v(0) + p(0) * v(2) - p(3) * v(1) - q(1));
        jj(0, 3) = T(2) * (p(3) * v(0) - p(2) * v(1) + p(1) * v(2) + q(0));
        jj(0, 4) = T(2) * p(3);
        jj(0, 5) = -T(2) * p(2);
        jj(0, 6) = T(2) * p(1);
        jj(0, 7) = -T(2) * p(0);

        jj(1, 0) = T(2) * (-p(0) * v(1) - p(3) * v(2) + p(1) * v(0) - q(2));
        jj(1, 1) = T(2) * (p(1) * v(1) + p(0) * v(0) + p(2) * v(2) - q(3));
        jj(1, 2) = T(2) * (-p(2) * v(1) + p(1) * v(2) + p(3) * v(0) + q(0));
        jj(1, 3) = T(2) * (-p(0) * v(2) + p(2) * v(0) + p(3) * v(1) + q(1));
        jj(1, 4) = T(2) * p(2);
        jj(1, 5) = T(2) * p(3);
        jj(1, 6) = -T(2) * p(0);
        jj(1, 7) = -T(2) * p(1);

        jj(2, 0) = T(2) * (-p(0) * v(2) + p(2) * v(0) + p(3) * v(1) + q(1));
        jj(2, 1) = T(2) * (-p(1) * v(2) + p(2) * v(1) - p(3) * v(0) - q(0));
        jj(2, 2) = T(2) * (p(0) * v(0) + p(1) * v(1) + p(2) * v(2) - q(3));
        jj(2, 3) = T(2) * (p(0) * v(1) - p(1) * v(0) + p(3) * v(2) + q(2));
        jj(2, 4) = -T(2) * p(1);
        jj(2, 5) = T(2) * p(0);
        jj(2, 6) = T(2) * p(3);
        jj(2, 7) = -T(2) * p(2);

        // std::cout << "\nv2:\n" << v2 << std::endl;
        // std::cout << "\ndq:\n" << dq.Value() << std::endl;
        // std::cout << "\njj:\n" << jj << std::endl;

        jacobian = dq.Jacobian().Premultiply(jj.sparseView());
    }
    return DiffData<T>(v2, jacobian);
}

template <class T>
DiffDataMatrix<T, 3, -1> DualQuaternionShapeTransformDiff(const DiffData<T>& dq, const Eigen::Matrix<T, 3, -1>& v)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");
    const int nVertices = static_cast<int>(v.cols());

    const Eigen::Vector4<T> p = dq.Value().block(0, 0, 4, 1);
    const Eigen::Vector4<T> q = dq.Value().block(4, 0, 4, 1);

    Vector<T> v2(3 * nVertices);

    for (int i = 0; i < nVertices; ++i)
    {
        const T t0 = p(3) * v(0, i) - p(2) * v(1, i) + p(1) * v(2, i) + q(0);
        const T t1 = p(2) * v(0, i) + p(3) * v(1, i) - p(0) * v(2, i) + q(1);
        const T t2 = -p(1) * v(0, i) + p(0) * v(1, i) + p(3) * v(2, i) + q(2);
        const T t3 = -p(0) * v(0, i) - p(1) * v(1, i) - p(2) * v(2, i) + q(3);

        v2(3 * i) = p(3) * q(0) - p(2) * q(1) + p(1) * q(2) - p(0) * q(3) - p(0) * t3 + p(1) * t2 - p(2) * t1 + p(3) * t0;
        v2(3 * i + 1) = p(2) * q(0) + p(3) * q(1) - p(0) * q(2) - p(1) * q(3) - p(0) * t2 - p(1) * t3 + p(2) * t0 + p(3) * t1;
        v2(3 * i + 2) = -p(1) * q(0) + p(0) * q(1) + p(3) * q(2) - p(2) * q(3) + p(0) * t1 - p(1) * t0 - p(2) * t3 + p(3) * t2;
    }

    // Timer timer;

    JacobianConstPtr<T> jacobian;
    if (dq.HasJacobian() && (dq.Jacobian().NonZeros() > 0))
    {
        Eigen::Matrix<T, -1, 8> jj(3 * nVertices, 8);
        jj.setZero();
        for (int i = 0; i < nVertices; ++i)
        {
            jj(3 * i, 0) = T(2) * (p(0) * v(0, i) + p(1) * v(1, i) + p(2) * v(2, i) - q(3));
            jj(3 * i, 1) = T(2) * (-p(1) * v(0, i) + p(0) * v(1, i) + p(3) * v(2, i) + q(2));
            jj(3 * i, 2) = T(2) * (-p(2) * v(0, i) + p(0) * v(2, i) - p(3) * v(1, i) - q(1));
            jj(3 * i, 3) = T(2) * (p(3) * v(0, i) - p(2) * v(1, i) + p(1) * v(2, i) + q(0));
            jj(3 * i, 4) = T(2) * p(3);
            jj(3 * i, 5) = -T(2) * p(2);
            jj(3 * i, 6) = T(2) * p(1);
            jj(3 * i, 7) = -T(2) * p(0);

            jj(3 * i + 1, 0) = T(2) * (-p(0) * v(1, i) - p(3) * v(2, i) + p(1) * v(0, i) - q(2));
            jj(3 * i + 1, 1) = T(2) * (p(1) * v(1, i) + p(0) * v(0, i) + p(2) * v(2, i) - q(3));
            jj(3 * i + 1, 2) = T(2) * (-p(2) * v(1, i) + p(1) * v(2, i) + p(3) * v(0, i) + q(0));
            jj(3 * i + 1, 3) = T(2) * (-p(0) * v(2, i) + p(2) * v(0, i) + p(3) * v(1, i) + q(1));
            jj(3 * i + 1, 4) = T(2) * p(2);
            jj(3 * i + 1, 5) = T(2) * p(3);
            jj(3 * i + 1, 6) = -T(2) * p(0);
            jj(3 * i + 1, 7) = -T(2) * p(1);

            jj(3 * i + 2, 0) = T(2) * (-p(0) * v(2, i) + p(2) * v(0, i) + p(3) * v(1, i) + q(1));
            jj(3 * i + 2, 1) = T(2) * (-p(1) * v(2, i) + p(2) * v(1, i) - p(3) * v(0, i) - q(0));
            jj(3 * i + 2, 2) = T(2) * (p(0) * v(0, i) + p(1) * v(1, i) + p(2) * v(2, i) - q(3));
            jj(3 * i + 2, 3) = T(2) * (p(0) * v(1, i) - p(1) * v(0, i) + p(3) * v(2, i) + q(2));
            jj(3 * i + 2, 4) = -T(2) * p(1);
            jj(3 * i + 2, 5) = T(2) * p(0);
            jj(3 * i + 2, 6) = T(2) * p(3);
            jj(3 * i + 2, 7) = -T(2) * p(2);
        }

        // timer.Restart();
        jacobian = dq.Jacobian().Premultiply(jj.sparseView());
        // const int nCols = dq.Jacobian().Cols();
        // const int nNonZeros = dq.Jacobian().NonZeros();
        // LOG_INFO("nCols: {} Non Zeros: {} Time: {}", nCols, nNonZeros, timer.Current());
    }
    return DiffDataMatrix<T, 3, -1>(3, nVertices, DiffData<T>(std::move(v2), jacobian));
}

template <class T>
DiffData<T> DualQuaternionQuatConjugateDiff(const DiffData<T>& dq)
{
    CARBON_ASSERT(dq.Size() == 8, "Size of dual quaternion must be 8");

    Vector<T> value(8);
    const Vector<T>& dqv = dq.Value();
    value << -dqv[0], -dqv[1], -dqv[2], dqv[3], -dqv[4], -dqv[5], -dqv[6], dqv[7];

    JacobianConstPtr<T> jacobian;
    if (dq.HasJacobian() && (dq.Jacobian().NonZeros() > 0))
    {
        Eigen::Matrix<T, 8, 8> jj;
        jj.setZero();
        jj(0, 0) = -T(1);
        jj(1, 1) = -T(1);
        jj(2, 2) = -T(1);
        jj(3, 3) = T(1);
        jj(4, 4) = -T(1);
        jj(5, 5) = -T(1);
        jj(6, 6) = -T(1);
        jj(7, 7) = T(1);
        jacobian = dq.Jacobian().Premultiply(jj.sparseView());
    }
    return DiffData<T>(std::move(value), jacobian);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
