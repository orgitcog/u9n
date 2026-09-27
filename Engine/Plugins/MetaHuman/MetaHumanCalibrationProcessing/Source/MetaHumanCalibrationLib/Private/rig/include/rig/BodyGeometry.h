// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffDataMatrix.h>
#include <nls/geometry/Mesh.h>
#include <nls/math/Math.h>

#include <map>

namespace dna {
class Reader;
}


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! BodyGeometry implements the rig geometry evaluation based on rig logic inputs.
template <class T>
class BodyGeometry
{
public:
    class State;

public:
	explicit BodyGeometry(const std::shared_ptr<TaskThreadPool>& taskThreadPool);
	explicit BodyGeometry(bool useMultithreading = true);
    BodyGeometry(int numLods, bool useMultithreading = true);
    ~BodyGeometry() = default;
    BodyGeometry(BodyGeometry&&) = default;
    BodyGeometry(BodyGeometry&) = default;
    BodyGeometry& operator=(BodyGeometry&&) = default;
    BodyGeometry& operator=(const BodyGeometry&) = default;

    void SetThreadPool(const std::shared_ptr<TaskThreadPool>& taskThreadPool);

    std::shared_ptr<BodyGeometry> Clone() const;

    void SetNumLODs(const int l);
    int GetNumLODs() const;

    //! Initializes BodyGeometry with the data from the dna::BinaryStreamReader
    bool Init(const dna::Reader* reader, bool computeMeshNormals = false);
    //! Assembles the mesh at index @p meshIndex stored within dna::BinaryStreamReader
    static Mesh<T> ReadMesh(const dna::Reader* reader, int meshIndex);

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State EvaluateBodyGeometry(const int lod, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd) const;

    //! Sets the joints and evaluates the mesh vertices for LOD @p lod and mesh indices @p meshIndices.
    State& EvaluateBodyGeometry(const int lod, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, State& state) const;
    State& EvaluateIndexedBodyGeometry(const int lod, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, const std::vector<int>& indices, State& state) const;
    State& EvaluateBodyGeometryWithOffset(const int lod, const Eigen::Matrix3X<T>& offset, const DiffData<T>& diffJoints, const DiffData<T>& diffPsd, State& state) const;

    //! @return the mesh
    const Mesh<T>& GetMesh(const int lod) const { return mesh[lod]; }
    Mesh<T>& GetMesh(const int lod) { return mesh[lod]; }

    // get num components
    int NumJoints() const { return int(jointNames.size()); }
    int NumBlendshapes(int lod) const { return static_cast<int>(blendshapeControlsToMeshBlendshapeControls[lod].size()); }

    // get joint names
    const std::vector<std::string>& GetJointNames() const { return jointNames; }
    std::vector<std::string>& GetJointNames() { return jointNames; }

    // get joint index by name
    int GetJointIndex(const std::string& jointName) const;

    // get blendshape data
    const Eigen::VectorXi& GetBlendshapeMap(const int lod) const { return blendshapeControlsToMeshBlendshapeControls[lod]; }
    Eigen::VectorXi& GetBlendshapeMap(const int lod) { return blendshapeControlsToMeshBlendshapeControls[lod]; }
    const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& GetBlendshapeMatrix(const int lod) const { return blendshapeMatrixDense[lod]; }
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor>& GetBlendshapeMatrix(const int lod) { return blendshapeMatrixDense[lod]; }

    // get joint parent indices
    const std::vector<int>& GetJointParentIndices() const { return jointParentIndices; }
    std::vector<int>& GetJointParentIndices() { return jointParentIndices; }
    int GetParentIndex(int jointIndex) const;

    //! @return the current bind matrix of the joint rig.
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& GetBindMatrices() const { return jointBindPoses; }

    const Eigen::Matrix3X<T>& GetJointRestPoses() const { return jointRestPose; }
    Eigen::Matrix3X<T>& GetJointRestPoses() { return jointRestPose; }

    const std::vector<Eigen::Matrix<T, 3, 3>>& GetJointRestOrientation() const { return jointRestOrientation; }
    std::vector<Eigen::Matrix<T, 3, 3>>& GetJointRestOrientation() { return jointRestOrientation; }

    const SparseMatrix<T>& GetVertexInfluenceWeights(const int lod) const { return vertexInfluenceWeights[lod]; }
    SparseMatrix<T>& GetVertexInfluenceWeights(const int lod) { return vertexInfluenceWeights[lod]; }

    void UpdateBindPoses();

private:
    //! Evaluates the joint deltas and stores it in @p state.
    void EvaluateJointDeltas(const DiffData<T>& diffJoints, State& state) const;

    //! Evaluates the joint deltas and stores it in @p state.
    void EvaluateJointDeltasWithoutJacobians(const DiffData<T>& diffJoints, State& state) const;

    //! Get original joint rest poses from state and pose
    const std::pair<Eigen::Matrix<T, 3, -1>, std::vector<Eigen::Matrix<T, 3, 3>>> EvaluateInverseJointDeltas(const DiffData<T>& diffJoints, const State& state) const;

