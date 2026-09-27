// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <nls/math/Math.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * SubdivisionMesh provides functionality to resample a mesh based on subdivision stencil weights.
 */
template <class T>
class SubdivisionMesh
{
public:
    void Create(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights);

    void SetBaseMesh(const Mesh<T>& mesh);
    void SetBaseVertices(const Eigen::Matrix<T, 3, -1>& vertices);

    Mesh<T> EvaluateMesh(Eigen::VectorX<T> vertexDisplacements = Eigen::VectorX<T>()) const;

    Eigen::VectorX<T> EvaluateWeights(const Eigen::VectorX<T>& baseWeights) const;

    int NumVertices() const { return m_subdivisionMesh.NumVertices(); }

    const Mesh<T>& GetSubdivisionMesh() const { return m_subdivisionMesh; }

private:
    std::vector<std::tuple<int, int, T>> m_stencilWeights;
    Mesh<T> m_subdivisionMesh;
    Mesh<T> m_triangulatedSubdivisionMesh;
};


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
