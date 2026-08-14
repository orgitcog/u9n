// Copyright Epic Games, Inc. All Rights Reserved.

#include <rigmorpher/RigMorphModule.h>

#include <carbon/geometry/AABBTree.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/EulerAngles.h>
#include <nls/rendering/Rasterizer.h>
#include <rig/RigGeometry.h>
#include <nrr/GridDeformation.h>

#include <dna/Reader.h>
#include <dna/Writer.h>
#include <pma/TypeDefs.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// mesh with triangles expected
template <class T>
Eigen::Matrix<T, 3, -1> UpdateLowerLODVerticesRaycasting(const Mesh<T>& lod0Asset, const Mesh<T>& asset)
{
    int lod0UvCount = static_cast<int>(lod0Asset.Texcoords().cols());
    int assetVtxCount = asset.NumVertices();

    Eigen::Matrix<T, 3, -1> outputDeltas = Eigen::Matrix<T, 3, -1>::Zero(3, assetVtxCount);
    Eigen::Matrix<T, 3, -1> texcoords3d = Eigen::Matrix<T, 3, -1>::Zero(3, lod0UvCount);
    for (int i = 0; i < lod0UvCount; ++i)
    {
        texcoords3d(0, i) = lod0Asset.Texcoords()(0, i);
        texcoords3d(1, i) = lod0Asset.Texcoords()(1, i);
    }

    TITAN_NAMESPACE::AABBTree<T> aabbTree(texcoords3d.transpose(), lod0Asset.TexTriangles().transpose());

    // Find intersection for each asset vertex
    for (int face = 0; face < asset.NumTriangles(); ++face)
    {
        for (int vtx = 0; vtx < 3; ++vtx)
        {
            const Eigen::Vector2f uv = asset.Texcoords().col(asset.TexTriangles()(vtx, face));
                                                             const Eigen::Vector3<T> query = Eigen::Vector3<T>(uv[0], uv[1], 0.0f);

                                                             const auto [triangleIndex, barycentric, _] = aabbTree.getClosestPoint(query.transpose(), 1e3f);
                                                             if (triangleIndex == -1)
            {
                outputDeltas.col(asset.Triangles()(vtx, face)) = Eigen::Vector3<T> (0.0f, 0.0f, 0.0f);
                                 continue;
            }

            Eigen::Matrix<T, 3, -1> vertices = Eigen::Matrix<T, 3, -1>(3, 3);
            const BarycentricCoordinates<T> bcOut(Eigen::Vector3i(0, 1, 2),
                                                            barycentric.transpose());

            vertices.col(0) = lod0Asset.Vertices().col(lod0Asset.Triangles()(0, triangleIndex));
                                                       vertices.col(1) = lod0Asset.Vertices().col(lod0Asset.Triangles()(1, triangleIndex));
                                                                                                  vertices.col(2) = lod0Asset.Vertices().col(
                lod0Asset.Triangles()(2, triangleIndex));

                const Eigen::Vector3<T> newVertexPosition = bcOut.template Evaluate<3>(vertices);

                outputDeltas.col(asset.Triangles()(vtx, face)) = newVertexPosition - asset.Vertices().col(asset.Triangles()(vtx, face));
        }
    }

    return outputDeltas;
}

// explicitly instanciate
template Eigen::Matrix<float, 3, -1> UpdateLowerLODVerticesRaycasting(const Mesh<float>& lod0Asset, const Mesh<float>& asset);
// template Eigen::Matrix<double, 3, -1> UpdateLowerLODVerticesRaycasting(const Mesh<double>& lod0Asset, const Mesh<double>& asset);


template <class T>
void SetVertexPositionsToAsset(int assetId, Eigen::Matrix<T, 3, -1> vertices, dna::Writer* dna)
{
    size_t numMeshVertices = vertices.cols();
    dna->setVertexPositions(uint16_t(assetId), (dna::Position*)vertices.data(), uint32_t(numMeshVertices));
}

template void SetVertexPositionsToAsset(int assetId, Eigen::Matrix<float, 3, -1> vertices, dna::Writer* dna);
// template void SetVertexPositionsToAsset(int assetId, Eigen::Matrix<double, 3, -1> vertices, dna::Writer* dna);


template <class T>
void ApplyMeshesToDna(const std::map<int, Mesh<T>>& meshes, dna::Writer* writer)
{
    for (const auto& [id, mesh] : meshes)
    {
        SetVertexPositionsToAsset(id, mesh.Vertices(), writer);
    }
}

template void ApplyMeshesToDna(const std::map<int, Mesh<float>>& meshes, dna::Writer* writer);
// template void ApplyMeshesToDna(const std::map<int, Mesh<double>>& meshes, const pma::ScopedPtr<dna::Writer>& writer);


std::string FindByValue(const std::string& searchedValue, const std::map<std::string, std::vector<std::string>>& map)
{
    for (const auto& [key, value] : map)
    {
        for (const auto& subValue : value)
        {
            if (searchedValue == subValue)
            {
                return key;
            }
        }
    }

    return std::string {};
}

template <class T>
Eigen::Matrix<T, 3, -1> ApplyMask(const Eigen::Matrix<T, 3, -1>& deltas, const VertexWeights<T>& weights)
{
    Eigen::Matrix<T, 3, -1> output = deltas;
    for (int i = 0; i < output.cols(); ++i)
    {
        output.col(i) = weights.Weights()[i] * deltas.col(i);
    }
    return output;
}

template Eigen::Matrix<float, 3, -1> ApplyMask(const Eigen::Matrix<float, 3, -1>& deltas, const VertexWeights<float>& weights);
// template Eigen::Matrix<double, 3, -1> ApplyMask(const Eigen::Matrix<double, 3, -1>& deltas, const VertexWeights<double>& weights);


