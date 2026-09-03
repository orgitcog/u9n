// Copyright Epic Games, Inc.All Rights Reserved.

#include "CubicCameraSystemIngest.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "MetaHumanCaptureSourceLog.h"
#include "ImageSequenceUtils.h"
#include "TrackingPathUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "ParseTakeUtils.h"


#define LOCTEXT_NAMESPACE "FootageIngest"

FCubicCameraSystemIngest::FCubicCameraSystemIngest(const FString& InInputDirectory, 
												   bool bInShouldCompressDepthFiles, 
												   bool bInCopyImagesToProject,
												   EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
												   EMetaHumanCaptureDepthResolutionType InDepthResolution)
	: FFileFootageIngest(InInputDirectory)
	, bShouldCompressDepthFiles(bInShouldCompressDepthFiles)
	, bCopyImagesToProject(bInCopyImagesToProject)
	, DepthPrecision(InDepthPrecision)
	, DepthResolution(InDepthResolution)
{
}

FCubicCameraSystemIngest::~FCubicCameraSystemIngest() = default;

void FCubicCameraSystemIngest::RefreshTakeListAsync(TCallback<void> InCallback)
{
	Cameras.Empty();
	TakeInfos.Empty();

	FFileFootageIngest::RefreshTakeListAsync(MoveTemp(InCallback));
}

bool FCubicCameraSystemIngest::CheckResolutions(const FMetaHumanTakeInfo& InTakeInfo, const FCameraCalibration& InCalibrationInfo) const
{
	FIntPoint CalibrationResolution(InCalibrationInfo.ImageSize.X, InCalibrationInfo.ImageSize.Y);
	return InTakeInfo.Resolution == CalibrationResolution;
}

FMetaHumanTakeInfo FCubicCameraSystemIngest::ReadTake(const FString& InFilePath, const FStopToken& InStopToken, const TakeId InNewTakeId)
{
	TOptional<FCubicTakeInfo> CubicTakeInfo = FCubicCameraSystemTakeParser::ParseTakeMetadataFile(InFilePath, InStopToken);

	if (!CubicTakeInfo)
	{
		// Create a take info object containing the bare essentials, so that the user can at least see the take in the UI and that there is an 
		// issue with it. Use the metadata file path as the take name, just so the user can see something informative on the tile.
		FMetaHumanTakeInfo MetaHumanTakeInfo;
		MetaHumanTakeInfo.Id = InNewTakeId;

		// Localized so that we use the correct local terminology, even if the value subsequently gets stored as a simple string
		MetaHumanTakeInfo.Name = LOCTEXT("TakeNameUnknown", "Unknown").ToString();

		FText Message = FText::Format(LOCTEXT("TakeMetadataReadFailed", "Failed to load metadata (check format): {0}"), FText::FromString(InFilePath));

		// Log and register as an issue, it's important to do both so the user has a reference log that can be sent, rather than just screenshots.
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("%s"), *Message.ToString());
		MetaHumanTakeInfo.Issues.Emplace(MoveTemp(Message));

		return MetaHumanTakeInfo;
	}

	FString OutputFolderName = FPaths::GetPathLeaf(FPaths::GetPath(InFilePath));
	FMetaHumanTakeInfo MetaHumanTakeInfo;
	TMap<FString, FCubicCameraInfo> TakeCameras;

	FCubicCameraSystemTakeParser::CubicToMetaHumanTakeInfo(
		InFilePath,
		MoveTemp(OutputFolderName),
		*CubicTakeInfo,
		InStopToken,
		InNewTakeId,
		CameraCount,
		Type,
		MetaHumanTakeInfo,
		TakeCameras
	);

	TOptional<FText> Result = FFootageIngest::TakeDurationExceedsLimit(static_cast<float>(MetaHumanTakeInfo.NumFrames) / MetaHumanTakeInfo.FrameRate);

	if (Result.IsSet())
	{
		MetaHumanTakeInfo.Issues.Emplace(Result.GetValue());

		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("The maximum audio duration can be increased by setting the \"au.SoundWaveImportLengthLimitInSeconds\" CVar."));
	}

	Cameras.Emplace(MetaHumanTakeInfo.Id, MoveTemp(TakeCameras));
	TakeInfos.Emplace(MetaHumanTakeInfo.Id, MoveTemp(CubicTakeInfo.GetValue()));

	return MetaHumanTakeInfo;
}

