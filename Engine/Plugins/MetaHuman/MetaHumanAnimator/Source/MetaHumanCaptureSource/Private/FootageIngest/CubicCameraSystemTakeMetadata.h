// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraCalibration.h"
#include "MetaHumanTakeData.h"
#include "FrameRange.h"

#include "Async/StopToken.h"

class FCubicCameraInfo
{
public:

	FString Name;
	FCameraCalibration Calibration;
};

class FCubicTakeInfo
{
public:
	struct DeviceInfo
	{
		FString Model;
		FString Type;
		FString Id;
	};

	struct FCamera
	{
		FString UserId;
		TPair<uint32, uint32> FrameRange;
		float FrameRate = 0.0f;
		FString FramesPath;
		FString StartTimecode;
		FIntPoint Resolution = FIntPoint::NoneValue;
		TArray<FFrameRange> CaptureExcludedFrames;
	};
	using FCameraMap = TMap<FString, FCamera>;

	struct FAudio
	{
		FString UserId;
		FString StreamPath;
		float TimecodeRate = 0.0f;
		FString StartTimecode;
	};
	using FAudioArray = TArray<FAudio>;

	uint32 Version;
	FString Id;
	uint32 Take;
	FString Slate;
	FString ThumbnailPath;
	FDateTime Date;
	DeviceInfo DeviceInfo;
	FString CalibrationFilePath;

	FCameraMap CameraMap;
	FAudioArray AudioArray;

	FString TakeJsonFilePath;

	FString GetName() const;
};

class FCubicCameraSystemTakeParser
{
public:

	static bool ParseCalibrationFile(const FString& InFileName, const FStopToken& InStopToken, TMap<FString, FCubicCameraInfo>& OutCameras);
	static TOptional<FCubicTakeInfo> ParseTakeMetadataFile(const FString& InFileName, const FStopToken& InStopToken);

	static void CubicToMetaHumanTakeInfo(
		const FString& InFilePath,
		const FString InOutputDirectory,
		const FCubicTakeInfo& InCubicTakeInfo,
		const FStopToken& InStopToken,
		const TakeId InNewTakeId,
		const int32 InExpectedCameraCount,
		const FString& InDeviceType,
		FMetaHumanTakeInfo& OutTakeInfo,
		TMap<FString, FCubicCameraInfo>& OutTakeCameras
	);

private:
	static void LoadCameras(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo);
	static TArray<TSharedPtr<class FJsonValue>> ParseJsonArrayFromFile(const FString& InFilePath);
	static TSharedPtr<class FJsonObject> ParseJsonObjectFromFile(const FString& InFilePath);
};
