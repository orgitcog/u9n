// Copyright Epic Games, Inc. All Rights Reserved.

#include "rig/BodyGeometry.h"

#include <carbon/utils/TaskThreadPool.h>
#include <nls/functions/GatherFunction.h>
#include <nls/geometry/EulerAngles.h>
#include <nls/geometry/Jacobians.h>

#include <riglogic/RigLogic.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
BodyGeometry<T>::BodyGeometry(const std::shared_ptr<TaskThreadPool>& taskThreadPool)
    : taskThreadPool(taskThreadPool)
{}

template <class T>
BodyGeometry<T>::BodyGeometry(bool useMultithreading)
{
    if (useMultithreading) {
        taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
    }
}

template <class T>
BodyGeometry<T>::BodyGeometry(int numLods, bool useMultithreading)
    : BodyGeometry(useMultithreading)
{
    vertexInfluenceWeights.resize(numLods);
    mesh.resize(numLods);
    blendshapeControlsToMeshBlendshapeControls.resize(numLods);
    blendshapeMatrixDense.resize(numLods);
}

template <class T>
int BodyGeometry<T>::GetParentIndex(int jointIndex) const
{
    return GetJointParentIndices()[size_t(jointIndex)];
}


template <class T>
void BodyGeometry<T>::SetThreadPool(const std::shared_ptr<TaskThreadPool>& taskThreadPool_)
{
    taskThreadPool = taskThreadPool_;
}

template <class T>
std::shared_ptr<BodyGeometry<T>> BodyGeometry<T>::Clone() const
{
    std::shared_ptr<BodyGeometry> clone = std::make_shared<BodyGeometry>();
    *clone = *this;
    return clone;
}

template <class T>
void BodyGeometry<T>::SetNumLODs(const int l) {
    mesh.resize(l);
    blendshapeControlsToMeshBlendshapeControls.resize(l);
    blendshapeMatrixDense.resize(l);
    vertexInfluenceWeights.resize(l);
}

template <class T>
int BodyGeometry<T>::GetNumLODs() const
{
    return static_cast<int>(mesh.size());
}


template <class T>
Mesh<T> BodyGeometry<T>::ReadMesh(const dna::Reader* reader, int meshIndex)
{
    Mesh<T> mesh;

    Eigen::Matrix<T, 3, -1> vertices(3, reader->getVertexPositionCount(std::uint16_t(meshIndex)));
    Eigen::Matrix<T, 2, -1> texcoords(2, reader->getVertexTextureCoordinateCount(std::uint16_t(meshIndex)));

    for (std::uint32_t j = 0; j < reader->getVertexPositionCount(std::uint16_t(meshIndex)); j++) {
        vertices(0, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).x;
        vertices(1, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).y;
        vertices(2, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).z;
    }
    mesh.SetVertices(vertices);

    for (std::uint32_t j = 0; j < reader->getVertexTextureCoordinateCount(std::uint16_t(meshIndex)); j++) {
        texcoords(0, j) = reader->getVertexTextureCoordinate(std::uint16_t(meshIndex), j).u;
        // texture coordinates are stored with origin in bottom left corner, but images are stored with origin in top left corner
        // and hence we flip the coordinate here
        texcoords(1, j) = T(1) -  reader->getVertexTextureCoordinate(std::uint16_t(meshIndex), j).v;
    }
    mesh.SetTexcoords(texcoords);

    const int numFaces = reader->getFaceCount(std::uint16_t(meshIndex));
    int numQuads = 0;
    int numTris = 0;
    std::map<size_t, int> numOthers;
    for (int faceIndex = 0; faceIndex < numFaces; faceIndex++) {
        rl4::ConstArrayView<std::uint32_t> faceLayoutIndices = reader->getFaceVertexLayoutIndices(std::uint16_t(meshIndex), faceIndex);
        if (faceLayoutIndices.size() == 3) {
            numTris++;
        } else if (faceLayoutIndices.size() == 4) {
            numQuads++;
        } else {
            numOthers[faceLayoutIndices.size()]++;
        }
    }
    for (const auto& [vertexCount, numFacesWithThatCount] : numOthers) {
        LOG_WARNING("mesh {} contains {} faces with {} vertices, but we only support triangles and quads", reader->getMeshName(std::uint16_t(meshIndex)).c_str(), numFacesWithThatCount, vertexCount);
    }
    rl4::ConstArrayView<std::uint32_t> vertexLayoutPositions = reader->getVertexLayoutPositionIndices(std::uint16_t(meshIndex));
    rl4::ConstArrayView<std::uint32_t> texLayoutPositons = reader->getVertexLayoutTextureCoordinateIndices(std::uint16_t(meshIndex));
    Eigen::Matrix<int, 4, -1> quads(4, numQuads);
    Eigen::Matrix<int, 3, -1> tris(3, numTris);
    Eigen::Matrix<int, 4, -1> texQuads(4, numQuads);
    Eigen::Matrix<int, 3, -1> texTris(3, numTris);

    int quadsIter = 0;
    int trisIter = 0;
    for (int faceIndex = 0; faceIndex < numFaces; faceIndex++) {
        rl4::ConstArrayView<std::uint32_t> faceLayoutIndices = reader->getFaceVertexLayoutIndices(std::uint16_t(meshIndex), faceIndex);
        if (faceLayoutIndices.size() == 3) {
            tris(0, trisIter) = vertexLayoutPositions[faceLayoutIndices[0]];
            tris(1, trisIter) = vertexLayoutPositions[faceLayoutIndices[1]];
            tris(2, trisIter) = vertexLayoutPositions[faceLayoutIndices[2]];

            texTris(0, trisIter) = texLayoutPositons[faceLayoutIndices[0]];
            texTris(1, trisIter) = texLayoutPositons[faceLayoutIndices[1]];
            texTris(2, trisIter) = texLayoutPositons[faceLayoutIndices[2]];

            trisIter++;
        } else if (faceLayoutIndices.size() == 4) {
            quads(0, quadsIter) = vertexLayoutPositions[faceLayoutIndices[0]];
            quads(1, quadsIter) = vertexLayoutPositions[faceLayoutIndices[1]];
            quads(2, quadsIter) = vertexLayoutPositions[faceLayoutIndices[2]];
            quads(3, quadsIter) = vertexLayoutPositions[faceLayoutIndices[3]];

            texQuads(0, quadsIter) = texLayoutPositons[faceLayoutIndices[0]];
            texQuads(1, quadsIter) = texLayoutPositons[faceLayoutIndices[1]];
            texQuads(2, quadsIter) = texLayoutPositons[faceLayoutIndices[2]];
            texQuads(3, quadsIter) = texLayoutPositons[faceLayoutIndices[3]];
            quadsIter++;
        }
    }
    mesh.SetTriangles(tris);
    mesh.SetQuads(quads);
    mesh.SetTexQuads(texQuads);
    mesh.SetTexTriangles(texTris);

    mesh.Validate(true);

    return mesh;
}

