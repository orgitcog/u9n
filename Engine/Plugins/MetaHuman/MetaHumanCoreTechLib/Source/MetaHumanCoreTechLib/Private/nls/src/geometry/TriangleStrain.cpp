// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/TriangleStrain.h>
#include <nls/math/Math.h>

#include <iostream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void TriangleStrain<T>::SetTopology(const Eigen::Matrix<int, 3, -1>& triangles) { m_triangles = triangles; }


template <class T>
void TriangleStrain<T>::SetRestPose(const Eigen::Matrix<T, 3, -1>& vertices, bool useAreaWeight)
{  
    const int numTriangles = int(m_triangles.cols());
    m_numVertices = int(vertices.cols());

    if (useAreaWeight)
    {
        m_sqrtRestArea.resize(size_t(numTriangles));
    }
    else
    {
        m_sqrtRestArea.clear();
    }

    // verify that all triangles are valid i.e. have an area larger than 0
    for (int i = 0; i < int(m_triangles.cols()); i++)
    {
        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);
        const Eigen::Vector3<T>& v0 = vertices.col(vID0);
        const Eigen::Vector3<T>& v1 = vertices.col(vID1);
        const Eigen::Vector3<T>& v2 = vertices.col(vID2);

        T doubleArea = (v1 - v0).cross(v2 - v0).norm();
        if (doubleArea <= T(1e-10))
        {
            CARBON_CRITICAL("Strain energy is not possible if triangles have degenerate triangles.");
        }
        if ((vID0 < 0) || (vID0 >= m_numVertices) || (vID1 < 0) || (vID1 >= m_numVertices) || (vID2 < 0) || (vID2 > m_numVertices))
        {
            CARBON_CRITICAL("Triangle index out of bounds.");
        }

        if(useAreaWeight){
            m_sqrtRestArea[i] = sqrt(abs(doubleArea) * T(0.5));
        }
    }

    m_invRestFrame2D.clear();
    m_invRestFrame2D.reserve(numTriangles);

    for (int i = 0; i < int(m_triangles.cols()); i++)
    {
        // for each triangle
        const Eigen::Vector3<T>& v0 = vertices.col(m_triangles(0, i));
        const Eigen::Vector3<T>& v1 = vertices.col(m_triangles(1, i));
        const Eigen::Vector3<T>& v2 = vertices.col(m_triangles(2, i));

        // calculate local frame
        Eigen::Matrix<T, 3, 2> restFrame;
        restFrame.col(0) = v1 - v0;
        restFrame.col(1) = v2 - v0;

        // calculate (arbitrary) local orthogonal 2D frame
        Eigen::Matrix<T, 3, 2> coord2D;
        coord2D.col(0) = restFrame.col(0).normalized();
        coord2D.col(1) = (restFrame.col(1) - restFrame.col(1).dot(coord2D.col(0)) * coord2D.col(0)).normalized();
        const Eigen::Matrix<T, 2, 3> proj2D = coord2D.transpose();

        m_invRestFrame2D.push_back((proj2D * restFrame).inverse());
    }
}

