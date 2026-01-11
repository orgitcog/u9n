// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class ProjectionConstraintFunction
{
public:
    /**
        * Function to a soft error based on the limits of the riglogic class
        */
    static DiffData<T> Evaluate(
        const DiffData<T>& inputVector,
        const std::vector<Vector<T>>& projectionVectors,
        const Vector<T>& targetValues,
        const T weight) {

        const T sqrtw = std::sqrt(weight);
        Vector<T> residual(targetValues.size());

        JacobianConstPtr<T> Jacobian;
        if (inputVector.HasJacobian()) {
            // calculate residuals + jacobians in a single loop for efficiency
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(inputVector.Size() * projectionVectors.size());

            for (int i = 0; i < projectionVectors.size(); i++) {
                const T proj = inputVector.Value().head(projectionVectors[i].size()).dot(projectionVectors[i]);
                
                for (int j = 0; j < projectionVectors[i].size(); j++) {
                    triplets.push_back(Eigen::Triplet<T>(i, j, (projectionVectors[i][j]) * sqrtw));
                }
                residual[i] = (proj - targetValues[i]) * sqrtw;
            }

            // calculate actual jacobian matrix
            SparseMatrix<T> J(projectionVectors.size(), inputVector.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());

            Jacobian = inputVector.Jacobian().Premultiply(J);
        }
        else {
            // calculate residuals only
            for (int i = 0; i < projectionVectors.size(); i++) {
                const T dot = inputVector.Value().dot(projectionVectors[i]);
                const T proj = dot / projectionVectors[i].squaredNorm();
                residual[i] = (proj - targetValues[i]) * sqrtw;
            }
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