template <class T>
int BodyGeometry<T>::GetJointIndex(const std::string& jointName) const {
    for (int i = 0; i < int(jointNames.size()); ++i) {
        if (jointName == jointNames[i]) {
            return i;
        }
    }
    return -1;
}

template <class T>
bool BodyGeometry<T>::Init(const dna::Reader* reader, bool computeMeshNormals)
{
    // read joints data
    const std::uint16_t numJoints = reader->getJointCount();
    jointNames.clear();
    jointParentIndices.clear();

    if (numJoints > 0) {
        for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++) {
            jointNames.push_back(reader->getJointName(jointIndex).c_str());
            const uint16_t parentIndex = reader->getJointParentIndex(jointIndex);
            if (jointIndex != parentIndex) {
                jointParentIndices.push_back(parentIndex);
            }
            else {
                jointParentIndices.push_back(-1);
            }
        }

        jointRestPose = Eigen::Matrix<T, 3, -1>(3, numJoints);
        jointRestOrientation.resize(numJoints);
        for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++) {
            rl4::Vector3 t = reader->getNeutralJointTranslation(jointIndex);
            constexpr float deg2rad = float(CARBON_PI / 180.0);
            rl4::Vector3 rot = reader->getNeutralJointRotation(jointIndex) * deg2rad;
            jointRestPose(0, jointIndex) = t.x;
            jointRestPose(1, jointIndex) = t.y;
            jointRestPose(2, jointIndex) = t.z;
            Eigen::Matrix<T, 3, 3> R = EulerXYZ<T>(rot.x, rot.y, rot.z);
            jointRestOrientation[jointIndex] = R;
        }
        UpdateBindPoses();
    }

    // read all mesh geometry
    const std::uint16_t numLODs = reader->getLODCount();
    const int numMeshes = reader->getMeshCount();

    if (numLODs != numMeshes) {
        CARBON_CRITICAL("Body rig expects only one mesh per LOD for now");
    }

    mesh.resize(numLODs);
    blendshapeMatrixDense.resize(numLODs);
    blendshapeControlsToMeshBlendshapeControls.resize(numLODs);
    vertexInfluenceWeights.resize(numLODs);

    for (std::uint16_t li = 0; li < numLODs; li++) {
        rl4::ConstArrayView<std::uint16_t> meshIndicesForLOD = reader->getMeshIndicesForLOD(li);

        const std::uint16_t mi = meshIndicesForLOD[0];
        mesh[mi] = ReadMesh(reader, mi);
        if (computeMeshNormals)
        {
            Mesh<T> triangulatedMesh = mesh[mi];
            triangulatedMesh.Triangulate();
            triangulatedMesh.CalculateVertexNormals(false, VertexNormalComputationType::AreaWeighted, /*stableNormalize=*/false);
            mesh[mi].SetVertexNormals(triangulatedMesh.VertexNormals());
        }

        // read blendshape data and put into sparse matrix
        const std::uint16_t numBlendshapeTargets = reader->getBlendShapeTargetCount(mi);
        blendshapeMatrixDense[mi] = Eigen::Matrix<T, -1, -1, Eigen::RowMajor>::Zero(3 * mesh[mi].NumVertices(), numBlendshapeTargets);
        blendshapeControlsToMeshBlendshapeControls[mi].resize(numBlendshapeTargets);
        for (std::uint16_t blendShapeTargetIndex = 0; blendShapeTargetIndex < numBlendshapeTargets; blendShapeTargetIndex++) {
            const std::uint16_t channelIndex = reader->getBlendShapeChannelIndex(mi, blendShapeTargetIndex);
            const int psdIndex = reader->getBlendShapeChannelInputIndices()[channelIndex];
            blendshapeControlsToMeshBlendshapeControls[mi][blendShapeTargetIndex] = static_cast<int>(psdIndex);
            const std::uint32_t numDeltas = reader->getBlendShapeTargetDeltaCount(mi, blendShapeTargetIndex);
            if (numDeltas == 0)
                continue;
            rl4::ConstArrayView<std::uint32_t> vertexIndices = reader->getBlendShapeTargetVertexIndices(mi, blendShapeTargetIndex);
            for (int deltaIndex = 0; deltaIndex < int(numDeltas); deltaIndex++) {
                const dna::Delta delta = reader->getBlendShapeTargetDelta(mi, blendShapeTargetIndex, deltaIndex);
                blendshapeMatrixDense[mi](3 * vertexIndices[deltaIndex] + 0, blendShapeTargetIndex) = delta.x;
                blendshapeMatrixDense[mi](3 * vertexIndices[deltaIndex] + 1, blendShapeTargetIndex) = delta.y;
                blendshapeMatrixDense[mi](3 * vertexIndices[deltaIndex] + 2, blendShapeTargetIndex) = delta.z;
            }
            if (blendshapeMatrixDense[mi].col(blendShapeTargetIndex).norm() == T(0)) {
                LOG_WARNING("blendshape {} ({}, psd {}) does not have any data, but {} deltas", reader->getBlendShapeChannelName(channelIndex).c_str(), channelIndex, psdIndex, numDeltas);
            }
        }

        // setup skinning weights
        std::vector<Eigen::Triplet<T>> influenceTriplets;
        for (int vertexIndex = 0; vertexIndex < mesh[mi].NumVertices(); vertexIndex++) {
            rl4::ConstArrayView<float> influenceWeights = reader->getSkinWeightsValues(mi, vertexIndex);
            rl4::ConstArrayView<std::uint16_t> jointIndices = reader->getSkinWeightsJointIndices(mi, vertexIndex);
            for (int k = 0; k < int(influenceWeights.size()); k++) {
                influenceTriplets.emplace_back(vertexIndex, jointIndices[k], influenceWeights[k]);
            }
        }
        vertexInfluenceWeights[mi].resize(mesh[mi].NumVertices(), NumJoints());
        vertexInfluenceWeights[mi].setFromTriplets(influenceTriplets.begin(), influenceTriplets.end());
    }

    return true;
}

