// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/Context.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/Mesh.h>
#include <nls/math/Math.h>
#include <rig/JointRig2.h>
#include <rig/RigLogic.h>
#include <rig/EyelashConnectedVertices.h>

#include <map>

namespace dna
{

class Reader;
class Writer;

} // namespace dna


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! An enum to define whether coordinate system is Local or World
enum class CoordinateSystem
{
    Local,
    World
};

//! RigGeometry implements the rig geometry evaluation based on rig logic inputs.
template <class T>
class RigGeometry
{
public:
    class State;

public:

    struct ErrorInfo
    {
        std::vector<std::pair<std::string, std::vector<int>>> duplicateBlendshapes;
        std::vector<std::pair<std::string, std::vector<int>>> zeroBlendshapes;
        typename JointRig2<T>::ErrorInfo skinningWeightsErrors;
    };

public:
    explicit RigGeometry(bool useMultithreading = true);
    ~RigGeometry();
    RigGeometry(RigGeometry&&);
    RigGeometry(RigGeometry&) = delete;
    RigGeometry& operator=(RigGeometry&&);
    RigGeometry& operator=(const RigGeometry&) = delete;

    std::shared_ptr<RigGeometry> Clone() const;

    //! Initializes RigGeometry with the data from the dna::Reader
    bool Init(const dna::Reader* reader, bool withJointScaling = false, std::map<std::string, ErrorInfo>* errorInfo = nullptr);

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State EvaluateRigGeometry(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd,
                              const std::vector<int>& meshIndices) const;

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State& EvaluateRigGeometry(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd,
                               const std::vector<int>& meshIndices, State& state) const;

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State& EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                          const DiffData<T>& diffJoints,
                                          const std::vector<DiffDataMatrix<T, 3, -1>>& diffBlendshapes,
                                          const std::vector<int>& meshIndices,
                                          const std::vector<Eigen::Ref<const Eigen::Matrix<T, 3, -1>>>& meshNeutrals,
                                          State& state) const;

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State& EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                          const DiffData<T>& diffJoints,
                                          const std::vector<DiffDataMatrix<T, 3, -1>>& diffBlendshapes,
                                          const std::vector<int>& meshIndices,
                                          State& state) const;

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State EvaluateWithPerMeshBlendshapes(const DiffDataAffine<T, 3, 3>& diffRigid,
                                         const DiffData<T>& diffJoints,
                                         const std::vector<DiffDataMatrix<T, 3, -1>>& diffBlendshapes,
                                         const std::vector<int>& meshIndices) const;

    //! @return the @p meshIndex 'th mesh
    const Mesh<T>& GetMesh(int meshIndex) const;

    //! @return the mesh @p meshName
    const Mesh<T>& GetMesh(const std::string& meshName) const;

    //! @return the name of the @p meshIndex 'th mesh
    const std::string& GetMeshName(int meshIndex) const;

    //! @return the index of the mesh with name @p meshName
    int GetMeshIndex(const std::string& meshName) const;

    //! @return True if there is valid data for mesh @p meshIndex
    bool IsValid(int meshIndex) const;

    //! @return the number of meshes
    int NumMeshes() const;

    //! @return all mesh indices that are part of @p lod
    const std::vector<int>& GetMeshIndicesForLOD(int lod) const;

    //! @return all mesh indices that contains blendshape targets
    const std::vector<int>& GetBlendshapeMeshIndices() const;

    //! @return the joint indices that are used on LOD @p lod
    const std::vector<int>& JointIndicesForLOD(int lod) const;

    //! @return the underlying joint rig
    const JointRig2<T>& GetJointRig() const;

    //! @return the current bind matrix of the joint rig.
    Eigen::Matrix<T, 4, 4> GetBindMatrix(int jointIndex) const;

    //! @return the current rest matrix of the joint rig.
    Eigen::Matrix<T, 4, 4> GetRestMatrix(int jointIndex) const;

    //! @return the current rest orientation of all joints in euler angles.
    const Eigen::Matrix<T, 3, -1>& GetRestOrientationEuler() const;

    //! @return the current rest pose of all joints in euler angles.
    const Eigen::Matrix<T, 3, -1>& GetRestPose() const;

    /*
     * @brief Set the rest pose. 
     * @param coordSystem: the coordinate system that the rest pose is provided in. This may be Local (the default) or World
     */ 
    void SetRestPose(Eigen::Matrix<T, 3, -1> restPose, CoordinateSystem coordSystem = CoordinateSystem::Local);

   /*
    * @brief Set the rest orientation (does not update joint deltas, see @p UpdateRestOrientationEuler ) 
    * @param coordSystem: the coordinate system that the rest orientation is provided in. This may be Local (the default) or World 
    * @param bUpdateJointPositionsToPreviousValues: if set to false, then the rest pose is not changed; if set to true then the rest pose is updated so that joint positions are the same as before
    */ 
   void SetRestOrientationEuler(Eigen::Matrix<T, 3, -1> restOrientationEuler, CoordinateSystem coordSystem = CoordinateSystem::Local, bool bUpdateJointPositionsToPreviousValues = false);

   //! Update the rest orientation and also update the joint deltas due to change of the rest orientation
   void UpdateRestOrientationEuler(const Eigen::Matrix<T, 3, -1>& restOrientationEuler, RigLogic<T>& rigLogic);

    /**
     * @brief Makes a blendshape only rig. However, it keeps the eyes as joints.
     */
    void MakeBlendshapeOnly(const RigLogic<T>& rigLogic);

    //! Removes all unused joints from both RigGeometry and RigLogic.
    void RemoveUnusedJoints(RigLogic<T>& rigLogic);

    //! Remove all LODs besides the highest.
    void ReduceToLOD0Only();

    //! Only keep the mesh indices in the the rig. Keeps the removed meshes as empty meshes.
    void ReduceToMeshes(const std::vector<int>& meshIndices);

    //! Remove the blendshapes for meshes @p meshIndices
    void RemoveBlendshapes(const std::vector<int>& meshIndices);

    //! Update the @p meshIndex 'th mesh vertices
    void SetMesh(int meshIndex, const Eigen::Matrix<T, 3, -1>& vertices);

    //! Assembles the mesh at index @p meshIndex stored within dna::Reader
    static Mesh<T> ReadMesh(const dna::Reader* reader, int meshIndex, bool triangulate = false);

    //! Resample the rig geometry such that new vertex i will correspond to previous vertex newToOldMap[i].
    void Resample(int meshIndex, const std::vector<int>& newToOldMap);

    //! @return the name of the head mesh
    std::string HeadMeshName(int lod) const;

    //! @return the name of the teeth mesh
    std::string TeethMeshName(int lod) const;

    //! @return the name of the left eye mesh
    std::string EyeLeftMeshName(int lod) const;

    //! @return the name of the right eye mesh
    std::string EyeRightMeshName(int lod) const;

    //! @return the index of the head mesh
    int HeadMeshIndex(int lod) const;

    //! @return the index of the teeth mesh
    int TeethMeshIndex(int lod) const;

    //! @return the index of the left eye mesh
    int EyeLeftMeshIndex(int lod) const;

    //! @return the index of the right eye mesh
    int EyeRightMeshIndex(int lod) const;

    //! @return for mesh index @p meshIndex, returns the corresponding mesh index at LOD @p lod. -1 if the mesh does not
    // exist.
    int CorrespondingMeshIndexAtLod(int meshIndex, int lod) const;

    //! Returns vertex influence weights as a sparse matrix. Matrix is row major, rows correspond to vertices, and columns to
    // joints
    SparseMatrix<T> VertexInfluenceWeights(const std::string& meshName) const;

    //! Update the rig by translating all elements
    void Translate(const Eigen::Vector3<T>& translation);

    //! @return the eye midpoint position
    Eigen::Vector3<T> EyesMidpoint() const;

    /**
     * @brief Mirror the rig.
     *
     * @param symmetries  The symmetry mapping for meshes that are symmetric. Eyes are handled explicitly.
     * @param meshRoots   The roots of meshes that are used to apply an as-rigid-as-possible deformation based on the head mesh.
     *                    Used to mirror eyelashes as they are not symmetric, hence an acceptable deformation to the new symmetric
     *                    head mesh is required. Roots are defined indices with a weight above 1.
     * @param rigLogic    The riglogic component that also needs to be mirrored.
     */
    void Mirror(const std::map<std::string, std::vector<int>>& symmetries, const std::map<std::string,
                                                                                          std::vector<std::pair<int,
                                                                                                                T>>>& meshRoots, RigLogic<T>& rigLogic);

    //! @returns the symmetric mesh for @p meshName. All meshes are self symmetric besides eyeLeft and eyeRight.
    std::string GetSymmetricMeshName(const std::string& meshName) const;

    //! Cfreate a subdivision version of mesh @p meshIndex replacing the current version of the mesh.
    void CreateSubdivision(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights, int meshIndex);

    //! Copy the mesh of @p otherRig to mesh @p meshIndex of this rig using the blendshapes of this mesh.
    void Transfer(int meshIndex, const RigGeometry<T>& otherRig, int otherMeshIndex);

    //! Save the rig bind pose to the dna binary stream
    void SaveBindPoseToDna(dna::Writer* writer) const;

    //! Save a specific mesh index to the binary stream
    void SaveDna(dna::Writer* writer, int meshIndex) const;

    //! Save the blendshape mapping to the dna binary stream
    void SaveBlendshapeMappingToDna(dna::Writer* writer) const;

    //! Get joint rest pose
    Eigen::Transform<T, 3, Eigen::Affine> GetRestPose(int jointIndex) const;

    const Eigen::Matrix<T, -1, -1>& GetBlendshapeMatrix(int meshIndex) const;

    //! get for each blendshape by which control it is being driven
    const Eigen::VectorXi GetBlendshapePsdControls(int meshIndex) const;

    //! the number of LODs
    std::uint16_t NumLODs() const;

	void CalculateLocalJointTransformsFromWorldTransforms(const std::vector<Affine<T, 3, 3>>& jointWorldTransforms,
		Eigen::Matrix<T, 3, -1>& restPose, Eigen::Matrix<T, 3, -1>& restOrientationEulers) const;

