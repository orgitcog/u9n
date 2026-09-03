// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoVideoIngestDevice.h"

#include "HAL/FileManager.h"
#include "Settings/CaptureManagerSettings.h"
#include "ImageUtils.h"
#include "CaptureManagerMediaRWModule.h"
#include "Asset/CaptureAssetSanitization.h"
#include "Misc/FileHelper.h"
#include "Engine/Texture2D.h"

#include "Misc/MessageDialog.h"
#include "Utils/TakeDiscoveryExpressionParser.h"
#include "Utils/VideoDeviceThumbnailExtractor.h"
#include "Utils/CaptureExtractTimecode.h"
#include "Utils/ParseTakeUtils.h"

#include "VideoLiveLinkDeviceLog.h"

DEFINE_LOG_CATEGORY(LogVideoLiveLinkDevice);
#define LOCTEXT_NAMESPACE "StereoVideoLLDevice"

namespace UE::StereoVideoLiveLinkDevice::Private
{
	static const TArray<FString::ElementType> Delimiters =
	{
		'-',
		'_',
		'.',
		'/'
	};

	static const TArray<FStringView> SupportedVideoExtensions =
	{
		TEXTVIEW("mp4"),
		TEXTVIEW("mov")
	};

	static const TArray<FStringView> SupportedAudioExtensions =
	{
		TEXTVIEW("wav")
	};

	static const TArray<FStringView> SupportedImageSequenceExtensions =
	{
		TEXTVIEW("jpg"),
		TEXTVIEW("jpeg"),
		TEXTVIEW("png")
	};

	FMediaRWManager& GetMediaRWManager()
	{
		return FModuleManager::LoadModuleChecked<FCaptureManagerMediaRWModule>("CaptureManagerMediaRW").Get();
	}
}

const UStereoVideoIngestDeviceSettings* UStereoVideoIngestDevice::GetSettings() const
{
	return GetDeviceSettings<UStereoVideoIngestDeviceSettings>();
}

TSubclassOf<ULiveLinkDeviceSettings> UStereoVideoIngestDevice::GetSettingsClass() const
{
	return UStereoVideoIngestDeviceSettings::StaticClass();
}

FText UStereoVideoIngestDevice::GetDisplayName() const
{
	return FText::FromString(GetSettings()->DisplayName);
}

EDeviceHealth UStereoVideoIngestDevice::GetDeviceHealth() const
{
	return EDeviceHealth::Nominal;
}

FText UStereoVideoIngestDevice::GetHealthText() const
{
	return FText::FromString("Nominal");
}

void UStereoVideoIngestDevice::OnSettingChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::OnSettingChanged(InPropertyChangedEvent);

	const FName& PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (GET_MEMBER_NAME_CHECKED(UStereoVideoIngestDeviceSettings, TakeDirectory) == PropertyName
		|| GET_MEMBER_NAME_CHECKED(UStereoVideoIngestDeviceSettings, VideoDiscoveryExpression) == PropertyName
		|| GET_MEMBER_NAME_CHECKED(UStereoVideoIngestDeviceSettings, AudioDiscoveryExpression) == PropertyName)
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

FString UStereoVideoIngestDevice::GetFullTakePath(UE::CaptureManager::FTakeId InTakeId) const
{
	if (const FString* TakePath = FullTakePaths.Find(InTakeId))
	{
		return *TakePath;
	}

	return FString();
}

void UStereoVideoIngestDevice::RunConvertAndUploadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
{
	AsyncTask(ENamedThreads::AnyThread, [this,
		ProcessHandle = TStrongObjectPtr<const UIngestCapability_ProcessHandle>(InProcessHandle),
		IngestOptions = TStrongObjectPtr<const UIngestCapability_Options>(InIngestOptions)]()
		{
			static constexpr uint32 NumberOfTasks = 2;

			using namespace UE::CaptureManager;
			TSharedPtr<FTaskProgress> TaskProgress
				= MakeShared<FTaskProgress>(NumberOfTasks, FTaskProgress::FProgressReporter::CreateLambda([this, ProcessHandle](double InProgress)
					{
						ExecuteProcessProgressReporter(ProcessHandle.Get(), InProgress);
					}));

			Super::IngestTake(ProcessHandle.Get(), IngestOptions.Get(), MoveTemp(TaskProgress));
		});
}