template <class T>
void BodyGeometry<T>::UpdateBindPoses()
{
    // temporary state to calculate the bind poses
    State state;

    const int numJoints = NumJoints();

    jointBindPoses.resize(numJoints);
    jointInverseBindPoses.resize(numJoints);

    state.localMatrices.resize(numJoints);
    state.worldMatrices.resize(numJoints);

    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++) {
        state.localMatrices[jointIndex].linear() = jointRestOrientation[jointIndex];
        state.localMatrices[jointIndex].translation() = jointRestPose.col(jointIndex);
    }

    for (int jointIndex = 0; jointIndex < NumJoints(); jointIndex++) {
        const int& parentIndex = jointParentIndices[jointIndex];
        if (parentIndex >= 0) {
            state.worldMatrices[jointIndex] = state.worldMatrices[parentIndex] * state.localMatrices[jointIndex];
        } else {
            state.worldMatrices[jointIndex] = state.localMatrices[jointIndex];
        }
        jointBindPoses[jointIndex] = state.worldMatrices[jointIndex];
        jointInverseBindPoses[jointIndex] = jointBindPoses[jointIndex].inverse();
    }
}

// jacobian calculation for out = aff1 * aff2 where both aff1 and aff2 have a jacobian
template <class T>
void AffineJacobianMultiply(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> outJacobian,
                            const Eigen::Matrix<T, 4, 4>& aff1,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> jac1,
                            const Eigen::Matrix<T, 4, 4>& aff2,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> jac2)
{
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            // out(r, c) = aff1.row(r) * aff2.col(c)
            outJacobian.row(3 * c + r).noalias() = aff1.matrix()(r, 0) * jac2.row(3 * c + 0) + aff2.matrix()(0, c) * jac1.row(3 * 0 + r);
            for (int k = 1; k < 3; ++k) {
                outJacobian.row(3 * c + r).noalias() += aff1(r, k) * jac2.row(3 * c + k);
                outJacobian.row(3 * c + r).noalias() += aff2(k, c) * jac1.row(3 * k + r);
            }
        }
    }
    for (int r = 0; r < 3; ++r) {
        outJacobian.row(9 + r) = jac1.row(9 + r);
        for (int k = 0; k < 3; ++k) {
            outJacobian.row(9 + r).noalias() += aff1(r, k) * jac2.row(9 + k);
            outJacobian.row(9 + r).noalias() += aff2(k, 3) * jac1.row(3 * k + r);
        }
    }
}

// jacobian calculation for outs = aff1 * aff2 where only aff1 has a jacobian
template <class T>
void AffineJacobianMultiply(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> outJacobian,
                            const Eigen::Matrix<T, 4, 4>& /*aff1*/,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> jac1,
                            const Eigen::Matrix<T, 4, 4>& aff2)
{
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            // out(r, c) = aff1.row(r) * aff2.col(c)
            outJacobian.row(3 * c + r).noalias() = aff2.matrix()(0, c) * jac1.row(3 * 0 + r);
            for (int k = 1; k < 3; ++k) {
                outJacobian.row(3 * c + r).noalias() += aff2(k, c) * jac1.row(3 * k + r);
            }
        }
    }
    for (int r = 0; r < 3; ++r) {
        outJacobian.row(9 + r).noalias() = jac1.row(9 + r);
        for (int k = 0; k < 3; ++k) {
            outJacobian.row(9 + r).noalias() += aff2(k, 3) * jac1.row(3 * k + r);
        }
    }
}

