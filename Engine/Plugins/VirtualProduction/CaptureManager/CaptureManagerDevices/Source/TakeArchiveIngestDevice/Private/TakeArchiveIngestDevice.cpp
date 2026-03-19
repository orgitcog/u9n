// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeArchiveIngestDevice.h"

#include "HAL/FileManager.h"
#include "Settings/CaptureManagerSettings.h"

#include "Utils/TakeArchiveIngestDeviceLog.h"

#include "LiveLinkFaceMetadata.h"
#include "StereoCameraMetadataParseUtils.h"

#include "Utils/IngestLiveLinkDeviceUtils.h"
#include "Utils/CaptureExtractTimecode.h"
#include "Utils/ParseTakeUtils.h"

DEFINE_LOG_CATEGORY(LogTakeArchiveIngestDevice);

const UTakeArchiveIngestDeviceSettings* UTakeArchiveIngestDevice::GetSettings() const
{
	return GetDeviceSettings<UTakeArchiveIngestDeviceSettings>();
}

TSubclassOf<ULiveLinkDeviceSettings> UTakeArchiveIngestDevice::GetSettingsClass() const
{
	return UTakeArchiveIngestDeviceSettings::StaticClass();
}

FText UTakeArchiveIngestDevice::GetDisplayName() const
{
	return FText::FromString(GetSettings()->DisplayName);
}

EDeviceHealth UTakeArchiveIngestDevice::GetDeviceHealth() const
{
	return EDeviceHealth::Nominal;
}

FText UTakeArchiveIngestDevice::GetHealthText() const
{
	return FText::FromString("Nominal");
}

void UTakeArchiveIngestDevice::OnSettingChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::OnSettingChanged(InPropertyChangedEvent);

	if (GET_MEMBER_NAME_CHECKED(UTakeArchiveIngestDeviceSettings, TakeDirectory) == InPropertyChangedEvent.GetMemberPropertyName())
	{
		SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Disconnected);

		FString Path = GetSettings()->TakeDirectory.Path;
		if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
		{
			SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Connected);
		}
		else
		{
			SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Disconnected);
		}
	}
}

FString UTakeArchiveIngestDevice::GetFullTakePath(UE::CaptureManager::FTakeId InTakeId) const
{
	FString StoragePath = GetSettings()->TakeDirectory.Path;

	if (const FString* RelativeTakePath = RelativeTakePaths.Find(InTakeId))
	{
		return FPaths::Combine(StoragePath, *RelativeTakePath);
	}

	return FString();
}

void UTakeArchiveIngestDevice::RunConvertAndUploadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
{
	AsyncTask(ENamedThreads::AnyThread, [this,
			  ProcessHandle = TStrongObjectPtr<const UIngestCapability_ProcessHandle>(InProcessHandle),
			  IngestOptions = TStrongObjectPtr<const UIngestCapability_Options>(InIngestOptions)]()
	{
		static constexpr uint32 NumberOfTasks = 2; // Convert, Upload

		using namespace UE::CaptureManager;
		TSharedPtr<FTaskProgress> TaskProgress
			= MakeShared<FTaskProgress>(NumberOfTasks, FTaskProgress::FProgressReporter::CreateLambda([this, ProcessHandle](double InProgress)
		{
			ExecuteProcessProgressReporter(ProcessHandle.Get(), InProgress);
		}));

		Super::IngestTake(ProcessHandle.Get(), IngestOptions.Get(), MoveTemp(TaskProgress));
	});
}