private:
    //! Evaluates the joint deltas and stores it in @p state.
    void EvaluateJointDeltas(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, State& state) const;

    //! Evaluates the joint deltas and stores it in @p state.
    void EvaluateJointDeltasWithoutJacobians(const DiffDataAffine<T, 3, 3>& diffRigid, const DiffData<T>& diffJoints, State& state) const;

    /**
     * Evaluates the mesh vertices of for LOD @p lod and mesh index @p meshIndex.
     * @param diffPsd          The psd coefficients. Typically the output from RigLogic::EvaluatePsd()
     * @param meshIndex        Which mesh to evaluate.
     * @return the evaluated deformed mesh vertices. The output contains Jacobians if either the internal rig has the jacobians set, or if @diffBlendshapes
     * contains the Jacobian.
     */
    void EvaluateBlendshapes(const DiffData<T>& diffPsd, int meshIndex, State& state) const;

    //! evaluates the skinning for geometry @p geometryName with state as input (evaluated blendshape vertices) and output
    // (final vertices).
    void EvaluateSkinningWithoutJacobians(int meshIndex, State& state) const;

    void EvaluateSkinningWithJacobians(int meshIndex, State& state) const;

    void UpdateBindPoses();

    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> GetPreviousBindPose(const Eigen::Matrix<T, 3, -1>& restOrientationEuler) const;

    void UpdateRestPose(const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& prevBindPoses);
 