template <class T>
void BodyGeometry<T>::EvaluateJointDeltas(const DiffData<T>& diffJoints, State& state) const
{
    Eigen::Ref<const Eigen::VectorX<T>> jointState = diffJoints.Value();

    state.withJacobians = true;

    const int numJoints = NumJoints();
    state.localMatrices.resize(numJoints);
    state.worldMatrices.resize(numJoints);
    state.skinningMatrices.resize(numJoints);

    int startCol = std::numeric_limits<int>::max();
    int endCol = 0;
    auto getColumnBounds = [&](const DiffData<T>& diffData) {
        if (diffData.HasJacobian()) {
            startCol = std::min<int>(startCol, diffData.Jacobian().StartCol());
            endCol = std::max<int>(endCol, diffData.Jacobian().Cols());
        }
    };
    getColumnBounds(diffJoints);

    state.jointDeltasJacobian.resize(diffJoints.Size(), endCol - startCol);
    state.jointDeltasJacobian.setZero();
    if (diffJoints.HasJacobian()) diffJoints.Jacobian().CopyToDenseMatrix(state.jointDeltasJacobian.block(0, diffJoints.Jacobian().StartCol() - startCol, diffJoints.Size(), diffJoints.Jacobian().Cols() - diffJoints.Jacobian().StartCol()));
    state.localMatricesJacobian.resize(numJoints * 12 + 12, state.jointDeltasJacobian.cols());
    state.worldMatricesJacobian.resize(numJoints * 12, state.jointDeltasJacobian.cols());
    state.skinningMatricesJacobian.resize(numJoints * 12, state.jointDeltasJacobian.cols());

    {
        auto updateLocalMatrices = [&](int start, int end) {

            constexpr int dofPerJoint = 9;

            Eigen::Matrix<T, 12, dofPerJoint, Eigen::RowMajor> dmat = Eigen::Matrix<T, 12, dofPerJoint, Eigen::RowMajor>::Zero(12, dofPerJoint);

            for (int jointIndex = start; jointIndex < end; ++jointIndex) {
                const T& drx = jointState[dofPerJoint * jointIndex + 3];
                const T& dry = jointState[dofPerJoint * jointIndex + 4];
                const T& drz = jointState[dofPerJoint * jointIndex + 5];

                const T& dsx = jointState[dofPerJoint * jointIndex + 6];
                const T& dsy = jointState[dofPerJoint * jointIndex + 7];
                const T& dsz = jointState[dofPerJoint * jointIndex + 8];

                state.localMatrices[jointIndex].linear().noalias() = jointRestOrientation[jointIndex] * EulerXYZAndScale(drx, dry, drz, T(1) + dsx, T(1) + dsy, T(1) + dsz);
                state.localMatrices[jointIndex].translation().noalias() = jointState.segment(dofPerJoint * jointIndex + 0, 3) + jointRestPose.col(jointIndex);

                if (diffJoints.HasJacobian()) {
                    // gather jacobian of drx, dry, drz, dsx, dsy, dsz, and combine with euler jacobian and scale jacobian
                    auto jacobianOfPreMultiply = JacobianOfPremultipliedMatrixDense<T, 3, 3, 3>(jointRestOrientation[jointIndex]);
                    auto eulerAndScaleJacobian = EulerXYZAndScaleJacobianDense<T>(drx, dry, drz, T(1) + dsx, T(1) + dsy, T(1) + dsz);
                    dmat.block(0, 3, 9, 6).noalias() = jacobianOfPreMultiply * eulerAndScaleJacobian;

                    // translation jacobian is just the copy of the respective rows of the joint jacobians
                    dmat(9, 0) = T(1);
                    dmat(10, 1) = T(1);
                    dmat(11, 2) = T(1);
                }

                state.localMatricesJacobian.block(jointIndex * 12, 0, 12, state.jointDeltasJacobian.cols()).noalias() = dmat * state.jointDeltasJacobian.block(jointIndex * dofPerJoint, 0, dofPerJoint, state.jointDeltasJacobian.cols());
            }
        };

        const int numTasks = int(jointRestPose.cols());
        if (diffJoints.HasJacobian() && state.localMatricesJacobian.size() > 1000 && taskThreadPool) {
            taskThreadPool->AddTaskRangeAndWait(numTasks, updateLocalMatrices);
        } else {
            updateLocalMatrices(0, numTasks);
        }

        state.localMatricesJacobian.block(numJoints * 12, 0, 12, state.localMatricesJacobian.cols()).setZero();
    }
    {
        // update world and skinning matrices
        const int numTasks = numJoints;
        auto updateWorldAndSkinningMatrices = [&](int start, int end) {
            for (int taskId = start; taskId < end; ++taskId) {
                const int jointIndex = taskId;
                const int parentIndex = jointParentIndices[jointIndex];
                if (parentIndex >= 0) {
                    state.worldMatrices[jointIndex] = state.worldMatrices[parentIndex] * state.localMatrices[jointIndex];
                    // dense jacobian multiply
                    AffineJacobianMultiply<T>(state.worldMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()),
                                                state.worldMatrices[parentIndex].matrix(),
                                                state.worldMatricesJacobian.block(parentIndex * 12, 0, 12, state.localMatricesJacobian.cols()),
                                                state.localMatrices[jointIndex].matrix(),
                                                state.localMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()));
                } else {
                    // root node
                    state.worldMatrices[jointIndex] = state.localMatrices[jointIndex];
                    // dense jacobian multiply
                    AffineJacobianMultiply<T>(state.worldMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()),
                                                Eigen::Matrix<T, 4, 4>::Identity(),
                                                state.localMatricesJacobian.block(numJoints * 12, 0, 12, state.localMatricesJacobian.cols()),
                                                state.localMatrices[jointIndex].matrix(),
                                                state.localMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()));
                }

            }
        };

        // Todo - check if using a thread pool is faster here
        updateWorldAndSkinningMatrices(0, numTasks);
    }

    {
        const int numTasks = static_cast<int>(state.worldMatrices.size());
        auto updateWorldAndSkinningMatrices = [&](int start, int end) {
            for (int taskId = start; taskId < end; ++taskId) {
                const int jointIndex = taskId;
                state.skinningMatrices[jointIndex] = state.worldMatrices[jointIndex] * jointInverseBindPoses[jointIndex];
                // dense jacobian multiply
                AffineJacobianMultiply<T>(state.skinningMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()),
                                          state.worldMatrices[jointIndex].matrix(),
                                          state.worldMatricesJacobian.block(jointIndex * 12, 0, 12, state.localMatricesJacobian.cols()),
                                          jointInverseBindPoses[jointIndex].matrix());
            }
        };
        if (diffJoints.HasJacobian() && state.localMatricesJacobian.size() > 1000 && taskThreadPool) {
            taskThreadPool->AddTaskRangeAndWait(numTasks, updateWorldAndSkinningMatrices);
        } else {
            updateWorldAndSkinningMatrices(0, numTasks);
        }
    }

    if (diffJoints.HasJacobian()) {
        state.jointJacobianColOffset = startCol;
    } else {
        state.jointJacobianColOffset = -1;
    }
}

