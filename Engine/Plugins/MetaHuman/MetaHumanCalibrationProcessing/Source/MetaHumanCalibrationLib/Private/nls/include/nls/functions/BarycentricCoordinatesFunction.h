// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/math/Math.h>
#include <nls/math/SparseMatrixBuilder.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to add gather values from diff data
 */
template <class T, int R, int C = 3>
class BarycentricCoordinatesFunction
{
static_assert(C >= 1);

public:
    /**
     * Evaluates the barycentric coordinates on the input vertices
     */
    static DiffDataMatrix<T, R, -1> Evaluate(const DiffDataMatrix<T, R, -1>& vertices, const std::vector<BarycentricCoordinates<T, C>>& barycentricCoordinates)
    {
        const int numElements = int(barycentricCoordinates.size());

        Vector<T> result(numElements * R);
        for (int i = 0; i < numElements; ++i)
        {
            result.segment(i * R, R) = barycentricCoordinates[i].template Evaluate<R>(vertices.Matrix());
        }

        JacobianConstPtr<T> Jacobian;
        if (vertices.HasJacobian() && (vertices.Jacobian().NonZeros() > 0))
        {
            SparseMatrix<T> localJacobian(R * numElements, vertices.Size());
            localJacobian.reserve(numElements * C * R);
            std::vector<Eigen::Triplet<T>> triplets;
            for (int i = 0; i < numElements; ++i)
            {
                for (int k = 0; k < R; ++k)
                {
                    localJacobian.startVec(i * R + k);
                    for (int j = 0; j < C; ++j)
                    {
                        const T w = barycentricCoordinates[i].Weight(j);
                        const int vID = barycentricCoordinates[i].Index(j);
                        if (w != T(0.0)) // negative barycentric weights are allowed
                        {
                            localJacobian.insertBackByOuterInnerUnordered(i * R + k, vID * R + k) = w;
                        }
                    }
                }
            }
            localJacobian.finalize();
            Jacobian = vertices.Jacobian().Premultiply(localJacobian);
        }

        return DiffDataMatrix<T, R, -1>(R, numElements, DiffData<T>(std::move(result), Jacobian));
    }


    static DiffDataMatrix<T, R, -1> Evaluate(const DiffDataMatrix<T, R, -1>& vertices, const std::vector<BarycentricCoordinatesExt<T, C>>& barycentricCoordinates)
    {
        const int numElements = int(barycentricCoordinates.size());

        Vector<T> result(numElements * R);
        for (int i = 0; i < numElements; ++i)
        {
            result.segment(i * R, R) = barycentricCoordinates[i].template Evaluate<R>(vertices.Matrix());
        }

        JacobianConstPtr<T> Jacobian;
        if (vertices.HasJacobian() && (vertices.Jacobian().NonZeros() > 0))
        {
            SparseMatrix<T> localJacobian(R * numElements, vertices.Size());
            localJacobian.reserve(numElements * C * R);
            std::vector<Eigen::Triplet<T>> triplets;
            for (int i = 0; i < numElements; ++i)
            {
                for (int k = 0; k < R; ++k)
                {
                    localJacobian.startVec(i * R + k);
                    for (int j = 0; j < C; ++j)
                    {
                        const T w = barycentricCoordinates[i].Weight(j);
                        const int vID = barycentricCoordinates[i].Index(j);
                        if (w != T(0.0)) // negative barycentric weights are allowed
                        {
                            localJacobian.insertBackByOuterInner(i * R + k, vID * R + k) = w;
                        }
                    }
                }
            }
            localJacobian.finalize();
            Jacobian = vertices.Jacobian().Premultiply(localJacobian);
        }

        return DiffDataMatrix<T, R, -1>(R, numElements, DiffData<T>(std::move(result), Jacobian));
    }
};


/**
 * Function to add gather values from diff data
 */
template <class T>
class BarycentricCoordinatesFunctionExt
{
public:

    /**
     * Evaluates the barycentric coordinates on the input vertices
     */
    static DiffDataMatrix<T, 3, -1> Evaluate(const DiffDataMatrix<T, 2, -1>& barycentricUVNew, const Eigen::Matrix<T, 3, -1>& vertices, const std::vector<BarycentricCoordinatesExt<T, 3>>& barycentricCoordinatesOld)
    {
        
        const int numElements = int(barycentricCoordinatesOld.size());

        Vector<T> result(numElements * 3);
        for (int i = 0; i < numElements; ++i)
        {
            result.segment(i * 3, 3) = vertices.col(barycentricCoordinatesOld[i].Indices()[0]) * barycentricUVNew.Matrix()(0, i) 
                    + vertices.col(barycentricCoordinatesOld[i].Indices()[1]) * barycentricUVNew.Matrix()(1, i)
                    + vertices.col(barycentricCoordinatesOld[i].Indices()[2]) * (T(1) - barycentricUVNew.Matrix()(0, i) - barycentricUVNew.Matrix()(1, i));
        }

        JacobianConstPtr<T> Jacobian;
        if (barycentricUVNew.HasJacobian()) //&& (barycentricUVNew.Jacobian().NonZeros() > 0))
        {
            SparseMatrix<T> localJacobian(3 * numElements, numElements * 2);
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(numElements * 2 * 3);
            for (int i = 0; i < numElements; ++i)
            {
                Eigen::Matrix<T, 3, 2> Ji;
                Ji.col(0) = vertices.col(barycentricCoordinatesOld[i].Indices()[0]) - vertices.col(barycentricCoordinatesOld[i].Indices()[2]);
                Ji.col(1) = vertices.col(barycentricCoordinatesOld[i].Indices()[1]) - vertices.col(barycentricCoordinatesOld[i].Indices()[2]);
                for (int k = 0; k < 3; ++k)
                {
                    for (int j = 0; j < 2; ++j)
                    {
                        triplets.push_back(Eigen::Triplet<T>(i * 3 + k, 2 * i + j, Ji(k, j)));
                    }
                }
            }
            localJacobian.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = barycentricUVNew.Jacobian().Premultiply(localJacobian);
        }

        return DiffDataMatrix<T, 3, -1>(3, numElements, DiffData<T>(std::move(result), Jacobian));
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
