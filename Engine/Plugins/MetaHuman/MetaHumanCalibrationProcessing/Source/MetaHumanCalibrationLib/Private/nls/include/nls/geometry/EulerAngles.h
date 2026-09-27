// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
Eigen::Matrix<T, 3, 3> EulerX(const T angleInRadians)
{
    const T c = cos(angleInRadians);
    const T s = sin(angleInRadians);
    Eigen::Matrix<T, 3, 3> R;
    R.coeffRef(0, 0) = 1;
    R.coeffRef(1, 0) = 0;
    R.coeffRef(2, 0) = 0;
    R.coeffRef(0, 1) = 0;
    R.coeffRef(1, 1) = c;
    R.coeffRef(2, 1) = s;
    R.coeffRef(0, 2) = 0;
    R.coeffRef(1, 2) = -s;
    R.coeffRef(2, 2) = c;
    return R;
}

template <class T>
Eigen::Matrix<T, 3, 3> EulerY(const T angleInRadians)
{
    const T c = cos(angleInRadians);
    const T s = sin(angleInRadians);
    Eigen::Matrix<T, 3, 3> R;
    R.coeffRef(0, 0) = c;
    R.coeffRef(1, 0) = 0;
    R.coeffRef(2, 0) = -s;
    R.coeffRef(0, 1) = 0;
    R.coeffRef(1, 1) = 1;
    R.coeffRef(2, 1) = 0;
    R.coeffRef(0, 2) = s;
    R.coeffRef(1, 2) = 0;
    R.coeffRef(2, 2) = c;
    return R;
}

template <class T>
Eigen::Matrix<T, 3, 3> EulerZ(const T angleInRadians)
{
    const T c = cos(angleInRadians);
    const T s = sin(angleInRadians);
    Eigen::Matrix<T, 3, 3> R;
    R.coeffRef(0, 0) = c;
    R.coeffRef(1, 0) = s;
    R.coeffRef(2, 0) = 0;
    R.coeffRef(0, 1) = -s;
    R.coeffRef(1, 1) = c;
    R.coeffRef(2, 1) = 0;
    R.coeffRef(0, 2) = 0;
    R.coeffRef(1, 2) = 0;
    R.coeffRef(2, 2) = 1;
    return R;
}

//! Calculates the Euler angles given the rotation matrix assuming the XYZ order of Maya i.e. in post-multiply this is EulerZ() * EulerY() * EulerX()
template <typename T>
Eigen::Vector3<T> RotationMatrixToEulerXYZ(const Eigen::Matrix<T, 3, 3>& R)
{
    Eigen::Vector3<T> angles = R.eulerAngles(2, 1, 0);
    auto clampAngle = [](T angle)
    {
        while (angle < -T(CARBON_PI))
        {
            angle += T(2 * CARBON_PI);
        }
        while (angle > T(CARBON_PI))
        {
            angle -= T(2 * CARBON_PI);
        }
        return angle;
    };
    // we would like to have all angles in the smallest range, so we may need to shift solutions
    if (angles[1] > 0)
    {
        Eigen::Vector3<T> altAngles = angles;
        altAngles[0] = clampAngle(angles[0] - T(CARBON_PI));
        altAngles[1] = T(CARBON_PI) - angles[1];
        altAngles[2] = clampAngle(angles[2] - T(CARBON_PI));
        if (altAngles.cwiseAbs().sum() < angles.cwiseAbs().sum())
        {
            angles = altAngles;
        }
    }
    if (angles[1] < 0)
    {
        Eigen::Vector3<T> altAngles = angles;
        altAngles[0] = clampAngle(angles[0]-  T(CARBON_PI));
        altAngles[1] = -T(CARBON_PI) - angles[1];
        altAngles[2] = clampAngle(angles[2] - T(CARBON_PI));
        if (altAngles.cwiseAbs().sum() < angles.cwiseAbs().sum())
        {
            angles = altAngles;
        }
    }

    // Eigen returns the XYZ angles above in order 2, 1, 0, but we return the result as the x angle, y angle, and z angle
    return Eigen::Vector3<T>(angles[2], angles[1], angles[0]);
}