template <class T>
Eigen::Matrix<T, 3, -1> UpdateLowerLODVerticesRasterizer(const Mesh<T>& lod0Asset, const Mesh<T>& asset)
{
    int width = 2048;
    int height = 2048;

    Eigen::Matrix<T, 3, -1> outputDeltas = Eigen::Matrix<T, 3, -1>::Zero(3, asset.Vertices().cols());

    // Initialize raster matrices
    Eigen::MatrixXi lod0TriIndex = Eigen::MatrixXi::Zero(width, height);
    Eigen::MatrixX<T> lod0BcX = Eigen::MatrixX<T>::Zero(width, height);
    Eigen::MatrixX<T> lod0BcY = Eigen::MatrixX<T>::Zero(width, height);
    Eigen::MatrixX<T> lod0BcZ = Eigen::MatrixX<T>::Zero(width, height);

    Eigen::Matrix<T, 4, 3> bcs = Eigen::Matrix<T, 4, 3>::Zero();
    bcs.col(0) = Eigen::Vector4f(1., 0., 0., 1.);
    bcs.col(1) = Eigen::Vector4f(0., 1., 0., 1.);
    bcs.col(2) = Eigen::Vector4f(0., 0., 1., 1.);

    // Rasterize each texture triangle in LOD0 mesh
    for (int tri = 0; tri < lod0Asset.TexTriangles().cols(); tri++)
    {
        std::function<void(int x, int y, T depth,
                           const Eigen::Vector3<T>& bc)> uvFunction = [&](int x, int y, T /*depth*/,
                                                                          const Eigen::Vector3<T>& bc) {
                lod0TriIndex(x, y) = tri;
                lod0BcX(x, y) = bc[0];
                lod0BcY(x, y) = bc[1];
                lod0BcZ(x, y) = bc[2];
            };
        Eigen::Matrix<T, 3, -1> projectedVertices = Eigen::Matrix<T, 3, -1>(3, 3);
        projectedVertices.col(2) = Eigen::Vector3<T>(width * lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(0, tri))[0],
                                                                                       height * lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(0, tri))[1],
                                                                                       0.5f);
                                                     projectedVertices.col(1) =
            Eigen::Vector3<T>(width * lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(1, tri))[0],
                                                                height *
                                                                lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(1, tri))[1],
                                                                0.5f);
                              projectedVertices.col(0) = Eigen::Vector3<T>(
                width * lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(2, tri))[0],
                                                  height *
                                                  lod0Asset.Texcoords().col(lod0Asset.TexTriangles()(2, tri))[1],
                                                  0.5f
                );

                RasterizeTriangleInsideOut<T>(
                    projectedVertices, bcs, width, height, uvFunction);
    }

    // Using raster matrices find correspondance to LOD0 and calculate delta
    for (int face = 0; face < asset.NumTriangles(); ++face)
    {
        for (int vtx = 0; vtx < 3; ++vtx)
        {
            // nearest neighbor
            int u = int(width * asset.Texcoords().col(asset.TexTriangles()(vtx, face))[0]);
            int v = int(height * asset.Texcoords().col(asset.TexTriangles()(vtx, face))[1]);
            int tIndex = lod0TriIndex(u, v);

            if (tIndex == 0)
            {
                outputDeltas.col(asset.Triangles()(vtx, face)) = Eigen::Vector3<T> (0.0f, 0.0f, 0.0f);
                                 continue;
            }

            Eigen::Matrix<T, 3, -1> vertices = Eigen::Matrix<T, 3, -1>(3, 3);
            vertices.col(0) = lod0Asset.Vertices().col(lod0Asset.Triangles()(2, tIndex));
                                                       vertices.col(1) = lod0Asset.Vertices().col(lod0Asset.Triangles()(1, tIndex));
                                                                                                  vertices.col(2) = lod0Asset.Vertices().col(
                lod0Asset.Triangles()(0, tIndex));
                Eigen::Vector3<T>
                barycentricCoords = Eigen::Vector3<T>(lod0BcX(u, v), lod0BcY(u, v), lod0BcZ(u, v));

                BarycentricCoordinates<T> bcOut(Eigen::Vector3i(0, 1, 2), barycentricCoords);

            Eigen::Vector3<T> newVertexPosition = bcOut.template Evaluate<3>(vertices);

            outputDeltas.col(asset.Triangles()(vtx,
                                               face)) = newVertexPosition - asset.Vertices().col(asset.Triangles()(vtx, face));
        }
    }

    return outputDeltas;
}

template Eigen::Matrix<float, 3, -1> UpdateLowerLODVerticesRasterizer(const Mesh<float>& lod0Asset, const Mesh<float>& asset);
// template Eigen::Matrix<double, 3, -1> UpdateLowerLODVerticesRasterizer(const Mesh<double>& lod0Asset, const Mesh<double>& asset);


template <class T>
Eigen::Matrix<T, 3, -1> UpdateVerticesWithDeformationGrid(GridDeformation<T>& gridDeformation,
                                                          const Eigen::Matrix<T, 3, -1>& vertices,
                                                          const Eigen::Vector3<T>& offset = Eigen::Vector3<T>::Zero(),
                                                          const T scale = T(1.0))
{
    const int vertexCount = static_cast<int>(vertices.cols());
    Eigen::Matrix<T, 3, -1> output = Eigen::Matrix<T, 3, -1>(3, vertexCount);

    for (int i = 0; i < vertexCount; ++i)
    {
        output.col(i) = scale * gridDeformation.EvaluateGridPosition(Eigen::Vector3<T>(vertices.col(i))) + offset;
    }

    return output;
}

template Eigen::Matrix<float, 3, -1> UpdateVerticesWithDeformationGrid(GridDeformation<float>& gridDeformation,
                                                                       const Eigen::Matrix<float, 3, -1>& vertices,
                                                                       const Eigen::Vector3<float>& offset,
                                                                       const float scale);
// template Eigen::Matrix<double, 3, -1> UpdateVerticesWithDeformationGrid(GridDeformation<double>& gridDeformation,
// const Eigen::Matrix<double, 3, -1>& vertices,
// const Eigen::Vector3<double>& offset,
// const double scale);


template <class T>
void ScaleJoints(const T scale, Eigen::Vector3<T> pivot, dna::Writer* dna, const std::shared_ptr<RigGeometry<T>>& rigGeometry)
{
    // replace the joints
    const JointRig2<T>& jointRig = rigGeometry->GetJointRig();
    std::vector<Affine<T, 3, 3>> jointWorldTransforms;

    const std::uint16_t numJoints = std::uint16_t(rigGeometry->GetJointRig().NumJoints());
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        // scale * (src - scalingPivot) + scalingPivot
        Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));
        jointWorldTransform.SetTranslation(scale * (jointWorldTransform.Translation() - pivot) + pivot);
        jointWorldTransforms.push_back(jointWorldTransform);
    }

    Eigen::Matrix<T, 3, -1> jointTranslations(3, numJoints);
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> localTransform;
        const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
        if (parentJointIndex >= 0)
        {
            auto parentTransform = jointWorldTransforms[parentJointIndex];
            localTransform = parentTransform.Inverse() * jointWorldTransforms[jointIndex];
        }
        else
        {
            localTransform = jointWorldTransforms[jointIndex];
        }
        jointTranslations.col(jointIndex) = localTransform.Translation().template cast<T>();
    }

    // Update joint translations
    dna->setNeutralJointTranslations((dna::Vector3*)jointTranslations.data(), numJoints);
}

template void ScaleJoints(const float scale, Eigen::Vector3<float> pivot, dna::Writer* dna, const std::shared_ptr<RigGeometry<float>>& rigGeometry);
// template void ScaleJoints(const double scale,
// Eigen::Vector3<double> pivot,
// dna::Writer* dna,
// const std::shared_ptr<RigGeometry<double>>& rigGeometry);