template <class T>
void BodyGeometry<T>::EvaluateJointDeltasWithoutJacobians(const DiffData<T>& diffJoints, State& state) const
{
    Eigen::Ref<const Eigen::VectorX<T>> jointState = diffJoints.Value();

    const int numJoints = NumJoints();
    state.withJacobians = false;
    state.localMatrices.resize(numJoints);
    state.worldMatrices.resize(numJoints);
    state.skinningMatrices.resize(numJoints);
    state.jointJacobianColOffset = -1;

    // calculate local matrices
    const int dofPerJoint = 9;
    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex) {
        const T& drx = jointState[dofPerJoint * jointIndex + 3];
        const T& dry = jointState[dofPerJoint * jointIndex + 4];
        const T& drz = jointState[dofPerJoint * jointIndex + 5];

        Eigen::Matrix<T, 3, 3> R;
        const T& dsx = jointState[dofPerJoint * jointIndex + 6];
        const T& dsy = jointState[dofPerJoint * jointIndex + 7];
        const T& dsz = jointState[dofPerJoint * jointIndex + 8];

        R = jointRestOrientation[jointIndex] * EulerXYZAndScale(drx, dry, drz, T(1) + dsx, T(1) + dsy, T(1) + dsz);

        state.localMatrices[jointIndex].linear() = R;
        state.localMatrices[jointIndex].translation() = jointState.segment(dofPerJoint * jointIndex + 0, 3) + jointRestPose.col(jointIndex);
    }

    // update world matrices
    for (int jointIndex = 0; jointIndex < numJoints; jointIndex++) {
        const int parentIndex = jointParentIndices[jointIndex];
        if (parentIndex >= 0) {
            state.worldMatrices[jointIndex] = state.worldMatrices[parentIndex] * state.localMatrices[jointIndex];
        } else {
            // root node
            state.worldMatrices[jointIndex] = state.localMatrices[jointIndex];
        }
    }

    // update skinning matrices
    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex) {
        state.skinningMatrices[jointIndex] = state.worldMatrices[jointIndex] * jointInverseBindPoses[jointIndex];
    }
}

template <class T>
const std::pair<Eigen::Matrix<T, 3, -1>, std::vector<Eigen::Matrix<T, 3, 3>>> BodyGeometry<T>::EvaluateInverseJointDeltas(const DiffData<T>& diffJoints, const State& state) const {
    Eigen::Ref<const Eigen::VectorX<T>> jointState = diffJoints.Value();
    const int numJoints = NumJoints();

    if ((int)state.localMatrices.size() != numJoints) {
        CARBON_CRITICAL("state is not valid");
    }

    Eigen::Matrix<T, 3, -1> jointRestPoseResult(3, numJoints);
    std::vector<Eigen::Matrix<T, 3, 3>> jointRestOrientationResult(numJoints);

    // calculate local matrices
    const int dofPerJoint = 9;
    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex) {
        const T& drx = jointState[dofPerJoint * jointIndex + 3];
        const T& dry = jointState[dofPerJoint * jointIndex + 4];
        const T& drz = jointState[dofPerJoint * jointIndex + 5];

        const T& dsx = jointState[dofPerJoint * jointIndex + 6];
        const T& dsy = jointState[dofPerJoint * jointIndex + 7];
        const T& dsz = jointState[dofPerJoint * jointIndex + 8];

        jointRestOrientationResult[jointIndex] = state.localMatrices[jointIndex].linear() * EulerXYZAndScale(drx, dry, drz, T(1) + dsx, T(1) + dsy, T(1) + dsz).inverse();
        jointRestPoseResult.col(jointIndex) = state.localMatrices[jointIndex].translation() - jointState.segment(dofPerJoint * jointIndex + 0, 3);
    }

    return { jointRestPoseResult, jointRestOrientationResult };
}

template <class T>
void BodyGeometry<T>::EvaluateBlendshapes(const int lod, const DiffData<T>& diffPsd, State& state) const
{
    // copy neutral
    state.blendshapeVertices = mesh[lod].Vertices();
    state.blendshapeJacobianColOffset = -1;

    // no blendshapes, then return neutral
    if (blendshapeControlsToMeshBlendshapeControls[lod].size() == 0) {
        return;
    }

    auto blendshapeVerticesFlattened = state.blendshapeVertices.reshaped();

    // get the blendshape activations for this mesh
    state.diffMeshBlendshapes = GatherFunction<T>::Gather(diffPsd, blendshapeControlsToMeshBlendshapeControls[lod]);
    
    // evaluate blendshapes
#ifdef EIGEN_USE_BLAS
    blendshapeVerticesFlattened.noalias() += blendshapeMatrixDense[lod] * state.diffMeshBlendshapes.Value();
#else
    // if we don't use MKL, then for large matrices (head) we parallelize the matrix vector product
    const int numVertices = int(state.blendshapeVertices.cols());
    if (blendshapeMatrixDense[lod].size() > 30000 && taskThreadPool)
    { // JaneH this code path causes an assertion in the UE TaskGraph for DNAs including blendshapes so have raised the threshold to a value which does not trigger this for now
        auto parallelMatrixMultiply = [&](int start, int end)
        {
            blendshapeVerticesFlattened.segment(start, end - start).noalias() += blendshapeMatrixDense[lod].block(start, 0, end - start, state.diffMeshBlendshapes.Size()) * state.diffMeshBlendshapes.Value();
        };
        taskThreadPool->AddTaskRangeAndWait(3 * numVertices, parallelMatrixMultiply);
    } else {
        blendshapeVerticesFlattened += blendshapeMatrixDense[lod] * state.diffMeshBlendshapes.Value();
    }
#endif

    if (state.diffMeshBlendshapes.HasJacobian()) {
        const int blendshapeJacobianColOffset = state.diffMeshBlendshapes.Jacobian().StartCol();
        SparseMatrix<T> diffBlendshapesSparseMatrixTransposed = state.diffMeshBlendshapes.Jacobian().AsSparseMatrix()->transpose();
        Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& jacobianMatrix = *(state.blendshapeJacobianRM);
        jacobianMatrix.resize(blendshapeMatrixDense[lod].rows(), state.diffMeshBlendshapes.Jacobian().Cols() - state.diffMeshBlendshapes.Jacobian().StartCol());

        auto calculate_dVertex_dBlendshapes_rm = [&](int start, int end) {
            for (int r = start; r < end; ++r) {
                for (int ctrl = blendshapeJacobianColOffset; ctrl < int(diffBlendshapesSparseMatrixTransposed.rows()); ++ctrl) {
                    T acc = 0;
                    for (typename SparseMatrix<T>::InnerIterator it(diffBlendshapesSparseMatrixTransposed, ctrl); it; ++it)
                    {
                        acc += it.value() * blendshapeMatrixDense[lod](r, it.col());
                    }
                    jacobianMatrix(r, ctrl - blendshapeJacobianColOffset) = acc;
                }
            }
        };
        if (jacobianMatrix.size() > 10000 && taskThreadPool) { // TODO: figure out a good magic number
            taskThreadPool->AddTaskRangeAndWait(int(state.blendshapeVertices.size()), calculate_dVertex_dBlendshapes_rm);
        } else {
            calculate_dVertex_dBlendshapes_rm(0, int(state.blendshapeVertices.size()));
        }

        state.blendshapeJacobianColOffset = blendshapeJacobianColOffset;
    }
}

