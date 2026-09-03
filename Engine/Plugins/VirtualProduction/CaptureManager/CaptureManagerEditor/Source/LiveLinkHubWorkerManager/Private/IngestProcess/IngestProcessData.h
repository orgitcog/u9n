// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IngestAssetCreator.h"

struct FCaptureDataTakeInfo
{
	FString Name;
	double FrameRate = 0.0;
	FIntPoint Resolution;
	FString DeviceModel;
};

struct FIngestProcessResult
{
	FString TakeIngestPackagePath;
	FCaptureDataTakeInfo CaptureDataTakeInfo;
	TArray<UE::CaptureManager::FCreateAssetsData> AssetsData;
};