template <class T>
void TransformJoints(const std::vector<Affine<T, 3, 3>>& transforms, dna::Writer* dna, const std::shared_ptr<RigGeometry<T>>& rigGeometry)
{
    // replace the joints
    const JointRig2<T>& jointRig = rigGeometry->GetJointRig();
    std::vector<Affine<T, 3, 3>> jointWorldTransforms;

    const std::uint16_t numJoints = std::uint16_t(rigGeometry->GetJointRig().NumJoints());
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));
        jointWorldTransform.SetTranslation(transforms[jointIndex].Linear() * jointWorldTransform.Translation() + transforms[jointIndex].Translation());
        jointWorldTransforms.push_back(jointWorldTransform);
    }

    Eigen::Matrix<T, 3, -1> jointTranslations(3, numJoints);
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> localTransform;
        const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
        if (parentJointIndex >= 0)
        {
            auto parentTransform = jointWorldTransforms[parentJointIndex];
            localTransform = parentTransform.Inverse() * jointWorldTransforms[jointIndex];
        }
        else
        {
            localTransform = jointWorldTransforms[jointIndex];
        }

        jointTranslations.col(jointIndex) = localTransform.Translation().template cast<T>();
    }

    // Update joint translations
    dna->setNeutralJointTranslations((dna::Vector3*)jointTranslations.data(), numJoints);
}

template void TransformJoints(const std::vector<Affine<float, 3, 3>>& transforms, dna::Writer* dna, const std::shared_ptr<RigGeometry<float>>& rigGeometry);
// template void TransformJoints(const std::vector<Affine<double, 3, 3> >& transforms,
// dna::Writer* dna,
// const std::shared_ptr<RigGeometry<double> >& rigGeometry);


template <class T>
void TransformDnaBindPose(const Affine<T, 3, 3>& transform, dna::Writer* dna, const std::shared_ptr<RigGeometry<T>>& rigGeometry, int numLODs)
{
    const std::uint16_t numJoints = std::uint16_t(rigGeometry->GetJointRig().NumJoints());

    std::vector<Affine<T, 3, 3>> jointTransforms(numJoints);

    std::fill(jointTransforms.begin(), jointTransforms.end(), transform);
    TransformJoints<T>(jointTransforms, dna, rigGeometry);

    for (int lod = 0; lod < numLODs; ++lod)
    {
        std::vector<int> meshIds = rigGeometry->GetMeshIndicesForLOD(lod);

        for (int i = 0; i < int(meshIds.size()); ++i)
        {
            int meshId = meshIds[i];
            Mesh<T> asset = rigGeometry->GetMesh(meshId);
            Eigen::Matrix<T, 3, -1> vertices;
            vertices = transform.Transform(asset.Vertices());
            SetVertexPositionsToAsset(meshId, vertices, dna);
        }
    }
}

template void TransformDnaBindPose(const Affine<float, 3, 3>& transform, dna::Writer* dna, const std::shared_ptr<RigGeometry<float>>& rigGeometry, int numLODs);
// template void TransformDnaBindPose(const Affine<double, 3, 3>& transform,
// dna::Writer* dna,
// const std::shared_ptr<RigGeometry<double> >& rigGeometry,
// int numLODs);


template <class T>
void ScaleDnaJointBehavior(const T scale, dna::Writer* outDna, dna::Reader* templateDna)
{
    constexpr std::uint16_t jointAttributeCount = 9u;
    constexpr std::uint16_t rotationOffset = 3u;

    for (std::uint16_t jointGroupIndex = 0u; jointGroupIndex < templateDna->getJointGroupCount(); ++jointGroupIndex)
    {
        const auto values = templateDna->getJointGroupValues(jointGroupIndex);
        const auto outputIndices = templateDna->getJointGroupOutputIndices(jointGroupIndex);
        const auto inputIndices = templateDna->getJointGroupInputIndices(jointGroupIndex);
        const auto columnCount = inputIndices.size();
        std::vector<T> newValues { values.begin(), values.end() };
        for (std::size_t row = 0ul; row < outputIndices.size(); ++row)
        {
            // Only the translation attributes need to be scaled
            const auto relAttributeIndex = (outputIndices[row] % jointAttributeCount);
            if (relAttributeIndex < rotationOffset)
            {
                for (std::size_t column = 0ul; column < columnCount; ++column)
                {
                    newValues[row * columnCount + column] *= scale;
                }
            }
        }
        outDna->setJointGroupValues(jointGroupIndex, (const T*)newValues.data(), (uint32_t)newValues.size());
    }
}

template void ScaleDnaJointBehavior(const float scale, dna::Writer* outDna, dna::Reader* templateDna);
// template void ScaleDnaJointBehavior(const double scale, dna::Writer* outDna, dna::Reader* templateDna);


template <class T>
void ScaleDnaBindPose(const T scale, const Eigen::Vector3<T>& pivot, dna::Writer* dna, const std::shared_ptr<RigGeometry<T>>& rigGeometry, int numLODs)
{
    Affine<T, 3, 3> scaleMatrix;
    scaleMatrix.SetLinear(scaleMatrix.Linear() * scale);
    Affine<T, 3, 3> negativePivot = Affine<T, 3, 3>::FromTranslation(-pivot);
    Affine<T, 3, 3> positivePivot = Affine<T, 3, 3>::FromTranslation(pivot);

    const Affine<T, 3, 3> transform = positivePivot * scaleMatrix * negativePivot;
    ScaleJoints(scale, pivot, dna, rigGeometry);

    for (int lod = 0; lod < numLODs; ++lod)
    {
        std::vector<int> meshIds = rigGeometry->GetMeshIndicesForLOD(lod);

        for (int i = 0; i < int(meshIds.size()); ++i)
        {
            int meshId = meshIds[i];
            Mesh<T> asset = rigGeometry->GetMesh(meshId);
            Eigen::Matrix<T, 3, -1> vertices;
            vertices = transform.Transform(asset.Vertices());
            SetVertexPositionsToAsset(meshId, vertices, dna);
        }
    }
}

template void ScaleDnaBindPose(const float scale,
                               const Eigen::Vector3<float>& pivot,
                               dna::Writer* dna,
                               const std::shared_ptr<RigGeometry<float>>& rigGeometry,
                               int numLODs);
// template void ScaleDnaBindPose(const double scale,
// const Eigen::Vector3<double>& pivot,
// dna::Writer* dna,
// const std::shared_ptr<RigGeometry<double> >& rigGeometry,
// int numLODs);

template <class T>
struct DeltaTransferCalcData
{
    Eigen::Matrix<T, 3, -1> resultDelta;
    std::vector<int> triangleIndices;
    std::vector<std::vector<T>> barycentric;
};

