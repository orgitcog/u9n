// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Utility functions for getting information about image sequences. */
class CAPTUREDATACORE_API FImageSequenceUtils
{
public:
	/** Get image sequence path and list of files from image media source asset. */
	static bool GetImageSequencePathAndFilesFromAsset(const class UImgMediaSource* InImgSequence, FString& OutFullSequencePath, TArray<FString>& OutImageFiles);

	/** Get list of image file paths from the image sequence path. */
	static bool GetImageSequenceFilesFromPath(const FString& InFullSequencePath, TArray<FString>& OutImageFiles);

	/** Get image sequence info (e.g. dimensions, number of images) from image media source asset. */
	static bool GetImageSequenceInfoFromAsset(const class UImgMediaSource* InImgSequence, FIntVector2& OutDimensions, int32& OutNumImages);

	/** Get image sequence info (e.g. dimensions, number of images) from image sequence path. */
	static bool GetImageSequenceInfoFromPath(const FString& InFullSequencePath, FIntVector2& OutDimensions, int32& OutNumImages);
};
