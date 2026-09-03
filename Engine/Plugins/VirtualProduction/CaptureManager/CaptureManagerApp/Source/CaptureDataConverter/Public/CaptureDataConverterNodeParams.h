// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskProgress.h"
#include "Async/StopToken.h"

#include "MediaSample.h"

struct FCaptureConvertDataNodeParams
{
	FString TakeOriginDirectory;
	TSharedPtr<UE::CaptureManager::FTaskProgress> TaskProgress;
	UE::CaptureManager::FStopToken StopToken;
};

struct FCaptureConvertVideoOutputParams
{
	FString ImageFileName = TEXT("frame");
	FString Format;
	UE::CaptureManager::EMediaTexturePixelFormat OutputPixelFormat;
	EMediaOrientation Rotation;
};

struct FCaptureConvertAudioOutputParams
{
	FString AudioFileName = TEXT("audio");
	FString Format = TEXT("wav");
};

struct FCaptureConvertDepthOutputParams
{
	FString ImageFileName = TEXT("depth");
	bool bShouldCompressFiles = true;
	EMediaOrientation Rotation;
};

struct FCaptureConvertCalibrationOutputParams
{
	FString FileName = TEXT("calibration");
};
