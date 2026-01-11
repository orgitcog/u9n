// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorCreationUtils.h"

#include <carbon/Common.h>
#include <nls/geometry/MetaShapeCamera.h>
#include <pma/PolyAllocator.h>
#include <nls/geometry/Procrustes.h>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

MultiCameraSetup<float> ScaledCameras(const MultiCameraSetup<float>& cameras, float scale)
{
    MultiCameraSetup<float> outputCameras;
    std::vector<MetaShapeCamera<float>> currentCameras = cameras.GetCamerasAsVector();

    for (auto& cam : currentCameras)
    {
        // scale from meter to cm
        Affine<float, 3, 3> extrinsics = cam.Extrinsics();
        extrinsics.SetTranslation(extrinsics.Translation() * scale);
        cam.SetExtrinsics(extrinsics);
    }
    outputCameras.Init(currentCameras);

    return outputCameras;
}

std::vector<MultiCameraSetup<float>> ScaledCamerasPerFrame(const MultiCameraSetup<float>& cameras, const std::vector<float>& scales)
{
    std::vector<MultiCameraSetup<float>> camerasPerFrame(scales.size());

    for (size_t i = 0; i < scales.size(); ++i)
    {
        camerasPerFrame[i] = ScaledCameras(cameras, scales[i]);
    }

    return camerasPerFrame;
}

std::shared_ptr<const LandmarkInstance<float, 2>> CreateLandmarkInstanceForCamera(const std::map<std::string, FaceTrackingLandmarkData>& perCameraLandmarkData,
                                                                                  const std::map<std::string, std::vector<std::string>>& curvesToMerge,
                                                                                  const MetaShapeCamera<float>& camera)
{
    std::shared_ptr<LandmarkConfiguration> landmarkConfiguration = std::allocate_shared<LandmarkConfiguration>(pma::PolyAllocator<LandmarkConfiguration>(
                                                                                                                   MEM_RESOURCE));
    for (const auto& [landmarkOrCurveName, faceTrackingLandmarkData] : perCameraLandmarkData)
    {
        if (faceTrackingLandmarkData.NumPoints() == 1)
        {
            landmarkConfiguration->AddLandmark(landmarkOrCurveName);
        }
        else if (faceTrackingLandmarkData.NumPoints() > 1)
        {
            landmarkConfiguration->AddCurve(landmarkOrCurveName, faceTrackingLandmarkData.NumPoints());
        }
        else
        {
            CARBON_CRITICAL("at least one point per landmark/curve required");
        }
    }

    Eigen::Matrix<float, 2, -1> landmarks(2, landmarkConfiguration->NumPoints());
    Eigen::Vector<float, -1> confidence(landmarkConfiguration->NumPoints());
    for (const auto& [landmarkOrCurveName, faceTrackingLandmarkData] : perCameraLandmarkData)
    {
        if (faceTrackingLandmarkData.PointsDimension() != 2)
        {
            CARBON_CRITICAL("input landmark data is not in 2D.");
        }
        if (faceTrackingLandmarkData.NumPoints() == 1)
        {
            const int index = landmarkConfiguration->IndexForLandmark(landmarkOrCurveName);
            for (int d = 0; d < 2; ++d)
            {
                landmarks(d, index) = faceTrackingLandmarkData.PointsData()[d];
            }
            confidence[index] = faceTrackingLandmarkData.ConfidenceData()[0];
        }
        else
        {
            const std::vector<int>& indices = landmarkConfiguration->IndicesForCurve(landmarkOrCurveName);
            for (int32_t i = 0; i < faceTrackingLandmarkData.NumPoints(); ++i)
            {
                const int index = indices[i];
                for (int d = 0; d < 2; ++d)
                {
                    landmarks(d, index) = faceTrackingLandmarkData.PointsData()[2 * i + d];
                }
                confidence[index] = faceTrackingLandmarkData.ConfidenceData()[i];
            }
        }
    }
    for (const auto& [mergedCurve, listOfCurves] : curvesToMerge)
    {
        landmarkConfiguration->MergeCurves(listOfCurves, mergedCurve, landmarks, /*ignoreMissingCurves=*/true);
    }

    std::shared_ptr<LandmarkInstance<float, 2>> landmarkInstance = std::allocate_shared<LandmarkInstance<float, 2>>(pma::PolyAllocator<LandmarkInstance<float, 2>>(
                                                                                                                        MEM_RESOURCE),
                                                                                                                    landmarks,
                                                                                                                    confidence);
    landmarkInstance->SetLandmarkConfiguration(landmarkConfiguration);
    for (int i = 0; i < landmarkInstance->NumLandmarks(); ++i)
    {
        const Eigen::Vector2f pix = camera.Undistort(landmarkInstance->Points().col(i));
        landmarkInstance->SetLandmark(i,
                                      pix,
                                      landmarkInstance->Confidence()[i]);
    }
    return landmarkInstance;
}