template <class T>
DeltaTransferCalcData<T> CalculateDeltasUsingBaseMesh(const Mesh<T>& baseMesh, const Mesh<T>& assetMesh, const Eigen::Matrix<T, 3, -1>& baseDeltas)
{
    if (baseMesh.NumTriangles() == 0)
    {
        CARBON_CRITICAL("Delta transfer failed. Base is no triangle mesh.");
    }
    if (assetMesh.NumTriangles() == 0)
    {
        CARBON_CRITICAL("Delta transfer failed. Target is no triangle mesh.");
    }

    Eigen::Matrix<T, 3, -1> outputDeltas = Eigen::Matrix<T, 3, -1>::Zero(3, assetMesh.Vertices().cols());
    TITAN_NAMESPACE::AABBTree<T> aabbTree(baseMesh.Vertices().transpose(),
                                       baseMesh.Triangles().transpose());

    std::vector<int> triangleIndices;
    std::vector<std::vector<T>> barycentric;

    // Find intersection for each asset vertex
    for (int vtx = 0; vtx < assetMesh.NumVertices(); vtx++)
    {
        const Eigen::Vector3<T> vtxPos = assetMesh.Vertices().col(vtx);

        const auto [triangleIndex, closestBarycentric,
                    squaredDistance] = aabbTree.getClosestPoint(vtxPos.transpose(), T(1e9));

        triangleIndices.push_back((int)triangleIndex);
        auto baryT = Eigen::Vector3<T>(closestBarycentric.transpose());
        barycentric.push_back(std::vector<T> { baryT[0], baryT[1], baryT[2] });

        Eigen::Matrix<T, 3, -1> vertices = Eigen::Matrix<T, 3, -1>(3, 3);
        BarycentricCoordinates<T> bcOut(Eigen::Vector3i(0, 1, 2), closestBarycentric.transpose());

        vertices.col(0) = baseDeltas.col(baseMesh.Triangles()(0, triangleIndex));
                                         vertices.col(1) = baseDeltas.col(baseMesh.Triangles()(1, triangleIndex));
                                                                          vertices.col(2) = baseDeltas.col(baseMesh.Triangles()(2, triangleIndex));

                                                                                                           outputDeltas.col(vtx) = bcOut.template Evaluate<3>(
            vertices);
    }

    DeltaTransferCalcData<T> output;
    output.resultDelta = outputDeltas;
    output.barycentric = barycentric;
    output.triangleIndices = triangleIndices;

    return output;
}

template DeltaTransferCalcData<float> CalculateDeltasUsingBaseMesh(const Mesh<float>& baseMesh,
                                                                   const Mesh<float>& assetMesh,
                                                                   const Eigen::Matrix<float, 3, -1>& baseDeltas);
// template Eigen::Matrix<double, 3, -1> CalculateDeltasUsingBaseMesh(const Mesh<double>& baseMesh,
// const Mesh<double>& assetMesh,
// const Eigen::Matrix<double, 3, -1>& baseDeltas);

template <class T>
T CalcMedian(const std::vector<T>& input)
{
    auto inputLocal = input;

    std::sort(inputLocal.begin(), inputLocal.end(), std::less<T>());

    int mid = (int)((float)input.size() / 2.0f);

    return inputLocal[mid];
}

template <class T>
Eigen::Vector3<T> JointTranslationDeltaFromTargetShape(const Eigen::Matrix<T, 3, -1>& target,
                                                       const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                                       const int jointIndex,
                                                       const int meshIndex,
                                                       bool doFitSphere = true)
{
    auto currentJointPosition = Affine<T, 3, 3>(rigGeometry->GetBindMatrix(jointIndex)).Translation();
    Mesh<T> mesh = rigGeometry->GetMesh(meshIndex);
    mesh.Triangulate();
    TITAN_NAMESPACE::AABBTree<T> aabbTree(target.transpose(), mesh.Triangles().transpose());

    int numSamples = 1500;

    std::vector<Eigen::Vector3<T>> sampledPoints, sampledPointsFiltered;
    std::vector<T> distances;

    for (int i = 0; i < numSamples; ++i)
    {
        Eigen::Vector3<T> direction = Eigen::Vector3<T>::Random();
        const auto [triangleIndex, barycentric, distance] = aabbTree.intersectRay(currentJointPosition.transpose(), direction.transpose());
        if (triangleIndex == -1)
        {
            continue;
        }

        Eigen::Matrix<T, 3, -1> vertices = Eigen::Matrix<T, 3, -1>(3, 3);
        const BarycentricCoordinates<T> bcOut(Eigen::Vector3i(0, 1, 2),
                                                         barycentric.transpose());

        vertices.col(0) = target.col(mesh.Triangles()(0, triangleIndex));
                                     vertices.col(1) = target.col(mesh.Triangles()(1, triangleIndex));
                                                                  vertices.col(2) = target.col(mesh.Triangles()(2, triangleIndex));

        const Eigen::Vector3<T> newVertexPosition = bcOut.template Evaluate<3>(vertices);
        sampledPoints.push_back(newVertexPosition);
        distances.push_back(distance);
    }

    T median = CalcMedian<T>(distances);
    for (int i = 0; i < (int)sampledPoints.size(); ++i)
    {
        if (distances[i] < T(1.4) * median)
        {
            sampledPointsFiltered.push_back(sampledPoints[i]);
        }
    }


    auto fitSphere = [&](const std::vector<Eigen::Vector3<T>>& samples)
        {
            int numPoints = (int)samples.size();

            Eigen::MatrixX<T> A(numPoints, 4);
            Eigen::VectorX<T> b(numPoints);

            for (int i = 0; i < numPoints; ++i)
            {
                T x = samples[i](0);
                T y = samples[i](1);
                T z = samples[i](2);

                A(i, 0) = x;
                A(i, 1) = y;
                A(i, 2) = z;
                A(i, 3) = 1.0;

                b(i) = x * x + y * y + z * z;
            }

            Eigen::Vector4<T> x = A.colPivHouseholderQr().solve(b);

            T a = x(0) / (T)2.0;
            T b_center = x(1) / (T)2.0;
            T c = x(2) / (T)2.0;
            T radius = std::sqrt(a * a + b_center * b_center + c * c + x(3));

            return Eigen::Vector4<T>(a, b_center, c, radius);
        };

    Eigen::Vector3<T> output;

    if (doFitSphere)
    {
        Eigen::Vector4<T> sphereParams = fitSphere(sampledPointsFiltered);
        Eigen::Vector3<T> sphereCenter = sphereParams.head(3);

        output = sphereCenter - currentJointPosition;
    }
    else
    {
        Eigen::Matrix<float, 3, -1> points = Eigen::Matrix<float, 3, -1>::Zero(3, sampledPointsFiltered.size());
        for (int i = 0; i < (int)sampledPointsFiltered.size(); ++i)
        {
            points.col(i) = sampledPointsFiltered[i];
        }

        output = points.rowwise().mean() - currentJointPosition;
    }

    return output;
}

