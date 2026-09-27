// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/Camera.h>
#include <nls/geometry/DepthmapData.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/MeshCorrespondenceSearch.h>
#include <nls/utils/Configuration.h>
#include <nrr/MeshLandmarks.h>
#include <nrr/VertexWeights.h>
#include <nrr/landmarks/LandmarkConfiguration.h>
#include <nrr/landmarks/LandmarkInstance.h>

#include <map>
#include <memory>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Module for initial rigid alignment of assets before fitting.
 *
 * Implemented for T=float and T=double
 */
template <class T>
class FittingInitializer
{
public:
    FittingInitializer();
    ~FittingInitializer();
    FittingInitializer(FittingInitializer&& o);
    FittingInitializer(const FittingInitializer& o) = delete;
    FittingInitializer& operator=(FittingInitializer&& o);
    FittingInitializer& operator=(const FittingInitializer& o) = delete;

    void SetTargetLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>>& landmarks);

    void SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& mesh);

    void SetToScanTransforms(const std::vector<Affine<T, 3, 3>>& transforms);

    bool InitializeEyes(Affine<T, 3, 3>& leftEyeToHead,
                        Affine<T, 3, 3>& rightEyeToHead,
                        const std::map<std::string, std::vector<Eigen::Vector3<T>>>& leftCurves,
                        const std::map<std::string, std::vector<Eigen::Vector3<T>>>& rightCurves,
                        int frame) const;

    bool InitializeTeeth(Affine<T, 3, 3>& teethToHead, const std::map<std::string, Eigen::Vector3<T>>& currentMeshLandmarks, int frame) const;

    bool InitializeFace(std::vector<Affine<T, 3, 3>>& toScanTransform,
                        std::vector<T>& toScanScale,
                        const std::map<std::string, Eigen::Vector3<T>>& currentMeshLandmarks,
                        bool withScale = false) const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

namespace fitting_tools
{

template <class T>
struct CorrespondenceData
{
    std::vector<int> srcIDs;
    std::vector<BarycentricCoordinates<T, 3>> targetBCs;

    Eigen::Matrix<T, 3, -1> EvaluateTargetBCs(const Eigen::Matrix<T, 3, -1>& inputPoints) const;

    TITAN_NAMESPACE::JsonElement SaveToJson() const;
    bool LoadFromJson(const TITAN_NAMESPACE::JsonElement& json);

};

template <class T>
std::shared_ptr<const CorrespondenceData<T>> FindClosestCorrespondences(const std::shared_ptr<const Mesh<T>>& src,
                                                                        const std::shared_ptr<const Mesh<T>>& tgt,
                                                                        const VertexWeights<T>& srcWeights);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
