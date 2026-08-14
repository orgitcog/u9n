// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RigGeometry.h>

#include <carbon/Algorithm.h>
#include <carbon/geometry/AABBTree.h>
#include <carbon/utils/StringReplace.h>
#include <carbon/utils/StringUtils.h>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/utils/Timer.h>
#include <nls/functions/GatherFunction.h>
#include <nls/geometry/EulerAngles.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/Jacobians.h>
#include <nls/geometry/Procrustes.h>
#include <nls/geometry/SubdivisionMesh.h>
#include <rig/JointRig2.h>

#include <riglogic/RigLogic.h>

CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/Geometry>
CARBON_RENABLE_WARNINGS

#include <algorithm>
#include <cctype>
#include <iostream>
#include <thread>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// ###################################################################################################################################
template <class T> RigGeometry<T>::State::State() = default;
template <class T> RigGeometry<T>::State::~State() = default;
template <class T> RigGeometry<T>::State::State(State&&) = default;
template <class T>
typename RigGeometry<T>::State& RigGeometry<T>::State::operator=(State&&) = default;

template <class T>
const std::vector<DiffDataMatrix<T, 3, -1>>& RigGeometry<T>::State::Vertices() const
{
    return m_vertices;
}

template <class T>
const Eigen::Matrix<T, 3, -1>& RigGeometry<T>::State::BlendshapeVertices(int meshIndex) const
{
    if (meshIndex >= (int)m_meshJacobianData.size())
    {
        CARBON_CRITICAL("mesh index {} is not valid", meshIndex);
    }
    return m_meshJacobianData[meshIndex].blendshapeVertices;
}

template <class T>
const std::vector<int>& RigGeometry<T>::State::MeshIndices() const
{
    return m_meshIndices;
}

template <class T>
Eigen::Matrix<T, 4, 4> RigGeometry<T>::State::GetWorldMatrix(int jointIndex) const
{
    return m_worldMatrices[jointIndex].matrix();
}

template <class T>
Eigen::Matrix<T, 4, 4> RigGeometry<T>::State::GetLocalMatrix(int jointIndex) const
{
    return m_localMatrices[jointIndex].matrix();
}

//! @return the current skinning matrix for a joint
template <class T>
Eigen::Matrix<T, 4, 4> RigGeometry<T>::State::GetSkinningMatrix(int jointIndex) const
{
    return m_skinningMatrices[jointIndex].matrix();
}

//! @return the current skinning matrices for all joints
template <class T>
std::vector<Eigen::Matrix<T, 4, 4>> RigGeometry<T>::State::GetAllSkinningMatrices() const
{
    std::vector<Eigen::Matrix<T, 4, 4>> skinningMatrices(m_skinningMatrices.size());

    for (int i = 0; i < (int)m_skinningMatrices.size(); ++i)
    {
        skinningMatrices[i] = m_skinningMatrices[i].matrix();
    }

    return skinningMatrices;
}

template <class T>
void RigGeometry<T>::State::SetLocalMatrix(int jointIndex, const Eigen::Matrix<T, 4, 4>& jointTransform) { m_localMatrices[jointIndex] = jointTransform; }

template <class T>
std::vector<DiffDataMatrix<T, 3, -1>> RigGeometry<T>::State::MoveVertices()
{
    m_meshIndices.clear();
    return std::move(m_vertices);
}

template <class T>
void RigGeometry<T>::State::SetupForMesh(int meshIndex)
{
    if (meshIndex >= int(m_meshJacobianData.size()))
    {
        m_meshJacobianData.resize(meshIndex + 1);
    }
}

struct BlendshapeInfo
{
    std::vector<std::string> blendshapeNames;
    std::vector<uint16_t> blendshapeInputIndices;
    std::vector<uint16_t> blendshapeOutputIndices;

    void Clear()
    {
        blendshapeNames.clear();
        blendshapeInputIndices.clear();
        blendshapeOutputIndices.clear();
    }

    int NumBlendshapes() const { return (int)blendshapeNames.size(); }
};

// ###################################################################################################################################
/**
 * @brief MeshData containing name, topology, blendshape mapping and blendshape matrix.
 */
template <class T>
struct MeshData
{
    //! the name of the mesh
    std::string meshName;

    //! the mesh topology
    Mesh<T> mesh;

    //! selection of which blendshapes are used by this mesh => for each blendshape target of this mesh, the corresponding psd input control
    Eigen::VectorXi blendshapeControlsToMeshBlendshapeControls;

    //! map from each blendshape target to the blendshape channel - @see BlendshapeInfo
    std::vector<uint16_t> blendshapeChannels;

    // we keep a dense blendshape matrix for evaluation as the blendshape matrix only has 50% zeros, so sparse matrix
    // multiplication is significantly more expensive
    Eigen::Matrix<T, -1, -1> blendshapeMatrixDense;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> blendshapeMatrixDenseRM;
};

// ###################################################################################################################################
template <class T>
struct RigGeometry<T>::Private
{
    std::vector<std::shared_ptr<const MeshData<T>>> meshData;
    std::vector<std::vector<int>> meshIndicesForLOD;
    std::vector<std::vector<int>> jointIndicesForLOD;
    BlendshapeInfo blendshapeInfo;
    std::vector<int> blendshapeMeshIndices;
    Eigen::Matrix<T, 3, -1> jointRestPose;
    Eigen::Matrix<T, 3, -1> jointRestOrientationEuler;
    std::vector<Eigen::Matrix<T, 3, 3>> jointRestOrientation;
    bool withJointScaling = false;

    JointRig2<T> jointRig2;
    std::vector<std::vector<int>> jointIndicesPerHierarchyLevel;
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointRig2StateBindPoses;
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointRig2StateInverseBindPoses;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> taskThreadPool {};
};

// ###################################################################################################################################
template <class T>
RigGeometry<T>::RigGeometry(bool useMultithreading) : m(new Private)
{
    if (useMultithreading)
    {
        m->taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
    }
}

template <class T> RigGeometry<T>::~RigGeometry() = default;
template <class T> RigGeometry<T>::RigGeometry(RigGeometry&&) = default;
template <class T> RigGeometry<T>& RigGeometry<T>::operator=(RigGeometry&&) = default;

template <class T>
std::shared_ptr<RigGeometry<T>> RigGeometry<T>::Clone() const
{
    std::shared_ptr<RigGeometry> clone = std::make_shared<RigGeometry>();
    *clone->m = *m;
    return clone;
}

template <class T>
Eigen::Matrix<T, 4, 4> RigGeometry<T>::GetBindMatrix(int jointIndex) const
{
    return m->jointRig2StateBindPoses[jointIndex].matrix();
}

template <class T>
Eigen::Matrix<T, 4, 4> RigGeometry<T>::GetRestMatrix(int jointIndex) const
{
    Eigen::Transform<T, 3, Eigen::Affine> transform;
    transform.linear() = m->jointRestOrientation[jointIndex];
    transform.translation() = m->jointRestPose.col(jointIndex);

    return transform.matrix();
}

template <class T>
Eigen::Transform<T, 3, Eigen::Affine> RigGeometry<T>::GetRestPose(int jointIndex) const
{
    Eigen::Transform<T, 3, Eigen::Affine> transform = Eigen::Transform<T, 3, Eigen::Affine>::Identity();
    transform.linear() = m->jointRestOrientation[jointIndex];
    transform.translation() = m->jointRestPose.col(jointIndex);
    return transform;
}

template <class T>
const Eigen::Matrix<T, 3, -1>& RigGeometry<T>::GetRestOrientationEuler() const
{
    return m->jointRestOrientationEuler;
}

template <class T>
const Eigen::Matrix<T, 3, -1>& RigGeometry<T>::GetRestPose() const
{
    return m->jointRestPose;
}

template <class T>
void RigGeometry<T>::CalculateLocalJointTransformsFromWorldTransforms(const std::vector<Affine<T, 3, 3>>& jointWorldTransforms, 
    Eigen::Matrix<T, 3, -1>& restPose, Eigen::Matrix<T, 3, -1> & restOrientationEulers) const
{
    const auto& jointRig = GetJointRig();
    restPose = Eigen::Matrix<T, 3, -1>(3, jointRig.NumJoints());
    restOrientationEulers = Eigen::Matrix<T, 3, -1>(3, jointRig.NumJoints());
    const T convertToDegrees = 180 / CARBON_PI;

    for (std::uint16_t jointIndex = 0; jointIndex < jointRig.NumJoints(); jointIndex++)
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

        restPose.col(jointIndex) = localTransform.Translation().template cast<T>();
        restOrientationEulers.col(jointIndex) = RotationMatrixToEulerXYZ(localTransform.Linear().template cast<T>()) * convertToDegrees;
    }
}


template <class T>
void RigGeometry<T>::SetRestPose(Eigen::Matrix<T, 3, -1> restPose, CoordinateSystem coordSystem)
{
    if (restPose.cols() != m->jointRestPose.cols())
    {
        CARBON_CRITICAL("rest pose matrix does not have correct size");
    }

    switch (coordSystem)
    {
    case CoordinateSystem::Local:
        break;

    case CoordinateSystem::World:
    {

        std::vector<Affine<T, 3, 3>> jointWorldTransforms(GetJointRig().NumJoints());
        for (int i = 0; i < GetJointRig().NumJoints(); ++i)
        {
            jointWorldTransforms[i] = GetBindMatrix(i);
            jointWorldTransforms[i].SetTranslation(restPose.col(i));
        }
        Eigen::Matrix<T, 3, -1> restOrientationEulers;
        CalculateLocalJointTransformsFromWorldTransforms(jointWorldTransforms, restPose, restOrientationEulers);
        break;
    }

    default:
        CARBON_CRITICAL("unsupported coordinate system");
        break;
    }

    m->jointRestPose = restPose;
    UpdateBindPoses();
}



//! Calculates the Euler angles given the rotation matrix assuming the XYZ order of Maya i.e. in post-multiply this is EulerZ() * EulerY() * EulerX()
template <typename T>
std::vector<Eigen::Vector3<T>> RotationMatrixToEulerXYZOptions(const Eigen::Matrix<T, 3, 3>& R)
{
    std::vector<Eigen::Vector3<T>> out;
    Eigen::Vector3<T> angles = R.eulerAngles(2, 1, 0);
    out.push_back(angles);
    auto clampAngle = [](T angle)
    {
        while (angle < -T(CARBON_PI))
        {
            angle += T(2 * CARBON_PI);
        }
        while (angle > T(CARBON_PI))
        {
            angle -= T(2 * CARBON_PI);
        }
        return angle;
    };
    if (angles[1] > 0)
    {
        Eigen::Vector3<T> altAngles = angles;
        altAngles[0] = clampAngle(angles[0] - T(CARBON_PI));
        altAngles[1] = T(CARBON_PI) - angles[1];
        altAngles[2] = clampAngle(angles[2] - T(CARBON_PI));
        out.push_back(altAngles);
    }
    else if (angles[1] < 0)
    {
        Eigen::Vector3<T> altAngles = angles;
        altAngles[0] = clampAngle(angles[0]-  T(CARBON_PI));
        altAngles[1] = -T(CARBON_PI) - angles[1];
        altAngles[2] = clampAngle(angles[2] - T(CARBON_PI));
        out.push_back(altAngles);
    }

    for (auto& item : out)
    {
        // Eigen returns the XYZ angles above in order 2, 1, 0, but we return the result as the x angle, y angle, and z angle
        item = Eigen::Vector3<T>(item[2], item[1], item[0]);
    }
    return out;
}

template <class T>
std::vector<Eigen::Transform<T, 3, Eigen::Affine>> RigGeometry<T>::GetPreviousBindPose(const Eigen::Matrix<T, 3, -1>& restOrientationEuler) const
{
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> prevBindPoses = m->jointRig2StateBindPoses;

    for (int jointIndex = 0; jointIndex < (int)restOrientationEuler.cols(); ++jointIndex)
    {
        const T diff = (restOrientationEuler.col(jointIndex) - m->jointRestOrientationEuler.col(jointIndex)).norm();
        if (diff > T(1e-4))
        {
            LOG_VERBOSE("rest orientation change for joint: \"{}\" ({}): {}° ({} => {})",
                m->jointRig2.GetJointNames()[jointIndex], jointIndex,
                diff,
                m->jointRestOrientationEuler.col(jointIndex).transpose(),
                restOrientationEuler.col(jointIndex).transpose());
        }
    }

    return prevBindPoses;
}

template <class T>
void RigGeometry<T>::UpdateRestPose(const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& prevBindPoses)
{
    // update rest pose so that joint positions are the same as before
    for (int jointIndex = 0; jointIndex < m->jointRig2.NumJoints(); ++jointIndex)
    {
        const int parentIndex = m->jointRig2.GetParentIndex(jointIndex);
        if (parentIndex >= 0)
        {
            const Eigen::Vector4<T> prevPosition = prevBindPoses[jointIndex].matrix().block(0, 3, 4, 1);
            Eigen::Matrix<T, 4, 4> newParentBindPose = m->jointRig2StateBindPoses[parentIndex].matrix();
            newParentBindPose.matrix().block(0, 3, 3, 1) = prevBindPoses[parentIndex].matrix().block(0, 3, 3, 1);
            m->jointRestPose.col(jointIndex) = (newParentBindPose.inverse() * prevPosition).template head<3>();
        }
    }

    UpdateBindPoses();

    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>> newBindPoses = m->jointRig2StateBindPoses;

    std::vector<bool> bindPoseChanged(newBindPoses.size(), false);
    for (int jointIndex = 0; jointIndex < (int)newBindPoses.size(); ++jointIndex)
    {
        // check which joints change in bind pose
        const T poseRotationChange = Eigen::Quaternion<T>(prevBindPoses[jointIndex].linear()).angularDistance(Eigen::Quaternion<T>(newBindPoses[jointIndex].linear())) * rad2degreeScale<T>();
        bindPoseChanged[jointIndex] = poseRotationChange > T(1e-4);
        if (bindPoseChanged[jointIndex])
        {
            LOG_VERBOSE("bind pose orientation change for joint \"{}\" ({}): {}°", jointIndex, m->jointRig2.GetJointNames()[jointIndex], poseRotationChange);
        }
    }
}

template <class T>
void RigGeometry<T>::SetRestOrientationEuler(Eigen::Matrix<T, 3, -1> restOrientationEuler, CoordinateSystem coordSystem, bool bUpdateJointPositionsToPreviousValues)
{
    if (restOrientationEuler.cols() != m->jointRestOrientationEuler.cols())
    {
        CARBON_CRITICAL("rest pose matrix does not have correct size");
    }

    switch (coordSystem)
    {
    case CoordinateSystem::Local:
        break;

    case CoordinateSystem::World:
    {
        // convert eulers to rotations and set in the jointWorldTransforms below. Then calculate local from world
        std::vector<Affine<T, 3, 3>> jointWorldTransforms(GetJointRig().NumJoints());
        for (int i = 0; i < GetJointRig().NumJoints(); ++i)
        {
            jointWorldTransforms[i] = GetBindMatrix(i);
            Eigen::Matrix<T, 3, 3> restOrientation = EulerXYZ<T>(restOrientationEuler.col(i) * degree2radScale<T>());
            jointWorldTransforms[i].SetLinear(restOrientation);
        }
        Eigen::Matrix<T, 3, -1> restPose;
        CalculateLocalJointTransformsFromWorldTransforms(jointWorldTransforms, restPose, restOrientationEuler);
        break;
    }

    default:
        CARBON_CRITICAL("unsupported coordinate system");
        break;
    }

    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> prevBindPoses;
    if (bUpdateJointPositionsToPreviousValues)
    {
        // get previous bind pose
        prevBindPoses = GetPreviousBindPose(restOrientationEuler);
    }


    m->jointRestOrientationEuler = restOrientationEuler;

    // update the rest orientation matrix
    for (int jointIndex = 0; jointIndex < (int)m->jointRestOrientationEuler.cols(); ++jointIndex)
    {
        m->jointRestOrientation[jointIndex] = EulerXYZ<T>(m->jointRestOrientationEuler.col(jointIndex) * degree2radScale<T>());
    }

    UpdateBindPoses();


    if (bUpdateJointPositionsToPreviousValues)
    {
        UpdateRestPose(prevBindPoses);
    }
}

