// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/LipClosureConstraints.h>

#include <carbon/geometry/AABBTree.h>
#include <nls/functions/BarycentricCoordinatesFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/utils/ConfigurationParameter.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct LipClosureConstraints<T>::Private
{
    std::vector<int> srcMask;
    Eigen::Matrix<int, 3, -1> srcTriangles;
    std::vector<std::vector<int>> srcContourLines;
    std::vector<int> targetMask;
    Eigen::Matrix<int, 3, -1> targetTriangles;
    std::vector<std::vector<int>> targetContourLines;
    LipClosure3D<T> lipClosure3D;

    LipClosureConstraintsData<T> lipClosureConstraintsData;

    T penetration = T(0.1);

    Configuration settings = { LipClosureConstraints<T>::ConfigName(), {
                                   //!< weighting of the lip closure constraint
                                   { "lip closure weight", ConfigurationParameter(0.f, 0.f, 10.f) },
                                   //!< the desired amount of pentration between the lower and upper lips
                                   { "penetration", ConfigurationParameter(T(0.02), T(0), T(0.1)) }
                               } };
};

template <class T>
LipClosureConstraints<T>::LipClosureConstraints() : m(new Private)
{}

template <class T> LipClosureConstraints<T>::~LipClosureConstraints() = default;
template <class T> LipClosureConstraints<T>::LipClosureConstraints(LipClosureConstraints&&) = default;
template <class T> LipClosureConstraints<T>& LipClosureConstraints<T>::operator=(LipClosureConstraints&&) = default;

template <class T> /* */Configuration& LipClosureConstraints<T>::Config()/* */{ return m->settings; }
template <class T> const Configuration& LipClosureConstraints<T>::Config() const { return m->settings; }

template <class T>
Eigen::Matrix<int, 3, -1> KeepTrianglesInMask(const Mesh<T>& newMesh, const std::vector<int>& mask)
{
    std::vector<bool> used(newMesh.NumVertices(), false);
    for (int vID : mask)
    {
        used[vID] = true;
    }
    std::vector<Eigen::Vector3i> triangles;
    for (int tID = 0; tID < newMesh.NumTriangles(); ++tID)
    {
        if (used[newMesh.Triangles()(0, tID)] &&
            used[newMesh.Triangles()(1, tID)] &&
            used[newMesh.Triangles()(2, tID)])
        {
            triangles.push_back(newMesh.Triangles().col(tID));
        }
    }

    return Eigen::Map<const Eigen::Matrix<int, 3, -1>>((const int*)triangles.data(), 3, triangles.size());
}

template <class T>
void LipClosureConstraints<T>::SetTopology(const Mesh<T>& mesh,
                                           const std::vector<int>& upperLipMask,
                                           const std::vector<std::vector<int>>& upperContourLines,
                                           const std::vector<int>& lowerLipMask,
                                           const std::vector<std::vector<int>>& lowerContourLines,
                                           TITAN_NAMESPACE::TaskThreadPool* taskThreadPool)
{
    Mesh<T> newMesh = mesh;
    newMesh.Triangulate(Mesh<T>::TriangulationMethod::Quality, taskThreadPool);

    m->srcMask = upperLipMask;
    m->srcTriangles = KeepTrianglesInMask(newMesh, upperLipMask);
    m->srcContourLines = upperContourLines;
    m->targetMask = lowerLipMask;
    m->targetTriangles = KeepTrianglesInMask(newMesh, lowerLipMask);
    m->targetContourLines = lowerContourLines;
}

template <class T>
void LipClosureConstraints<T>::SetLipClosure(const LipClosure3D<T>& lipClosure3D) { m->lipClosure3D = lipClosure3D; }

template <class T>
void LipClosureConstraints<T>::ResetLipClosure() { m->lipClosure3D.Reset(); }

template <class T>
bool LipClosureConstraints<T>::ValidLipClosure() const { return m->lipClosure3D.Valid() && (m->srcMask.size() > 0); }