template Eigen::Vector3<float> JointTranslationDeltaFromTargetShape(const Eigen::Matrix<float, 3, -1>& target,
                                                                    const std::shared_ptr<RigGeometry<float>>& rigGeometry,
                                                                    const int jointIndex,
                                                                    const int meshIndex,
                                                                    bool fitSphere);

template <class T>
Eigen::Vector3<T> JointTranslationDeltaFromMeshes(const Eigen::Matrix<T, 3, -1>& source,
                                                  const Eigen::Matrix<T, 3, -1>& target,
                                                  const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                                  int jointIndex)
{
    //auto tgt = target.rowwise().mean();
    //auto src = source.rowwise().mean();
    //Eigen::Vector3<T> output = tgt - src;

    const auto [scale, transform] = Procrustes<T, 3>::AlignRigidAndScale(source, target);
    Eigen::Vector3<T> output = transform.Transform(Affine<T, 3, 3>(scale * rigGeometry->GetBindMatrix(jointIndex)).Translation()) -
        Affine<T, 3, 3>(rigGeometry->GetBindMatrix(jointIndex)).Translation();

    return output;
}

template Eigen::Vector3<float> JointTranslationDeltaFromMeshes(const Eigen::Matrix<float, 3, -1>& source,
                                                               const Eigen::Matrix<float, 3, -1>& target,
                                                               const std::shared_ptr<RigGeometry<float>>& rigGeometry,
                                                               int jointIndex);
// template Eigen::Vector3<double> JointTranslationDeltaFromMeshes(const Eigen::Matrix<double, 3, -1>& source,
// const Eigen::Matrix<double, 3, -1>& target,
// const std::shared_ptr<RigGeometry<double>> & rigGeometry,
// const int jointIndex);

template <class T>
Eigen::Matrix<T, 3, -1> MeshMorphModule<T>::Morph(const Eigen::Matrix<T, 3, -1>& sourceMeshVerticesStart,
                                                  const Eigen::Matrix<T, 3, -1>& sourceMeshVerticesEnd,
                                                  const Eigen::Matrix<T, 3, -1>& targetMeshVerticesStart,
                                                  const VertexWeights<T>& targetVerticesMask,
                                                  int gridSize)
{
    const int gridPtsX = gridSize;
    const int gridPtsY = gridSize;
    const int gridPtsZ = gridSize;

    GridDeformation<T> gridDeformation(gridPtsX, gridPtsY, gridPtsZ);
    gridDeformation.Init(sourceMeshVerticesStart);
    gridDeformation.Solve(sourceMeshVerticesStart, sourceMeshVerticesEnd, 10.0);

    const int vertexCount = static_cast<int>(targetMeshVerticesStart.cols());
    Eigen::Matrix<T, 3, -1> outputTargetVertices = targetMeshVerticesStart;

    for (int i = 0; i < vertexCount; ++i)
    {
        outputTargetVertices.col(i) += gridDeformation.EvaluateGridPosition(Eigen::Vector3<T>(targetMeshVerticesStart.col(i)));
    }

    Eigen::Matrix<T, 3, -1> deltas = ApplyMask<T>(outputTargetVertices - targetMeshVerticesStart, targetVerticesMask);
    return Eigen::Matrix<T, 3, -1>(targetMeshVerticesStart + deltas);
}

template class MeshMorphModule<float>;
// TODO: enable double by adding specific conversion before storing to DNA
// template class MeshMorphModule<double>;


template <class T>
std::map<std::string, std::tuple<std::string, std::vector<int>, std::vector<std::vector<T>>>> RigMorphModule<T>::CollectDeltaTransferCorrespondences(
    dna::Reader* reader,
    const std::map<std::string, std::vector<std::string>>& deltaTransferMeshNames)
{
    std::map<std::string, std::tuple<std::string, std::vector<int>, std::vector<std::vector<T>>>> output;
    std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>();
    rigGeometry->Init(reader);
    int lodCount = (int)reader->getLODCount();

    for (int lod = 0; lod < lodCount; ++lod)
    {
        std::vector<int> meshIds = rigGeometry->GetMeshIndicesForLOD(lod);
        for (int i = 0; i < int(meshIds.size()); ++i)
        {
            int meshId = meshIds[i];
            const std::string meshName = rigGeometry->GetMeshName(meshId);
            Mesh<T> mesh = rigGeometry->GetMesh(meshId);
            mesh.Triangulate();
            const auto deltaTransferBaseMeshName = FindByValue(meshName, deltaTransferMeshNames);

            // check if the mesh is driving and use vertices directly
            if (!deltaTransferBaseMeshName.empty())
            {
                auto baseMesh = rigGeometry->GetMesh(rigGeometry->GetMeshIndex(deltaTransferBaseMeshName));
                baseMesh.Triangulate();
                const Eigen::Matrix<T, 3, -1> baseDelta = rigGeometry->GetMesh(deltaTransferBaseMeshName).Vertices() - baseMesh.Vertices();
                DeltaTransferCalcData<T> deltaTransfCalcData = CalculateDeltasUsingBaseMesh<T>(baseMesh, mesh, baseDelta);
                output[meshName] = std::tuple<std::string, std::vector<int>, std::vector<std::vector<T>>>(deltaTransferBaseMeshName,
                                                                                                          deltaTransfCalcData.triangleIndices,
                                                                                                          deltaTransfCalcData.barycentric);
            }
        }
    }
    return output;
}

template <class T>
void RigMorphModule<T>::ApplyRigidTransform(dna::Reader* reader, dna::Writer* writer, const Affine<T, 3, 3>& rigidTransform, bool inParallel)
{
    std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>(inParallel);
    rigGeometry->Init(reader);
    int lodCount = (int)reader->getLODCount();
    TransformDnaBindPose(rigidTransform, writer, rigGeometry, lodCount);
}

template <class T>
void RigMorphModule<T>::ApplyScale(dna::Reader* reader, dna::Writer* writer, const T scale, const Eigen::Vector3<T>& scalingPivot, bool inParallel)
{
    std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>(inParallel);
    rigGeometry->Init(reader);
    int lodCount = (int)reader->getLODCount();
    ScaleDnaBindPose(scale, scalingPivot, writer, rigGeometry, lodCount);
    ScaleDnaJointBehavior(scale, writer, reader);
}