/**
 * Calculates the Euler rotation using the XYZ order of Maya i.e. in post-multiply this is EulerZ() * EulerY() * EulerX()
 * Expects the angles in radians.
 */
template <class T>
Eigen::Matrix<T, 3, 3> EulerXYZ(const T rx, const T ry, const T rz)
{
    Eigen::Matrix<T, 3, 3> m;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    m(0, 0) = cry * crz;
    m(1, 0) = cry * srz;
    m(2, 0) = -sry;

    m(0, 1) = (srx * sry * crz - crx * srz);
    m(1, 1) = (crx * crz + srx * sry * srz);
    m(2, 1) = srx * cry;

    m(0, 2) = (srx * srz + crx * sry * crz);
    m(1, 2) = (crx * sry * srz - srx * crz);
    m(2, 2) = crx * cry;

    return m;
}

template <class T>
Eigen::Matrix<T, 3, 3> EulerXYZ(const Eigen::Vector3<T>& angles)
{
    return EulerXYZ<T>(angles[0], angles[1], angles[2]);
}


//! Calculates the Jacobian of EulerXYZ
template <class T>
SparseMatrix<T> EulerXYZJacobian(const T rx, const T ry, const T rz)
{
    SparseMatrix<T> J;
    J.resize(9, 3);

    const int irx = 0;
    const int iry = 1;
    const int irz = 2;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    // m00 = cry * crz
    J.insert(0, iry) = (-sry * crz);
    J.insert(0, irz) = (-cry * srz);
    // m10 = cry * srz
    J.insert(1, iry) = (-sry * srz);
    J.insert(1, irz) = (cry * crz);
    // m20 = -sry
    J.insert(2, iry) = (-cry);
    // m01 = srx * sry * crz - crx * srz
    J.insert(3, irx) = (crx * sry * crz + srx * srz);
    J.insert(3, iry) = (srx * cry * crz);
    J.insert(3, irz) = (-srx * sry * srz - crx * crz);
    // m11 = crx * crz + srx * sry * srz
    J.insert(4, irx) = (-srx * crz + crx * sry * srz);
    J.insert(4, iry) = (srx * cry * srz);
    J.insert(4, irz) = (-crx * srz + srx * sry * crz);
    // m21 = srx * cry
    J.insert(5, irx) = (crx * cry);
    J.insert(5, iry) = (-srx * sry);
    // m02 = srx * srz + crx * sry * crz
    J.insert(6, irx) = (crx * srz - srx * sry * crz);
    J.insert(6, iry) = (crx * cry * crz);
    J.insert(6, irz) = (srx * crz - crx * sry * srz);
    // m12 = crx * sry * srz - srx * crz
    J.insert(7, irx) = (-srx * sry * srz - crx * crz);
    J.insert(7, iry) = (crx * cry * srz);
    J.insert(7, irz) = (crx * sry * crz + srx * srz);
    // m22 = crx * cry
    J.insert(8, irx) = (-srx * cry);
    J.insert(8, iry) = (-crx * sry);

    J.makeCompressed();

    return J;
}

