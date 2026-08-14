// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nrr/TemplateDescription.h>
#include <nls/geometry/Camera.h>
#include <nls/geometry/Affine.h>
#include <nrr/landmarks/LandmarkInstance.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class BrowLandmarksGenerator
{
public:
    BrowLandmarksGenerator();
    ~BrowLandmarksGenerator();

    void Init(const TemplateDescription& templateDesc);

    void SetLandmarks(const std::pair<LandmarkInstance<T, 2>, Camera<T>>& landmarks);

    MeshLandmarks<T> Generate(const Eigen::Matrix<T, 3, -1>& vertices,
                                         const Affine<T, 3, 3>& mesh2scanTransform,
                                         const T mesh2scanScale,
                                         bool concatenate = false);

private:
    std::vector<int> m_browMaskL, m_browMaskR;
    std::pair<LandmarkInstance<T, 2>, Camera<T>> m_landmarks;
    Mesh<T> m_topology;
    MeshLandmarks<T> m_templateMeshLandmarks;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
