// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/SubdivisionMesh.h>
#include <nls/rendering/Rasterizer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void SubdivisionMesh<T>::Create(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights)
{
    m_stencilWeights = stencilWeights;
    m_subdivisionMesh = subdivisionTopology;
    m_triangulatedSubdivisionMesh = m_subdivisionMesh;
    m_triangulatedSubdivisionMesh.Triangulate();
    m_triangulatedSubdivisionMesh.CalculateVertexNormals();
    m_subdivisionMesh.SetVertexNormals(m_triangulatedSubdivisionMesh.VertexNormals());
}


template <class T>
void SubdivisionMesh<T>::SetBaseMesh(const Mesh<T>& mesh)
{
    SetBaseVertices(mesh.Vertices());
}

template <class T>
void SubdivisionMesh<T>::SetBaseVertices(const Eigen::Matrix<T, 3, -1>& vertices)
{
    Eigen::Matrix<T, 3, -1> newVertices = Eigen::Matrix<T, 3, -1>::Zero(3, m_subdivisionMesh.NumVertices());
    for (const auto& [subdiv_vID, base_vID, weight] : m_stencilWeights) {
        newVertices.col(subdiv_vID) += weight * vertices.col(base_vID);
    }
    m_triangulatedSubdivisionMesh.SetVertices(newVertices);
    m_triangulatedSubdivisionMesh.CalculateVertexNormals();
    m_subdivisionMesh.SetVertices(newVertices);
    m_subdivisionMesh.SetVertexNormals(m_triangulatedSubdivisionMesh.VertexNormals());
}

template <class T>
Mesh<T> SubdivisionMesh<T>::EvaluateMesh(Eigen::VectorX<T> vertexDisplacements) const
{
    Mesh<T> newMesh = m_subdivisionMesh;
    const bool hasDisplacements = (vertexDisplacements.size() > 0);
    if (hasDisplacements) {
        Eigen::Matrix<T, 3, -1> newVertices = m_subdivisionMesh.Vertices();
        for (int vID = 0; vID < m_triangulatedSubdivisionMesh.NumVertices(); ++vID) {
            newVertices.col(vID) += vertexDisplacements[vID] * m_subdivisionMesh.VertexNormals().col(vID);
        }
        Mesh<T> triangulatedNewMesh = m_triangulatedSubdivisionMesh;
        triangulatedNewMesh.SetVertices(newVertices);
        triangulatedNewMesh.CalculateVertexNormals();
        newMesh.SetVertices(newVertices);
        newMesh.SetVertexNormals(triangulatedNewMesh.VertexNormals());
    }

    return newMesh;
}

template <class T>
Eigen::VectorX<T> SubdivisionMesh<T>::EvaluateWeights(const Eigen::VectorX<T>& baseWeights) const
{
    Eigen::VectorX<T> weights = Eigen::VectorX<T>::Zero(m_subdivisionMesh.NumVertices());
    for (const auto& [subdiv_vID, base_vID, weight] : m_stencilWeights) {
        weights[subdiv_vID] += weight * baseWeights[base_vID];
    }
    return weights;
}

template class SubdivisionMesh<float>;
template class SubdivisionMesh<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
