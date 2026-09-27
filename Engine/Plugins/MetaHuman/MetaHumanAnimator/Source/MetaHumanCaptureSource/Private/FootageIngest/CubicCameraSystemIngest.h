// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FileFootageIngest.h"
#include "MetaHumanCaptureSource.h"

#include "CubicCameraSystemTakeMetadata.h"

class FCubicCameraSystemIngest
	: public FFileFootageIngest
{
public:

	FCubicCameraSystemIngest(const FString& InInputDirectory, 
							 bool bInShouldCompressDepthFiles, 
							 bool bInCopyImagesToProject,
							 EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
							 EMetaHumanCaptureDepthResolutionType InDepthResolution);

	virtual ~FCubicCameraSystemIngest();

	virtual void RefreshTakeListAsync(TCallback<void> InCallback) override;

	//~ FFileFootageIngest interface
	virtual FMetaHumanTakeInfo ReadTake(const FString& InFilePath, const FStopToken& InStopToken, TakeId InNewTakeId) override;
	virtual TResult<void, FMetaHumanCaptureError> CreateAssets(const FMetaHumanTakeInfo& InTakeInfo, const FStopToken& InStopToken, FCreateAssetsData& OutCreateAssetsData) override;


protected:
	FString Type;

	TMap<TakeId, TMap<FString, FCubicCameraInfo>> Cameras;
	TMap<TakeId, const FCubicTakeInfo> TakeInfos;
	bool bShouldCompressDepthFiles;
	bool bCopyImagesToProject;
	EMetaHumanCaptureDepthPrecisionType DepthPrecision = EMetaHumanCaptureDepthPrecisionType::Eightieth;
	EMetaHumanCaptureDepthResolutionType DepthResolution = EMetaHumanCaptureDepthResolutionType::Full;

	int32 CameraCount;

	struct FCameraContext
	{
		FTimecode Timecode;
		int32 FirstFrameIndex = 0;
		int32 FrameOffset = 0;
		FFrameRate FrameRate;
		FString FramesPath;
		int32 FrameCount = 0;
	};

	using FCameraContextMap = TMap<FString, FCameraContext>;

	bool CheckResolutions(const FMetaHumanTakeInfo& InTakeInfo, const FCameraCalibration& InCalibrationInfo) const;

	TResult<FCubicCameraSystemIngest::FCameraContextMap, FMetaHumanCaptureError> PrepareCameraContext(const int32 InTakeIndex,
		const FCubicTakeInfo::FCameraMap& InCubicCamerasInfo) const;

	bool PrepareImageSequenceFilePath(const FString& InOriginalFramesPath,
		const uint32 InFrameCount,
		FString& OutFilePath,
		int32& OutOffset)const;

	TResult<void, FMetaHumanCaptureError> PrepareAssetsForCreation(const FMetaHumanTakeInfo& InTakeInfo,
		const FCubicTakeInfo& InCubicTakeInfo,
		const TMap<FString, FCubicCameraInfo>& InTakeCameras,
		const FCameraContextMap& InTakeCameraContextMap,
		const FCameraCalibration& InDepthCameraCalibration,
		FCreateAssetsData& OutCreateAssetsData)const;

	// override this method in derived classes to do the actual work of ingesting the files
	virtual TResult<void, FMetaHumanCaptureError> IngestFiles(const FStopToken& InStopToken,
		const FMetaHumanTakeInfo& InTakeInfo,
		const FCubicTakeInfo& InCubicTakeInfo,
		const FCameraContextMap& InCameraContextMap,
		const TMap<FString, FCubicCameraInfo>& InTakeCameras,
		FCameraCalibration& OutDepthCameraCalibration) = 0;
};