template <class T>
void RigMorphModule<T>::UpdateTeeth(dna::Reader* reader,
                                    dna::Writer* writer,
                                    const Eigen::Matrix<T, 3, -1>& teethMeshVertices,
                                    const std::string& teethMeshName,
                                    const std::string& headMeshName,
                                    const std::vector<std::string>& drivenJointNames,
                                    const std::vector<std::string>& deltaTransferMeshNames,
                                    const std::vector<std::string>& rigidTransformMeshNames,
                                    const std::vector<std::string>& uvProjectionMeshNames,
                                    const VertexWeights<T>& mouthSocketVertices,
                                    int gridSize,
                                    bool inParallel)
{
    // perform input data check
    if (teethMeshName.empty())
    {
        CARBON_CRITICAL("Input data does not contain driving meshes.");
    }
    if (teethMeshName.find("teeth") == std::string::npos)
    {
        CARBON_CRITICAL("Input data does not contain mesh labeled as teeth.");
    }

    const std::uint16_t numJoints = std::uint16_t(reader->getJointCount());

    Affine<T, 3, 3> identity;
    identity.SetIdentity();

    std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>(inParallel);
    rigGeometry->Init(reader);

    std::map<std::string, int> jointNameToIndex;
    const JointRig2<T>& jointRig = rigGeometry->GetJointRig();
    for (const std::string& jointName : jointRig.GetJointNames())
    {
        jointNameToIndex[jointName] = jointRig.GetJointIndex(jointName);
    }

    const int teethMeshId = rigGeometry->GetMeshIndex(teethMeshName);
    const int headMeshId = rigGeometry->GetMeshIndex(headMeshName);

    Mesh<T> targetTeethMesh = rigGeometry->GetMesh(teethMeshId);
    targetTeethMesh.SetVertices(teethMeshVertices);
    targetTeethMesh.Triangulate();
    targetTeethMesh.CalculateVertexNormals();

    Mesh<T> sourceMesh = rigGeometry->GetMesh(teethMeshId);
    Mesh<T> headMesh = rigGeometry->GetMesh(headMeshId);

    const auto tgtPos = teethMeshVertices.rowwise().mean();
    const auto srcPos = sourceMesh.Vertices().rowwise().mean();
    const auto translation = tgtPos - srcPos;

    const Affine<T, 3, 3> rigid = Affine<T, 3, 3>::FromTranslation(translation);

    const int gridPtsX = gridSize;
    const int gridPtsY = gridSize;
    const int gridPtsZ = gridSize;

    GridDeformation<T> gridDeformation(gridPtsX, gridPtsY, gridPtsZ);
    gridDeformation.Init(sourceMesh.Vertices());
    gridDeformation.Solve(sourceMesh.Vertices(), targetTeethMesh.Vertices(), 10.0);

    const int vertexCount = static_cast<int>(headMesh.NumVertices());
    Eigen::Matrix<T, 3, -1> outputHeadVertices = headMesh.Vertices();

    for (int i = 0; i < vertexCount; ++i)
    {
        outputHeadVertices.col(i) += gridDeformation.EvaluateGridPosition(Eigen::Vector3<T>(headMesh.Vertices().col(i)));
    }

    Eigen::Matrix<T, 3, -1> headMeshDeltas = ApplyMask<T>(outputHeadVertices - headMesh.Vertices(), mouthSocketVertices);
    headMesh.SetVertices(headMesh.Vertices() + headMeshDeltas);

    std::vector<Affine<T, 3, 3>> jointWorldTransforms;
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));
        jointWorldTransforms.push_back(jointWorldTransform);
    }

    for (const auto& jointName : drivenJointNames)
    {
        if (jointNameToIndex.find(jointName) == jointNameToIndex.end())
        {
            CARBON_CRITICAL("{} does not exist in the input DNA file.", jointName);
        }
        const int jointIndex = jointNameToIndex[jointName];
        Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));
        Eigen::Vector3<T> jointTranslation = JointTranslationDeltaFromMeshes(rigGeometry->GetMesh(teethMeshId).Vertices(),
                                                                             teethMeshVertices,
                                                                             rigGeometry,
                                                                             jointIndex);
        jointWorldTransform.SetTranslation(jointTranslation + jointWorldTransform.Translation());
        jointWorldTransforms[jointIndex] = jointWorldTransform;
    }

    // local transformations
    Eigen::Matrix<T, 3, -1> jointTranslations(3, numJoints);
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> localTransform;
        const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
        if (parentJointIndex >= 0)
        {
            auto parentTransform = jointWorldTransforms[parentJointIndex];
            localTransform = parentTransform.Inverse() * jointWorldTransforms[jointIndex];
        }
        else
        {
            localTransform = jointWorldTransforms[jointIndex];
        }
        jointTranslations.col(jointIndex) = localTransform.Translation().template cast<T>();
    }

    // Update joint translations
    writer->setNeutralJointTranslations((dna::Vector3*)jointTranslations.data(), numJoints);

    std::map<int, Mesh<T>> updatedMeshes;
    const int lodCount = (int)reader->getLODCount();
    for (int lod = 0; lod < lodCount; ++lod)
    {
        std::vector<int> meshIds = rigGeometry->GetMeshIndicesForLOD(lod);

        for (int i = 0; i < int(meshIds.size()); ++i)
        {
            int meshId = meshIds[i];
            std::string meshName = rigGeometry->GetMeshName(meshId);
            Mesh<T> asset = rigGeometry->GetMesh(meshId);
            asset.Triangulate();
            Eigen::Matrix<T, 3, -1> vertices;

            if (teethMeshName == meshName)
            {
                vertices = targetTeethMesh.Vertices();
            }
            else if (headMeshName == meshName)
            {
                vertices = headMesh.Vertices();
            }
            else if (std::count(deltaTransferMeshNames.begin(), deltaTransferMeshNames.end(), meshName))
            {
                const auto& baseMesh = rigGeometry->GetMesh(rigGeometry->GetMeshIndex(teethMeshName));
                const auto baseDelta = updatedMeshes[rigGeometry->GetMeshIndex(teethMeshName)].Vertices() - baseMesh.Vertices();

                auto deltaTransferData = CalculateDeltasUsingBaseMesh<T>(baseMesh, asset, baseDelta);

                vertices = asset.Vertices() + deltaTransferData.resultDelta;
            }
            else if (std::count(rigidTransformMeshNames.begin(), rigidTransformMeshNames.end(), meshName))
            {
                vertices = rigid.Transform(asset.Vertices());
            }
            else if (std::count(uvProjectionMeshNames.begin(), uvProjectionMeshNames.end(), meshName))
            {
                vertices = asset.Vertices() + UpdateLowerLODVerticesRaycasting<T>(targetTeethMesh, asset);
            }

            if (vertices.cols() > 0)
            {
                asset.SetVertices(vertices);
                asset.CalculateVertexNormals();
                updatedMeshes[meshId] = asset;
            }
        }
    }
    ApplyMeshesToDna(updatedMeshes,
                     writer);
}

