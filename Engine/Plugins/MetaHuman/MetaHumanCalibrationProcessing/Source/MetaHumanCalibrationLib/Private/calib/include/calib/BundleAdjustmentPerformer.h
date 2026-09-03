// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - High Level Bundle Adjustment algorithm wrapper

    Header contains the C++ implementation of the high level bundle adjustment wrapper classes and related structures and functionalities.
 */

#pragma once

#include <carbon/Common.h>
#include <calib/CameraModel.h>
#include <calib/Defs.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
    @brief Bundle adjustment of created cameras/object scene
        Bundle Adjustment (BA) is almost invariably used as the last step of every feature-based multiple view
        reconstruction vision algorithm to obtain optimal 3D structure and motion (i.e. camera matrix) parameter
        estimates. Provided with initial estimates, BA simultaneously refines motion and structure by minimizing
        the reprojection error between the observed and predicted image points. The minimization is carried out
        with the aid of the Levenberg-Marquardt (LM) algorithm.

        Lourakis sba paper: http://users.ics.forth.gr/~lourakis/sba/sba-toms.pdf
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/BundleAdjustmentPerformer
{
public:
    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Bundle adjust scene
            Perform bundle adjust algorithm on created scene in order to reduce reprojection error
            between the observed and predicted image points.

        @param object
            Pointer to calibration Object object existing on the scene.

        @param params
            BAParams object - Set of bundle adjustment params that are leading the optimization process.

        @param cameras
            Array of Camera objects existing on the scene.

        @return
            Reprojection error after optimization (in pixels).

     */
    static std::optional<real_t> bundleAdjustScene(Object* object, std::vector<Camera*>& cameras, BAParams& params) noexcept;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