template <class T>
DiffData<T> TriangleStrain<T>::EvaluateProjectiveStrain(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight, const Eigen::Vector<T, -1>& inversionCheck, const T minLambda, const T maxLambda) const
{
    const int numTriangles = int(m_triangles.cols());
    Vector<T> outputData(numTriangles * 6);

    if( (inversionCheck.size() > 0) && (inversionCheck.size() != m_triangles.cols()) ){
        CARBON_CRITICAL("Size of input vector (for inversions check) is different from the number of triangles");
    }

    std::vector<Eigen::Triplet<T>> triplets;
    if (vertices.HasJacobian())
    {
        triplets.reserve(numTriangles * 18);
    }

    const T strainWeightSqrt = sqrt(strainWeight);

    for (int i = 0; i < numTriangles; i++)
    {
        T coefficient = strainWeightSqrt;
        if (m_sqrtRestArea.size() > 1){
            coefficient = coefficient * m_sqrtRestArea[i];
        }

        // Stretch Energy = || CurrFrame3D * inv(Proj2D * RestFrame3D) - Proj2D.tranpose() * F' ||_2^2
        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);

        const Eigen::Vector3<T>& v0 = vertices.Matrix().col(vID0);
        const Eigen::Vector3<T>& v1 = vertices.Matrix().col(vID1);
        const Eigen::Vector3<T>& v2 = vertices.Matrix().col(vID2);

        // calculate local frame
        Eigen::Matrix<T, 3, 2> currFrame;
        currFrame.col(0) = v1 - v0;
        currFrame.col(1) = v2 - v0;

        // calculate deformation gradient and project to closest admissible deformation gradient
        const Eigen::Matrix<T, 3, 2> F = currFrame * m_invRestFrame2D[i];
        const Eigen::JacobiSVD<Eigen::Matrix<T, -1, -1>> svd(F, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::Vector2<T> S = svd.singularValues();
        S[0] = std::clamp(S[0], minLambda, maxLambda); // T(1.0);
        S[1] = std::clamp(S[1], minLambda, maxLambda); // T(1.0);

        if ((inversionCheck.size() > 0) && (inversionCheck[i] < 0))
        {
            // F is a reflection, so we need to invert the matrix
            S[1] = -S[1];
        }

        const Eigen::Matrix<T, 3, 2> Fdash = svd.matrixU() * S.asDiagonal() * svd.matrixV().transpose();

        const Eigen::Matrix<T, 3, 2> residual = coefficient * (currFrame * m_invRestFrame2D[i] - Fdash);

        outputData.segment(6 * i, 6) = Eigen::Map<const Eigen::Vector<T, 6>>(residual.data(), residual.size());

        if (vertices.HasJacobian())
        {
            for (int j = 0; j < 2; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID0 + k,
                                                         -coefficient * (m_invRestFrame2D[i](0, j) + m_invRestFrame2D[i](1, j))));
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID1 + k, coefficient * m_invRestFrame2D[i](0, j)));
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID2 + k, coefficient * m_invRestFrame2D[i](1, j)));
                }
            }
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (vertices.HasJacobian())
    {
        SparseMatrix<T> localJacobian(int(outputData.size()), vertices.Size());
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = vertices.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(outputData), Jacobian);
}

