// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/TemplateConstraints.h>

#include <carbon/geometry/AABBTree.h>
#include <nls/functions/BarycentricCoordinatesFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/functions/PointPointConstraintFunction.h>

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
void TemplateConstraints<T>::SetSourceTopology(const Mesh<T>& mesh, const std::vector<int>& srcMask)
{
    m_srcMask = srcMask;
    m_srcTriangles = KeepTrianglesInMask(mesh, srcMask, KeepTrianglesMode::ANY);
}

template <class T>
void TemplateConstraints<T>::SetTargetTopology(const Mesh<T>& mesh, const std::vector<int>& targetMask)
{
    m_targetMask = targetMask;
    m_targetTriangles = KeepTrianglesInMask(mesh, targetMask, KeepTrianglesMode::ALL);
}

template <class T>
std::shared_ptr<const typename TemplateConstraints<T>::Data>
TemplateConstraints<T>::SetupCorrespondences(const Eigen::Matrix<T, 3, -1>& srcVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const
{
    if ((m_srcTriangles.size() == 0) || (m_targetTriangles.size() == 0))
    {
        return nullptr;
    }

    std::shared_ptr<Data> templateConstraintsData = std::make_shared<Data>();

    TITAN_NAMESPACE::AABBTree<T> targetAabbTree(targetVertices.transpose(), m_targetTriangles.transpose());
    std::vector<Eigen::Vector3<T>> deltaVectors;

    for (int vID : m_srcMask)
    {
        auto [tID, bcWeights, dist] = targetAabbTree.getClosestPoint(srcVertices.col(vID).transpose(), T(1e3));
        if (tID >= 0)
        {
            BarycentricCoordinates<T> bc(m_targetTriangles.col(tID), bcWeights.transpose());

            const Eigen::Vector3<T> targetVertex = bc.template Evaluate<3>(targetVertices);

            templateConstraintsData->srcIDs.push_back(vID);
            templateConstraintsData->targetTemplateBCs.push_back(bc);
            const Eigen::Vector3<T> templateDelta = targetVertex - srcVertices.col(vID);
            deltaVectors.push_back(templateDelta);

        }
    }

    templateConstraintsData->templateDeltas = Eigen::Matrix3X<T>::Zero(3, deltaVectors.size());
    for (int i = 0; i < (int)deltaVectors.size(); ++i)
    {
        templateConstraintsData->templateDeltas.col(i) = deltaVectors[i];
    }

    return templateConstraintsData;
}


template <class T>
DiffData<T> TemplateConstraints<T>::Data::Evaluate(const DiffDataMatrix<T, 3, -1>& srcVertices, const DiffDataMatrix<T, 3, -1>& targetVertices) const
{
    Eigen::VectorX<T> weights = Eigen::VectorX<T>::Ones(srcIDs.size());

    DiffDataMatrix<T, 3, -1> gatheredSrcVertices = GatherFunction<T>::template GatherColumns<3, -1, -1>(srcVertices, srcIDs);
    DiffDataMatrix<T, 3, -1> targetPoints = BarycentricCoordinatesFunction<T, 3>::Evaluate(targetVertices, targetTemplateBCs);
    DiffDataMatrix<T, 3, -1> currentDeltasMatrix = targetPoints - gatheredSrcVertices;

    return PointPointConstraintFunction<T, 3>::Evaluate(currentDeltasMatrix,
                                                        templateDeltas,
                                                        weights,
                                                        T(1));
}

// explicitly instantiate the TemplateConstraints classes
template class TemplateConstraints<float>;
template class TemplateConstraints<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
