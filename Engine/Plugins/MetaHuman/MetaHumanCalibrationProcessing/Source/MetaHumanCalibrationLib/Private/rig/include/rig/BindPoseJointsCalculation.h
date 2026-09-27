// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class BindPoseJointsCalculation
{
public:
    BindPoseJointsCalculation();
    ~BindPoseJointsCalculation();
    BindPoseJointsCalculation(BindPoseJointsCalculation&& o);
    BindPoseJointsCalculation(const BindPoseJointsCalculation& o) = delete;
    BindPoseJointsCalculation& operator=(BindPoseJointsCalculation&& o);
    BindPoseJointsCalculation& operator=(const BindPoseJointsCalculation& o) = delete;

    bool LoadVolumetricConfig(const std::string& filepath);
    bool LoadSurfaceConfig(const std::string& filepath);

    bool Load(const std::string& surfaceJointsConfig, const std::string& volumetricJointsConfig);

    void UpdateVolumetric(Eigen::Matrix<float, 3, -1>& vertices,
                          const std::map<std::string, std::pair<int, int>>& meshMapping,
                          const std::map<std::string, int>& jointNameToId) const;

    void UpdateSurface(Eigen::Matrix<float, 3, -1>& vertices, const int jointOffset, const std::map<std::string, int>& jointNameToId) const;

    void Update(Eigen::Matrix<float, 3, -1>& vertices, const int jointOffset, const std::map<std::string, std::pair<int, int>>& meshMapping,
                const std::map<std::string, int>& jointNameToId) const;

    bool VolumetricDataLoaded() const;

    bool SurfaceDataLoaded() const;

    std::vector<std::string> SurfaceJointsList() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)