TResult<void, FMetaHumanCaptureError> FCubicCameraSystemIngest::CreateAssets(const FMetaHumanTakeInfo& InTakeInfo, const FStopToken& InStopToken, FCreateAssetsData& OutCreateAssetsData)
{
	OutCreateAssetsData.TakeId = InTakeInfo.Id;
	OutCreateAssetsData.PackagePath = TargetIngestBasePackagePath / InTakeInfo.OutputDirectory;

	TMap<FString, FCubicCameraInfo> TakeCameras = Cameras[InTakeInfo.Id]; // Intentional copy as we need to preserve the data from the take.json and calib.json
	const FCubicTakeInfo TakeInfo = TakeInfos[InTakeInfo.Id]; // Intentional copy so we avoid crash if ingest is started during loading of the takes.

	if (!CheckResolutions(InTakeInfo, TakeCameras.CreateIterator()->Value.Calibration))
	{
		FText Message = LOCTEXT("IngestError_ResolutionValidationFailed", "Calibration and Image resolution differ");
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InvalidArguments, Message.ToString());
	}

	Pipeline.Reset();

	TResult<FCubicCameraSystemIngest::FCameraContextMap, FMetaHumanCaptureError> CameraContextMapResult = PrepareCameraContext(InTakeInfo.Id, TakeInfo.CameraMap);
	if (CameraContextMapResult.IsError())
	{
		return CameraContextMapResult.ClaimError();
	}

	FCubicCameraSystemIngest::FCameraContextMap CameraContextMap = CameraContextMapResult.ClaimResult();
	FCameraCalibration DepthCameraCalibration;
	TResult<void, FMetaHumanCaptureError> IngestResult = IngestFiles(InStopToken, InTakeInfo, TakeInfo, CameraContextMap, TakeCameras, DepthCameraCalibration);

	if (!IngestResult.IsError() || IngestResult.GetError().GetCode() == EMetaHumanCaptureError::Warning)
	{
		IngestResult = PrepareAssetsForCreation(InTakeInfo, TakeInfo, TakeCameras, CameraContextMap, DepthCameraCalibration, OutCreateAssetsData);
	}

	if (IngestResult.IsError())
	{
		return IngestResult.ClaimError();
	}

	return ResultOk;
}