template <class T>
DiffData<T> TriangleStrain<T>::EvaluateGreenStrain(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight) const
{
    const int numTriangles = int(m_triangles.cols());
    Vector<T> outputData(numTriangles * 3);

    std::vector<Eigen::Triplet<T>> triplets;
    if (vertices.HasJacobian())
    {
        triplets.reserve(numTriangles * 27);
    }

    for (int i = 0; i < numTriangles; i++)
    {

        T coefficient = strainWeight;
        if (m_sqrtRestArea.size() > 1){
            coefficient = coefficient * m_sqrtRestArea[i];
        }

        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] inv(proj2D * [v1 - v0, v2 - v0])
        // K = inv(proj2D * [v1 - v0, v2 - v0]) : 2x2 matrix
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] K
        // Green = 1/2 [F^t F - I]
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] K
        // F^t F = K^t [e1.e1 e1.e2] K
        // [e2.e1 e2.e2]
        //
        //
        // F^t F = [e1.e1 K.row(0) + e2.e1 K.row(1),  e2.e1 K.row(0) + e2.e2 K.row(1)] * K
        //
        // FtF00 = (e1.e1 K(0,0) + e2.e1 K(1,0)) * K(0,0) + (e2.e1 K(0,0) + e2.e2 K(1,0)) * K(1,0)
        // FtF01 = (e1.e1 K(0,0) + e2.e1 K(1,0)) * K(0,1) + (e2.e1 K(0,0) + e2.e2 K(1,0)) * K(1,1)

        // FtF10 = (e1.e1 K(0,1) + e2.e1 K(1,1)) * K(0,0) + (e2.e1 K(0,1) + e2.e2 K(1,1)) * K(1,0)
        // FtF11 = (e1.e1 K(0,1) + e2.e1 K(1,1)) * K(0,1) + (e2.e1 K(0,1) + e2.e2 K(1,1)) * K(1,1)

        // FtF00 = e1.e1 K(0,0)^2 + 2 e1.e2 K(1,0) K(0,0) + e2.e2 K(1,0)^2
        // FtF11 = e1.e1 K(0,1)^2 + 2 e1.e2 K(1,1) K(0,1) + e2.e2 K(1,1)^2
        // FtF01 = FtF10 = e1.e1 K(0,0) K(0,1) + e1.e2 K(1,0) K(0,1) + e1.e2 K(0,0) K(1,1) + e2.e2 K(1,0) K(1,1)

        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);

        const Eigen::Vector3<T>& v0 = vertices.Matrix().col(vID0);
        const Eigen::Vector3<T>& v1 = vertices.Matrix().col(vID1);
        const Eigen::Vector3<T>& v2 = vertices.Matrix().col(vID2);

        // calculate local frame
        // Eigen::Matrix<T, 3, 2> currFrame;
        // currFrame.col(0) = v1 - v0;
        // currFrame.col(1) = v2 - v0;

        // const Eigen::Matrix<T, 3, 2> F = currFrame * m_invRestFrame2D[i];
        // const Eigen::Matrix<T, 2, 2> GreenStrain2D = F.transpose() * F - Eigen::Matrix<T, 2, 2>::Identity();
        // std::cout << "GreenStrain2D:\n" << GreenStrain2D << std::endl;
        // printf("GreenStrain2D norm: %f\n", GreenStrain2D.squaredNorm());

        const T e1e1 = (v1 - v0).dot(v1 - v0); // v1x^2 + v0x^2 - 2 v1x v0x + ...
        const T e2e2 = (v2 - v0).dot(v2 - v0); // v2x^2 + v0x^2 - 2 v2x v0x + ...
        const T e1e2 = (v1 - v0).dot(v2 - v0); // v1x v2x + v0x^2 - v1x v0x - v2x v0x + ...

        const Eigen::Matrix<T, 2, 2> K = m_invRestFrame2D[i];
        Eigen::Vector<T, 3> GreenStrain2Db;
        GreenStrain2Db[0] = coefficient * (e1e1 * K(0, 0) * K(0, 0) + T(2) * e1e2 * K(1, 0) * K(0, 0) + e2e2 * K(1, 0) * K(1, 0) - T(1.0));
        GreenStrain2Db[1] = coefficient * (e1e1 * K(0, 1) * K(0, 1) + T(2) * e1e2 * K(1, 1) * K(0, 1) + e2e2 * K(1, 1) * K(1, 1) - T(1.0));
        GreenStrain2Db[2] = coefficient *
            (std::sqrt(T(2)) * (e1e1 * K(0, 0) * K(0, 1) + e1e2 * K(1, 0) * K(0, 1) + e1e2 * K(0, 0) * K(1, 1) + e2e2 * K(1, 0) * K(1, 1)));
        // std::cout << "GreenStrain2Db:\n" << GreenStrain2Db << std::endl;
        // printf("GreenStrain2Db norm: %f\n", GreenStrain2Db.squaredNorm());

        outputData.segment(3 * i, 3) = GreenStrain2Db;

        if (vertices.HasJacobian())
        {
            Eigen::Matrix<T, 3, 3> dG_dE;
            dG_dE(0, 0) = coefficient * K(0, 0) * K(0, 0);
            dG_dE(0, 1) = coefficient * K(1, 0) * K(1, 0);
            dG_dE(0, 2) = coefficient * T(2) * K(1, 0) * K(0, 0);

            dG_dE(1, 0) = coefficient * K(0, 1) * K(0, 1);
            dG_dE(1, 1) = coefficient * K(1, 1) * K(1, 1);
            dG_dE(1, 2) = coefficient * T(2) * K(1, 1) * K(0, 1);

            dG_dE(2, 0) = coefficient * std::sqrt(T(2)) * K(0, 0) * K(0, 1);
            dG_dE(2, 1) = coefficient * std::sqrt(T(2)) * K(1, 0) * K(1, 1);
            dG_dE(2, 2) = coefficient * std::sqrt(T(2)) * (K(1, 0) * K(0, 1) + K(0, 0) * K(1, 1));

            // const T de1e1_dv0x = 2 v0x - 2 v1x;
            // const T de1e1_dv1x = 2 v1x - 2 v0x = -de1e1_dv-x;
            // const T de2e2_dv0x = 2 v0x - 2 v2x;
            // const T de2e2_dv2x = 2 v2x - 2 v0x = -de2e2_dv0x;
            // const T de1e2_dv0x = 2 v0x - v1x - v2x;
            // const T de1e2_dv1x = v2x - v0x;
            // const T de1e2_dv2x = v1x - v0x;
            // equivalent for de1e1_dv0y, de1e1_dv0z
            Eigen::Matrix<T, 3, 9> dE_dV;
            for (int k = 0; k < 3; k++)
            {
                // de1e1
                dE_dV(0, 0 + k) = T(2) * v0[k] - T(2) * v1[k];
                dE_dV(0, 3 + k) = -dE_dV(0, 0 + k);
                dE_dV(0, 6 + k) = T(0);
                // de2e2
                dE_dV(1, 0 + k) = T(2) * v0[k] - T(2) * v2[k];
                dE_dV(1, 3 + k) = T(0);
                dE_dV(1, 6 + k) = -dE_dV(1, 0 + k);
                // de1e2
                dE_dV(2, 0 + k) = T(2) * v0[k] - v1[k] - v2[k];
                dE_dV(2, 3 + k) = v2[k] - v0[k];
                dE_dV(2, 6 + k) = v1[k] - v0[k];
            }

            const Eigen::Matrix<T, 3, 9> dG_dV = dG_dE * dE_dV;
            for (int j = 0; j < 3; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    triplets.push_back(Eigen::Triplet<T>(3 * i + j, 3 * vID0 + k, dG_dV(j, 0 + k)));
                    triplets.push_back(Eigen::Triplet<T>(3 * i + j, 3 * vID1 + k, dG_dV(j, 3 + k)));
                    triplets.push_back(Eigen::Triplet<T>(3 * i + j, 3 * vID2 + k, dG_dV(j, 6 + k)));
                }
            }
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (vertices.HasJacobian() && (triplets.size() > 0))
    {
        SparseMatrix<T> localJacobian(int(outputData.size()), vertices.Size());
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = vertices.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(outputData), Jacobian);
}