    /**
     * Evaluates the mesh vertices of for LOD @p lod and mesh index @p meshIndex.
     * @param diffPsd          The psd coefficients. Typically the output from RigLogic::EvaluatePsd()
     * @param lod              The lod for which to evalute the vertices.
     * @param meshIndex        Which mesh to evaluate.
     * @return the evaluated deformed mesh vertices. The output contains Jacobians if either the internal rig has the jacobians set, or if @diffBlendshapes contains the Jacobian.
     */
    void EvaluateBlendshapes(const int lod, const DiffData<T>& diffPsd, State& state) const;
    void EvaluateIndexedBlendshapes(const int lod, const DiffData<T>& diffPsd, State& state, const std::vector<int>& indices) const;

    //! evaluates the skinning for geometry @p geometryName with state as input (evaluated blendshape vertices) and output (final vertices).
    void EvaluateSkinningWithoutJacobians(const int lod, State& state) const;
    void EvaluateSkinningWithJacobians(const int lod, State& state) const;

    //! perform inverse skinning for a given state plus optional mesh
    const Eigen::Matrix3X<T> EvaluateInverseSkinning(const int lod, const State& state, const Eigen::Matrix3X<T>& vertices);

    //! evaluates the skinning only for the given vertex indices for geometry @p geometryName with state as input (evaluated blendshape vertices) and output (final vertices).
    void EvaluateIndexedSkinningWithoutJacobians(const int lod, State& state, const std::vector<int>& indices) const;
    void EvaluateIndexedSkinningWithJacobians(const int lod, State& state, const std::vector<int>& indices) const;

private:
    //! the mesh topology
    std::vector<Mesh<T>> mesh;

    //! selection of which blendshapes are used by this mesh
    std::vector<Eigen::VectorXi> blendshapeControlsToMeshBlendshapeControls;
    std::vector<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> blendshapeMatrixDense;

    // skinning data
    std::vector<SparseMatrix<T>> vertexInfluenceWeights;

    // joint data
    std::vector<std::string> jointNames;
    std::vector<int> jointParentIndices;

    Eigen::Matrix<T, 3, -1> jointRestPose;
    std::vector<Eigen::Matrix<T, 3, 3>> jointRestOrientation;

    // bind pose
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointBindPoses;
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> jointInverseBindPoses;

    std::shared_ptr<TaskThreadPool> taskThreadPool{};
};


//! Rig geometry state containing results of a specific evaluation
template <class T>
class BodyGeometry<T>::State
{
public:
    State() {};
    ~State() = default;
    State(State&&) = default;
    State& operator=(State&&) = default;
    State(const State&) = default;
    State& operator=(const State&) = default;

    /**
     * @return const std::vector<DiffDataMatrix<T, 3, -1>>&  the evaluated vertices. See BodyGeometry<T>::EvaluateBodyGeometry
     * @warning The returned DiffDataMatrices are only valid as long as the State has not been updated by another call to BodyGeometry<T>::EvaluateBodyGeometry with
     *          the same state. Explanation: the dense matrix of the Jacobian is resized in BodyGeometry<T>::EvaluateBodyGeometry and therefore the DiffDataMatrix Jacobian
     *          will map to an invalidated dense matrix.
     */
    const DiffDataMatrix<T, 3, -1>& Vertices() const { return vertices; }

    //! @return the current world matrix of the joint rig.
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& GetWorldMatrices() const { return worldMatrices; }

    //! @return the current local matrix of the joint rig.
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& GetLocalMatrices() const { return localMatrices; }

    //! @return the current skinning matrix of the joint rig.
    const std::vector<Eigen::Transform<T, 3, Eigen::Affine>>& GetSkinningMatrices() const { return skinningMatrices; }

    //! @return the blendshape coefficients
    const Vector<T>& GetBlendshapeCoefficients() const { return diffMeshBlendshapes.Value(); }

    //! @return the blendshape mesh of index i
    const Eigen::Matrix3X<T>& GetBlendshapeVertices() const { return blendshapeVertices; }

    //! @return the blendshape mesh jacobian
    const std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>>& GetBlendshapeJacobian() const { return blendshapeJacobianRM; }

private:
    //! the evaluated vertices
    DiffDataMatrix<T, 3, -1> vertices = DiffDataMatrix<T, 3, -1>(Eigen::Matrix<T, 3, -1>());

    //! flag specifying whether the state has been set including Jacobians
    bool withJacobians = false;

    //! the evaluated local transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> localMatrices;

    //! the evaluated world transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> worldMatrices;

    //! the evaluated skinning transformations for each joint
    std::vector<Eigen::Transform<T, 3, Eigen::Affine>> skinningMatrices;

    //! the evaluated blendshape weights
    DiffData<T> diffMeshBlendshapes = DiffData<T>(Eigen::VectorX<T>());

    //! local caching of the jacobians for the joints
    int jointJacobianColOffset = -1;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> jointDeltasJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> localMatricesJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> worldMatricesJacobian;
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> skinningMatricesJacobian;

    //! blendshapes as they were evaluated
    Eigen::Matrix<T, 3, -1> blendshapeVertices;
    std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> blendshapeJacobianRM = std::make_shared<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>>();
    int blendshapeJacobianColOffset = -1;

    //! final vertices after applying the joint evaluation
    Eigen::Matrix<T, 3, -1> finalVertices;
    std::shared_ptr<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> finalJacobianRM = std::make_shared<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>>();
    int finalJacobianColOffset = -1;

    friend class BodyGeometry<T>;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)