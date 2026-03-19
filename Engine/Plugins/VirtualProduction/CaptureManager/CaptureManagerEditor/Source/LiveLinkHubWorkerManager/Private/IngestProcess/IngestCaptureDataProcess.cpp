// Copyright Epic Games, Inc. All Rights Reserved.

#include "IngestCaptureDataProcess.h"

#include "HAL/PlatformFileManager.h"

#include "Utils/ParseTakeUtils.h"
#include "Utils/UnrealCalibrationParser.h"

#include "Asset/CaptureAssetSanitization.h"
#include "Async/HelperFunctions.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "GlobalNamingTokens.h"
#include "NamingTokenData.h"
#include "NamingTokensEngineSubsystem.h"

#include "Settings/CaptureManagerEditorSettings.h"
#include "Settings/CaptureManagerEditorTemplateTokens.h"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureManagerIngest, Log, All);

#define LOCTEXT_NAMESPACE "IngestCaptureDataProcess"

TOptional<FString> GetIngestDataFileName(FString InTakeStoragePath)
{
	IFileManager& FileManager = IFileManager::Get();

	FString FileName = TEXT("");
	FileManager.IterateDirectory(*InTakeStoragePath, [&FileName](const TCHAR* InFileOrDirectoryName, bool bInIsDirectory)
		{
			if (bInIsDirectory)
			{
				return true;
			}

			if (FPaths::GetExtension(InFileOrDirectoryName) == FIngestCaptureData::Extension)
			{
				FileName = InFileOrDirectoryName;
				return false;
			}

			return true;
		});

	if (FileName.IsEmpty())
	{
		return {};
	}

	return FileName;
}

TValueOrError<FIngestProcessResult, FText> FIngestCaptureDataProcess::StartIngestProcess(const FString& InTakeStoragePath,
																						 const FString& InDeviceName,
																						 const FGuid& InTakeUploadId)
{
	using namespace UE::CaptureManager;

	TOptional<FString> IngestDataFileName = GetIngestDataFileName(InTakeStoragePath);
	if (!IngestDataFileName.IsSet())
	{
		FText Message = FText::Format(LOCTEXT("StartIngestProcess_TakeFileMissing", "Ingest capture data file is not found: {0}"),
			FText::FromString(*InTakeStoragePath));
		UE_LOG(LogCaptureManagerIngest, Error, TEXT("%s"), *Message.ToString());
		return MakeError(MoveTemp(Message));
	}

	const FString IngestCaptureDataFilePath = IngestDataFileName.GetValue();

	IngestCaptureData::FParseResult IngestCaptureDataParseResult = IngestCaptureData::ParseFile(IngestCaptureDataFilePath);
	if (IngestCaptureDataParseResult.HasError())
	{
		FText Message = FText::Format(LOCTEXT("StartIngestProcess_TakeMetadataFailure", "Failed to read capture data file metadata: {0} - {1}"),
			IngestCaptureDataParseResult.GetError(),
			FText::FromString(*InTakeStoragePath));

		UE_LOG(LogCaptureManagerIngest, Error, TEXT("%s"), *Message.ToString());
		return MakeError(MoveTemp(Message));
	}

	FIngestCaptureData IngestCaptureData = IngestCaptureDataParseResult.GetValue();

	ConvertPathsToFull(InTakeStoragePath, IngestCaptureData);

	FCreateAssetsData AssetCreationData = PrepareAssetsData(InTakeUploadId, InDeviceName, IngestCaptureData);

	FIngestProcessResult IngestProcessResult;
	IngestProcessResult.TakeIngestPackagePath = AssetCreationData.PackagePath;
	IngestProcessResult.AssetsData.Add(AssetCreationData);

	IngestProcessResult.CaptureDataTakeInfo.Name = AssetCreationData.CaptureDataAssetName;
	if (!IngestCaptureData.Video.IsEmpty())
	{
		const FIngestCaptureData::FVideo& FirstVideo = IngestCaptureData.Video[0];
		if (FirstVideo.FrameRate.IsSet())
		{
			IngestProcessResult.CaptureDataTakeInfo.FrameRate = FirstVideo.FrameRate.GetValue();
		}

		if (FirstVideo.FrameWidth.IsSet() && FirstVideo.FrameHeight.IsSet())
		{
			IngestProcessResult.CaptureDataTakeInfo.Resolution = FIntPoint(FirstVideo.FrameWidth.GetValue(), FirstVideo.FrameHeight.GetValue());
		}

	}
	IngestProcessResult.CaptureDataTakeInfo.DeviceModel = IngestCaptureData.DeviceModel;

	return MakeValue(MoveTemp(IngestProcessResult));

}

