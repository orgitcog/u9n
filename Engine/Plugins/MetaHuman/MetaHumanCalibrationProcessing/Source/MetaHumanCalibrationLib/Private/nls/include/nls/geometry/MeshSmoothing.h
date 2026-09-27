// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <typename T>
class MeshSmoothing
{
public:
    void SetTopology(const Mesh<T>& mesh, T identityWeight = T(0.44), T step = T(0.5));
    void SetWeights(const Eigen::VectorX<T>& weights) { m_weights = weights; }

    void Apply(Eigen::Matrix<T, 3, -1>& vertices, int iterations);

private:
    Eigen::SparseMatrix<T, Eigen::RowMajor> m_smoothingMatrix;
    Eigen::VectorX<T> m_weights;
};

template <typename T>
void MeshSmoothing<T>::SetTopology(const Mesh<T>& mesh, T identityWeight, T step)
{
    const std::vector<std::pair<int, int>> edges = mesh.GetEdges({});
    const std::vector<int> borderVertices = mesh.CalculateBorderVertices();
    std::vector<bool> isBorder(mesh.NumVertices(), false);
    for (int vID : borderVertices) isBorder[vID] = true;
    std::vector<std::vector<int>> adjacency(mesh.NumVertices());

    for (const auto& [vID1, vID2] : edges)
    {
        if (!isBorder[vID1]) adjacency[vID1].push_back(vID2);
        if (!isBorder[vID2]) adjacency[vID2].push_back(vID1);
    }

    m_smoothingMatrix = Eigen::SparseMatrix<T, Eigen::RowMajor>(mesh.NumVertices(), mesh.NumVertices());
    std::vector<Eigen::Triplet<T>> triplets;
    for (int vID1 = 0; vID1 < mesh.NumVertices(); ++vID1)
    {
        float sum = (identityWeight + T(adjacency[vID1].size()));
        float weight = T(1) / sum;
        triplets.push_back(Eigen::Triplet<T>(vID1, vID1, identityWeight / sum * step + (T(1) - step)));
        for (int vID2 : adjacency[vID1])
        {
            triplets.push_back(Eigen::Triplet<float>(vID1, vID2, weight * step));
        }
    }
    m_smoothingMatrix.setFromTriplets(triplets.begin(), triplets.end());
}

template <typename T>
void MeshSmoothing<T>::Apply(Eigen::Matrix<T, 3, -1>& vertices, int iterations)
{
    Eigen::Matrix<float, 3, -1> initVertices = vertices;
    Eigen::Matrix<float, 3, -1> newVertices = vertices;

    for (int iter = 0; iter < iterations; ++iter)
    {
        // ParallelNoAliasGEMV<float>(Eigen::Map<Eigen::VectorXf>(newVertices.data(), newVertices.size()), smat, Eigen::Map<Eigen::VectorXf>(vertices.data(), vertices.size()), globalThreadPool.get());
        newVertices.noalias() = vertices * m_smoothingMatrix.transpose();
        std::swap(newVertices, vertices);
    }

    if (m_weights.size() == (int)vertices.cols())
    {
        vertices.array() = initVertices.array() + (vertices - initVertices).array().rowwise() * m_weights.array().transpose();
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
