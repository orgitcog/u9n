// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceDevice.h"

#include "Settings/CaptureManagerSettings.h"

#include "ILiveLinkRecordingSessionInfo.h"

#include "Control/Messages/Constants.h"
#include "Control/Messages/ControlUpdate.h"

#include "Protocol/CPSDataStream.h"
#include "Protocol/CPSFileStream.h"

#include "LiveLinkFaceMetadata.h"
#include "StereoCameraMetadataParseUtils.h"
#include "Utils/CaptureExtractTimecode.h"
#include "Utils/ParseTakeUtils.h"

#include "HAL/FileManager.h"

const ULiveLinkFaceDeviceSettings* ULiveLinkFaceDevice::GetSettings() const
{
	return GetDeviceSettings<ULiveLinkFaceDeviceSettings>();
}

TSubclassOf<ULiveLinkDeviceSettings> ULiveLinkFaceDevice::GetSettingsClass() const
{
	return ULiveLinkFaceDeviceSettings::StaticClass();
}

FText ULiveLinkFaceDevice::GetDisplayName() const
{
	return FText::FromString(GetSettings()->DisplayName);
}

EDeviceHealth ULiveLinkFaceDevice::GetDeviceHealth() const
{
	return EDeviceHealth::Nominal;
}

FText ULiveLinkFaceDevice::GetHealthText() const
{
	return FText::FromString("Example Health");
}

void ULiveLinkFaceDevice::OnDeviceAdded()
{
	GetDeviceSettings<ULiveLinkFaceDeviceSettings>()->ConnectAction.DeviceGuid = GetDeviceId();

	Super::OnDeviceAdded();
}

void ULiveLinkFaceDevice::OnDeviceRemoved()
{
	ILiveLinkDeviceCapability_Connection::Execute_Disconnect(this);

	Super::OnDeviceRemoved();
}

FString ULiveLinkFaceDevice::GetFullTakePath(UE::CaptureManager::FTakeId InTakeId) const
{
	FScopeLock Lock(&DownloadedTakesMutex);
	if (const FString* DownloadedTake = DownloadedTakes.Find(InTakeId))
	{
		return *DownloadedTake;
	}
	
	return FString();
}

void ULiveLinkFaceDevice::UpdateTakeList_Implementation(UIngestCapability_UpdateTakeListCallback* InCallback)
{
	using namespace UE::CaptureManager;

	RemoveAllTakes();

	if (!Device)
	{
		return;
	}

	TProtocolResult<TArray<FGetTakeMetadataResponse::FTakeObject>> TakesResult = Device->FetchTakeList();

	if (TakesResult.HasError())
	{
		return;
	}

	TMap<FString, FTakeId> NameToIdMap;

	for (const FGetTakeMetadataResponse::FTakeObject& Take : TakesResult.StealValue())
	{
		FTakeId TakeId = AddTake(ParseTakeMetadata(Take));
		Device->AddTakeMetadata(TakeId, Take);
		NameToIdMap.Add(Take.Name, TakeId);
	}

	FetchPreIngestFiles(MoveTemp(NameToIdMap));

	ExecuteUpdateTakeListCallback(InCallback, Execute_GetTakeIdentifiers(this));
}

void ULiveLinkFaceDevice::RunDownloadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
{
	using namespace UE::CaptureManager;

	FTakeId TakeId = InProcessHandle->GetTakeId();

	const FGetTakeMetadataResponse::FTakeObject& Take = Device->GetTake(TakeId);

	uint64 TotalSize = 0;
	for (const FGetTakeMetadataResponse::FFileObject& File : Take.Files)
	{
		TotalSize += File.Length;
	}

	FString DownloadedStorage = InIngestOptions->DownloadDirectory;

	TStrongObjectPtr<const UIngestCapability_Options> IngestOptions(InIngestOptions);
	TStrongObjectPtr<const UIngestCapability_ProcessHandle> ProcessHandle(InProcessHandle);

	TUniquePtr<FCPSFileStream> CPSStream = MakeUnique<FCPSFileStream>(MoveTemp(DownloadedStorage), TotalSize);
	CPSStream->SetExportFinished(FCPSFileStream::FExportFinished::CreateUObject(this, &ULiveLinkFaceDevice::OnExportFinished, Take.Name, ProcessHandle, IngestOptions));
	CPSStream->SetProgressHandler(FCPSFileStream::FReportProgress::CreateUObject(this, &ULiveLinkFaceDevice::OnExportProgressReport, ProcessHandle));

	Device->StartExport(TakeId, MoveTemp(CPSStream));
}

