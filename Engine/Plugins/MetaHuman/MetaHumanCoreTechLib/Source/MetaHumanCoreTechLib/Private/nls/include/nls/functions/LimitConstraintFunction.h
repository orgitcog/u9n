// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <rig/BodyLogic.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class LimitConstraintFunction {
public:
    /**
     * Function to a soft error based on the limits of the riglogic class
     */
    static DiffData<T> Evaluate(
        const DiffData<T>& guiControls,
        const BodyLogic<T>& rig,
        const T weight) {

        if (guiControls.Size() != rig.NumGUIControls()) {
            CARBON_CRITICAL("Size mismatch between provided control and rig control count");
        }

        const T sqrtw = std::sqrt(weight);
        Vector<T> residual(guiControls.Size());
        const Eigen::Matrix<T, 2, -1>& limits = rig.GuiControlRanges();

        JacobianConstPtr<T> Jacobian;
        if (guiControls.HasJacobian()) {
            // calculate residuals + jacobians in a single loop for efficiency
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(guiControls.Size());

            for (int i = 0; i < guiControls.Size(); i++) {
                const T lower = guiControls.Value()[i] - limits(0, i);
                const T upper = guiControls.Value()[i] - limits(1, i);
                if (lower < T(0)) {
                    triplets.push_back(Eigen::Triplet<T>(i, i, sqrtw));
                    residual[i] = lower * sqrtw;
                }
                else if (upper > T(0)) {
                    triplets.push_back(Eigen::Triplet<T>(i, i, sqrtw));
                    residual[i] = upper * sqrtw;
                }
                else {
                    residual[i] = T(0.0);
                }
            }

            // calculate actual jacobian matrix
            SparseMatrix<T> J(guiControls.Size(), guiControls.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());

            Jacobian = guiControls.Jacobian().Premultiply(J);
        }
        else {
            // calculate residuals only
            for (int i = 0; i < guiControls.Size(); i++) {
                const T lower = guiControls.Value()[i] - limits(0, i);
                const T upper = limits(1, i) - guiControls.Value()[i];
                if (lower > T(0)) {
                    residual[i] = lower;
                }
                else if (upper > T(0)) {
                    residual[i] = upper;
                }
                else {
                    residual[i] = T(0.0);
                }
            }
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
