// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class AxisConstraintFunction
{
public:
    /**
     * Function to calculate a length constraint measured along an axis (i.e. max(v) - min(v) = l)
     */
    static DiffData<T> Evaluate(const DiffDataMatrix<T, 3, -1>& v, const int axis, const int minIndex, const int maxIndex, const T targetLength, const T weight)
    {

        // calculate min/max indices
        const T minValue = v.Matrix().col(minIndex)[axis];
        const T maxValue = v.Matrix().col(maxIndex)[axis];
        const T length = maxValue - minValue;

        const T sqrtWeight = std::sqrt(weight);

        Vector<T> residual(1);
        residual[0] = (length - targetLength) * sqrtWeight;

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian())
        {
            // calculate residuals + jacobians in a single loop for efficiency
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(2);
            triplets.push_back(Eigen::Triplet<T>(0, 3 * minIndex + axis, -sqrtWeight));
            triplets.push_back(Eigen::Triplet<T>(0, 3 * maxIndex + axis, sqrtWeight));

            // calculate actual jacobian matrix
            SparseMatrix<T> J(1, v.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
