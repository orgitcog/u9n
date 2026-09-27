// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>

CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/Geometry>
CARBON_RENABLE_WARNINGS

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// Quaternion Average based on https://www.acsu.buffalo.edu/%7Ejohnc/ave_quat07.pdf (equation 12)
// Eigenvector corresponding to maximum eigenvalue of sum_i (wi qi qi_t)
template <class T>
Eigen::Quaternion<T> WeightedQuaternionAverage(const std::vector<Eigen::Quaternion<T>>& qs, const std::vector<T>& weights)
{
    // normalize weights
    const T total = Eigen::Map<const Eigen::VectorX<T>>(weights.data(), (int)weights.size()).sum();
    Eigen::Matrix<T, 4, 4> QQt = Eigen::Matrix<T, 4, 4>::Zero();
    for (int k = 0; k < (int)qs.size(); ++k)
    {
        Eigen::Vector4<T> q = qs[k].normalized().coeffs();
        QQt.noalias() += (weights[k] / total) * q * q.transpose();
    }
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix<T, -1, -1>> eig(QQt);
    return Eigen::Quaternion<T>(Eigen::Vector4<T>(eig.eigenvectors().rightCols(1))).normalized();
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
