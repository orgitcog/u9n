// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Camera classes

    Header contains the C++ implementation of the coordinate system aligner class and related functionalities.
 */

#pragma once

#include <carbon/Common.h>

#include <calib/CameraManager.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
    @brief Align local object coordinate systems to unified global system.
        Each object plane is defined in its own coordinate system. For the purpose of
        joint calibration of all cameras, whole scene must be defined in one coordinate
        system (orthogonal system relative to one origin point).
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/CoordinateSystemAligner
{
public:
    virtual ~CoordinateSystemAligner() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Align coordinate systems between two object planes.

        @param ref
            Pointer to referent object plane.

        @param al
            Pointer to object plane which will be aligned to the referent plane.

        @param cameras
            Array of input camera objects.

        @return
            trust::Expected<void> which is invalid if there is an internal error.
     */

    static void alignCoordinateSystems(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras) noexcept;

    /**
        @brief Check if two input object planes are neighbors.

        @param ref
            Pointer to object plane.

        @param al
            Pointer to object plane.

        @param cameras
            Array of input camera objects.

        @return
            Boolean which is true if input planes are detected as neighbors.
     */

    static bool neighborhoodCheck(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras) noexcept;

    /**
        @brief Transform cameras with local transformation of object plane.

        @param plane
            Pointer to object plane.

        @param cameras
            Array of input camera objects.

        @return
            trust::Expected<void> which is invalid if there is an internal error.
     */

    static void transformCamerasGlobal(ObjectPlane* plane, std::vector<Camera*>& cameras) noexcept;
};


Eigen::Matrix4<real_t> estimatePatternTransform(Camera* referent, ObjectPlane* plane, size_t refFrame, size_t alFrame);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