template <class T>
void BodyGeometry<T>::EvaluateIndexedBlendshapes(const int lod, const DiffData<T>& diffPsd, State& state, const std::vector<int>& indices) const
{
    // copy neutral
    state.blendshapeVertices.resize(3, mesh[lod].Vertices().cols());
    state.blendshapeJacobianColOffset = -1;

    for (const auto& vID : indices) {
        state.blendshapeVertices.col(vID) = mesh[lod].Vertices().col(vID);
    }

    // no blendshapes, then return neutral
    if (blendshapeControlsToMeshBlendshapeControls[lod].size() == 0) {
        return;
    }

    auto blendshapeVerticesFlattened = state.blendshapeVertices.reshaped();

    // get the blendshape activations for this mesh
    state.diffMeshBlendshapes = GatherFunction<T>::Gather(diffPsd, blendshapeControlsToMeshBlendshapeControls[lod]);

    // evaluate blendshapes
    for (const auto& vID : indices) {
        blendshapeVerticesFlattened.segment(vID * 3, 3).noalias() += blendshapeMatrixDense[lod].block(vID * 3, 0, 3, state.diffMeshBlendshapes.Size()) * state.diffMeshBlendshapes.Value();
    }

    if (state.diffMeshBlendshapes.HasJacobian()) {
        const int blendshapeJacobianColOffset = state.diffMeshBlendshapes.Jacobian().StartCol();
        SparseMatrix<T> diffBlendshapesSparseMatrixTransposed = state.diffMeshBlendshapes.Jacobian().AsSparseMatrix()->transpose();
        Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& jacobianMatrix = *(state.blendshapeJacobianRM);
        jacobianMatrix.resize(blendshapeMatrixDense[lod].rows(), state.diffMeshBlendshapes.Jacobian().Cols() - state.diffMeshBlendshapes.Jacobian().StartCol());

        auto processIndexedBlendshapesJacobian = [&](int start, int end)
        {
            for (int i = start; i < end; ++i)
            {
                const int vID = indices[i];
                for (int ctrl = blendshapeJacobianColOffset; ctrl < int(diffBlendshapesSparseMatrixTransposed.rows()); ++ctrl) {
                    Eigen::Vector3<T> acc = Eigen::Vector3<T>::Zero();
                    for (typename SparseMatrix<T>::InnerIterator it(diffBlendshapesSparseMatrixTransposed, ctrl); it; ++it) {
                        acc += it.value() * blendshapeMatrixDense[lod](Eigen::seqN(3 * vID, 3), it.col());
                    }
                    jacobianMatrix(Eigen::seqN(3 * vID, 3), ctrl - blendshapeJacobianColOffset) = acc;
                }
            }
        };
        if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait((int)indices.size(), processIndexedBlendshapesJacobian);
        else processIndexedBlendshapesJacobian(0, (int)indices.size());

        state.blendshapeJacobianColOffset = blendshapeJacobianColOffset;
    }
}

template <class T>
DiffDataMatrix<T, 3, -1> CreateDiffDataMatrix(const Eigen::Matrix<T, 3, -1>& matrix, const std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>>& denseJacobian, int colOffset)
{
    if (denseJacobian && denseJacobian->size() > 0) {
        return DiffDataMatrix<T, 3, -1>(matrix, std::make_shared<DenseJacobian<T>>(denseJacobian, colOffset));
    } else {
        return DiffDataMatrix<T, 3, -1>(matrix);
    }
}

template <class T>
typename BodyGeometry<T>::State& BodyGeometry<T>::EvaluateBodyGeometry(const int lod, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, State& state) const
{
    const bool requiresJacobians = diffJoints.HasJacobian() || diffPsd.HasJacobian();
    EvaluateBlendshapes(lod, diffPsd, state);
    
    if (NumJoints() > 0) {
        if (requiresJacobians) EvaluateJointDeltas(diffJoints, state);
        else EvaluateJointDeltasWithoutJacobians(diffJoints, state);
    } else {
        if (diffJoints.Size() > 0) {
            LOG_ERROR("BodyGeometry does not contain joints, but BodyGeometry is called with deltas on joints");
        }
    }

    if (requiresJacobians) EvaluateSkinningWithJacobians(lod, state);
    else EvaluateSkinningWithoutJacobians(lod, state);

    if (state.finalJacobianColOffset >= 0) {
        state.vertices = CreateDiffDataMatrix(state.finalVertices, state.finalJacobianRM, state.finalJacobianColOffset);
    } else {
        state.vertices = DiffDataMatrix<T, 3, -1>(state.finalVertices);
    }

    return state;
}

