// Copyright Epic Games, Inc. All Rights Reserved.

#include "MonoVideoIngestDevice.h"

#include "HAL/FileManager.h"
#include "Settings/CaptureManagerSettings.h"
#include "ImageUtils.h"

#include "CaptureManagerMediaRWModule.h"

#include "Misc/MessageDialog.h"
#include "Utils/TakeDiscoveryExpressionParser.h"
#include "Utils/VideoDeviceThumbnailExtractor.h"
#include "Utils/CaptureExtractTimecode.h"

#include "VideoLiveLinkDeviceLog.h"

#define LOCTEXT_NAMESPACE "MonoVideoDevice"

DEFINE_LOG_CATEGORY(LogVideoLiveLinkDevice);

namespace UE::MonoVideoLiveLinkDevice::Private
{

static const TArray<FString::ElementType> Delimiters =
{
	'-',
	'_',
	'.'
};

static const TArray<FStringView> SupportedVideoFormats =
{
	TEXTVIEW("mp4"),
	TEXTVIEW("mov")
};


FMediaRWManager& GetMediaRWManager()
{
	return FModuleManager::LoadModuleChecked<FCaptureManagerMediaRWModule>("CaptureManagerMediaRW").Get();
}

}

const UMonoVideoIngestDeviceSettings* UMonoVideoIngestDevice::GetSettings() const
{
	return GetDeviceSettings<UMonoVideoIngestDeviceSettings>();
}

TSubclassOf<ULiveLinkDeviceSettings> UMonoVideoIngestDevice::GetSettingsClass() const
{
	return UMonoVideoIngestDeviceSettings::StaticClass();
}

FText UMonoVideoIngestDevice::GetDisplayName() const
{
	return FText::FromString(GetSettings()->DisplayName);
}

EDeviceHealth UMonoVideoIngestDevice::GetDeviceHealth() const
{
	return EDeviceHealth::Nominal;
}

FText UMonoVideoIngestDevice::GetHealthText() const
{
	return FText::FromString("Nominal");
}

void UMonoVideoIngestDevice::OnSettingChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::OnSettingChanged(InPropertyChangedEvent);

	const FName& PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (GET_MEMBER_NAME_CHECKED(UMonoVideoIngestDeviceSettings, TakeDirectory) == PropertyName
		|| GET_MEMBER_NAME_CHECKED(UMonoVideoIngestDeviceSettings, VideoDiscoveryExpression) == PropertyName)
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

FString UMonoVideoIngestDevice::GetFullTakePath(UE::CaptureManager::FTakeId InTakeId) const
{
	if (const FString* TakePath = FullTakePaths.Find(InTakeId))
	{
		return *TakePath;
	}

	return FString();
}

void UMonoVideoIngestDevice::RunConvertAndUploadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
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

void UMonoVideoIngestDevice::UpdateTakeList_Implementation(UIngestCapability_UpdateTakeListCallback* InCallback)
{
	AsyncTask(ENamedThreads::AnyThread, [this, Callback = TStrongObjectPtr<UIngestCapability_UpdateTakeListCallback>(InCallback)]
	{
		RemoveAllTakes();

		FString StoragePath = GetSettings()->TakeDirectory.Path;
		int32 DirectoriesInterrogatedCount = 0;
		const int32 DirectoriesToInterrogateInOneRun = 200;
	
		TArray<FString> SupportedVideoFiles;
		const bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*StoragePath, [&](const TCHAR* InFileNameOrDirectory, bool bInIsDirectory)
		{
			if (bInIsDirectory)
			{
				if (++DirectoriesInterrogatedCount > DirectoriesToInterrogateInOneRun)
				{
					return false;
				}
			}

			if (!bInIsDirectory && IsVideoFile(InFileNameOrDirectory))
			{			
				SupportedVideoFiles.Add(InFileNameOrDirectory);
			}

			return true;
		});

		for (const FString& SupportedVideoFile : SupportedVideoFiles)
		{
			TOptional<FTakeMetadata> TakeInfoResult = ReadTake(SupportedVideoFile);

			if (TakeInfoResult.IsSet())
			{
				int32 TakeId = AddTake(TakeInfoResult.GetValue());
				FullTakePaths.Add(TakeId, SupportedVideoFile);

				PublishEvent<FIngestCapability_TakeAddedEvent>(TakeId);
			}
		}

		ExecuteUpdateTakeListCallback(Callback.Get(), Execute_GetTakeIdentifiers(this));
	});
}

ELiveLinkDeviceConnectionStatus UMonoVideoIngestDevice::GetConnectionStatus_Implementation() const
{
	const UMonoVideoIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		return ELiveLinkDeviceConnectionStatus::Connected;
	}

	return ELiveLinkDeviceConnectionStatus::Disconnected;
}

FString UMonoVideoIngestDevice::GetHardwareId_Implementation() const
{
	return FPlatformMisc::GetDeviceId();
}

bool UMonoVideoIngestDevice::SetHardwareId_Implementation(const FString& HardwareID)
{
	return false;
}

bool UMonoVideoIngestDevice::Connect_Implementation()
{
	const UMonoVideoIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Connected);
		return true;
	}

	return false;
}

bool UMonoVideoIngestDevice::Disconnect_Implementation()
{
	SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Disconnected);
	return true;
}