template <class T>
bool RigMorphModule<T>::Morph(dna::Reader* reader,
                              dna::Writer* writer,
                              const std::map<std::string, Eigen::Matrix<T, 3, -1>>& targetVertices,
                              const std::vector<std::string>& drivingMeshNames,
                              const std::vector<std::string>& inactiveJointNames,
                              const std::map<std::string,
                                             std::vector<std::string>>& drivenJointNames,
                              const std::map<std::string, std::vector<std::string>>& dependentJointNames,
                              const std::vector<std::string>& jointsToOptimize,
                              const std::map<std::string, std::vector<std::string>>& deltaTransferMeshNames,
                              const std::map<std::string, std::vector<std::string>>& rigidTransformMeshNames,
                              const std::map<std::string, std::vector<std::string>>& uvProjectionMeshNames,
                              const VertexWeights<T>& mainMeshGridDeformMask,
                              int gridSize,
                              bool inParallel)
{
    // perform input data check
    if (drivingMeshNames.empty())
    {
        LOG_ERROR("Input data does not contain driving meshes.");
        return false;
    }

    if (drivingMeshNames[0].find("head") == std::string::npos)
    {
        LOG_ERROR("Input data does not contain mesh labeled as head.");
        return false;
    }

    std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>(inParallel);
    rigGeometry->Init(reader);

    const std::string mainMeshName = drivingMeshNames[0];
    const int mainMeshIndex = rigGeometry->GetMeshIndex(mainMeshName);

    auto it = targetVertices.find(mainMeshName);
    if (it == targetVertices.end())
    {
        LOG_ERROR("Target vertices input does not contain head mesh key.");
        return false;
    }

    // main mesh from the rig
    const Mesh<T>& currentMainMesh = rigGeometry->GetMesh(mainMeshIndex);

    // init target main mesh
    Mesh<T> targetMainMesh = currentMainMesh;
    targetMainMesh.SetVertices(it->second);

    // apply input mask
    const Eigen::Matrix<T, 3, -1> mainMeshDeltas = ApplyMask<T>(targetMainMesh.Vertices() - currentMainMesh.Vertices(), mainMeshGridDeformMask);

    // apply calculated vertices to target mesh
    targetMainMesh.SetVertices(currentMainMesh.Vertices() + mainMeshDeltas);
    targetMainMesh.Triangulate();
    targetMainMesh.CalculateVertexNormals();

    std::map<std::string, Mesh<T>> drivingMeshes;

    const int gridPtsX = gridSize;
    const int gridPtsY = gridSize;
    const int gridPtsZ = gridSize;

    // use only vertices of interest (defined by input mask)
    const size_t numOfGridDefVertices = mainMeshGridDeformMask.NonzeroVertices().size();
    Eigen::Matrix<T, 3, -1> sourceGridDefVertices = Eigen::Matrix<T, 3, -1>(3, numOfGridDefVertices);
    Eigen::Matrix<T, 3, -1> targetGridDefVertices = Eigen::Matrix<T, 3, -1>(3, numOfGridDefVertices);

    for (size_t i = 0; i < numOfGridDefVertices; ++i)
    {
        sourceGridDefVertices.col(i) = currentMainMesh.Vertices().col(mainMeshGridDeformMask.NonzeroVertices()[i]);
        targetGridDefVertices.col(i) = targetMainMesh.Vertices().col(mainMeshGridDeformMask.NonzeroVertices()[i]);
    }

    GridDeformation<T> gridDeformation(gridPtsX, gridPtsY, gridPtsZ);
    gridDeformation.Init(sourceGridDefVertices);
    gridDeformation.Solve(sourceGridDefVertices, targetGridDefVertices, 10.0);

    std::map<std::string, int> jointNameToIndex;
    const JointRig2<T>& jointRig = rigGeometry->GetJointRig();
    for (const std::string& jointName : jointRig.GetJointNames())
    {
        jointNameToIndex[jointName] = jointRig.GetJointIndex(jointName);
    }

    std::map<std::string, Affine<T, 3, 3>> drivingMeshesDeltaTransform;
    for (size_t i = 0; i < drivingMeshNames.size(); ++i)
    {
        const auto meshName = drivingMeshNames[i];
        Mesh<T> currentMesh = rigGeometry->GetMesh(rigGeometry->GetMeshIndex(meshName));

        Eigen::Matrix<T, 3, -1> drivingMeshVertices;
        const auto driverMeshVerticesIt = targetVertices.find(meshName);
        // if target driving vertices exist, use them
        if (driverMeshVerticesIt != targetVertices.end())
        {
            if (meshName == mainMeshName)
            {
                drivingMeshVertices = targetMainMesh.Vertices();
            }
            else
            {
                drivingMeshVertices = driverMeshVerticesIt->second;
            }

            const auto tgtPos = drivingMeshVertices.rowwise().mean();
            const auto srcPos = currentMesh.Vertices().rowwise().mean();
            const auto translation = tgtPos - srcPos;
            drivingMeshesDeltaTransform[meshName] = Affine<T, 3, 3>::FromTranslation(translation);
        }
        // otherwise transform the mesh using grid deformation
        else
        {
            const auto centerOfGravity = currentMesh.Vertices().rowwise().mean();
            const Affine<T, 3, 3> rigToTargetTransform = Affine<T, 3, 3>::FromTranslation(gridDeformation.EvaluateGridPosition(centerOfGravity));
            drivingMeshesDeltaTransform[meshName] = rigToTargetTransform;
            drivingMeshVertices = rigToTargetTransform.Transform(currentMesh.Vertices());
        }
        currentMesh.SetVertices(drivingMeshVertices);
        currentMesh.Triangulate();
        currentMesh.CalculateVertexNormals();

        drivingMeshes[meshName] = currentMesh;
    }

    std::vector<Affine<T, 3, 3>> jointWorldTransforms;
    const std::uint16_t numJoints = std::uint16_t(reader->getJointCount());

    std::vector<int> inactiveJointIds;
    for (size_t i = 0; i < inactiveJointNames.size(); i++)
    {
        const auto idx = jointNameToIndex.find(inactiveJointNames[i])->second;
        inactiveJointIds.push_back(idx);
    }

    // modify joints using calculated grid space
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));

        const Eigen::Vector3<T> originalJointPosition = jointWorldTransform.Translation();
        if (std::find(inactiveJointIds.begin(), inactiveJointIds.end(), jointIndex) != inactiveJointIds.end())
        {
            jointWorldTransform.SetTranslation(originalJointPosition);
        }
        else
        {
            // get the joint translation to new deformed state
            const Eigen::Vector3<T> jointTranslation = gridDeformation.EvaluateGridPosition(originalJointPosition);
            jointWorldTransform.SetTranslation(jointTranslation + originalJointPosition);
        }

        jointWorldTransforms.push_back(jointWorldTransform);
    }

    // additionally modify joints if influenced by driving meshes
    for (const auto& [meshName, jointNames] : drivenJointNames)
    {
        const int meshIndex = rigGeometry->GetMeshIndex(meshName);
        if (meshIndex < 0)
        {
            LOG_ERROR("{} does not exist in the input DNA file.", meshName);
            return false;
        }

        const auto rigMeshVertices = rigGeometry->GetMesh(meshIndex).Vertices();
        const auto driverMeshVertices = drivingMeshes[meshName].Vertices();

        for (const auto& jointName : jointNames)
        {
            if (jointNameToIndex.find(jointName) == jointNameToIndex.end())
            {
                LOG_ERROR("{} does not exist in the input DNA file.", jointName);
                return false;
            }
            const int jointIndex = jointNameToIndex[jointName];
            Affine<T, 3, 3> jointWorldTransform(rigGeometry->GetBindMatrix(jointIndex));
            Eigen::Vector3<T> jointTranslation;
            if (std::find(jointsToOptimize.begin(), jointsToOptimize.end(), jointName) == jointsToOptimize.end())
            {
                jointTranslation = JointTranslationDeltaFromMeshes(rigMeshVertices,
                                                                   driverMeshVertices,
                                                                   rigGeometry,
                                                                   jointIndex);
            }
            else
            {
                jointTranslation = JointTranslationDeltaFromTargetShape(driverMeshVertices,
                                                                        rigGeometry,
                                                                        jointIndex,
                                                                        meshIndex);
            }
            jointWorldTransform.SetTranslation(jointTranslation + jointWorldTransform.Translation());
            jointWorldTransforms[jointIndex] = jointWorldTransform;
        }
    }

    for (const auto& [sourceJointName, targetJointNames] : dependentJointNames)
    {
        auto sourceJointIndex = jointNameToIndex.find(sourceJointName)->second;
        const auto currentSourceJointTranslation = jointWorldTransforms[sourceJointIndex].Translation();
        const auto originalSourceJointTranslation = Affine<T, 3, 3>(rigGeometry->GetBindMatrix(sourceJointIndex)).Translation();
        const auto sourceTranslationDelta = currentSourceJointTranslation - originalSourceJointTranslation;

        // just copy the transformation
        // TO DO: make this a part of the process above to make it more efficient
        for (auto& jointName : targetJointNames)
        {
            auto targetJointIndex = jointNameToIndex.find(jointName)->second;
            jointWorldTransforms[targetJointIndex].SetTranslation(Affine<T, 3, 3>(rigGeometry->GetBindMatrix(
                                                                                      targetJointIndex)).Translation() + sourceTranslationDelta);
        }
    }

    // local transformations
    Eigen::Matrix<T, 3, -1> jointTranslations(3, numJoints);
    Eigen::Matrix<T, 3, -1> jointRotations(3, numJoints);

    // local joint transformations to be stored in dna
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> localTransform;
        const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
        if (parentJointIndex >= 0)
        {
            auto parentTransform = jointWorldTransforms[parentJointIndex];
            localTransform = parentTransform.Inverse() * jointWorldTransforms[jointIndex];
        }
        else
        {
            localTransform = jointWorldTransforms[jointIndex];
        }

        jointTranslations.col(jointIndex) = localTransform.Translation().template cast<T>();
    }

    // Update joint translations
    writer->setNeutralJointTranslations((dna::Vector3*)jointTranslations.data(), numJoints);

    std::map<int, Mesh<T>> updatedMeshes;
    int lodCount = (int)reader->getLODCount();
    // modify all dna meshes according to defined rules
    for (int lod = 0; lod < lodCount; ++lod)
    {
        std::vector<int> meshIds = rigGeometry->GetMeshIndicesForLOD(lod);

        for (int i = 0; i < int(meshIds.size()); ++i)
        {
            int meshId = meshIds[i];
            const std::string meshName = rigGeometry->GetMeshName(meshId);
            Mesh<T> mesh = rigGeometry->GetMesh(meshId);
            mesh.Triangulate();
            Eigen::Matrix<T, 3, -1> vertices;

            const auto rigidTransformBaseMeshName = FindByValue(meshName, rigidTransformMeshNames);
            const auto uvProjectionBaseMeshName = FindByValue(meshName, uvProjectionMeshNames);
            const auto deltaTransferBaseMeshName = FindByValue(meshName, deltaTransferMeshNames);

            // check if the mesh is driving and use vertices directly
            if (drivingMeshes.find(meshName) != drivingMeshes.end())
            {
                vertices = drivingMeshes[meshName].Vertices();
            }
            // whether the delta transfer operation is defined over current mesh
            else if (!deltaTransferBaseMeshName.empty())
            {
                auto baseMesh = rigGeometry->GetMesh(rigGeometry->GetMeshIndex(deltaTransferBaseMeshName));
                baseMesh.Triangulate();
                const Eigen::Matrix<T, 3, -1> baseDelta = drivingMeshes[deltaTransferBaseMeshName].Vertices() - baseMesh.Vertices();

                DeltaTransferCalcData<T> deltaTransfCalcData = CalculateDeltasUsingBaseMesh<T>(baseMesh, mesh, baseDelta);
                vertices = mesh.Vertices() + deltaTransfCalcData.resultDelta;
            }
            // whether the delta projection operation is defined over current mesh
            else if (!uvProjectionBaseMeshName.empty())
            {
                vertices = mesh.Vertices() + UpdateLowerLODVerticesRaycasting<T>(drivingMeshes[uvProjectionBaseMeshName], mesh);
            }
            // whether the rigid transform operation is defined over current mesh
            else if (!rigidTransformBaseMeshName.empty())
            {
                vertices = drivingMeshesDeltaTransform[rigidTransformBaseMeshName].Transform(mesh.Vertices());
            }
            // if no specific operation defined, apply per vertex deformation grid results
            else
            {
                vertices = mesh.Vertices() + UpdateVerticesWithDeformationGrid<T>(gridDeformation, mesh.Vertices());
            }
            // add to the dna update meshes pool
            mesh.SetVertices(vertices);
            mesh.CalculateVertexNormals();
            updatedMeshes[meshId] = mesh;
        }
    }

    ApplyMeshesToDna(updatedMeshes, writer);

    return true;
}

// explicitly instantiate the RigMorphModule classes
template class RigMorphModule<float>;
// TO DO: enable double by adding specific conversion before storing to DNA
// template class RigMorphModule<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
