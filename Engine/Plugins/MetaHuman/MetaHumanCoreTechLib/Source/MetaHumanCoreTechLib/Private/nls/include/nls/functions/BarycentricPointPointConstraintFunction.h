// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <nls/geometry/BarycentricCoordinates.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T, int C, typename DISCARD = typename std::enable_if<(C == 2 || C ==3), void>::type>
class BarycentricPointPointConstraintFunction
{
public:
    /**
     * Function to calculate the an point-point constraint from vertices given as barycentric coordinates:
     * residual(x) = sqrt(wPoint2Point) * w * (v(x) - target)
     */
    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v,
        const std::vector<BarycentricCoordinates<T, 3> >& barycentricCoordinates,
        const Eigen::Matrix<T, C, -1>& targets,
        const Vector<T>& weights,
        const T wPoint2Point) {

        const int numConstraints = int(barycentricCoordinates.size());

        if (numConstraints != int(targets.cols())) {
            CARBON_CRITICAL("barycentric point point constraint: number of vertices and targets not matching");
        }
        if (numConstraints != int(weights.size())) {
            CARBON_CRITICAL("barycentric point point constraint: number of vertices and weights not matching");
        }

        const T sqrtwPoint2Point = std::sqrt(wPoint2Point);

        Vector<T> residual(C * numConstraints);
        for (int i = 0; i < numConstraints; i++) {
            residual.template segment<C>(C * i) = sqrtwPoint2Point * weights[i] * (barycentricCoordinates[i].template Evaluate<C>(v.Matrix()) - targets.col(i));
        }

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian()) {
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(C * numConstraints);
            for (int i = 0; i < numConstraints; i++) {
                for (int k = 0; k < C; k++) {
                    const T val = sqrtwPoint2Point * weights[i];
                    for (int j = 0; j < 3; j++) {
                        const T w = barycentricCoordinates[i].Weight(j);
                        const int vID = barycentricCoordinates[i].Index(j);
                        if (w != T(0.0)) { // negative barycentric weights are allowed
                            triplets.emplace_back(C * i + k, vID * C + k, w * val);
                        }
                    }
                }
            }

            SparseMatrix<T> J(C * numConstraints, C * numConstraints);
            J.setFromTriplets(triplets.begin(), triplets.end());

            Jacobian = v.Jacobian().Premultiply(J);
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
