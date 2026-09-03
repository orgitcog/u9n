// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>

#include <cmath>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * @brief Initialize a diagonal sparse matrix.
 *
 * A sparse diagonal matrix is constructed from a vector of values.
 * This can be used for generating a diagonal Jacobian matrix.
 */
template <class T>
void makeDiag(SparseMatrix<T>* sparseMat, const Vector<T>& diagValues)
{
    int noRows = int(sparseMat->rows());
    int noCols = int(sparseMat->cols());
    int size = int(diagValues.size());

    if ((noRows == 0) && (noCols == 0))
    {
        sparseMat->resize(size, size);
    }
    else
    {
        sparseMat->setZero();
    }

    if ((noRows != size) || (noCols != size))
    {
        CARBON_CRITICAL("Sizes do not match");
    }

    std::vector<Eigen::Triplet<T>> triplets;
    triplets.reserve(size);
    for (int idx = 0; idx < size; idx++)
    {
        triplets.push_back(Eigen::Triplet<T>(idx, idx, diagValues(idx)));
    }
    sparseMat->setFromTriplets(triplets.begin(), triplets.end());
}

/// **
// * @brief Switch sign of loss terms if necessary.
// *
// * The sign of the loss vector elements is switched
// * if the corresponding reference values are negative.
// */
// template <class T>
// void switchSign(Vector<T>* lossTerms, const Vector<T>& refValues) {

// int size = int(lossTerms->size());

// if (size != int(refValues.size())) {
// CARBON_CRITICAL("Sizes do not match");
// }

// for (int idx = 0; idx < size; idx++) {
// if (refValues(idx) < 0) {
// (*lossTerms)(idx) = -(*lossTerms)(idx);
// }
// }
// }

/**
 * @brief Compute a smooth L1 loss.
 *
 * A smooth L1 penalty variant is implemented by this function.
 * It behaves like an L2 around zero and like an L1 far away from it.
 * The option to return the square root, rather than just the loss value,
 * facilitates the application in the context of Gauss-Newton optimization.
 */
template <class T>
DiffData<T> smoothL1(const DiffData<T>& diffData, T beta, bool calcSqrt = true)
{
    // compute loss terms z(y)
    int size = int(diffData.Value().size());
    Vector<T> lossTerms(size);
    for (int idx = 0; idx < size; idx++)
    {
        if (std::abs(diffData.Value()(idx)) < beta)
        {
            if (calcSqrt)
            {
                lossTerms(idx) = std::sqrt(T(0.5)) * diffData.Value()(idx) / std::sqrt(beta);
            }
            else
            {
                lossTerms(idx) = T(0.5) * std::pow(diffData.Value()(idx), T(2)) / beta;
            }
        }
        else
        {
            lossTerms(idx) = std::abs(diffData.Value()(idx)) - T(0.5) * beta;
                                      if (calcSqrt)
                {
                    lossTerms(idx) = std::sqrt(lossTerms(idx));
                }
        }
    }

    if (diffData.HasJacobian())
    {
        // compute gradients dz_i/dy_i
        Vector<T> gradTerms(size);
        for (int idx = 0; idx < size; idx++)
        {
            if (std::abs(diffData.Value()(idx)) < beta)
            {
                if (calcSqrt)
                {
                    gradTerms(idx) = std::sqrt(T(0.5) / beta);
                }
                else
                {
                    gradTerms(idx) = diffData.Value()(idx) / beta;
                }
            }
            else
            {
                T sign = (diffData.Value()(idx) >= 0) ? T(1) : -T(1);
                if (calcSqrt)
                {
                    gradTerms(idx) = T(0.5) * sign / lossTerms(idx);
                }
                else
                {
                    gradTerms(idx) = sign;
                }
            }
        }

        // create the matrix dz/dy (diagonal due to elementwise loss)
        SparseMatrix<T> dzdy(size, size);
        makeDiag(&dzdy, gradTerms);

        // apply the chain rule (dz/dx = dz/dy * dy/dx)
        JacobianConstPtr<T> jacobian = diffData.Jacobian().Premultiply(dzdy);

        return DiffData<T>(lossTerms, jacobian);
    }
    else
    {
        return DiffData<T>(lossTerms);
    }
}

/**
 * @brief Compute the generalized Charbonnier loss.
 *
 * This function establishes a generalized Charbonnier loss.
 * It is a robust loss which can provide edge-preserving
 * smoothing regularization for rig inverse problems.
 * The square root of the loss can be optionally computed.
 */
template <class T>
DiffData<T> generalizedCharbonnier(const DiffData<T>& diffData, T alpha, T eps, bool calcSqrt = true, bool zeroMin = true, T smallEps = T(1e-07))
{
    // compute loss terms z(y)
    Vector<T> valSquared = Eigen::pow(diffData.Value().array(), T(2));
    T epsSquared = std::pow(eps, T(2));
    Vector<T> lossTerms = Eigen::pow(valSquared.array() + epsSquared, alpha / T(2));

    T minVal = T(0);
    if (zeroMin)
    {
        minVal = std::pow(epsSquared, alpha / T(2));
        lossTerms = lossTerms.array() - minVal;
    }

    // take the square root
    if (calcSqrt)
    {
        lossTerms = Eigen::sqrt(lossTerms.array() + smallEps);
    }

    if (diffData.HasJacobian())
    {
        // compute gradients dz_i/dy_i
        int size = int(diffData.Value().size());
        Vector<T> gradTerms(size);
        if (calcSqrt)
        {
            if (zeroMin)
            {
                gradTerms = T(0.5) * alpha * diffData.Value().array() * Eigen::pow(valSquared.array() + epsSquared, (alpha / T(2)) - T(1)) / lossTerms.array();
            }
            else
            {
                gradTerms = T(0.5) * alpha * diffData.Value().array() * Eigen::pow(valSquared.array() + epsSquared, (alpha / T(4)) - T(1));
            }
        }
        else
        {
            gradTerms = alpha * diffData.Value().array() * Eigen::pow(valSquared.array() + epsSquared, (alpha / T(2)) - T(1));
        }

        // create the matrix dz/dy (diagonal due to elementwise loss)
        SparseMatrix<T> dzdy(size, size);
        makeDiag(&dzdy, gradTerms);

        // apply the chain rule (dz/dx = dz/dy * dy/dx)
        JacobianConstPtr<T> jacobian = diffData.Jacobian().Premultiply(dzdy);

        return DiffData<T>(lossTerms, jacobian);
    }
    else
    {
        return DiffData<T>(lossTerms);
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
