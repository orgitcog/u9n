// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraCalibration.h"
#include "OpenCVCamera.h"
#include <Containers/Array.h>
#include <map>
#include <string>

namespace UE
{
    namespace Wrappers
    {
		/**
		 * Convert the array of camera calibrations into the form required to pass to the titan API
		 * @param[in] InCalibration An array of camera calibrations.
		 * @param[out] OutCameras The cameras in the form required by the titan API
		 */
		void SetCamerasHelper(const TArray<FCameraCalibration>& InCalibrations, std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera>& OutCameras);

		/**
		 * Convert a map of the titan API cameras to an array of camera calibrations
		 * @param[in] InCameras A map of the cameras in form given by the titan API
		 * @param[out] OutCalibrations An array of camera calibrations
		 */
		void GetCalibrationsHelper(const std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera>& InCameras, TArray<FCameraCalibration>& OutCalibrations);
    }
}