//! Calculates the Jacobian of EulerXYZ
template <class T>
Eigen::Matrix<T, 9, 3, Eigen::RowMajor> EulerXYZJacobianDense(const T rx, const T ry, const T rz)
{
    Eigen::Matrix<T, 9, 3, Eigen::RowMajor> J = Eigen::Matrix<T, 9, 3, Eigen::RowMajor>::Zero();

    const int irx = 0;
    const int iry = 1;
    const int irz = 2;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    // m00 = cry * crz
    J(0, iry) = (-sry * crz);
    J(0, irz) = (-cry * srz);
    // m10 = cry * srz
    J(1, iry) = (-sry * srz);
    J(1, irz) = (cry * crz);
    // m20 = -sry
    J(2, iry) = (-cry);
    // m01 = srx * sry * crz - crx * srz
    J(3, irx) = (crx * sry * crz + srx * srz);
    J(3, iry) = (srx * cry * crz);
    J(3, irz) = (-srx * sry * srz - crx * crz);
    // m11 = crx * crz + srx * sry * srz
    J(4, irx) = (-srx * crz + crx * sry * srz);
    J(4, iry) = (srx * cry * srz);
    J(4, irz) = (-crx * srz + srx * sry * crz);
    // m21 = srx * cry
    J(5, irx) = (crx * cry);
    J(5, iry) = (-srx * sry);
    // m02 = srx * srz + crx * sry * crz
    J(6, irx) = (crx * srz - srx * sry * crz);
    J(6, iry) = (crx * cry * crz);
    J(6, irz) = (srx * crz - crx * sry * srz);
    // m12 = crx * sry * srz - srx * crz
    J(7, irx) = (-srx * sry * srz - crx * crz);
    J(7, iry) = (crx * cry * srz);
    J(7, irz) = (crx * sry * crz + srx * srz);
    // m22 = crx * cry
    J(8, irx) = (-srx * cry);
    J(8, iry) = (-crx * sry);

    return J;
}

/*
 * Calculates the Euler rotation using the XYZ order of Maya as well as a prior scaling:
 * M = EulerZ * EulerY * EulerX * (sx, sy, sz).diagonalMatrix()
 * Expects the angles in radians.
 */

template <class T>
Eigen::Matrix<T, 3, 3> EulerXYZAndScale(const T rx, const T ry, const T rz, const T sx, const T sy, const T sz)
{
    Eigen::Matrix<T, 3, 3> m;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    m(0, 0) = (cry * crz) * sx;
    m(1, 0) = (cry * srz) * sx;
    m(2, 0) = (-sry) * sx;

    m(0, 1) = (srx * sry * crz - crx * srz) * sy;
    m(1, 1) = (crx * crz + srx * sry * srz) * sy;
    m(2, 1) = (srx * cry) * sy;

    m(0, 2) = (srx * srz + crx * sry * crz) * sz;
    m(1, 2) = (crx * sry * srz - srx * crz) * sz;
    m(2, 2) = (crx * cry) * sz;

    return m;
}

//! Calculates the Jacobian of EulerXYZAndScale
template <class T>
SparseMatrix<T> EulerXYZAndScaleJacobian(const T rx, const T ry, const T rz, const T sx, const T sy, const T sz)
{
    SparseMatrix<T> J;
    J.resize(9, 6);

    const int irx = 0;
    const int iry = 1;
    const int irz = 2;
    const int isx = 3;
    const int isy = 4;
    const int isz = 5;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    // m00 = (cry * crz) * sx
    J.insert(0, iry) = (-sry * crz) * sx;
    J.insert(0, irz) = (-cry * srz) * sx;
    J.insert(0, isx) = (cry * crz);
    // m10 = (cry * srz) * sx
    J.insert(1, iry) = (-sry * srz) * sx;
    J.insert(1, irz) = (cry * crz) * sx;
    J.insert(1, isx) = (cry * srz);
    // m20 = (-sry) * sx
    J.insert(2, iry) = (-cry) * sx;
    J.insert(2, isx) = (-sry);
    // m01 = (srx * sry * crz - crx * srz) * sy
    J.insert(3, irx) = (crx * sry * crz + srx * srz) * sy;
    J.insert(3, iry) = (srx * cry * crz) * sy;
    J.insert(3, irz) = (-srx * sry * srz - crx * crz) * sy;
    J.insert(3, isy) = (srx * sry * crz - crx * srz);
    // m11 = (crx * crz + srx * sry * srz) * sy
    J.insert(4, irx) = (-srx * crz + crx * sry * srz) * sy;
    J.insert(4, iry) = (srx * cry * srz) * sy;
    J.insert(4, irz) = (-crx * srz + srx * sry * crz) * sy;
    J.insert(4, isy) = (crx * crz + srx * sry * srz);
    // m21 = (srx * cry) * sy
    J.insert(5, irx) = (crx * cry) * sy;
    J.insert(5, iry) = (-srx * sry) * sy;
    J.insert(5, isy) = (srx * cry);
    // m02 = (srx * srz + crx * sry * crz) * sz
    J.insert(6, irx) = (crx * srz - srx * sry * crz) * sz;
    J.insert(6, iry) = (crx * cry * crz) * sz;
    J.insert(6, irz) = (srx * crz - crx * sry * srz) * sz;
    J.insert(6, isz) = (srx * srz + crx * sry * crz);
    // m12 = (crx * sry * srz - srx * crz) * sz
    J.insert(7, irx) = (-srx * sry * srz - crx * crz) * sz;
    J.insert(7, iry) = (crx * cry * srz) * sz;
    J.insert(7, irz) = (crx * sry * crz + srx * srz) * sz;
    J.insert(7, isz) = (crx * sry * srz - srx * crz);
    // m22 = (crx * cry) * sz
    J.insert(8, irx) = (-srx * cry) * sz;
    J.insert(8, iry) = (-crx * sry) * sz;
    J.insert(8, isz) = (crx * cry);

    J.makeCompressed();

    return J;
}

