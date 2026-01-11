// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nrr/IdentityBlendModel.h>
#include <nls/geometry/Procrustes.h>
#include <rig/RigGeometry.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct SculptFittingParams
{
    //! @regularization
    T modelRegularization { T(0.2) };

    //! @point-to-point weight
    T p2pWeight { T(1) };

    //! @whether to optimize global rigid transform
    bool fitRigid = false;

    //! @whether to use blendshapes in the optimization
    bool fitBlendshapes = true;

    //! @number of iterations
    int numIterations { 3 };
};

template <class T>
struct SculptFittingState
{
    Affine<T, 3, 3> toTargetTransform;
    Eigen::VectorX<T> pcaParameters;
};

template <class T>
class SculptFittingOptimization
{
public:

    //! @solve to target vertices
    static SculptFittingState<T> RegisterPose(const Eigen::Matrix<T, 3, -1>& targetVertices,
                                              const SculptFittingState<T>& previousState,
                                              const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                              const std::shared_ptr<IdentityBlendModel<T>> model,
                                              const SculptFittingParams<T>& params,
                                              const Eigen::VectorX<T>& mask);

    //! @solve to target vertices
    static SculptFittingState<T> RegisterPose(const std::map<std::string, Eigen::Matrix<T, 3, -1>>& targetVertices,
                                              const std::map<std::string, Eigen::VectorX<T>>& masks,
                                              const std::map<std::string, std::pair<int, int>>& targetToModelMapping,
                                              const SculptFittingState<T>& previousState,
                                              const std::shared_ptr<RigGeometry<T>>& rigGeometry,
                                              const std::shared_ptr<IdentityBlendModel<T>> model,
                                              const SculptFittingParams<T>& params);

    //! @solve to target vertices, while projecting the parameters to the linear model defined with
    static SculptFittingState<T> RegisterPoseProjectToLinear(const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
                                                             const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
                                                             const SculptFittingState<T> &previousState,
                                                             const std::shared_ptr<RigGeometry<T>> &rigGeometry,
                                                             const std::shared_ptr<IdentityBlendModel<T>> model,
                                                             const Eigen::Matrix<T, -1, -1> &linearModel,
                                                             const Eigen::Vector<T, -1> &linearModelMean,
                                                             const SculptFittingParams<T> &params);

    //! @solve to target vertices, for multiple expressions, while projecting the parameters to the linear model
    static std::vector<SculptFittingState<T>> RegisterMultiplePosesProjectToLinear(
        const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
        const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
        const std::vector<SculptFittingState<T>> &previousStates,
        const std::shared_ptr<RigGeometry<T>> &rigGeometry,
        const std::vector<std::shared_ptr<IdentityBlendModel<T>>> models,
        const std::vector<Eigen::Matrix<T, -1, -1>> &linearModels,
        const std::vector<Eigen::Vector<T, -1>> &linearModelsMeans,
        const SculptFittingParams<T> &params);

    //! @solve to target vertices, for multiple expressions, while projecting the parameters to the linear model, per region (increases number of DOF)
    static std::vector<SculptFittingState<T>> RegisterMultiplePosesPerRegionProjectToLinear(
        const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
        const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
        const std::vector<SculptFittingState<T>> &previousStates,
        const std::shared_ptr<RigGeometry<T>> &rigGeometry,
        const std::vector<std::shared_ptr<IdentityBlendModel<T>>> models,
        const std::vector<std::vector<Eigen::Matrix<T, -1, -1>>> &linearModels,
        const std::vector<std::vector<Eigen::Vector<T, -1>>> &linearModelsMeans,
        const std::vector<std::map<std::string, Eigen::Vector<T, -1>>> &fittingMasks,
        const SculptFittingParams<T> &params);

    //! @solve to target vertices only with joints
    static SculptFittingState<T> RegisterPoseJointsOnly(const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
                                                        const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
                                                        const SculptFittingState<T> &previousState,
                                                        const std::shared_ptr<RigGeometry<T>> &rigGeometry,
                                                        const std::shared_ptr<IdentityBlendModel<T>> model,
                                                        const SculptFittingParams<T> &params);

    //! @solve to target vertices only with joints, while projecting the parameters to the linear model defined with
    static SculptFittingState<T> RegisterPoseJointsOnlyProjectToLinear(const std::map<std::string, Eigen::Matrix<T, 3, -1>> &targetVertices,
                                                                       const std::map<std::string, std::pair<int, int>> &targetToModelMapping,
                                                                       const SculptFittingState<T> &previousState,
                                                                       const std::shared_ptr<RigGeometry<T>> &rigGeometry,
                                                                       const std::shared_ptr<IdentityBlendModel<T>> model,
                                                                       const Eigen::Matrix<T, -1, -1> &linearModel,
                                                                       const Eigen::Vector<T, -1> &linearModelMean,
                                                                       const SculptFittingParams<T> &params);

    //! @solve to target vertices only with joints, for multiple expressions, while projecting the parameters to the linear model defined with
    static std::vector<SculptFittingState<T>> RegisterMultiplePosesJointsOnlyProjectToLinear(
        const std::vector<std::map<std::string, Eigen::Matrix<T, 3, -1>>> &targetVertices,
        const std::vector<std::map<std::string, std::pair<int, int>>> &targetToModelMapping,
        const std::vector<SculptFittingState<T>> &previousStates,
        const std::shared_ptr<RigGeometry<T>> &rigGeometry,
        const std::shared_ptr<IdentityBlendModel<T>> jointsModel,
        const std::vector<Eigen::Matrix<T, -1, -1>> &linearModels,
        const std::vector<Eigen::Vector<T, -1>> &linearModelsMeans,
        const SculptFittingParams<T> &params);
};

template <class T>
struct ExpressionParametersFittingParams
{
    //! @regularization
    T modelRegularization { T(1) };

    //! @point-to-point weight
    T geometryWeight { T(10) };

    //! @number of iterations
    int numIterations { 5 };
};


template <class T>
class ExpressionParametersFittingOptimization
{
public:
    //! @solve to target joint deltas and blendshape deltas
    static Eigen::VectorX<T> RegisterPose(const Eigen::Matrix<T, 3, -1>& targetJointDeltas,
                                          const Eigen::Matrix<T, 3, -1>& targetBlendshapeDeltas,
                                          const Eigen::VectorX<T>& previousParams,
                                          const std::shared_ptr<IdentityBlendModel<T>>& model,
                                          const ExpressionParametersFittingParams<T>& params);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
