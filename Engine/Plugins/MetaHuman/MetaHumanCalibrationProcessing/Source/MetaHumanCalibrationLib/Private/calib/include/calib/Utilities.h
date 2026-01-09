// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <calib/Defs.h>
#include <carbon/Common.h>
#include <optional>
CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/Dense>
CARBON_RENABLE_WARNINGS
#include <type_traits>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
    @brief Utilities for multiple-view-geometry.
 */

/**
    @brief Split transformation matrix [R|t] to separated rotation
    matrix and translation vector.

    @param transformation
        Input parameter - input transformation matrix (4x3 or 4x4 matrix).

    @param rotation
        Output parameter - output rotation matrix (3x3).

    @param translation
        Output parameter - output translation vector (3x1).
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/
void splitRotationAndTranslation(const Eigen::Matrix4<real_t>& transformation, Eigen::Matrix3<real_t>& rotation, Eigen::Vector3<real_t>& translation);

/**
    @brief Inverts the transformation matrix with R = R.transposed and
    t = - R * t

    @param transformation
        Input/Output parameter - input transformation matrix (4x3 or 4x4 matrix).

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void inverseGeometricTransform(
    Eigen::Matrix4<real_t>& transformation);

/**
    @brief Utility function: extracts 4x1 homogenious 3D point from point
    container.

    @param matrix
        Input parameter - Input point container (pointCount x 3)

    @param row
        Input parameter - Point position in point container.

    @param point
        Output parameter - Extracted point with shape 4x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/bool pointFromRow3dHomogenious(
    const Eigen::MatrixX<real_t>& matrix,
    size_t row,
    Eigen::Vector4<real_t>& point);

/**
    @brief Utility function: extracts 3x1 3D point from point
    container.

    @param matrix
        Input parameter - Input point container (pointCount x 3)

    @param row
        Input parameter - Point position in point container.

    @param point
        Output parameter - Extracted point with shape 3x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void pointFromRow3d(
    const Eigen::MatrixX<real_t>& matrix,
    size_t row,
    Eigen::Vector3<real_t>& point);

/**
    @brief Utility function: packs 3x1 3D point into point
    container.

    @param matrix
        Output parameter - Output point container (pointCount x 3)

    @param row
        Input parameter - Point position in point container.

    @param point
        Input parameter - Input point with shape 3x1 or 4x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void rowFromPoint3d(
    Eigen::MatrixX<real_t>& matrix,
    size_t row,
    const Eigen::VectorX<real_t>& point);

/**
   @brief Utility function: extracts 3x1 homogenious 2D point from point
   container.

   @param matrix
    Input parameter - Input point container (pointCount x 2)

   @param row
    Input parameter - Point position in point container.

   @param point
    Output parameter - Extracted point with shape 3x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void pointFromRow2dHomogenious(
    const Eigen::MatrixX<real_t>& matrix,
    size_t row,
    Eigen::Vector3<real_t>& point);

/**
    @brief Utility function: extracts 2x1 homogenious 2D point from point
    container.

    @param matrix
        Input parameter - Input point container (pointCount x 2)

    @param row
        Input parameter - Point position in point container.

    @param point
        Output parameter - Extracted point with shape 2x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void pointFromRow2d(
    const Eigen::MatrixX<real_t>& matrix,
    size_t row,
    Eigen::Vector2<real_t>& point);

/**
    @brief Utility function: packs 2x1 2D point into point
    container.

    @param matrix
        Output parameter - Output point container (pointCount x 2)

    @param row
        Input parameter - Point position in point container.

    @param point
        Input parameter - Input point with shape 2x1 or 3x1.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void rowFromPoint2d(
    Eigen::MatrixX<real_t>& matrix,
    size_t row,
    const Eigen::VectorX<real_t>& point);

/**
   @brief Concatenates rotation matrix and translation vector into
   transformation matrix.

   @param rotation
    Input parameter - Rotation matrix 3x3.

   @param translation
    Input parameter - Translation vector 3x1.

   @return
    If valid, function returns transformation 4x4 matrix.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Matrix4<real_t>
makeTransformationMatrix(const Eigen::Matrix3<real_t>& rotation, const Eigen::Vector3<real_t>& translation);

/**
    @brief Calculate quaternion from input rotation matrix.

    @param rotation
    Input parameter - Rotation matrix 3x3.

    @return
    If valid, function returns 4x1 quaternion.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector4<real_t>
rotationMatrixToQuaternion(const Eigen::Matrix3<real_t>& rotation);

/**
    @brief Calculate rotation matrix from input quaternion.

    @param quaternion
    Input parameter - quaternion vector 4x1.

    @return
    If valid, function returns 3x3 rotation matrix.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Matrix3<real_t>
quaternionToRotationMatrix(const Eigen::Vector4<real_t>& quaternion);

/**
    @brief Quaternion normalization and reshaping to 3x1.

    @param quat
    Input parameter - quaternion vector 4x1.

    @return
    If valid, function returns 3x1 normalized quaternion vector.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector3<real_t>
quaternionToNormQuat(const Eigen::Vector4<real_t>& quat);

/**
    @brief Calculate quaternion from normalized quaternion.

    @param normQuat
    Input parameter - Normalized quaternion vector 3x1.

    @return
    If valid, function returns 4x1 quaternion vector.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector4<real_t>
normQuatToQuaternion(const Eigen::Vector3<real_t>& normQuat);

/**
    @brief Calculate quaternion from rotation vector.

    @param vec
        Input parameter - rotation vector with 3x1 shape.

    @return
        If valid, function returns 4x1  quaternion vector.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector4<real_t>
vectorToQuaternion(const Eigen::Vector3<real_t>& vec);

/**
    @brief Get transformation matrix with minimal reprojection error from array of matrices.

    @param transformations
    Input parameter - Array of transformation matrices.

    @return
    If valid, function returns 4x4  transformation matrix.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Matrix4<real_t>
averageTransformationMatrices(const std::vector<Eigen::Matrix4<real_t>>& transformations);

/**
    @brief Multiply quaternions.

    @param q1
        Input parameter - Quaternion of 4x1 shape.

    @param q2
        Input parameter - Quaternion of 4x1 shape.

    @return
        If valid, function returns quaternion product.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector4<real_t>
quaternionMultFast(const Eigen::Vector4<real_t>& q1, const Eigen::Vector4<real_t>& q2);

/**
   @brief Triangulate point pair e.g. calculate 3D point coordinates.

   @param p2d1
    Input parameter - 2D point coordinates on first camera image plane.

   @param p2d2
    Input parameter - 2D point coordinates on second camera image plane.

   @param K1
    Input parameter - Intrinsics matrix of first camera.

   @param K2
    Input parameter - Intrinsics matrix of second camera.

   @param T1
    Input parameter - Transformation matrix of first camera.

   @param T2
    Input parameter - Transformation matrix of second camera.

   @return
    If valid, function returns coordinates of estimated 3D point (3x1).
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<Eigen::MatrixX<real_t>> triangulatePoint(const Eigen::Vector2<real_t>& p2d1, const Eigen::Vector2<real_t>& p2d2, const Eigen::Matrix3<real_t>& K1, const Eigen::Matrix3<real_t>& K2, const Eigen::Matrix4<real_t>& T1, const Eigen::Matrix4<real_t>& T2);

/**
    @brief Triangulate point pairs e.g. calculate 3D points coordinates.

    @param p2d1
    Input parameter - Array of 2D point coordinates on first camera image plane.

    @param p2d2
    Input parameter - Array of 2D point coordinates on second camera image plane.

    @param K1
        Input parameter - Intrinsics matrix of first camera.

    @param K2
        Input parameter - Intrinsics matrix of second camera.

    @param T1
        Input parameter - Transformation matrix of first camera.

    @param T2
        Input parameter - Transformation matrix of second camera.

    @return
        If valid, function returns array of coordinates of estimated 3D points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<Eigen::MatrixX<real_t>> triangulatePoints(const Eigen::MatrixX<real_t>& p2d1, const Eigen::MatrixX<real_t>& p2d2, const Eigen::Matrix3<real_t>& K1, const Eigen::Matrix3<real_t>& K2, const Eigen::Matrix4<real_t>& T1, const Eigen::Matrix4<real_t>& T2);

/**
   @brief Calculate mean squared error between points.

   @param lhs
   Input parameter - Array of point coordinates.

   @param rhs
   Input parameter - Array of point coordinates.

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calculateMeanSquaredError(const Eigen::MatrixX<real_t>& lhs, const Eigen::MatrixX<real_t>& rhs);

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void transformPoints(
    Eigen::MatrixX<real_t>& points,
    const Eigen::Matrix4<real_t>& transform);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