template <class T>
void RigGeometry<T>::UpdateRestOrientationEuler(const Eigen::Matrix<T, 3, -1>& restOrientationEuler, RigLogic<T>& rigLogic) // TODO add world or local option here too
{
    // get previous bind pose
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>> prevBindPoses = GetPreviousBindPose(restOrientationEuler);

    // get the world matrices for each activation and lod
    std::vector<std::vector<std::vector<Eigen::Transform<T, 3, Eigen::Affine>>>> worldMatrices(rigLogic.NumLODs());
    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        State state;
        const SparseMatrix<T>& jointMatrix = rigLogic.JointMatrix(lod);
        for (int c = 0; c < (int)jointMatrix.cols(); ++c)
        {
            Eigen::VectorX<T> jointDeltas = jointMatrix.col(c);
            EvaluateJointDeltasWithoutJacobians(DiffDataAffine<T, 3, 3>(), DiffData<T>(jointDeltas), state);
            worldMatrices[lod].push_back(state.GetWorldMatrices());
        }
    }


    // set new rest orientation but without updating the rest pose
    SetRestOrientationEuler(restOrientationEuler, CoordinateSystem::Local, /*bUpdateJointPositionsToPreviousValues*/ false);

    // update rest pose so that joint positions are the same as before
    UpdateRestPose(prevBindPoses);

    // get new world matrices for each activation
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>> newBindPoses = m->jointRig2StateBindPoses;

    std::vector<std::vector<std::vector<Eigen::Transform<T, 3, Eigen::Affine>>>> newWorldMatrices = worldMatrices;
    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        for (size_t i = 0; i < newWorldMatrices[lod].size(); ++i)
        {
            for (size_t jointIndex = 0; jointIndex < newWorldMatrices[lod][i].size(); ++jointIndex)
            {
                const Eigen::Transform<T, 3, Eigen::Affine> skinning = worldMatrices[lod][i][jointIndex] * prevBindPoses[jointIndex].inverse();
                newWorldMatrices[lod][i][jointIndex] = skinning * newBindPoses[jointIndex];
            }
        }
    }

    // get new local matrices for each activation
    std::vector<std::vector<std::vector<Eigen::Transform<T, 3, Eigen::Affine>>>> newLocalMatrices = newWorldMatrices;
    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        for (size_t i = 0; i < newLocalMatrices[lod].size(); ++i)
        {
            for (int jointIndex = 0; jointIndex < (int)newLocalMatrices[lod][i].size(); ++jointIndex)
            {
                const int parentIndex = m->jointRig2.GetParentIndex(jointIndex);
                if (parentIndex >= 0)
                {
                    newLocalMatrices[lod][i][jointIndex] = newWorldMatrices[lod][i][parentIndex].inverse() * newWorldMatrices[lod][i][jointIndex];
                }
            }
        }
    }

    // get new delta transforms for each activation
    std::vector<std::vector<std::vector<Eigen::Transform<T, 3, Eigen::Affine>>>> newDeltaTransforms = newWorldMatrices;

    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        for (size_t i = 0; i < newWorldMatrices[lod].size(); ++i)
        {
            for (size_t jointIndex = 0; jointIndex < newLocalMatrices[lod][i].size(); ++jointIndex)
            {
                const auto& newLocalMatrix = newLocalMatrices[lod][i][jointIndex];
                const Eigen::Matrix<T, 3, 3> jointRotation = m->jointRestOrientation[jointIndex];
                const Eigen::Matrix<T, 3, 3> jointDeltaRotation = jointRotation.transpose() * newLocalMatrix.linear();
                newDeltaTransforms[lod][i][jointIndex].linear() = jointDeltaRotation;

                const Eigen::Vector3<T> jointTranslation = m->jointRestPose.col(jointIndex);
                Eigen::Vector3<T> jointDeltaTranslation = newLocalMatrix.translation() - jointTranslation;

                newDeltaTransforms[lod][i][jointIndex].translation() = jointDeltaTranslation;
            }
        }
    }

    std::vector<bool> translationUpdated(m->jointRig2.NumJoints(), false);
    std::vector<bool> rotationUpdated(m->jointRig2.NumJoints(), false);
    std::vector<bool> scaleUpdated(m->jointRig2.NumJoints(), false);
    const T translationEps = T(1e-4);
    const T rotationEps = T(1e-4);
    const T scaleEps = T(1e-4);
    const int entriesPerJoint = rigLogic.WithJointScaling() ? 9 : 6;
    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        std::vector<Eigen::Triplet<T>> triplets;
        SparseMatrix<T> jointMatrix = rigLogic.JointMatrix(lod);
        Eigen::SparseMatrix<T> jointMatrixCM = jointMatrix;

        for (int i = 0; i < (int)newDeltaTransforms[lod].size(); ++i)
        {
            for (int jointIndex = 0; jointIndex < (int)newDeltaTransforms[lod][i].size(); ++jointIndex)
            {
                Eigen::Vector<T, 9> prevJointDelta;
                prevJointDelta.segment(6, 3).setOnes();
                prevJointDelta.segment(0, entriesPerJoint) = jointMatrixCM.col(i).segment(entriesPerJoint * jointIndex, entriesPerJoint);

                Eigen::Vector<T, 9> newJointDelta;
                newJointDelta.segment(0, 3) = newDeltaTransforms[lod][i][jointIndex].translation();
                Eigen::Matrix<T, 3, 3> deltaRotation = newDeltaTransforms[lod][i][jointIndex].linear();
                deltaRotation.colwise().normalize();
                // there are multiple ways to represent a rotation via euler angles,
                // hence use the angles that are closest to the previous setting
                const std::vector<Eigen::Vector3<T>> rotEulerNewOptions = RotationMatrixToEulerXYZOptions(deltaRotation);
                Eigen::Vector3<T> rotEulerNew = rotEulerNewOptions.front();
                T bestEulerChange = (prevJointDelta.segment(3, 3) - rotEulerNew).norm();
                for (size_t j = 1; j < rotEulerNewOptions.size(); ++j)
                {
                    const T eulerChange = (prevJointDelta.segment(3, 3) - rotEulerNewOptions[j]).norm();
                    if (eulerChange < bestEulerChange)
                    {
                        rotEulerNew = rotEulerNewOptions[j];
                        bestEulerChange = eulerChange;
                    }
                }
                newJointDelta.segment(3, 3) = rotEulerNew;
                newJointDelta[6] = newDeltaTransforms[lod][i][jointIndex].linear().col(0).norm() - T(1);
                newJointDelta[7] = newDeltaTransforms[lod][i][jointIndex].linear().col(1).norm() - T(1);
                newJointDelta[8] = newDeltaTransforms[lod][i][jointIndex].linear().col(2).norm() - T(1);

                Eigen::Vector<T, 9> diffDelta = newJointDelta - prevJointDelta;

                const T scaleX = newJointDelta[6];
                const T scaleY = newJointDelta[7];
                const T scaleZ = newJointDelta[8];
                const Eigen::Vector3<T> deltaTranslation = newDeltaTransforms[lod][i][jointIndex].translation();
                translationUpdated[jointIndex] = translationUpdated[jointIndex] || ((diffDelta.segment(0, 3)).norm() > translationEps);
                for (int k = 0; k < 3; ++k)
                {
                    if (fabs(deltaTranslation[k]) > translationEps)
                    {
                        triplets.push_back(Eigen::Triplet<T>((int)entriesPerJoint * jointIndex + k, (int)i, deltaTranslation[k]));
                    }
                }
                scaleUpdated[jointIndex] = scaleUpdated[jointIndex] || (fabs(scaleX - prevJointDelta[6]) > scaleEps) || (fabs(scaleY - prevJointDelta[7]) > scaleEps) || (fabs(scaleZ - prevJointDelta[8]) > scaleEps);
                const bool hasScaleX = (fabs(scaleX) > scaleEps);
                const bool hasScaleY = (fabs(scaleY) > scaleEps);
                const bool hasScaleZ = (fabs(scaleZ) > scaleEps);
                const bool hasScale = hasScaleX || hasScaleY || hasScaleZ;
                if (hasScale)
                {
                    if ((newBindPoses[jointIndex].linear() - prevBindPoses[jointIndex].linear()).norm() > rotationEps)
                    {
                        CARBON_CRITICAL("change of scale is only supported for joints that keep the same bind pose: {}\n{}\n{}",
                            (newBindPoses[jointIndex].linear() - prevBindPoses[jointIndex].linear()).norm(),
                            newBindPoses[jointIndex].matrix(), prevBindPoses[jointIndex].matrix());
                    }
                }
                const T poseRotationAngle = Eigen::Quaternion<T>::Identity().angularDistance(Eigen::Quaternion<T>(deltaRotation)) * rad2degreeScale<T>();
                const T eulerDifference = (newJointDelta.segment(3, 3) - prevJointDelta.segment(3, 3)).norm() * rad2degreeScale<T>();
                const bool hasRotationChanged = (eulerDifference > rotationEps);
                rotationUpdated[jointIndex] = rotationUpdated[jointIndex] || hasRotationChanged;
                if (poseRotationAngle > rotationEps)
                {
                    // if (hasRotationChanged && m->jointRig2.GetJointNames()[jointIndex] == "FACIAL_R_LipLowerOuter")
                    // {
                    //     LOG_INFO("rotation for joint {} (lod {}, ctrl {}) : {} {} => {} [{}]", jointIndex, lod, i, prevJointDelta.segment(3, 3).transpose() * rad2degreeScale<T>(), rotEulerNew.transpose() *  rad2degreeScale<T>(), eulerDifference, poseRotationAngle);
                    // }
                    for (int k = 0; k < 3; ++k)
                    {
                        if (fabs(rotEulerNew[k]) > rotationEps)
                        {
                            triplets.push_back(Eigen::Triplet<T>(entriesPerJoint * jointIndex + 3 + k, i, rotEulerNew[k]));
                        }
                    }
                }
                if (hasScale && entriesPerJoint > 6)
                {
                    if (hasScaleX)
                    {
                        triplets.push_back(Eigen::Triplet<T>(entriesPerJoint * jointIndex + 6, i, scaleX));
                    }
                    if (hasScaleY)
                    {
                        triplets.push_back(Eigen::Triplet<T>(entriesPerJoint * jointIndex + 7, i, scaleY));
                    }
                    if (hasScaleZ)
                    {
                        triplets.push_back(Eigen::Triplet<T>(entriesPerJoint * jointIndex + 8, i, scaleZ));
                    }
                }
            }
        }

        jointMatrix.setFromTriplets(triplets.begin(), triplets.end());
        rigLogic.SetJointMatrix(lod, jointMatrix);
    }

    for (int jointIndex = 0; jointIndex < m->jointRig2.NumJoints(); ++jointIndex)
    {
        if (translationUpdated[jointIndex] || rotationUpdated[jointIndex] || scaleUpdated[jointIndex])
        {
            LOG_VERBOSE("joint delta update for joint {} - \"{}\": t-{} r-{} s-{}", jointIndex, m->jointRig2.GetJointNames()[jointIndex], translationUpdated[jointIndex], rotationUpdated[jointIndex], scaleUpdated[jointIndex]);
        }
    }
}

template <class T>
const Eigen::Matrix<T, -1, -1>& RigGeometry<T>::GetBlendshapeMatrix(int meshIndex) const
{
    return m->meshData[meshIndex]->blendshapeMatrixDense;
}

template <class T>
const Eigen::VectorXi RigGeometry<T>::GetBlendshapePsdControls(int meshIndex) const
{
    return m->meshData[meshIndex]->blendshapeControlsToMeshBlendshapeControls;
}

template <class T>
const std::vector<int>& RigGeometry<T>::JointIndicesForLOD(int lod) const
{
    if (lod >= (int)m->jointIndicesForLOD.size())
    {
        LOG_ERROR("ask out of bounds");
    }
    return m->jointIndicesForLOD[lod];
}

template <class T>
const JointRig2<T>& RigGeometry<T>::GetJointRig() const
{
    return m->jointRig2;
}

template <class T>
const Mesh<T>& RigGeometry<T>::GetMesh(int meshIndex) const
{
    if (!IsValid(meshIndex))
    {
        CARBON_CRITICAL("mesh index {} is not valid", meshIndex);
    }
    return m->meshData[meshIndex]->mesh;
}

template <class T>
const Mesh<T>& RigGeometry<T>::GetMesh(const std::string& meshName) const
{
    return GetMesh(GetMeshIndex(meshName));
}

template <class T>
void RigGeometry<T>::SetMesh(int meshIndex, const Eigen::Matrix<T, 3, -1>& vertices)
{
    if (!IsValid(meshIndex))
    {
        CARBON_CRITICAL("mesh index {} is not valid", meshIndex);
    }
    if (int(vertices.cols()) != m->meshData[meshIndex]->mesh.NumVertices())
    {
        CARBON_CRITICAL("mesh {} has a different number of vertices: {} vs {}",
                        meshIndex,
                        vertices.cols(),
                        m->meshData[meshIndex]->mesh.NumVertices());
    }
    auto newMeshData = std::make_shared<MeshData<T>>(*m->meshData[meshIndex]);
    newMeshData->mesh.SetVertices(vertices);
    m->meshData[meshIndex] = newMeshData;
}

template <class T>
const std::string& RigGeometry<T>::GetMeshName(int meshIndex) const
{
    if (!IsValid(meshIndex))
    {
        CARBON_CRITICAL("mesh index {} is not valid", meshIndex);
    }
    return m->meshData[meshIndex]->meshName;
}

template <class T>
int RigGeometry<T>::GetMeshIndex(const std::string& meshName) const
{
    for (size_t i = 0; i < m->meshData.size(); ++i)
    {
        if (m->meshData[i] && (m->meshData[i]->meshName == meshName))
        {
            return int(i);
        }
    }
    return -1;
}

template <class T>
bool RigGeometry<T>::IsValid(int meshIndex) const
{
    if ((meshIndex < 0) || (meshIndex >= NumMeshes()))
    {
        return false;
    }
    return bool(m->meshData[meshIndex].get());
}

template <class T>
int RigGeometry<T>::NumMeshes() const
{
    return int(m->meshData.size());
}

template <class T>
const std::vector<int>& RigGeometry<T>::GetMeshIndicesForLOD(int lod) const
{
    return m->meshIndicesForLOD[lod];
}

template <class T>
const std::vector<int>& RigGeometry<T>::GetBlendshapeMeshIndices() const
{
    return m->blendshapeMeshIndices;
}

