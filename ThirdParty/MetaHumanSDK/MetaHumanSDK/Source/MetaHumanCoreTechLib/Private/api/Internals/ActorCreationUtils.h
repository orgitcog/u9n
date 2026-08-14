// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include "FrameInputData.h"
#include <LandmarkData.h>
#include <nrr/TemplateDescription.h>
#include <nls/geometry/MultiCameraSetup.h>

#include <string>
#include <vector>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

MultiCameraSetup<float> ScaledCameras(const MultiCameraSetup<float>& cameras, float scale);

std::vector<MultiCameraSetup<float>> ScaledCamerasPerFrame(const MultiCameraSetup<float>& cameras, const std::vector<float>& scales);

std::shared_ptr<const LandmarkInstance<float, 2>> CreateLandmarkInstanceForCamera(const std::map<std::string, FaceTrackingLandmarkData>& perCameraLandmarkData,
                                                                                  const std::map<std::string, std::vector<std::string>>& curvesToMerge,
                                                                                  const MetaShapeCamera<float>& camera);

std::shared_ptr<const LandmarkInstance<float, 3>> Create3dLandmarkInstance(const std::map<std::string, const FaceTrackingLandmarkData>& landmarkData,
                                                                           const std::map<std::string, std::vector<std::string>>& curvesToMerge);

std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>> CollectDepthmapsAsMeshes(
    const std::vector<std::shared_ptr<FrameInputData>>& frameData);

std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>> CollectMeshes(
    const std::vector<std::shared_ptr<FrameInputData>>& frameData,
    std::vector<float> scale);

std::pair<LandmarkInstance<float, 2>, Camera<float>> Extract2DLandmarksForCamera(const std::shared_ptr<FrameInputData>& frameData,
                                                                                 const MultiCameraSetup<float>& cameras,
                                                                                 const std::string& cameraName);

std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> Collect2DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData,
                                                                                                  const MultiCameraSetup<float>& cameras);

std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> Collect2DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData,
                                                                                                  const std::vector<MultiCameraSetup<float>>& camerasPerFrame);

std::vector<LandmarkInstance<float, 3>> Collect3DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData);

} // namespace TITAN_API_NAMESPACE
