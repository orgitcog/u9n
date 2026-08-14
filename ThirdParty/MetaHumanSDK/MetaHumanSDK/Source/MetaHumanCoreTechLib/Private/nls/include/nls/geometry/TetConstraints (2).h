// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <nls/geometry/VertexConstraints.h>
#include <nls/functions/SubtractFunction.h>

#include <vector>
#include <unordered_set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Estimate the closest rotation @p R based on input transformation @p F. If dRdF is not-zero then it will contain the derivative of R with respect to F.
template <class T>
void FtoR(const Eigen::Matrix<T, 3, 3>& F, Eigen::Matrix<T, 3, 3>& R, Eigen::Matrix<T, 9, 9>* dRdF);

/**
 * Various tetrahedral contraints including strain hyperelasticity, volume preservation, and the deformation gradient.
 */
template <class T>
class TetConstraints
{
public:
    enum ElasticityModel
    {
        Linear = 0, Corotated = 1, NeoHookean = 2
    };

    TetConstraints() = default;
    TetConstraints(const TetConstraints& o) = delete;
    TetConstraints& operator=(const TetConstraints& o) = delete;

    //! Sets the topology of the tetrahedral mesh.
    void SetTopology(const Eigen::Matrix<int, 4, -1>& tets) { m_tets = tets; }

    //! Set the rest pose of the tetrahedral mesh.
    void SetRestPose(const Eigen::Matrix<T, 3, -1>& vertices, bool allowInvertedTets = false);

    //! Set mask for the tets.
    // it is different from fixing vertices in a MatrixVariable as
    // boundary primitives would still be evaluated.
    //
    // (mask > 0) -> the tet is not optimized
    // (mask <= 0) -> the tet is optimized
    void SetTetsMask(const Eigen::Vector<int, -1>& mask);

    //! Delete the mask for the tets. All the tets will be optimized.
    void ClearTetsMask()
    {
        m_mask = Eigen::Vector<int, -1>(0);
    }

    //! @returns the number of tetraheda.
    int NumTets() const { return int(m_tets.cols()); }

    const std::vector<Eigen::Matrix<T, 3, 3>>& InvRestFrame() const { return m_invRestFrame; }
    const std::vector<T>& SqrtRestVolume() const { return m_sqrtRestVolume; }

    /**
     * Tetrahedral strain hyperelasticity.
     * Evaluates residual function r(x) and its Jacobian, e.g.
     * - corotated: r(x) = F(x) - R(F(x))
     * - Neo Hookean: r(x) = sqrt(trace(F^T * F)) - sqrt(3)
     */
    DiffData<T> EvaluateStrain(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight, ElasticityModel elModel = Corotated) const;

    //! Setup strain constraints in @p vertexConstraints.
    // It is not compatible with pre-stretch
    void SetupStrain(const Eigen::Matrix<T, 3, -1>& vertices, const T strainWeight, VertexConstraintsExt<T, 9, 4>& vertexConstraints) const;

    /**
     * Tetrahedral strain hyperelasticity.
     * Evaluates residual function r(x) and its Jacobian, e.g.
     * - linear elasticity: r(x) = F(x) - I
     * - corotated: r(x) = F(x) - R (where R is fixed to the rotation closest to F).
     */
    DiffData<T> EvaluateStrainLinearProjective(
        const DiffDataMatrix<T, 3, -1>& vertices, 
        const T strainWeight, 
        ElasticityModel elModel = Corotated,
        T minRange = T(1),
        T maxRange = T(1)) const;

    /**
     * Tetrahedral volume loss.
     * Evaluates residual function r(x) and its Jacobian, e.g. r(x) = det(F(x)) - 1
     */
    DiffData<T> EvaluateVolumeLoss(
        const DiffDataMatrix<T, 3, -1>& vertices, 
        T volumeWeight) const;

    /**
     * Tetrahedral volume loss.
     * Evaluates residual function r(x) and its Jacobian, e.g. r(x) = F(x) - F' (where F' is fixed to the volume preserving deformation closest to F)
     */
    DiffData<T> EvaluateVolumeLossProjective(
        const DiffDataMatrix<T, 3, -1>& vertices, 
        T volumeWeight,
        T minRange = T(1),
        T maxRange = T(1)) const;

    //! @returns F(x)
    DiffDataMatrix<T, 9, -1> EvaluateDeformationGradient(const DiffDataMatrix<T, 3, -1>& vertices,
                                                         bool volumeWeighted = true,
                                                         const std::vector<T>& perTetWeight = std::vector<T>()) const;

    /**
     * Tetrahedral strain hyperelasticity evaluated in terms of activation.
     * Evaluates residual function r(x) and its Jacobian, e.g.
     * - r(x) = F(x) - R(x) * A
     */
    DiffData<T> EvaluateStrainActivation(
        const DiffDataMatrix<T, 3, -1>& vertices, 
        const Eigen::Matrix<T, 9, -1>& activations,
        const T strainWeight) const;

    /**
     * Evaluate residual and Jacobian between deformation gradient
     * and desired gradient value.
     */
    DiffDataMatrix<T, 9, -1> EvaluateDeformationGradientLossProjective(
        const DiffDataMatrix<T, 3, -1>& vertices,
        const Eigen::Matrix<T, 9, -1>& targetGradients,
        bool volumeWeighted) const;

    /**
     * Evaluate gravity potential per-vertex
     * R(x) = mass * acceleration * location[hAxis]
     * 
     * The constraint is undefined for points below the zero plane.
     * In the current implementation, points under the zero plane are given zero potential
    */
    DiffData<T> EvaluateGravityPotential(
        const DiffDataMatrix<T, 3, -1>& vertices, 
        const T acceleration,
        const T density,
        const int hAxis = 1) const;

    /**
     * Evaluates the Cauchy-Green tensor, e.g.
     * r(x) = 0.5 * (F^T * F - I)
     * 
     * It is useful for color coding the strain over a mesh.
     * 
     * The current implementation does not provide the Jacobian.
     */
    Eigen::VectorX<T> EvaluateCauchyGreenStrainTensor(
        const DiffDataMatrix<T, 3, -1>& vertices) const;


private:
    /**
     * Tetrahedral strain hyperelasticity.
     * Evaluates residual function r(x) and its Jacobian, e.g.
     * - corotated: r(x) = F(x) - R(F(x))
    */
    DiffData<T> EvaluateStrainCorotated(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight) const;

    /**
     * Tetrahedral strain hyperelasticity.
     * Evaluates residual function r(x) and its Jacobian, e.g.
     * - Neo Hookean: r(x) = sqrt(trace(F^T * F)) - sqrt(3)
    */
    DiffData<T> EvaluateStrainNH(const DiffDataMatrix<T, 3, -1>& vertices, const T strainWeight) const;

    /**
     * Evaluate elastic deformation gradient for one tet.
     * F_elastic = F_geometry * F_rest^-1
     * The elastic deformation gradient is employed in the elastic strain evaluation
    */
    Eigen::Matrix<T, 3, 3> EvaluateDeformationGradient(const DiffDataMatrix<T, 3, -1>& vertices, const int* v, const int t) const;

    int m_numVertices;
    Eigen::Matrix<int, 4, -1> m_tets;
    std::vector<Eigen::Matrix<T, 3, 3>> m_invRestFrame;
    std::vector<T> m_sqrtRestVolume;
    std::unordered_set<ElasticityModel> validElModels = {Linear, Corotated, NeoHookean};
    Eigen::Vector<int, -1> m_mask;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