private:
    struct Private;
    // TITAN_NAMESPACE::Pimpl<Private> m;
    std::unique_ptr<Private> m;
};


//! Rig geometry state containing results of a specific evaluation
template <class T>
class RigGeometry<T>::State
{
public:
    State();
    ~State();
    State(State&&);
    State& operator=(State&&);
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    /**
     * @return const std::vector<DiffDataMatrix<T, 3, -1>>&  the evaluated vertices. See RigGeometry<T>::EvaluateRigGeometry
     * @warning The returned DiffDataMatrices are only valid as long as the State has not been updated by another call to RigGeometry<T>::EvaluateRigGeometry
     * with
     *          the same state. Explanation: the dense matrix of the Jacobian is resized in RigGeometry<T>::EvaluateRigGeometry and therefore the DiffDataMatrix
     * Jacobian
     *          will map to an invalidated dense matrix.
     */
    const std::vector<DiffDataMatrix<T, 3, -1>>& Vertices() const;

    //! @return blendshape deltas in bind pose space
    const Eigen::Matrix<T, 3, -1>& BlendshapeVertices(int meshId) const;

    //! @return the mesh indices that were passed to RigGeometry<T>::EvaluateRigGeometry for evalution
    const std::vector<int>& MeshIndices() const;

    //! @return all world matrices
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& GetWorldMatrices() const { return m_worldMatrices; }

