// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanStereoCalibrator.h"

#include "api/MultiCameraCalibration.h"
#include "SetCamerasHelper.h"

namespace UE
{
namespace Wrappers
{
	void PointsVectorToTArrayPoints(const std::vector<float>& InPoints, TArray<FVector2D>& OutPoints)
	{
		std::size_t PointsSize = InPoints.size() / 2;
		OutPoints.Reserve(PointsSize);

		for (std::size_t PointIndex = 0; PointIndex < PointsSize; ++PointIndex)
		{
			double x = InPoints[PointIndex * 2];
			double y = InPoints[PointIndex * 2 + 1];

			OutPoints.Emplace(FVector2D(x,y));
		}
	}

	void TArrayToPointsVector(const TArray<FVector2D>& InPoints, std::vector<float>& OutPoints)
	{
		OutPoints.reserve(InPoints.Num() * 2);
		for (int32 PointIndex = 0; PointIndex < InPoints.Num(); PointIndex++)
		{
			OutPoints.push_back(InPoints[PointIndex].X);
			OutPoints.push_back(InPoints[PointIndex].Y);
		}
	}

	struct FMetaHumanStereoCalibrator::Private
	{
		TITAN_API_NAMESPACE::MultiCameraCalibration API;
	};

	FMetaHumanStereoCalibrator::FMetaHumanStereoCalibrator()
	{
		Impl = MakePimpl<Private>();
	}

	bool FMetaHumanStereoCalibrator::Init(uint32 PatternWidth, uint32 PatternHeight, float SquareSize)
	{
		return Impl->API.Init(PatternWidth, PatternHeight, SquareSize);
	}

	bool FMetaHumanStereoCalibrator::AddCamera(const FString& InCameraName, uint32 InWidth, uint32 InHeight)
	{
		return Impl->API.AddCamera(TCHAR_TO_ANSI(*InCameraName), InWidth, InHeight);
	}

	bool FMetaHumanStereoCalibrator::DetectPattern(const FString& InCameraName, const unsigned char* InImage, TArray<FVector2D>& OutCornerPoints, double& OutChessboardSharpness)
	{
		std::vector<float> DetectedPoints;

		bool DetectionSucessful = Impl->API.DetectPattern(TCHAR_TO_ANSI(*InCameraName), InImage, DetectedPoints, OutChessboardSharpness);
			
		PointsVectorToTArrayPoints(DetectedPoints, OutCornerPoints);

		return DetectionSucessful;
	}

	bool FMetaHumanStereoCalibrator::Calibrate(const TArray<TMap<FString, TArray<FVector2D>>>& InPointsPerCameraPerFrame, TArray<FCameraCalibration>& OutCalibrations, double& OutMse)
	{
		std::vector<std::map<std::string, std::vector<float>>> PointsVectorPerCameraPerFrame;
		std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera> Calibrations;
			
		for (const TMap<FString, TArray<FVector2D>>& PointsPerCamera : InPointsPerCameraPerFrame)
		{
			std::map<std::string, std::vector<float>>& PointsVectorPerCamera = PointsVectorPerCameraPerFrame.emplace_back();
			for (const TPair<FString, TArray<FVector2D>>& Points: PointsPerCamera)
			{
				PointsVectorPerCamera[TCHAR_TO_ANSI(*Points.Key)] = std::vector<float>();
				TArrayToPointsVector(Points.Value, PointsVectorPerCamera[TCHAR_TO_ANSI(*Points.Key)]);
			}
		}

		bool CalibrationSucessful = Impl->API.Calibrate(PointsVectorPerCameraPerFrame, Calibrations, OutMse);

		if (CalibrationSucessful)
		{
			GetCalibrationsHelper(Calibrations, OutCalibrations);
		}

		return CalibrationSucessful;
	}

	bool FMetaHumanStereoCalibrator::ExportCalibrations(const TArray<FCameraCalibration>& InCalibrations, const FString& ExportFilepath)
	{
		std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera> OpenCVCameras;
		SetCamerasHelper(InCalibrations, OpenCVCameras);
		return Impl->API.ExportCalibrations(OpenCVCameras, TCHAR_TO_ANSI(*ExportFilepath));
	}
}
}