void ULiveLinkFaceDevice::RunConvertAndUploadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
{
	using namespace UE::CaptureManager;

	static constexpr uint32 NumberOfTasks = 2; // Convert, Upload

	TStrongObjectPtr<const UIngestCapability_ProcessHandle> ProcessHandle(InProcessHandle);
	TStrongObjectPtr<const UIngestCapability_Options> IngestOptions(InIngestOptions);

	TSharedPtr<FTaskProgress> TaskProgress = MakeShared<FTaskProgress>
		(NumberOfTasks, FTaskProgress::FProgressReporter::CreateLambda([this, ProcessHandle](double InProgress)
		 {
			 ExecuteProcessProgressReporter(ProcessHandle.Get(), InProgress);
		 }));

	// Free the current thread that is waiting on next download
	TWeakObjectPtr<ULiveLinkFaceDevice> WeakThis(this);
	AsyncTask(ENamedThreads::Type::AnyThread, [WeakThis = MoveTemp(WeakThis), ProcessHandle, IngestOptions, TaskProgress = MoveTemp(TaskProgress)]()
	{
		TStrongObjectPtr<ULiveLinkFaceDevice> StrongThis = WeakThis.Pin();

		if (!StrongThis.IsValid())
		{
			return;
		}

		StrongThis->Super::IngestTake(ProcessHandle.Get(), IngestOptions.Get(), TaskProgress);
		StrongThis->RemoveDownloadedTakeData(ProcessHandle->GetTakeId());
	});
}

void ULiveLinkFaceDevice::CancelIngestProcess_Implementation(const UIngestCapability_ProcessHandle* InProcessHandle)
{
	UE::CaptureManager::FTakeId TakeId = InProcessHandle->GetTakeId();

	Device->CancelExport(TakeId);

	Super::CancelIngest(TakeId);
}

ELiveLinkDeviceConnectionStatus ULiveLinkFaceDevice::GetConnectionStatus_Implementation() const
{
	if (!Device)
	{
		return ELiveLinkDeviceConnectionStatus::Disconnected;
	}

	if (bIsConnecting)
	{
		return ELiveLinkDeviceConnectionStatus::Connecting;
	}
	else if (Device->IsConnected())
	{
		return ELiveLinkDeviceConnectionStatus::Connected;
	}

	return ELiveLinkDeviceConnectionStatus::Disconnected;
}

FString ULiveLinkFaceDevice::GetHardwareId_Implementation() const
{
	return GetSettings()->IpAddress.IpAddressString;
}

bool ULiveLinkFaceDevice::SetHardwareId_Implementation(const FString& HardwareID)
{
	return false;
}

bool ULiveLinkFaceDevice::Connect_Implementation()
{
	const ULiveLinkFaceDeviceSettings* DeviceSettings = GetSettings();

	using namespace UE::CaptureManager;
	
	if (DeviceSettings->IpAddress.IpAddressString.IsEmpty())
	{
		return false;
	}

	Device = FCPSDevice::MakeCPSDevice(DeviceSettings->IpAddress.IpAddressString, static_cast<uint16>(DeviceSettings->Port));
	
	Device->SubscribeToEvent(FConnectionStateChangedEvent::Name, FCaptureEventHandler(
		FCaptureEventHandler::Type::CreateUObject(this, &ULiveLinkFaceDevice::HandleConnectionChanged),
		EDelegateExecutionThread::AnyThread));

	Device->SubscribeToEvent(FCPSStateEvent::Name, FCaptureEventHandler(
		FCaptureEventHandler::Type::CreateUObject(this, &ULiveLinkFaceDevice::HandleCPSStateUpdate),
		EDelegateExecutionThread::AnyThread));

	Device->SubscribeToEvent(FCPSEvent::Name, FCaptureEventHandler(
		FCaptureEventHandler::Type::CreateUObject(this, &ULiveLinkFaceDevice::HandleCPSEvent),
		EDelegateExecutionThread::AnyThread));

	Device->InitiateConnect();

	return true;
}

bool ULiveLinkFaceDevice::Disconnect_Implementation()
{
	AsyncTask(ENamedThreads::AnyThread, [this]()
	{
		if (Device)
		{
			Device->Stop();
			Device->UnsubscribeAll();
			bIsConnecting = false;
			Device = nullptr;
		}
	});
	
	return true;
}

