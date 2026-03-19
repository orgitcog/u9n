// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class METAHUMANCAPTUREDATA_API FTrackingPathUtils
{
public:
	static bool GetTrackingFilePathAndInfo(const class UImgMediaSource* InImgSequence, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames);
	static bool GetTrackingFilePathAndInfo(const FString& InFullSequencePath, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames);

	static FString ExpandFilePathFormat(const FString& InFilePathFormat, int32 InFrameNumber);
};