std::shared_ptr<const LandmarkInstance<float, 3>> Create3dLandmarkInstance(const std::map<std::string, const FaceTrackingLandmarkData>& landmarkData,
                                                                           const std::map<std::string, std::vector<std::string>>& curvesToMerge)
{
    if (landmarkData.empty())
    {
        return std::shared_ptr<const LandmarkInstance<float, 3>>{};
    }

    std::shared_ptr<LandmarkConfiguration> landmarkConfiguration = std::allocate_shared<LandmarkConfiguration>(pma::PolyAllocator<LandmarkConfiguration>(
                                                                                                                   MEM_RESOURCE));
    for (const auto& [landmarkOrCurveName, faceTrackingLandmarkData] : landmarkData)
    {
        if (faceTrackingLandmarkData.NumPoints() == 1)
        {
            landmarkConfiguration->AddLandmark(landmarkOrCurveName);
        }
        else if (faceTrackingLandmarkData.NumPoints() > 1)
        {
            landmarkConfiguration->AddCurve(landmarkOrCurveName, faceTrackingLandmarkData.NumPoints());
        }
        else
        {
            CARBON_CRITICAL("at least one point per landmark/curve required");
        }
    }

    Eigen::Matrix<float, 3, -1> landmarks(3, landmarkConfiguration->NumPoints());
    Eigen::Vector<float, -1> confidence(landmarkConfiguration->NumPoints());
    for (const auto& [landmarkOrCurveName, faceTrackingLandmarkData] : landmarkData)
    {
        if (faceTrackingLandmarkData.PointsDimension() != 3)
        {
            CARBON_CRITICAL("input landmark data is not in 3D.");
        }
        if (faceTrackingLandmarkData.NumPoints() == 1)
        {
            const int index = landmarkConfiguration->IndexForLandmark(landmarkOrCurveName);
            landmarks(0, index) = faceTrackingLandmarkData.PointsData()[0];
            landmarks(1, index) = faceTrackingLandmarkData.PointsData()[1];
            landmarks(2, index) = faceTrackingLandmarkData.PointsData()[2];
            confidence[index] = faceTrackingLandmarkData.ConfidenceData()[0];
        }
        else
        {
            const std::vector<int>& indices = landmarkConfiguration->IndicesForCurve(landmarkOrCurveName);
            for (int32_t i = 0; i < faceTrackingLandmarkData.NumPoints(); ++i)
            {
                const int index = indices[i];
                landmarks(0, index) = faceTrackingLandmarkData.PointsData()[3 * i + 0];
                landmarks(1, index) = faceTrackingLandmarkData.PointsData()[3 * i + 1];
                landmarks(2, index) = faceTrackingLandmarkData.PointsData()[3 * i + 2];
                confidence[index] = faceTrackingLandmarkData.ConfidenceData()[i];
            }
        }
    }
    for (const auto& [mergedCurve, listOfCurves] : curvesToMerge)
    {
        landmarkConfiguration->MergeCurves(listOfCurves, mergedCurve, landmarks, /*ignoreMissingCurves=*/true);
    }

    std::shared_ptr<LandmarkInstance<float, 3>> landmarkInstance = std::allocate_shared<LandmarkInstance<float, 3>>(pma::PolyAllocator<LandmarkInstance<float, 3>>(
                                                                                                                        MEM_RESOURCE),
                                                                                                                    landmarks,
                                                                                                                    confidence);

    landmarkInstance->SetLandmarkConfiguration(landmarkConfiguration);

    return landmarkInstance;
}

