// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/geometry/BarycentricCoordinates.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * A way of storing data to represent the closest point on a mesh in terms of BaryCentric coordinates for a triangle, and delta Vector from the triangle 
 * in terms of its local reference frame (which allows the mesh to be rotated and still obtain a sensible offset)
 */
template <class T>
struct ClosestPointData
{
    ClosestPointData() { }

    static constexpr int32_t version = 1;
    BarycentricCoordinates<T> bcs;
    T weight;
    Eigen::Vector<T, 3> deltaInLocalCoordSystem; // x = delta.dot(Tangent vector), y = delta.dot(Bitangent vector), z = delta.dot(normal vector)

    //! calculate the transformed point in the local coordinate frame; also allows an offset along the normal if we want to offset the results
    Eigen::Vector<T, 3> CalculateTransformedPoint(const Eigen::Matrix<T, 3, -1>& inTransformedMeshVertices, T normalOffset = 0) const;

    ClosestPointData(const Eigen::Matrix<T, 3, -1>& inMeshVertices, const Eigen::Vector<T, 3>& inPoint,
        const BarycentricCoordinates<T>& inBcs, const T& inWeight);

private:
    void CalculateLocalCoordSystem(const Eigen::Matrix<T, 3, -1>& inMeshVertices, Eigen::Vector<T, 3>& outTangent,
        Eigen::Vector<T, 3>& outBitangent, Eigen::Vector<T, 3>& outNormal) const;
};

template <class T>
bool ToBinaryFile(FILE* pFile, const ClosestPointData<T>& closestPointData);

template <class T>
bool FromBinaryFile(FILE* pFile, ClosestPointData<T>& closestPointData);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
