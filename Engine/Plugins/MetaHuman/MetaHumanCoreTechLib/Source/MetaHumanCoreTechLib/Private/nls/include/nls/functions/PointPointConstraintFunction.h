// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T, int C, typename DISCARD = typename std::enable_if<(C == 2 || C == 3), void>::type>
class PointPointConstraintFunction
{
public:
    /**
     * Function to calculate the an point-point constraint:
     * residual(x) = sqrt(wPoint2Point) * w * (v(x) - target)
     */
    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v, const Eigen::Matrix<T, C, -1>& targets, const Vector<T>& weights, const T wPoint2Point)
    {
        if (v.Cols() != int(targets.cols()))
        {
            CARBON_CRITICAL("point point constraint: number of vertices and targets not matching");
        }
        if (v.Cols() != int(weights.size()))
        {
            CARBON_CRITICAL("point point constraint: number of vertices and weights not matching");
        }

        const T sqrtwPoint2Point = std::sqrt(wPoint2Point);

        const int numConstraints = v.Cols();
        Vector<T> residual(C * numConstraints);
        for (int i = 0; i < numConstraints; i++)
        {
            for (int k = 0; k < C; k++)
            {
                residual[C * i + k] = sqrtwPoint2Point * weights[i] * (v.Matrix()(k, i) - targets(k, i));
            }
        }

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian())
        {
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(C * numConstraints);
            for (int i = 0; i < numConstraints; i++)
            {
                for (int k = 0; k < C; k++)
                {
                    triplets.push_back(Eigen::Triplet<T>(C * i + k, C * i + k, sqrtwPoint2Point * weights[i]));
                }
            }

            SparseMatrix<T> J(C * numConstraints, C * numConstraints);
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }

    /**
     * Function to calculate the an point-point constraint:
     * residual(x) = sqrt(wPoint2Point) * w * (v(x) - target)
     * Additional indices vector specifying on which vertices to evaluate the constraints. In this case the targets and weights need to have the samme number of
     * points as the size of the indices vector.
     */
    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v,
                                const Eigen::VectorXi& indices,
                                const Eigen::Matrix<T, C, -1>& targets,
                                const Vector<T>& weights,
                                const T wPoint2Point)
    {
        const int numConstraints = int(indices.size());

        if (numConstraints != int(targets.cols()))
        {
            CARBON_CRITICAL("point point constraint: number of vertices and targets not matching");
        }
        if (numConstraints != int(weights.size()))
        {
            CARBON_CRITICAL("point point constraint: number of vertices and weights not matching");
        }

        const T sqrtwPoint2Point = std::sqrt(wPoint2Point);

        Vector<T> residual(C * numConstraints);
        for (int i = 0; i < numConstraints; i++)
        {
            for (int k = 0; k < C; k++)
            {
                residual[C * i + k] = sqrtwPoint2Point * weights[i] * (v.Matrix()(k, indices[i]) - targets(k, i));
            }
        }

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian())
        {
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(C * numConstraints);
            for (int i = 0; i < numConstraints; i++)
            {
                for (int k = 0; k < C; k++)
                {
                    triplets.push_back(Eigen::Triplet<T>(C * i + k, C * indices[i] + k, sqrtwPoint2Point * weights[i]));
                }
            }

            SparseMatrix<T> J(C * numConstraints, v.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