TResult<FCubicCameraSystemIngest::FCameraContextMap, FMetaHumanCaptureError> FCubicCameraSystemIngest::PrepareCameraContext(const int32 InTakeIndex,
	const FCubicTakeInfo::FCameraMap& InCubicCamerasInfo) const
{
	FCubicCameraSystemIngest::FCameraContextMap CameraContextMap;
	CameraContextMap.Reserve(InCubicCamerasInfo.Num());

	FFrameNumber MaxStartFrameNumber(-1);
	FFrameNumber MinEndFrameNumber(MAX_int32);
	FString MaxFrameNumberCameraName;
	FString MinFrameNumberCameraName;
	for (const TPair<FString, FCubicTakeInfo::FCamera>& CameraInfo : InCubicCamerasInfo)
	{
		CameraContextMap.Emplace(CameraInfo.Key);
		CameraContextMap[CameraInfo.Key].Timecode = ParseTimecode(CameraInfo.Value.StartTimecode);
		CameraContextMap[CameraInfo.Key].FrameRate = ConvertFrameRate(CameraInfo.Value.FrameRate);
		CameraContextMap[CameraInfo.Key].FrameCount = (CameraInfo.Value.FrameRange.Value - CameraInfo.Value.FrameRange.Key) + 1;

		bool Result = PrepareImageSequenceFilePath(CameraInfo.Value.FramesPath,
			CameraContextMap[CameraInfo.Key].FrameCount,
			CameraContextMap[CameraInfo.Key].FramesPath,
			CameraContextMap[CameraInfo.Key].FirstFrameIndex);
		if (!Result)
		{
			FText Message = FText::Format(LOCTEXT("IngestFailed_InvalidImageFiles", "Number of frames in {0} doesn't match the information in `take.json`"), FText::FromString(CameraInfo.Value.FramesPath));
			FMetaHumanCaptureError Error(EMetaHumanCaptureError::InternalError, Message.ToString());
			return Error;
		}

		CameraContextMap[CameraInfo.Key].FrameOffset = CameraContextMap[CameraInfo.Key].FirstFrameIndex;

		const FTimecode& CurrentTimecode = CameraContextMap[CameraInfo.Key].Timecode;

		FFrameNumber CurrentStartFrameNumber = CurrentTimecode.ToFrameNumber(CameraContextMap[CameraInfo.Key].FrameRate);
		if (MaxStartFrameNumber < CurrentStartFrameNumber)
		{
			MaxStartFrameNumber = CurrentStartFrameNumber;
			MaxFrameNumberCameraName = CameraInfo.Key;
		}

		FFrameNumber CurrentEndFrameNumber = CurrentStartFrameNumber + CameraContextMap[CameraInfo.Key].FrameCount;
		if (MinEndFrameNumber > CurrentEndFrameNumber)
		{
			MinEndFrameNumber = CurrentEndFrameNumber;
			MinFrameNumberCameraName = CameraInfo.Key;
		}
	}

	for (TPair<FString, FCameraContext>& CameraContext : CameraContextMap)
	{
		FFrameNumber CurrentStartFrameNumber = CameraContext.Value.Timecode.ToFrameNumber(CameraContext.Value.FrameRate);
		FFrameNumber CurrentEndFrameNumber = CurrentStartFrameNumber + CameraContext.Value.FrameCount;

		if (CameraContext.Key != MaxFrameNumberCameraName)
		{
			if (MaxStartFrameNumber != CurrentStartFrameNumber)
			{
				int32 Offset = (MaxStartFrameNumber - CurrentStartFrameNumber).Value;
				UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Mismatch detected: Start timecode in \"%s\"(%s) differs from start timecode in \"%s\"(%s)."),
					*CameraContext.Key, *CameraContext.Value.Timecode.ToString(),
					*MaxFrameNumberCameraName, *CameraContextMap[MaxFrameNumberCameraName].Timecode.ToString());

				CameraContext.Value.FrameOffset = CameraContext.Value.FirstFrameIndex + Offset;

				CameraContext.Value.Timecode = CameraContextMap[MaxFrameNumberCameraName].Timecode;
			}
		}

		if (CameraContext.Key != MinFrameNumberCameraName)
		{
			if (MinEndFrameNumber < CurrentEndFrameNumber)
			{
				uint32 DiffFromEnd = (CurrentEndFrameNumber - MinEndFrameNumber).Value;
				UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Mismatch detected: %d trailing frames in \"%s\" do not have corresponding frames in \"%s\". They will be ignored."),
					DiffFromEnd, *CameraContext.Key, *MinFrameNumberCameraName);
			}
		}
	}

	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Number of frames that will be used for depth reconstruction %d"), (MinEndFrameNumber - MaxStartFrameNumber).Value);

	return CameraContextMap;
}

bool FCubicCameraSystemIngest::PrepareImageSequenceFilePath(const FString& InOriginalFramesPath, const uint32 InFrameCount, FString& OutFilePath, int32& OutOffset) const
{
	int32 FirstFrameNumber = 0;
	int32 FrameCount = 0;
	FString FrameFilePath;
	if (!FTrackingPathUtils::GetTrackingFilePathAndInfo(InOriginalFramesPath, FrameFilePath, FirstFrameNumber, FrameCount))
	{
		return false;
	}

	if ((int32)InFrameCount != FrameCount)
	{
		return false;
	}

	OutFilePath = FrameFilePath;
	OutOffset = FirstFrameNumber;

	return true;
}