template <class T>
DiffData<T> TriangleStrain<T>::EvaluateNHStrain(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight) const
{
    const int numTriangles = int(m_triangles.cols());
    Vector<T> outputData(numTriangles);

    std::vector<Eigen::Triplet<T>> triplets;
    if (vertices.HasJacobian())
    {
        triplets.reserve(numTriangles * 9);
    }

    const T strainWeightSqrt = sqrt(strainWeight);

    for (int tri = 0; tri < numTriangles; tri++)
    {
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] inv(proj2D * [v1 - v0, v2 - v0])
        // K = inv(proj2D * [v1 - v0, v2 - v0]) : 2x2 matrix
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] K
        // Neo-Hookean = sqrt(tr(F^t F)) - sqrt(2)

        const int vIDs[3] = {m_triangles(0, tri), m_triangles(1, tri), m_triangles(2, tri)};

        const Eigen::Vector3<T>& v0 = vertices.Matrix().col(vIDs[0]);
        const Eigen::Vector3<T>& v1 = vertices.Matrix().col(vIDs[1]);
        const Eigen::Vector3<T>& v2 = vertices.Matrix().col(vIDs[2]);

        Eigen::Matrix<T, 3, 2> currFrame;
        currFrame.col(0) = v1 - v0;
        currFrame.col(1) = v2 - v0;

        const Eigen::Matrix<T, 2, 2> K = m_invRestFrame2D[tri];
        const Eigen::Matrix<T, 3, 2> F = currFrame * K;

        Eigen::Vector<T, 3> FtF;
        FtF[0] = F(0, 0) * F(0, 0) + F(1, 0) * F(1, 0) + F(2, 0) * F(2, 0); // F00
        FtF[1] = F(0, 1) * F(0, 1) + F(1, 1) * F(1, 1) + F(2, 1) * F(2, 1); // F11
        FtF[2] = F(0, 0) * F(0, 1) + F(1, 0) * F(1, 1) + F(2, 0) * F(2, 1); // F01 and F10

        T trC = FtF[0] + FtF[1]; // Frobenius norm

        if(trC < T(0)){
            trC = T(0);
        }
        T sqrtTrC = std::sqrt(trC);

        T coefficient = strainWeightSqrt;
        if (m_sqrtRestArea.size() > 1){
            coefficient = coefficient * m_sqrtRestArea[tri];
        }
        outputData[tri] = coefficient * (sqrtTrC - sqrt(T(2)));

        if (vertices.HasJacobian())
        {
            T invSqrtTrC = T(1) / sqrtTrC;
            Eigen::Vector<T, 6> dCdF_linear = Eigen::Vector<T, 6>::Zero(6);
            dCdF_linear.segment(0, 3) = F.col(0) * invSqrtTrC;
            dCdF_linear.segment(3, 3) = F.col(1) * invSqrtTrC;

            for (int i = 0; i < 2; i++) // dv1, dv2
            {
                Eigen::Matrix<T, 6, 3> dFdx = Eigen::Matrix<T, 6, 3>::Zero();
                for (int j = 0; j < 2; j++) // x, y, z
                {
                    for (int c = 0; c < 3; c++)
                    {
                        dFdx(3 * j + c, c) = coefficient * m_invRestFrame2D[tri](i, j);
                    }
                }
                const Eigen::Vector<T, 3> dWdx = dFdx.transpose() * dCdF_linear;

                for (int c = 0; c < 3; c++)
                {
                    triplets.push_back(Eigen::Triplet<T>(tri, 3 * vIDs[i + 1] + c, dWdx(c)));
                }
            }

            // dv0 is special:
            {
                const Eigen::Vector<T, 2> sum = Eigen::Matrix<T, 1, 2>(T(-1.0), T(-1.0)) * coefficient * m_invRestFrame2D[tri];
                Eigen::Matrix<T, 6, 3> dFdx = Eigen::Matrix<T, 6, 3>::Zero();
                for (int j = 0; j < 2; j++) // x, y, z
                {
                    for (int c = 0; c < 3; c++)
                    {
                        dFdx(3 * j + c, c) = sum[j];
                    }
                }
                const Eigen::Vector<T, 3> dWdx = dFdx.transpose() * dCdF_linear;
                for (int c = 0; c < 3; c++)
                {
                    triplets.push_back(Eigen::Triplet<T>(tri, 3 * vIDs[0] + c, dWdx(c)));
                }
            }
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (vertices.HasJacobian() && (triplets.size() > 0))
    {
        SparseMatrix<T> localJacobian(int(outputData.size()), vertices.Size());
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = vertices.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(outputData), Jacobian);
}

