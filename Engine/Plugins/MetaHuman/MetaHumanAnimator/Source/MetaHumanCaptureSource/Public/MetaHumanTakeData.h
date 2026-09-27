// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Misc/Timecode.h"
#include "FrameRange.h"

#include "MetaHumanTakeData.generated.h"

/**
 * Enum that describes how the operations of the capture source should be executed
 */
UENUM()
enum class ETakeIngestMode : uint8
{
	Async,
	Blocking,
};

using TakeId = int32;
enum { INVALID_ID = -1 };

USTRUCT(BlueprintType)
struct FMetaHumanTakeInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FString Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	int32 Id = INVALID_ID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	int32 NumFrames = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	double FrameRate = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	int32 TakeNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FIntPoint Resolution = FIntPoint::NoneValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FIntPoint DepthResolution = FIntPoint::NoneValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FDateTime Date;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	int32 NumStreams = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FString DeviceModel;

	// A list of tags that describe this take, if any
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	TSet<FString> Tags;

	// Any data we want to have on a per-take basis
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	TMap<FString, FString> Metadata;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	TArray<uint8> RawThumbnailData;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	FString OutputDirectory;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take Info")
	TArray<FText> Issues;
};

USTRUCT(BlueprintType)
struct FMetaHumanTakeView
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	TObjectPtr<class UImgMediaSource> Video;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	bool bVideoTimecodePresent = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	FTimecode VideoTimecode;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	FFrameRate VideoTimecodeRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	TObjectPtr<class UImgMediaSource> Depth;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	bool bDepthTimecodePresent = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	FTimecode DepthTimecode;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take View")
	FFrameRate DepthTimecodeRate;
};

USTRUCT(BlueprintType)
struct FMetaHumanTake
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	int32 TakeId = INVALID_ID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	TArray<FMetaHumanTakeView> Views;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	TObjectPtr<class UCameraCalibration> CameraCalibration;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	TObjectPtr<class USoundWave> Audio;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	bool bAudioTimecodePresent = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	FTimecode AudioTimecode;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	FFrameRate AudioTimecodeRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Take")
	TArray<FFrameRange> CaptureExcludedFrames;
};