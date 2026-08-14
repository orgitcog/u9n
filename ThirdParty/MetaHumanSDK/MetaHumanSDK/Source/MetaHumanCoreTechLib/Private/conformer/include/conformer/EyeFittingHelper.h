// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/Camera.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class EyeFittingHelper
{
public:
    static bool UpdateScanMaskBasedOnLandmarks(const Eigen::Matrix2X<T>& lowerLid,
                                               const Eigen::Matrix2X<T>& upperLid,
                                               const Eigen::Matrix2X<T>& iris,
                                               const Camera<T>& camera,
                                               const std::shared_ptr<const Mesh<T>>& scanMesh,
                                               Eigen::VectorX<T>& mask);

    static const std::vector<int> CalculateIrisInliers(const Eigen::Matrix2X<T>& lowerLid,
                                                       const Eigen::Matrix2X<T>& upperLid,
                                                       const Eigen::Matrix2X<T>& iris,
                                                       const Camera<T>& camera);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