//! Calculates the Jacobian of EulerXYZAndScale
template <class T>
Eigen::Matrix<T, 9, 6, Eigen::RowMajor> EulerXYZAndScaleJacobianDense(const T rx, const T ry, const T rz, const T sx, const T sy, const T sz)
{
    Eigen::Matrix<T, 9, 6, Eigen::RowMajor> J = Eigen::Matrix<T, 9, 6, Eigen::RowMajor>::Zero();

    const int irx = 0;
    const int iry = 1;
    const int irz = 2;
    const int isx = 3;
    const int isy = 4;
    const int isz = 5;

    const T srx = sin(rx);
    const T crx = cos(rx);
    const T sry = sin(ry);
    const T cry = cos(ry);
    const T srz = sin(rz);
    const T crz = cos(rz);

    // m00 = (cry * crz) * sx
    J(0, iry) = (-sry * crz) * sx;
    J(0, irz) = (-cry * srz) * sx;
    J(0, isx) = (cry * crz);
    // m10 = (cry * srz) * sx
    J(1, iry) = (-sry * srz) * sx;
    J(1, irz) = (cry * crz) * sx;
    J(1, isx) = (cry * srz);
    // m20 = (-sry) * sx
    J(2, iry) = (-cry) * sx;
    J(2, isx) = (-sry);
    // m01 = (srx * sry * crz - crx * srz) * sy
    J(3, irx) = (crx * sry * crz + srx * srz) * sy;
    J(3, iry) = (srx * cry * crz) * sy;
    J(3, irz) = (-srx * sry * srz - crx * crz) * sy;
    J(3, isy) = (srx * sry * crz - crx * srz);
    // m11 = (crx * crz + srx * sry * srz) * sy
    J(4, irx) = (-srx * crz + crx * sry * srz) * sy;
    J(4, iry) = (srx * cry * srz) * sy;
    J(4, irz) = (-crx * srz + srx * sry * crz) * sy;
    J(4, isy) = (crx * crz + srx * sry * srz);
    // m21 = (srx * cry) * sy
    J(5, irx) = (crx * cry) * sy;
    J(5, iry) = (-srx * sry) * sy;
    J(5, isy) = (srx * cry);
    // m02 = (srx * srz + crx * sry * crz) * sz
    J(6, irx) = (crx * srz - srx * sry * crz) * sz;
    J(6, iry) = (crx * cry * crz) * sz;
    J(6, irz) = (srx * crz - crx * sry * srz) * sz;
    J(6, isz) = (srx * srz + crx * sry * crz);
    // m12 = (crx * sry * srz - srx * crz) * sz
    J(7, irx) = (-srx * sry * srz - crx * crz) * sz;
    J(7, iry) = (crx * cry * srz) * sz;
    J(7, irz) = (crx * sry * crz + srx * srz) * sz;
    J(7, isz) = (crx * sry * srz - srx * crz);
    // m22 = (crx * cry) * sz
    J(8, irx) = (-srx * cry) * sz;
    J(8, iry) = (-crx * sry) * sz;
    J(8, isz) = (crx * cry);

    return J;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
