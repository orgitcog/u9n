// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <calib/CameraModel.h>
#include <nls/geometry/MetaShapeCamera.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

template <class T>
std::vector<Camera*> loadCamerasJson(const std::string& path);

template <class T>
std::vector<Camera*> loadCamerasXml(const std::string& path);

template <class T>
void writeCamerasJson(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera);

template <class T>
void writeCamerasXml(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera);

template <class T>
void writeCamerasRealityCapture(const std::string& path, const std::vector<TITAN_NAMESPACE::MetaShapeCamera<T>>& cameras, int type);

template <class T>
void writeCameraRealityCapture(const std::string& path, const TITAN_NAMESPACE::MetaShapeCamera<T>& camera, int type);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
