// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../IFootageIngestAPI.h"
#include "../LiveLinkFaceMetadata.h"

#include "MetaHumanTakeData.h"
#include "CameraCalibration.h"
#include "AssetImportTask.h"

struct FCreateAssetsData
{
	struct FImageSequenceData
	{
		FString Name;
		FString SequenceDirectory;
		double FrameRate = 0;
		TObjectPtr<class UImgMediaSource> Asset;
		bool bTimecodePresent = false;
		FTimecode Timecode = FTimecode(0, 0, 0, 0, false);
		FFrameRate TimecodeRate = FFrameRate(30, 1);
	};

	struct FViewData
	{
		FImageSequenceData Video;
		FImageSequenceData Depth;
	};

	struct FAudioData
	{
		FString Name;
		FString WAVFile;
		TObjectPtr<USoundWave> Asset;
		TOptional<FTimecode> Timecode;
		TOptional<FFrameRate> TimecodeRate;
	};

	struct FCalibrationData
	{
		FString Name;
		FString CalibrationFile;
		TArray<FCameraCalibration> CalibrationData;
		TObjectPtr<UCameraCalibration> Asset;
	};

	TakeId TakeId;
	FString PackagePath;
	TArray<FViewData> Views;
	TArray<FAudioData> AudioClips;
	FCalibrationData Calibration;
	TArray<FFrameRange> CaptureExcludedFrames;
};

class FIngestAssetCreator
{
public:
	using FPerTakeCallback = IFootageIngestAPI::TPerTakeCallback<void>;
	using FPerTakeResult = IFootageIngestAPI::TPerTakeResult<void>;

	static void CreateAssets_GameThread(TArray<FCreateAssetsData>& InOutCreateAssetDataList,
										TArray<FMetaHumanTake>& InOutTakeList,
										FPerTakeCallback InPerTakeCallback);

	static void RemoveAssetsByPath(const FString& InPackagePath);

	template<class TReturnType>
	static TReturnType* GetOrCreateAsset(const FString& InTargetPackagePath, const FString& InAssetName)
	{
		return Cast<TReturnType>(GetOrCreateAsset(InTargetPackagePath, InAssetName, TReturnType::StaticClass()));
	}

private:
	static const FText AudioImportFailedText;

	static void CreateTakeAssets_GameThread(TArray<FCreateAssetsData>& InOutCreateAssetsData,
											TArray<FMetaHumanTake>& OutTakes,
											const FPerTakeCallback& InPerTakeCallback);

	static void VerifyIngestedData_GameThread(const TArray<FCreateAssetsData>& InCreateAssetsData,
											  const TArray<FMetaHumanTake>& InTakes,
											  const FPerTakeCallback& InPerTakeCallback);

	static bool CreateTakeAssetViews_GameThread(FCreateAssetsData& InCreateAssetDate,
												TArray<FMetaHumanTakeView>& OutViews);

	static TResult<void, FMetaHumanCaptureError> AssignAudioAsset(const TObjectPtr<UAssetImportTask>& InAssetImportTask, FMetaHumanTake& OutTake);
	static TResult<void, FMetaHumanCaptureError> AssignCalibrationAsset(const TObjectPtr<UAssetImportTask>& InAssetImportTask, FMetaHumanTake& OutTake);

	static TResult<void, FMetaHumanCaptureError> CheckCreatedTakeAssets_GameThread(const FCreateAssetsData& InCreateAssetsData);
	static TResult<void, FMetaHumanCaptureError> CheckCreatedTakeStruct_GameThread(const FMetaHumanTake& InCreatedTakeStruct, bool bInShouldContainAudio);

	static void DeleteFailedTakes(TArray<int32> InTakesToDelete,
								  TArray<FMetaHumanTake>& OutTakeList,
								  TArray<FCreateAssetsData>& OutCreateAssetDataList);

	static TResult<void, FMetaHumanCaptureError> CheckTakeAssets(const FMetaHumanTake& InTake, bool bInHasAudio);

	static UObject* GetAssetIfExists(const FString& InTargetPackagePath, const FString& InAssetName);

	template<class TReturnType>
	static TReturnType* GetAssetIfExists(const FString& InTargetPackagePath, const FString& InAssetName)
	{
		return Cast<TReturnType>(GetAssetIfExists(InTargetPackagePath, InAssetName));
	}

	static UObject* GetOrCreateAsset(const FString& InTargetPackagePath, const FString& InAssetName, UClass* InClass);
	static void PrepareSoundWave(const FMetaHumanTake& InMetaHumanTake, const UAssetImportTask* InAssetImportTask, USoundWave* OutSoundWave);
};