bool ULiveLinkFaceDevice::StartRecording_Implementation()
{
	if (!Device)
	{
		return false;
	}

	using namespace UE::CaptureManager;

	ILiveLinkRecordingSessionInfo& SessionInfo = ILiveLinkRecordingSessionInfo::Get();
	if (SessionInfo.GetSlateName().IsEmpty() || SessionInfo.GetTakeNumber() == -1)
	{
		return false;
	}

	uint16 TakeNumber = static_cast<uint16>(SessionInfo.GetTakeNumber());
	TProtocolResult<void> Result = Device->StartRecording(SessionInfo.GetSlateName(), TakeNumber);

	return Result.HasValue();
}

bool ULiveLinkFaceDevice::StopRecording_Implementation()
{
	if (!Device)
	{
		return false;
	}

	using namespace UE::CaptureManager;

	TProtocolResult<void> Result = Device->StopRecording();

	return Result.HasValue();
}

bool ULiveLinkFaceDevice::IsRecording_Implementation() const
{
	return bIsRecording;
}

void ULiveLinkFaceDevice::HandleConnectionChanged(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent)
{
	using namespace UE::CaptureManager;

	TSharedPtr<const FConnectionStateChangedEvent> Event = StaticCastSharedPtr<const FConnectionStateChangedEvent>(InEvent);

	if (Event->ConnectionState == FConnectionStateChangedEvent::EState::Connecting)
	{
		bIsConnecting = true;
	}
	else if (Event->ConnectionState == FConnectionStateChangedEvent::EState::Connected)
	{
		bIsConnecting = false;
	}

	auto GetConnectionState = [](FConnectionStateChangedEvent::EState InConnectionState) -> ELiveLinkDeviceConnectionStatus
	{
		switch (InConnectionState)
		{
			case FConnectionStateChangedEvent::EState::Connecting:
				return ELiveLinkDeviceConnectionStatus::Connecting;
			case FConnectionStateChangedEvent::EState::Connected:
				return ELiveLinkDeviceConnectionStatus::Connected;
			case FConnectionStateChangedEvent::EState::Disconnected:
			case FConnectionStateChangedEvent::EState::Unknown:
			default:
				return ELiveLinkDeviceConnectionStatus::Disconnected;
		}
	};

	ELiveLinkDeviceConnectionStatus ConnectionStatus = GetConnectionState(Event->ConnectionState);

	SetConnectionStatus(ConnectionStatus);
}

void ULiveLinkFaceDevice::HandleCPSStateUpdate(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent)
{
	using namespace UE::CaptureManager;

	TSharedPtr<const FCPSStateEvent> Event = StaticCastSharedPtr<const FCPSStateEvent>(InEvent);

	bIsRecording = Event->GetStateResponse.IsRecording();
}

void ULiveLinkFaceDevice::HandleCPSEvent(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent)
{
	using namespace UE::CaptureManager;

	TSharedPtr<const FCPSEvent> Event = StaticCastSharedPtr<const FCPSEvent>(InEvent);

	if (Event->UpdateMessage->GetAddressPath() == CPS::AddressPaths::GRecordingStatus)
	{
		TSharedPtr<FRecordingStatusUpdate> Update = StaticCastSharedPtr<FRecordingStatusUpdate>(Event->UpdateMessage);
		bIsRecording = Update->IsRecording();
	}
	else if (Event->UpdateMessage->GetAddressPath() == CPS::AddressPaths::GTakeAdded)
	{
		TSharedPtr<const FTakeAddedUpdate> TakeAddedUpdate = StaticCastSharedPtr<const FTakeAddedUpdate>(Event->UpdateMessage);

		TProtocolResult<FGetTakeMetadataResponse::FTakeObject> FetchTakeResult = Device->FetchTake(TakeAddedUpdate->GetTakeName());

		if (FetchTakeResult.IsValid())
		{
			FGetTakeMetadataResponse::FTakeObject Take = FetchTakeResult.StealValue();
			
			FString TakeName = Take.Name;

			FTakeId TakeId = AddTake(ParseTakeMetadata(Take));
			Device->AddTakeMetadata(TakeId, MoveTemp(Take));

			PublishEvent<FIngestCapability_TakeAddedEvent>(TakeId);

			TMap<FString, UE::CaptureManager::FTakeId> NameToIdMap;
			NameToIdMap.Add(TakeName, TakeId);

			FetchPreIngestFiles(MoveTemp(NameToIdMap));
		}
	}
	else if (Event->UpdateMessage->GetAddressPath() == CPS::AddressPaths::GTakeRemoved)
	{
		TSharedPtr<const FTakeRemovedUpdate> TakeRemoveUpdate = StaticCastSharedPtr<const FTakeRemovedUpdate>(Event->UpdateMessage);

		FTakeId TakeId = Device->GetTakeId(TakeRemoveUpdate->GetTakeName());

		if (TakeId != INDEX_NONE)
		{
			CancelIngest(TakeId);

			RemoveTake(TakeId);
			Device->RemoveTakeMetadata(TakeId);
			
			PublishEvent<FIngestCapability_TakeRemovedEvent>(TakeId);
		}
	}
}

