// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>

#include <memory>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Class to evaluate generic collision constraints between two sets of vertices (from a single mesh or two different meshes).
 */
template <class T>
class CollisionConstraints
{
public:
    struct Data
    {
        std::vector<int> srcCollisionIDs;
        std::vector<BarycentricCoordinates<T, 3>> targetCollisionBCs;
        std::vector<Eigen::Vector3<T>> targetNormals;

        void Clear()
        {
            srcCollisionIDs.clear();
            targetCollisionBCs.clear();
            targetNormals.clear();
        }

        int NumCollisions() const { return static_cast<int>(srcCollisionIDs.size()); }

        //! Evaluate collision constraints
        DiffData<T> Evaluate(const DiffDataMatrix<T, 3, -1>& srcVertices,
                             const DiffDataMatrix<T, 3, -1>& targetVertices) const;
    };

public:
    CollisionConstraints() = default;
    ~CollisionConstraints() = default;
    CollisionConstraints(CollisionConstraints&&) = default;
    CollisionConstraints(CollisionConstraints&) = delete;
    CollisionConstraints& operator=(CollisionConstraints&&) = default;
    CollisionConstraints& operator=(const CollisionConstraints&) = delete;

    //! Set the mask for the source mesh
    void SetSourceTopology(const Mesh<T>& mesh, const std::vector<int>& srcMask);

    //! Set the mask for the target mesh (can be the same mesh as the source mesh)
    void SetTargetTopology(const Mesh<T>& mesh, const std::vector<int>& targetMask);

    std::shared_ptr<const Data> CalculateCollisions(const Mesh<T>& srcMesh, const Mesh<T>& targetMesh) const;

    //! Calculate collision points and the collision normal (calculates the required vertex normals on the fly)
    std::shared_ptr<const Data> CalculateCollisions(const Eigen::Matrix<T, 3, -1>& srcVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const;

    //! Calculate collision points and the collision normal
    std::shared_ptr<const Data> CalculateCollisions(const Eigen::Matrix<T, 3, -1>& srcVertices, const Eigen::Matrix<T, 3, -1>& srcNormals,
                                                    const Eigen::Matrix<T, 3, -1>& targetVertices, const Eigen::Matrix<T, 3, -1>& targetNormals) const;

protected:
    std::vector<int> m_srcMask;
    Eigen::Matrix<int, 3, -1> m_srcTriangles;
    std::vector<int> m_targetMask;
    Eigen::Matrix<int, 3, -1> m_targetTriangles;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
