// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameInputData.h"
#include <carbon/Common.h>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

FrameInputData::FrameInputData(const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& landmarks,
                               const std::map<std::string,
                                              GeometryData>& depthmapsAsMeshes)
    : m_perCameraLandmarkData(landmarks),
    m_depthmapsAsMeshes(depthmapsAsMeshes)
{}

FrameInputData::FrameInputData(const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& landmarks2d,
                               const std::shared_ptr<const LandmarkInstance<float, 3>>& landmarks3d, const GeometryData& scan)
    : m_perCameraLandmarkData(landmarks2d),
    m_landmark3Ddata(landmarks3d),
    m_scan(scan)
{}

const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& FrameInputData::LandmarksPerCamera() const
{
    return m_perCameraLandmarkData;
}

const std::shared_ptr<const LandmarkInstance<float, 3>>& FrameInputData::Landmarks3D() const
{
    return m_landmark3Ddata;
}

const GeometryData& FrameInputData::Scan() const
{
    CARBON_PRECONDITION(m_scan.mesh, "no scan loaded");
    return m_scan;
}

const std::map<std::string, GeometryData>& FrameInputData::DepthmapsAsMeshes() const
{
    CARBON_PRECONDITION(!m_depthmapsAsMeshes.empty(), "no depthmaps loaded");
    return m_depthmapsAsMeshes;
}

void FrameInputData::UpdateScanMask(const Eigen::VectorXf& newWeights)
{
    CARBON_PRECONDITION(m_scan.mesh, "no scan loaded");
    m_scan.weights = newWeights;
}

void FrameInputData::UpdateDepthmapsMask(const std::map<std::string, Eigen::VectorXf>& vectorOfNewWeights)
{
    CARBON_PRECONDITION(!m_depthmapsAsMeshes.empty(), "no depthmaps loaded");
    CARBON_PRECONDITION(vectorOfNewWeights.size() == m_depthmapsAsMeshes.size(), "input vector size mismatch");

    for (const auto& [cameraName, weights] : vectorOfNewWeights)
    {
        if (vectorOfNewWeights.find(cameraName) == vectorOfNewWeights.end())
        {
            CARBON_CRITICAL("failed to update depth masks, no depth data for camera {}", cameraName);
        }
        m_depthmapsAsMeshes[cameraName].weights = weights;
    }
}

void FrameInputData::Clear()
{
    m_perCameraLandmarkData.clear();
    m_depthmapsAsMeshes.clear();
    m_landmark3Ddata.reset();
    m_scan.mesh.reset();
}

} // namespace TITAN_API_NAMESPACE
