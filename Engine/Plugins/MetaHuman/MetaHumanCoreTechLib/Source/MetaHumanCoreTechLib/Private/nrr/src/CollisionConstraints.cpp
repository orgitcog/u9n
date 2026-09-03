// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/CollisionConstraints.h>

#include <carbon/geometry/AABBTree.h>
#include <nls/functions/BarycentricCoordinatesFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/SubtractFunction.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

enum class KeepTrianglesMode
{
    ALL,
    ANY
};

template <class T>
Eigen::Matrix<int, 3, -1> KeepTrianglesInMask(const Mesh<T>& mesh, const std::vector<int>& mask, KeepTrianglesMode mode)
{
    Mesh<T> newMesh = mesh;
    newMesh.Triangulate();

    std::vector<bool> used(newMesh.NumVertices(), false);
    for (int vID : mask)
    {
        used[vID] = true;
    }
    std::vector<Eigen::Vector3i> triangles;
    for (int tID = 0; tID < newMesh.NumTriangles(); ++tID)
    {
        int count = used[newMesh.Triangles()(0, tID)] ? 1 : 0;
        count += used[newMesh.Triangles()(1, tID)] ? 1 : 0;
        count += used[newMesh.Triangles()(2, tID)] ? 1 : 0;
        if (((mode == KeepTrianglesMode::ALL) && (count == 3)) ||
            ((mode == KeepTrianglesMode::ANY) && (count > 0)))
        {
            triangles.push_back(newMesh.Triangles().col(tID));
        }
    }

    return Eigen::Map<const Eigen::Matrix<int, 3, -1>>((const int*)triangles.data(), 3, triangles.size());
}

template <class T>
void CollisionConstraints<T>::SetSourceTopology(const Mesh<T>& mesh, const std::vector<int>& srcMask)
{
    m_srcMask = srcMask;
    m_srcTriangles = KeepTrianglesInMask(mesh, srcMask, KeepTrianglesMode::ANY);
}

template <class T>
void CollisionConstraints<T>::SetTargetTopology(const Mesh<T>& mesh, const std::vector<int>& targetMask)
{
    m_targetMask = targetMask;
    m_targetTriangles = KeepTrianglesInMask(mesh, targetMask, KeepTrianglesMode::ALL);
}

template <class T>
static void CalculateVertexNormals(const Eigen::Matrix<T, 3, -1>& vertices, Eigen::Matrix<T, 3, -1>& vertexNormals, const Eigen::Matrix<int, 3, -1>& tris)
{
    vertexNormals = Eigen::Matrix<T, 3, -1>::Zero(3, vertices.cols());
    for (int i = 0; i < (int)tris.cols(); i++)
    {
        const auto vID0 = tris(0, i);
        const auto vID1 = tris(1, i);
        const auto vID2 = tris(2, i);

        const auto v0 = vertices.col(vID0);
        const auto v1 = vertices.col(vID1);
        const auto v2 = vertices.col(vID2);

        const Eigen::Vector3<T> fn = (v1 - v0).cross(v2 - v0);

        vertexNormals.col(vID0) += fn;
        vertexNormals.col(vID1) += fn;
        vertexNormals.col(vID2) += fn;
    }
    for (auto k = 0; k < vertexNormals.cols(); ++k)
    {
        vertexNormals.col(k).stableNormalize();
    }
}

template <class T>
std::shared_ptr<const typename CollisionConstraints<T>::Data>
CollisionConstraints<T>::CalculateCollisions(const Mesh<T>& srcMesh, const Mesh<T>& targetMesh) const
{
    return CalculateCollisions(srcMesh.Vertices(), srcMesh.VertexNormals(), targetMesh.Vertices(), targetMesh.VertexNormals());
}