    //! @return the current world matrix of the joint rig.
    Eigen::Matrix<T, 4, 4> GetWorldMatrix(int jointIndex) const;

    //! @return the current local matrix of the joint rig.
    Eigen::Matrix<T, 4, 4> GetLocalMatrix(int jointIndex) const;

    //! Set the current local matrix of the joint rig.
    void SetLocalMatrix(int jointIndex, const Eigen::Matrix<T, 4, 4>& jointTransform);

    //! @return the current skinning matrix of the joint rig
    Eigen::Matrix<T, 4, 4> GetSkinningMatrix(int jointIndex) const;

    //! @return the current skinning matrices for all joints
    std::vector<Eigen::Matrix<T, 4, 4>> GetAllSkinningMatrices() const;

    //! std::move the vertices from the state
    std::vector<DiffDataMatrix<T, 3, -1>> MoveVertices();

private:
    //! setup the state to support adding data for mesh @p meshIndex
    void SetupForMesh(int meshIndex);

private:
    //! the evaluated vertices
    std::vector<DiffDataMatrix<T, 3, -1>> m_vertices;

    //! vector containing mapping mesh indices to the evaluated vertices
    std::vector<int> m_meshIndices;

    //! flag specifying whether the state has been set including Jacobians
    bool m_withJacobians = false;

    //! the evaluated local transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> m_localMatrices;

    //! the evaluated world transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> m_worldMatrices;

    //! the evaluated skinning transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> m_skinningMatrices;

    //! local caching of the jacobians for the joints
    int m_jointJacobianColOffset = -1;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> m_jointDeltasJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> m_localMatricesJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> m_worldMatricesJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> m_skinningMatricesJacobian;

    struct JacobianData
    {
        //! blendshapes as they were evaluated
        Eigen::Matrix<T, 3, -1> blendshapeVertices;
        std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> blendshapeJacobianRM = std::make_shared<Eigen::Matrix<T,
                                                                                                                         -1,
                                                                                                                         -1,
                                                                                                                         Eigen::RowMajor>>();
        int blendshapeJacobianColOffset = -1;

        //! final vertices after applying the joint evaluation
        Eigen::Matrix<T, 3, -1> finalVertices;
        std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> finalJacobianRM = std::make_shared<Eigen::Matrix<T, -1,
                                                                                                                    -1,
                                                                                                                    Eigen::RowMajor>>();
        int finalJacobianColOffset = -1;
    };

    //! structure containing temporary data for jacobian calculations of the blendshapes
    std::vector<JacobianData> m_meshJacobianData;

    friend class RigGeometry<T>;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
