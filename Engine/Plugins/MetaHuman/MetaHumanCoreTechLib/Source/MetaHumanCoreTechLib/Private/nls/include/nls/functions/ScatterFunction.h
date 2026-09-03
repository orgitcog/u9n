// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to add scatter values from diff data
 */
template <class T>
class ScatterFunction
{
public:
    ScatterFunction() {}

    static DiffData<T> Scatter(const DiffData<T>& a, int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize = 1)
    {
        Vector<T> result = Vector<T>::Zero(outputSize);
        for (int i = 0; i < int(blockIndices.size()); i++)
        {
            const int blockIdx = blockIndices[i];
            for (int k = 0; k < blockSize; k++)
            {
                const int idx = blockIdx * blockSize + k;
                result[idx] = a.Value()[i * blockSize + k];
            }
        }

        JacobianConstPtr<T> Jacobian;
        if (a.HasJacobian() && (a.Jacobian().NonZeros() > 0))
        {
            Jacobian = a.Jacobian().RowScatter(outputSize, blockIndices, blockSize);
            CARBON_ASSERT(Jacobian->Rows() == outputSize, "scatter jacobian needs to have as many rows as the number of scattered outputs");
            CARBON_ASSERT(Jacobian->Cols() == a.Jacobian().Cols(), "scatter jacobian needs to have same number of output columns as the input data");
        }

        return DiffData<T>(std::move(result), Jacobian);
    }

    template <int R, int C1, int C2>
    static DiffDataMatrix<T, R, C1> ScatterColumns(const DiffDataMatrix<T, R, C2>& a, int outputCols, const Eigen::VectorX<int>& colIndices)
    {
        if constexpr (C1 > 0)
        {
            CARBON_PRECONDITION(outputCols == C1, "for fixed size scatter output the number of outputs needs to match the number of scatter indices");
        }
        return DiffDataMatrix<T, R, C1>(a.Rows(), outputCols, Scatter(a, a.Rows() * outputCols, colIndices, a.Rows()));
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
