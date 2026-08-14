// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Camera manager class

    Header contains the C++ implementation of the camera manager functions.
 */

#pragma once

#include <carbon/Common.h>

#include <calib/CameraModel.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
    @brief Cameras managing functions.
        Functions for grouping cameras and managing a group of cameras.
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/CameraManager
{
public:
    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Group cameras by detected pattern
            From the current set of cameras, extract and group cameras that detected
            a certain object plane.

        @param plane
            Pointer to referent calibration object plane.

        @param input_cameras
            Input array of cameras (Camera objects).

        @return
            Array of cameras grouped by referent pattern.
     */

    static std::vector<Camera*> groupCamerasByPattern(ObjectPlane* plane, const std::vector<Camera*>& input_cameras) noexcept;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