FTakeMetadata ULiveLinkFaceDevice::ParseTakeMetadata(const UE::CaptureManager::FGetTakeMetadataResponse::FTakeObject& InTake)
{
	FTakeMetadata TakeMetadata;

	TakeMetadata.Slate = InTake.Slate;
	TakeMetadata.TakeNumber = InTake.TakeNumber;
	FDateTime DateTime = TakeMetadata.DateTime.Get(FDateTime());
	FDateTime::ParseIso8601(*InTake.DateTime, DateTime);
	TakeMetadata.DateTime = DateTime;

	FTakeMetadata::FVideo Video;
	Video.FrameRate = InTake.Video.FrameRate;
	Video.FramesCount = static_cast<uint32>(InTake.Video.Frames);
	Video.Format = TEXT("mov");
	Video.FrameHeight = InTake.Video.Height;
	Video.FrameWidth = InTake.Video.Width;

	TakeMetadata.Video.Add(MoveTemp(Video));

	return TakeMetadata;
}

void ULiveLinkFaceDevice::OnExportProgressReport(float InProgress, TStrongObjectPtr<const UIngestCapability_ProcessHandle> InProcessHandle)
{
	ExecuteProcessProgressReporter(InProcessHandle.Get(), InProgress);
}

void ULiveLinkFaceDevice::OnExportFinished(UE::CaptureManager::TProtocolResult<void> InResult,
										   FString InTakeName,
										   TStrongObjectPtr<const UIngestCapability_ProcessHandle> InProcessHandle,
										   TStrongObjectPtr<const UIngestCapability_Options> InIngestOptions)
{
	using namespace UE::CaptureManager;

	const FString DownloadedStorage = InIngestOptions->DownloadDirectory;
	const FString DownloadedTake = FPaths::Combine(DownloadedStorage, InTakeName);

	if (InResult.HasValue())
	{
		// Blocking function
		TOptional<FTakeMetadata> MaybeTakeMetadata = ParseTake(DownloadedStorage, InTakeName);
		if (MaybeTakeMetadata.IsSet())
		{
			FTakeId TakeId = InProcessHandle->GetTakeId();

			FScopeLock Lock(&DownloadedTakesMutex);
			DownloadedTakes.Add(TakeId, DownloadedTake);
			Lock.Unlock();

			FTakeMetadata TakeMetadata = MaybeTakeMetadata.GetValue();
			
			ExtractTimecodeIfNotSet(TakeMetadata);
			
			UpdateTake(TakeId, MoveTemp(TakeMetadata));

			ExecuteProcessFinishedReporter(InProcessHandle.Get(), MakeValue());
		}
		else
		{
			TValueOrError<void, FIngestCapability_Error> Result = MakeError(FIngestCapability_Error::DownloaderError, TEXT("Failed to parse the take metadata"));
			ExecuteProcessFinishedReporter(InProcessHandle.Get(), MoveTemp(Result));
			IFileManager::Get().DeleteDirectory(*DownloadedTake, false, true);
		}
	}
	else
	{
		TValueOrError<void, FIngestCapability_Error> Result = MakeError(FIngestCapability_Error::DownloaderError, InResult.GetError().GetMessage());
		ExecuteProcessFinishedReporter(InProcessHandle.Get(), MoveTemp(Result));
		IFileManager::Get().DeleteDirectory(*DownloadedTake, false, true);
	}
}

TOptional<FTakeMetadata> ULiveLinkFaceDevice::ParseTake(const FString& InTakeDirectory, const FString& InTakeName)
{
	using namespace UE::CaptureManager;

	const FString TakeDirectory = InTakeDirectory;
	const FString TakePath = FPaths::Combine(TakeDirectory, InTakeName);

	TArray<FString> TakeFiles;
	IFileManager::Get().FindFiles(TakeFiles, *TakePath, *FTakeMetadata::FileExtension);
	if (!TakeFiles.IsEmpty())
	{
		check(TakeFiles.Num() == 1);

		FTakeMetadataParser TakeMetadataParser;
		TValueOrError<FTakeMetadata, FTakeMetadataParserError> TakeMetadataResult = TakeMetadataParser.Parse(TakePath / TakeFiles[0]);
		if (!TakeMetadataResult.HasError())
		{
			return TakeMetadataResult.StealValue();
		}
	}

	TArray<FText> ValidationFailures;
	TOptional<FTakeMetadata> ParseOldMetadataResult = LiveLinkMetadata::ParseOldLiveLinkTakeMetadata(TakePath, ValidationFailures);

	if (ParseOldMetadataResult.IsSet())
	{
		return ParseOldMetadataResult.GetValue();
	}

	ParseOldMetadataResult = StereoCameraMetadata::ParseOldStereoCameraMetadata(TakePath, ValidationFailures);

	if (ParseOldMetadataResult.IsSet())
	{
		return ParseOldMetadataResult.GetValue();
	}

	return {};
}

