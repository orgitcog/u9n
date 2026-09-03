// Copyright Epic Games, Inc. All Rights Reserved.

#include "SetCamerasHelper.h"

namespace UE
{
	namespace Wrappers
	{
		void SetCamerasHelper(const TArray<FCameraCalibration>& InCalibrations, std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera>& OutCameras)
		{
			OutCameras.clear();

			for (const FCameraCalibration& Calibration : InCalibrations)
			{
				TITAN_API_NAMESPACE::OpenCVCamera Camera;
				Camera.width = Calibration.ImageSize.X;
				Camera.height = Calibration.ImageSize.Y;
				Camera.fx = Calibration.FocalLength.X;
				Camera.fy = Calibration.FocalLength.Y;
				Camera.cx = Calibration.PrincipalPoint.X;
				Camera.cy = Calibration.PrincipalPoint.Y;
				Camera.k1 = Calibration.K1;
				Camera.k2 = Calibration.K2;
				Camera.k3 = Calibration.K3;
				Camera.p1 = Calibration.P1;
				Camera.p2 = Calibration.P2;

				//! Transform from world coordinates to camera coordinates in column-major format.
				for (int32 Row = 0; Row < 4; Row++)
				{
					for (int32 Col = 0; Col < 4; Col++)
					{
						Camera.Extrinsics[Row * 4 + Col] = Calibration.Transform.M[Row][Col];
					}
				}

				OutCameras[TCHAR_TO_ANSI(*Calibration.CameraId)] = Camera;
			}
		}

		void GetCalibrationsHelper(const std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera>& InCameras, TArray<FCameraCalibration>& OutCalibrations)
		{
			OutCalibrations.Empty();
			for (const std::pair<const std::string, TITAN_API_NAMESPACE::OpenCVCamera>& InCamera : InCameras)
			{
				FCameraCalibration Calibration;
				Calibration.CameraId = FString(InCamera.first.c_str());
				Calibration.CameraType = FCameraCalibration::EType::Video;
				Calibration.ImageSize = FVector2D(InCamera.second.width, InCamera.second.height);
				Calibration.FocalLength = FVector2D(InCamera.second.fx, InCamera.second.fx);
				Calibration.PrincipalPoint = FVector2D(InCamera.second.cx, InCamera.second.cy);
				Calibration.K1 = InCamera.second.k1;
				Calibration.K2 = InCamera.second.k2;
				Calibration.K3 = InCamera.second.k3;
				Calibration.P1 = InCamera.second.p1;
				Calibration.P2 = InCamera.second.p2;
				//! Transform camera coordinates to world coordinates in row major format
				for (int32 Col = 0; Col < 4; Col++)
				{
					for (int32 Row = 0; Row < 4; Row++)
					{
						Calibration.Transform.M[Row][Col] = InCamera.second.Extrinsics[Col * 4 + Row];;
					}
				}
				OutCalibrations.Add(Calibration);
			}
		}
	}
}