template <class T>
typename BodyGeometry<T>::State& BodyGeometry<T>::EvaluateIndexedBodyGeometry(const int lod, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, const std::vector<int>& indices, State& state) const {
    const bool requiresJacobians = diffJoints.HasJacobian() || diffPsd.HasJacobian();
    EvaluateIndexedBlendshapes(lod, diffPsd, state, indices);
    
    if (NumJoints() > 0) {
        if (requiresJacobians) EvaluateJointDeltas(diffJoints, state);
        else EvaluateJointDeltasWithoutJacobians(diffJoints, state);
    }
    else {
        if (diffJoints.Size() > 0) {
            LOG_ERROR("BodyGeometry does not contain joints, but BodyGeometry is called with deltas on joints");
        }
    }

    if (requiresJacobians) EvaluateIndexedSkinningWithJacobians(lod, state, indices);
    else EvaluateIndexedSkinningWithoutJacobians(lod, state, indices);
    
    if (state.finalJacobianColOffset >= 0) {
        state.vertices = CreateDiffDataMatrix(state.finalVertices, state.finalJacobianRM, state.finalJacobianColOffset);
    }
    else {
        state.vertices = DiffDataMatrix<T, 3, -1>(state.finalVertices);
    }
    
    return state;
}

template <class T>
typename BodyGeometry<T>::State& BodyGeometry<T>::EvaluateBodyGeometryWithOffset(const int lod, const Eigen::Matrix3X<T>& offset, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, State& state) const {
    const bool requiresJacobians = diffJoints.HasJacobian() || diffPsd.HasJacobian();

    EvaluateBlendshapes(lod, diffPsd, state);

    state.blendshapeVertices += offset;

    if (NumJoints() > 0) {
        if (requiresJacobians) EvaluateJointDeltas(diffJoints, state);
        else EvaluateJointDeltasWithoutJacobians(diffJoints, state);
    }
    else {
        if (diffJoints.Size() > 0) {
            LOG_ERROR("BodyGeometry does not contain joints, but BodyGeometry is called with deltas on joints");
        }
    }

    if (requiresJacobians) EvaluateSkinningWithJacobians(lod, state);
    else EvaluateSkinningWithoutJacobians(lod, state);
    if (state.finalJacobianColOffset >= 0) {
        state.vertices = CreateDiffDataMatrix(state.finalVertices, state.finalJacobianRM, state.finalJacobianColOffset);
    }
    else {
        state.vertices = DiffDataMatrix<T, 3, -1>(state.finalVertices);
    }

    return state;
}

template <class T>
typename BodyGeometry<T>::State BodyGeometry<T>::EvaluateBodyGeometry(const int lod, const DiffData<T>& joints, const DiffData<T>& psd) const
{
    State state;
    EvaluateBodyGeometry(lod, joints, psd, state);
    return state;
}

template <class T>
void BodyGeometry<T>::EvaluateSkinningWithJacobians(const int lod, State& state) const
{
    // rest vertices are the vertices after blendshape evaluation
    const Eigen::Matrix<T, 3, -1>& restVertices = state.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = state.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    const int numVertices = int(vertexInfluenceWeights[lod].outerSize());

    if (int(vertexInfluenceWeights[lod].outerSize()) != int(restVertices.cols())) {
        CARBON_CRITICAL("all vertices need to be influenced by a node");
    }

    // get column size for jacobian
    int maxCols = -1;
    int startCol = std::numeric_limits<int>::max();

    if (state.jointJacobianColOffset >= 0) {
        maxCols = std::max<int>(int(state.skinningMatricesJacobian.cols()) + state.jointJacobianColOffset, maxCols);
        startCol = std::min<int>(state.jointJacobianColOffset, startCol);
    }

    if (state.blendshapeJacobianColOffset >= 0) {
        maxCols = std::max<int>(int(state.blendshapeJacobianRM->cols()) + state.blendshapeJacobianColOffset, maxCols);
        startCol = std::min<int>(state.blendshapeJacobianColOffset, startCol);
    }
    startCol = std::max<int>(0, startCol);

    Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& denseJacobian = *(state.finalJacobianRM);
    denseJacobian.resize(3 * numVertices, maxCols - startCol);

    auto evaluateVertexSkinning = [&](int start, int end) {
        for (int vID = start; vID < end; ++vID) {
            if (denseJacobian.cols() > 0) {
                denseJacobian.block(3 * vID, 0, 3, denseJacobian.cols()).setZero();
            }
            Eigen::Vector3<T> result(0,0,0);
            for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights[lod], vID); it; ++it)
            {
                const int64_t& jointIndex = it.col();
                const T& weight = it.value();
                result += weight * (state.skinningMatrices[jointIndex] * restVertices.col(vID));

                if (state.jointJacobianColOffset >= 0) {
                    const int colOffset = state.jointJacobianColOffset - startCol;
                    const int jacCols = int(state.skinningMatricesJacobian.cols());
                    for (int j = 0; j < 3; j++) {
                        denseJacobian.block(3 * vID, colOffset, 3, jacCols) += (weight * restVertices(j, vID)) * state.skinningMatricesJacobian.block(12 * jointIndex + 3 * j, 0, 3, jacCols);
                    }
                    denseJacobian.block(3 * vID, colOffset, 3, jacCols) += weight * state.skinningMatricesJacobian.block(12 * jointIndex + 9, 0, 3, jacCols);
                }

                if (state.blendshapeJacobianColOffset >= 0) {
                    const int colOffset = state.blendshapeJacobianColOffset - startCol;
                    denseJacobian.block(3 * vID, colOffset, 3, state.blendshapeJacobianRM->cols()) += (weight * state.skinningMatrices[jointIndex].linear()) * state.blendshapeJacobianRM->block(3 * vID, 0, 3, state.blendshapeJacobianRM->cols());
                }
            }
            deformedVertices.col(vID) = result;
        }
    };

    if (denseJacobian.size() > 5000 && taskThreadPool) { // TODO: figure out a good magic number
        taskThreadPool->AddTaskRangeAndWait(int(vertexInfluenceWeights[lod].outerSize()), evaluateVertexSkinning);
    } else {
        evaluateVertexSkinning(0, int(vertexInfluenceWeights[lod].outerSize()));
    }

    state.finalJacobianColOffset = (maxCols > 0) ? startCol : -1;
}