template <class T>
Eigen::VectorX<T> TriangleStrain<T>::EvaluateProjectiveStrainPerVertex(const DiffDataMatrix<T, 3, -1>& vertices) const
{
    const Eigen::VectorX<T> projectiveStrain = EvaluateProjectiveStrain(vertices, T(1)).Value();
    Eigen::VectorX<T> verticesProjectiveStrain = Eigen::VectorX<T>::Zero(m_numVertices);

    for (int i = 0; i < int(m_triangles.cols()); i++)
    {
        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);
        verticesProjectiveStrain[vID0] = std::max<T>(verticesProjectiveStrain[vID0], projectiveStrain[i]);
        verticesProjectiveStrain[vID1] = std::max<T>(verticesProjectiveStrain[vID1], projectiveStrain[i]);
        verticesProjectiveStrain[vID2] = std::max<T>(verticesProjectiveStrain[vID2], projectiveStrain[i]);
    }

    return verticesProjectiveStrain;
}

template <class T>
Eigen::VectorX<T> TriangleStrain<T>::EvaluateGreenStrainPerVertex(const DiffDataMatrix<T, 3, -1>& vertices) const
{
    const Eigen::VectorX<T> greenStrain = EvaluateGreenStrain(vertices, T(1)).Value();
    Eigen::VectorX<T> verticesGreenStrain = Eigen::VectorX<T>::Zero(m_numVertices);

    for (int i = 0; i < int(m_triangles.cols()); i++)
    {
        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);
        verticesGreenStrain[vID0] = std::max<T>(verticesGreenStrain[vID0], greenStrain[i]);
        verticesGreenStrain[vID1] = std::max<T>(verticesGreenStrain[vID1], greenStrain[i]);
        verticesGreenStrain[vID2] = std::max<T>(verticesGreenStrain[vID2], greenStrain[i]);
    }

    return verticesGreenStrain;
}

