// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/CameraManager.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

std::vector<Camera*> CameraManager::groupCamerasByPattern(ObjectPlane* plane, const std::vector<Camera*>& input_cameras) noexcept
{
    std::vector<Camera*> output_cameras;
    const size_t camera_count = input_cameras.size();

    for (size_t i = 0; i < camera_count; i++)
    {
        size_t proj_count = input_cameras[i]->getProjectionData().size();
        for (size_t j = 0; j < proj_count; j++)
        {
            if (plane == input_cameras[i]->getProjectionData()[j]->getObjectPlane())
            {
                output_cameras.push_back(input_cameras[i]);
            }
        }
    }

    return output_cameras;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