template <class T>
void BodyGeometry<T>::EvaluateSkinningWithoutJacobians(const int lod, State& state) const
{
    const Eigen::Matrix<T, 3, -1>& restVertices = state.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = state.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    const int numVertices = int(vertexInfluenceWeights[lod].outerSize());

    for (int vID = 0; vID < numVertices; ++vID) {
        Eigen::Vector3<T> result(0,0,0);
        for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights[lod], vID); it; ++it)
        {
            result.noalias() += it.value() * (state.skinningMatrices[it.col()] * restVertices.col(vID));
        }
        deformedVertices.col(vID) = result;
    }

    state.finalJacobianColOffset = -1;
}

template <class T>
const Eigen::Matrix3X<T> BodyGeometry<T>::EvaluateInverseSkinning(const int lod, const BodyGeometry<T>::State& state, const Eigen::Matrix3X<T>& vertices) {
    const int numVertices = int(vertexInfluenceWeights[lod].outerSize());

    Eigen::Matrix3X<T> result(3, numVertices);

    for (int vID = 0; vID < numVertices; ++vID) {
        Eigen::Matrix4<T> t = Eigen::Matrix4<T>::Zero();
        for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights[lod], vID); it; ++it) {
            t += it.value() * state.skinningMatrices[it.col()].matrix();
        }
        t = t.inverse();
        result.col(vID) = t.template topLeftCorner<3, 3>() * vertices.col(vID) + t.template topRightCorner<3, 1>();
    }

    return result;
}

template <class T>
void BodyGeometry<T>::EvaluateIndexedSkinningWithJacobians(const int lod, State& state, const std::vector<int>& indices) const
{
    // rest vertices are the vertices after blendshape evaluation
    const Eigen::Matrix<T, 3, -1>& restVertices = state.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = state.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    const int numVertices = int(vertexInfluenceWeights[lod].outerSize());

    if (int(vertexInfluenceWeights[lod].outerSize()) != int(restVertices.cols())) {
        CARBON_CRITICAL("all vertices need to be influenced by a node");
    }

    // get column size for jacobian
    int maxCols = -1;
    int startCol = std::numeric_limits<int>::max();

    if (state.jointJacobianColOffset >= 0) {
        maxCols = std::max<int>(int(state.skinningMatricesJacobian.cols()) + state.jointJacobianColOffset, maxCols);
        startCol = std::min<int>(state.jointJacobianColOffset, startCol);
    }

    if (state.blendshapeJacobianColOffset >= 0) {
        maxCols = std::max<int>(int(state.blendshapeJacobianRM->cols()) + state.blendshapeJacobianColOffset, maxCols);
        startCol = std::min<int>(state.blendshapeJacobianColOffset, startCol);
    }
    startCol = std::max<int>(0, startCol);

    Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& denseJacobian = *(state.finalJacobianRM);
    denseJacobian.resize(3 * numVertices, maxCols - startCol);

    auto processIndexedSkinning = [&](int start, int end) {
        for (int i = start; i < end; ++i)
        {
            const int vID = indices[i];
            if (denseJacobian.cols() > 0) {
                denseJacobian.block(3 * vID, 0, 3, denseJacobian.cols()).setZero();
            }
            Eigen::Vector3<T> result(0, 0, 0);
            for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights[lod], vID); it; ++it) {
                const int64_t& jointIndex = it.col();
                const T& weight = it.value();
                result += weight * (state.skinningMatrices[jointIndex] * restVertices.col(vID));

                if (state.jointJacobianColOffset >= 0) {
                    const int colOffset = state.jointJacobianColOffset - startCol;
                    const int jacCols = int(state.skinningMatricesJacobian.cols());
                    for (int j = 0; j < 3; j++) {
                        denseJacobian.block(3 * vID, colOffset, 3, jacCols).noalias() += (weight * restVertices(j, vID)) * state.skinningMatricesJacobian.block(12 * jointIndex + 3 * j, 0, 3, jacCols);
                    }
                    denseJacobian.block(3 * vID, colOffset, 3, jacCols).noalias() += weight * state.skinningMatricesJacobian.block(12 * jointIndex + 9, 0, 3, jacCols);
                }

                if (state.blendshapeJacobianColOffset >= 0) {
                    const int colOffset = state.blendshapeJacobianColOffset - startCol;
                    denseJacobian.block(3 * vID, colOffset, 3, state.blendshapeJacobianRM->cols()).noalias() += (weight * state.skinningMatrices[jointIndex].linear()) * state.blendshapeJacobianRM->block(3 * vID, 0, 3, state.blendshapeJacobianRM->cols());
                }
            }
            deformedVertices.col(vID) = result;
        }
    };
    if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait((int)indices.size(), processIndexedSkinning);
    else processIndexedSkinning(0, (int)indices.size());

    state.finalJacobianColOffset = (maxCols > 0) ? startCol : -1;
}

template <class T>
void BodyGeometry<T>::EvaluateIndexedSkinningWithoutJacobians(const int lod, State& state, const std::vector<int>& indices) const {
    const Eigen::Matrix<T, 3, -1>& restVertices = state.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = state.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    // const int numVertices = int(vertexInfluenceWeights[lod].outerSize());

    for (const auto& vID: indices) {
        Eigen::Vector3<T> result(0, 0, 0);
        for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights[lod], vID); it; ++it) {
            result += it.value() * (state.skinningMatrices[it.col()] * restVertices.col(vID));
        }
        deformedVertices.col(vID) = result;
    }

    state.finalJacobianColOffset = -1;
}

// explicitly instantiate the rig geometry classes
template class BodyGeometry<float>;
template class BodyGeometry<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)