std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>> CollectMeshes(
    const std::vector<std::shared_ptr<FrameInputData>>& frameData,
    std::vector<float> scale)
{
    std::vector<Eigen::VectorX<float>> weights;
    std::vector<std::shared_ptr<const Mesh<float>>> meshes;

    for (size_t frameNum = 0; frameNum < frameData.size(); ++frameNum)
    {
        if (scale.size() > 0)
        {
            std::shared_ptr<Mesh<float>> currentData = std::make_shared<Mesh<float>>(*frameData[frameNum]->Scan().mesh);
            currentData->SetVertices(scale[frameNum] * currentData->Vertices());
            currentData->CalculateVertexNormals();
            meshes.emplace_back(currentData);
        }
        else
        {
            meshes.emplace_back(frameData[frameNum]->Scan().mesh);
        }
        weights.emplace_back(frameData[frameNum]->Scan().weights);
    }

    return std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>>(weights,
                                                                                                          meshes);
}

std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>> CollectDepthmapsAsMeshes(
    const std::vector<std::shared_ptr<FrameInputData>>& frameData)
{
    std::vector<Eigen::VectorX<float>> weights;
    std::vector<std::shared_ptr<const Mesh<float>>> meshes;
    for (auto& frame : frameData)
    {
        for (const auto& [cameraName, depthAsMeshData] : frame->DepthmapsAsMeshes())
        {
            meshes.emplace_back(depthAsMeshData.mesh);
            weights.emplace_back(depthAsMeshData.weights);
        }
    }

    return std::pair<std::vector<Eigen::VectorX<float>>, std::vector<std::shared_ptr<const Mesh<float>>>>(weights,
                                                                                                          meshes);
}

std::pair<LandmarkInstance<float, 2>, Camera<float>> Extract2DLandmarksForCamera(const std::shared_ptr<FrameInputData>& frameData,
                                                                                 const MultiCameraSetup<float>& cameras,
                                                                                 const std::string& cameraName)
{
    auto it = frameData->LandmarksPerCamera().find(cameraName);
    if (it == frameData->LandmarksPerCamera().end())
    {
        CARBON_CRITICAL("No camera {} in frame data", cameraName);
    }

    return std::pair<LandmarkInstance<float, 2>, Camera<float>>(*it->second, cameras.GetCamera(cameraName));
}

std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> Collect2DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData,
                                                                                                  const MultiCameraSetup<float>& cameras)
{
    std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> output;

    for (auto& frame : frameData)
    {
        std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>> landmarks;
        for (const auto& [cameraName, landmarkInstance] : frame->LandmarksPerCamera())
        {
            landmarks.emplace_back(std::pair<LandmarkInstance<float, 2>, Camera<float>>(*landmarkInstance,
                                                                                        cameras.GetCamera(cameraName)));
        }
        output.emplace_back(landmarks);
    }

    return output;
}

std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> Collect2DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData,
                                                                                                  const std::vector<MultiCameraSetup<float>>& camerasPerFrame)
{
    CARBON_ASSERT(frameData.size() == camerasPerFrame.size(), "inputs size misalignment");
    std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>> output;

    for (int frameNum = 0; frameNum < int(frameData.size()); ++frameNum)
    {
        MultiCameraSetup<float> cameras = camerasPerFrame[frameNum];

        std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>> landmarks;
        for (const auto& [cameraName, landmarkInstance] : frameData[frameNum]->LandmarksPerCamera())
        {
            landmarks.emplace_back(std::pair<LandmarkInstance<float, 2>, Camera<float>>(*landmarkInstance,
                                                                                        cameras.GetCamera(cameraName)));
        }
        output.emplace_back(landmarks);
    }

    return output;
}

std::vector<LandmarkInstance<float, 3>> Collect3DLandmarks(const std::vector<std::shared_ptr<FrameInputData>>& frameData)
{
    std::vector<LandmarkInstance<float, 3>> output;
    for (auto& frame : frameData)
    {
        auto landmarks = frame->Landmarks3D();
        if (!landmarks)
        {
            continue;
        }
        output.emplace_back(*landmarks);
    }

    return output;
}

} // namespace TITAN_API_NAMESPACE