int32 UStereoVideoIngestDevice::FTakeWithComponents::CountComponents(UStereoVideoIngestDevice::ETakeComponentType Type)
{
	int32 Count = 0;
	for (const UStereoVideoIngestDevice::FTakeWithComponents::Component& Component : Components)
	{
		if (Component.Type == Type)
		{
			++Count;
		}
	}
	return Count;
}

void UStereoVideoIngestDevice::UpdateTakeList_Implementation(UIngestCapability_UpdateTakeListCallback* InCallback)
{
	AsyncTask(ENamedThreads::AnyThread, [this, Callback = TStrongObjectPtr<UIngestCapability_UpdateTakeListCallback>(InCallback)]()
	{
		RemoveAllTakes();

		FString StoragePath = GetSettings()->TakeDirectory.Path;

		TMap<FString, UStereoVideoIngestDevice::FTakeWithComponents> TakesWithComponentsCandidates = DiscoverTakes(StoragePath);

		TArray<FString> SlateNames;
		TakesWithComponentsCandidates.GetKeys(SlateNames);

		for (const FString& SlateName : SlateNames)
		{
			FTakeWithComponents& TakeWithComponents = TakesWithComponentsCandidates[SlateName];

			bool TakeAsExpected = ((TakeWithComponents.CountComponents(ETakeComponentType::VIDEO) == 2 && TakeWithComponents.CountComponents(ETakeComponentType::IMAGE_SEQUENCE) == 0) ||
									(TakeWithComponents.CountComponents(ETakeComponentType::VIDEO) == 0 && TakeWithComponents.CountComponents(ETakeComponentType::IMAGE_SEQUENCE) == 2)) &&
				TakeWithComponents.CountComponents(ETakeComponentType::AUDIO) <= 1;

			if (!TakeAsExpected)
			{
				UE_LOG(LogVideoLiveLinkDevice, Warning, TEXT("Invalid take at '%s'. Take should have exactly two video files or two image sequences. Optionally, one audio file."), *TakeWithComponents.TakePath);
				continue;
			}

			TOptional<FTakeMetadata> TakeInfoResult = CreateTakeMetadata(TakeWithComponents);

			if (TakeInfoResult.IsSet())
			{
				FTakeMetadata TakeMetadata = TakeInfoResult.GetValue();

				int32 TakeId = AddTake(MoveTemp(TakeMetadata));
				FullTakePaths.Add(TakeId, TakeWithComponents.TakePath);

				PublishEvent<FIngestCapability_TakeAddedEvent>(TakeId);
			}
		}

		ExecuteUpdateTakeListCallback(Callback.Get(), Execute_GetTakeIdentifiers(this));
	});
}

