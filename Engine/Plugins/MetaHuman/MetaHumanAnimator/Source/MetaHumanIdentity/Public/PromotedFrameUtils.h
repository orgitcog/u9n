// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"

#include "Math/Color.h"
#include "Math/IntPoint.h"

#include "PromotedFrameUtils.generated.h"

enum class ETimecodeAlignment : int32;

/**
 * Utility functions to generate data for promoted frame
 */

UCLASS()
class METAHUMANIDENTITY_API UPromotedFrameUtils
	: public UObject
{
	GENERATED_BODY()

public:

	/** Initialization function exposed for scripting - parses contour data config & updates footage frame */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Frame Initialization")
	static bool InitializeContourDataForFootageFrame(class UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityFootageFrame* InFootageFrame);

	/** Assigns loaded texture from specified path to TArray of BGRA colors. Returns true if successfully read and assigned data */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Texture Retrieval")
	static bool GetPromotedFrameAsPixelArrayFromDisk(const FString& InImagePath, FIntPoint& OutImageSize, TArray<FColor>& OutLocalSamples);

	/** Uses image wrapper to determine image format and returns BGRA texture */
	static class UTexture2D* GetBGRATextureFromFile(const FString& InFilePath);

	/** Returns a depth texture, loaded from a file on disk specified in the path */
	static class UTexture2D* GetDepthTextureFromFile(const FString& InFilePath);

	/** A helper function to determine the full path to a frame, taking into account timecode alignment */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Texture Retrieval")
	static FString GetImagePathForFrame(const class UFootageCaptureData* InFootageCaptureData, const FString& InCamera, const int32 InFrameId, bool bInIsImageSequence, ETimecodeAlignment InAlignment);

	/** Converts a frame number used by identity into the corresponding frame number in the underlying image sequence.
		Accounts for the case where the media track in sequencer does not start at zero. */
	static int32 IdentityFrameNumberToImageSequenceFrameNumber(const class UFootageCaptureData* InFootageCaptureData, const FString& InCamera, const int32 InFrameId, bool bInIsImageSequence, ETimecodeAlignment InAlignment);
};