TResult<void, FMetaHumanCaptureError> FCubicCameraSystemIngest::PrepareAssetsForCreation(const FMetaHumanTakeInfo& InTakeInfo,
	const FCubicTakeInfo& InCubicTakeInfo,
	const TMap<FString, FCubicCameraInfo>& InTakeCameras,
	const FCameraContextMap& InTakeCameraContextMap,
	const FCameraCalibration& InDepthCameraCalibration,
	FCreateAssetsData& OutCreateAssetsData) const
{
	const FString BasePath = TargetIngestBaseDirectory / InTakeInfo.OutputDirectory;
	for (const TPair<FString, FCubicCameraInfo>& Elem : InTakeCameras)
	{
		FCreateAssetsData::FViewData ImageView;
		FCreateAssetsData::FImageSequenceData ImageSequence;
		ImageSequence.FrameRate = InTakeInfo.FrameRate;

		ImageSequence.Name = FString::Format(TEXT("{0}_{1}_ImageSequence"), { InTakeInfo.Name, Elem.Key });

		FString FramesPath = BasePath / Elem.Key;

		// When copying files to the current project, we only copy files that are involved in depth reconstruction.
		// This means that the timecode for the image sequence will change and will be aligned with the depth timecode.
		// Otherwise, the timecode will retain its original value.
		ImageSequence.SequenceDirectory = bCopyImagesToProject ? MoveTemp(FramesPath) : InCubicTakeInfo.CameraMap[Elem.Key].FramesPath;
		ImageView.Video = ImageSequence;

		if (InCubicTakeInfo.CameraMap[Elem.Key].StartTimecode.IsEmpty())
		{
			ImageView.Video.bTimecodePresent = false;
			ImageView.Video.Timecode = FTimecode(0, 0, 0, 0, false);
			ImageView.Video.TimecodeRate = FFrameRate(60, 1); // Assume frame rate
		}
		else
		{
			ImageView.Video.bTimecodePresent = true;
			ImageView.Video.Timecode = bCopyImagesToProject ? InTakeCameraContextMap[Elem.Key].Timecode : ParseTimecode(InCubicTakeInfo.CameraMap[Elem.Key].StartTimecode);
			ImageView.Video.TimecodeRate = InTakeCameraContextMap[Elem.Key].FrameRate;
		}

		for (const FFrameRange& FrameRange : InCubicTakeInfo.CameraMap[Elem.Key].CaptureExcludedFrames)
		{
			if (!OutCreateAssetsData.CaptureExcludedFrames.Contains(FrameRange))
			{
				OutCreateAssetsData.CaptureExcludedFrames.Add(FrameRange);
			}
		}

		OutCreateAssetsData.Views.Add(MoveTemp(ImageView));
	}

	FString DepthDirectory = BasePath / TEXT("Depth");

	FCreateAssetsData::FViewData DepthViewData;
	FCreateAssetsData::FImageSequenceData DepthSequence;
	DepthSequence.Name = FString::Format(TEXT("{0}_DepthSequence"), { InTakeInfo.Name });
	DepthSequence.SequenceDirectory = MoveTemp(DepthDirectory);
	DepthSequence.bTimecodePresent = OutCreateAssetsData.Views[0].Video.bTimecodePresent;

	// The depth timecode for all cameras involved in depth reconstruction should have the same value.
	DepthSequence.Timecode = InTakeCameraContextMap.CreateConstIterator()->Value.Timecode;
	DepthSequence.TimecodeRate = OutCreateAssetsData.Views[0].Video.TimecodeRate;
	DepthSequence.FrameRate = OutCreateAssetsData.Views[0].Video.FrameRate;
	DepthViewData.Depth = DepthSequence;
	// OutCreateAssetsData.Views.Add(MoveTemp(DepthViewData));

	for (int32 Index = 0; Index < OutCreateAssetsData.Views.Num(); ++Index)
	{
		OutCreateAssetsData.Views[Index].Depth = DepthSequence;
	}

	// Audio
	for (const FCubicTakeInfo::FAudio& Audio : InCubicTakeInfo.AudioArray)
	{
		if (IFileManager::Get().FileExists(*Audio.StreamPath))
		{
			FCreateAssetsData::FAudioData AudioAsset;
			AudioAsset.Name = FPaths::GetBaseFilename(Audio.StreamPath);
			AudioAsset.WAVFile = Audio.StreamPath;

			if (!Audio.StartTimecode.IsEmpty())
			{
				AudioAsset.Timecode = ParseTimecode(Audio.StartTimecode);
			}

			if (Audio.TimecodeRate > 0.0f)
			{
				AudioAsset.TimecodeRate = ConvertFrameRate(Audio.TimecodeRate);
			}

			OutCreateAssetsData.AudioClips.Add(AudioAsset);
		}
		else
		{
			FString AudioPath = Audio.StreamPath;
			FPaths::NormalizeDirectoryName(AudioPath);

			FText Message =
				FText::Format(LOCTEXT("IngestError_AudioFileSpecifiedNotFound", "Specified audio file {0} doesn't exist for take {1}."),
					FText::FromString(AudioPath),
					FText::FromString(InTakeInfo.Name));

			return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString());
		}
	}

	// Calibration
	OutCreateAssetsData.Calibration.Name = FString::Format(TEXT("{0}_Calibration"), { InTakeInfo.Name });

	for (const TPair<FString, FCubicCameraInfo>& Elem : InTakeCameras)
	{
		OutCreateAssetsData.Calibration.CalibrationData.Add(Elem.Value.Calibration);
	}

	OutCreateAssetsData.Calibration.CalibrationData.Add(InDepthCameraCalibration);

	return ResultOk;
}

#undef LOCTEXT_NAMESPACE