void ULiveLinkFaceDevice::FetchPreIngestFiles(TMap<FString, UE::CaptureManager::FTakeId> InNameToIdMap)
{
	using namespace UE::CaptureManager;

	FCPSDataStream::FFileExportFinished OnFileExportFinishedCallback = FCPSDataStream::FFileExportFinished::CreateLambda(
		[this, NameToIdMap = MoveTemp(InNameToIdMap)](TMap<FString, TMap<FString, TProtocolResult<FCPSDataStream::FData>>> InData)
		{
			for (const TPair<FString, TMap<FString, TProtocolResult<FCPSDataStream::FData>>>& TakePair : InData)
			{
				const FString& TakeName = TakePair.Key;
				for (const TPair<FString, TProtocolResult<FCPSDataStream::FData>>& FilePair : TakePair.Value)
				{
					const FString& FileName = FilePair.Key;
					TProtocolResult<FCPSDataStream::FData> Result = FilePair.Value;

					if (Result.HasError())
					{
						continue;
					}

					if (const FTakeId* TakeId = NameToIdMap.Find(TakeName))
					{
						TOptional<FTakeMetadata> TakeMetadataOpt = GetTakeMetadata(*TakeId);
						if (!TakeMetadataOpt.IsSet())
						{
							return;
						}

						FTakeMetadata TakeMetadata = TakeMetadataOpt.GetValue();

						FCPSDataStream::FData Data = Result.GetValue();

						if (FileName == "thumbnail.jpg")
						{
							TakeMetadata.Thumbnail = FTakeThumbnailData(MoveTemp(Data));
						}
						else if (FileName == "video_metadata.json")
						{

							FString DataString = FString(StringCast<UTF8CHAR>(reinterpret_cast<const char*>(Data.GetData()), Data.Num()));

							TArray<FText> ValidationFailures;
							TArray<FTakeMetadata::FVideo> ParsedVideoObject = LiveLinkMetadata::ParseOldLiveLinkVideoMetadataFromString(DataString, ValidationFailures);

							TakeMetadata.Video = ParsedVideoObject;
						}

						UpdateTake(*TakeId, MoveTemp(TakeMetadata));

						PublishEvent<FIngestCapability_TakeUpdatedEvent>(*TakeId);
					}
				}
			}
		});

	TUniquePtr<FCPSDataStream> DataStream = MakeUnique<FCPSDataStream>(MoveTemp(OnFileExportFinishedCallback));

	Device->FetchFiles(MoveTemp(DataStream), { FString("thumbnail.jpg"), FString("video_metadata.json") });
}

void ULiveLinkFaceDevice::RemoveDownloadedTakeData(const UE::CaptureManager::FTakeId InTakeId)
{
	FScopeLock Lock(&DownloadedTakesMutex);
	FString* DownloadedTake = DownloadedTakes.Find(InTakeId);

	if (!DownloadedTake)
	{
		return;
	}

	IFileManager::Get().DeleteDirectory(**DownloadedTake, false, true);
}

void ULiveLinkFaceDevice::ExtractTimecodeIfNotSet(FTakeMetadata& InOutTakeMetadata)
{
	using namespace UE::CaptureManager;

	bool bVideoFrameRateSet = false;
	FFrameRate VideoFrameRate;
	for (FTakeMetadata::FVideo& Video : InOutTakeMetadata.Video)
	{
		if (!Video.TimecodeStart.IsSet())
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

	for (FTakeMetadata::FAudio& Audio : InOutTakeMetadata.Audio)
	{
		if (!Audio.TimecodeStart.IsSet() && !Audio.TimecodeRate.IsSet())
		{
			TSharedPtr<FCaptureExtractAudioTimecode> Extractor = MakeShareable(new FCaptureExtractAudioTimecode(Audio.Path));

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
		}
	}
}