template <class T>
void LipClosureConstraints<T>::CalculateLipClosureData(const Eigen::Matrix<T, 3, -1>& vertices,
                                                       const Eigen::Matrix<T, 3, -1>& normals,
                                                       const Eigen::Transform<T, 3, Eigen::Affine>& transform,
                                                       bool useLandmarks,
                                                       const Eigen::Transform<T, 3,
                                                                              Eigen::Affine>& toFaceTransform,
                                                       TITAN_NAMESPACE::TaskThreadPool* taskThreadPool)
{
    m->lipClosureConstraintsData.Clear();

    if (useLandmarks && !ValidLipClosure()) { return; }

    std::unique_ptr<TITAN_NAMESPACE::AABBTree<T>> srcAabbTree;
    std::unique_ptr<TITAN_NAMESPACE::AABBTree<T>> targetAabbTree;

    auto makeAABBTree = [&](const Eigen::Matrix<int, 3, -1>& triangles) {
            return std::make_unique<TITAN_NAMESPACE::AABBTree<T>>(vertices.transpose(), triangles.transpose());
        };

    if (taskThreadPool)
    {
        TITAN_NAMESPACE::TaskFutures taskFutures;

        taskFutures.Add(taskThreadPool->AddTask([&]() {
                srcAabbTree = makeAABBTree(m->srcTriangles);
            }));

        taskFutures.Add(taskThreadPool->AddTask([&]() {
                targetAabbTree = makeAABBTree(m->targetTriangles);
            }));

        taskFutures.Wait();
    }
    else
    {
        srcAabbTree = makeAABBTree(m->srcTriangles);
        targetAabbTree = makeAABBTree(m->targetTriangles);
    }

    auto removeXComponent = [&](Eigen::Vector3<T> normal) {
            normal = toFaceTransform.linear() * normal;
            normal[0] = 0;
            normal.normalize();
            normal = toFaceTransform.linear().transpose() * normal;
            return normal;
        };

    auto createLipClosureDataPart = [&](const std::vector<std::vector<int>>& contourLines,
                                        const Eigen::Matrix<int, 3, -1>& triangles,
                                        const TITAN_NAMESPACE::AABBTree<T>& aabbTree,
                                        std::vector<int>& bestSrcVIDArray,
                                        std::vector<BarycentricCoordinates<T>>& bestTargetBCArray,
                                        std::vector<Eigen::Vector3<T>>& bestNormalArray,
                                        std::vector<T>& bestWeightArray) {
            bestSrcVIDArray = std::vector<int>(contourLines.size(), -1);
            bestTargetBCArray = std::vector<BarycentricCoordinates<T>>(contourLines.size());
            bestNormalArray = std::vector<Eigen::Vector3<T>>(contourLines.size());
            bestWeightArray = std::vector<T>(contourLines.size(), T(1));

            auto process = [&](int start, int end) {
                    for (int i = start; i < end; ++i)
                    {
                        T maxPenetration = std::numeric_limits<T>::lowest();
                        BarycentricCoordinates<T> bestTargetBC;
                        int bestSrcVID = -1;
                        Eigen::Vector3<T> bestNormal { 0, 0, 0 };
                        T bestWeight = T(1);
                        for (int vID : contourLines[i])
                        {
                            const Eigen::Vector3<T> srcVertex = vertices.col(vID);
                            const Eigen::Vector3<T> srcNormal = removeXComponent(normals.col(vID));
                            auto [tID, bcWeights, dist] = aabbTree.intersectRayBidirectional(srcVertex.transpose(), srcNormal.transpose());
                            if (tID >= 0)
                            {
                                const BarycentricCoordinates<T> bc(triangles.col(tID), bcWeights.transpose());
                                const Eigen::Vector3<T> targetVertex = bc.template Evaluate<3>(vertices);
                                const Eigen::Vector3<T> targetNormal = removeXComponent(bc.template Evaluate<3>(normals));
                                const T dir = targetNormal.dot(srcNormal);
                                const T penetration = srcNormal.dot(srcVertex - targetVertex);
                                if ((dir < 0) && (penetration > maxPenetration))
                                {
                                    maxPenetration = penetration;
                                    bestSrcVID = vID;
                                    bestTargetBC = bc;
                                    bestNormal = srcNormal - targetNormal;
                                    if (useLandmarks)
                                    {
                                        bestWeight = m->lipClosure3D.Valid() ? m->lipClosure3D.ClosureValue(transform * srcVertex) : 0;
                                    }
                                }
                            }
                        }
                        if ((bestSrcVID >= 0) && (bestWeight >= 0))
                        {
                            bestSrcVIDArray[i] = bestSrcVID;
                            bestTargetBCArray[i] = bestTargetBC;
                            bestNormalArray[i] = bestNormal.normalized();
                            bestWeightArray[i] = bestWeight;
                        }
                    }
                };

            if (taskThreadPool)
            {
                taskThreadPool->AddTaskRangeAndWait((int)contourLines.size(), process, 8);
            }
            else
            {
                process(0, (int)contourLines.size());
            }
        };

    std::vector<std::vector<int>> rawBestSrcVIDArray(2);
    std::vector<std::vector<BarycentricCoordinates<T>>> rawBestTargetBCArray(2);
    std::vector<std::vector<Eigen::Vector3<T>>> rawBestNormalArray(2);
    std::vector<std::vector<T>> rawBestWeightArray(2);

    auto createLipClosureDataPartSrc = [&]() {
            createLipClosureDataPart(m->srcContourLines,
                                     m->targetTriangles,
                                     *targetAabbTree,
                                     rawBestSrcVIDArray[0],
                                     rawBestTargetBCArray[0],
                                     rawBestNormalArray[0],
                                     rawBestWeightArray[0]);
        };

    auto createLipClosureDataPartTarget = [&]() {
            createLipClosureDataPart(m->targetContourLines,
                                     m->srcTriangles,
                                     *srcAabbTree,
                                     rawBestSrcVIDArray[1],
                                     rawBestTargetBCArray[1],
                                     rawBestNormalArray[1],
                                     rawBestWeightArray[1]);
        };

    if (taskThreadPool)
    {
        TITAN_NAMESPACE::TaskFutures taskFutures;

        taskFutures.Add(taskThreadPool->AddTask([&]() {
                createLipClosureDataPartSrc();
            }));

        taskFutures.Add(taskThreadPool->AddTask([&]() {
                createLipClosureDataPartTarget();
            }));

        taskFutures.Wait();
    }
    else
    {
        createLipClosureDataPartSrc();
        createLipClosureDataPartTarget();
    }

    for (size_t i = 0; i < rawBestSrcVIDArray.size(); ++i)
    {
        for (size_t j = 0; j < rawBestSrcVIDArray[i].size(); ++j)
        {
            if ((rawBestSrcVIDArray[i][j] >= 0) && (rawBestWeightArray[i][j] >= 0))
            {
                m->lipClosureConstraintsData.srcIDs.push_back(rawBestSrcVIDArray[i][j]);
                m->lipClosureConstraintsData.targetBCs.push_back(rawBestTargetBCArray[i][j]);
                m->lipClosureConstraintsData.normals.push_back(rawBestNormalArray[i][j]);
                m->lipClosureConstraintsData.weights.push_back(rawBestWeightArray[i][j]);
            }
        }
    }
}