template <class T>
Mesh<T> RigGeometry<T>::ReadMesh(const dna::Reader* reader, int meshIndex, bool triangulate)
{
    Mesh<T> mesh;

    Eigen::Matrix<T, 3, -1> vertices(3, reader->getVertexPositionCount(std::uint16_t(meshIndex)));
    Eigen::Matrix<T, 2, -1> texcoords(2, reader->getVertexTextureCoordinateCount(std::uint16_t(meshIndex)));

    for (std::uint32_t j = 0; j < reader->getVertexPositionCount(std::uint16_t(meshIndex)); j++)
    {
        vertices(0, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).x;
        vertices(1, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).y;
        vertices(2, j) = reader->getVertexPosition(std::uint16_t(meshIndex), j).z;
    }
    mesh.SetVertices(vertices);

    for (std::uint32_t j = 0; j < reader->getVertexTextureCoordinateCount(std::uint16_t(meshIndex)); j++)
    {
        texcoords(0, j) = reader->getVertexTextureCoordinate(std::uint16_t(meshIndex), j).u;
        // texture coordinates are stored with origin in bottom left corner, but images are stored with origin in top left corner
        // and hence we flip the coordinate here
        texcoords(1, j) = T(1) - reader->getVertexTextureCoordinate(std::uint16_t(meshIndex), j).v;
    }
    mesh.SetTexcoords(texcoords);

    const int numFaces = reader->getFaceCount(std::uint16_t(meshIndex));
    int numQuads = 0;
    int numTris = 0;
    std::map<size_t, int> numOthers;
    for (int faceIndex = 0; faceIndex < numFaces; faceIndex++)
    {
        rl4::ConstArrayView<std::uint32_t> faceLayoutIndices = reader->getFaceVertexLayoutIndices(std::uint16_t(meshIndex),
                                                                                                  faceIndex);
        if (faceLayoutIndices.size() == 3)
        {
            numTris++;
        }
        else if (faceLayoutIndices.size() == 4)
        {
            if (triangulate) numTris += 2;
            else numQuads++;
        }
        else
        {
            if (triangulate) numTris += (int)faceLayoutIndices.size() - 2;
            else numOthers[faceLayoutIndices.size()]++;
        }
    }
    for (const auto& [vertexCount, numFacesWithThatCount] : numOthers)
    {
        LOG_WARNING("mesh {} contains {} faces with {} vertices, but we only support triangles and quads",
                    reader->getMeshName(std::uint16_t(meshIndex)).c_str(),
                    numFacesWithThatCount,
                    vertexCount);
    }
    rl4::ConstArrayView<std::uint32_t> vertexLayoutPositions = reader->getVertexLayoutPositionIndices(std::uint16_t(meshIndex));
    rl4::ConstArrayView<std::uint32_t> texLayoutPositons = reader->getVertexLayoutTextureCoordinateIndices(std::uint16_t(
                                                                                                               meshIndex));
    Eigen::Matrix<int, 4, -1> quads(4, numQuads);
    Eigen::Matrix<int, 3, -1> tris(3, numTris);
    Eigen::Matrix<int, 4, -1> texQuads(4, numQuads);
    Eigen::Matrix<int, 3, -1> texTris(3, numTris);

    int quadsIter = 0;
    int trisIter = 0;
    for (int faceIndex = 0; faceIndex < numFaces; faceIndex++)
    {
        rl4::ConstArrayView<std::uint32_t> faceLayoutIndices = reader->getFaceVertexLayoutIndices(std::uint16_t(meshIndex),
                                                                                                  faceIndex);

        if (triangulate)
        {
            // basic ordered triangulation
            for (int k = 2; k < (int)faceLayoutIndices.size(); ++k)
            {
                tris(0, trisIter) = vertexLayoutPositions[faceLayoutIndices[0]];
                tris(1, trisIter) = vertexLayoutPositions[faceLayoutIndices[k - 1]];
                tris(2, trisIter) = vertexLayoutPositions[faceLayoutIndices[k]];

                texTris(0, trisIter) = texLayoutPositons[faceLayoutIndices[0]];
                texTris(1, trisIter) = texLayoutPositons[faceLayoutIndices[k - 1]];
                texTris(2, trisIter) = texLayoutPositons[faceLayoutIndices[k]];

                trisIter++;
            }
        }
        else
        {
            if (faceLayoutIndices.size() == 3)
            {
                tris(0, trisIter) = vertexLayoutPositions[faceLayoutIndices[0]];
                tris(1, trisIter) = vertexLayoutPositions[faceLayoutIndices[1]];
                tris(2, trisIter) = vertexLayoutPositions[faceLayoutIndices[2]];

                texTris(0, trisIter) = texLayoutPositons[faceLayoutIndices[0]];
                texTris(1, trisIter) = texLayoutPositons[faceLayoutIndices[1]];
                texTris(2, trisIter) = texLayoutPositons[faceLayoutIndices[2]];

                trisIter++;
            }
            else if (faceLayoutIndices.size() == 4)
            {
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
    }
    mesh.SetTriangles(tris);
    mesh.SetQuads(quads);
    mesh.SetTexQuads(texQuads);
    mesh.SetTexTriangles(texTris);

    mesh.Validate(true);

    return mesh;
}

template<class T>
std::uint16_t RigGeometry<T>::NumLODs() const
{
    return static_cast<std::uint16_t>(m->jointIndicesForLOD.size());
}


template <class T>
bool RigGeometry<T>::Init(const dna::Reader* reader, bool withJointScaling, std::map<std::string, ErrorInfo>* errorInfo)
{
    m->withJointScaling = withJointScaling;

    const std::uint16_t numMeshes = reader->getMeshCount();
    m->meshData.resize(numMeshes);
    m->blendshapeMeshIndices.clear();

    std::vector<std::vector<std::pair<std::string, std::vector<int>>>> duplicateBlendshapes(numMeshes);
    std::vector<std::vector<std::pair<std::string, std::vector<int>>>> zeroBlendshapes(numMeshes);
    std::vector<typename JointRig2<T>::ErrorInfo> skinningWeightsErrors(numMeshes);

    m->blendshapeInfo.blendshapeNames = std::vector<std::string>(reader->getBlendShapeChannelCount());
    m->blendshapeInfo.blendshapeInputIndices = std::vector<uint16_t>(reader->getBlendShapeChannelCount());
    m->blendshapeInfo.blendshapeOutputIndices = std::vector<uint16_t>(reader->getBlendShapeChannelCount());
    for (uint16_t channelIndex = 0; channelIndex < reader->getBlendShapeChannelCount(); ++channelIndex)
    {
        m->blendshapeInfo.blendshapeNames[channelIndex] = reader->getBlendShapeChannelName(channelIndex);
        m->blendshapeInfo.blendshapeInputIndices[channelIndex] = reader->getBlendShapeChannelInputIndices()[channelIndex];
        m->blendshapeInfo.blendshapeOutputIndices[channelIndex] = reader->getBlendShapeChannelOutputIndices()[channelIndex];
        const int bsOutputIndex = m->blendshapeInfo.blendshapeOutputIndices[channelIndex];
        if (channelIndex != bsOutputIndex)
        {
            LOG_WARNING("unexpected blendshape channel data: {} vs {}", channelIndex, bsOutputIndex);
        }
    }

    // read mesh and blendshape data
    for (std::uint16_t meshIndex = 0; meshIndex < numMeshes; meshIndex++)
    {
        auto meshDataPtr = std::make_shared<MeshData<T>>();
        m->meshData[meshIndex] = meshDataPtr;
        auto& meshData = *meshDataPtr;

        // read mesh geometry
        meshData.mesh = ReadMesh(reader, meshIndex, /*triangulate=*/false);
        meshData.meshName = reader->getMeshName(meshIndex).c_str();
        meshData.blendshapeChannels.clear();

        const int numVertices = meshData.mesh.NumVertices();

        // read blendshape data and put into sparse matrix
        // const std::uint16_t numTotalBlendshapes = reader->getBlendShapeChannelCount();
        const std::uint16_t numBlendshapeTargets = reader->getBlendShapeTargetCount(meshIndex);
        if (numBlendshapeTargets > std::uint16_t(0))
        {
            m->blendshapeMeshIndices.push_back(meshIndex);
        }
        meshData.blendshapeMatrixDense = Eigen::Matrix<T, -1, -1>::Zero(3 * meshData.mesh.NumVertices(), numBlendshapeTargets);
        meshData.blendshapeControlsToMeshBlendshapeControls.resize(numBlendshapeTargets);
        for (std::uint16_t blendShapeTargetIndex = 0; blendShapeTargetIndex < numBlendshapeTargets; blendShapeTargetIndex++)
        {
            std::vector<int> entryCount(numVertices, 0);
            std::vector<int> zeroCount(numVertices, 0);

            const std::uint16_t channelIndex = reader->getBlendShapeChannelIndex(meshIndex, blendShapeTargetIndex);
            meshData.blendshapeChannels.push_back(channelIndex);
            const std::string& blendshapeName = m->blendshapeInfo.blendshapeNames[channelIndex];
            const int psdIndex = m->blendshapeInfo.blendshapeInputIndices[channelIndex];
            meshData.blendshapeControlsToMeshBlendshapeControls[blendShapeTargetIndex] = static_cast<int>(psdIndex);
            const std::uint32_t numDeltas = reader->getBlendShapeTargetDeltaCount(meshIndex, blendShapeTargetIndex);
            rl4::ConstArrayView<std::uint32_t> vertexIndices = reader->getBlendShapeTargetVertexIndices(meshIndex, blendShapeTargetIndex);
            for (int deltaIndex = 0; deltaIndex < int(numDeltas); deltaIndex++)
            {
                const dna::Delta delta = reader->getBlendShapeTargetDelta(meshIndex, blendShapeTargetIndex, deltaIndex);
                const int vID = (int)vertexIndices[deltaIndex];
                if (Eigen::Map<const Eigen::Vector3f>(&delta.x).norm() == 0)
                {
                    zeroCount[vID]++;
                }
                entryCount[vID]++;
                meshData.blendshapeMatrixDense(3 * vertexIndices[deltaIndex] + 0, blendShapeTargetIndex) += delta.x;
                meshData.blendshapeMatrixDense(3 * vertexIndices[deltaIndex] + 1, blendShapeTargetIndex) += delta.y;
                meshData.blendshapeMatrixDense(3 * vertexIndices[deltaIndex] + 2, blendShapeTargetIndex) += delta.z;
            }
            if (meshData.blendshapeMatrixDense.col(blendShapeTargetIndex).norm() == T(0) && numDeltas > 0)
            {
                LOG_WARNING("blendshape {} ({}, psd {}) does not have any data, but {} deltas",
                            blendshapeName,
                            channelIndex,
                            psdIndex,
                            numDeltas);
            }
            // LOG_INFO("mesh {} - blendshape {} ({}, psd {})", meshIndex, reader->getBlendShapeChannelName(channelIndex).c_str(),
            // channelIndex, psdIndex);
            std::vector<int> duplicateVertices;
            std::vector<int> zeroVertices;
            for (int vID = 0; vID < meshData.mesh.NumVertices(); ++vID)
            {
                if (zeroCount[vID] > 0) zeroVertices.push_back(vID);
                if (entryCount[vID] > 1) duplicateVertices.push_back(vID);
            }
            if (duplicateVertices.size() > 0)
            {
                duplicateBlendshapes[meshIndex].push_back({blendshapeName, duplicateVertices});
                LOG_WARNING("mesh {} and blendshape {} has {} duplicate deltas", meshData.meshName, blendshapeName, duplicateVertices.size());
            }
            if (zeroVertices.size() > 0)
            {
                zeroBlendshapes[meshIndex].push_back({blendshapeName, zeroVertices});
                // log this as verbose rather than as a warning as it is flagged in the UE automation tests, and warning are treated as errors
                LOG_VERBOSE("mesh {} and blendshape {} has {} zero deltas", meshData.meshName, blendshapeName, zeroVertices.size());
            }
        }
        meshData.blendshapeMatrixDenseRM = meshData.blendshapeMatrixDense;
    }

    const std::uint16_t numLODs = reader->getLODCount();

    m->meshIndicesForLOD.clear();
    m->meshIndicesForLOD.resize(numLODs);
    for (std::uint16_t lod = 0; lod < numLODs; lod++)
    {
        rl4::ConstArrayView<std::uint16_t> meshIndicesForLOD = reader->getMeshIndicesForLOD(lod);
        for (std::uint16_t meshIndex : meshIndicesForLOD)
        {
            m->meshIndicesForLOD[lod].push_back(meshIndex);
        }
    }

    // read joints data
    const std::uint16_t numJoints = reader->getJointCount();
    m->jointRig2.Clear();

    if (numJoints > 0)
    {
        for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
        {
            const std::string jointName = reader->getJointName(jointIndex).c_str();
            m->jointRig2.AddJoint(jointName);
        }

        // setup hierarchy
        for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
        {
            std::uint16_t parentIndex = reader->getJointParentIndex(jointIndex);
            if (parentIndex != jointIndex)
            {
                const std::string childJointName = reader->getJointName(jointIndex).c_str();
                const std::string parentJointName = reader->getJointName(parentIndex).c_str();
                m->jointRig2.AttachJointToParent(childJointName, parentJointName);
            }
        }

        m->jointIndicesPerHierarchyLevel = m->jointRig2.GetJointsPerHierarchyLevel();

        m->jointRestPose = Eigen::Matrix<T, 3, -1>(3, numJoints);
        m->jointRestOrientationEuler = Eigen::Matrix<T, 3, -1>(3, numJoints);
        m->jointRestOrientation.resize(numJoints);
        for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
        {
            rl4::Vector3 t = reader->getNeutralJointTranslation(jointIndex);
            rl4::Vector3 rot = reader->getNeutralJointRotation(jointIndex);
            m->jointRestPose(0, jointIndex) = t.x;
            m->jointRestPose(1, jointIndex) = t.y;
            m->jointRestPose(2, jointIndex) = t.z;
            m->jointRestOrientationEuler(0, jointIndex) = (T)rot.x;
            m->jointRestOrientationEuler(1, jointIndex) = (T)rot.y;
            m->jointRestOrientationEuler(2, jointIndex) = (T)rot.z;
            m->jointRestOrientation[jointIndex] = EulerXYZ<T>(m->jointRestOrientationEuler.col(jointIndex) * degree2radScale<T>());
        }

        UpdateBindPoses();

        // get the joints that are nedded for an lod
        m->jointIndicesForLOD.clear();
        m->jointIndicesForLOD.resize(numLODs);
        for (std::uint16_t lod = 0; lod < numLODs; lod++)
        {
            const rl4::ConstArrayView<std::uint16_t> jointIndices = reader->getJointIndicesForLOD(lod);
            std::vector<bool> used(m->jointRig2.NumJoints(), false);
            for (const std::uint16_t& jointIndex : jointIndices)
            {
                used[jointIndex] = true;
                // add parents
                int currIdx = m->jointRig2.GetParentIndex(jointIndex);
                while (currIdx >= 0)
                {
                    used[currIdx] = true;
                    currIdx = m->jointRig2.GetParentIndex(currIdx);
                }
            }
            for (int i = 0; i < m->jointRig2.NumJoints(); ++i)
            {
                if (used[i])
                {
                    m->jointIndicesForLOD[lod].push_back(i);
                }
            }
        }

        // setup skinning weights
        for (std::uint16_t meshIndex = 0; meshIndex < numMeshes; meshIndex++)
        {
            std::vector<std::vector<std::pair<int, T>>> jointInfluences(numJoints);
            for (int vertexIndex = 0; vertexIndex < m->meshData[meshIndex]->mesh.NumVertices(); vertexIndex++)
            {
                rl4::ConstArrayView<float> influenceWeights = reader->getSkinWeightsValues(meshIndex, vertexIndex);
                rl4::ConstArrayView<std::uint16_t> jointIndices = reader->getSkinWeightsJointIndices(meshIndex, vertexIndex);
                for (int k = 0; k < int(influenceWeights.size()); k++)
                {
                    jointInfluences[jointIndices[k]].push_back(std::pair<int, T>(vertexIndex, influenceWeights[k]));
                }
            }
            std::map<std::string, InfluenceWeights<T>> allInfluenceWeights;
            for (int jointIndex = 0; jointIndex < numJoints; jointIndex++)
            {
                InfluenceWeights<T> influenceWeights;
                const int numInfluences = int(jointInfluences[jointIndex].size());
                if (numInfluences > 0)
                {
                    influenceWeights.indices.resize(numInfluences);
                    influenceWeights.weights.resize(numInfluences);
                    for (int k = 0; k < numInfluences; k++)
                    {
                        influenceWeights.indices[k] = jointInfluences[jointIndex][k].first;
                        influenceWeights.weights[k] = jointInfluences[jointIndex][k].second;
                    }
                    const std::string jointName = m->jointRig2.GetJointNames()[jointIndex];
                    allInfluenceWeights[jointName] = influenceWeights;
                }
            }
            const std::string& meshName = m->meshData[meshIndex]->meshName; // std::to_string(meshIndex)
            m->jointRig2.SetSkinningWeights(meshName,
                                            allInfluenceWeights,
                                            m->meshData[meshIndex]->mesh.NumVertices(),
                                            /*normalizeWeights=*/true,
                                            /*allowNegativeWeights=*/false,
                                            (errorInfo ? &skinningWeightsErrors[meshIndex] : nullptr));
        }

        // validate the rig
        m->jointRig2.CheckValidity();
    }

    if (errorInfo)
    {
        errorInfo->clear();
        for (int meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            ErrorInfo info;
            info.skinningWeightsErrors = std::move(skinningWeightsErrors[meshIndex]);
            info.duplicateBlendshapes = std::move(duplicateBlendshapes[meshIndex]);
            info.zeroBlendshapes = std::move(zeroBlendshapes[meshIndex]);

            std::vector<int> unskinnedVertices;
        std::vector<int> unnormalizedVertices;
        std::vector<int> negativeWeightsVertices;

            if (info.duplicateBlendshapes.size() > 0
                || info.zeroBlendshapes.size() > 0
                || info.skinningWeightsErrors.unskinnedVertices.size() > 0
                || info.skinningWeightsErrors.unnormalizedVertices.size() > 0
                || info.skinningWeightsErrors.negativeWeightsVertices.size() > 0)
            {
                (*errorInfo).insert({GetMeshName(meshIndex), std::move(info)});
            }
        }
    }

    return true;
}

template <class T>
void RigGeometry<T>::UpdateBindPoses()
{
    // temporary state to calculate the bind poses
    State state;

    const int numJoints = m->jointRig2.NumJoints();

    m->jointRig2StateBindPoses.resize(numJoints);
    m->jointRig2StateInverseBindPoses.resize(numJoints);

    state.m_localMatrices.resize(numJoints);
    state.m_worldMatrices.resize(numJoints);

    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        state.m_localMatrices[jointIndex].linear() = m->jointRestOrientation[jointIndex];
        state.m_localMatrices[jointIndex].translation() = m->jointRestPose.col(jointIndex);
    }

    for (size_t hierarchyLevel = 0; hierarchyLevel < m->jointIndicesPerHierarchyLevel.size(); ++hierarchyLevel)
    {
        const std::vector<int>& jointsPerLevel = m->jointIndicesPerHierarchyLevel[hierarchyLevel];
        for (int jointIndex : jointsPerLevel)
        {
            const int parentIndex = m->jointRig2.GetParentIndex(jointIndex);
            if (parentIndex >= 0)
            {
                state.m_worldMatrices[jointIndex] = state.m_worldMatrices[parentIndex] * state.m_localMatrices[jointIndex];
            }
            else
            {
                state.m_worldMatrices[jointIndex] = state.m_localMatrices[jointIndex];
            }
            m->jointRig2StateBindPoses[jointIndex] = state.m_worldMatrices[jointIndex];
            m->jointRig2StateInverseBindPoses[jointIndex] = m->jointRig2StateBindPoses[jointIndex].inverse();
        }
    }
}

// jacobian calculation for out = aff1 * aff2 where both aff1 and aff2 have a jacobian
template <class T>
void AffineJacobianMultiply(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> outJacobian,
                            const Eigen::Matrix<T, 4, 4>& aff1,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> jac1,
                            const Eigen::Matrix<T, 4, 4>& aff2,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1,
                                                           Eigen::RowMajor>> jac2)
{
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            // out(r, c) = aff1.row(r) * aff2.col(c)
            outJacobian.row(3 * c + r) = aff1.matrix()(r, 0) * jac2.row(3 * c + 0) + aff2.matrix()(0, c) * jac1.row(3 * 0 + r);
            for (int k = 1; k < 3; ++k)
            {
                outJacobian.row(3 * c + r) += aff1(r, k) * jac2.row(3 * c + k);
                outJacobian.row(3 * c + r) += aff2(k, c) * jac1.row(3 * k + r);
            }
        }
    }
    for (int r = 0; r < 3; ++r)
    {
        outJacobian.row(9 + r) = jac1.row(9 + r);
        for (int k = 0; k < 3; ++k)
        {
            outJacobian.row(9 + r) += aff1(r, k) * jac2.row(9 + k);
            outJacobian.row(9 + r) += aff2(k, 3) * jac1.row(3 * k + r);
        }
    }
}