TMap<FString, UStereoVideoIngestDevice::FTakeWithComponents> UStereoVideoIngestDevice::DiscoverTakes(FString StoragePath)
{
	TMap<FString, FTakeWithComponents> TakesWithComponentsCandidates;

	int32 DirectoriesInterrogatedCount = 0;
	const int32 DirectoriesToInterrogateInOneRun = 200;

	const bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*StoragePath, [&](const TCHAR* InPath, bool bInIsDirectory)
		{
			if (bInIsDirectory)
			{
				if (++DirectoriesInterrogatedCount > DirectoriesToInterrogateInOneRun)
				{
					return false;
				}
			}

			FString Extension = FPaths::GetExtension(InPath);
			if (UE::StereoVideoLiveLinkDevice::Private::SupportedVideoExtensions.Contains(Extension))
			{
				ExtractTakeComponents(InPath, StoragePath, ETakeComponentType::VIDEO, GetSettings()->VideoDiscoveryExpression.Value, "UnknownVideoName", TakesWithComponentsCandidates);
			}
			else if (UE::StereoVideoLiveLinkDevice::Private::SupportedImageSequenceExtensions.Contains(Extension))
			{
				ExtractTakeComponents(InPath, StoragePath, ETakeComponentType::IMAGE_SEQUENCE, GetSettings()->VideoDiscoveryExpression.Value, "UnknownImageSequenceName", TakesWithComponentsCandidates);
			}
			else if (UE::StereoVideoLiveLinkDevice::Private::SupportedAudioExtensions.Contains(Extension))
			{
				ExtractTakeComponents(InPath, StoragePath, ETakeComponentType::AUDIO, GetSettings()->AudioDiscoveryExpression.Value, "UnknownAudioName", TakesWithComponentsCandidates);
			}

			return true;
		});

	return TakesWithComponentsCandidates;
}

void UStereoVideoIngestDevice::ExtractTakeComponents(FString ComponentPath, FString StoragePath, ETakeComponentType ComponentType, FString Format, FString UnknownComponentName, TMap<FString, UStereoVideoIngestDevice::FTakeWithComponents>& OutTakesWithComponentsCandidates)
{
	TOptional<FTakeWithComponents> TakeWithComponents;

	if (Format == TEXT("<Auto>"))
	{
		TakeWithComponents = ExtractTakeComponentsFromDirectoryStructure(ComponentPath, StoragePath, ComponentType);
	}
	else
	{
		TakeWithComponents = ExtractTakeComponentsUsingTokens(ComponentPath, StoragePath, Format, ComponentType);
	}
	
	if (!TakeWithComponents.IsSet())
	{
		FString ComponentRootPath;
		FString FileName;
		FString Extension;
		FPaths::Split(ComponentPath, ComponentRootPath, FileName, Extension);

		TakeWithComponents = { ComponentRootPath, "Slate name not determined", -1, {{UnknownComponentName, ComponentType, ComponentPath}} };
	}

	GroupFoundComponents(OutTakesWithComponentsCandidates, TakeWithComponents.GetValue());
};

TOptional<UStereoVideoIngestDevice::FTakeWithComponents> UStereoVideoIngestDevice::ExtractTakeComponentsUsingTokens(FString ComponentPath, FString StoragePath, FString Format, ETakeComponentType ComponentType)
{
	FStringView InPathStringView(ComponentPath);
	FString RelativePath = FString(InPathStringView.SubStr(StoragePath.Len(), InPathStringView.Len() - StoragePath.Len()));
	FString Extension = FPaths::GetExtension(RelativePath);
	FString RelativePathNoExtension = RelativePath.Mid(0, RelativePath.Len() - Extension.Len() - 1);
	
	FTakeDiscoveryExpressionParser TokenParser(Format, RelativePathNoExtension, UE::StereoVideoLiveLinkDevice::Private::Delimiters);

	FString StorageLeafWithRelativePath = FPaths::GetPathLeaf(StoragePath) + RelativePathNoExtension;
	FTakeDiscoveryExpressionParser WithLeafTokenParser(Format, StorageLeafWithRelativePath, UE::StereoVideoLiveLinkDevice::Private::Delimiters);

	FString TakePath;
	FTakeDiscoveryExpressionParser* SuccessfulParser = nullptr;
	if (TokenParser.Parse())
	{
		SuccessfulParser = &TokenParser;
		TArray<FString> RelativePathParts;
		FPaths::NormalizeDirectoryName(RelativePath);
		RelativePath.ParseIntoArray(RelativePathParts, TEXT("/"));
		TakePath = StoragePath + TEXT("/") + RelativePathParts[0];
	}
	else if (WithLeafTokenParser.Parse())
	{
		SuccessfulParser = &WithLeafTokenParser;
		TakePath = StoragePath;
	}

	if (SuccessfulParser != nullptr)
	{
		return FTakeWithComponents{ TakePath, SuccessfulParser->GetSlateName(), SuccessfulParser->GetTakeNumber(), { { SuccessfulParser->GetName(), ComponentType, ComponentPath } } };
	}

	return {};
};