template <class T>
DiffData<T> TriangleStrain<T>::EvaluateAreaLoss(const DiffDataMatrix<T, 3, -1>& vertices, T areaWeight) const{
    const int numTriangles = int(m_triangles.cols());
    Vector<T> outputData(numTriangles);

    std::vector<Eigen::Triplet<T>> triplets;
    if (vertices.HasJacobian())
    {
        triplets.reserve(numTriangles * 9);
    }

    const T areaWeightSqrt = sqrt(areaWeight);

    for (int tri = 0; tri < numTriangles; tri++)
    {
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] inv(proj2D * [v1 - v0, v2 - v0])
        // K = inv(proj2D * [v1 - v0, v2 - v0]) : 2x2 matrix
        // F = [v1+d1 - v0+d0, v2+d2 - v0+d0] K
        // Green = 1/2 [F^t F - I]

        const int vIDs[3] = {m_triangles(0, tri), m_triangles(1, tri), m_triangles(2, tri)};

        const Eigen::Vector3<T>& v0 = vertices.Matrix().col(vIDs[0]);
        const Eigen::Vector3<T>& v1 = vertices.Matrix().col(vIDs[1]);
        const Eigen::Vector3<T>& v2 = vertices.Matrix().col(vIDs[2]);

        Eigen::Matrix<T, 3, 2> currFrame;
        currFrame.col(0) = v1 - v0;
        currFrame.col(1) = v2 - v0;

        const Eigen::Matrix<T, 2, 2> K = m_invRestFrame2D[tri];
        const Eigen::Matrix<T, 3, 2> F = currFrame * K;

        Eigen::Vector<T, 3> FtF;
        FtF[0] = F(0, 0) * F(0, 0) + F(1, 0) * F(1, 0) + F(2, 0) * F(2, 0); // F00
        FtF[1] = F(0, 1) * F(0, 1) + F(1, 1) * F(1, 1) + F(2, 1) * F(2, 1); // F11
        FtF[2] = F(0, 0) * F(0, 1) + F(1, 0) * F(1, 1) + F(2, 0) * F(2, 1); // F01 and F10

        T coefficient = areaWeightSqrt;
        if (m_sqrtRestArea.size() > 1){
            coefficient = coefficient * m_sqrtRestArea[tri];
        }
        
        T determinant = FtF[0] * FtF[1] - FtF[2] * FtF[2];
        if(determinant < 0) // avoid determinant = - 0 in degenerate configurations
        {
            determinant = T(0);
        }
        T sqrtDeterminant = sqrt(determinant);
        outputData[tri] = coefficient * (sqrtDeterminant - T(1));

        if (vertices.HasJacobian())
        {
            
            Eigen::Matrix<T, 2, 2> adjFtF;
            adjFtF(0, 0) = FtF[1];
            adjFtF(0, 1) = - FtF[2];
            adjFtF(1, 0) = - FtF[2];
            adjFtF(1, 1) = FtF[0];

            Eigen::Vector<T, 6> dCdF_linear = Eigen::Vector<T, 6>::Zero(6);
            dCdF_linear.segment(0, 3) = F * adjFtF.col(0) / sqrtDeterminant;
            dCdF_linear.segment(3, 3) = F * adjFtF.col(1) / sqrtDeterminant;

            for (int i = 0; i < 2; i++) // dv1, dv2
            {
                Eigen::Matrix<T, 6, 3> dFdx = Eigen::Matrix<T, 6, 3>::Zero();
                for (int j = 0; j < 2; j++) // x, y, z
                {
                    for (int c = 0; c < 3; c++)
                    {
                        dFdx(3 * j + c, c) = coefficient * m_invRestFrame2D[tri](i, j);
                    }
                }
                const Eigen::Vector<T, 3> dWdx = dFdx.transpose() * dCdF_linear;

                for (int c = 0; c < 3; c++)
                {
                    triplets.push_back(Eigen::Triplet<T>(tri, 3 * vIDs[i + 1] + c, dWdx(c)));
                }
            }

            // dv0 is special:
            {
                const Eigen::Vector<T, 2> sum = Eigen::Matrix<T, 1, 2>(T(-1.0), T(-1.0)) * coefficient * m_invRestFrame2D[tri];
                Eigen::Matrix<T, 6, 3> dFdx = Eigen::Matrix<T, 6, 3>::Zero();
                for (int j = 0; j < 2; j++) // x, y, z
                {
                    for (int c = 0; c < 3; c++)
                    {
                        dFdx(3 * j + c, c) = sum[j];
                    }
                }
                const Eigen::Vector<T, 3> dWdx = dFdx.transpose() * dCdF_linear;
                for (int c = 0; c < 3; c++)
                {
                    triplets.push_back(Eigen::Triplet<T>(tri, 3 * vIDs[0] + c, dWdx(c)));
                }
            }
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (vertices.HasJacobian() && (triplets.size() > 0))
    {
        SparseMatrix<T> localJacobian(int(outputData.size()), vertices.Size());
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = vertices.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(outputData), Jacobian);
}

template <class T>
DiffData<T> TriangleStrain<T>::EvaluateAreaLossProjective(const DiffDataMatrix<T, 3, -1>& vertices, const T areaWeight, const Eigen::Vector<T, -1>& inversionCheck, const T minLambda, const T maxLambda) const
{
    const int numTriangles = int(m_triangles.cols());
    Vector<T> outputData(numTriangles * 6);

    if((inversionCheck.size() > 0) && (inversionCheck.size() != m_triangles.cols())){
        CARBON_CRITICAL("Size of input vector (for inversions check) is different from the number of triangles");
    }

    std::vector<Eigen::Triplet<T>> triplets;
    if (vertices.HasJacobian())
    {
        triplets.reserve(numTriangles * 18);
    }

    const T areaWeightSqrt = sqrt(areaWeight);

    for (int i = 0; i < numTriangles; i++)
    {
        T coefficient = areaWeightSqrt;
        if (m_sqrtRestArea.size() > 1){
            coefficient = coefficient * m_sqrtRestArea[i];
        }

        // Stretch Energy = || CurrFrame3D * inv(Proj2D * RestFrame3D) - Proj2D.tranpose() * F' ||_2^2
        const int vID0 = m_triangles(0, i);
        const int vID1 = m_triangles(1, i);
        const int vID2 = m_triangles(2, i);

        const Eigen::Vector3<T>& v0 = vertices.Matrix().col(vID0);
        const Eigen::Vector3<T>& v1 = vertices.Matrix().col(vID1);
        const Eigen::Vector3<T>& v2 = vertices.Matrix().col(vID2);

        // calculate local frame
        Eigen::Matrix<T, 3, 2> currFrame;
        currFrame.col(0) = v1 - v0;
        currFrame.col(1) = v2 - v0;

        // calculate deformation gradient and project to closest admissible deformation gradient
        const Eigen::Matrix<T, 3, 2> F = currFrame * m_invRestFrame2D[i];
        const Eigen::JacobiSVD<Eigen::Matrix<T, -1, -1>> svd(F, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::Vector2<T> S = svd.singularValues();

        constexpr int innerIterations = 8;
        Eigen::Vector2<T> d = Eigen::Vector<T, 2>::Zero();
        for (int it = 0; it < innerIterations; ++it)
        {
            const T currentArea = S[0] * S[1];
            /*
            const T f = currentArea - T(1); // clamp(v, rangeMin_, rangeMax_);
            */
            T f = T(0);
            if(currentArea < minLambda) f = currentArea - minLambda;
            if(currentArea > maxLambda) f = currentArea - maxLambda;
            // f = currentArea - T(1);

            Eigen::Vector2<T> g(S[1], S[0]);
            d = -((f - g.dot(d)) / g.dot(g)) * g;
            S = svd.singularValues() + d;
        }
        if ((inversionCheck.size() > 0) && (inversionCheck[i] < 0))
        {
            // F is a reflection, so we need to invert the matrix
            S[1] = -S[1];
        }

        const Eigen::Matrix<T, 3, 2> Fdash = svd.matrixU() * S.asDiagonal() * svd.matrixV().transpose();
        const Eigen::Matrix<T, 3, 2> residual = coefficient * (currFrame * m_invRestFrame2D[i] - Fdash);
        outputData.segment(6 * i, 6) = Eigen::Map<const Eigen::Vector<T, 6>>(residual.data(), residual.size());

        if (vertices.HasJacobian())
        {
            for (int j = 0; j < 2; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID0 + k,
                                                         -coefficient * (m_invRestFrame2D[i](0, j) + m_invRestFrame2D[i](1, j))));
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID1 + k, coefficient * m_invRestFrame2D[i](0, j)));
                    triplets.push_back(Eigen::Triplet<T>(6 * i + 3 * j + k, 3 * vID2 + k, coefficient * m_invRestFrame2D[i](1, j)));
                }
            }
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (vertices.HasJacobian())
    {
        SparseMatrix<T> localJacobian(int(outputData.size()), vertices.Size());
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = vertices.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(outputData), Jacobian);
}

template class TriangleStrain<float>;
template class TriangleStrain<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