// jacobian calculation for out = aff1 * aff2 where only aff1 has a jacobian
template <class T>
void AffineJacobianMultiply(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> outJacobian,
                            const Eigen::Matrix<T, 4, 4>&/*aff1*/,
                            Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> jac1,
                            const Eigen::Matrix<T, 4,
                                                4>& aff2)
{
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            // out(r, c) = aff1.row(r) * aff2.col(c)
            outJacobian.row(3 * c + r) = aff2.matrix()(0, c) * jac1.row(3 * 0 + r);
            for (int k = 1; k < 3; ++k)
            {
                outJacobian.row(3 * c + r) += aff2(k, c) * jac1.row(3 * k + r);
            }
        }
    }
    for (int r = 0; r < 3; ++r)
    {
        outJacobian.row(9 + r) = jac1.row(9 + r);
        for (int k = 0; k < 3; ++k)
        {
            outJacobian.row(9 + r) += aff2(k, 3) * jac1.row(3 * k + r);
        }
    }
}

template <class T>
void RigGeometry<T>::EvaluateJointDeltas(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, State& state) const
{
    // Timer timer;
    Eigen::Ref<const Eigen::VectorX<T>> jointState = diffJoints.Value();

    state.m_withJacobians = true;

    const int numJoints = m->jointRig2.NumJoints();
    state.m_localMatrices.resize(numJoints);
    state.m_worldMatrices.resize(numJoints);
    state.m_skinningMatrices.resize(numJoints);
    // LOG_INFO("evaluate joint deltas - setup: {}", timer.Current()); timer.Restart();

    int startCol = std::numeric_limits<int>::max();
    int endCol = 0;
    auto getColumnBounds = [&](const DiffData<T>& diffData) {
            if (diffData.HasJacobian())
            {
                startCol = std::min<int>(startCol, diffData.Jacobian().StartCol());
                endCol = std::max<int>(endCol, diffData.Jacobian().Cols());
            }
        };
    getColumnBounds(diffRigid.Linear());
    getColumnBounds(diffRigid.Translation());
    getColumnBounds(diffJoints);

    state.m_jointDeltasJacobian.resize(diffJoints.Size(), endCol - startCol);
    state.m_jointDeltasJacobian.setZero();
    if (diffJoints.HasJacobian())
    {
        diffJoints.Jacobian().CopyToDenseMatrix(state.m_jointDeltasJacobian.block(0, diffJoints.Jacobian().StartCol() - startCol,
                                                                                  diffJoints.Size(),
                                                                                  diffJoints.Jacobian().Cols() -
                                                                                  diffJoints.Jacobian().StartCol()));
    }
    state.m_localMatricesJacobian.resize(numJoints * 12 + 12, state.m_jointDeltasJacobian.cols());
    state.m_worldMatricesJacobian.resize(numJoints * 12, state.m_jointDeltasJacobian.cols());
    state.m_skinningMatricesJacobian.resize(numJoints * 12, state.m_jointDeltasJacobian.cols());

    // LOG_INFO("evaluate joint deltas - local matrix (setup): {}", timer.Current()); timer.Restart();

    {
        auto updateLocalMatricesNoScale = [&](int start, int end) {
                constexpr int dofPerJoint = 6;

                Eigen::Matrix<T, 12, dofPerJoint,
                              Eigen::RowMajor> dmat = Eigen::Matrix<T, 12, dofPerJoint, Eigen::RowMajor>::Zero(12, dofPerJoint);

                for (int jointIndex = start; jointIndex < end; ++jointIndex)
                {
                    const T& drx = jointState[dofPerJoint * jointIndex + 3];
                    const T& dry = jointState[dofPerJoint * jointIndex + 4];
                    const T& drz = jointState[dofPerJoint * jointIndex + 5];

                    state.m_localMatrices[jointIndex].linear() = m->jointRestOrientation[jointIndex] * EulerXYZ(drx, dry, drz);
                    state.m_localMatrices[jointIndex].translation() =
                        jointState.segment(dofPerJoint * jointIndex + 0, 3) + m->jointRestPose.col(jointIndex);

                    if (diffJoints.HasJacobian())
                    {
                        // gather jacobian of drx, dry, drz and combine with euler jacobian
                        auto jacobianOfPreMultiply =
                            JacobianOfPremultipliedMatrixDense<T, 3, 3, 3>(m->jointRestOrientation[jointIndex]);
                        auto eulerJacobian = EulerXYZJacobianDense<T>(drx, dry, drz);

                        dmat.block(0, 3, 9, 3) = jacobianOfPreMultiply * eulerJacobian;

                        // translation jacobian is just the copy of the respective rows of the joint jacobians
                        dmat(9, 0) = T(1);
                        dmat(10, 1) = T(1);
                        dmat(11, 2) = T(1);
                    }

                    state.m_localMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                        state.m_jointDeltasJacobian.cols()) = dmat *
                        state.m_jointDeltasJacobian.block(
                        jointIndex * dofPerJoint,
                        0,
                        dofPerJoint,
                        state.m_jointDeltasJacobian.cols());
                }
            };

        auto updateLocalMatrices = [&](int start, int end) {
                constexpr int dofPerJoint = 9;

                Eigen::Matrix<T, 12, dofPerJoint,
                              Eigen::RowMajor> dmat = Eigen::Matrix<T, 12, dofPerJoint, Eigen::RowMajor>::Zero(12, dofPerJoint);

                for (int jointIndex = start; jointIndex < end; ++jointIndex)
                {
                    const T& drx = jointState[dofPerJoint * jointIndex + 3];
                    const T& dry = jointState[dofPerJoint * jointIndex + 4];
                    const T& drz = jointState[dofPerJoint * jointIndex + 5];

                    const T& dsx = jointState[dofPerJoint * jointIndex + 6];
                    const T& dsy = jointState[dofPerJoint * jointIndex + 7];
                    const T& dsz = jointState[dofPerJoint * jointIndex + 8];

                    state.m_localMatrices[jointIndex].linear() = m->jointRestOrientation[jointIndex] * EulerXYZAndScale(drx,
                                                                                                                        dry,
                                                                                                                        drz,
                                                                                                                        T(
                                                                                                                            1) + dsx,
                                                                                                                        T(
                                                                                                                            1) + dsy,
                                                                                                                        T(1) +
                                                                                                                        dsz);
                    state.m_localMatrices[jointIndex].translation() =
                        jointState.segment(dofPerJoint * jointIndex + 0, 3) + m->jointRestPose.col(jointIndex);

                    if (diffJoints.HasJacobian())
                    {
                        // gather jacobian of drx, dry, drz, dsx, dsy, dsz, and combine with euler jacobian and scale jacobian
                        auto jacobianOfPreMultiply =
                            JacobianOfPremultipliedMatrixDense<T, 3, 3, 3>(m->jointRestOrientation[jointIndex]);
                        auto eulerAndScaleJacobian = EulerXYZAndScaleJacobianDense<T>(drx,
                                                                                      dry,
                                                                                      drz,
                                                                                      T(1) + dsx,
                                                                                      T(1) + dsy,
                                                                                      T(1) + dsz);
                        dmat.block(0, 3, 9, 6) = jacobianOfPreMultiply * eulerAndScaleJacobian;

                        // translation jacobian is just the copy of the respective rows of the joint jacobians
                        dmat(9, 0) = T(1);
                        dmat(10, 1) = T(1);
                        dmat(11, 2) = T(1);
                    }

                    state.m_localMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                        state.m_jointDeltasJacobian.cols()) = dmat *
                        state.m_jointDeltasJacobian.block(
                        jointIndex * dofPerJoint,
                        0,
                        dofPerJoint,
                        state.m_jointDeltasJacobian.cols());
                }
            };

        const int numTasks = int(m->jointRestPose.cols());
        if (diffJoints.HasJacobian() && (state.m_localMatricesJacobian.size() > 1000) && m->taskThreadPool)
        {
            if (m->withJointScaling)
            {
                m->taskThreadPool->AddTaskRangeAndWait(numTasks, updateLocalMatrices);
            }
            else
            {
                m->taskThreadPool->AddTaskRangeAndWait(numTasks, updateLocalMatricesNoScale);
            }
        }
        else
        {
            if (m->withJointScaling)
            {
                updateLocalMatrices(0, numTasks);
            }
            else
            {
                updateLocalMatricesNoScale(0, numTasks);
            }
        }

        state.m_localMatricesJacobian.block(numJoints * 12, 0, 12, state.m_localMatricesJacobian.cols()).setZero();
        // copy jacobian of rigid to local
        if (diffRigid.Linear().HasJacobian())
        {
            // copy jacobian of rigid to local
            diffRigid.Linear().Jacobian().CopyToDenseMatrix(state.m_localMatricesJacobian.block(numJoints * 12,
                                                                                                diffRigid.Linear().Jacobian().
                                                                                                StartCol() - startCol, 9,
                                                                                                diffRigid.Linear().Jacobian().Cols()
                                                                                                -
                                                                                                diffRigid.Linear().Jacobian().
                                                                                                StartCol()));
        }
        if (diffRigid.Translation().HasJacobian())
        {
            // copy jacobian of rigid to local
            diffRigid.Translation().Jacobian().CopyToDenseMatrix(state.m_localMatricesJacobian.block(numJoints * 12 + 9,
                                                                                                     diffRigid.Translation().
                                                                                                     Jacobian().StartCol() -
                                                                                                     startCol,
                                                                                                     3,
                                                                                                     diffRigid.Translation().
                                                                                                     Jacobian().Cols() -
                                                                                                     diffRigid.Translation().
                                                                                                     Jacobian().StartCol()));
        }
    }
    // LOG_INFO("evaluate joint deltas - local matrix (dense): {}", timer.Current()); timer.Restart();

    // update world and skinning matrices
    for (size_t hierarchyLevel = 0; hierarchyLevel < m->jointIndicesPerHierarchyLevel.size(); ++hierarchyLevel)
    {
        const std::vector<int>& jointsPerLevel = m->jointIndicesPerHierarchyLevel[hierarchyLevel];
        const int numTasks = static_cast<int>(jointsPerLevel.size());
        auto updateWorldAndSkinningMatrices = [&](int start, int end) {
                for (int taskId = start; taskId < end; ++taskId)
                {
                    const int jointIndex = jointsPerLevel[taskId];
                    const int parentIndex = m->jointRig2.GetParentIndex(jointIndex);
                    if (parentIndex >= 0)
                    {
                        state.m_worldMatrices[jointIndex] = state.m_worldMatrices[parentIndex] *
                            state.m_localMatrices[jointIndex];
                        // dense jacobian multiply
                        AffineJacobianMultiply<T>(state.m_worldMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()),
                                                  state.m_worldMatrices[parentIndex].matrix(),
                                                  state.m_worldMatricesJacobian.block(parentIndex * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()),
                                                  state.m_localMatrices[jointIndex].matrix(),
                                                  state.m_localMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()));
                    }
                    else
                    {
                        // root node
                        state.m_worldMatrices[jointIndex] =
                            Eigen::Transform<T, 3, Eigen::Affine>(diffRigid.Matrix()) * state.m_localMatrices[jointIndex];
                        // dense jacobian multiply
                        AffineJacobianMultiply<T>(state.m_worldMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()),
                                                  diffRigid.Matrix(),
                                                  state.m_localMatricesJacobian.block(numJoints * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()),
                                                  state.m_localMatrices[jointIndex].matrix(),
                                                  state.m_localMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                      state.m_localMatricesJacobian.cols()));
                    }
                }
            };
        if (diffJoints.HasJacobian() && (state.m_localMatricesJacobian.size() > 1000) && m->taskThreadPool)
        {
            m->taskThreadPool->AddTaskRangeAndWait(numTasks, updateWorldAndSkinningMatrices);
        }
        else
        {
            updateWorldAndSkinningMatrices(0, numTasks);
        }
    }
    // LOG_INFO("evaluate joint deltas - world matrix: {}", timer.Current()); timer.Restart();

    {
        const int numTasks = static_cast<int>(state.m_worldMatrices.size());
        auto updateWorldAndSkinningMatrices = [&](int start, int end) {
                for (int taskId = start; taskId < end; ++taskId)
                {
                    const int jointIndex = taskId;
                    state.m_skinningMatrices[jointIndex] = state.m_worldMatrices[jointIndex] *
                        m->jointRig2StateInverseBindPoses[jointIndex];
                    // dense jacobian multiply
                    AffineJacobianMultiply<T>(state.m_skinningMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                     state.m_localMatricesJacobian.cols()),
                                              state.m_worldMatrices[jointIndex].matrix(),
                                              state.m_worldMatricesJacobian.block(jointIndex * 12, 0, 12,
                                                                                  state.m_localMatricesJacobian.cols()),
                                              m->jointRig2StateInverseBindPoses[jointIndex].matrix());
                }
            };
        if (diffJoints.HasJacobian() && (state.m_localMatricesJacobian.size() > 1000) && m->taskThreadPool)
        {
            m->taskThreadPool->AddTaskRangeAndWait(numTasks, updateWorldAndSkinningMatrices);
        }
        else
        {
            updateWorldAndSkinningMatrices(0, numTasks);
        }
    }

    // LOG_INFO("evaluate joint deltas - skinning matrix: {}", timer.Current()); timer.Restart();

    if (diffRigid.HasJacobian() || diffJoints.HasJacobian())
    {
        state.m_jointJacobianColOffset = startCol;
    }
    else
    {
        state.m_jointJacobianColOffset = -1;
    }
}

template <class T>
void RigGeometry<T>::EvaluateJointDeltasWithoutJacobians(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, State& state) const
{
    Eigen::Ref<const Eigen::VectorX<T>> jointState = diffJoints.Value();

    const int numJoints = m->jointRig2.NumJoints();
    state.m_withJacobians = false;
    state.m_localMatrices.resize(numJoints);
    state.m_worldMatrices.resize(numJoints);
    state.m_skinningMatrices.resize(numJoints);
    state.m_jointJacobianColOffset = -1;

    // calculate local matrices
    const int dofPerJoint = m->withJointScaling ? 9 : 6;
    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex)
    {
        const T& drx = jointState[dofPerJoint * jointIndex + 3];
        const T& dry = jointState[dofPerJoint * jointIndex + 4];
        const T& drz = jointState[dofPerJoint * jointIndex + 5];

        Eigen::Matrix<T, 3, 3> R;

        if (m->withJointScaling && (jointState.segment(dofPerJoint * jointIndex + 6, 3).squaredNorm() > 0))
        {
            const T& dsx = jointState[dofPerJoint * jointIndex + 6];
            const T& dsy = jointState[dofPerJoint * jointIndex + 7];
            const T& dsz = jointState[dofPerJoint * jointIndex + 8];

            R = m->jointRestOrientation[jointIndex] * EulerXYZAndScale(drx, dry, drz, T(1) + dsx, T(1) + dsy, T(1) + dsz);
        }
        else
        {
            R = m->jointRestOrientation[jointIndex] * EulerXYZ(drx, dry, drz);
        }

        state.m_localMatrices[jointIndex].linear() = R;
        state.m_localMatrices[jointIndex].translation() =
            jointState.segment(dofPerJoint * jointIndex + 0, 3) + m->jointRestPose.col(jointIndex);
    }

    // update world matrices
    for (size_t hierarchyLevel = 0; hierarchyLevel < m->jointIndicesPerHierarchyLevel.size(); ++hierarchyLevel)
    {
        const std::vector<int>& jointsPerLevel = m->jointIndicesPerHierarchyLevel[hierarchyLevel];
        for (const int jointIndex : jointsPerLevel)
        {
            const int parentIndex = m->jointRig2.GetParentIndex(jointIndex);
            if (parentIndex >= 0)
            {
                state.m_worldMatrices[jointIndex] = state.m_worldMatrices[parentIndex] * state.m_localMatrices[jointIndex];
            }
            else
            {
                // root node
                state.m_worldMatrices[jointIndex] =
                    Eigen::Transform<T, 3, Eigen::Affine>(diffRigid.Matrix()) * state.m_localMatrices[jointIndex];
            }
        }
    }

    // update skinning matrices
    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex)
    {
        state.m_skinningMatrices[jointIndex] = state.m_worldMatrices[jointIndex] * m->jointRig2StateInverseBindPoses[jointIndex];
    }
}