TOptional<UStereoVideoIngestDevice::FTakeWithComponents> UStereoVideoIngestDevice::ExtractTakeComponentsFromDirectoryStructure(FString ComponentPath, FString StoragePath, ETakeComponentType ComponentType)
{
	FStringView InPathStringView(ComponentPath);
	FString RelativePath = FString(InPathStringView.SubStr(StoragePath.Len(), InPathStringView.Len() - StoragePath.Len()));

	FString TakePath;
	FString TakeName;

	if (ComponentType == ETakeComponentType::VIDEO || ComponentType == ETakeComponentType::AUDIO)
	{
		// Use video file container folder name as TakeName
		// this is for case where user navigate StoragePath to a container folder
		TArray<FString> Parts;
		FPaths::NormalizeDirectoryName(RelativePath);
		RelativePath.ParseIntoArray(Parts, TEXT("/"));
		if (Parts.Num() > 1)
		{
			TakeName = Parts[0];
			TakePath = StoragePath + "/" + TakeName;
		}
		else
		{
			// If the above fails, because user navigates StoragePath to TakeFolder, 
			// we could use specified storage path leaf folder as TakeName
			TakeName = FPaths::GetPathLeaf(StoragePath);
			TakePath = StoragePath;
		}
	}
	else if (ComponentType == ETakeComponentType::IMAGE_SEQUENCE)
	{
		// Assumption is that frames are stored in separate folders
		TArray<FString> Parts;
		FPaths::NormalizeDirectoryName(RelativePath);
		RelativePath.ParseIntoArray(Parts, TEXT("/"));
		if (Parts.Num() > 2)
		{
			TakeName = Parts[0];
			TakePath = StoragePath + "/" + TakeName;
		}
		else
		{
			TakePath = StoragePath;
			TakeName = FPaths::GetPathLeaf(StoragePath);
		}

		{
			FString Filename, Extension;
			FPaths::Split(ComponentPath, ComponentPath, Filename, Extension);
		}
	}

	if (TakeName.IsEmpty())
	{
		return {}; // Failed to match TakeName
	}

	FString SlateName = TakeName;
	int32 TakeNumber = -1;
	FString Name;

	return FTakeWithComponents{ TakePath, SlateName, TakeNumber, { { Name, ComponentType, ComponentPath } } };
};

void UStereoVideoIngestDevice::GroupFoundComponents(TMap<FString, UStereoVideoIngestDevice::FTakeWithComponents>& TakesWithComponentsCandidates, FTakeWithComponents TakeWithComponents)
{
	FString TakeIdentifier = TakeWithComponents.SlateName + FString::FromInt(TakeWithComponents.TakeNumber);

	FTakeWithComponents* TakeWithComponentsPtr = TakesWithComponentsCandidates.Find(TakeIdentifier);
	if (TakeWithComponentsPtr == nullptr)
	{
		TakeWithComponentsPtr = &TakesWithComponentsCandidates.Add(TakeIdentifier, { TakeWithComponents.TakePath, TakeWithComponents.SlateName, TakeWithComponents.TakeNumber, {} });
	}

	for (FTakeWithComponents::Component& Component : TakeWithComponents.Components)
	{
		FTakeWithComponents::Component* TakeComponentPtr = TakeWithComponentsPtr->Components.FindByPredicate(
			[&Component](const FTakeWithComponents::Component& Other)
			{
				return Other.Path == Component.Path;
			});

		if (TakeComponentPtr == nullptr)
		{
			TakeWithComponentsPtr->Components.Add(MoveTemp(Component));
		}
	}
};

