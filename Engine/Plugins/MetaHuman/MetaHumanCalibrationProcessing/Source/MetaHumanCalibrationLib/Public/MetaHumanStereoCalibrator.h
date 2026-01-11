// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/PimplPtr.h"
#include "CameraCalibration.h"

namespace UE
{
namespace Wrappers
{
class METAHUMANCALIBRATIONLIB_API FMetaHumanStereoCalibrator
{
	public:
		FMetaHumanStereoCalibrator();

		/**
		* Initialize multi camera calibration.
		* @param[in] InPatternWidth The number of inner corners on the chessboard pattern wide
		* @param[in] InPatternHeight The number of inner corners on the chessboard pattern high
		* @param[in] InPatternSquareSize The size of the square on the chessboard pattern in cm
		* @returns True if initialization is successful, False otherwise.
		*/
		bool Init(uint32 PatternWidth, uint32 PatternHeight, float SquareSize);

		/**
		* Adds a camera view used for calibration
		* @param[in] InCameraName The name of the camera view
		* @param[in] InWidth The width of the camera image in pixels
		* @param[in] InHeight The height of the camera image in pixels
		* @returns True if successful, false upon any error.
		*/
		bool AddCamera(const FString& InCameraName, uint32 InWidth, uint32 InHeight);

		/**
		* Detects the corner points in the image of a chessboard pattern. Calculates an average sharpness of the corner points in the image 
		* @param[in] InCameraName The name of the camera view
		* @param[in] InImage The image of the chessboard pattern
		* @param[out] OutPoints The corner points detected in the image
		* @param[out] OutChessboardSharpness The estimated sharpness of the corner points in the image points detected in the image
		* @returns True if successful, false if chessboard pattern is not detected.
		*/
		bool DetectPattern(const FString& InCameraName, const unsigned char* InImage, TArray<FVector2D>& OutCornerPoints, double& OutChessboardSharpness);

		/**
		* Calibrates the intrinsics and extrinsics of the added cameras
		* @param[in] InCornerPointsPerCameraPerFrame The detected corner points per camera per frame
		* @param[out] OutCalibrations A array of the calibrated cameras
		* @param[out] OutMSE The reprojection error from the calibration
		* @returns True if successful, false upon any error.
		*/
		bool Calibrate(const TArray<TMap<FString, TArray<FVector2D>>>& InPointsPerCameraPerFrame, TArray<FCameraCalibration>& OutCalibrations, double& OutMse);

		/**
		* Exports the results of Calibrate to a json file
		* @param[in] InCalibrations A array of the calibrated cameras
		* @param[in] InExportFilepath The filepath the calibration json will be written to
		* @returns True if successful, false upon any error.
		*/
		bool ExportCalibrations(const TArray<FCameraCalibration>& InCalibrations, const FString& ExportFilepath);

	private:
		struct Private;
		TPimplPtr<Private> Impl;
	};
}
}