template <class T, int R, int C>
Eigen::Ref<Eigen::Vector<T, -1>> Flatten(Eigen::Matrix<T, R, C>& matrix) {
    return Eigen::Map<Eigen::Vector<T, -1>>(matrix.data(), matrix.rows() * matrix.cols());
}

template <class T, int R, int C>
Eigen::Ref<const Eigen::Vector<T, -1>> Flatten(const Eigen::Matrix<T, R, C>& matrix) {
    return Eigen::Map<const Eigen::Vector<T, -1>>(matrix.data(), matrix.rows() * matrix.cols());
}

template <class T>
void RigGeometry<T>::EvaluateBlendshapes(const DiffData<T>& diffPsd, int meshIndex, State& state) const
{
    state.SetupForMesh(meshIndex);
    auto& meshData = *m->meshData[meshIndex];

    // copy neutral
    state.m_meshJacobianData[meshIndex].blendshapeVertices = meshData.mesh.Vertices();
    state.m_meshJacobianData[meshIndex].blendshapeJacobianColOffset = -1;

    // no blendshapes, then return neutral
    if (meshData.blendshapeControlsToMeshBlendshapeControls.size() == 0)
    {
        return;
    }

    auto blendshapeVerticesFlattened = Flatten(state.m_meshJacobianData[meshIndex].blendshapeVertices);

    // get the blendshape activations for this mesh
    const DiffData<T> diffMeshBlendshapes =
        GatherFunction<T>::Gather(diffPsd, meshData.blendshapeControlsToMeshBlendshapeControls);

    // evaluate blendshapes
    #ifdef EIGEN_USE_BLAS
    blendshapeVerticesFlattened.noalias() += meshData.blendshapeMatrixDenseRM * diffMeshBlendshapes.Value();
    #else
    // if we don't use MKL, then for large matrices (head) we parallelize the matrix vector product
    const int numVertices = int(state.m_meshJacobianData[meshIndex].blendshapeVertices.cols());
    if ((meshData.blendshapeMatrixDenseRM.size() > 30000) && m->taskThreadPool)       // JaneH this code path causes an assertion
                                                                                      // in the UE TaskGraph for DNAs including
                                                                                      // blendshapes so have raised the threshold
                                                                                      // to a value which does not
                                                                                      // trigger this for now
    {
        auto parallelMatrixMultiply = [&](int start, int end)
            {
                blendshapeVerticesFlattened.segment(start, end - start).noalias() += meshData.blendshapeMatrixDenseRM.block(
                    start,
                    0,
                    end - start,
                    diffMeshBlendshapes.Size())
                    *
                    diffMeshBlendshapes.Value();
            };
        m->taskThreadPool->AddTaskRangeAndWait(3 * numVertices, parallelMatrixMultiply);
    }
    else
    {
        blendshapeVerticesFlattened += meshData.blendshapeMatrixDenseRM * diffMeshBlendshapes.Value();
    }
    #endif
    // LOG_INFO("time for blendshape evaluation {}: {}", meshData.meshName, timer.Current()); timer.Restart();

    if (diffMeshBlendshapes.HasJacobian())
    {
        const int blendshapeJacobianColOffset = diffMeshBlendshapes.Jacobian().StartCol();
        SparseMatrix<T> diffBlendshapesSparseMatrixTransposed = diffMeshBlendshapes.Jacobian().AsSparseMatrix()->transpose();
        Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& jacobianMatrix = *(state.m_meshJacobianData[meshIndex].blendshapeJacobianRM);
        jacobianMatrix.resize(meshData.blendshapeMatrixDenseRM.rows(),
                              diffMeshBlendshapes.Jacobian().Cols() - diffMeshBlendshapes.Jacobian().StartCol());

        auto calculate_dVertex_dBlendshapes_rm = [&](int start,
                                                     int end) {
                for (int r = start; r < end; ++r)
                {
                    for (int ctrl = blendshapeJacobianColOffset; ctrl < int(diffBlendshapesSparseMatrixTransposed.rows());
                         ++ctrl)
                    {
                        T acc = 0;
                        for (typename SparseMatrix<T>::InnerIterator it(diffBlendshapesSparseMatrixTransposed, ctrl); it; ++it)
                        {
                            acc += it.value() * meshData.blendshapeMatrixDenseRM(r, it.col());
                        }
                        jacobianMatrix(r, ctrl - blendshapeJacobianColOffset) = acc;
                    }
                }
            };
        if ((jacobianMatrix.size() > 10000) && m->taskThreadPool) // TODO: figure out a good magic number
        {
            m->taskThreadPool->AddTaskRangeAndWait(int(state.m_meshJacobianData[meshIndex].blendshapeVertices.size()),
                                                   calculate_dVertex_dBlendshapes_rm);
        }
        else
        {
            calculate_dVertex_dBlendshapes_rm(0, int(state.m_meshJacobianData[meshIndex].blendshapeVertices.size()));
        }

        state.m_meshJacobianData[meshIndex].blendshapeJacobianColOffset = blendshapeJacobianColOffset;
    }
}

template <class T>
DiffDataMatrix<T, 3, -1> CreateDiffDataMatrix(const Eigen::Matrix<T, 3, -1>& matrix,
                                              const std::shared_ptr<Eigen::Matrix<T, -1,
                                                                                  -1,
                                                                                  Eigen::RowMajor>>& denseJacobian,
                                              int colOffset)
{
    if (denseJacobian && (denseJacobian->size() > 0))
    {
        return DiffDataMatrix<T, 3, -1>(matrix, std::make_shared<DenseJacobian<T>>(denseJacobian, colOffset));
    }
    else
    {
        return DiffDataMatrix<T, 3, -1>(matrix);
    }
}

template <class T>
typename RigGeometry<T>::State& RigGeometry<T>::EvaluateRigGeometry(const DiffDataAffine<T, 3, 3>& diffRigid,
                                                                    const DiffData<T>& diffJoints,
                                                                    const DiffData<T>& diffPsd,
                                                                    const std::vector<int>& meshIndices,
                                                                    State& state) const
{
    const bool requiresJacobians = diffRigid.HasJacobian() || diffJoints.HasJacobian() || diffPsd.HasJacobian();

    const bool hasRigid = diffRigid.HasJacobian() ||
        ((diffRigid.Matrix() - Eigen::Matrix<T, 4, 4>::Identity()).norm() > T(1e-10));

    // Timer timer;
    for (const int meshIndex : meshIndices)
    {
        if ((meshIndex >= 0) && m->meshData[meshIndex])
        {
            EvaluateBlendshapes(diffPsd, meshIndex, state);
            // if (requiresJacobians) LOG_INFO("time for blendshapes for mesh {}: {} (with jacobian: {})", meshIndex,
            // timer.Current(), requiresJacobians ?
            // "true" : "false"); timer.Restart();
        }
    }

    std::vector<DiffDataMatrix<T, 3, -1>> deformedVertices;

    if (m->jointRig2.NumJoints() > 0)
    {
        if (requiresJacobians)
        {
            EvaluateJointDeltas(diffRigid, diffJoints, state);
        }
        else
        {
            EvaluateJointDeltasWithoutJacobians(diffRigid, diffJoints, state);
        }
        // if (requiresJacobians) LOG_INFO("time to evaluate joints: {} (with jacobian: {})", timer.Current(), requiresJacobians ?
        // "true" : "false");
        // timer.Restart();
    }
    else
    {
        if (diffJoints.Size() > 0)
        {
            LOG_WARNING("RigGeometry does not contain joints, but RigGeometry is called with deltas on joints");
        }
    }

    for (size_t i = 0; i < meshIndices.size(); ++i)
    {
        const int meshIndex = meshIndices[i];
        if ((meshIndex >= 0) && m->meshData[meshIndex])
        {
            const auto& meshData = *m->meshData[meshIndex];
            const auto& jacData = state.m_meshJacobianData[meshIndex];
            if (m->jointRig2.HasSkinningWeights(meshData.meshName))
            {
                if (requiresJacobians)
                {
                    EvaluateSkinningWithJacobians(meshIndex, state);
                }
                else
                {
                    EvaluateSkinningWithoutJacobians(meshIndex, state);
                }
                // if (requiresJacobians) LOG_INFO("time to evaluate skinning: {} (with jacobian: {})", timer.Current(),
                // requiresJacobians ? "true" : "false");
                // timer.Restart();
                if (state.m_meshJacobianData[meshIndices[i]].finalJacobianColOffset >= 0)
                {
                    deformedVertices.emplace_back(CreateDiffDataMatrix(jacData.finalVertices, jacData.finalJacobianRM,
                                                                       jacData.finalJacobianColOffset));
                }
                else
                {
                    deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(jacData.finalVertices));
                }
                // if (requiresJacobians) LOG_INFO("time to create matrix: {} (with jacobian: {}) (mesh index: {}, num vertices:
                // {})", timer.Current(),
                // requiresJacobians ? "true" : "false", meshIndex, jacData.finalVertices.cols()); timer.Restart();
            }
            else
            {
                // joints do not influence geometry, just use rigid
                if (state.m_meshJacobianData[meshIndices[i]].blendshapeJacobianColOffset >= 0)
                {
                    deformedVertices.emplace_back(CreateDiffDataMatrix(jacData.blendshapeVertices, jacData.blendshapeJacobianRM,
                                                                       jacData.blendshapeJacobianColOffset));
                }
                else
                {
                    deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(jacData.blendshapeVertices));
                }
                if (hasRigid)
                {
                    // TODO: this is particularly slow
                    deformedVertices[i] = diffRigid.Transform(deformedVertices[i]);
                }
            }
        }
        else
        {
            deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(3, 0, DiffData<T>(Vector<T>())));
        }
    }

    state.m_vertices = std::move(deformedVertices);
    state.m_meshIndices = meshIndices;

    return state;
}

template <class T>
typename RigGeometry<T>::State RigGeometry<T>::EvaluateRigGeometry(const DiffDataAffine<T, 3, 3>& rigid,
                                                                   const DiffData<T>& joints,
                                                                   const DiffData<T>& blendshapes,
                                                                   const std::vector<int>& meshIndices) const
{
    State state;
    EvaluateRigGeometry(rigid, joints, blendshapes, meshIndices, state);
    return state;
}

template <class T>
void RigGeometry<T>::EvaluateSkinningWithJacobians(int meshIndex, State& state) const
{
    // Timer timer;

    const auto& meshData = *m->meshData[meshIndex];
    auto& stateData = state.m_meshJacobianData[meshIndex];

    // rest vertices are the vertices after blendshape evaluation
    const Eigen::Matrix<T, 3, -1>& restVertices = stateData.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = stateData.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    const SparseMatrix<T>& vertexInfluenceWeights = m->jointRig2.GetSkinningWeights(meshData.meshName);
    const int numVertices = int(vertexInfluenceWeights.outerSize());

    if (int(vertexInfluenceWeights.outerSize()) != int(restVertices.cols()))
    {
        CARBON_CRITICAL("all vertices need to be influenced by a node");
    }

    // get column size for jacobian
    int maxCols = -1;
    int startCol = std::numeric_limits<int>::max();

    if (state.m_jointJacobianColOffset >= 0)
    {
        maxCols = std::max<int>(int(state.m_skinningMatricesJacobian.cols()) + state.m_jointJacobianColOffset, maxCols);
        startCol = std::min<int>(state.m_jointJacobianColOffset, startCol);
    }

    if (stateData.blendshapeJacobianColOffset >= 0)
    {
        maxCols = std::max<int>(int(stateData.blendshapeJacobianRM->cols()) + stateData.blendshapeJacobianColOffset, maxCols);
        startCol = std::min<int>(stateData.blendshapeJacobianColOffset, startCol);
    }
    startCol = std::max<int>(0, startCol);

    Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& denseJacobian = *(stateData.finalJacobianRM);
    denseJacobian.resize(3 * numVertices, maxCols - startCol);

    auto evaluateVertexSkinning = [&](int start, int end) {
            for (int vID = start; vID < end; ++vID)
            {
                if (denseJacobian.cols() > 0)
                {
                    denseJacobian.block(3 * vID, 0, 3, denseJacobian.cols()).setZero();
                }
                Eigen::Vector3<T> result(0, 0, 0);
                for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights, vID); it; ++it)
                {
                    const int64_t& jointIndex = it.col();
                    const T& weight = it.value();
                    result += weight * (state.m_skinningMatrices[jointIndex] * restVertices.col(vID));

                    if (state.m_jointJacobianColOffset >= 0)
                    {
                        const int colOffset = state.m_jointJacobianColOffset - startCol;
                        const int jacCols = int(state.m_skinningMatricesJacobian.cols());
                        for (int j = 0; j < 3; j++)
                        {
                            denseJacobian.block(3 * vID, colOffset, 3,
                                                jacCols) +=
                                (weight * restVertices(j, vID)) * state.m_skinningMatricesJacobian.block(
                                    12 * jointIndex + 3 * j,
                                    0,
                                    3,
                                    jacCols);
                        }
                        denseJacobian.block(3 * vID, colOffset, 3, jacCols) += weight * state.m_skinningMatricesJacobian.block(
                            12 * jointIndex + 9,
                            0,
                            3,
                            jacCols);
                    }

                    if (stateData.blendshapeJacobianColOffset >= 0)
                    {
                        const int colOffset = stateData.blendshapeJacobianColOffset - startCol;
                        denseJacobian.block(3 * vID, colOffset, 3,
                                            stateData.blendshapeJacobianRM->cols()) +=
                            (weight * state.m_skinningMatrices[jointIndex].linear()) *
                            stateData.blendshapeJacobianRM->block(3 * vID, 0, 3, stateData.blendshapeJacobianRM->cols());
                    }
                }
                deformedVertices.col(vID) = result;
            }
        };

    if ((denseJacobian.size() > 5000) && m->taskThreadPool) // TODO: figure out a good magic number
    {
        m->taskThreadPool->AddTaskRangeAndWait(int(vertexInfluenceWeights.outerSize()), evaluateVertexSkinning);
    }
    else
    {
        evaluateVertexSkinning(0, int(vertexInfluenceWeights.outerSize()));
    }

    stateData.finalJacobianColOffset = (maxCols > 0) ? startCol : -1;

    // LOG_INFO("time for skinning {}: {}", meshIndex, timer.Current());
}

template <class T>
void RigGeometry<T>::EvaluateSkinningWithoutJacobians(int meshIndex, State& state) const
{
    const auto& meshData = *m->meshData[meshIndex];
    auto& stateData = state.m_meshJacobianData[meshIndex];

    const Eigen::Matrix<T, 3, -1>& restVertices = stateData.blendshapeVertices;
    Eigen::Matrix<T, 3, -1>& deformedVertices = stateData.finalVertices;
    deformedVertices.resize(3, restVertices.cols());

    const SparseMatrix<T>& vertexInfluenceWeights = m->jointRig2.GetSkinningWeights(meshData.meshName);
    const int numVertices = int(vertexInfluenceWeights.outerSize());

    for (int vID = 0; vID < numVertices; ++vID)
    {
        Eigen::Vector3<T> result(0, 0, 0);
        for (typename SparseMatrix<T>::InnerIterator it(vertexInfluenceWeights, vID); it; ++it)
        {
            result += it.value() * (state.m_skinningMatrices[it.col()] * restVertices.col(vID));
        }
        deformedVertices.col(vID) = result;
    }

    stateData.finalJacobianColOffset = -1;
}

