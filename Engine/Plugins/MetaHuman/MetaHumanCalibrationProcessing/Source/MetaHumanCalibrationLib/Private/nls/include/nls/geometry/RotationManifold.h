// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Projects the matrix M to the closest Rotation Matrix using SVD.
 * M = UDV'
 * R = UV' (or U [1 ... -1] V' if M is a reflection with a negative determinant)
 */
template <class T, int SIZE>
Eigen::Matrix<T, SIZE, SIZE> ProjectToClosestRotationMatrix(const Eigen::Matrix<T, SIZE, SIZE>& M);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