template <class T>
std::shared_ptr<const typename CollisionConstraints<T>::Data>
CollisionConstraints<T>::CalculateCollisions(const Eigen::Matrix<T, 3, -1>& srcVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const
{
    return CalculateCollisions(srcVertices, Eigen::Matrix<T, 3, -1>(), targetVertices, Eigen::Matrix<T, 3, -1>());
}

template <class T>
std::shared_ptr<const typename CollisionConstraints<T>::Data>
CollisionConstraints<T>::CalculateCollisions(const Eigen::Matrix<T, 3, -1>& srcVertices,
                                             const Eigen::Matrix<T, 3, -1>& srcNormals,
                                             const Eigen::Matrix<T, 3, -1>& targetVertices,
                                             const Eigen::Matrix<T, 3, -1>& targetNormals) const
{
    if ((m_srcTriangles.size() == 0) || (m_targetTriangles.size() == 0))
    {
        return nullptr;
    }

    if (srcNormals.size() == 0)
    {
        // calculate src normals is not available
        Eigen::Matrix<T, 3, -1> newSrcNormals;
        CalculateVertexNormals<T>(srcVertices, newSrcNormals, m_srcTriangles);
        return CalculateCollisions(srcVertices, newSrcNormals, targetVertices, targetNormals);
    }
    if (targetNormals.size() == 0)
    {
        // calculate target normals if not available
        Eigen::Matrix<T, 3, -1> newTargetNormals;
        CalculateVertexNormals<T>(targetVertices, newTargetNormals, m_targetTriangles);
        return CalculateCollisions(srcVertices, srcNormals, targetVertices, newTargetNormals);
    }

    std::shared_ptr<Data> collisionConstraintsData = std::make_shared<Data>();

    TITAN_NAMESPACE::AABBTree<T> srcAabbTree(srcVertices.transpose(), m_srcTriangles.transpose());
    TITAN_NAMESPACE::AABBTree<T> targetAabbTree(targetVertices.transpose(), m_targetTriangles.transpose());

    // using approach from "Ray-traced collision detection for deformable bodies", Hermann et al
    for (int vID : m_srcMask)
    {
        // search for each vertex along the negative direction where it intersects the target mesh
        // const Eigen::Vector3<T> srcNormal = srcMesh.VertexNormals().col(vID);
        const Eigen::Vector3<T> srcNormal = srcNormals.col(vID);
        auto [tID, bcWeights, dist] = targetAabbTree.intersectRay(srcVertices.col(vID).transpose(), -srcNormal.transpose());
        if (tID >= 0)
        {
            // a collision is only possible if the ray is hitting the inner surface of the target mesh
            BarycentricCoordinates<T> bc(m_targetTriangles.col(tID), bcWeights.transpose());
            // const Eigen::Vector3<T> targetNormal = bc.template Evaluate<3>(targetMesh.VertexNormals());
            const Eigen::Vector3<T> targetNormal = bc.template Evaluate<3>(targetNormals);
            if (targetNormal.dot(srcNormal) < 0)
            {
                // check that the collision from target to src is the same
                const Eigen::Vector3<T> targetVertex = bc.template Evaluate<3>(targetVertices);
                auto [tID2, bcWeights2, dist2] = srcAabbTree.intersectRay(targetVertex.transpose(), srcNormal.transpose());
                if (fabs(dist - dist2) < 1e-3)
                {
                    collisionConstraintsData->srcCollisionIDs.push_back(vID);
                    collisionConstraintsData->targetCollisionBCs.push_back(bc);
                    const Eigen::Vector3<T> collisionNormal = (srcNormal - targetNormal).normalized();
                    collisionConstraintsData->targetNormals.push_back(collisionNormal);
                }
            }
        }
    }

    return collisionConstraintsData;
}

template <class T>
DiffData<T> CollisionConstraints<T>::Data::Evaluate(const DiffDataMatrix<T, 3, -1>& srcVertices, const DiffDataMatrix<T, 3, -1>& targetVertices) const
{
    DiffDataMatrix<T, 3, -1> srcCollisionVertices = GatherFunction<T>::template GatherColumns<3, -1, -1>(srcVertices, srcCollisionIDs);
    DiffDataMatrix<T, 3, -1> targetCollisionPoints = BarycentricCoordinatesFunction<T, 3>::Evaluate(targetVertices, targetCollisionBCs);
    DiffDataMatrix<T, 3, -1> offsetMatrix = srcCollisionVertices - targetCollisionPoints;

    const int numConstraints = srcCollisionVertices.Cols();
    int validConstraints = 0;

    Vector<T> result(numConstraints);
    for (int i = 0; i < numConstraints; ++i)
    {
        validConstraints++;
        const T cost = targetNormals[i].dot(offsetMatrix.Matrix().col(i));
        if (cost > 0)
        {
            result[i] = cost;
            validConstraints++;
        }
        else
        {
            result[i] = 0;
        }
    }

    JacobianConstPtr<T> Jacobian;
    if (offsetMatrix.HasJacobian() && (validConstraints > 0))
    {
        SparseMatrix<T> localJacobian(numConstraints, offsetMatrix.Size());
        localJacobian.reserve(validConstraints * 3);
        for (int i = 0; i < numConstraints; ++i)
        {
            localJacobian.startVec(i);
            if (result[i] > 0)
            {
                for (int k = 0; k < 3; ++k)
                {
                    localJacobian.insertBackByOuterInner(i, 3 * i + k) = targetNormals[i][k];
                }
            }
        }
        localJacobian.finalize();
        Jacobian = offsetMatrix.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(result), Jacobian);
}

// explicitly instantiate the CollisionConstraints classes
template class CollisionConstraints<float>;
template class CollisionConstraints<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