template <class T>
void RigGeometry<T>::MakeBlendshapeOnly(const RigLogic<T>& rigLogic)
{
    m->blendshapeInfo.Clear();

    // do not convert eye geometry to blendshapes
    std::set<int> meshesToExclude;
    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        meshesToExclude.insert(EyeLeftMeshIndex(lod));
        meshesToExclude.insert(EyeRightMeshIndex(lod));
    }

    const int totalControls = rigLogic.NumTotalControls(); // raw + psd

    Timer timer;
    // get all expressions
    const std::vector<std::tuple<int, int, Eigen::VectorX<T>>> allExpressionPSDs = rigLogic.GetAllExpressions();
    if (int(allExpressionPSDs.size()) != totalControls)
    {
        CARBON_CRITICAL("number of expressions should match the total number of controls (raw + psd)");
    }
    LOG_INFO("time to get expressions: {}", timer.Current());
    timer.Restart();

    // evaluate vertices for all expressions
    std::vector<std::vector<Eigen::Matrix<T, 3, -1>>> allExpressions(m->meshData.size(), std::vector<Eigen::Matrix<T, 3, -1>>(totalControls));

    for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
    {
        if (GetMeshIndicesForLOD(lod).empty())
        {
            continue;
        }

        auto evaluateExpressions = [&](int start, int end) {
                State state;

                for (int i = start; i < end; ++i)
                {
                    const auto& [numAffected, psdIndex, psdControls] = allExpressionPSDs[i];
                    const DiffData<T> psdValues(psdControls);
                    const DiffData<T> diffJoints = rigLogic.EvaluateJoints(psdValues, lod);
                    EvaluateRigGeometry(DiffDataAffine<T, 3, 3>(), diffJoints, psdValues, GetMeshIndicesForLOD(lod), state);
                    for (size_t j = 0; j < GetMeshIndicesForLOD(lod).size(); ++j)
                    {
                        allExpressions[GetMeshIndicesForLOD(lod)[j]][psdIndex] = state.Vertices()[j].Matrix();
                    }
                }
            };
        if (m->taskThreadPool)
        {
            m->taskThreadPool->AddTaskRangeAndWait(int(allExpressionPSDs.size()), evaluateExpressions);
        }
        else
        {
            evaluateExpressions(0, int(allExpressionPSDs.size()));
        }
    }

    LOG_INFO("time to evalute {} expressions: {}", totalControls, timer.Current());
    timer.Restart();

    m->blendshapeInfo.Clear();
    std::vector<int> allBlendshapeChannels(totalControls, -1);

    // create blendshapes
    for (int meshIndex = 0; meshIndex < NumMeshes(); ++meshIndex)
    {
        if (!IsValid(meshIndex))
        {
            continue;
        }
        if (meshesToExclude.find(meshIndex) != meshesToExclude.end())
        {
            continue;
        }

        auto meshDataPtr = std::make_shared<MeshData<T>>(*m->meshData[meshIndex]);
        m->meshData[meshIndex] = meshDataPtr;
        MeshData<T>& meshData = *meshDataPtr;

        meshData.blendshapeChannels.clear();
        meshData.blendshapeMatrixDense = Eigen::Matrix<T, -1, -1>::Zero(meshData.blendshapeMatrixDense.rows(), totalControls);
        meshData.blendshapeControlsToMeshBlendshapeControls.resize(totalControls);

        std::vector<bool> blendshapeCreated(totalControls, false);
        for (const auto& [numAffected, psdControlIndex, psdControls] : allExpressionPSDs)
        {
            Eigen::Matrix<T, 3, -1> existingExpression = meshData.mesh.Vertices();
            for (int psdIndex = 0; psdIndex < int(psdControls.size()); ++psdIndex)
            {
                if ((psdControls[psdIndex] > 0) && (psdIndex != psdControlIndex))
                {
                    if (!blendshapeCreated[psdIndex])
                    {
                        CARBON_CRITICAL("using psd blendshape {} for psd control {}, but it has not been created yet",
                                        psdIndex,
                                        psdControlIndex);
                    }
                    Eigen::Map<Eigen::VectorX<T>>(existingExpression.data(),
                                                  existingExpression.size()) += psdControls[psdIndex] *
                        meshData.blendshapeMatrixDense.col(psdIndex);
                }
            }
            Eigen::Map<Eigen::Matrix<T, 3, -1>>(meshData.blendshapeMatrixDense.col(psdControlIndex).data(), 3,
                                                meshData.mesh.NumVertices()) = allExpressions[meshIndex][psdControlIndex] -
                existingExpression;
            meshData.blendshapeControlsToMeshBlendshapeControls[psdControlIndex] = psdControlIndex;
            blendshapeCreated[psdControlIndex] = true;

            if (allBlendshapeChannels[psdControlIndex] < 0)
            {
                const int currBlendshapeChannel = m->blendshapeInfo.NumBlendshapes();
                allBlendshapeChannels[psdControlIndex] = currBlendshapeChannel;
                m->blendshapeInfo.blendshapeNames.push_back(TITAN_NAMESPACE::fmt::format("control_{}", currBlendshapeChannel));
                m->blendshapeInfo.blendshapeInputIndices.push_back((uint16_t)psdControlIndex);
                m->blendshapeInfo.blendshapeOutputIndices.push_back((uint16_t)currBlendshapeChannel);
            }
            meshData.blendshapeChannels.push_back((uint16_t)allBlendshapeChannels[psdControlIndex]);
        }

        int currControl = 0;
        const T eps = T(0.05);
        for (int k = 0; k < totalControls; ++k)
        {
            const T blendshapeLength = meshData.blendshapeMatrixDense.col(k).norm();
            if (blendshapeLength > eps)
            {
                meshData.blendshapeMatrixDense.col(currControl) = meshData.blendshapeMatrixDense.col(k);
                meshData.blendshapeControlsToMeshBlendshapeControls[currControl] = k;
                currControl++;
            }
        }
        meshData.blendshapeMatrixDense.conservativeResize(meshData.blendshapeMatrixDense.rows(), currControl);
        meshData.blendshapeMatrixDenseRM = meshData.blendshapeMatrixDense;
        meshData.blendshapeControlsToMeshBlendshapeControls.conservativeResize(currControl);
        LOG_INFO("{} blendshapes for mesh {}", currControl, meshData.meshName);
    }
    LOG_INFO("time to create blendshapes: {}", timer.Current());
    timer.Restart();

    // remove geometry from joints
    for (int meshIndex = 0; meshIndex < NumMeshes(); ++meshIndex)
    {
        if (!IsValid(meshIndex))
        {
            continue;
        }
        if (meshesToExclude.find(meshIndex) != meshesToExclude.end())
        {
            continue;
        }
        m->jointRig2.RemoveSkinningWeights(m->meshData[meshIndex]->meshName);
    }
    LOG_INFO("time to remove joints: {}", timer.Current());
    timer.Restart();

    // uncomment below to verify blendshapes are correct
    const T errorEps = T(1e-3);
    State state;
    for (int i = 0; i < int(allExpressionPSDs.size()); ++i)
    {
        const int psdIndex = std::get<1>(allExpressionPSDs[i]);
        const DiffData<T> psdValues(std::get<2>(allExpressionPSDs[i]));
        for (int lod = 0; lod < rigLogic.NumLODs(); ++lod)
        {
            const DiffData<T> diffJoints = rigLogic.EvaluateJoints(psdValues, lod);
            EvaluateRigGeometry(DiffDataAffine<T, 3, 3>(), diffJoints, psdValues, GetMeshIndicesForLOD(lod), state);
            for (size_t j = 0; j < GetMeshIndicesForLOD(lod).size(); ++j)
            {
                const int meshIndex = GetMeshIndicesForLOD(lod)[j];
                if (!IsValid(meshIndex))
                {
                    continue;
                }
                const Eigen::Matrix<T, 3, -1> vertexDelta = allExpressions[meshIndex][psdIndex] - state.Vertices()[j].Matrix();
                const T error = vertexDelta.colwise().norm().mean();
                if (error > errorEps)
                {
                    LOG_WARNING("expression error for mesh {} and psd control {}: {} (max: {})",
                                meshIndex,
                                psdIndex,
                                error,
                                vertexDelta.rowwise().maxCoeff().transpose());
                }
            }
        }
    }
    LOG_INFO("time to verify psd expressions: {}", timer.Current());
    timer.Restart();
}

template <class T>
void RigGeometry<T>::RemoveUnusedJoints(RigLogic<T>& rigLogic)
{
    const int numJointsBefore = m->jointRig2.NumJoints();
    const std::vector<int> newToOldJointMapping = m->jointRig2.RemoveUnusedJoints();
    const int numJointsAfter = int(newToOldJointMapping.size());
    std::vector<int> oldToNew(numJointsBefore, -1);
    for (int newIdx = 0; newIdx < int(newToOldJointMapping.size()); ++newIdx)
    {
        const int oldIdx = newToOldJointMapping[newIdx];
        oldToNew[oldIdx] = newIdx;
    }

    // update which joint indices are part of which lod
    std::vector<std::vector<int>> newJointIndicesForLOD;
    for (const auto& jointIndices : m->jointIndicesForLOD)
    {
        std::vector<int> newJointIndices;
        for (const auto& jointIdx : jointIndices)
        {
            if (oldToNew[jointIdx] >= 0)
            {
                newJointIndices.push_back(oldToNew[jointIdx]);
            }
        }
        newJointIndicesForLOD.push_back(newJointIndices);
    }
    m->jointIndicesForLOD = newJointIndicesForLOD;

    Eigen::Matrix<T, 3, -1> jointRestPose(3, numJointsAfter);
    Eigen::Matrix<T, 3, -1> jointRestOrientationEuler(3, numJointsAfter);
    std::vector<Eigen::Matrix<T, 3, 3>> jointRestOrientation(numJointsAfter);
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointRig2StateBindPoses(numJointsAfter);
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointRig2StateInverseBindPoses(numJointsAfter);
    for (int newIdx = 0; newIdx < numJointsAfter; ++newIdx)
    {
        const int oldIdx = newToOldJointMapping[newIdx];
        jointRestPose.col(newIdx) = m->jointRestPose.col(oldIdx);
        jointRestOrientationEuler.col(newIdx) = m->jointRestOrientationEuler.col(oldIdx);
        jointRestOrientation[newIdx] = m->jointRestOrientation[oldIdx];
        jointRig2StateBindPoses[newIdx] = m->jointRig2StateBindPoses[oldIdx];
        jointRig2StateInverseBindPoses[newIdx] = m->jointRig2StateInverseBindPoses[oldIdx];
    }
    m->jointRestPose = jointRestPose;
    m->jointRestOrientationEuler = jointRestOrientationEuler;
    m->jointRestOrientation = jointRestOrientation;
    m->jointRig2StateBindPoses = jointRig2StateBindPoses;
    m->jointRig2StateInverseBindPoses = jointRig2StateInverseBindPoses;

    m->jointIndicesPerHierarchyLevel = m->jointRig2.GetJointsPerHierarchyLevel();

    // update joints for riglogic
    rigLogic.RemoveJoints(newToOldJointMapping);
}

template <class T>
void RigGeometry<T>::ReduceToLOD0Only()
{
    for (size_t lod = 1; lod < m->meshIndicesForLOD.size(); ++lod)
    {
        for (size_t j = 0; j < m->meshIndicesForLOD[lod].size(); ++j)
        {
            const int meshIndex = m->meshIndicesForLOD[lod][j];
            if (m->meshData[meshIndex])
            {
                m->jointRig2.RemoveSkinningWeights(m->meshData[meshIndex]->meshName);
                m->meshData[meshIndex].reset();
            }
        }
    }

    m->meshIndicesForLOD.resize(1);
    m->jointIndicesForLOD.resize(1);
}

template <class T>
void RigGeometry<T>::ReduceToMeshes(const std::vector<int>& meshIndices)
{
    std::set<int> meshesToKeep(meshIndices.begin(), meshIndices.end());
    for (size_t lod = 0; lod < m->meshIndicesForLOD.size(); ++lod)
    {
        std::vector<int> newMeshIndicesForLOD;
        for (size_t j = 0; j < m->meshIndicesForLOD[lod].size(); ++j)
        {
            const int meshIndex = m->meshIndicesForLOD[lod][j];
            if (m->meshData[meshIndex])
            {
                if (meshesToKeep.find(meshIndex) != meshesToKeep.end())
                {
                    newMeshIndicesForLOD.push_back(meshIndex);
                }
                else
                {
                    m->jointRig2.RemoveSkinningWeights(m->meshData[meshIndex]->meshName);
                    m->meshData[meshIndex].reset();
                }
            }
        }
        m->meshIndicesForLOD[lod] = newMeshIndicesForLOD;
    }
}

template <class T>
void RigGeometry<T>::RemoveBlendshapes(const std::vector<int>& meshIndices)
{
    for (int meshIndex : meshIndices)
    {
        if (IsValid(meshIndex) && (m->meshData[meshIndex]->blendshapeControlsToMeshBlendshapeControls.size() > 0))
        {
            auto newMeshData = std::make_shared<MeshData<T>>(*m->meshData[meshIndex]);
            newMeshData->blendshapeControlsToMeshBlendshapeControls = Eigen::VectorXi();
            newMeshData->blendshapeMatrixDense = Eigen::Matrix<T, -1, -1>();
            newMeshData->blendshapeMatrixDenseRM = newMeshData->blendshapeMatrixDense;
            newMeshData->blendshapeChannels.clear();
            m->meshData[meshIndex] = newMeshData;
        }
    }
}

template <class T>
void RigGeometry<T>::Resample(int meshIndex, const std::vector<int>& newToOldMap)
{
    if (!IsValid(meshIndex))
    {
        CARBON_CRITICAL("mesh index {} is not valid", meshIndex);
    }
    m->jointRig2.Resample(m->meshData[meshIndex]->meshName, newToOldMap);
    auto newMeshData = std::make_shared<MeshData<T>>(*m->meshData[meshIndex]);
    m->meshData[meshIndex] = newMeshData;
    newMeshData->mesh.Resample(newToOldMap);
    const int numBlendshapes = int(newMeshData->blendshapeMatrixDense.cols());
    Eigen::Matrix<T, -1, -1> blendshapeMatrix(newToOldMap.size() * 3, numBlendshapes);
    if (numBlendshapes > 0)
    {
        for (int i = 0; i < int(newToOldMap.size()); ++i)
        {
            blendshapeMatrix.block(3 * i, 0, 3, numBlendshapes) = newMeshData->blendshapeMatrixDense.block(3 * newToOldMap[i],
                                                                                                           0,
                                                                                                           3,
                                                                                                           numBlendshapes);
        }
    }
    newMeshData->blendshapeMatrixDense = blendshapeMatrix;
    newMeshData->blendshapeMatrixDenseRM = newMeshData->blendshapeMatrixDense;
}

template <class T>
std::string RigGeometry<T>::HeadMeshName(int lod) const
{
    return "head_lod" + std::to_string(lod) + "_mesh";
}

template <class T>
std::string RigGeometry<T>::TeethMeshName(int lod) const
{
    return "teeth_lod" + std::to_string(lod) + "_mesh";
}

template <class T>
std::string RigGeometry<T>::EyeLeftMeshName(int lod) const
{
    return "eyeLeft_lod" + std::to_string(lod) + "_mesh";
}

template <class T>
std::string RigGeometry<T>::EyeRightMeshName(int lod) const
{
    return "eyeRight_lod" + std::to_string(lod) + "_mesh";
}

template <class T>
int RigGeometry<T>::HeadMeshIndex(int lod) const
{
    return GetMeshIndex(HeadMeshName(lod));
}

template <class T>
int RigGeometry<T>::TeethMeshIndex(int lod) const
{
    return GetMeshIndex(TeethMeshName(lod));
}

template <class T>
int RigGeometry<T>::EyeLeftMeshIndex(int lod) const
{
    return GetMeshIndex(EyeLeftMeshName(lod));
}

template <class T>
int RigGeometry<T>::EyeRightMeshIndex(int lod) const
{
    return GetMeshIndex(EyeRightMeshName(lod));
}

template <class T>
int RigGeometry<T>::CorrespondingMeshIndexAtLod(int meshIndex, int lod) const
{
    std::string meshName = GetMeshName(meshIndex);
    int currLod = -1;
    for (size_t i = 0; i < m->meshIndicesForLOD.size(); ++i)
    {
        for (int idx : m->meshIndicesForLOD[i])
        {
            if (idx == meshIndex)
            {
                currLod = int(i);
                break;
            }
        }
    }
    if (currLod == -1)
    {
        return currLod;
    }

    const std::string lodName = "lod" + std::to_string(currLod);
    if (meshName.find(lodName) == meshName.npos)
    {
        return -1;
    }
    std::string newMeshName = TITAN_NAMESPACE::ReplaceSubstring(meshName,
                                                             "lod" + std::to_string(currLod),
                                                             "lod" + std::to_string(lod));
    for (size_t i = 0; i < m->meshData.size(); ++i)
    {
        if (m->meshData[i] && (m->meshData[i]->meshName == newMeshName))
        {
            return int(i);
        }
    }

    return -1;
}

//! Returns vertex influence weights as a sparse matrix. Matrix is row major, rows correspond to vertices, and columns to joints
template <class T>
SparseMatrix<T> RigGeometry<T>::VertexInfluenceWeights(const std::string& meshName) const
{
    return m->jointRig2.GetSkinningWeights(meshName);
}

template <class T>
std::string RigGeometry<T>::GetSymmetricMeshName(const std::string& meshName) const
{
    if (TITAN_NAMESPACE::StringStartsWith(meshName, "eyeLeft") ||
        TITAN_NAMESPACE::StringStartsWith(meshName, "eyeRight"))
    {
        return TITAN_NAMESPACE::StringStartsWith(meshName, "eyeLeft") ?
               TITAN_NAMESPACE::ReplaceSubstring(meshName, "eyeLeft", "eyeRight") :
               TITAN_NAMESPACE::ReplaceSubstring(meshName, "eyeRight", "eyeLeft");
    }
    else
    {
        return meshName;
    }
}

