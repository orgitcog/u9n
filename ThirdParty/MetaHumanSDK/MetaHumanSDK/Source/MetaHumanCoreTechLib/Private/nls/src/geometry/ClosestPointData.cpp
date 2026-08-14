// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/ClosestPointData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template<class T>
Eigen::Vector<T, 3> ClosestPointData<T>::CalculateTransformedPoint(const Eigen::Matrix<T, 3, -1>& inTransformedMeshVertices, T normalOffset) const
{
    Eigen::Vector<T, 3> tangent, bitangent, normal;
    CalculateLocalCoordSystem(inTransformedMeshVertices, tangent, bitangent, normal);
    Eigen::Vector<T, 3> transformedPoint = bcs.template Evaluate<3>(inTransformedMeshVertices)
        + deltaInLocalCoordSystem.x() * tangent + deltaInLocalCoordSystem.y() * bitangent + (deltaInLocalCoordSystem.z() + normalOffset) * normal;
    return transformedPoint;
}

template <class T>
ClosestPointData<T>::ClosestPointData(const Eigen::Matrix<T, 3, -1>& inMeshVertices, const Eigen::Vector<T, 3>& inPoint,
    const BarycentricCoordinates<T>& inBcs, const T& inWeight)
    : bcs(inBcs)
    , weight(inWeight)
{
    for (int i = 0; i < 3; ++i)
    {
        if (inBcs.Index(i) < 0 || inBcs.Index(i) >= inMeshVertices.cols())
        {
            CARBON_CRITICAL("Barycentric coordinate {} is out of range for the supplied {} mesh vertices", inBcs.Index(i), inMeshVertices.cols());
        }
    }
    Eigen::Vector<T, 3> tangent, bitangent, normal;
    CalculateLocalCoordSystem(inMeshVertices, tangent, bitangent, normal);
    Eigen::Vector<T, 3> delta = inPoint - bcs.template Evaluate<3>(inMeshVertices);
    deltaInLocalCoordSystem = { delta.dot(tangent), delta.dot(bitangent), delta.dot(normal) };
}

template <class T>
void ClosestPointData<T>::CalculateLocalCoordSystem(const Eigen::Matrix<T, 3, -1>& inMeshVertices, Eigen::Vector<T, 3>& outTangent,
    Eigen::Vector<T, 3>& outBitangent, Eigen::Vector<T, 3>& outNormal) const
{
    // TODO what if mesh vertices are coincident? need to handle this
    Eigen::Vector<T, 3> edge1 = inMeshVertices.col(bcs.Index(1)) - inMeshVertices.col(bcs.Index(0));
    Eigen::Vector<T, 3> edge2 = inMeshVertices.col(bcs.Index(2)) - inMeshVertices.col(bcs.Index(0));
    outTangent = edge1.normalized();
    outBitangent = (edge2 - edge2.dot(outTangent) * outTangent).normalized();
    outNormal = (edge1.cross(edge2)).normalized();
}


template <class T>
bool ToBinaryFile(FILE* pFile, const ClosestPointData<T>& closestPointData)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, closestPointData.version);
    success &= ToBinaryFile(pFile, closestPointData.bcs);
    success &= io::ToBinaryFile(pFile, closestPointData.weight);
    success &= io::ToBinaryFile(pFile, closestPointData.deltaInLocalCoordSystem);
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, ClosestPointData<T>& closestPointData)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 1)
    {
        success &= FromBinaryFile(pFile, closestPointData.bcs);
        success &= io::FromBinaryFile(pFile, closestPointData.weight);
        success &= io::FromBinaryFile(pFile, closestPointData.deltaInLocalCoordSystem);
    }
    else
    {
        success = false;
    }
    return success;
}

template struct ClosestPointData<float>;
template struct ClosestPointData<double>;

template bool ToBinaryFile(FILE* pFile, const ClosestPointData<float>& closestPointData);
template bool ToBinaryFile(FILE* pFile, const ClosestPointData<double>& closestPointData);

template bool FromBinaryFile(FILE* pFile, ClosestPointData<float>& closestPointData);
template bool FromBinaryFile(FILE* pFile, ClosestPointData<double>& closestPointData);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
