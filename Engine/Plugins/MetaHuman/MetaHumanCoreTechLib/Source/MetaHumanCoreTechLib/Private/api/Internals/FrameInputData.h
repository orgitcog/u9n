// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <nrr/landmarks/LandmarkInstance.h>
#include <nls/geometry/DepthmapData.h>
#include <nls/geometry/Mesh.h>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

enum class InputDataType
{
    DEPTHS,
    SCAN,
    NONE
};

struct GeometryData
{
    std::shared_ptr<const Mesh<float>> mesh;
    Eigen::VectorXf weights;
};

class FrameInputData
{
public:
    FrameInputData(const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& landmarks,
                   const std::map<std::string, GeometryData>& depthmapsData);

    FrameInputData(const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& landmarks2d,
                   const std::shared_ptr<const LandmarkInstance<float, 3>>& landmarks3d, const GeometryData& scanData);

    ~FrameInputData() = default;

    const std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>>& LandmarksPerCamera() const;

    const std::shared_ptr<const LandmarkInstance<float, 3>>& Landmarks3D() const;

    const GeometryData& Scan() const;

    const std::map<std::string, GeometryData>& DepthmapsAsMeshes() const;

    void UpdateScanMask(const Eigen::VectorXf& newWeights);

    void UpdateDepthmapsMask(const std::map<std::string, Eigen::VectorXf>& vectorOfNewWeights);

    void Clear();

private:
    std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>> m_perCameraLandmarkData;
    std::shared_ptr<const LandmarkInstance<float, 3>> m_landmark3Ddata;
    GeometryData m_scan;
    std::map<std::string, GeometryData> m_depthmapsAsMeshes;
};

} // namespace TITAN_API_NAMESPACE
