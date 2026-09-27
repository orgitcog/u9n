// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to calculate the point surface constraint: residual(x) = sqrt(wNormal) * w * normal.dot(v(x) - target)
 */
template <class T, int C>
class PointSurfaceConstraintFunction
{
public:
    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v,
                                const Eigen::Vector<T, C>* targets,
                                const Eigen::Vector<T, C>* normals,
                                const T* weights,
                                const T wNormal)
    {
        const T sqrtwNormal = std::sqrt(wNormal);

        const int numConstraints = v.Cols();

        Vector<T> residual(v.Cols());
        for (int i = 0; i < numConstraints; i++)
        {
            residual[i] = sqrtwNormal * weights[i] * normals[i].dot(v.Matrix().col(i) - targets[i]);
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
                    triplets.push_back(Eigen::Triplet<T>(i, C * i + k, sqrtwNormal * weights[i] * normals[i][k]));
                }
            }
            SparseMatrix<T> J(numConstraints, C * numConstraints);
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual),
                           Jacobian);
    }

    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v, const Eigen::Matrix<T, C, -1>& targets, const Eigen::Matrix<T, C, -1>& normals,
                                const Vector<T>& weights, const T wNormal)
    {
        if (v.Cols() != int(targets.cols()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and targets not matching");
        }
        if (v.Cols() != int(normals.cols()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and target normals not matching");
        }
        if (v.Cols() != int(weights.size()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and weights not matching");
        }

        return Evaluate(v, (const Eigen::Vector<T, C>*)targets.data(), (const Eigen::Vector<T, C>*)normals.data(), weights.data(), wNormal);
    }

    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v,
                                const int* indices,
                                const Eigen::Vector<T, C>* targets,
                                const Eigen::Vector<T, C>* normals,
                                const T* weights,
                                const int numConstraints,
                                const T wNormal)
    {
        const T sqrtwNormal = std::sqrt(wNormal);

        Vector<T> residual(numConstraints);
        for (int i = 0; i < numConstraints; i++)
        {
            residual[i] = sqrtwNormal * weights[i] * normals[i].dot(v.Matrix().col(indices[i]) - targets[i]);
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
                    triplets.push_back(Eigen::Triplet<T>(i, C * indices[i] + k, sqrtwNormal * weights[i] * normals[i][k]));
                }
            }
            SparseMatrix<T> J(numConstraints, v.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual),
                           Jacobian);
    }

    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v, const Eigen::VectorXi& indices, const Eigen::Matrix<T, C, -1>& targets,
                                const Eigen::Matrix<T, C, -1>& normals, const Vector<T>& weights, const T wNormal)
    {
        if (int(indices.size()) != int(targets.cols()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and targets not matching");
        }
        if (int(indices.size()) != int(normals.cols()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and target normals not matching");
        }
        if (int(indices.size()) != int(weights.size()))
        {
            CARBON_CRITICAL("point surface constraint: number of vertices and weights not matching");
        }

        return Evaluate(v, indices.data(), (const Eigen::Vector<T, C>*)targets.data(), (const Eigen::Vector<T, C>*)normals.data(), weights.data(),
                        int(indices.size()), wNormal);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
