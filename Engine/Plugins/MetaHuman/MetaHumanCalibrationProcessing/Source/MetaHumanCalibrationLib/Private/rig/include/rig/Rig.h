// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <rig/RigLogic.h>
#include <rig/RigGeometry.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Class combining RigLogic, RigGeometry, and the default values.
 */
template <class T>
class Rig
{
public:
    Rig();
    ~Rig();

    Rig(const std::shared_ptr<const RigLogic<T>>& rigLogic, const std::shared_ptr<const RigGeometry<T>>& rigGeometry);

    /**
     * @brief Loads the rig from @p DNAFilename.
     *
     * @param DNAFilename The name of the DNA file.
     * @param withJointScaling If False, then any joint scaling that is part of the DNA is discarded. Currently only pupils have scaling information.
     * @return true   If DNA file could be read.
     * @return false  If any error occurred during reading.
     */
    bool LoadRig(const std::string& DNAFilename, bool withJointScaling = false, std::map<std::string, typename RigGeometry<T>::ErrorInfo>* errorInfo = nullptr);

    /**
     * @brief Loads the rig from @p DNAStream.
     *
     * @param DNAStream The DNA stream.
     * @param withJointScaling If False, then any joint scaling that is part of the DNA is discarded. Currently only pupils have scaling information.
     * @return true   If DNA file could be read.
     * @return false  If any error occurred during reading.
     */
    bool LoadRig(const dna::Reader* DNAStream, bool withJointScaling = false, std::map<std::string, typename RigGeometry<T>::ErrorInfo>* errorInfo = nullptr);

    //! Tests whether the rig adheres to \p topology
    bool VerifyTopology(const Mesh<T>& topology) const;

    const std::shared_ptr<const RigLogic<T>>& GetRigLogic() const { return m_rigLogic; }
    const std::shared_ptr<const RigGeometry<T>>& GetRigGeometry() const { return m_rigGeometry; }
    const std::vector<std::string>& GetGuiControlNames() const;
    const std::vector<std::string>& GetRawControlNames() const;

    const Mesh<T>& GetBaseMesh() const { return m_baseMesh; }
    const Mesh<T>& GetBaseMeshTriangulated() const { return m_baseMeshTriangulated; }

    //! Update the base mesh triangulation using @p triangulatedMesh
    void UpdateBaseMeshTriangulation(const Mesh<T>& triangulatedMesh);

    enum class ControlsType
    {
        GuiControls,
        RawControls
    };

    //! Evaluates the vertices of multiple meshes
    std::vector<Eigen::Matrix<T, 3, -1>> EvaluateVertices(const Eigen::VectorX<T>& controls,
                                                          int lod,
                                                          const std::vector<int>& meshIndices,
                                                          ControlsType controlsType = ControlsType::GuiControls) const;

    //! Evaluates the vertices of multiple meshes
    void EvaluateVertices(const Eigen::VectorX<T>& controls,
                          int lod,
                          const std::vector<int>& meshIndices,
                          typename RigGeometry<T>::State& state,
                          ControlsType controlsType = ControlsType::GuiControls) const;

    //! Convert joint-based shapes into blendshape-only shapes. Does not create blendshapes for the eye geometry.
    void MakeBlendshapeOnly();

    //! Only keep the highest resolution mesh
    void ReduceToLOD0Only();

    /**
     * @brief Reduce the rig to only contain the meshes @p meshIndices
     * Removes unused joints in the rig.
     */
    void ReduceToMeshes(const std::vector<int>& meshIndices);

    //! Remove the blendshapes for meshes @p meshIndices
    void RemoveBlendshapes(const std::vector<int>& meshIndices);

    //! Simplify the rig to only support @p guiControls. All other gui controls will be ignored even if the are non-zero.
    void ReduceToGuiControls(const std::vector<int>& guiControls);

    //! Resample the rig such that for each mesh, a new vertex i will correspond to previous vertex vertexIndices[i].
    void Resample(const std::map<int, std::vector<int>>& vertexIndicesMap);

    //! Update the @p meshIndex 'th mesh vertices
    void SetMesh(int meshIndex, const Eigen::Matrix<T, 3, -1>& vertices);

    void Translate(const Eigen::Vector3<T>& translation);

    //! Mirror the rig horizontally
    void Mirror(const std::map<std::string, std::vector<int>>& symmetries, const std::map<std::string, std::vector<std::pair<int, T>>>& meshRoots);

    //! Update the rest orientation and also update the joint deltas due to change of the rest orientation
    void UpdateRestOrientationEuler(const Eigen::Matrix<T, 3, -1>& restOrientationEuler);

    //! Update the rest pose
    void UpdateRestPose(const Eigen::Matrix<T, 3, -1>& restPose, CoordinateSystem coordinateSystem);

    //! Create a subdivision version of mesh @p meshIndex
    void CreateSubdivision(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights, int meshIndex);

private:
    std::shared_ptr<const RigLogic<T>> m_rigLogic;
    std::shared_ptr<const RigGeometry<T>> m_rigGeometry;

    //! The base mesh (quads)
    Mesh<T> m_baseMesh;

    //! the base mesh (triangulated)
    Mesh<T> m_baseMeshTriangulated;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