template <class T>
void LipClosureConstraints<T>::EvaluateLipClosure(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 3, 1>& lipClosureVertexConstraints) const
{
    if (m->lipClosureConstraintsData.NumConstraints() == 0) { return; }

    const T lipClosureWeight = m->settings["lip closure weight"].template Value<T>();
    const T penetration = m->settings["penetration"].template Value<T>();
    if (lipClosureWeight <= 0) { return; }

    lipClosureVertexConstraints.ResizeToFitAdditionalConstraints(m->lipClosureConstraintsData.NumConstraints());
    for (int i = 0; i < m->lipClosureConstraintsData.NumConstraints(); ++i)
    {
        const Eigen::Vector3<T> srcVertex = vertices.col(m->lipClosureConstraintsData.srcIDs[i]);
        const BarycentricCoordinates<T, 3>& bc = m->lipClosureConstraintsData.targetBCs[i];
        const Eigen::Vector3<T> targetPos = bc.template Evaluate<3>(vertices);
        const Eigen::Vector3<T> normal = m->lipClosureConstraintsData.normals[i];
        const T totalWeight = lipClosureWeight * m->lipClosureConstraintsData.weights[i];

        // move the vertex halfway
        const Eigen::Vector3<T> newTarget = T(0.5) * (targetPos + penetration * normal) + T(0.5) * srcVertex;
        const Eigen::Vector3<T> residual = totalWeight * (srcVertex - newTarget);
        Eigen::Matrix<T, 3, 3> drdV = Eigen::Matrix<T, 3, 3>::Zero();
        for (int j = 0; j < 3; ++j)
        {
            drdV(j, j) = totalWeight;
        }
        lipClosureVertexConstraints.AddConstraint(m->lipClosureConstraintsData.srcIDs[i], residual, drdV);
    }
}

template <class T>
void LipClosureConstraints<T>::EvaluateLipClosure(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 3, 4>& lipClosureVertexConstraints) const
{
    if (m->lipClosureConstraintsData.NumConstraints() == 0) { return; }

    const T lipClosureWeight = m->settings["lip closure weight"].template Value<T>();
    const T penetration = m->settings["penetration"].template Value<T>();
    if (lipClosureWeight <= 0) { return; }

    lipClosureVertexConstraints.ResizeToFitAdditionalConstraints(m->lipClosureConstraintsData.NumConstraints());
    for (int i = 0; i < m->lipClosureConstraintsData.NumConstraints(); ++i)
    {
        const Eigen::Vector3<T> srcVertex = vertices.col(m->lipClosureConstraintsData.srcIDs[i]);
        const BarycentricCoordinates<T, 3>& bc = m->lipClosureConstraintsData.targetBCs[i];
        const Eigen::Vector3<T> targetPos = bc.template Evaluate<3>(vertices);
        const Eigen::Vector3<T> normal = m->lipClosureConstraintsData.normals[i];
        const T totalWeight = lipClosureWeight * m->lipClosureConstraintsData.weights[i];

        const Eigen::Vector3<T> newTarget = targetPos + penetration * normal;
        const Eigen::Vector3<T> residual = totalWeight * (srcVertex - newTarget);
        Eigen::Matrix<T, 3, 3> drdV = Eigen::Matrix<T, 3, 3>::Identity();
        Eigen::Vector<T, 4> weightsPerVertex(totalWeight, -totalWeight * bc.Weight(0), -totalWeight * bc.Weight(1), -totalWeight * bc.Weight(2));
        const Eigen::Vector4i indices(m->lipClosureConstraintsData.srcIDs[i], bc.Index(0), bc.Index(1), bc.Index(2));
        lipClosureVertexConstraints.AddConstraint(indices, weightsPerVertex, residual, drdV);
    }
}

template <class T>
void LipClosureConstraints<T>::GetLipClosureData(LipClosureConstraintsData<T>& lipClosureConstraintsData) const
{
    lipClosureConstraintsData = m->lipClosureConstraintsData;
}

// explicitly instantiate the LipClosureConstraints classes
template class LipClosureConstraints<float>;
template class LipClosureConstraints<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