template <class T>
void RigGeometry<T>::Translate(const Eigen::Vector3<T>& translation)
{
    for (int i = 0; i < (int)m->jointRestPose.cols(); ++i)
    {
        if (m->jointRig2.GetParentIndex(i) < 0)
        {
            m->jointRestPose.col(i) += translation;
        }
    }
    for (size_t i = 0; i < m->meshData.size(); ++i)
    {
        if (m->meshData[i])
        {
            Eigen::Matrix<T, 3, -1> vertices = m->meshData[i]->mesh.Vertices();
            vertices.colwise() += translation;
            SetMesh((int)i, vertices);
        }
    }
    UpdateBindPoses();
}

template <class T>
Eigen::Vector3<T> RigGeometry<T>::EyesMidpoint() const
{
    const Eigen::Vector3<T> eyeLeftCenter = GetMesh(EyeLeftMeshIndex(/*lod=*/0)).Vertices().rowwise().mean();
    const Eigen::Vector3<T> eyeRightCenter = GetMesh(EyeRightMeshIndex(/*lod=*/0)).Vertices().rowwise().mean();
    return T(0.5) * (eyeLeftCenter + eyeRightCenter);
}

template <class T>
typename RigGeometry<T>::State& RigGeometry<T>::EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                                                               const DiffData<T>& diffJoints,
                                                                               const std::vector<DiffDataMatrix<T, 3, -1>>& diffBlendshapes,
                                                                               const std::vector<int>& meshIndices,
                                                                               const std::vector<Eigen::Ref<const Eigen::Matrix<T, 3, -1>>>& meshNeutrals,
                                                                               State& state) const
{
    const bool hasBlendshapes = (int)diffBlendshapes.size() > 0 ? true : false;
    // if there are blendshapes, we must be sure the data is aligned with meshes
    if (hasBlendshapes)
    {
        if (diffBlendshapes.size() != meshIndices.size())
        {
            CARBON_CRITICAL("Input blendshapes vs mesh indices size mismatch.");
        }
    }

    const bool requiresJacobians = diffRigid.HasJacobian() || diffJoints.HasJacobian();
    const bool hasRigid = diffRigid.HasJacobian() ||
        ((diffRigid.Matrix() - Eigen::Matrix<T, 4, 4>::Identity()).norm() > T(1e-10));

    for (int i = 0; i < (int)meshIndices.size(); ++i)
    {
        const int meshIndex = meshIndices[i];
        if ((meshIndex >= 0) && m->meshData[meshIndex])
        {
            state.SetupForMesh(meshIndex);
            auto& meshData = *m->meshData[meshIndex];

            if (i < (int)meshNeutrals.size() && meshNeutrals[i].cols() == meshData.mesh.NumVertices())
            {
                state.m_meshJacobianData[meshIndex].blendshapeVertices = meshNeutrals[i];
            }
            else
            {
                state.m_meshJacobianData[meshIndex].blendshapeVertices = meshData.mesh.Vertices();
            }
            if (hasBlendshapes)
            {
                if ((int)diffBlendshapes[i].Value().size() > 0)
                {
                    state.m_meshJacobianData[meshIndex].blendshapeVertices += diffBlendshapes[i].Matrix();
                }
            }
            state.m_meshJacobianData[meshIndex].blendshapeJacobianColOffset = -1;
        }
    }

    std::vector<DiffDataMatrix<T, 3, -1>> deformedVertices;

    if (m->jointRig2.NumJoints() > 0)
    {
        if (requiresJacobians)
        {
            EvaluateJointDeltas(diffRigid, diffJoints, state);
        }
        else
        {
            EvaluateJointDeltasWithoutJacobians(diffRigid, diffJoints, state);
        }
        // if (requiresJacobians) LOG_INFO("time to evaluate joints: {} (with jacobian: {})", timer.Current(), requiresJacobians ?
        // "true" : "false"); timer.Restart();
    }
    else
    {
        if (diffJoints.Size() > 0)
        {
            LOG_WARNING("RigGeometry does not contain joints, but RigGeometry is called with deltas on joints");
        }
    }

    for (size_t i = 0; i < meshIndices.size(); ++i)
    {
        const int meshIndex = meshIndices[i];
        if ((meshIndex >= 0) && m->meshData[meshIndex])
        {
            const auto& meshData = *m->meshData[meshIndex];
            const auto& jacData = state.m_meshJacobianData[meshIndex];
            if (m->jointRig2.HasSkinningWeights(meshData.meshName))
            {
                if (requiresJacobians)
                {
                    EvaluateSkinningWithJacobians(meshIndex, state);
                }
                else
                {
                    EvaluateSkinningWithoutJacobians(meshIndex, state);
                }

                // if (requiresJacobians) LOG_INFO("time to evaluate skinning: {} (with jacobian: {})", timer.Current(),
                // requiresJacobians ? "true" : "false"); timer.Restart();
                if (state.m_meshJacobianData[meshIndices[i]].finalJacobianColOffset >= 0)
                {
                    deformedVertices.emplace_back(CreateDiffDataMatrix(jacData.finalVertices, jacData.finalJacobianRM,
                                                                       jacData.finalJacobianColOffset));
                }
                else
                {
                    deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(jacData.finalVertices));
                }
                // if (requiresJacobians) LOG_INFO("time to create matrix: {} (with jacobian: {}) (mesh index: {}, num vertices:
                // {})", timer.Current(), requiresJacobians ? "true" : "false", meshIndex, jacData.finalVertices.cols());
                // timer.Restart();
            }
            else
            {
                // joints do not influence geometry, just use rigid
                if (state.m_meshJacobianData[meshIndices[i]].blendshapeJacobianColOffset >= 0)
                {
                    deformedVertices.emplace_back(CreateDiffDataMatrix(jacData.blendshapeVertices, jacData.blendshapeJacobianRM,
                                                                       jacData.blendshapeJacobianColOffset));
                }
                else
                {
                    deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(jacData.blendshapeVertices));
                }

                if (hasRigid)
                {
                    // TODO: this is particularly slow
                    deformedVertices[i] = diffRigid.Transform(deformedVertices[i]);
                }
            }
        }
        else
        {
            deformedVertices.emplace_back(DiffDataMatrix<T, 3, -1>(3, 0, DiffData<T>(Vector<T>())));
        }
    }

    state.m_vertices = std::move(deformedVertices);
    state.m_meshIndices = meshIndices;

    return state;
}

template <class T>
typename RigGeometry<T>::State& RigGeometry<T>::EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                                                               const DiffData<T>& diffJoints,
                                                                               const std::vector<DiffDataMatrix<T, 3, -1>>& diffBlendshapes,
                                                                               const std::vector<int>& meshIndices,
                                                                               State& state) const
{
    return EvaluateWithPerMeshBlendshapes(diffRigid, diffJoints, diffBlendshapes, meshIndices, {}, state);
}

template <class T>
typename RigGeometry<T>::State RigGeometry<T>::EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                                                              const DiffData<T>& diffJoints,
                                                                              const std::vector<DiffDataMatrix<T, 3,
                                                                                                               -1>>& diffBlendshapes,
                                                                              const std::vector<int>& meshIndices) const
{
    State state;
    EvaluateWithPerMeshBlendshapes(diffRigid, diffJoints, diffBlendshapes, meshIndices, {}, state);
    return state;
}

template <class T>
void RigGeometry<T>::Mirror(const std::map<std::string, std::vector<int>>& symmetries,
                            const std::map<std::string, std::vector<std::pair<int,T>>>& meshRoots,
                            RigLogic<T>& rigLogic)
{
    const std::vector<int> symmetricPsdIndices = rigLogic.GetSymmetricPsdIndices();

    // for (int k = 0; k < (int)m->blendshapeInfo.blendshapeNames.size(); ++k)
    // {
    //     LOG_INFO("blendshape {}: {}", k, m->blendshapeInfo.blendshapeNames[k]);
    // }

    // mirror base geometry and blendshapes
    std::vector<std::shared_ptr<const MeshData<T>>> newMeshDataVec;
    for (size_t idx = 0; idx < m->meshData.size(); ++idx)
    {
        const auto& meshData = m->meshData[idx];

        auto sIt = symmetries.find(meshData->meshName);
        if (sIt == symmetries.end())
        {
            newMeshDataVec.push_back(meshData);
            continue;
        }

        std::shared_ptr<const MeshData<T>> symmetricMeshData = meshData;
        const std::string symmetricMeshName = GetSymmetricMeshName(meshData->meshName);
        if (meshData->meshName != symmetricMeshName)
        {
            if (GetMesh(meshData->meshName).NumVertices() != GetMesh(symmetricMeshName).NumVertices())
            {
                CARBON_CRITICAL("mesh \"{}\" and \"{}\" do not match", meshData->meshName, symmetricMeshName);
            }

            auto sIt2 = symmetries.find(symmetricMeshName);
            if (sIt2 == symmetries.end())
            {
                if (sIt->second != sIt2->second)
                {
                    CARBON_CRITICAL("mesh \"{}\" and \"{}\" do not have matching symmetries",
                                    meshData->meshName,
                                    symmetricMeshName);
                }
            }

            for (size_t i = 0; i < m->meshData.size(); ++i)
            {
                if (m->meshData[i]->meshName == symmetricMeshName)
                {
                    symmetricMeshData = m->meshData[i];
                }
            }

            if (meshData->blendshapeMatrixDense.cols() != symmetricMeshData->blendshapeMatrixDense.cols())
            {
                CARBON_CRITICAL("mesh \"{}\" and \"{}\" do not have matching blendshape sizes",
                                meshData->meshName,
                                symmetricMeshName);
            }
        }

        const std::vector<int>& meshSymmetry = sIt->second;
        Eigen::Matrix<T, -1, -1> blendshapeMatrixDense = meshData->blendshapeMatrixDense;
        LOG_VERBOSE("updating mesh {} for \"{}\"", ((blendshapeMatrixDense.size() > 0) ? "and blendshapes " : ""), meshData->meshName);
        Eigen::Matrix<T, 3, -1> vertices = symmetricMeshData->mesh.Vertices();
        for (int i = 0; i < (int)meshSymmetry.size(); ++i)
        {
            vertices.col(i) = symmetricMeshData->mesh.Vertices().col(meshSymmetry[i]);
            vertices(0, i) = -vertices(0, i);
        }

        for (int c = 0; c < (int)blendshapeMatrixDense.cols(); ++c)
        {
            const int blendshapeChannel = meshData->blendshapeChannels[c];
            const int psdIndex = m->blendshapeInfo.blendshapeInputIndices[blendshapeChannel];
            const int symmetricPsdIndex = symmetricPsdIndices[psdIndex];
            const int symmetricBlendshapeChannel = TITAN_NAMESPACE::GetItemIndex<uint16_t>(m->blendshapeInfo.blendshapeInputIndices, (uint16_t)symmetricPsdIndex);
            if (symmetricBlendshapeChannel < 0)
            {
                CARBON_CRITICAL("no blendshape channel for psd index {} (symmetric psd index of psd index {} and channel {})", symmetricPsdIndex, blendshapeChannel, psdIndex);
            }
            const int symmetricIndex = TITAN_NAMESPACE::GetItemIndex<uint16_t>(symmetricMeshData->blendshapeChannels, (uint16_t)symmetricBlendshapeChannel);
            if (symmetricIndex < 0)
            {
                CARBON_CRITICAL("blendshape for psd index {} ({}, {}) is not a blendshape for the symmetric mesh {}", psdIndex, meshData->meshName, m->blendshapeInfo.blendshapeNames[blendshapeChannel], symmetricMeshName);
            }
            for (int i = 0; i < (int)meshSymmetry.size(); ++i)
            {
                blendshapeMatrixDense.col(c).segment(3 * i, 3) = symmetricMeshData->blendshapeMatrixDense.col(symmetricIndex).segment(
                    3 * meshSymmetry[i],
                    3);
                blendshapeMatrixDense(3 * i, c) = -blendshapeMatrixDense(3 * i, c);
            }
        }

        std::shared_ptr<MeshData<T>> newMeshData = std::make_shared<MeshData<T>>(*meshData);
        newMeshData->mesh.SetVertices(vertices);
        newMeshData->blendshapeMatrixDense = blendshapeMatrixDense;
        newMeshData->blendshapeMatrixDenseRM = blendshapeMatrixDense;
        newMeshDataVec.push_back(newMeshData);
    }

    // mirror the joints
    const JointRig2<T>& jointRig2 = GetJointRig();
    const std::vector<int> symmetricJointIndices = jointRig2.GetSymmetricJointIndices();

    // mirror the joint transformations
    std::vector<Eigen::Matrix3<T>> restOrientation = m->jointRestOrientation;
    Eigen::Matrix<T, 3, -1> restOrientationEuler = m->jointRestOrientationEuler;
    Eigen::Matrix<T, 3, -1> restPose = m->jointRestPose;
    for (int jointIndex = 0; jointIndex < jointRig2.NumJoints(); ++jointIndex)
    {
        const int mirroredIndex = symmetricJointIndices[jointIndex];
        restPose.col(jointIndex) = m->jointRestPose.col(mirroredIndex);
        restPose(0, jointIndex) = -restPose(0, jointIndex);
        // negate rotation for y and z euler angles, x remains the same
        restOrientationEuler.col(jointIndex) = m->jointRestOrientationEuler.col(mirroredIndex);
        restOrientationEuler(1, jointIndex) = - restOrientationEuler(1, jointIndex);
        restOrientationEuler(2, jointIndex) = - restOrientationEuler(2, jointIndex);
        restOrientation[jointIndex] = EulerXYZ<T>(degree2radScale<T>() * restOrientationEuler.col(jointIndex));
        T diff = (m->jointRestOrientationEuler.col(jointIndex).transpose() - restOrientationEuler.col(jointIndex).transpose()).norm();
        if (diff > T(1e-6))
        {
            LOG_VERBOSE("{} {} before: {} {} after: {}", m->jointRig2.GetJointNames()[jointIndex], m->jointRig2.GetJointNames()[mirroredIndex], diff, m->jointRestOrientationEuler.col(jointIndex).transpose(), restOrientationEuler.col(jointIndex).transpose());
        }
    }
    m->jointRestOrientation = restOrientation;
    m->jointRestOrientationEuler = restOrientationEuler;
    m->jointRestPose = restPose;

    // mirror the joint weights - this works even for the eyes given that the vertex symmetries are the same
    // for left and right eye
    for (auto& meshData : newMeshDataVec)
    {
        auto sIt = symmetries.find(meshData->meshName);
        if (sIt == symmetries.end())
        {
            continue;
        }
        const std::vector<int>& meshSymmetry = sIt->second;
        LOG_INFO("updating skinning weights for \"{}\"", meshData->meshName);
        const std::string symmetricMeshName = GetSymmetricMeshName(meshData->meshName);

        SparseMatrix<T> smat = m->jointRig2.GetSkinningWeights(symmetricMeshName);
        std::vector<Eigen::Triplet<T>> triplets;
        for (int vID = 0; vID < (int)smat.rows(); ++vID)
        {
            // a vertex should be influenced by a joint according to how the symmetric vertex is influenced by the symmetric joint
            const int symmetricVID = meshSymmetry[vID];
            for (typename SparseMatrix<T>::InnerIterator it(smat, vID); it; ++it)
            {
                const int jointIndex = (int)it.col();
                const int symmetricJointIndex = symmetricJointIndices[jointIndex];
                triplets.push_back(Eigen::Triplet<T>(symmetricVID, symmetricJointIndex, it.value()));
            }
        }
        smat.setFromTriplets(triplets.begin(), triplets.end());
        m->jointRig2.SetSkinningWeights(meshData->meshName, smat);
    }

    // mirror the joint deltas
    rigLogic.MirrorJoints(symmetricJointIndices);

    // Mirror the meshes that are not symmetric (eyelashes), and hence we use the eyelash roots to best transform
    // the eyelashes from the original head mesh to the mirrored head mesh. We do not modify blendshapes.
    for (const auto& [name, roots] : meshRoots)
    {
        if (!TITAN_NAMESPACE::StringEndsWith(name, "_roots"))
        {
            CARBON_CRITICAL("\"{}\" should end with \"_roots\"", name);
        }
        if (!TITAN_NAMESPACE::StringStartsWith(name, "eyelashes"))
        {
            CARBON_CRITICAL("\"{}\" should be a symmetric mesh", name);
        }
        const std::string name2 = name.substr(0, name.size() - 6);
        // find lod
        int lod = -1;
        {
            size_t pos = name2.find("lod");
            if (pos != std::string::npos)
            {
                pos += 3;
                size_t end = pos;
                while (end < name2.size() && std::isdigit(name2[end]))
                {
                    end++;
                }
                lod = std::stoi(name.substr(pos, end - pos));
            }
            else
            {
                CARBON_CRITICAL("mesh \"{}\" does not contain lod", name);
            }
        }
        const int headMeshIndex = HeadMeshIndex(lod);
        const Mesh<T>& srcHeadMesh = m->meshData[headMeshIndex]->mesh;

        const int meshIndex = GetMeshIndex(name2);
        const Mesh<T>& srcEyelashesMesh = m->meshData[meshIndex]->mesh;
        // initialize the mapping between eyelashes and head mesh
        std::vector<std::shared_ptr<EyelashConnectedVertices<T>>> connectedVertices;
        bool bInitializedEyelashMapping = EyelashConnectedVertices<T>::InitializeEyelashMapping(srcHeadMesh, srcEyelashesMesh, roots, connectedVertices);
        if (!bInitializedEyelashMapping)
        {
            CARBON_CRITICAL("failed to initialize eyelash mapping");
        }

        std::shared_ptr<MeshData<T>> newMeshData = std::make_shared<MeshData<T>>(*newMeshDataVec[meshIndex]);

        // update the eyelash positions
        const Mesh<T>& targetHeadMesh = newMeshDataVec[HeadMeshIndex(lod)]->mesh;

        Eigen::Matrix<T, 3, -1> newEyelashVertices;
        EyelashConnectedVertices<T>::ApplyEyelashMapping(srcHeadMesh, targetHeadMesh.Vertices(), srcEyelashesMesh, connectedVertices, newEyelashVertices);

        newMeshData->mesh.SetVertices(newEyelashVertices);
        newMeshDataVec[meshIndex] = newMeshData;
    }

    m->meshData = newMeshDataVec;


    UpdateBindPoses();
}