bool UMonoVideoIngestDevice::IsVideoFile(const FString& InFileNameWithExtension)
{
	FString Extension = FPaths::GetExtension(InFileNameWithExtension);

	return UE::MonoVideoLiveLinkDevice::Private::SupportedVideoFormats.Contains(Extension);
}

TOptional<FTakeMetadata> UMonoVideoIngestDevice::ReadTake(const FString& InCurrentTakeFile) const
{
	using namespace UE::CaptureManager;

	FMediaRWManager& MediaManager = UE::MonoVideoLiveLinkDevice::Private::GetMediaRWManager();

	FTakeMetadata TakeMetadata;

	FFileStatData FileData = IFileManager::Get().GetStatData(*InCurrentTakeFile);

	FString FileNameFormat = GetSettings()->VideoDiscoveryExpression.Value;
	FString FileName = FPaths::GetBaseFilename(InCurrentTakeFile);

	FString SlateName;
	FString Name;
	int32 TakeNumber = INDEX_NONE;

	if (FileNameFormat != "<Auto>")
	{
		FTakeDiscoveryExpressionParser TokenParser(FileNameFormat, FileName, UE::MonoVideoLiveLinkDevice::Private::Delimiters);
		bool bParse = TokenParser.Parse();

		if (!bParse)
		{
			UE_LOG(LogVideoLiveLinkDevice, Warning, TEXT("Failed to match the specified format (%s) with the video file (%s)"), *(FileNameFormat), *InCurrentTakeFile);
			return {};
		}

		SlateName = TokenParser.GetSlateName();
		TakeNumber = TokenParser.GetTakeNumber();
		Name = TokenParser.GetName();
	}

	if (SlateName.IsEmpty())
	{
		SlateName = MoveTemp(FileName);
	}

	if (TakeNumber == INDEX_NONE)
	{
		TakeNumber = 1;
	}

	if (Name.IsEmpty())
	{
		Name = TEXT("video");
	}

	TakeMetadata.Version.Major = 4;
	TakeMetadata.Version.Minor = 1;

	TakeMetadata.Slate = MoveTemp(SlateName);
	TakeMetadata.TakeNumber = TakeNumber;

	// Create new guid
	TakeMetadata.UniqueId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	TakeMetadata.DateTime = FileData.CreationTime;
	TakeMetadata.Device.Model = TEXT("MonoVideo");
	
	FTakeMetadata::FVideo Video;
	TValueOrError<TUniquePtr<IVideoReader>, FText> VideoReaderResult = MediaManager.CreateVideoReader(InCurrentTakeFile);
	if (VideoReaderResult.HasValue())
	{
		TUniquePtr<IVideoReader> VideoReader = VideoReaderResult.StealValue();
		Video.FrameRate = static_cast<float>(VideoReader->GetFrameRate().AsDecimal());
		TValueOrError<TUniquePtr<UE::CaptureManager::FMediaTextureSample>, FText> TextureSampleResult = VideoReader->Next();
		if (TextureSampleResult.HasValue())
		{
			FVideoDeviceThumbnailExtractor ThumbnailExtractor;
			UE::CaptureManager::FMediaTextureSample* Sample = TextureSampleResult.GetValue().Get();

			if (Sample)
			{
				TArray<FColor> ThumbnailRawColorData = ThumbnailExtractor.ExtractThumbnail(Sample);

				if (ThumbnailRawColorData.Num() > 0)
				{
					FTakeThumbnailData::FRawImage RawImage;
					RawImage.DecompressedImageData = MoveTemp(ThumbnailRawColorData);
					RawImage.Width = Sample->Dimensions.X;
					RawImage.Height = Sample->Dimensions.Y;
					RawImage.Format = ERawImageFormat::BGRA8;

					TakeMetadata.Thumbnail = MoveTemp(RawImage);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogVideoLiveLinkDevice, Warning, TEXT("Failed to determine the frame rate for the video file %s. Consider enabling Third Party Encoder in Capture Manager settings."), *InCurrentTakeFile);
	}

	Video.Name = MoveTemp(Name);
	Video.Format = FPaths::GetExtension(InCurrentTakeFile);
	Video.Path = InCurrentTakeFile;
	Video.PathType = FTakeMetadata::FVideo::EPathType::File;

	TSharedPtr<FCaptureExtractVideoTimecode> Extractor = MakeShareable(new FCaptureExtractVideoTimecode(Video.Path));
	FTimecodeAndRateResult TimecodeAndRateResult = Extractor->Extract();
	if (TimecodeAndRateResult.IsValid())
	{
		FTimecodeAndRate TimecodeAndRate = TimecodeAndRateResult.GetValue();
		Video.TimecodeStart = TimecodeAndRate.Timecode.ToString();
	}

	TakeMetadata.Video.Add(MoveTemp(Video));

	// Check if the video file contains audio stream by creating a audio reader from the file
	// If creation fails, we assume that there is no audio in the file
	TValueOrError<TUniquePtr<IAudioReader>, FText> AudioReaderResult = MediaManager.CreateAudioReader(InCurrentTakeFile);
	
	if (AudioReaderResult.HasValue())
	{
		FTakeMetadata::FAudio Audio;
		Audio.Name = TEXT("audio");
		Audio.Path = InCurrentTakeFile;
		Audio.Duration = static_cast<float>(AudioReaderResult.StealValue()->GetDuration().GetTotalSeconds());

		TakeMetadata.Audio.Add(MoveTemp(Audio));
	}

	return TakeMetadata;
}

#undef LOCTEXT_NAMESPACE
