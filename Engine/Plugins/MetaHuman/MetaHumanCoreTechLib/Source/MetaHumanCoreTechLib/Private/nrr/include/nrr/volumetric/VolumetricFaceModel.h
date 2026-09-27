// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/BarycentricEmbedding.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/TetMesh.h>
#include <nrr/VertexWeights.h>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class VolumetricFaceModel
{
public:
    bool Load(const std::string& directory);
    bool Save(const std::string& directory, bool forceOverwrite = false) const;

    const std::string GetCraniumMeshName() const;
    const std::string GetMandibleMeshName() const;
    const std::string GetFleshMeshName() const;

    const Mesh<T>& GetSkinMesh() const { return m_skinMesh; }
    const Mesh<T>& GetFleshMesh() const { return m_fleshMesh; }
    const Mesh<T>& GetCraniumMesh() const { return m_craniumMesh; }
    const Mesh<T>& GetMandibleMesh() const { return m_mandibleMesh; }
    const Mesh<T>& GetTeethMesh() const { return m_teethMesh; }
    const TetMesh<T>& GetTetMesh() const { return m_tetMesh; }
    const BarycentricEmbedding<T>& Embedding() const { return m_embedding; }
    const std::vector<std::pair<int, int>>& SkinFleshMapping() const { return m_skinFleshMapping; }
    const std::vector<std::pair<int, int>>& CraniumFleshMapping() const { return m_craniumFleshMapping; }
    const std::vector<std::pair<int, int>>& MandibleFleshMapping() const { return m_mandibleFleshMapping; }
    const std::vector<std::pair<int, BarycentricCoordinates<T>>>& TetFleshMapping() const { return m_tetFleshMapping; }
    const std::vector<std::pair<int, BarycentricCoordinates<T>>>& FleshTetMapping() const { return m_fleshTetMapping; }

    void SetSkinMeshVertices(const Eigen::Matrix<T, 3, -1>& skinVertices);
    void SetFleshMeshVertices(const Eigen::Matrix<T, 3, -1>& fleshVertices);
    void SetCraniumMeshVertices(const Eigen::Matrix<T, 3, -1>& craniumVertices);
    void SetMandibleMeshVertices(const Eigen::Matrix<T, 3, -1>& mandibleVertices);
    void SetTeethMeshVertices(const Eigen::Matrix<T, 3, -1>& teethVertices);
    void SetTetMeshVertices(const Eigen::Matrix<T, 3, -1>& tetVertices);

    void UpdateFleshMeshVerticesFromSkinCraniumAndMandible();

private:
    Mesh<T> m_skinMesh;
    Mesh<T> m_fleshMesh;
    Mesh<T> m_craniumMesh;
    Mesh<T> m_mandibleMesh;
    Mesh<T> m_teethMesh;

    TetMesh<T> m_tetMesh;

    BarycentricEmbedding<T> m_embedding;
    std::vector<std::pair<int, BarycentricCoordinates<T>>> m_tetFleshMapping;
    std::vector<std::pair<int, BarycentricCoordinates<T>>> m_fleshTetMapping;

    std::vector<std::pair<int, int>> m_skinFleshMapping;
    std::vector<std::pair<int, int>> m_craniumFleshMapping;
    std::vector<std::pair<int, int>> m_mandibleFleshMapping;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