UE::CaptureManager::FCreateAssetsData FIngestCaptureDataProcess::PrepareAssetsData(const FGuid& InTakeUploadId,
																				   const FString& InDeviceName,
																				   const FIngestCaptureData& InIngestCaptureData)
{
	using namespace UE::CaptureManager;

	UCaptureManagerEditorSettings* Settings = GetMutableDefault<UCaptureManagerEditorSettings>();

	const TObjectPtr<const UCaptureManagerIngestNamingTokens> Tokens = Settings->GetGeneralNamingTokens();
	check(Tokens);

	FStringFormatNamedArguments ImportNamedArgs;
	{
		// Editor tokens
		ImportNamedArgs.Add(Tokens->GetToken(FString(GeneralTokens::IdKey)).Name, InTakeUploadId.ToString());
		ImportNamedArgs.Add(Tokens->GetToken(FString(GeneralTokens::DeviceKey)).Name, InDeviceName);
		ImportNamedArgs.Add(Tokens->GetToken(FString(GeneralTokens::SlateKey)).Name, InIngestCaptureData.Slate);
		ImportNamedArgs.Add(Tokens->GetToken(FString(GeneralTokens::TakeKey)).Name, FString::FromInt(InIngestCaptureData.TakeNumber));
	}

	int32 TakeId = 0;

	FCreateAssetsData CreateAssetData;

	CreateAssetData.TakeId = TakeId;

	// Naming tokens subsystem consults asset registry so need to run on the game thread
	CallOnGameThread(
		[&CreateAssetData, &InIngestCaptureData, &Settings, &ImportNamedArgs]()
		{
			UNamingTokensEngineSubsystem* NamingTokensSubsystem = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>();
			check(NamingTokensSubsystem);

			FNamingTokenFilterArgs NamingTokenArgs;
			if (const TObjectPtr<const UCaptureManagerIngestNamingTokens> Tokens = Settings->GetGeneralNamingTokens())
			{
				NamingTokenArgs.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
			}

			// Evaluate Asset Folder
			FString VerifiedImportDirectory = Settings->GetVerifiedImportDirectory();

			FString ImportDirectory = FString::Format(*VerifiedImportDirectory, ImportNamedArgs);
			FNamingTokenResultData ImportDirectoryResult = NamingTokensSubsystem->EvaluateTokenString(ImportDirectory, NamingTokenArgs);
			FString ProcessedPackagePath = ImportDirectoryResult.EvaluatedText.ToString();
			SanitizePackagePath(ProcessedPackagePath);
			CreateAssetData.PackagePath = ProcessedPackagePath;

			// Evaluate Asset Settings
			FString CaptureDataName = FString::Format(*Settings->CaptureDataAssetName, ImportNamedArgs);
			FNamingTokenResultData CaptureDataNameResult = NamingTokensSubsystem->EvaluateTokenString(CaptureDataName, NamingTokenArgs);
			FString ProcessedCaptureDataName = CaptureDataNameResult.EvaluatedText.ToString();
			SanitizeAssetName(ProcessedCaptureDataName);
			CreateAssetData.CaptureDataAssetName = ProcessedCaptureDataName;

			FNamingTokenFilterArgs VideoEvaluationArgs = NamingTokenArgs;
			if (const TObjectPtr<const UCaptureManagerVideoNamingTokens> Tokens = Settings->GetVideoNamingTokens())
			{
				VideoEvaluationArgs.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
			}

			for (int32 Index = 0; Index < InIngestCaptureData.Video.Num(); ++Index)
			{
				FCreateAssetsData::FImageSequenceData ImageSequenceData;

				const FIngestCaptureData::FVideo& Video = InIngestCaptureData.Video[Index];

				FFrameRate FrameRate = Video.FrameRate.IsSet() ? ParseFrameRate(Video.FrameRate.GetValue()) : FFrameRate();

				FStringFormatNamedArguments VideoNamedArgs;
				{
					// Video tokens
					VideoNamedArgs.Add(Settings->GetVideoNamingTokens()->GetToken(FString(VideoTokens::NameKey)).Name, Video.Name);
					VideoNamedArgs.Add(Settings->GetVideoNamingTokens()->GetToken(FString(VideoTokens::FrameRateKey)).Name, FString::Printf(TEXT("%.2f"), FrameRate.AsDecimal()));
				}
				// Evaluate Video Settings
				FString ImageSequenceAssetName = FString::Format(*Settings->ImageSequenceAssetName, VideoNamedArgs);
				ImageSequenceAssetName = FString::Format(*ImageSequenceAssetName, ImportNamedArgs);
				FNamingTokenResultData ImageSequenceAssetNameResult = NamingTokensSubsystem->EvaluateTokenString(ImageSequenceAssetName, VideoEvaluationArgs);
				FString ProcessedImageSequenceAssetName = ImageSequenceAssetNameResult.EvaluatedText.ToString();
				SanitizeAssetName(ProcessedImageSequenceAssetName);
				ImageSequenceData.AssetName = ProcessedImageSequenceAssetName;

				ImageSequenceData.FrameRate = FrameRate;
				ImageSequenceData.Name = Video.Name;
				ImageSequenceData.SequenceDirectory = Video.Path;
				ImageSequenceData.bTimecodePresent = Video.TimecodeStart.IsSet();
				ImageSequenceData.Timecode = Video.TimecodeStart.IsSet() ? ParseTimecode(Video.TimecodeStart.GetValue()) : FTimecode();
				ImageSequenceData.TimecodeRate = ImageSequenceData.FrameRate;

				CreateAssetData.ImageSequences.Add(ImageSequenceData);
			}

			for (int32 Index = 0; Index < InIngestCaptureData.Depth.Num(); ++Index)
			{
				FCreateAssetsData::FImageSequenceData DepthSequenceData;

				const FIngestCaptureData::FVideo& Depth = InIngestCaptureData.Depth[Index];

				FFrameRate FrameRate = Depth.FrameRate.IsSet() ? ParseFrameRate(Depth.FrameRate.GetValue()) : FFrameRate();

				FStringFormatNamedArguments DepthNamedArgs;
				{
					// Depth tokens
					DepthNamedArgs.Add(Settings->GetVideoNamingTokens()->GetToken(FString(VideoTokens::NameKey)).Name, Depth.Name);
					DepthNamedArgs.Add(Settings->GetVideoNamingTokens()->GetToken(FString(VideoTokens::FrameRateKey)).Name, FString::Printf(TEXT("%.2f"), FrameRate.AsDecimal()));
				}
				// Evaluate Depth Settings
				FString DepthSequenceAssetName = FString::Format(*Settings->DepthSequenceAssetName, DepthNamedArgs);
				DepthSequenceAssetName = FString::Format(*DepthSequenceAssetName, ImportNamedArgs);

				FNamingTokenResultData DepthSequenceAssetNameResult = NamingTokensSubsystem->EvaluateTokenString(DepthSequenceAssetName, VideoEvaluationArgs);
				FString ProcessedDepthSequenceAssetName = DepthSequenceAssetNameResult.EvaluatedText.ToString();
				SanitizeAssetName(ProcessedDepthSequenceAssetName);
				DepthSequenceData.AssetName = ProcessedDepthSequenceAssetName;

				DepthSequenceData.FrameRate = FrameRate;
				DepthSequenceData.Name = Depth.Name;
				DepthSequenceData.SequenceDirectory = Depth.Path;
				DepthSequenceData.bTimecodePresent = Depth.TimecodeStart.IsSet();
				DepthSequenceData.Timecode = Depth.TimecodeStart.IsSet() ? ParseTimecode(Depth.TimecodeStart.GetValue()) : FTimecode();
				DepthSequenceData.TimecodeRate = DepthSequenceData.FrameRate;
				CreateAssetData.DepthSequences.Add(DepthSequenceData);
			}

			FNamingTokenFilterArgs AudioEvaluationArgs = NamingTokenArgs;
			if (const TObjectPtr<const UCaptureManagerAudioNamingTokens> Tokens = Settings->GetAudioNamingTokens())
			{
				AudioEvaluationArgs.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
			}

			for (int32 Index = 0; Index < InIngestCaptureData.Audio.Num(); ++Index)
			{
				const FIngestCaptureData::FAudio& Audio = InIngestCaptureData.Audio[Index];

				FCreateAssetsData::FAudioData AudioData;

				FStringFormatNamedArguments AudioNamedArgs;
				{
					// Audio tokens
					AudioNamedArgs.Add(Settings->GetAudioNamingTokens()->GetToken(FString(AudioTokens::NameKey)).Name, Audio.Name);
				}
				// Evaluate Audio Settings
				FString AudioAssetName = FString::Format(*Settings->SoundwaveAssetName, AudioNamedArgs);
				AudioAssetName = FString::Format(*AudioAssetName, ImportNamedArgs);

				FNamingTokenResultData AudioAssetNameResult = NamingTokensSubsystem->EvaluateTokenString(AudioAssetName, AudioEvaluationArgs);
				FString ProcessedAudioAssetName = AudioAssetNameResult.EvaluatedText.ToString();
				SanitizeAssetName(ProcessedAudioAssetName);
				AudioData.AssetName = ProcessedAudioAssetName;

				AudioData.Name = Audio.Name;
				AudioData.WAVFile = Audio.Path;
				AudioData.bTimecodePresent = Audio.TimecodeStart.IsSet();
				AudioData.Timecode = Audio.TimecodeStart.IsSet() ? ParseTimecode(Audio.TimecodeStart.GetValue()) : FTimecode();
				AudioData.TimecodeRate = Audio.TimecodeRate.IsSet() ? ParseFrameRate(Audio.TimecodeRate.GetValue()) : FFrameRate();

				CreateAssetData.AudioClips.Add(AudioData);
			}

			FNamingTokenFilterArgs CalibEvaluationArgs = NamingTokenArgs;
			if (const TObjectPtr<const UCaptureManagerCalibrationNamingTokens> Tokens = Settings->GetCalibrationNamingTokens())
			{
				CalibEvaluationArgs.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
			}

			for (const FIngestCaptureData::FCalibration& Calibration : InIngestCaptureData.Calibration)
			{
				FCreateAssetsData::FCalibrationData CalibrationData;

				FStringFormatNamedArguments CalibNamedArgs;
				{
					// Calibration tokens
					CalibNamedArgs.Add(Settings->GetCalibrationNamingTokens()->GetToken(FString(CalibTokens::NameKey)).Name, CalibrationData.Name);
				}

				// Evaluate Calibration Settings
				FString CalibAssetName = FString::Format(*Settings->CalibrationAssetName, CalibNamedArgs);
				CalibAssetName = FString::Format(*CalibAssetName, ImportNamedArgs);
				
				FNamingTokenResultData CalibAssetNameResult = NamingTokensSubsystem->EvaluateTokenString(CalibAssetName, CalibEvaluationArgs);
				FString ProcessedCalibAssetName = CalibAssetNameResult.EvaluatedText.ToString();
				SanitizeAssetName(ProcessedCalibAssetName);
				CalibrationData.AssetName = ProcessedCalibAssetName;

				FUnrealCalibrationParser::FParseResult Result = FUnrealCalibrationParser::Parse(Calibration.Path);

				if (Result.HasValue())
				{
					CalibrationData.CameraCalibrations = Result.StealValue();

					for (const FCameraCalibration& CamCalib : CalibrationData.CameraCalibrations)
					{
						FStringFormatNamedArguments LensFileNamedArgs;
						{
							// Lens File tokens
							LensFileNamedArgs.Add(Settings->GetLensFileNamingTokens()->GetToken(FString(LensFileTokens::CameraNameKey)).Name, CamCalib.CameraId);
						}

						FString LensFileAssetName = FString::Format(*Settings->LensFileAssetName, LensFileNamedArgs);
						LensFileAssetName = FString::Format(*LensFileAssetName, CalibNamedArgs);
						LensFileAssetName = FString::Format(*LensFileAssetName, ImportNamedArgs);
						
						FNamingTokenResultData LensFileAssetNameResult = NamingTokensSubsystem->EvaluateTokenString(LensFileAssetName, CalibEvaluationArgs);
						FString ProcessedLensFileAssetName = LensFileAssetNameResult.EvaluatedText.ToString();
						SanitizeAssetName(ProcessedLensFileAssetName);
						CalibrationData.LensFileAssetNames.Add(CamCalib.CameraId, ProcessedLensFileAssetName);
					}
				}

				CreateAssetData.Calibrations.Add(MoveTemp(CalibrationData));
			}

			uint32 LastDroppedFrameIndex = 0;
			uint32 FirstDroppedFrameIndex = 0;
			if (!InIngestCaptureData.Video.IsEmpty())
			{
				for (const uint32 DroppedFrameIndex : InIngestCaptureData.Video[0].DroppedFrames)
				{
					if (FirstDroppedFrameIndex == 0)
					{
						FirstDroppedFrameIndex = DroppedFrameIndex;
					}

					if (DroppedFrameIndex - LastDroppedFrameIndex == 1)
					{
						LastDroppedFrameIndex = DroppedFrameIndex;
					}
					else
					{
						FFrameRange Range;
						Range.StartFrame = FirstDroppedFrameIndex;
						Range.EndFrame = LastDroppedFrameIndex;
						CreateAssetData.CaptureExcludedFrames.Add(MoveTemp(Range));

						FirstDroppedFrameIndex = DroppedFrameIndex;
						LastDroppedFrameIndex = DroppedFrameIndex;
					}
				}
			}
		});

	return CreateAssetData;
}

void FIngestCaptureDataProcess::ConvertPathsToFull(const FString& InTakeStoragePath, FIngestCaptureData& OutIngestCaptureData)
{
	for (FIngestCaptureData::FVideo& Video : OutIngestCaptureData.Video)
	{
		Video.Path = FPaths::ConvertRelativePathToFull(InTakeStoragePath, Video.Path);
	}

	for (FIngestCaptureData::FVideo& Depth : OutIngestCaptureData.Depth)
	{
		Depth.Path = FPaths::ConvertRelativePathToFull(InTakeStoragePath, Depth.Path);
	}

	for (FIngestCaptureData::FAudio& Audio : OutIngestCaptureData.Audio)
	{
		Audio.Path = FPaths::ConvertRelativePathToFull(InTakeStoragePath, Audio.Path);
	}

	for (FIngestCaptureData::FCalibration& Calibration : OutIngestCaptureData.Calibration)
	{
		Calibration.Path = FPaths::ConvertRelativePathToFull(InTakeStoragePath, Calibration.Path);
	}
}

#undef LOCTEXT_NAMESPACE