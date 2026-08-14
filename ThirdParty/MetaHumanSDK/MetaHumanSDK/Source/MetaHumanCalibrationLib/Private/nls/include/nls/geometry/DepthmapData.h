// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/geometry/Camera.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct DepthmapData
{
    Camera<T> camera;
    Eigen::Matrix<T, 4, -1> depthAndNormals;

    void Create(const Camera<T>& cam);
    void Create(const Camera<T>& cam, const T* depthPtr, TaskThreadPool* taskThreadPool = nullptr);

    void ApplyScale(T scale, TaskThreadPool* taskThreadPool = nullptr);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