void UTakeArchiveIngestDevice::UpdateTakeList_Implementation(UIngestCapability_UpdateTakeListCallback* InCallback)
{
	AsyncTask(ENamedThreads::AnyThread, [this, Callback = TStrongObjectPtr<UIngestCapability_UpdateTakeListCallback>(InCallback)]()
	{
		RemoveAllTakes();

		FString StoragePath = GetSettings()->TakeDirectory.Path;

		TArray<FString> TakeMetadataFiles;
		const bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*StoragePath, [&](const TCHAR* InFileNameOrDirectory, bool bInIsDirectory)
		{
			if (!bInIsDirectory)
			{
				const FString CurrentFileName = FPaths::GetCleanFilename(InFileNameOrDirectory);
				if (CurrentFileName == TEXT("take.json"))
				{
					TakeMetadataFiles.Add(InFileNameOrDirectory);
				}
				else if (FPaths::GetExtension(CurrentFileName) == FTakeMetadata::FileExtension)
				{
					TakeMetadataFiles.Add(InFileNameOrDirectory);
				}
			}

			return true;
		});

		for (const FString& CurrentTakeFile : TakeMetadataFiles)
		{
			TOptional<FTakeMetadata> TakeInfoResult = ReadTake(CurrentTakeFile);

			if (TakeInfoResult.IsSet())
			{
				int32 TakeId = AddTake(TakeInfoResult.GetValue());
				FString CurrentDirectory = FPaths::GetPath(CurrentTakeFile);

				FString TakePath = CurrentDirectory.Right(CurrentDirectory.Len() - StoragePath.Len());
				FPaths::NormalizeDirectoryName(TakePath); // Removes trailing slash
				RelativeTakePaths.Add(TakeId, MoveTemp(TakePath));

				PublishEvent<FIngestCapability_TakeAddedEvent>(TakeId);
			}
		}

		ExecuteUpdateTakeListCallback(Callback.Get(), Execute_GetTakeIdentifiers(this));
	});
}

ELiveLinkDeviceConnectionStatus UTakeArchiveIngestDevice::GetConnectionStatus_Implementation() const
{
	const UTakeArchiveIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		return ELiveLinkDeviceConnectionStatus::Connected;
	}

	return ELiveLinkDeviceConnectionStatus::Disconnected;
}

FString UTakeArchiveIngestDevice::GetHardwareId_Implementation() const
{
	return FPlatformMisc::GetDeviceId();
}

bool UTakeArchiveIngestDevice::SetHardwareId_Implementation(const FString& HardwareID)
{
	return false;
}

bool UTakeArchiveIngestDevice::Connect_Implementation()
{
	const UTakeArchiveIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Connected);
		return true;
	}
	
	return false;
}

bool UTakeArchiveIngestDevice::Disconnect_Implementation()
{
	SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Disconnected);
	return true;
}

