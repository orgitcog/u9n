// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/DiffDataSparseMatrix.h>
#include <nls/math/Math.h>
#include <iostream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to multiply two matrices: f(x) = A(x) * B(x)
 */
template <class T>
class MatrixMultiplyFunction
{
public:
    MatrixMultiplyFunction() {}

    template <int R1, int C1, int R2, int C2>
    static DiffDataMatrix<T, R1, C2> DenseMatrixMatrixMultiply(const DiffDataMatrix<T, R1, C1>& matA, const DiffDataMatrix<T, R2, C2>& matB)
    {
        CARBON_PRECONDITION(matA.Cols() == matB.Rows(), "for matrix multiplication the number of columns of A needs to match the number of rows of B");

        const int rows = matA.Rows();
        const int innerDim = matA.Cols();
        const int cols = matB.Cols();
        Vector<T> output(rows * cols);

        // C = A * B
        Eigen::Map<Eigen::MatrixX<T>>(output.data(), rows, cols) = matA.Matrix() * matB.Matrix();

        JacobianConstPtr<T> mergedJacobian;

        // dC/dx = dC/dA dA/dx + dC/dB dB/dx
        if (matB.HasJacobian() && (matB.Jacobian().NonZeros() > 0))
        {
            // dC/dB dB/dx
            // create dC/dB
            SparseMatrix<T> dCdB(rows * cols, matB.Rows() * matB.Cols());
            dCdB.reserve(rows * cols * innerDim);
            for (int j = 0; j < cols; j++)
            {
                for (int i = 0; i < rows; i++)
                {
                    dCdB.startVec(rows * j + i);
                    for (int k = 0; k < innerDim; k++)
                    {
                        // c(i,j) += a(i,k) * b(k,j)
                        dCdB.insertBackByOuterInner(rows * j + i, innerDim * j + k) = matA.Matrix().coeff(i, k);
                    }
                }
            }
            dCdB.finalize();
            JacobianConstPtr<T> dCdx = matB.Jacobian().Premultiply(dCdB);
            mergedJacobian = dCdx;
        }

        if (matA.HasJacobian() && (matA.Jacobian().NonZeros() > 0))
        {
            // dC/dA dA/dx
            // create dC/dA
            SparseMatrix<T> dCdA(rows * cols, matA.Rows() * matA.Cols());
            dCdA.reserve(rows * cols * innerDim);
            for (int j = 0; j < cols; j++)
            {
                for (int i = 0; i < rows; i++)
                {
                    dCdA.startVec(rows * j + i);
                    for (int k = 0; k < innerDim; k++)
                    {
                        // c(i,j) += a(i,k) * b(k,j)
                        dCdA.insertBackByOuterInner(rows * j + i, rows * k + i) = matB.Matrix().coeff(k, j);
                    }
                }
            }
            dCdA.finalize();
            JacobianConstPtr<T> dCdx = matA.Jacobian().Premultiply(dCdA);
            // add dCdx to the Jacobian matrix
            if (!mergedJacobian)
            {
                mergedJacobian = dCdx;
            }
            else
            {
                mergedJacobian = mergedJacobian->Add(dCdx);
            }
        }

        return DiffDataMatrix<T, R1, C2>(rows, cols, DiffData<T>(std::move(output), mergedJacobian));
    }

    template <int R, int C>
    static DiffData<T> DenseMatrixVectorMultiply(const DiffDataMatrix<T, R, C>& mat, const DiffData<T>& vec)
    {
        CARBON_PRECONDITION(mat.Cols() == vec.Size(), "for matrix-vector multiplication the number of columns of A needs to match the size of x");

        const int cols = mat.Cols();

        DiffDataMatrix<T, C, 1> vecAsMat(std::move(Eigen::Map<const Eigen::MatrixX<T>>(vec.Value().data(), cols, 1)), vec.JacobianPtr());
        DiffDataMatrix<T, R, 1> output = DenseMatrixMatrixMultiply(mat, vecAsMat);
        return DiffData<T>(std::move(output.Value()), output.JacobianPtr());
    }

    template <int R, int C>
    static DiffData<T> SparseMatrixVectorMultiply(const DiffDataSparseMatrix<T, R, C>& mat, const DiffData<T>& vec)
    {
        CARBON_PRECONDITION(mat.Cols() == vec.Size(), "for matrix-vector multiplication the number of columns of A needs to match the size of b");

        SparseMatrix<T> matMatrix = mat.Matrix();

        const int rows = mat.Rows();
        const int nonZeros = static_cast<int>(matMatrix.nonZeros());

        // c = A * b
        Eigen::VectorX<T> output = mat.Matrix() * vec.Value();

        JacobianConstPtr<T> mergedJacobian;

        // dc/dx = dc/dA dA/dx + dc/db db/dx
        if (vec.HasJacobian() && (vec.Jacobian().NonZeros() > 0))
        {
            // dC/db db/dx
            // dC/b = A
            JacobianConstPtr<T> dcdx = vec.Jacobian().Premultiply(mat.Matrix());
            mergedJacobian = dcdx;
        }

        if (mat.HasJacobian() && (mat.Jacobian().NonZeros() > 0))
        {
            // dc/dA dA/dx
            // mat.Jacobian() stores dNonZeros/dx, so calculate dc/dNonZeros
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(nonZeros);
            int nonZeroIdx = 0;
            for (int k = 0; k < matMatrix.outerSize(); ++k) {
                for (typename SparseMatrix<T>::InnerIterator it(matMatrix, k); it; ++it)
                {
                    triplets.emplace_back(static_cast<int>(it.row()), nonZeroIdx, vec.Value().coeff(it.col()));
                    nonZeroIdx++;
                }
            }
            SparseMatrix<T> dcdNonZeros(rows, mat.Jacobian().Rows());
            dcdNonZeros.setFromTriplets(triplets.begin(), triplets.end());

            JacobianConstPtr<T> dcdx = mat.Jacobian().Premultiply(dcdNonZeros);
            // add dCdx to the Jacobian matrix
            if (!mergedJacobian)
            {
                mergedJacobian = dcdx;
            }
            else
            {
                mergedJacobian = mergedJacobian->Add(dcdx);
            }
        }

        return DiffData<T>(std::move(output), mergedJacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
