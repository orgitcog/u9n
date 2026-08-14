// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/utils/TaskThreadPool.h>
#include <nls/geometry/Procrustes.h>
#include <nrr/PatchBlendModel.h>
#include <nrr/VertexWeights.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct NeutralPoseFittingParams
{
    //! @regularization
    T modelRegularization { T(0.1) };

    //! @patch smoothness
    T patchSmoothness { T(0.5) };

    //! @point-to-point weight
    T p2pWeight { T(1) };

    //! @whether to optimize for scale along with mode weights
    bool modelFitOptimizeScale { false };

    //! @whether to optimize global rigid transform in model fit
    bool modelFitOptimizeRigid { true };

    //! @whether to optimize scale in rigid fit
    bool rigidFitOptimizeScale { true };

    //! @whether to optimize translation in rigid fit
    bool rigidFitOptimizeTranslation { true };

    //! @whether to optimize rotation in rigid fit
    bool rigidFitOptimizeRotation { true };

    //! @number of iterations
    int numIterations { 5 };

    //! @fixed region
    int fixedRegion { -1 };

    void SetFromConfiguration(const Configuration& config);

    Configuration ToConfiguration() const;
};

template <class T>
class NeutralPoseFittingOptimization
{
public:
    //! @solve to target vertices
    static Affine<T, 3, 3> RegisterPose(const Eigen::Matrix<T, 3, -1>& targetData,
                                        const Eigen::VectorXi& targetToModelDataMapping,
                                        const Affine<T, 3, 3> toTargetTransform,
                                        const std::shared_ptr<PatchBlendModel<T>>& model,
                                        PatchBlendModel<T>::OptimizationState& modelState,
                                        const NeutralPoseFittingParams<T>& params,
                                        const Eigen::VectorX<T>& mask);

    static std::pair<T, Affine<T, 3, 3>> RegisterPose(const Eigen::Matrix<T, 3, -1>& targetData,
                                                      const std::shared_ptr<IdentityBlendModel<T>>& model,
                                                      const NeutralPoseFittingParams<T>& params,
                                                      const Eigen::VectorX<T>& mask,
                                                      Eigen::VectorX<T>& result);
};

//! fast fitting of the patch model using projection
template <class T>
class FastPatchModelFitting
{
public:
    struct Settings
    {
        bool withScale = true;
        int fixedRegion = 19; // neck (TODO: specify by name?)
        T regularization = T(1);
    };

public:
    void Init(const std::shared_ptr<const PatchBlendModel<T>>& patchModel, const std::shared_ptr<TaskThreadPool>& taskThreadPool);

    void UpdateMask(const VertexWeights<T>& mask);

    void UpdateRegularization(T regularization);

    void Fit(PatchBlendModel<T>::State& state, const Eigen::Matrix3X<T>& vertices, const Settings& settings);

    Eigen::VectorX<T> FitRegion(int regionIndex, const Eigen::Matrix3X<T>& vertices) const;

private:
    void UpdateSolveData();

private:
    std::shared_ptr<const PatchBlendModel<T>> m_patchModel;

    std::shared_ptr<TaskThreadPool> m_taskThreadPool;

    //! regularization
    T m_regularization = T(1);

    //! weights
    VertexWeights<T> m_mask;

    //! solve data per region
    struct SolveData {
        std::vector<int> vIDs;
        Eigen::Matrix<T, 3, -1> base;
        Eigen::MatrixX<T> AtAinvAt;
    };

    //! precalculated solve data
    std::vector<SolveData> m_solveData;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
