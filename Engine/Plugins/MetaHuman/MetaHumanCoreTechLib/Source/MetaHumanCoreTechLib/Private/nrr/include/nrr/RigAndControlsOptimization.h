// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/math/Math.h>
#include <nls/geometry/Affine.h>
#include <rig/RigGeometry.h>
#include <rig/RigLogic.h>
#include <dna/BinaryStreamReader.h>
#include <nrr/VertexWeights.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class RigAndControlsOptimization
{
public:
    RigAndControlsOptimization();
    ~RigAndControlsOptimization();
    RigAndControlsOptimization(RigAndControlsOptimization&& other);
    RigAndControlsOptimization(const RigAndControlsOptimization& other) = delete;
    RigAndControlsOptimization& operator=(RigAndControlsOptimization&& other);
    RigAndControlsOptimization& operator=(const RigAndControlsOptimization& other) = delete;

    /**
     * Set RigGeometry and RigLogic using a dna binary stream reader.
     * @param dnaStream  dna binary file stream
     */
    void SetRig(dna::BinaryStreamReader* dnaStream);

    /**
     * Set topology
     * @param mesh  map of expected mesh topologies
     */
    void SetMeshes(std::vector<std::string> meshNames);

    /**
     * Set active GUI controls
     * @param activeControls GUI names of active controls
     */
    void SetActiveControls(std::vector<std::string> activeControls);

    /**
     * Runtime joint rig optimization.
     * @param targetGeometry           The definition of the target geometry
     * @param vertexWeights              The weight of each vertex in the loss function
     * @param rotationRegularization     Regularization on the delta of the joint rotation relative to the rest rig state.
     * @param translationRegularization  Regularization of the delta of the joint translation relative to the rest rig state.
     * @param strainWeight               How much to regularize strain between the evaluated vertices and the target vertices
     * @param bendingWeight              How much to regularize bending between the evaluated vertices and the target vertices
     * @param numIterations              The number of iterations for the optimization
     */
    void OptimizeRigAndControls(std::map<std::string, Eigen::Matrix<T, 3, -1>> targetGeometry,
                                Eigen::VectorX<T> guiControls,
                                std::map<std::string, Vector<T>> vertexWeights,
                                T strainWeight,
                                T bendingWeight,
                                int numIterations = 20);

    //! Get the current controls
    Eigen::VectorX<T> GetControls();

    //! Get the current joint matrix parameters
    Eigen::VectorX<T> GetJointMatrixNonzeros();

    //! Get the joint deltas from the current state
    Eigen::VectorX<T> GetJointState();

    //! Get the joint deltas from the current state
    SparseMatrix<T> GetJointMatrixState();

    //! Get the joint deltas from a new set of rig parameters
    // or maybe just evaluate the joint deltas for every control in the psd set and hope that the length is the same as the length of the rigCalibData->GetExpressionNames(); but that's pca stuff idkkkkkkk

    //! Sets the degrees of freedom: either optimize controls, joint matrix nonzeros, or both
    void SetDegreesOfFreedom(bool jointMatrix, bool controls);

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};


/**
 * @brief RigAndControlsFitting class parameters for joints fitting
 */
struct RigAndControlsFittingParams
{
    bool m_fixJointMatrix = false; //!< bool to indicate should the optimization fixes the joint matrix
    bool m_fixPsdControls = true; //!< bool to indicate should the optimization fix the psd controls
    int m_numIterations = 2; //!< number of iterations for the joints fitting optimizer
    float m_bendingWeight = 0.0f; //!< joints bending constraint weight
    float m_strainWeight = 0.0f; //!< joints strain constraint weight
};

/**
 * @brief Joint rig fitting class. Can be used to calculate joint deltas needed to fit a geometry. Works only on head mesh on LOD0
 *
 * JointRigOptimizer class is used to calculate the joints translations and rotations that will deform the input geometry
 * to match the deformed one. Gauss-Newton optimizer is used.
 *
 * @note Works only on head mesh on LOD0
 */
template <class T>
class RigAndControlsFitting
{
private:
    std::unique_ptr<RigAndControlsOptimization<T>> m_pRigOptimization; //!< Model used to optimize the rig and controls with the goal of
                                                                       // minimizing the difference
                                                                       //!< compared to the target geometry

    dna::BinaryStreamReader* m_pDNAResource; //!< DNA file used to initialize the joints optimization

public:
    RigAndControlsFitting() = default;
    ~RigAndControlsFitting() = default;

    RigAndControlsFitting(RigAndControlsFitting&&) = delete;
    RigAndControlsFitting(RigAndControlsFitting&) = delete;
    RigAndControlsFitting& operator=(RigAndControlsFitting&&) = delete;
    RigAndControlsFitting& operator=(const RigAndControlsFitting&) = delete;

    /**
     * @brief Setup the joint rig optimization model
     *
     * @param[in] dnaResource        - DNA resource that is used to setup the joint optimization
     * @param[in] rigGeometry        - Rig geometry model that is used to setup the joint optimization
     * @param[in] currentRigState    - Rig state used to get the
     */
    void Setup(dna::BinaryStreamReader* dnaResource,
               std::vector<std::string> activeControls);

    /**
     * @brief Optimize the joint matrix and/or PSD controls so that the resulting mesh is the closes (w.r.t. given constraints) to the target geometry
     *
     * @param[in] targetGeometry    - Target geometry to fit to
     * @param[in] fittingMask       - Mask that selects the vertices that can be moved by the optimization
     * @param[in] guiControls       - GUI controls to initialize the optimization with
     * @param[in] params            - Joints optimization parameters
     *
     * @return Updated joint matrix
     */
    SparseMatrix<T> Optimize(const Eigen::Matrix<T, 3, -1>& targetGeometry,
                             const VertexWeights<T>& fittingMask,
                             const Eigen::VectorX<T> guiControls,
                             const RigAndControlsFittingParams& params);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