TOptional<FTakeMetadata> UTakeArchiveIngestDevice::ReadTake(const FString& InCurrentTakeFile)
{
	using namespace UE::CaptureManager;

	FTakeMetadataParser TakeMetadataParser;
	TValueOrError<FTakeMetadata, FTakeMetadataParserError> TakeMetadataResult = TakeMetadataParser.Parse(InCurrentTakeFile);
	FString CurrentDirectory = FPaths::GetPath(InCurrentTakeFile);

	if (TakeMetadataResult.HasError())
	{
		FTakeMetadataParserError ParserError = TakeMetadataResult.StealError();
		FString OriginName = UE::CaptureManager::ErrorOriginToString(ParserError.Origin);

		FString Message =
			FString::Format(TEXT("Unable to parse take metadata file - {0} (Error origin: {1}): {2}"),
							{
								InCurrentTakeFile,
								ParserError.Message.ToString(),
								OriginName
							});

		UE_LOG(LogTakeArchiveIngestDevice, Warning, TEXT("%s"), *Message);

		UE_LOG(LogTakeArchiveIngestDevice, Display, TEXT("Checking backwards compatible take metadata formats"), *CurrentDirectory);

		TArray<FText> ValidationFailures;

		UE_LOG(LogTakeArchiveIngestDevice, Display, TEXT("Checking directory (%s) for pre UE 5.6 LiveLink take metadata format"), *CurrentDirectory);
		TOptional<FTakeMetadata> ParseOldMetadataResult = LiveLinkMetadata::ParseOldLiveLinkTakeMetadata(CurrentDirectory, ValidationFailures);

		if (ParseOldMetadataResult.IsSet())
		{
			return ParseOldMetadataResult.GetValue();
		}

		UE_LOG(LogTakeArchiveIngestDevice, Display, TEXT("Checking directory (%s) for pre UE 5.6 StereoCamera take metadata format"), *CurrentDirectory);
		ParseOldMetadataResult = StereoCameraMetadata::ParseOldStereoCameraMetadata(CurrentDirectory, ValidationFailures);

		if (ParseOldMetadataResult.IsSet())
		{
			return ParseOldMetadataResult.GetValue();
		}

		UE_LOG(LogTakeArchiveIngestDevice, Error, TEXT("Failed to parse take metadata file: %s"), *CurrentDirectory);

		return {};
	}

	FTakeMetadata& TakeMetadata = TakeMetadataResult.GetValue();

	TOptional<FString> ThumbnailPath = TakeMetadata.Thumbnail.GetThumbnailPath();
	if (ThumbnailPath.IsSet())
	{
		if (FPaths::IsRelative(*ThumbnailPath))
		{
			TakeMetadata.Thumbnail = FTakeThumbnailData(FPaths::ConvertRelativePathToFull(CurrentDirectory, *ThumbnailPath));
		}
	}
	
	bool bVideoFrameRateSet = false;
	FFrameRate VideoFrameRate;
	for (FTakeMetadata::FVideo& Video : TakeMetadata.Video)
	{
		if (FPaths::IsRelative(*Video.Path))
		{
			Video.Path = FPaths::ConvertRelativePathToFull(CurrentDirectory, *Video.Path);
		}

		// Check if this is a file (and not an image sequence directory)
		if (FPaths::FileExists(Video.Path) && !Video.TimecodeStart.IsSet())
		{
			TSharedPtr<FCaptureExtractVideoTimecode> Extractor = MakeShareable(new FCaptureExtractVideoTimecode(Video.Path));
			FTimecodeAndRateResult TimecodeAndRateResult = Extractor->Extract();
			if (TimecodeAndRateResult.IsValid())
			{
				FTimecodeAndRate TimecodeAndRate = TimecodeAndRateResult.GetValue();
				Video.TimecodeStart = TimecodeAndRate.Timecode.ToString();
			}
		}

		if (!bVideoFrameRateSet)
		{
			VideoFrameRate = UE::CaptureManager::ParseFrameRate(Video.FrameRate);
		}
	}

	for (FTakeMetadata::FAudio& Audio : TakeMetadata.Audio)
	{
		if (FPaths::IsRelative(*Audio.Path))
		{
			Audio.Path = FPaths::ConvertRelativePathToFull(CurrentDirectory, *Audio.Path);
		}

		TSharedPtr<FCaptureExtractAudioTimecode> Extractor = MakeShareable(new FCaptureExtractAudioTimecode(Audio.Path));

		// The video frame rate will be used to calculate the timecode rate if the timecode rate cannot be extracted from the audio file
		// If the VideoFrameRate is not set, the timecode rate may be set to the audio sample rate
		FTimecodeAndRateResult TimecodeAndRateResult = Extractor->Extract(VideoFrameRate);
		if (TimecodeAndRateResult.IsValid())
		{
			FTimecodeAndRate TimecodeAndRate = TimecodeAndRateResult.GetValue();
			Audio.TimecodeStart = TimecodeAndRate.Timecode.ToString();
			
			if (TimecodeAndRate.MaybeTimecodeRate.IsSet())
			{
				Audio.TimecodeRate = static_cast<float>(TimecodeAndRate.MaybeTimecodeRate.GetValue().AsDecimal());
			}
		}
	}
	
	for (FTakeMetadata::FCalibration& Calib : TakeMetadata.Calibration)
	{
		if (FPaths::IsRelative(*Calib.Path))
		{
			Calib.Path = FPaths::ConvertRelativePathToFull(CurrentDirectory, *Calib.Path);
		}
	}

	return TakeMetadataResult.StealValue();
}