template <class T>
void RigGeometry<T>::CreateSubdivision(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights, int meshIndex)
{
    std::shared_ptr<MeshData<T>> newMeshData = std::make_shared<MeshData<T>>(*m->meshData[meshIndex]);
    m->meshData[meshIndex] = newMeshData;

    SubdivisionMesh<T> subdivMesh;
    subdivMesh.Create(subdivisionTopology, stencilWeights);
    subdivMesh.SetBaseMesh(newMeshData->mesh);
    newMeshData->mesh = subdivMesh.EvaluateMesh();

    // update blendshapes
    Eigen::Matrix<T, -1, -1> blendshapeMatrixDense = Eigen::Matrix<T, -1, -1>::Zero(newMeshData->mesh.NumVertices() * 3,
                                                                                    newMeshData->blendshapeMatrixDense.cols());
    for (int c = 0; c < (int)newMeshData->blendshapeMatrixDense.cols(); ++c)
    {
        for (const auto& [subdiv_vID, base_vID, weight] : stencilWeights)
        {
            blendshapeMatrixDense.block(3 * subdiv_vID, c, 3, 1) += weight * newMeshData->blendshapeMatrixDense.block(
                3 * base_vID,
                c,
                3,
                1);
        }
    }
    newMeshData->blendshapeMatrixDense = blendshapeMatrixDense;
    newMeshData->blendshapeMatrixDenseRM = newMeshData->blendshapeMatrixDense;

    if (m->jointRig2.HasSkinningWeights(newMeshData->meshName))
    {
        // update skinning weights
        const SparseMatrix<T>& skinningMatrix = m->jointRig2.GetSkinningWeights(newMeshData->meshName);
        std::vector<Eigen::Triplet<T>> triplets;
        for (const auto& [subdiv_vID, base_vID, weight] : stencilWeights)
        {
            for (typename SparseMatrix<T>::InnerIterator it(skinningMatrix, base_vID); it; ++it)
            {
                triplets.emplace_back(Eigen::Triplet<T>(subdiv_vID, (int)it.col(), weight * it.value()));
            }
        }
        SparseMatrix<T> newSkinningMatrix(newMeshData->mesh.NumVertices(), skinningMatrix.cols());
        newSkinningMatrix.setFromTriplets(triplets.begin(), triplets.end());
        m->jointRig2.SetSkinningWeights(newMeshData->meshName, newSkinningMatrix);
    }
}

template <class T>
void RigGeometry<T>::Transfer(int meshIndex, const RigGeometry<T>& otherRig, int otherMeshIndex)
{
    std::shared_ptr<const MeshData<T>> currMeshData = m->meshData[meshIndex];
    std::shared_ptr<const MeshData<T>> otherMeshData = otherRig.m->meshData[otherMeshIndex];
    if (!otherMeshData)
    {
        CARBON_CRITICAL("invalid other mesh data");
    }

    std::shared_ptr<MeshData<T>> newMeshData = std::make_shared<MeshData<T>>(*otherMeshData);
    m->meshData[meshIndex] = newMeshData;

    m->jointRig2.RemoveSkinningWeights(currMeshData->meshName);
    m->jointRig2.SetSkinningWeights(newMeshData->meshName, otherRig.m->jointRig2.GetSkinningWeights(newMeshData->meshName));

    Eigen::Matrix<T, -1, -1> blendshapeMatrixDense;

    if (otherMeshData->blendshapeMatrixDense.size() > 0)
    {
        blendshapeMatrixDense = Eigen::Matrix<T, -1, -1>::Zero(newMeshData->mesh.NumVertices() * 3, currMeshData->blendshapeMatrixDense.cols());
        Mesh<T> currMesh = currMeshData->mesh;
        currMesh.Triangulate();

        const Mesh<T>& newMesh = newMeshData->mesh;
        if (newMesh.NumTriangles() > 0)
        {
            CARBON_CRITICAL("no support for triangle meshes");
        }

        Eigen::Matrix<T, 3, -1> texcoords3d = Eigen::Matrix<T, 3, -1>::Zero(3, currMesh.Texcoords().cols());
        texcoords3d.topRows(2) = currMesh.Texcoords();

        TITAN_NAMESPACE::AABBTree<T, 8, TITAN_NAMESPACE::AABBTreeDefaultEPS<T>, TITAN_NAMESPACE::NoCompatibility<T>> aabbTree(texcoords3d.transpose(), currMesh.TexTriangles().transpose());

        // Find intersection for each vertex
        for (int face = 0; face < newMesh.NumQuads(); ++face)
        {
            for (int vtx = 0; vtx < 4; ++vtx)
            {
                const int vID = newMesh.Quads()(vtx, face);
                const Eigen::Vector2<T> uv = newMesh.Texcoords().col(newMesh.TexQuads()(vtx, face));
                const Eigen::Vector3<T> query = Eigen::Vector3<T>(uv[0], uv[1], 0.0f);
                auto [tID, barycentric, dist] = aabbTree.getClosestPoint(query.transpose(), T(1e9));
                if (tID < 0 || dist > std::numeric_limits<T>::epsilon())
                {
                    CARBON_CRITICAL("failed to transfer blendshapes for face {} {}: {} {}", face, vtx, tID, dist);
                }
                if (tID >= 0)
                {
                    const Eigen::Vector3i vIDs = currMesh.Triangles().col(tID);
                    for (int c = 0; c < blendshapeMatrixDense.cols(); ++c)
                    {
                        Eigen::Vector3<T> delta = barycentric[0] * currMeshData->blendshapeMatrixDense.col(c).segment(3 * vIDs[0], 3);
                        delta += barycentric[1] * currMeshData->blendshapeMatrixDense.col(c).segment(3 * vIDs[1], 3);
                        delta += barycentric[2] * currMeshData->blendshapeMatrixDense.col(c).segment(3 * vIDs[2], 3);
                        blendshapeMatrixDense.col(c).segment(3 * vID, 3) = delta;
                    }
                }
            }
        }
    }
    newMeshData->blendshapeMatrixDense = blendshapeMatrixDense;
    newMeshData->blendshapeMatrixDenseRM = newMeshData->blendshapeMatrixDense;
}

template <class T>
void RigGeometry<T>::SaveBindPoseToDna(dna::Writer* writer) const
{
    const Eigen::Matrix<float, 3, -1> poses = m->jointRestPose.template cast<float>();
    const Eigen::Matrix<float, 3, -1> angles = m->jointRestOrientationEuler.template cast<float>();
    writer->setNeutralJointTranslations((const dna::Vector3*)poses.data(), (uint16_t)poses.cols());
    writer->setNeutralJointRotations((const dna::Vector3*)angles.data(), (uint16_t)angles.cols());
}

template <class T>
void RigGeometry<T>::SaveDna(dna::Writer* writer, int meshIndexIn) const
{
    const std::vector<std::uint16_t> meshIndices = { (std::uint16_t)meshIndexIn };

    for (std::uint16_t meshIndex : meshIndices)
    {
        const MeshData<T>& meshData = *m->meshData[meshIndex];
        const std::uint32_t numVertices = (std::uint32_t)meshData.mesh.NumVertices();
        Mesh<float> quadMesh = meshData.mesh.template Cast<float>();
        Mesh<float> triMesh = quadMesh;
        triMesh.Triangulate();
        triMesh.CalculateVertexNormals();

        {
            writer->clearFaceVertexLayoutIndices((std::uint16_t)meshIndex);
            writer->setVertexPositions((std::uint16_t)meshIndex, (const dna::Position*)triMesh.Vertices().data(), numVertices);
            writer->setVertexNormals((std::uint16_t)meshIndex, (const dna::Normal*)triMesh.VertexNormals().data(), numVertices);
            Eigen::Matrix<float, 2, -1> texcoords = quadMesh.Texcoords();
            texcoords.row(1) = Eigen::RowVectorXf::Ones(texcoords.cols()) - texcoords.row(1);
            writer->setVertexTextureCoordinates(meshIndex, (const dna::TextureCoordinate*)texcoords.data(),
                                                uint32_t(texcoords.cols()));

            int totalFaces = quadMesh.NumQuads() + quadMesh.NumTriangles();
            writer->setFaceVertexLayoutIndices(uint16_t(meshIndex), totalFaces - 1, nullptr, 0);

            int faceCount = 0;
            std::vector<dna::VertexLayout> vertexLayouts;
            for (int quadIndex = 0; quadIndex < quadMesh.NumQuads(); ++quadIndex)
            {
                std::vector<uint32_t> layoutIndices;
                for (int k = 0; k < 4; ++k)
                {
                    dna::VertexLayout vertexLayout;
                    vertexLayout.position = quadMesh.Quads()(k, quadIndex);
                    vertexLayout.normal = quadMesh.Quads()(k, quadIndex);
                    vertexLayout.textureCoordinate = quadMesh.TexQuads()(k, quadIndex);
                    layoutIndices.push_back(uint32_t(vertexLayouts.size()));
                    vertexLayouts.push_back(vertexLayout);
                }
                writer->setFaceVertexLayoutIndices(meshIndex, faceCount, layoutIndices.data(), uint32_t(layoutIndices.size()));
                faceCount++;
            }
            for (int triIndex = 0; triIndex < quadMesh.NumTriangles(); ++triIndex)
            {
                std::vector<uint32_t> layoutIndices;
                for (int k = 0; k < 3; ++k)
                {
                    dna::VertexLayout vertexLayout;
                    vertexLayout.position = quadMesh.Triangles()(k, triIndex);
                    vertexLayout.normal = quadMesh.Triangles()(k, triIndex);
                    vertexLayout.textureCoordinate = quadMesh.TexTriangles()(k, triIndex);
                    layoutIndices.push_back(uint32_t(vertexLayouts.size()));
                    vertexLayouts.push_back(vertexLayout);
                }
                writer->setFaceVertexLayoutIndices(meshIndex, faceCount, layoutIndices.data(), uint32_t(layoutIndices.size()));
                faceCount++;
            }
            writer->setVertexLayouts(meshIndex, vertexLayouts.data(), uint32_t(vertexLayouts.size()));
        }

        // write skinning
        writer->clearSkinWeights((std::uint16_t)meshIndex);
        Eigen::SparseMatrix<T, Eigen::RowMajor> smat = m->jointRig2.GetSkinningWeights(meshData.meshName);
        std::vector<std::uint16_t> jointIndices;
        std::vector<float> weights;
        for (int vID = numVertices - 1; vID >= 0; --vID)
        {
            jointIndices.clear();
            weights.clear();
            for (typename SparseMatrix<T>::InnerIterator it(smat, vID); it; ++it)
            {
                jointIndices.push_back((std::uint16_t)it.col());
                weights.push_back((float)it.value());
            }
            writer->setSkinWeightsValues((std::uint16_t)meshIndex,
                                         (std::uint32_t)vID,
                                         weights.data(),
                                         (std::uint16_t)weights.size());
            writer->setSkinWeightsJointIndices((std::uint16_t)meshIndex,
                                               (std::uint32_t)vID,
                                               jointIndices.data(),
                                               (std::uint16_t)jointIndices.size());
        }

        // write blendshapes
        std::vector<Eigen::Vector3f> vertices;
        std::vector<uint32_t> vertexIndices;
        writer->clearBlendShapeTargets(meshIndex);
        for (std::uint16_t bsIndex = 0; bsIndex < (std::uint16_t)meshData.blendshapeMatrixDense.cols(); ++bsIndex)
        {
            vertices.clear();
            vertexIndices.clear();
            for (std::uint32_t k = 0; k < numVertices; ++k)
            {
                const Eigen::Vector3f delta = meshData.blendshapeMatrixDense.block(3 * k, bsIndex, 3, 1).template cast<float>();
                if (delta.squaredNorm() > 0)
                {
                    vertices.push_back(delta);
                    vertexIndices.push_back(k);
                }
            }
            writer->setBlendShapeChannelIndex(meshIndex, bsIndex, m->meshData[meshIndex]->blendshapeChannels[bsIndex]);
            writer->setBlendShapeTargetDeltas(meshIndex,
                                              bsIndex,
                                              (const dna::Delta*)vertices.data(),
                                              (std::uint32_t)vertices.size());
            writer->setBlendShapeTargetVertexIndices(meshIndex, bsIndex, vertexIndices.data(),
                                                     (std::uint32_t)vertexIndices.size());
        }
    }
}

template <class T>
void RigGeometry<T>::SaveBlendshapeMappingToDna(dna::Writer* writer) const
{
    writer->clearMeshBlendShapeChannelMappings();
    writer->clearLODBlendShapeChannelMappings();
    writer->clearBlendShapeChannelNames();

    for (uint16_t channelIndex = 0; channelIndex < (uint16_t)m->blendshapeInfo.NumBlendshapes(); ++channelIndex)
    {
        writer->setBlendShapeChannelName(channelIndex, m->blendshapeInfo.blendshapeNames[channelIndex].c_str());
    }
    writer->setBlendShapeChannelInputIndices(m->blendshapeInfo.blendshapeInputIndices.data(), (std::uint16_t)m->blendshapeInfo.blendshapeInputIndices.size());
    writer->setBlendShapeChannelOutputIndices(m->blendshapeInfo.blendshapeOutputIndices.data(), (std::uint16_t)m->blendshapeInfo.blendshapeOutputIndices.size());


    std::map<std::uint16_t, std::uint16_t> mesh2lodMapping;
    const std::uint16_t numLODs = (std::uint16_t)m->meshIndicesForLOD.size();
    for (std::uint16_t lod = 0; lod < numLODs; ++lod)
    {
        for (const auto& meshIndex : m->meshIndicesForLOD[lod])
        {
            mesh2lodMapping[(std::uint16_t)meshIndex] = lod;
        }
    }
    std::vector<std::set<std::uint16_t>> blendshapeChannelsPerLOD(numLODs);

    std::set<std::pair<uint16_t, uint16_t>> blendshapeChannelMappings;
    for (std::uint16_t meshIndex = 0; meshIndex < (std::uint16_t)NumMeshes(); ++meshIndex)
    {
        int numBlendshapes = (int)m->meshData[meshIndex]->blendshapeControlsToMeshBlendshapeControls.size();
        if (numBlendshapes > 0)
        {
            for (std::uint16_t i = 0; i < (std::uint16_t)numBlendshapes; ++i)
            {
                std::uint16_t blendshapeChannel = m->meshData[meshIndex]->blendshapeChannels[i];
                writer->setBlendShapeChannelIndex(meshIndex, i, blendshapeChannel);
                blendshapeChannelMappings.insert({blendshapeChannel, meshIndex});
                const std::uint16_t lod = mesh2lodMapping[meshIndex];
                blendshapeChannelsPerLOD[lod].insert(blendshapeChannel);
            }
        }
    }

    std::uint16_t meshBlendshapeChannelCount = 0;
    for (const auto& [blendshapeChannel, meshIndex] : blendshapeChannelMappings)
    {
        writer->setMeshBlendShapeChannelMapping(meshBlendshapeChannelCount++, meshIndex, blendshapeChannel);
    }

    std::vector<std::uint16_t> numBlendshapesPerLOD(numLODs, 0);
    for (std::uint16_t lod = 0; lod < (std::uint16_t)blendshapeChannelsPerLOD.size(); ++lod)
    {
        std::vector<std::uint16_t> indices(blendshapeChannelsPerLOD[lod].begin(), blendshapeChannelsPerLOD[lod].end());
        writer->setBlendShapeChannelIndices(lod, indices.data(), (std::uint16_t)indices.size());
        writer->setLODBlendShapeChannelMapping(lod, lod);
        numBlendshapesPerLOD[lod] += (std::uint16_t)indices.size();
    }
    writer->setBlendShapeChannelLODs(numBlendshapesPerLOD.data(), (std::uint16_t)numBlendshapesPerLOD.size());
}


// explicitly instantiate the rig geometry classes
template class RigGeometry<float>;
template class RigGeometry<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