ELiveLinkDeviceConnectionStatus UStereoVideoIngestDevice::GetConnectionStatus_Implementation() const
{
	const UStereoVideoIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		return ELiveLinkDeviceConnectionStatus::Connected;
	}

	return ELiveLinkDeviceConnectionStatus::Disconnected;
}

FString UStereoVideoIngestDevice::GetHardwareId_Implementation() const
{
	return FPlatformMisc::GetDeviceId();
}

bool UStereoVideoIngestDevice::SetHardwareId_Implementation(const FString& HardwareID)
{
	return false;
}

bool UStereoVideoIngestDevice::Connect_Implementation()
{
	const UStereoVideoIngestDeviceSettings* DeviceSettings = GetSettings();
	FString Path = DeviceSettings->TakeDirectory.Path;

	if (!Path.IsEmpty() && FPaths::DirectoryExists(Path))
	{
		SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Connected);
		return true;
	}

	return false;
}

bool UStereoVideoIngestDevice::Disconnect_Implementation()
{
	SetConnectionStatus(ELiveLinkDeviceConnectionStatus::Disconnected);
	return true;
}

bool UStereoVideoIngestDevice::IsVideoFile(const FString& InFileNameWithExtension)
{
	FString Extension = FPaths::GetExtension(InFileNameWithExtension);

	return UE::StereoVideoLiveLinkDevice::Private::SupportedVideoExtensions.Contains(Extension);
}

bool UStereoVideoIngestDevice::IsFrameInSequenceFile(const FString& InFileNameWithExtension)
{
	FString Extension = FPaths::GetExtension(InFileNameWithExtension);

	return UE::StereoVideoLiveLinkDevice::Private::SupportedImageSequenceExtensions.Contains(Extension);
}

bool UStereoVideoIngestDevice::IsAudioFile(const FString& InFileNameWithExtension)
{
	FString Extension = FPaths::GetExtension(InFileNameWithExtension);

	return UE::StereoVideoLiveLinkDevice::Private::SupportedAudioExtensions.Contains(Extension);
}

