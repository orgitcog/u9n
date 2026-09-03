// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to add a vector to a matrix colwise f(x).colwise() += t(x)
 */
template <class T>
class ColwiseAddFunction
{
public:
    ColwiseAddFunction() {}

    template <int R, int C>
    DiffDataMatrix<T, R, C> colwiseAddFunction(const DiffDataMatrix<T, R, C>& matA, const DiffDataMatrix<T, R, 1>& vecB) const
    {
        CARBON_PRECONDITION(matA.Rows() == vecB.Size(), "row size needs to match the vector size that is added per column");

        Vector<T> output(matA.Size());
        Eigen::Map<Eigen::MatrixX<T>>(output.data(), matA.Rows(), matA.Cols()) = matA.Matrix().colwise() + vecB.Value();

        JacobianConstPtr<T> outputJacobian = matA.JacobianPtr();

        if (vecB.HasJacobian() && (vecB.Jacobian().NonZeros() > 0))
        {
            // the translation jacobian is with respect to a single vertex, so we need to extend
            // it to all vertices
            JacobianConstPtr<T> repeatedJacobian = vecB.Jacobian().Repeat(matA.Cols());

            if (outputJacobian)
            {
                // TODO: potential optimization: don't repeat rows above but repeat and add directly
                outputJacobian = outputJacobian->Add(repeatedJacobian);
            }
            else
            {
                outputJacobian = repeatedJacobian;
            }
        }

        return DiffDataMatrix<T, R, C>(matA.Rows(), matA.Cols(), DiffData<T>(std::move(output), outputJacobian));
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