TOptional<FTakeMetadata> UStereoVideoIngestDevice::CreateTakeMetadata(const FTakeWithComponents& InTakeWithComponents) const
{
	using namespace UE::CaptureManager;

	FMediaRWManager& MediaManager = UE::StereoVideoLiveLinkDevice::Private::GetMediaRWManager();

	FTakeMetadata TakeMetadata;

	int32 TakeNumber = InTakeWithComponents.TakeNumber;

	if (TakeNumber == INDEX_NONE)
	{
		TakeNumber = 1;
	}

	TakeMetadata.Version.Major = 4;
	TakeMetadata.Version.Minor = 1;

	TakeMetadata.Slate = InTakeWithComponents.SlateName;
	TakeMetadata.TakeNumber = TakeNumber;

	// Create new guid
	TakeMetadata.UniqueId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	FString InCurrentTakeFile = InTakeWithComponents.Components[0].Path;
	FFileStatData FileData = IFileManager::Get().GetStatData(*InCurrentTakeFile);
	TakeMetadata.DateTime = FileData.CreationTime;

	TakeMetadata.Device.Model = TEXT("StereoHMC");

	int32 AudioNameCounter = 1;

	bool bVideoFrameRateSet = false;
	FFrameRate VideoFrameRate;

	for (const FTakeWithComponents::Component& TakeComponent : InTakeWithComponents.Components)
	{
		if (TakeComponent.Type == ETakeComponentType::VIDEO || TakeComponent.Type == ETakeComponentType::IMAGE_SEQUENCE)
		{
			FTakeMetadata::FVideo Video;

			FString Name = TakeComponent.Name;
			if (Name.IsEmpty())
			{
				Name = TakeComponent.Path.Mid(InTakeWithComponents.TakePath.Len() + 1);
				SanitizePackagePath(Name, '_');
			}

			Video.Name = MoveTemp(Name);
			Video.Format = FPaths::GetExtension(TakeComponent.Path);

			if (TakeComponent.Type == ETakeComponentType::VIDEO)
			{
				Video.PathType = FTakeMetadata::FVideo::EPathType::File;

				TValueOrError<TUniquePtr<IVideoReader>, FText> VideoReaderResult = MediaManager.CreateVideoReader(TakeComponent.Path);
				if (VideoReaderResult.HasValue())
				{
					TUniquePtr<IVideoReader> VideoReader = VideoReaderResult.StealValue();
					Video.FrameRate = static_cast<float>(VideoReader->GetFrameRate().AsDecimal());

					if (!bVideoFrameRateSet)
					{
						VideoFrameRate = UE::CaptureManager::ParseFrameRate(Video.FrameRate);
					}

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
					UE_LOG(LogVideoLiveLinkDevice, Warning, TEXT("Failed to determine the frame rate for the video file %s. Consider enabling Third Party Encoder in Capture Manager settings."), *TakeComponent.Path);
				}

				Video.Path = TakeComponent.Path;
				
				FString MediaFilePath = FPaths::ConvertRelativePathToFull(InTakeWithComponents.TakePath, Video.Path);
				TSharedPtr<FCaptureExtractVideoTimecode> Extractor = MakeShareable(new FCaptureExtractVideoTimecode(MediaFilePath));
				FTimecodeAndRateResult TimecodeAndRateResult = Extractor->Extract();
				if (TimecodeAndRateResult.IsValid())
				{
					FTimecodeAndRate TimecodeAndRate = TimecodeAndRateResult.GetValue();
					Video.TimecodeStart = TimecodeAndRate.Timecode.ToString();
				}
			}
			else
			{
				Video.PathType = FTakeMetadata::FVideo::EPathType::Folder;

				int32 FramesCount = 0;
				bool FirstFrame = true;

				bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*TakeComponent.Path, [&](const TCHAR* InPath, bool bInIsDirectory)
					{
						if (FirstFrame)
						{
							FirstFrame = false;

							if (!TakeMetadata.Thumbnail.GetThumbnailData().IsSet())
							{
								TArray<uint8> ThumbnailData;
								if (FFileHelper::LoadFileToArray(ThumbnailData, InPath))
								{
									TakeMetadata.Thumbnail = ThumbnailData;
								}
							}

							FString Path, Filename, Extension;
							FPaths::Split(InPath, Path, Filename, Extension);
							Video.Format = Extension;

							Video.Path = TakeComponent.Path;
							{
								FImage Image;
								FImageUtils::LoadImage(InPath, Image);
								if (Image.IsImageInfoValid())
								{
									Video.FrameWidth = Image.SizeX;
									Video.FrameHeight = Image.SizeY;
								}
							}
						}

						FramesCount++;

						return true;
					});

				Video.FramesCount = FramesCount;
			}

			TakeMetadata.Video.Add(MoveTemp(Video));
		}
		else if (TakeComponent.Type == ETakeComponentType::AUDIO)
		{
			FTakeMetadata::FAudio Audio;
			FString Name = TakeComponent.Name;
			if (Name.IsEmpty())
			{
				Name = TEXT("audio") + FString::FromInt(AudioNameCounter++);
			}
			Audio.Name = Name;
			Audio.Path = TakeComponent.Path;
			Audio.Duration = 0;

			FString MediaFilePath = FPaths::ConvertRelativePathToFull(InTakeWithComponents.TakePath, Audio.Path);
			TSharedPtr<FCaptureExtractAudioTimecode> Extractor = MakeShareable(new FCaptureExtractAudioTimecode(MediaFilePath));

			// The video frame rate will be used to calculate the timecode rate if the timecode rate cannot be extracted from the audio file
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

			TakeMetadata.Audio.Add(MoveTemp(Audio));
		}
	}
	return TakeMetadata;
}

#undef LOCTEXT_NAMESPACE
